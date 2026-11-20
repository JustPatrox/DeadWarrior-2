#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto root=std::filesystem::path(argc>1 ? argv[1] : "assets");MenuAssets art(root/"menu");
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","interaction"}) art.loadTextures(root/folder);
        auto require=[](bool b,const char* why){if(!b) throw std::runtime_error(why);};
        CampaignTutorial tutorial(art,root/"tutorial",Difficulty::Normal);
        tutorial.point(Point{30,610});for(int i=0;i<4;++i) tutorial.tick();
        const auto locked=tutorial.drawList();
        require(std::none_of(locked.begin(),locked.end(),[](const Draw& d){return d.symbol==1119;}),"Hidden tutorial controls produced a tooltip");
        CampaignProgress p;p.level=7;p.points=7;CampaignTutorial game(art,root,p);
        const auto hidden=game.drawList();
        require(std::none_of(hidden.begin(),hidden.end(),[](const Draw& d){return d.symbol==4979||d.symbol==4932||d.symbol==8763;}),"Enemies leaked through unexplored terrain");
        game.selectArmy();require(game.battleStatus().selectedMilitary==3,"Army selection missing");
        auto click=[&](Point at){game.press(at);game.release(at);};
        click({310,600});for(int i=0;i<80;++i) game.tick();
        click({550-game.camera(),100});game.selectArmy();
        auto selected=game.drawList();
        require(std::any_of(selected.begin(),selected.end(),[](const Draw& d){return d.symbol==2311;}),"Castle selection blocked troop toolbar");
        game.point(Point{30,610});for(int i=0;i<3;++i) game.tick();auto early=game.drawList();
        require(std::none_of(early.begin(),early.end(),[](const Draw& d){return d.symbol==1119;}),"Original tooltip delay was skipped");
        game.tick();auto tips=game.drawList();
        require(std::any_of(tips.begin(),tips.end(),[](const Draw& d){return d.symbol==1119 && d.frame==20001;}),"Original recruitment tooltip missing");
        require(std::find_if(tips.begin(),tips.end(),[](const Draw& d){return d.symbol==1119;})<
                std::find_if(tips.begin(),tips.end(),[](const Draw& d){return d.symbol==10118;}),"Tooltip was drawn above the original HUD");
        game.point({});game.tick();game.tick();game.point(Point{335,625});
        for(int i=0;i<23;++i) game.tick();const auto earlyOrder=game.drawList();
        require(std::none_of(earlyOrder.begin(),earlyOrder.end(),[](const Draw& d){return d.symbol==1119;}),"Original army-order tooltip delay was skipped");
        game.tick();const auto orderTip=game.drawList();
        require(std::any_of(orderTip.begin(),orderTip.end(),[](const Draw& d){return d.symbol==1119 && d.frame==20022;}),"Original army-order tooltip missing");
        game.point(Point{550-game.camera(),100});game.tick();auto hover=game.drawList();
        int expected=0;std::ifstream states(root/"castle/states.tsv");
        for(std::string line;std::getline(states,line);) {std::istringstream row(line);int owner,state,depth,base;row>>owner>>state>>depth>>base;if(owner==1865&&state==2&&depth==0){expected=base+1;break;}}
        require(expected && std::any_of(hover.begin(),hover.end(),[&](const Draw& d){return d.children && std::any_of(d.children->begin(),d.children->end(),[&](const Draw& c){return c.symbol==1865&&c.frame==expected;});}),"Original castle hover state missing");
        const int soldiers=game.battleStatus().fighters;
        click({350,660});require(game.defenceStatus().garrisoned==4,"Miners-only garrison affected soldiers");
        click({490,660});require(!game.defenceStatus().garrisoned&&game.battleStatus().fighters==soldiers,"Resume mining changed army");
        CampaignProgress second;second.level=1;second.points=1;
        CampaignTutorial attack(art,root,second),defend(art,root,second);
        auto order=[](CampaignTutorial& g,Point at){g.press(at);g.release(at);};
        order(attack,{500,634});order(attack,{88,610});order(defend,{88,610});
        for(int i=0;i<1300;++i){attack.tick();defend.tick();}
        require(attack.battleStatus().furthestMilitary>defend.battleStatus().furthestMilitary+400,"New recruits did not inherit global attack");
        for(auto [w,h]:{std::pair{2560,1080},std::pair{3840,1080},std::pair{1024,768},std::pair{720,1280}}) {
            Viewport::configure(Viewport::Aspect::Dynamic,w,h);Viewport view(w,h);
            require(std::abs(view.pixels.w-w)<.01&&std::abs(view.pixels.h-h)<.01,"Dynamic aspect leaves bars");
            const auto center=view.toStage({w/2.,h/2.});require(center&&std::abs(center->x-425)<.01,"Dynamic inverse input moved center");
            auto draws=game.drawList();
            require(std::any_of(draws.begin(),draws.end(),[](const Draw& d){return d.symbol==10102&&d.target.y>Viewport::height-40;}),"Dynamic HUD was not bottom-anchored");
        }
        Viewport::configure(Viewport::Aspect::Stretched,1024,768);Viewport stretched(1024,768);
        require(stretched.pixels.w==1024&&stretched.pixels.h==768,"Stretched mode leaves bars");
        Viewport::configure(Viewport::Aspect::Fixed,1280,720);
        auto drawn=[&](int symbol,int frame){const auto list=game.drawList();return std::any_of(list.begin(),list.end(),[&](const Draw& d){return d.symbol==symbol&&d.frame==frame;});};
        game.point(Point{506,683.5});require(drawn(10084,71001),"Original MENU hover state missing");
        game.toolbarMouseDown(true);require(drawn(10084,72001),"Original MENU pressed state missing");
        game.point(Point{425,550});require(drawn(10084,70001),"Dragging outside MENU retained its pressed state");
        game.point(Point{506,683.5});require(drawn(10084,72001),"Dragging back into the held button lost its pressed state");
        game.toolbarMouseDown(false);require(drawn(10084,71001),"Release did not restore MENU hover");
        game.selectArmy();game.point(Point{813,642});require(drawn(10009,71001),"Original action hover pulse missing");
        for(int i=0;i<10;++i) game.tick();require(drawn(10009,71011),"Original action pulse did not advance at 30 Hz");
        game.toolbarMouseDown(true);require(drawn(10009,72001),"Original action pressed state missing");
        game.tick();game.tick();game.toolbarMouseDown(false);
        require(drawn(10009,71001),"Release did not restart the original hover timeline");
        game.release({});require(drawn(10009,70001),"Cancelled action press remained visible");
        click({554,582});game.point(Point{813,642});const int simulationBefore=game.ticks();
        for(int i=0;i<5;++i) game.tick();
        require(game.ticks()==simulationBefore+10&&drawn(10009,71006),"Fast-forward changed the toolbar animation rate");
        game.point(Point{30,610});require(drawn(10109,74001),"Original recruitment building highlight missing");
        std::cout<<"Toolbar, fog, castle hover, miner orders, inherited attack and aspect transforms passed\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
