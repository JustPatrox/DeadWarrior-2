#include "campaign_tutorial.hpp"
#include <algorithm>

namespace sw2 {
double CampaignTutorial::crawlerDamage(const Unit& unit) const {
    // Cat.checkForHit uses _damageToArmour for every target in the original.
    double damage=stat(unit,"damage")+stat(unit,"toArmour");
    if(unit.enemy && crawlerTech_) {
        const auto count=std::count_if(units_.begin(),units_.end(),[&](const Unit& other){
            return other.enemy==unit.enemy && other.symbol==crawler && (!other.deadCounted || other.hp>0);
        });
        // Cat stores the XML value 1.2 in an AS3 int field, so it contributes
        // one damage per Cat, rather than retaining the XML fraction.
        damage+=std::min(double(count),stat(unit,"pack.stacks"))*int(stat(unit,"pack.damagePerUnit"));
    }
    return damage;
}
}
