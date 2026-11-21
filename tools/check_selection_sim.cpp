#include "campaign_tutorial.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    static int count(const CampaignTutorial& g){return int(std::count_if(g.units_.begin(),g.units_.end(),[](const auto& u){return u.selected;}));}
    static void run(CampaignTutorial& g) {
        g.units_.clear();g.camera_=g.targetCamera_=2000;
        g.spawn(g.sword,{2400,125});g.spawn(g.sword,{2400,130});
        g.spawn(g.sword,{2455,125});g.spawn(g.sword,{2400,140},true);
        g.spawn(g.sword,{2400,145});g.units_.back().hp=0;
        const Point at{400,g.battlefieldY_+100};
        g.press(at);require(count(g)==1&&g.units_[1].selected,"Overlapping click did not pick one frontmost living ally");
        g.point(Point{at.x+2,at.y+2});g.release(Point{at.x+2,at.y+2});
        require(count(g)==1&&g.units_[1].selected&&!g.selectionDragging_,"Pointer jitter expanded the click selection");
        g.point(Point{455,at.y});require(count(g)==1&&g.units_[1].selected,"Hover changed released selection");
        g.units_[0].position.y=130;
        g.press(at);g.release(at);require(count(g)==1&&g.units_[1].selected,"Same-depth pick disagrees with stable render order");
        g.units_[0].position.y=125;
        g.press({360,g.battlefieldY_+60});g.point(Point{440,g.battlefieldY_+140});
        require(g.selectionDragging_&&count(g)==2&&g.units_[0].selected&&g.units_[1].selected,"Held rectangle failed to select its two allies");
        require(!g.units_[2].selected,"Unit under drag endpoint was selected outside the rectangle");
        g.release(Point{440,g.battlefieldY_+140});require(count(g)==2&&!g.selectionDragging_,"Release lost the rectangle result");
        g.press(at);g.release(at);require(count(g)==1,"Single click did not replace an existing group");
        g.press({360,g.battlefieldY_+60});g.point(Point{440,g.battlefieldY_+140});
        g.point(Point{360,g.battlefieldY_+60});g.release(Point{360,g.battlefieldY_+60});
        require(count(g)==0,"Returning a drag to its origin selected overlapping point hits");
        g.press(at);g.release({});g.point(Point{455,at.y});
        require(count(g)==1&&!g.dragStart_&&!g.selectionDragging_,"Lost capture left selection dragging active");
        // Guided tutorial steps must not expand a user's explicit single pick.
        g.units_.resize(3);g.units_[2].hp=0;g.step_=CampaignTutorial::Step::Move;
        for(auto& u:g.units_)u.canFight=false;
        g.camera_=g.targetCamera_=2200;
        g.press({200,at.y});g.release(Point{200,at.y});g.tick();
        require(count(g)==1,"Tutorial reselected multiple units after a single click");
        std::cout<<"Overlaps, front depth, ties, jitter, held rectangle, endpoint exclusion, release/capture and tutorial selection passed\n";
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets";
        MenuAssets art(root/"menu");art.loadTextures(root/"tutorial");art.loadTextures(root/"interaction");
        CampaignTutorial g(art,root/"tutorial",Difficulty::Normal);CampaignProbe::run(g);return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
