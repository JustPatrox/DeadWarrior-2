#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sw2 {
namespace {constexpr double radians=std::numbers::pi/180;}

void CampaignTutorial::updateBomberController() {
    // CampaignGameScreen runs its controller before the next simulation update.
    // Keep frame zero and the controller's once-per-update fast-forward behavior.
    const int frame=ticks_-1;
    if (frame%(30*45)==0) {
        for (int i=0;i<nextBomberWave_;++i) {
            const int id=spawn(bomber,{statues_[1].position.x,depth_/2},true);
            units_[id].hold=true;units_[id].order=Order::Stand;
            ++enemyPopulation_;
        }
        ++bomberWaves_;
    }
    // GIANT_GROWTH_I/II (-47/-48) are not CHAOS_GIANT_GROWTH_I/II
    // (-50/-51). The original controller's flags do not buff Chaos Giants.
    if (frame%(30*60)==0) nextBomberWave_=std::min(nextBomberWave_+1,10);
    orderArmy(true,ArmyOrder::Attack,statues_[0].position.x);
    for (auto& unit:units_) if (unit.enemy && unit.symbol==bomber) unit.hold=false;
}

void CampaignTutorial::detonate(Unit& u) {
    magic_.push_back({3172,1,u.position,u.position,u.enemy,u.position.y,stat(u,"explosionDamage")});
    sound("mediumExplosion"+std::to_string(1+soundSequence_++%3),u.position);
    ++explosions_;
}

Point CampaignTutorial::updateEnslavedGiant(Unit& u) {
    const bool attacking=u.pose.starts_with("attack_");
    const bool engage=u.canFight && u.order==Order::Attack && u.target!=-1 && !u.garrisoned;
    if (engage) {
        const auto& target=combatTarget(u.target);
        const double dx=target.position.x-u.position.x,distance=std::abs(dx);
        const double speed=stat(u,"projectileVelocity"),gravity=rules_.at("gravity");
        // _mc carries the perspective scale; the Unit container does not.
        // EnslavedGiant derives its aim offsets with localToGlobal/globalToLocal.
        const double bodyScale=scale(u),aimX=50*bodyScale,aimZ=-90*bodyScale+25;
        const double x=distance-aimX;
        const double discriminant=std::pow(speed,4)-gravity*(gravity*x*x+2*aimZ*speed*speed);
        const double rangeDiscriminant=std::pow(speed,4)-gravity*(gravity*std::pow(distance+aimX,2)+2*aimZ*speed*speed);
        const bool inRange=target.hp>0 && distance<=stat(u,"maximumRange") && rangeDiscriminant>0 && discriminant>0;
        if (inRange) {
            const double theta=std::atan2(speed*speed-std::sqrt(discriminant),gravity*x);
            u.bowAngle=-theta/radians;u.facing=dx<0 ? -1 : 1;
            const double home=u.enemy ? length_-legacyWidth : legacyWidth;
            if (!attacking && (u.position.x-home)*(u.enemy ? -1 : 1)>=200) {
                animate(u,"attack_1");u.hit=false;
            }
            if (!attacking && !u.pose.starts_with("attack_") && !u.hold) {
                const double approach=dx-100*(u.enemy ? -1 : 1);
                return {approach*dx<0 ? 0 : approach/100,0};
            }
            return {};
        }
        // EnslavedGiantAi resumes its commanded formation when the target is
        // outside throwing range; it does not pursue that target instead.
        if (!attacking && !u.stunned && !u.hold) return {(u.goal.x-u.position.x)/100,(u.goal.y-u.position.y)/100};
    }
    if (!attacking && !u.stunned && !u.hold && u.order==Order::Move)
        return {(u.goal.x-u.position.x)/100,(u.goal.y-u.position.y)/100};
    return {};
}

void CampaignTutorial::throwBoulder(Unit& u) {
    BoulderFlight shot;
    shot.enemy=u.enemy;shot.x=u.position.x+u.facing*50*scale(u);shot.depth=u.position.y;shot.z=-90*scale(u);
    const double speed=stat(u,"projectileVelocity"),gravity=rules_.at("gravity"),theta=-u.bowAngle*radians;
    shot.dx=u.facing*speed*std::cos(theta);shot.dz=-speed*std::sin(theta);
    if (u.target!=-1) {
        const auto& target=combatTarget(u.target);
        if (target.hp>0 && std::abs(target.position.x-u.position.x)<=stat(u,"maximumRange")) {
            const double aimZ=-90*scale(u)+25;
            const double time=(speed*std::sin(theta)+std::sqrt(std::max(0.,speed*speed*std::pow(std::sin(theta),2)-2*gravity*aimZ)))/gravity;
            shot.dy=(target.position.y-u.position.y)/(std::abs(time)<.001 ? 5 : time);
        }
    }
    shot.spin=random()%4-2;shot.rotation=random()%360;
    boulders_.push_back(shot);++boulderShots_;
}

void CampaignTutorial::updateBoulders() {
    std::vector<BoulderFlight> fragments;
    for (auto& shot:boulders_) {
        if (shot.hidden || shot.struck) {++shot.age;continue;}
        const double oldX=shot.x,oldY=shot.depth+shot.z;
        shot.drawPosition={oldX,oldY};shot.drawDepth=shot.depth;shot.drawRotation=shot.rotation;
        const double vx=shot.dx,vy=shot.dy,vz=shot.dz;
        const double size=shot.size*(rules_.at("backScale")+shot.depth/depth_*(rules_.at("frontScale")-rules_.at("backScale")));
        shot.x+=shot.dx;shot.depth+=shot.dy;shot.z+=shot.dz;shot.dz+=rules_.at("gravity");
        const double angle=std::atan2(shot.dz+shot.dy,shot.dx);
        if (shot.debris) shot.drawRotation=angle/radians;
        auto contains=[&](const Unit& unit,double x,double y,bool arrow=true) {
            const bool structure=unit.symbol==1904 || unit.symbol==1174;
            const double width=structure ? unit.width : stat(unit,"hitWidth");
            const double height=structure ? unit.height : stat(unit,"hitHeight");
            return std::abs(x-unit.position.x)<width && y-unit.position.y>-height && y-unit.position.y<(arrow ? 0 : height);
        };
        auto contact=[&](const Unit& unit) {
            if (unit.hp<=0 || unit.enemy==shot.enemy || unit.cloakTicks ||
                std::abs(shot.depth-unit.position.y)>150 ||
                (unit.garrisoned && (unit.enemy ? unit.position.x>=length_-legacyWidth : unit.position.x<=legacyWidth))) return false;
            for (double x:{-10.,40.,90.}) {
                const double px=oldX+size*(x*std::cos(angle)-40*std::sin(angle));
                const double py=oldY+size*(x*std::sin(angle)+40*std::cos(angle));
                if (contains(unit,px,py)) return true;
            }
            return false;
        };
        bool stop=shot.target==-1 && shot.z>0 && shot.dz>0;
        if (!shot.debris && !stop) {
            if (shot.target==-1) {
                for (int i=0;i<int(units_.size());++i) if (contact(units_[i])) {shot.target=i;break;}
                if (shot.target==-1 && contact(statues_[!shot.enemy])) shot.target=shot.enemy ? ownStatue : opposingStatue;
                if (shot.target==-1 && shot.enemy && wall_.hp>0 && contact(wall_)) shot.target=ownWall;
                if (shot.target!=-1) shot.previousDistance=std::abs(oldX-combatTarget(shot.target).position.x);
            } else {
                auto& target=combatTarget(shot.target);
                const double distance=std::abs(oldX-target.position.x);
                const double px=oldX+size*(90*std::cos(angle)-40*std::sin(angle));
                const double py=oldY+size*(90*std::sin(angle)+40*std::cos(angle));
                if (!contains(target,px,py,false) || distance>shot.previousDistance) {
                    if (target.hp>0) {
                        const bool structure=target.symbol==1904 || target.symbol==1174;
                        const double damage=stat(enslavedGiant,"damage")+stat(enslavedGiant,!structure && stat(target,"armoured") ? "toArmour" : "toNotArmour");
                        applyMagicDamage(target,damage,shot.enemy);impactSound(target);
                        if (!structure && target.symbol!=giant && target.symbol!=enslavedGiant) {
                            target.stunned=int(stat(enslavedGiant,"stunTime"));target.velocity.x+=shot.dx<0 ? -2 : 2;
                        }
                        ++boulderHits_;
                    }
                    stop=true;
                }
                shot.previousDistance=distance;
            }
        }
        if (stop) {
            shot.struck=true;shot.dx=shot.dy=shot.dz=0;
            if (!shot.debris) {
                shot.hidden=true;sound("BoulderSmashSound",{shot.x,shot.depth});
                for (int i=0;i<5;++i) {
                    BoulderFlight piece;
                    piece.x=shot.x;piece.depth=shot.depth;piece.z=shot.z;piece.size=.2;piece.debris=true;
                    piece.dx=vx*.6+random()*4.656612875245797e-10*4-2;
                    piece.dy=vy*.6+random()*4.656612875245797e-10*10-5;
                    piece.dz=vz*.6+random()*4.656612875245797e-10*4-2;
                    piece.spin=random()%4-2;piece.rotation=random()%360;
                    piece.drawPosition={piece.x,piece.depth+piece.z};piece.drawDepth=piece.depth;piece.drawRotation=piece.rotation;
                    fragments.push_back(piece);
                }
            }
        }
        shot.rotation+=shot.spin;
    }
    std::erase_if(boulders_,[](const BoulderFlight& shot){return shot.hidden || shot.age>1200;});
    boulders_.insert(boulders_.end(),fragments.begin(),fragments.end());
}

void CampaignTutorial::drawBoulders(std::vector<std::pair<double,Draw>>& layers) const {
    for (const auto& shot:boulders_) {
        if (shot.hidden || shot.drawDepth<0) continue;
        const double size=shot.size*(rules_.at("backScale")+shot.drawDepth/depth_*(rules_.at("frontScale")-rules_.at("backScale")));
        auto image=rotatedSprite(3970,1,{shot.drawPosition.x-camera_,battlefieldY_+shot.drawPosition.y},size,shot.drawRotation);
        layers.emplace_back(shot.drawDepth,std::move(image));
    }
}
}
