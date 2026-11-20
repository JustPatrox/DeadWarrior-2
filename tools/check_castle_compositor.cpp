#include "campaign_tutorial.hpp"
#include "windows_render.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cmath>
int main(int argc,char** argv) {
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&startup,nullptr);
    int result=1;
    try {
        const std::filesystem::path root=argc>1 ? argv[1] : "assets",output=argc>2 ? argv[2] : "work/native-castle";
        std::filesystem::create_directories(output);sw2::MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","interaction"})art.loadTextures(root/name);
        sw2::RasterCompositor composer(art,loadWindowsPng);
        struct Case {const char* name;sw2::Point at;};
        for(const auto& test:{Case{"neutral",{-9999,-9999}},Case{"magicShopMc",{432,130}},Case{"templeMc",{145,160}},
                             Case{"bankMc",{60,350}},Case{"siegeShop",{325,70}},Case{"barracksMc",{650,150}},Case{"archeryRangeMc",{550,100}}}) {
            sw2::CampaignProgress p;p.level=7;p.points=7;sw2::CampaignTutorial game(art,root,p);
            for(int tick=1;tick<=400;++tick) {
                if(test.at.x>-9000)game.point(sw2::Point{test.at.x-game.camera(),test.at.y});else game.point({});
                game.tick();
                if(tick!=1&&tick!=5&&tick!=15&&tick!=30&&tick!=60&&tick!=240&&tick!=400)continue;
                const auto list=game.drawList();const auto found=std::find_if(list.begin(),list.end(),[](const sw2::Draw& d){return bool(d.children);});
                if(found==list.end()) throw std::runtime_error("Original castle group missing");
                const auto image=composer.image(*found);const auto stem=output/(std::string(test.name)+"-"+std::to_string(tick));
                std::ofstream data(stem.string()+".rgba",std::ios::binary);data.write(reinterpret_cast<const char*>(image->rgba.data()),std::streamsize(image->rgba.size()));
                std::ofstream bounds(stem.string()+".txt");bounds<<found->target.x+game.camera()<<' '<<found->target.y<<' '<<image->width<<' '<<image->height;

            }
            std::cout<<test.name<<" captured\n";
        }
        result=0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
    Gdiplus::GdiplusShutdown(token);return result;
}
