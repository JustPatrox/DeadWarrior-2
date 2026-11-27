#include "campaign_tutorial.hpp"
#include <iostream>
#include <stdexcept>
using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void swordIdle(CampaignTutorial& g) {
        g.units_.clear();
        const int id=g.spawn(g.sword,{2500,125});
        auto& u=g.units_[id];u.canFight=false;u.mayWalkThrough=true;
        // Original swordwrath_392: frame150 executes gotoAndPlay(60).
        // Repeating the opening gesture is visibly different from this tail.
        for(int tick=1;tick<=500;++tick) {
            g.updateUnit(u);
            const int expected=tick<149 ? tick+1 : 60+(tick-149)%90;
            if(u.pose!="stand" || u.frame!=expected)
                throw std::runtime_error("Swordwrath repeated the opening idle gesture instead of the authored tail");
        }
        u.pose="stand_breath";u.frame=86;g.updateUnit(u);
        if(u.frame!=87)throw std::runtime_error("Swordwrath breathing lost its final frame");
        g.updateUnit(u);
        if(u.frame!=2)throw std::runtime_error("Swordwrath breathing did not retain its original cycle");
        u.pose="stand";u.frame=149;u.order=CampaignTutorial::Order::Move;u.goal={3000,125};
        g.updateUnit(u);
        if(u.pose!="run" || u.frame!=2)
            throw std::runtime_error("A move command did not interrupt Swordwrath idle");
    }
    static void run(CampaignTutorial& g) {
        swordIdle(g);
        g.units_.clear();g.wall_.hp=0;
        g.spawn(g.spearton,{2500,125});g.spawn(g.giant,{2560,125},true);
        auto& target=g.units_[0];target.canFight=false;target.hp=target.maxHp=5000;
        auto& giant=g.units_[1];giant.order=CampaignTutorial::Order::Move;giant.goal={2563,127};
        giant.velocity={1.1,0};giant.engageOnMove=false;giant.facing=-1;
        g.updateBattle(false);
        if(giant.order!=CampaignTutorial::Order::Attack||giant.target!=0)
            throw std::runtime_error("Arrived MOVE unit did not acquire the adjacent enemy before motion");
        for(int tick=0;tick<400;++tick) {
            ++g.ticks_;
            // Explosive War: macro AI can defend, then its controller attacks
            // in the same step. Neither original AI command cancels the strike.
            g.orderArmy(true,CampaignTutorial::ArmyOrder::Defend,g.length_-850-900);
            g.orderArmy(true,CampaignTutorial::ArmyOrder::Attack,g.statues_[0].position.x);
            g.updateBattle(false);for(auto& u:g.units_)g.updateUnit(u);
        }
        if(g.giantHits_==0||target.hp>=5000)throw std::runtime_error("Giant remained passive instead of defending itself");
        // Retarget after the victim dies; the previous attack must not strand it.
        target.hp=0;g.spawn(g.spearton,{giant.position.x-60,giant.position.y});
        g.units_.back().canFight=false;g.units_.back().hp=g.units_.back().maxHp=5000;
        const int previous=g.giantHits_;
        for(int tick=0;tick<500;++tick) {++g.ticks_;g.updateBattle(false);for(auto& u:g.units_)g.updateUnit(u);}
        if(g.giantHits_<=previous)throw std::runtime_error("Giant did not respond to the next enemy after target death");
        std::cout<<"Swordwrath idle tail, arrived-MOVE response, residual velocity, Giant strikes and target replacement passed\n";
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets";MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","interaction"})art.loadTextures(root/name);
        CampaignProgress p;p.level=6;p.points=6;CampaignTutorial g(art,root,p);CampaignProbe::run(g);return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
