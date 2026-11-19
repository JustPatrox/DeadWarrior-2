#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sw2 {
namespace {
constexpr double home=850;
struct TrainingButton {int type,building,icon; Point origin;};
constexpr TrainingButton buttons[]={
    {3104,0,10036,{4,589.35}},{4922,1,10039,{61.6,590}},{6456,1,10051,{32.85,643.35}},
    {4236,2,10042,{119.2,590}},{7289,1,10054,{90.4,643.35}},
    {7926,3,10048,{234.4,589.35}},{8233,4,10045,{176.8,589.35}},
    {7526,5,9852,{205.65,643.35}}};
Draw timerOverlay(Rect rect,double fraction) {
    const double w=rect.w,h=rect.h,pi=std::numbers::pi,c=std::atan2(w,h);
    const double angle=std::clamp(fraction,0.,1.)*2*pi;
    Point edge;
    if (angle<c) edge={w/2+std::tan(angle)*h/2,0};
    else if (angle<=pi/2) edge={w,std::tan(angle-c)*h/2};
    else if (angle<=pi-c) edge={w,h/2+std::tan(angle-pi/2)*h/2};
    else if (angle<=pi) edge={w/2+std::tan(pi-angle)*h/2,h};
    else if (angle<=pi+c) edge={std::tan(pi+c-angle)*h/2,h};
    else if (angle<=1.5*pi) edge={0,h-std::tan(angle-pi-c)*h/2};
    else if (angle<=2*pi-c) edge={0,h/2-std::tan(angle-2*pi-pi/2)*h/2};
    else edge={w/2+std::tan(angle)*h/2,0};
    auto draw=Draw::shape(Draw::Kind::Polygon,rect,0);
    draw.vertices={{w/2,0},{w/2,h/2},edge};
    if (angle<=c) draw.vertices.push_back({w,0});
    if (angle<=pi-c) draw.vertices.push_back({w,h});
    if (angle<=pi+c) draw.vertices.push_back({0,h});
    if (angle<=2*pi-c) draw.vertices.push_back({0,0});
    return draw;
}
}

CampaignTutorial::BattleStatus CampaignTutorial::battleStatus() const {
    int workers=0,fighters=0;
    for (const auto& u:units_) if (!u.enemy && u.hp>0) {
        if (isWorker(u.symbol)) ++workers; else ++fighters;
    }
    BattleStatus status{goldBalance_,mana_,population_,int(production_[0][0].units.size()),
        int(production_[0][1].units.size()),workers,fighters,statues_[0].hp,statues_[1].hp,reinforcement_};
    for (const auto& u:units_) if (u.hp>0) {
        if (!u.enemy && !isWorker(u.symbol)) status.fighterHealth+=u.hp;
        if (!u.enemy && u.symbol==sword && mission_ && u.rageCounter<rules_.at("rage.effect")) ++status.activeRage;
        if (!u.enemy && u.symbol==spearton) ++status.lancers;
        if (!u.enemy && u.symbol==archer) ++status.archers;
        if (u.enemy && u.symbol==archer) ++status.enemyArchers;
        if (u.enemy && u.symbol==ninja) ++status.enemyNinjas;
        if (!u.enemy && u.poison>0) ++status.poisonedAllies;
        if (!u.enemy && u.symbol==ninja) ++status.ninjas;
        if (u.enemy && u.symbol==wizard) ++status.wizards;
        if (u.enemy && u.symbol==healer) ++status.healers;
        if (u.enemy && u.symbol==giant) {++status.giants; status.giantHealth+=u.hp;}
        if (u.enemy && u.symbol==bomber) ++status.bombers;
        if (!u.enemy && u.symbol==enslavedGiant) ++status.enslavedGiants;
    }
    status.mobileShots=mobileShots_; status.towerControl=towerControl_;
    status.magicHits=magicHits_; status.heals=heals_; status.enemyCastleShots=enemyCastleShots_;
    status.spells=spells_;
    status.giantHits=giantHits_; status.cinematic=cinematic_; status.dialogue=dialogueIndex_;
    status.bomberWaves=bomberWaves_;status.nextBomberWave=nextBomberWave_;status.explosions=explosions_;
    status.boulderShots=boulderShots_;status.boulderHits=boulderHits_;
    return status;
}
CampaignTutorial::Unit& CampaignTutorial::combatTarget(int id) {
    if (id==ownStatue) return statues_[0];
    if (id==opposingStatue) return statues_[1];
    if (id==ownWall) return wall_;
    return units_.at(id);
}
void CampaignTutorial::spawnGroup(bool enemy,const std::vector<int>& symbols) {
    const double base=enemy ? length_-home : home;
    for (size_t i=0;i<symbols.size();++i) {
        const auto id=spawn(symbols[i],{base+100,i*depth_/symbols.size()},enemy);
        auto& u=units_[id];
        (enemy ? enemyPopulation_ : population_)+=int(stat(u,"population"));
        u.goal={base+(enemy ? -1 : 1)*(isWorker(u.symbol) ? 850. : 1000.),depth_/2};
        u.engageOnMove=true; u.order=Order::Move;
    }
}
int CampaignTutorial::recruitType(bool enemy,int building) const {
    if (building==0) return enemy && chaosOpponent_ ? chaosMiner : miner;
    if (building==2) return archer;
    if (building==5) return enemy ? giant : enslavedGiant;
    return enemy && !mission_ ? spearton : sword;
}
bool CampaignTutorial::recruit(bool enemy,int building,bool cancel,int requested) {
    auto& q=production_[enemy][building];
    const int type=requested ? requested : recruitType(enemy,building);
    if (cancel) {
        const auto order=std::find_if(q.units.rbegin(),q.units.rend(),[&](const Training& t){return t.symbol==type;});
        if (order==q.units.rend()) return false;
        const auto pos=std::prev(order.base());
        const bool first=pos==q.units.begin();
        (enemy ? enemyGold_ : goldBalance_)+=int(pos->gold);
        (enemy ? enemyMana_ : mana_)+=int(pos->mana);
        (enemy ? enemyPopulation_ : population_)-=int(stat(type,"population"));
        q.units.erase(pos);
        if (first) q.elapsed=0;
        return true;
    }
    const double handicap=enemy ? rules_.at("enemy.handicap") : 1;
    const double price=stat(type,"gold")*handicap,magic=stat(type,"mana")*handicap;
    const int slots=int(stat(type,"population"));
    int& gold=enemy ? enemyGold_ : goldBalance_;
    int& mana=enemy ? enemyMana_ : mana_;
    int& population=enemy ? enemyPopulation_ : population_;
    if (gold<price || mana<magic || population+slots>rules_.at("populationLimit")) return false;
    gold=int(gold-price); mana=int(mana-magic); population+=slots;
    q.units.push_back({type,price,magic});
    return true;
}
bool CampaignTutorial::productionPress(Point p) {
    if (!trainingAvailable_) return false;
    for (const auto& slot:buttons) {
        if (slot.type==enslavedGiant && mission_<6) continue;
        if (slot.type==archer && mission_<2) continue;
        if (slot.type==ninja && mission_<3) continue;
        if ((slot.type==wizard || slot.type==healer) && mission_<4) continue;
        if ((slot.type==spearton && !mission_) || (slot.type==miner && step_<Step::Commands)) continue;
        const auto button=sprite(slot.icon,1,slot.origin).target;
        if (!button.contains(p)) continue;
        const auto cross=art_.texture(1874).bounds;
        const auto cancel=sprite(1874,1,{button.x+button.w-cross.w,button.y}).target;
        const auto& q=production_[0][slot.building];
        const bool queued=std::any_of(q.units.begin(),q.units.end(),[&](const Training& t){return t.symbol==slot.type;});
        const bool refund=queued && cancel.contains(p);
        if (recruit(false,slot.building,refund,slot.type)) {
            sound(refund ? "clickButton" : "UnitMake",{},false);
            if (step_==Step::Train && !refund) step_=Step::Training;
            if (step_==Step::Training && production_[0][1].units.empty() && refund) step_=Step::Train;
        } else if (!refund) sound("UnitMakeFail",{},false);
        return true;
    }
    return false;
}
void CampaignTutorial::drawProduction(std::vector<Draw>& out) const {
    if (!trainingAvailable_) return;
    for (const auto& slot:buttons) {
        if (slot.type==enslavedGiant && mission_<6) continue;
        if (slot.type==archer && mission_<2) continue;
        if (slot.type==ninja && mission_<3) continue;
        if ((slot.type==wizard || slot.type==healer) && mission_<4) continue;
        if ((slot.type==spearton && !mission_) || (slot.type==miner && step_<Step::Commands)) continue;
        auto button=sprite(slot.icon,1,slot.origin);
        const auto rect=button.target;
        out.push_back(std::move(button));
        const auto& q=production_[0][slot.building];
        const auto count=std::count_if(q.units.begin(),q.units.end(),[&](const Training& t){return t.symbol==slot.type;});
        if (!count) continue;
        if (q.units.front().symbol==slot.type)
            out.push_back(timerOverlay({rect.x,rect.y-1,rect.w,rect.h+1},q.elapsed/stat(slot.type,"cooldown")));
        out.push_back(Draw::label({rect.x+25,rect.y+15,25,25},std::to_string(count),20,0xffffff));
        out.push_back(sprite(1874,1,{rect.x+rect.w-art_.texture(1874).bounds.w,rect.y}));
    }
}
void CampaignTutorial::updateProduction() {
    for (int side=0;side<2;++side) for (int b=0;b<int(production_[side].size());++b) {
        auto& q=production_[side][b];
        if (q.units.empty()) continue;
        const int type=q.units.front().symbol;
        if (q.elapsed<=stat(type,"cooldown")) {++q.elapsed; continue;}
        q.elapsed=0; q.units.pop_front();
        const double base=side ? length_-home : home,dir=side ? -1. : 1.;
        const double x=b==0 ? base : base+dir*(rules_.at("barracks.spawnX")-home);
        auto& u=units_[spawn(type,{x,100},side!=0)];
        if (!side) sound("UnitReady",{},false);
        const double dx=b==0 ? 850+random()*4.656612875245797e-10*40-20 : 1000;
        u.goal={base+dir*dx,depth_/2+random()*4.656612875245797e-10*60-30};
        u.order=Order::Move; u.engageOnMove=true;
        if (orders_[side]==ArmyOrder::Garrison) {
            u.garrisoned=true; u.goal={base-dir*home/3,depth_/2};
        }
        if (!side && b==1 && step_<Step::Commands) trainingAvailable_=false;
    }
}

bool CampaignTutorial::reserveOre(Unit& u,int resource) {
    if (resource>=0 && !oreRemaining_[resource]) return false;
    const int capacity=resource<0 ? int(rules_.at("maxWorshipers")) : 2;
    for (int slot=0;slot<capacity;++slot) {
        const bool taken=std::any_of(units_.begin(),units_.end(),[&](const Unit& other){
            return &u!=&other && other.hp>0 && isWorker(other.symbol) && other.resource==resource &&
                   other.miningSlot==slot && (resource>=0 || other.enemy==u.enemy);
        });
        if (taken) continue;
        u.resource=resource; u.miningSlot=slot; u.garrisoned=false;
        u.order=Order::Gather; u.engageOnMove=false; u.target=-1;
        if (u.pose.starts_with("attack_")) animate(u,"run");
        return true;
    }
    return false;
}
void CampaignTutorial::assignFreeMine(Unit& u) {
    // Map.createMiningBlock sorts deposits on x. MinerAi visits the home side first.
    for (size_t i=0;i<mineOrder_.size();++i) {
        const int index=mineOrder_[u.enemy ? mineOrder_.size()-1-i : i];
        if (reserveOre(u,index)) return;
    }
}
void CampaignTutorial::orderArmy(bool enemy,ArmyOrder order,double destination) {
    const double base=enemy ? length_-home : home,dir=enemy ? -1. : 1.;
    orders_[enemy]=order;
    if (!enemy) sound(order==ArmyOrder::Garrison ? "GarrisonEnter" : order==ArmyOrder::Defend ? "GarrisonExit" : "CommandMove1",{},false);
    std::vector<Unit*> formation;
    for (auto& u:units_) {
        if (u.enemy!=enemy || u.hp<=0) continue;
        u.garrisoned=order==ArmyOrder::Garrison;
        if (!u.garrisoned && isWorker(u.symbol)) {
            if (u.resource!=-2) { u.order=Order::Gather; continue; }
            if (order==ArmyOrder::Attack) {
                if (dir*(u.position.x-base)<900) {u.goal={base+dir*900,100}; u.order=Order::Move;}
                continue;
            }
        }
        u.engageOnMove=!u.garrisoned && (order==ArmyOrder::Attack || dir*(u.position.x-destination)<=0);
        if (enemy && u.engageOnMove && u.order==Order::Attack && combatTarget(u.target).hp>0) continue;
        formation.push_back(&u);
    }
    std::stable_sort(formation.begin(),formation.end(),[](auto* a,auto* b){
        return !isWorker(a->symbol) && isWorker(b->symbol);
    });
    moveFormation(formation,{destination,depth_/2});
}

void CampaignTutorial::updateOpponent() {
    std::array<int,2> military{};
    std::array<std::vector<double>,2> positions;
    std::vector<Unit*> working,praying;
    for (auto& u:units_) {
        if (u.hp<=0) continue;
        if (!isWorker(u.symbol)) {
            military[u.enemy]+=int(stat(u,"population"));
            positions[u.enemy].push_back(u.position.x);
        } else if (u.enemy) {
            if (u.resource==-2) assignFreeMine(u);
            if (u.resource==-1) praying.push_back(&u);
            if (u.resource>=0) working.push_back(&u);
        }
    }
    for (int side=0;side<2;++side) {
        auto& xs=positions[side];
        if (xs.empty()) continue;
        auto middle=xs.begin()+xs.size()/2;
        std::nth_element(xs.begin(),middle,xs.end());
        median_[side]=*middle;
    }
    const auto workers=working.size()+praying.size(),worshippers=workers/3;
    if (praying.size()<worshippers) reserveOre(*working.front(),-1);
    if (praying.size()>worshippers) assignFreeMine(*praying.front());
    const bool weak=military[0]+(castleArcher_ ? 4 : 0)<military[1];
    const bool even=military[1] && std::abs(military[0]-military[1])<military[1]*.1;
    const double aggression=median_[0]/length_;
    double destination=length_-home-900;
    ArmyOrder order=ArmyOrder::Defend;
    if (!mission_ && step_==Step::Free) {
        order=ArmyOrder::Attack; destination=statues_[0].position.x;
        if (military[1]*2>military[0]) enemyMana_=0;
    } else if (military[1]) {
        if (weak) { order=ArmyOrder::Attack; destination=median_[1]-250; }
        else if (even) { order=ArmyOrder::Attack; destination=std::max(length_/2,median_[1]-250); }
        else if (aggression<=.4) { order=ArmyOrder::Attack; destination=length_/2; }
    }
    orderArmy(true,order,destination);
    if (aggression<.6 && (enemyPopulation_<6 || weak) && workers<rules_.at("composition.Miner") &&
        production_[1][0].units.empty()) recruit(true,0);
    // The first level's military build order contains only the Spearton. After
    // meeting its composition target the original repeats that same build order.
    if (mission_==6) {
        auto count=[&](int type){return std::count_if(units_.begin(),units_.end(),[&](const Unit& u){return u.enemy && u.hp>0 && u.symbol==type;});};
        const bool full=count(giant)>=rules_.at("composition.Giant") && count(bomber)>=rules_.at("composition.Bomber");
        if (production_[1][5].units.empty() && (full || count(giant)<rules_.at("composition.Giant"))) recruit(true,5,false,giant);
        if (military[1]>=6 && production_[1][1].units.empty() && (full || count(bomber)<rules_.at("composition.Bomber"))) recruit(true,1,false,bomber);
    } else if (mission_==5) {
        // CampaignCutScene1 resets enemy gold to zero each tick.
    } else if (!mission_) {
        if (production_[1][1].units.empty()) recruit(true,1);
    } else if (mission_==4) {
        struct Desired {int type,building; const char* name;};
        const Desired roster[]={{wizard,3,"Magikill"},{spearton,1,"Spearton"},{sword,1,"Swordwrath"},
            {archer,2,"Archidon"},{healer,4,"Monk"},{ninja,1,"Ninja"}};
        auto count=[&](int type){return std::count_if(units_.begin(),units_.end(),[&](const Unit& u){return u.enemy && u.hp>0 && u.symbol==type;});};
        const bool full=std::all_of(std::begin(roster),std::end(roster),[&](const Desired& entry){
            return count(entry.type)>=rules_.at(std::string("composition.")+entry.name);
        });
        for (const auto& entry:roster)
            if (production_[1][entry.building].units.empty() && (full || count(entry.type)<rules_.at(std::string("composition.")+entry.name)))
                recruit(true,entry.building,false,entry.type);
    } else if (mission_==3) {
        const int wizards=int(std::count_if(units_.begin(),units_.end(),[](const Unit& u){return u.enemy && u.hp>0 && u.symbol==wizard;}));
        const int healers=int(std::count_if(units_.begin(),units_.end(),[](const Unit& u){return u.enemy && u.hp>0 && u.symbol==healer;}));
        const bool full=wizards>=rules_.at("composition.Magikill") && healers>=rules_.at("composition.Monk");
        if (production_[1][3].units.empty() && (full || wizards<rules_.at("composition.Magikill"))) {
            recruit(true,3,false,wizard);
        }
        if (production_[1][4].units.empty() && (full || healers<rules_.at("composition.Monk"))) recruit(true,4,false,healer);
    } else {
        const int swords=int(std::count_if(units_.begin(),units_.end(),[](const Unit& u){return u.enemy && u.hp>0 && u.symbol==sword;}));
        const int type=mission_==2 ? ninja : archer;
        const int building=mission_==2 ? 1 : 2;
        const double desired=rules_.at(mission_==2 ? "composition.Ninja" : "composition.Archidon");
        const int count=int(std::count_if(units_.begin(),units_.end(),[&](const Unit& u){return u.enemy && u.hp>0 && u.symbol==type;}));
        const bool full=swords>=rules_.at("composition.Swordwrath") && count>=desired;
        if (production_[1][1].units.empty() && (full || swords<rules_.at("composition.Swordwrath"))) recruit(true,1);
        if (production_[1][building].units.empty() && (full || count<desired)) recruit(true,building,false,type);
    }
}
void CampaignTutorial::simulateActors(bool strategy) {
    if (statues_[1].hp<=0 || statues_[0].hp<=0) {
        step_=statues_[1].hp<=0 ? Step::Victory : Step::Defeat; counter_=0;
        return;
    }
    if (step_>=Step::Commands) updateBattle(strategy);
    for (auto& u:units_) updateUnit(u);
    if (castleArcher_) updateCastleArcher();
    if (enemyCastleArcher_) updateCastleArcher(true);
    if (chaosOpponent_) updateCastleDead();
    updateMobileArrows();
    updateBoulders();
    if (mission_>=3) updateMagic();
    updateProduction();
}
void CampaignTutorial::updateBattle(bool strategy) {
    if (!mission_ && !reinforcement_ && statues_[1].hp<statues_[1].maxHp*.75) {
        const auto id=spawn(spearton,{length_-home,depth_/2},true);
        enemyPopulation_+=int(stat(units_[id],"population")); reinforcement_=true;
    }
    if (strategy) updateOpponent();
    if (strategy && mission_==6) updateBomberController();
    for (auto& u:units_) {
        if (!u.canFight || !u.engageOnMove || u.garrisoned || u.hp<=0 || u.pose.starts_with("attack_")) continue;
        const auto& statue=statues_[!u.enemy];
        auto squared=[&](const Unit& target){
            const double x=target.position.x-u.position.x,y=target.position.y-u.position.y;
            return x*x+y*y;
        };
        double best=640000;
        int closest=-1;
        for (int i=0;i<int(units_.size());++i) {
            const auto& other=units_[i];
            if (other.enemy==u.enemy || other.hp<=0 || other.cloakTicks || (other.garrisoned &&
                (other.enemy ? other.position.x>=length_-home : other.position.x<=home))) continue;
            const double distance=squared(other);
            if (distance<best) {best=distance; closest=i;}
        }
        if (closest<0 && squared(statue)<best) closest=u.enemy ? ownStatue : opposingStatue;
        if (mission_ && u.enemy && wall_.hp>0 && u.position.x>wall_.position.x && squared(wall_)<640000 &&
            (closest==-1 || combatTarget(closest).position.x<wall_.position.x)) closest=ownWall;
        if (closest!=-1) { u.order=Order::Attack; u.target=closest; }
        else if (u.order==Order::Attack) {u.order=Order::Move; u.target=-1;}
    }
}
void CampaignTutorial::skipTutorial() {
    spawnGroup(false,{miner,miner,sword});
    if (difficulty_==Difficulty::Normal) spawnGroup(true,{miner,miner,miner});
    else spawnGroup(true,{miner,miner,miner,miner,spearton});
    orderArmy(false,ArmyOrder::Defend,home+900);
    goldBalance_=500; enemyGold_=150; trainingAvailable_=true;
    castleArcher_=researchStarted_=true; step_=Step::Free;
    displayed_=voiceTicks_=0; nextVoice_.clear();
}
}
