#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
    static void clear(CampaignTutorial& g) {
        g.units_.clear();g.goldBalance_=g.mana_=g.population_=0;
        g.oreRemaining_.assign(g.gold_.size(),0);
    }
    static void mine(CampaignTutorial& g) {
        clear(g);g.oreRemaining_[0]=1;
        const auto ore=g.gold_[0];const int id=g.spawn(g.miner,{ore.x-50,ore.y});
        auto& u=g.units_[id];g.reserveOre(u,0);g.animate(u,"mine");
        u.frame=g.animations_.at({g.miner,"mine"}).length;g.workMiner(u);
        require(g.oreRemaining_[0]==0 && u.bag==1,"Final mining stroke did not consume exactly the remaining gold");
        for(int i=0;i<300;++i)g.updateUnit(u);
        require(u.bag==1 && g.goldBalance_==0 && g.oreRemaining_[0]==0,"An exhausted deposit generated additional gold");
        const auto draws=g.drawList();
        require(std::all_of(draws.begin(),draws.end(),[](const Draw& d){return d.symbol!=1078 || d.target.w==0;}),"Empty ore retained visible gold");
        g.oreRemaining_[0]=3500;g.reserveOre(u,0);u.bag=int(g.stat(u,"bagSize"));
        const int carried=u.bag;g.workMiner(u);
        require(u.miningSlot==-1,"Returning miner still reserved a gold slot");
        u.position={850,125};g.workMiner(u);
        require(g.goldBalance_==carried && u.bag==0,"Previously carried gold was not deposited exactly once");
        clear(g);g.upgrades_.reset();g.upgrades_.set(3);
        for(int i=1;i<=150;++i) {g.ticks_=i;g.simulateActors(false);}
        require(g.goldBalance_==5,"Passive Income I depended on live mines or changed its original five-second award");
        std::cout<<"Finite ore, final bag deposit and original passive income passed\n";
    }
    static void retarget(CampaignTutorial& g,bool alternative) {
        clear(g);g.oreRemaining_[0]=3500;
        if(alternative) for(size_t i=1;i<g.mineOrder_.size()/2;++i)g.oreRemaining_[g.mineOrder_[i]]=3500;
        else g.oreRemaining_[g.mineOrder_.back()]=3500; // A free enemy-side mine is not an automatic fallback.
        const auto ore=g.gold_[0];
        g.spawn(g.miner,{ore.x-50,ore.y});g.spawn(g.miner,{ore.x+50,ore.y});
        g.spawn(g.miner,{1300,235});
        g.reserveOre(g.units_[0],0);g.reserveOre(g.units_[1],0);g.reserveOre(g.units_[2],-1);
        auto& u=g.units_[2];g.animate(u,"pray");u.selected=true;
        g.command({ore.x-g.camera_,g.battlefieldY_+ore.y-5});
        require(u.resource>=0 && u.order==CampaignTutorial::Order::Gather && u.pose!="pray",
                "A command to a full gold mine left the miner praying");
        if(alternative)require(u.resource!=0 && u.miningSlot>=0,"Miner did not select a free home deposit");
        else {
            require(u.resource==0 && u.miningSlot==-1,"Full-mine target was confused with its slot reservation");
            for(int i=0;i<200;++i)g.updateUnit(u);
            require(g.mana_==0 && u.pose!="pray","Waiting for gold continued to generate mana");
            g.units_[0].hp=0;
        }
        for(int i=0;i<1000 && u.bag==0;++i)g.updateUnit(u);
        require(u.bag>0 && u.resource>=0,"Miner did not begin mining after a free spot became available");
        // An existing miner may keep its spot; changing to prayer frees it.
        g.targetOre(u,-1);require(u.resource==-1,"Gold-to-prayer command was lost");
        std::cout<<"Prayer-to-gold command passed (alternative="<<alternative<<")\n";
    }
    static void formation(CampaignTutorial& g,const std::filesystem::path& reference) {
        clear(g);
        const int types[]={g.archer,g.sword,g.spearton,g.miner,g.ninja,g.wizard,g.healer,g.sword,g.spearton,g.archer};
        for(int i=0;i<10;++i) {
            g.spawn(types[i],{i==1 ? 800. : i%2==0 ? 1900. : 1500.,125});
            g.units_.back().canFight=false;
        }
        g.orderArmy(false,CampaignTutorial::ArmyOrder::Defend,1750);
        std::ifstream input(reference);int id,type,command;double x,y,fromX,fromY;int count=0;
        while(input>>id>>type>>command>>x>>y>>fromX>>fromY) {
            const auto& u=g.units_.at(id);
            require(g.formationType(u.symbol)==type && u.goal.x==x && u.goal.y==y,
                    "Native defence goals differ from the captured original Team.defend/UnitMove");
            require(u.engageOnMove==(command==2),"Original MOVE and ATTACK_MOVE defence groups were merged");
            ++count;
        }
        require(count==10 && input.eof(),"Missing or invalid original formation reference");
        for(int i=0;i<1500;++i) for(auto& u:g.units_)g.updateUnit(u);
        for(const auto& u:g.units_) if(!g.isWorker(u.symbol))
            require(u.facing==1 && u.order==CampaignTutorial::Order::Stand,"Player defence did not settle facing right");
        const int enemy=g.spawn(g.sword,{g.length_-1750,125},true);
        g.orderArmy(true,CampaignTutorial::ArmyOrder::Defend,g.length_-1750);
        require(g.units_[enemy].idleFacing==0,"Player-only facing override was applied to the enemy");
        for(int i=0;i<300;++i)g.updateUnit(g.units_[enemy]);
        require(g.units_[enemy].facing==-1,"Enemy defence was forced to face right");
        g.orderArmy(false,CampaignTutorial::ArmyOrder::Attack,g.statues_[1].position.x);
        require(std::none_of(g.units_.begin(),g.units_.end(),[](const auto& u){return !u.enemy && u.idleFacing;}),
                "Attack retained the defensive facing override");
        std::cout<<"Captured original formation and player-only defensive facing passed\n";
    }
    static void castle(CampaignTutorial& g) {
        g.enemyCastleArcher_=true;
        std::vector<std::pair<double,Draw>> draws;g.drawCastleArcher(draws,true);
        require(!draws.empty() && std::all_of(draws.begin(),draws.end(),[](const auto& row){return row.second.redOffset==0;}),
                "Original untinted castle archer acquired the regular enemy tint");
        std::cout<<"Original black enemy castle archer preserved\n";
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1 ? argv[1] : "assets";
        const std::filesystem::path reference=argc>2 ? argv[2] : "tools/fixtures/original-defence.tsv";
        MenuAssets art(root/"menu");
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","interaction"})art.loadTextures(root/folder);
        CampaignProgress progress;progress.level=7;progress.points=7;
        CampaignTutorial economy(art,root,progress);CampaignProbe::mine(economy);
        CampaignTutorial alternative(art,root,progress);CampaignProbe::retarget(alternative,true);
        CampaignTutorial waiting(art,root,progress);CampaignProbe::retarget(waiting,false);
        CampaignTutorial formation(art,root,progress);CampaignProbe::formation(formation,reference);
        CampaignProbe::castle(formation);
        std::cout<<"Economy and orders probe passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
