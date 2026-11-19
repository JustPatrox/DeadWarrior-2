#include "campaign_tutorial.hpp"

namespace sw2 {
void CampaignTutorial::updateKnightController() {
    if (knightHintTicks_>=0) {
        if (knightHintTicks_++>30*5) knightHintTicks_=-1;
    } else if (!knightHintTriggered_ && frontLine()>length_/2) {
        knightHintTriggered_=true;knightHintTicks_=0;
    }
}
void CampaignTutorial::selectArmy(bool center) {
    if (ended() || cinematic_>=0) return;
    rangeSelected_=false;aimedSpell_=0;aimedAttackMove_=false;
    for (auto& unit:units_)
        unit.selected=!unit.enemy && unit.hp>0 && !isWorker(unit.symbol) && !unit.garrisoned;
    if (center && frontLine()>0) targetCamera_=frontLine()-legacyWidth/2;
}
}
