#include "campaign_menu.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace sw2;
static void require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static bool onScreen(Rect r) {return r.x+r.w>Viewport::left&&r.x<Viewport::right&&r.y+r.h>0&&r.y<Viewport::height;}
static bool panelButton(int id) {return id==1002||id==708||id==1005||id==1009||id==1010||id==1015||id==1021||id==1018||id==1027;}
static void check(CampaignMenu& menu,const std::vector<int>& active) {
    int visible=0;
    for(const auto& hit:menu.hits())if(panelButton(hit.symbol)&&onScreen(hit.bounds)) {
        require(std::find(active.begin(),active.end(),hit.symbol)!=active.end(),"An inactive menu page leaked into the viewport");
        require(hit.bounds.y>=0&&hit.bounds.y+hit.bounds.h<=Viewport::height+1,"Active menu control is clipped");
        const Viewport transform(686,720);
        Point pixel{transform.pixels.x+(hit.bounds.x+hit.bounds.w/2-Viewport::left)*transform.scaleX,
                    transform.pixels.y+(hit.bounds.y+hit.bounds.h/2)*transform.scale};
        const auto inverse=transform.toStage(pixel);
        require(inverse&&hit.bounds.contains(*inverse),"Menu input transform disagrees with rendered control");++visible;
    }
    require(visible==int(active.size()),"Expected active menu controls are missing");
    const auto draws=menu.drawList();
    const auto version=std::find_if(draws.begin(),draws.end(),[](const Draw& d){return d.symbol==1033;});
    require(version!=draws.end()&&version->target.y>Viewport::height-50&&version->target.y+version->target.h<=Viewport::height,"Version text lost its bottom anchor");
}
static void settle(CampaignMenu& menu) {for(int i=0;i<100;++i)menu.tick();}
static void click(CampaignMenu& menu,int id) {menu.press(id);menu.release(id);settle(menu);}
int main(int argc,char** argv) {
    try {
        MenuAssets art((argc>1?std::filesystem::path(argv[1]):"assets")/"menu");
        for(auto mode:{Viewport::Aspect::Fixed,Viewport::Aspect::Stretched,Viewport::Aspect::Dynamic})
        for(auto size:{Point{1280,720},Point{1920,1080},Point{686,720},Point{720,1280},Point{2560,1080}})
        for(bool save:{false,true}) {
            Viewport::configure(mode,int(size.x),int(size.y));CampaignMenu menu(art,save);
            for(int i=0;i<280;++i)menu.tick();check(menu,{1002,708,1005});
            // Resizing must move the bottom anchor immediately, even between UI ticks.
            Viewport::configure(Viewport::Aspect::Dynamic,686,1100);check(menu,{1002,708,1005});
            Viewport::configure(mode,int(size.x),int(size.y));
            click(menu,1002);check(menu,save?std::vector<int>{1009,1010}:std::vector<int>{1009});
            click(menu,1009);check(menu,{1015,1021,1018});
            click(menu,1015);check(menu,{1027});
        }
        Viewport::configure(Viewport::Aspect::Fixed,1280,720);
        std::cout<<"Menu page visibility, bottom anchors, resize and inverse input passed in 30 mode/size/save combinations\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
