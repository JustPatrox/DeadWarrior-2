#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    static void block(CampaignTutorial& g) {
        g.units_.clear();g.spawn(g.spearton,{2400,125});
        auto& u=g.units_[0];u.selected=true;u.canFight=false;g.upgrades_.set(4);
        g.unitAbility();require(u.inBlock&&u.blocking,"Block toggle did not enter the researched stance");
        for(int i=1;i<=22;++i){++g.ticks_;g.updateUnit(u);if(i==1)require(u.frame==2,"Original block entry frame differs");}
        require(u.frame==15&&u.inBlock,"Block did not hold original frame 15");
        u.hp=u.maxHp=1000;g.applyMagicDamage(u,50,true,false);
        require(std::abs(u.hp-982)<1e-8,"Original raw Block damage coercion differs");
        u.hp=1000;g.applyMagicDamage(u,12,true);
        require(std::abs(u.hp-992.8)<1e-8,"Block did not reduce unit/projectile damage by 40 percent");
        g.unitAbility();
        int ticks=0;while(u.inBlock&&ticks<60){++g.ticks_;g.updateUnit(u);++ticks;}
        require(ticks==15&&u.pose=="block"&&u.frame==2,"Original 15-update Block exit differs");
        g.updateUnit(u);require(u.pose=="stand","Block exit left the unit posing forever");
        g.unitAbility();
        for(int i=0;i<22;++i){++g.ticks_;g.updateUnit(u);}
        require(u.frame==15&&u.blocking,"Hold fixture did not reach the held Block frame");
        u.hold=true;
        ticks=0;while(u.inBlock&&ticks<60){++g.ticks_;g.updateUnit(u);++ticks;}
        require(ticks==15&&!u.blocking,"Hold retained the original Block command instead of exiting its stance");
        g.updateUnit(u);require(u.pose=="stand","Hold failed to resume the standing pose after Block exit");
        u.hold=false;
        g.unitAbility();g.moveFormation({&u},{2700,125});
        for(int i=0;i<80;++i)g.updateUnit(u);
        require(!u.inBlock&&u.position.x>2450,"A movement command failed to leave Block");
        g.upgrades_.reset(4);u.order=CampaignTutorial::Order::Stand;g.unitAbility();
        require(!u.inBlock,"Unresearched Block activated");
        std::cout<<"Original Block frame 15, 15-update exit, raw18/unit7.2 damage, movement and upgrade locks passed\n";
    }
    static void lane(CampaignTutorial& g) {
        for(int lateral:{20,40,60}) {
            g.units_.clear();g.spawn(g.spearton,{2400,125});g.spawn(g.spearton,{2520.,125.+lateral},true);
            auto& u=g.units_[0];u.order=CampaignTutorial::Order::Attack;u.target=1;
            g.updateUnit(u);
            require(std::abs(u.velocity.x-.5*.89)<1e-8,"Melee approach x differs from original");
            require(std::abs(u.velocity.y-(lateral==20?0:.5333333333333333*.89))<1e-8,"Melee attack lane differs from original");
        }
        std::cout<<"Original melee lateral 20/40/60 cases matched; close attackers retain separate lanes\n";
    }
    static void tower(CampaignTutorial& g,bool second) {
        g.units_.clear();g.spawn(g.spearton,{g.length_/2,g.depth_/2});
        g.units_[0].canFight=false;g.towerControl_=50;g.towerSpawnIndex_=-1;g.towerSpawnTime_=0;g.towerEffects_.clear();
        g.upgrades_.set(19);g.upgrades_.set(18,second);g.population_=3;
        g.camera_=g.length_/2-425;
        for(int tick=1;tick<=301;++tick){g.ticks_=tick;g.updateHill();}
        require(g.towerControl_==0,"Original center tower was not captured at update 301");
        g.ticks_=501;g.updateHill();require(g.towerSpawnIndex_==-1,"Ghost spawned without the strict 500-frame delay");
        g.ticks_=502;g.updateHill();
        require(g.towerSpawnIndex_==1,"Controlling the tower did not create its ghost");
        const auto& ghost=g.units_[1];
        require(ghost.towerSpawned&&ghost.symbol==(second?g.enslavedGiant:g.spearton)&&g.population_==3,"Wrong ghost tier or population cost");
        const Point at=ghost.position;g.orderArmy(false,CampaignTutorial::ArmyOrder::Defend,1500);
        require(g.units_[1].goal.x==g.statues_[1].position.x,"Army commands took control of the ghost");
        g.selectArmy();require(!g.units_[1].selected,"SPACE selected an uncontrolled ghost");
        g.units_[1].position.x=g.length_-1500;
        require(g.visionBoundary()>g.frontLine()+g.rules_.at("visionSize")-1,"Uncontrolled ghost did not extend original team vision");
        g.drawList();g.units_[1].hp=0;g.updateUnit(g.units_[1]);require(g.population_==3,"Ghost death deducted real army population");
        g.ticks_=503;g.updateHill();require(g.towerSpawnIndex_==-1,"Ghost corpse blocked replacement");
        g.ticks_=1003;g.updateHill();require(g.towerSpawnIndex_==-1,"Ghost replacement skipped its strict delay");
        g.ticks_=1004;g.updateHill();require(g.towerSpawnIndex_==2,"The tower failed to replace a dead ghost");
        std::cout<<"Tower tier "<<(second?2:1)<<": capture301, strict delay, free/uncontrolled ghost and replacement passed\n";
    }
};
}
int main(int argc,char** argv){
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets";MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","deads","interaction"})art.loadTextures(root/name);
        CampaignProgress p;p.level=8;p.points=8;CampaignTutorial g(art,root,p);
        CampaignProbe::block(g);CampaignProbe::lane(g);CampaignProbe::tower(g,false);CampaignProbe::tower(g,true);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
