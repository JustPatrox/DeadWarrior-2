#include "campaign_tutorial.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    static void run(CampaignTutorial& g,const std::filesystem::path& fixture) {
        g.units_.clear();g.goldBalance_=2000;g.mana_=500;g.rangeSelected_=true;
        std::ifstream input(fixture);int lines=0;
        for(int tier=1;tier<=3;++tier) {
            const int gold=g.goldBalance_;g.defencePress({675,606});
            require(g.castleResearchLevel_==tier&&g.castleArcherLevel()==tier-1,"Next unlocked Castle Archer research was blocked");
            g.defencePress({675,606});require(g.goldBalance_==gold-int(g.castleTech(tier,"cost")),"Repeated press charged research twice");
            const int duration=int(g.castleTech(tier,"time"));
            for(int tick=0;tick<=duration;++tick)g.updateCastleResearch();
            require(g.castleArcherLevel()==tier-1,"Castle research finished before the original negative-countdown update");
            g.updateCastleResearch();require(g.castleArcherLevel()==tier&&g.castleResearchLevel_==0,"Castle research did not complete");
            g.updateCastleArcher();std::vector<std::pair<double,Draw>> draws;g.drawCastleArcher(draws);
            require(draws.size()==size_t(tier*2),"Wrong number of original castle bodies/bows");
            for(int i=0;i<tier;++i) {
                int level,cost,ticks,index,body;double x,y,z,scale;
                input>>level>>cost>>ticks>>index>>x>>y>>z>>scale>>body;
                require(bool(input)&&level==tier&&index==i&&cost==gold-g.goldBalance_&&ticks==duration+2,"Research cost/timing differs from original runtime reference");
                const auto& d=draws[i*2].second;const auto b=g.art_.texture(4176,body).bounds;
                require(d.symbol==4176&&d.frame==body&&std::abs(draws[i*2].first-y)<1e-9,"Original independent castle animation/depth differs");
                require(std::abs(d.target.x-(x-g.camera_+(g.rules_.at("archer.bodyX")+b.x)*scale))<1e-8&&
                        std::abs(d.target.y-(g.battlefieldY_+y+z+(g.rules_.at("archer.bodyY")+b.y)*scale))<1e-8,
                        "Castle Archer position/perspective differs from original reference");++lines;
            }
        }
        require(lines==6&&g.goldBalance_==800,"Three castle tiers spent the wrong total");
        g.defencePress({675,606});require(g.goldBalance_==800&&!g.castleResearchLevel_,"Fourth player castle tier became available");
        g.castleArcherCount_=1;g.upgrades_.reset(6);g.upgrades_.reset(10);
        g.defencePress({675,606});require(g.goldBalance_==800&&!g.castleResearchLevel_,"Locked campaign Castle Archer II accepted research");
        // Each defender owns a firing cycle; all three must actually launch.
        g.castleArcherCount_=3;g.spawn(g.spearton,{1500,125},true);g.units_.back().hp=50000;
        const int before=g.shots_;
        for(int tick=0;tick<180;++tick)g.updateCastleArcher();
        require(g.shots_-before>=6,"Additional castle defenders did not fire independently");
        std::cout<<"Original three-tier costs/countdowns, six defender positions, upgrade locks and independent firing passed\n";
    }
    static void fast(CampaignTutorial& g) {
        g.rangeSelected_=true;g.goldBalance_=300;g.defencePress({675,606});g.fastForward_=true;
        const int ticks=g.ticks_;g.tick();
        require(g.ticks_==ticks+2&&g.researchTicks_==298,"Research clock did not follow both fast-forward simulation steps");
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets",fixture=argc>2?argv[2]:"tools/fixtures/original-castle-research.tsv";
        MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","interaction"})art.loadTextures(root/name);
        CampaignProgress p;p.level=6;p.points=4;p.upgrades.set(6);p.upgrades.set(10);
        CampaignTutorial g(art,root,p);CampaignProbe::run(g,fixture);
        CampaignTutorial fast(art,root,p);CampaignProbe::fast(fast);return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
