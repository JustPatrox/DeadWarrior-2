#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
    static void giant(CampaignTutorial& g) {
        for(int type:{g.giant,g.enslavedGiant}) {
            g.units_.clear();g.wall_.hp=0;g.camera_=2000;
            g.spawn(type,{2400,125});g.spawn(g.spearton,{2500,125},true);
            auto& u=g.units_[0];auto& victim=g.units_[1];
            u.selected=true;u.hold=true;u.facing=-1;victim.canFight=false;victim.hp=victim.maxHp=5000;
            g.command({500,g.battlefieldY_+100});
            require(u.order==CampaignTutorial::Order::Attack&&!u.hold,"Direct Giant attack retained the previous Hold command");
            const int before=g.giantHits_+g.boulderShots_;
            for(int tick=0;tick<250;++tick) {++g.ticks_;g.updateUnit(u);g.updateBoulders();}
            require(g.giantHits_+g.boulderShots_>before,"Explicit Giant target did not produce a strike or throw");
        }
        std::cout<<"Both Giant types replace Hold with an explicit attack and respond without a workaround\n";
    }
    static void retarget(CampaignTutorial& g) {
        // UnitAi.setCommand cuts an allied unit's strike only when the target
        // changes. EnslavedGiant and active Monk/Magikill spells override it.
        for(int type:{g.sword,g.spearton,g.ninja,g.archer,g.giant,g.enslavedGiant,g.healer,g.wizard}) {
            g.units_.clear();g.wall_.hp=0;g.camera_=2000;
            g.spawn(type,{2400,125});g.spawn(g.spearton,{2500,125},true);g.spawn(g.spearton,{2800,125},true);
            auto& u=g.units_[0];u.selected=true;u.target=1;u.order=CampaignTutorial::Order::Attack;
            g.animate(u,"attack_1");u.frame=10;u.hit=false;
            if(type==g.healer)u.spell=4;
            if(type==g.wizard)u.spell=1;
            g.command({500,g.battlefieldY_+100});
            require(u.target==1&&u.pose=="attack_1"&&u.frame==10,"Repeating the same enemy target restarted an allied strike");
            g.command({800,g.battlefieldY_+100});
            require(u.target==2&&u.order==CampaignTutorial::Order::Attack,"Direct retarget failed to replace the commanded enemy");
            if(type==g.enslavedGiant || u.spell)
                require(u.pose=="attack_1"&&u.frame==10,"Retarget interrupted an authored throw or active spell");
            else require(!u.pose.starts_with("attack_")&&u.hit,"Changing an allied target kept the old strike animation active");
        }
        std::cout<<"Allied direct retarget cuts ordinary strikes, preserves same-target strikes, Giant throws and active spells\n";
    }
    static void stopHold(CampaignTutorial& g) {
        for(bool hold:{false,true})for(int type:{g.sword,g.spearton,g.ninja,g.archer,g.giant,g.enslavedGiant,g.healer,g.wizard}) {
            g.units_.clear();g.spawn(type,{2400,125});g.spawn(g.spearton,{2500,125},true);
            auto& u=g.units_[0];u.selected=true;u.target=1;u.order=CampaignTutorial::Order::Attack;
            g.animate(u,"attack_1");u.frame=10;u.hit=false;
            if(type==g.healer)u.spell=4;
            if(type==g.wizard)u.spell=1;
            const Point button=hold ? Point{813,678} : Point{744,679.5};
            g.press(button);g.release(button);
            require(u.order==CampaignTutorial::Order::Stand&&u.hold==hold,"Stop/Hold failed to replace the pending unit command");
            if(type==g.enslavedGiant || u.spell)
                require(u.pose=="attack_1"&&u.frame==10,"Stop/Hold interrupted a protected throw or active spell");
            else require(!u.pose.starts_with("attack_")&&u.hit,"Stop/Hold left an ordinary allied strike active");
        }
        std::cout<<"Stop/Hold cuts ordinary allied strikes while preserving protected throws and spells\n";
    }
    static void throwMove(CampaignTutorial& g) {
        g.units_.clear();g.wall_.hp=0;
        g.spawn(g.enslavedGiant,{2400,125});g.spawn(g.spearton,{2600,185},true);
        auto& u=g.units_[0];u.target=1;u.order=CampaignTutorial::Order::Attack;
        g.updateUnit(u);require(u.pose=="attack_1","Throw/Move fixture failed to start a throw");
        u.frame=10;const int shots=g.boulderShots_;g.moveFormation({&u},{2700,125});
        require(u.order==CampaignTutorial::Order::Move&&u.goal.x==2684,"Move did not queue the Giant's original formation destination");
        require(u.pose=="attack_1"&&u.frame==10&&u.target==1,"Move interrupted the Giant's protected throw or discarded its firing target");
        for(int tick=0;tick<25;++tick){++g.ticks_;g.updateUnit(u);}
        require(g.boulderShots_==shots+1&&!g.boulders_.empty()&&g.boulders_.back().dy>0,"Pending Move prevented the throw event or lost its depth aim");
        for(int tick=0;tick<110;++tick){++g.ticks_;g.updateUnit(u);}
        require(!u.pose.starts_with("attack_")&&u.position.x>2450,"Giant did not resume its queued movement after throwing");
        std::cout<<"Enslaved Giant finishes its protected throw and depth aim before following Move\n";
    }
    static void meric(CampaignTutorial& g) {
        g.units_.clear();g.spawn(g.healer,{2400,125});g.spawn(g.spearton,{2440,125});
        auto& u=g.units_[0];auto& patient=g.units_[1];patient.canFight=false;patient.hp=100;
        require(u.autoHeal&&u.autoCure,"Monk.init's default spell toggles differ");
        u.order=CampaignTutorial::Order::Move;u.goal={2700,125};
        require(!g.updateCaster(u)&&!u.spell,"Meric started automatic healing during an explicit Move");
        u.order=CampaignTutorial::Order::Stand;
        require(g.updateCaster(u)&&u.spell==4,"Idle Meric did not heal an injured ally");
        u.frame=10;g.moveFormation({&u},{2700,125});
        require(u.pose=="attack_1"&&u.frame==10,"Movement replaced a live healing animation without finishing its state");
        for(int tick=0;tick<130;++tick){++g.ticks_;g.updateUnit(u);g.updateUnit(patient);}
        require(!u.spell&&u.position.x>2450&&patient.hp>100,"Meric failed to finish its cast and obey the pending Move");
        u.spellCooldown[0]=0;patient.hp=0;u.order=CampaignTutorial::Order::Stand;
        require(!g.updateCaster(u)&&!u.spell,"Meric started healing a dead target");
        g.units_.clear();g.spawn(g.healer,{2400,125});g.spawn(g.spearton,{2600,125},true);
        g.units_[0].order=CampaignTutorial::Order::Attack;g.units_[0].target=1;
        g.updateUnit(g.units_[0]);
        require(g.units_[0].position.x==2400,"Allied Meric closed inside the original 50000 squared support distance");
        g.units_.clear();g.spawn(g.healer,{2400,125});g.spawn(g.spearton,{2533,125});
        g.units_[1].hp=100;
        require(g.updateCaster(g.units_[0])&&g.units_[0].spellTarget==1,"Original spatial-hash healing candidate at 133 units was lost");
        for(const auto order:{CampaignTutorial::ArmyOrder::Attack,CampaignTutorial::ArmyOrder::Defend}) {
            g.units_.clear();g.spawn(g.healer,{2400,125});g.spawn(g.spearton,{2440,125});
            auto& caster=g.units_[0];auto& friendUnit=g.units_[1];friendUnit.canFight=false;friendUnit.hp=100;
            require(g.updateCaster(caster)&&caster.spell==4,"Macro-order fixture failed to start a live heal from Stand");
            caster.frame=10;g.orderArmy(false,order,2700);
            require(caster.order==CampaignTutorial::Order::Move&&caster.goal.x>2600,
                    "A macro army order left the casting Meric in Stand instead of queuing movement");
            require(caster.spell==4&&caster.pose=="attack_1"&&caster.frame==10,
                    "A macro army order interrupted the Meric's authored cast");
            for(int tick=0;tick<130;++tick){++g.ticks_;g.updateUnit(caster);g.updateUnit(friendUnit);}
            require(!caster.spell&&caster.position.x>2450&&friendUnit.hp>100,
                    "Meric failed to finish healing and move after a macro Attack/Defend order");
        }
        std::cout<<"Meric Move priority, intact cast, healing event, movement resumption and dead-target exclusion passed\n";
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets";
        const std::string test=argc>2?argv[2]:"all";
        MenuAssets art(root/"menu");
        for(const char* name:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","deads","interaction"})art.loadTextures(root/name);
        CampaignProgress p;p.level=8;p.points=8;CampaignTutorial g(art,root,p);
        if(test=="all"||test=="giant")CampaignProbe::giant(g);
        if(test=="all"||test=="retarget")CampaignProbe::retarget(g);
        if(test=="all"||test=="stop-hold")CampaignProbe::stopHold(g);
        if(test=="all"||test=="throw-move")CampaignProbe::throwMove(g);
        if(test=="all"||test=="meric")CampaignProbe::meric(g);
        if(test=="all"||test=="tree") {
            // An old or edited save may contain a child whose own parent is absent.
            // It must not allow a further purchase that bypasses that missing root.
            p.upgrades.set(9);p.upgrades.reset(5);
            CampaignIntermission tree(art,root/"intermission",p,CampaignIntermission::Page::Upgrades);
            const auto draws=tree.drawList();
            const auto node=std::find_if(draws.begin(),draws.end(),[](const Draw& d){return d.symbol==529;});
            if(node==draws.end())throw std::runtime_error("Tower Spawn node missing");
            const Point at{node->target.x+node->target.w/2,node->target.y+node->target.h/2};
            tree.point(at);tree.press();tree.release();
            CampaignProbe::require(!p.upgrades[19]&&p.points==8,"A missing ancestor allowed an upgrade purchase");
            std::cout<<"Upgrade purchases require the complete root chain\n";
        }
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
