#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>

namespace sw2 {
bool CampaignTutorial::spatialAreaContains(Point center,double halfExtent,Point entity) const {
    const double boxY=depth_/7;
    const int cols=int(length_/50),rows=7,partitions=int(length_/50*rows);
    const int cx=int(entity.x/50),cy=int(entity.y/boxY);
    if(cx<0||cx>=cols||cy<0||cy>=rows)return false;
    std::array<int,5> cells{};int count=1;cells[0]=cy*cols+cx;
    if(cx>0)cells[count++]=cy*cols+cx-1;
    if(cx<cols-1)cells[count++]=cy*cols+cx+1;
    if(cy>0)cells[count++]=(cy-1)*cols+cx;
    if(cy<rows-1)cells[count++]=(cy+1)*cols+cx;
    for(int x=int((center.x-halfExtent)/50);x<(center.x+halfExtent)/50;++x)
        for(int y=int((center.y-halfExtent)/boxY);y<(center.y+halfExtent)/boxY;++y) {
            const int cell=y*cols+x;
            if(cell>=0&&cell<partitions&&std::find(cells.begin(),cells.begin()+count,cell)!=cells.begin()+count)return true;
        }
    return false;
}

bool CampaignTutorial::prepareTacticalCheck(int which) {
    if(mission_<6 || which<1 || which>5)return false;
    units_.clear();flights_.clear();boulders_.clear();magic_.clear();towerEffects_.clear();
    for(auto& side:production_)for(auto& queue:side)queue={};
    diagnosticFixture_=true;ticks_=1;step_=Step::Free;wall_.hp=0;cinematic_=-1;
    fastForward_=false;dragStart_.reset();selectionDragging_=false;rangeSelected_=false;
    aimedSpell_=0;aimedAttackMove_=false;population_=enemyPopulation_=0;
    goldBalance_=2000;mana_=1000;hint_=0;towerControl_=50;towerSpawnIndex_=-1;towerSpawnTime_=0;
    if(which<=3)camera_=targetCamera_=2000;
    if(which==1) {
        spawn(enslavedGiant,{2400,125});spawn(spearton,{2500,125},true);
        units_[0].selected=true;units_[0].hold=true;units_[0].facing=-1;
        units_[1].canFight=false;units_[1].hp=units_[1].maxHp=5000;
    } else if(which==2) {
        spawn(healer,{2400,125});spawn(spearton,{2440,125});
        units_[0].selected=true;units_[1].canFight=false;units_[1].hp=100;
    } else if(which==3) {
        spawn(spearton,{2400,125});units_[0].selected=true;units_[0].canFight=false;upgrades_.set(4);
    } else {
        camera_=targetCamera_=length_/2-425;
        spawn(spearton,{length_/2,depth_/2});units_[0].canFight=false;
        upgrades_.set(19);upgrades_.set(18,which==5);
    }
    return true;
}

void CampaignTutorial::updateHill() {
    // Hill.registerUnit receives the broad-phase SpatialHash result, not an
    // additional exact 300x300 hit test. Entities occupy their own cell and
    // its four orthogonal neighbours (50 by battlefieldHeight/7).
    std::array<bool,2> present{};
    for(const auto& u:units_)if(u.hp>0 && !u.garrisoned) {
        if(spatialAreaContains({length_/2,depth_/2},150,u.position))present[u.enemy]=true;
    }
    const double previous=towerControl_;
    if(present[0]!=present[1])towerControl_=std::clamp(towerControl_+(present[0]?-1:1)*50/rules_.at("towerCaptureTime"),0.,100.);
    if(previous!=0 && towerControl_==0)sound("SelectRaceSound",{},false);
    const int frame=ticks_-1;
    if(frame%int(rules_.at("hill.frequency"))==0) {
        if(towerControl_==0)goldBalance_+=int(rules_.at("hill.goldCollectionRate"));
        if(towerControl_==100)enemyGold_+=int(rules_.at("hill.goldCollectionRate"));
    }
    for(auto& effect:towerEffects_)++effect.tick;
    std::erase_if(towerEffects_,[&](const TowerEffect& e){return e.tick>=int(rules_.at("effect."+std::to_string(e.symbol)+".frames"))-1;});
    if(towerControl_==0 && upgrades_[19]) {
        if(towerSpawnIndex_>=0) {
            if(units_[towerSpawnIndex_].hp<=0)towerSpawnIndex_=-1;
            towerSpawnTime_=frame;
        } else if(frame-towerSpawnTime_>int(rules_.at("towerSpawnDelay"))) {
            const int type=upgrades_[18] ? enslavedGiant : spearton;
            towerSpawnIndex_=spawn(type,{length_/2,depth_/2});
            auto& u=units_[towerSpawnIndex_];u.towerSpawned=true;u.engageOnMove=true;
            u.goal=statues_[1].position;u.order=Order::Move;
            towerEffects_.push_back({3732,0,u.position,upgrades_[18]?1.5:1.});
            sound("GhostTower"+std::to_string(1+soundSequence_++%2),{},false);
        }
    }
    if(frame%60==0)for(const auto& u:units_)if(u.hp>0 && u.towerSpawned)
        towerEffects_.push_back({4027,0,u.position,1});
}

void CampaignTutorial::drawHill(std::vector<std::pair<double,Draw>>& layers) const {
    const double size=(rules_.at("backScale")+rules_.at("frontScale"))/2;
    const Point at{length_/2-camera_,battlefieldY_+depth_/2};
    layers.emplace_back(depth_/2,sprite(1162,1,at,size));
    auto flame=sprite(1158,1+ticks_%int(rules_.at("hill.flameFrames")),{at.x+2.5*size,at.y-10.3*size},size);
    flame.alpha=std::abs(towerControl_-50)/50;flame.redOffset=towerControl_>50?255:0;
    layers.emplace_back(depth_/2,std::move(flame));
    if(towerControl_!=0 && towerControl_!=50 && towerControl_!=100) {
        const double y=at.y+rules_.at("hill.barY");
        // HillControlBar: source 30x5 Sprite, doubled on both axes in Hill.init.
        layers.emplace_back(depth_/2,Draw::shape(Draw::Kind::Box,{at.x-30,y-5,60,10},0,.5));
        const double width=60*(towerControl_-50)/100;
        layers.emplace_back(depth_/2,Draw::shape(Draw::Kind::Box,{at.x+std::min(0.,width),y-5,std::abs(width),10},0xff0000));
    }
    for(const auto& effect:towerEffects_)layers.emplace_back(effect.position.y,
        sprite(effect.symbol,1+effect.tick,{effect.position.x-camera_,battlefieldY_+effect.position.y},effect.scale));
}
}
