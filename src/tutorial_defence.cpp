#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sw2 {
namespace {
constexpr double home=850;
constexpr int swordsman=4922, worker=3104, lancer=6456;
constexpr double radians=std::numbers::pi/180;
}
CampaignTutorial::DefenceStatus CampaignTutorial::defenceStatus() const {
    const auto inside=std::count_if(units_.begin(),units_.end(),[](auto& u){return u.hp>0 && !u.enemy && u.garrisoned;});
    return {goldBalance_,mana_,int(inside),researchTicks_,shots_,impacts_,rangeSelected_,castleArcher_};
}
double CampaignTutorial::frontLine() const {
    double front=0;
    for (const auto& u:units_) if (!u.enemy && u.hp>0) front=std::max(front,u.position.x);
    return front;
}
bool CampaignTutorial::hitControl(const char* name,Point p) const {
    const auto& mask=controlMasks_.at(name);
    return std::any_of(mask.begin(),mask.end(),[p](auto& row){return row.contains(p);});
}
void CampaignTutorial::teamOrder(bool garrison) {
    sound(garrison ? "GarrisonEnter" : "GarrisonExit",{},false);
    std::vector<Unit*> formation;
    for (auto& u:units_) {
        if (u.enemy || u.hp<=0) continue;
        u.selected=false; u.garrisoned=garrison;
        if (!garrison && u.symbol==worker && u.resource!=-2) { u.order=Order::Gather; continue; }
        formation.push_back(&u);
    }
    // UnitMove.formationOrder sorts descending by unit type, then ascending by spawn ID.
    std::stable_sort(formation.begin(),formation.end(),[](auto a,auto b){
        return a->symbol!=worker && b->symbol==worker;
    });
    moveFormation(formation,{garrison ? home-home/3 : home+900,depth_/2});
    defending_=!garrison;
    orders_[0]=garrison ? ArmyOrder::Garrison : ArmyOrder::Defend;
}
bool CampaignTutorial::defencePress(Point p) {
    if (step_>=Step::Objective && step_<Step::Victory) {
        if (hitControl("garrisonButton",p)) { orderArmy(false,ArmyOrder::Garrison,home-home/3); return true; }
        if (hitControl("defendButton",p)) { orderArmy(false,ArmyOrder::Defend,home+900); return true; }
        if (hitControl("attackButton",p)) { orderArmy(false,ArmyOrder::Attack,statues_[1].position.x); return true; }
    }
    if (step_==Step::Garrison && hitControl("garrisonButton",p)) {
        teamOrder(true); return true;
    }
    if (step_==Step::Defend && hitControl("defendButton",p)) {
        teamOrder(false); rangeSelected_=false; return true;
    }
    if ((step_==Step::Range || mission_) && hitControl("fletcherHitArea",{p.x+camera_,p.y})) {
        if (!rangeSelected_) sound("MouseoverStructure",{},false);
        rangeSelected_=true;
        for (auto& u:units_) u.selected=false;
        return true;
    }
    if ((step_==Step::Upgrade || mission_) && rangeSelected_ && Rect{643,590,64,32}.contains(p)) {
        const auto cost=int(rules_.at("castleArchers1.cost")),manaCost=int(rules_.at("castleArchers1.mana"));
        if (!researchStarted_ && goldBalance_>=cost && mana_>=manaCost) {
            goldBalance_-=cost; mana_-=manaCost;
            researchStarted_=true;
            researchTicks_=int(rules_.at("castleArchers1.time"));
            sound("UnitMake",{},false);
        }
        return true;
    }
    return false;
}
void CampaignTutorial::updateDefence() {
    if (step_==Step::Garrison || step_==Step::Retreat) {
        for (auto& u:units_) u.selected=false;
    }
    if (step_==Step::Range) goldBalance_=400;
    if (researchTicks_>=0) {
        --researchTicks_;
    } else if (researchStarted_ && !castleArcher_) {
        // Tech.update marks research complete the tick after the countdown becomes negative.
        castleArcher_=true;
    }
    if (step_==Step::Defend || step_==Step::Fight) {
        auto& foe=units_.at(invader_);
        if (step_==Step::Defend) foe.hp=std::max(50.,foe.hp);
        if (foe.hp<=0) return;
        // The tutorial repeats an ATTACK_MOVE towards the castle in both
        // phases. A garrison leaves its statue exposed to the Spearton's spear.
        int closest=ownStatue;
        double square=640000;
        for (int i=0;i<int(units_.size());++i) {
            auto& u=units_[i];
            if (u.enemy || u.garrisoned || u.hp<=0 || u.position.x<=home) continue;
            const double dx=u.position.x-foe.position.x,dy=u.position.y-foe.position.y;
            const double distance=dx*dx+dy*dy;
            if (distance<square) {square=distance; closest=i;}
            if (defending_ && u.symbol==swordsman && distance<640000) {
                u.order=Order::Attack; u.target=invader_;
            }
        }
        if (!foe.pose.starts_with("attack_")) {
            foe.order=Order::Attack; foe.target=closest;
        }
    }
}
void CampaignTutorial::advanceDefence() {
    if (step_>=Step::Free) return;
    auto& enemy=units_[invader_];
    const bool narrated=displayed_==requestedMessage() && voiceTicks_==0;
    switch (step_) {
    case Step::Enemy:
        if (narrated && enemy.position.x<length_-home-900) step_=Step::Garrison;
        break;
    case Step::Garrison:
        if (std::none_of(units_.begin(),units_.end(),[](auto& u){return !u.enemy && u.hp>0 && !u.garrisoned;}))
            step_=Step::Retreat;
        break;
    case Step::Retreat: if (narrated && frontLine()<home+200) step_=Step::Range; break;
    case Step::Range: if (narrated && rangeSelected_) step_=Step::Upgrade; break;
    case Step::Upgrade: if (narrated && researchStarted_) step_=Step::Research; break;
    case Step::Research:
        if (narrated && castleArcher_) {
            enemy.position={home+700,depth_*3/4}; enemy.goal={home,depth_/2};
            enemy.order=Order::Move; step_=Step::Defend;
        }
        break;
    case Step::Defend: if (defending_) step_=Step::Fight; break;
    case Step::Fight:
        if (narrated && defending_ && enemy.hp<=0) {
            counter_=0; step_=Step::Commands;
            // Team.spawnUnitGroup places members at homeX + 100 for both factions.
            spawnGroup(false,{worker,worker});
            spawnGroup(true,{worker,worker,lancer});
            goldBalance_=500; trainingAvailable_=true;
            teamOrder(false);
        }
        break;
    case Step::Commands: if (++counter_>300) {step_=Step::Objective; counter_=0;} break;
    case Step::Objective: if (++counter_>300) {step_=Step::AttackWait; counter_=0;} break;
    case Step::AttackWait: if (++counter_>30*30) {step_=Step::AttackHint; counter_=0;} break;
    case Step::AttackHint: if (++counter_>30*7) {step_=Step::QualityWait; counter_=0;} break;
    case Step::QualityWait: if (++counter_>30*5) {step_=Step::QualityHint; counter_=0;} break;
    case Step::QualityHint: if (++counter_>30*7) {step_=Step::Free; counter_=0;} break;
    default: break;
    }
}

void CampaignTutorial::updateCastleArcher(bool enemy) {
    // The first CastleArchers.addUnit archer is at the back of the battlefield (py = 0).
    const double size=rules_.at("archer.scale")*1.1*rules_.at("backScale");
    const double direction=enemy ? -1. : 1.;
    const double x=enemy ? length_-home-180 : home+180,z=-390*size;
    auto& bowAngle=enemy ? enemyBowAngle_ : bowAngle_;
    auto& bowFrame=enemy ? enemyBowFrame_ : bowFrame_;
    auto& archerFrame=enemy ? enemyArcherFrame_ : archerFrame_;
    const double vx=rules_.at("archer.arrowVelocity"),gravity=rules_.at("gravity");
    int closest=-1;
    double best=rules_.at("archer.castleRange")*rules_.at("archer.castleRange")+depth_*depth_;
    for (int i=0;i<int(units_.size());++i) {
        const auto& u=units_[i];
        if (u.hp<=0 || u.enemy==enemy || u.cloakTicks || direction*(u.position.x-x)<=0) continue;
        const double dx=direction*(u.position.x-x),dist=dx*dx+u.position.y*u.position.y;
        if (dist<best && dx<=rules_.at("archer.castleRange")) { best=dist; closest=i; }
    }
    double theta=-1.2;
    if (closest>=0) {
        const double distance=direction*(units_[closest].position.x-x);
        const double discriminant=std::pow(vx,4)-gravity*(gravity*distance*distance+2*z*vx*vx);
        theta=discriminant>0 ? std::atan2(vx*vx-std::sqrt(discriminant),gravity*distance) : -1.35;
    }
    const double residual=std::remainder(-theta/radians-bowAngle,360.);
    bowAngle+=residual*(std::abs(residual)<10 ? .8 : .1);
    if (closest>=0 && theta!=-1.35 && bowFrame==1 && std::abs(std::remainder(-theta/radians-bowAngle,360.))<.5) {
        const double vz=vx*std::sin(theta);
        const double time=(vz+std::sqrt(std::max(0.,vz*vz-2*gravity*z)))/gravity;
        Flight shot;
        shot.enemy=enemy;
        shot.x=x+direction*(rules_.at("archer.bodyX")+rules_.at("archer.bowX"))*size;
        shot.z=z+(rules_.at("archer.bodyY")+rules_.at("archer.bowY"))*size;
        shot.dx=direction*vx*std::cos(bowAngle*radians); shot.dz=vx*std::sin(bowAngle*radians);
        shot.dy=units_[closest].position.y/(std::abs(time)<.001 ? 5 : time);
        advanceArrow(shot); advanceArrow(shot); // ProjectileManager.initArrow
        flights_.push_back(shot); ++(enemy ? enemyCastleShots_ : shots_); ++bowFrame;
        sound("launchArrow"+std::to_string(1+soundSequence_++%4),{x,0});
    }
    if (bowFrame!=1 && ++bowFrame>=int(rules_.at("archer.bowFrames"))) bowFrame=1;
    if (++archerFrame>int(rules_.at("archer.bodyFrames"))) archerFrame=2;
}
void CampaignTutorial::advanceArrow(Flight& a) {
    if (a.struck || a.hidden) { ++a.age; return; }
    const double x=a.x,y=a.depth+a.z;
    const double size=rules_.at("backScale")+a.depth/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
    a.x+=a.dx; a.depth+=a.dy; a.z+=a.dz; a.dz+=rules_.at("gravity");
    const Point tip{30*size*std::cos(a.angle*radians),30*size*std::sin(a.angle*radians)};
    auto damage=[&](Unit& unit,double base) {
        applyMagicDamage(unit,base,a.enemy);
    };
    bool area=false;
    if (a.target==-1) {
        for (int i=0;i<int(units_.size());++i) {
            const auto& u=units_[i];
            if (u.enemy==a.enemy || u.hp<=0 || u.cloakTicks || std::abs(a.depth-u.position.y)>=150 ||
                (u.garrisoned && (u.enemy ? u.position.x>=length_-home : u.position.x<=home))) continue;
            for (int pos:{0,-1,1}) {
                const double hitX=x+pos*tip.x-u.position.x,hitY=y+pos*tip.y-u.position.y;
                if (std::abs(hitX)<u.width && hitY>-u.height && hitY<0) {
                    a.target=i; a.previousDistance=std::abs(x-u.position.x); break;
                }
            }
            if (a.target>=0) break;
        }
        if (a.target==-1 && !a.castle) {
            const auto& s=statues_[!a.enemy];
            if (std::abs(x-s.position.x)<s.width && y>s.position.y-s.height && y<s.position.y) {
                a.target=a.enemy ? ownStatue : opposingStatue;
                a.previousDistance=std::abs(x-s.position.x);
            }
        }
    } else {
        auto& u=combatTarget(a.target);
        const double distance=std::abs(x-u.position.x);
        if (distance>a.previousDistance || std::abs(x+tip.x-u.position.x)>=u.width ||
            std::abs(y+tip.y-u.position.y)>=u.height) {
            const bool armored=u.symbol!=1904 && u.symbol!=1174 && stat(u,"armoured")!=0;
            const double hit=a.guts ? stat(dead,"damage")+stat(dead,armored ? "toArmour" : "toNotArmour") :
                a.castle ? rules_.at("archer.castleDamage")+rules_.at(armored ? "archer.castleToArmour" : "archer.castleToNotArmour") :
                stat(archer,"damage")+stat(archer,armored ? "toArmour" : "toNotArmour");
            if (u.hp>0) {
                damage(u,hit); impactSound(u);
                if (a.guts && u.symbol!=1904 && u.symbol!=1174 && !u.poison) {
                    u.poison=a.poison; sound("PoisonedSound",u.position);
                }
            }
            area=a.hidden=true; ++impacts_;
        }
        a.previousDistance=distance;
    }
    if (a.z>0 && a.dz>0 && a.target==-1) { a.struck=area=true; a.age=0; }
    else if (a.guts) a.angle+=a.spin;
    else a.angle=std::atan2(a.dz+a.dy,a.dx)/radians;
    if (a.castle && area) for (auto& u:units_) {
        if (u.enemy!=a.enemy && u.hp>0 && std::hypot(a.x-u.position.x,a.depth-u.position.y)<rules_.at("archer.castleArea"))
            damage(u,rules_.at("archer.castleAreaDamage"));
    }
}
Draw CampaignTutorial::rotatedSprite(int symbol,int frame,Point origin,double size,double angle) const {
    auto d=sprite(symbol,frame,origin,size);
    const double x=d.target.x+d.target.w/2-origin.x,y=d.target.y+d.target.h/2-origin.y;
    const double cos=std::cos(angle*radians),sin=std::sin(angle*radians);
    d.target.x=origin.x+x*cos-y*sin-d.target.w/2;
    d.target.y=origin.y+x*sin+y*cos-d.target.h/2;
    d.rotation=angle;
    return d;
}
void CampaignTutorial::drawCastleArcher(std::vector<std::pair<double,Draw>>& world,bool enemy) const {
    const double size=rules_.at("archer.scale")*1.1*rules_.at("backScale");
    if (enemy) {
        const double x=length_-home-180-camera_;
        const Point body{x-rules_.at("archer.bodyX")*size,battlefieldY_+(-390+rules_.at("archer.bodyY"))*size};
        world.emplace_back(0,sprite(4176,enemyArcherFrame_,body,size,true));
        const Point arms{body.x-rules_.at("archer.bowX")*size,body.y+rules_.at("archer.bowY")*size};
        auto bow=rotatedSprite(4168,enemyBowFrame_,arms,size,-enemyBowAngle_);
        bow.mirrored=true; bow.target.x=2*arms.x-bow.target.x-bow.target.w;
        world.emplace_back(0,std::move(bow));
        return;
    }
    Point body{home+180-camera_+rules_.at("archer.bodyX")*size,battlefieldY_+(-390+rules_.at("archer.bodyY"))*size};
    world.emplace_back(0,sprite(4176,archerFrame_,body,size));
    Point arms{body.x+rules_.at("archer.bowX")*size,body.y+rules_.at("archer.bowY")*size};
    world.emplace_back(0,rotatedSprite(4168,bowFrame_,arms,size,bowAngle_));
}
void CampaignTutorial::drawArrows(std::vector<std::pair<double,Draw>>& world) const {
    for (const auto& a:flights_) {
        if (a.hidden || a.depth<0) continue;
        const double scale=rules_.at("backScale")+a.depth/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
        world.emplace_back(a.depth,rotatedSprite(a.guts ? 3914 : 3988,a.guts ? 1 : a.struck ? 3 : 1,
            {a.x-camera_,battlefieldY_+a.z+a.depth},scale*(a.guts ? 2 : 1),a.angle));
    }
}
}
