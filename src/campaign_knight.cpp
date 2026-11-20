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
    rangeSelected_=false;selectedBuilding_=0;aimedSpell_=0;aimedAttackMove_=false;
    for (auto& unit:units_)
        unit.selected=!unit.enemy && unit.hp>0 && !isWorker(unit.symbol) && !unit.garrisoned;
    if (center && frontLine()>0) targetCamera_=frontLine()-legacyWidth/2;
}
void CampaignTutorial::minerOrder(bool garrison) {
    if (ended() || cinematic_>=0 || step_<Step::Commands) return;
    std::vector<Unit*> workers;
    for (auto& unit:units_) if (!unit.enemy && unit.hp>0 && isWorker(unit.symbol)) workers.push_back(&unit);
    if (garrison) moveFormation(workers,{legacyWidth-legacyWidth/3,depth_/2});
    for (auto* unit:workers) {
        unit->garrisoned=garrison;unit->engageOnMove=false;
        if (!garrison) {
            if (unit->resource!=-2) unit->order=Order::Gather;
            else {
                unit->goal={legacyWidth+900,depth_/2};unit->order=Order::Move;unit->engageOnMove=true;
            }
        }
    }
    sound(garrison ? "manthefortSoundOrder" : "defendSoundOrder",{},false);
}
}
