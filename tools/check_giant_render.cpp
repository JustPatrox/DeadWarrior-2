// Render the original giant poses with the actual Windows backend. The fixture
// changes only prepared actor positions/poses; no campaign availability change.
#include "campaign_tutorial.hpp"
#include "windows_render.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
namespace sw2 {
struct CampaignProbe {
    static void prepare(CampaignTutorial& game,int count,bool enemy,bool castle) {
        game.units_.clear();game.cinematic_=-1;game.step_=CampaignTutorial::Step::Free;
        game.camera_=game.targetCamera_=castle ? 200 : 2600;
        for(int i=0;i<count;++i) {
            game.spawn(enemy ? game.giant : game.enslavedGiant,
                {game.camera_+300+(i%4)*110.,60+(i/4)*70.},enemy);
        }
        // Reveal enemy actors through normal player vision. Fog-hidden actors
        // would make an invalid performance fixture.
        if(enemy)game.spawn(game.sword,{game.camera_+950,125});
    }
    static void pose(CampaignTutorial& game,int frame) {
        const char* labels[]={"stand","run","attack_1"};
        for(size_t i=0;i<game.units_.size();++i) {
            auto& u=game.units_[i];u.pose=labels[frame/160%3];
            if(u.symbol!=game.giant && u.symbol!=game.enslavedGiant) {u.pose="stand";continue;}
            u.frame=1+(frame/2+int(i)*17)%game.animations_.at({u.symbol,u.pose}).length;
        }
    }
};
}
int main(int argc,char** argv) {
    using Clock=std::chrono::steady_clock;
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&startup,nullptr);
    int result=1;
    try {
        const std::filesystem::path root=argc>1 ? argv[1] : "assets",output=argc>2 ? argv[2] : "work/giant-render.tsv";
        const int width=argc>3 ? std::stoi(argv[3]) : 1920,height=width*9/16;
        const bool paced=argc>5,predict=!(argc>6 && std::string(argv[6])=="sync");
        sw2::Viewport::configure(sw2::Viewport::Aspect::Fixed,width,height);
        sw2::MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","interaction"})art.loadTextures(root/name);
        if(argc>4)for(const char* folder:{"giants","bomber"})art.loadTextures(std::filesystem::path(argv[4])/folder);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DW2GiantRenderProbe";
        RegisterClassW(&wc);
        auto hwnd=CreateWindowW(wc.lpszClassName,L"DeadWarrior 2 render check",WS_POPUP,0,0,width,height,nullptr,nullptr,wc.hInstance,nullptr);
        if(!hwnd)throw std::runtime_error("Cannot create render probe window");
        ShowWindow(hwnd,SW_SHOWNOACTIVATE);
        if(argc>5 && std::string(argv[5])=="transitions") {
            sw2::CampaignProgress progress;progress.level=6;sw2::CampaignTutorial game(art,root,progress);
            sw2::CampaignProbe::prepare(game,8,true,true);HardwareRenderer renderer(art);
            for(int f=0;f<12;++f) {
                const int w=f%3 ? 1280 : 1920,h=w*9/16;const bool sync=f%2;
                SetWindowPos(hwnd,nullptr,0,0,w,h,SWP_NOACTIVATE|SWP_NOZORDER);
                MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE))DispatchMessageW(&m);
                sw2::CampaignProbe::pose(game,f*10);
                if(!renderer.draw(hwnd,w,h,game.drawList(),false,sync))throw std::runtime_error("Target recreation failed while decoding giant frames");
                const auto actual=renderer.pixelSize();
                if(actual.cx!=w || actual.cy!=h || renderer.synchronized()!=sync)throw std::runtime_error("Recreated render target dimensions or sync differ");
            }
            const auto stats=renderer.statistics();
            if(!stats.prefetched)throw std::runtime_error("Transition fixture never consumed prefetched pixels");
            std::ofstream(output)<<"{\"transitions\":12,\"resize\":true,\"vsyncRecreation\":true,\"prefetched\":"<<stats.prefetched<<",\"passed\":true}\n";
            std::cout<<"Twelve target transitions passed; prefetched="<<stats.prefetched<<'\n';
        } else {
        std::ofstream report(output);report<<"enemy\tcount\tcastle\tpass\tmean_ms\tp95_ms\tmax_ms\tloads\tevictions\tprefetched\tdecode_ms\tresident_bytes\n";
        for(bool enemy:{true,false}) for(int count:{1,8}) for(bool castle:{false,true}) {
            if(paced && (!enemy || !castle))continue;
            sw2::CampaignProgress progress;progress.level=6;
            sw2::CampaignTutorial game(art,root,progress);sw2::CampaignProbe::prepare(game,count,enemy,castle);
            HardwareRenderer renderer(art,predict);
            for(int pass=0;pass<2;++pass) {
                const auto before=renderer.statistics();std::vector<double> times;const auto epoch=Clock::now();
                for(int f=0;f<480;++f) {
                    if(paced)std::this_thread::sleep_until(epoch+std::chrono::microseconds(16667)*f);
                    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
                    sw2::CampaignProbe::pose(game,f);const auto start=Clock::now();
                    const auto draws=game.drawList();
                    const int actors=int(std::count_if(draws.begin(),draws.end(),[](const sw2::Draw& d){
                        return d.symbol==6465 || d.symbol==6550 || d.symbol==6815 || d.symbol==7519 || d.symbol==7388 || d.symbol==7514;
                    }));
                    if(actors!=count)throw std::runtime_error("Prepared giants were not all visible");
                    if(!renderer.draw(hwnd,width,height,draws,false,false))throw std::runtime_error("Hardware renderer failed");
                    times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
                }
                const auto after=renderer.statistics();double sum=0;for(double t:times)sum+=t;std::sort(times.begin(),times.end());
                report<<enemy<<'\t'<<count<<'\t'<<castle<<'\t'<<pass<<'\t'<<sum/times.size()<<'\t'<<times[size_t(times.size()*.95)]<<'\t'<<times.back()
                    <<'\t'<<after.loads-before.loads<<'\t'<<after.evictions-before.evictions<<'\t'<<after.prefetched-before.prefetched<<'\t'<<after.decodeMs-before.decodeMs<<'\t'<<after.textureBytes<<'\n';report.flush();
                std::cout<<enemy<<' '<<count<<' '<<castle<<' '<<pass<<" mean="<<sum/times.size()<<" p95="<<times[size_t(times.size()*.95)]<<" loads="<<after.loads-before.loads<<'\n';
            }
        }
        }
        DestroyWindow(hwnd);result=0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';}
    Gdiplus::GdiplusShutdown(token);return result;
}
