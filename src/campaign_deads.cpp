#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sw2 {
namespace {constexpr double radians=std::numbers::pi/180;}

void CampaignTutorial::updateDeadsController() {
    if (deadsHintTicks_>=0) {
        if (deadsHintTicks_++>30*5) deadsHintTicks_=-1;
    } else if (!deadsHintTriggered_ && std::any_of(units_.begin(),units_.end(),[](const Unit& u){
        return !u.enemy && u.hp>0 && u.poison>0;
    })) {
        deadsHintTriggered_=true;deadsHintTicks_=0;
    }
}

Point CampaignTutorial::updateDead(Unit& u) {
    const bool attacking=u.pose.starts_with("attack_");
    const bool engage=u.canFight && u.order==Order::Attack && u.target!=-1 && !u.garrisoned;
    if (engage) {
        const auto& target=combatTarget(u.target);
        const double dx=target.position.x-u.position.x,distance=std::abs(dx);
        const double speed=stat(u,"arrowVelocity"),gravity=rules_.at("gravity");
        const double discriminant=std::pow(speed,4)-gravity*gravity*distance*distance;
        const bool inRange=target.hp>0 && distance<=stat(u,"maximumRange") && discriminant>0;
        if (!attacking || inRange) {
            const double theta=inRange ? std::atan2(speed*speed-std::sqrt(discriminant),gravity*distance) : -1.35;
            const double residual=std::remainder(-theta/radians-u.bowAngle,360.);
            u.bowAngle+=residual*(std::abs(residual)<10 ? .8 : .1);
            const double home=u.enemy ? length_-legacyWidth : legacyWidth;
            if (!attacking && !u.stunned && inRange && ticks_-u.lastHitTick>45 &&
                (u.position.x-home)*(u.enemy ? -1 : 1)>=200 &&
                std::abs(std::remainder(-theta/radians-u.bowAngle,360.))<.5) {
                u.facing=dx<0 ? -1 : 1;animate(u,"attack_1");u.hit=false;
                u.lastHitTick=ticks_;u.projectileTarget=u.target;
            }
        }
        // Dead inherits RangedUnit.isLoaded()==true; the original kiting
        // branch requires !isLoaded(), even when its team enables mayKite.
        if (inRange) {u.facing=dx<0 ? -1 : 1;return {};}
        if (!attacking && !u.stunned && !u.hold)
            return {(u.goal.x-u.position.x)/100,(u.goal.y-u.position.y)/100};
    }
    if (!attacking && !u.stunned && !u.hold && u.order==Order::Move)
        return {(u.goal.x-u.position.x)/100,(u.goal.y-u.position.y)/100};
    return {};
}

void CampaignTutorial::throwGuts(Unit& u) {
    const auto& pose=animations_.at({dead,u.pose});const double size=scale(u);
    const double speed=stat(u,"arrowVelocity"),gravity=rules_.at("gravity");
    Flight shot;shot.enemy=u.enemy;shot.castle=false;shot.guts=true;
    shot.x=u.position.x+u.facing*pose.origin.x*size;shot.depth=u.position.y;shot.z=pose.origin.y*size;
    shot.dx=u.facing*speed*std::cos(u.bowAngle*radians);shot.dz=speed*std::sin(u.bowAngle*radians);
    if (u.projectileTarget!=-1) {
        const auto& target=combatTarget(u.projectileTarget);
        const double distance=std::abs(target.position.x-u.position.x);
        const double discriminant=std::pow(speed,4)-gravity*gravity*distance*distance;
        const double theta=discriminant>0 && distance<=stat(u,"maximumRange") ?
            std::atan2(speed*speed-std::sqrt(discriminant),gravity*distance) : -1.35;
        const double vz=speed*std::sin(theta),time=(vz+std::abs(vz))/gravity;
        shot.dy=(target.position.y-u.position.y)/(std::abs(time)<.001 ? 5 : time);
        auto& mana=u.enemy ? enemyMana_ : mana_;
        if (u.enemy && mana>stat(u,"poison.mana") && target.hp>0 && !target.poison &&
            target.symbol!=1904 && target.symbol!=1174) {
            shot.poison=stat(u,"poison.damage");mana-=int(stat(u,"poison.mana"));
        }
    }
    shot.spin=random()%4-2;shot.angle=random()%360;
    flights_.push_back(shot);++gutsShots_;
}
}
