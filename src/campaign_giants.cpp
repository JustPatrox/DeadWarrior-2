#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace sw2 {
namespace {constexpr double radians=std::numbers::pi/180;}

void CampaignTutorial::loadGiants(const std::filesystem::path& folder,bool scripted) {
    std::ifstream dialogue(folder/"dialogue.tsv"),events(folder/"events.tsv");
    for (std::string line;std::getline(dialogue,line);) {
        std::istringstream fields(line);
        Dialogue entry;
        std::string seconds;
        std::getline(fields,entry.voice,'\t'); std::getline(fields,seconds,'\t'); std::getline(fields,entry.text);
        entry.ticks=int(std::ceil(std::stod(seconds)*30));
        dialogue_.push_back(std::move(entry));
    }
    int symbol,frame; std::string label;
    while (events>>symbol>>frame>>label) if (label=="hit") giantHitFrames_[{symbol,frame}]=true;
    if (!events.eof() || dialogue_.size()!=7 || giantHitFrames_.size()!=2)
        throw std::runtime_error("Incomplete Massive Battle controller");
    castleDead_.symbol=dead; castleDead_.enemy=true; castleDead_.facing=-1;
    castleDead_.position={length_-legacyWidth-110,-depth_/5}; castleDead_.altitude=390;
    if (scripted) {
        statues_[1].hp=180;
        sound("GiantGrowl"+std::to_string(1+soundSequence_++%6),{length_-legacyWidth,depth_/2});
    }
}

void CampaignTutorial::giantStrike(Unit& u) {
    const auto& pose=animations_.at({giant,u.pose});
    const auto hitFrame=giantHitFrames_.lower_bound({pose.symbol,0});
    if (hitFrame==giantHitFrames_.end() || hitFrame->first.first!=pose.symbol ||
        u.frame<hitFrame->first.second || u.targetsHit>=stat(u,"maxTargetsToHit")) return;
    auto hit=[&](Unit& target) {
        if (target.hp<=0 || target.enemy==u.enemy || (target.position.x-u.position.x)*u.facing<=0 ||
            std::abs(target.position.x-u.position.x)>200 || std::abs(target.position.y-u.position.y)>50) return;
        if (target.garrisoned && (target.enemy ? target.position.x>=length_-legacyWidth : target.position.x<=legacyWidth)) return;
        const bool structure=target.symbol==1904 || target.symbol==1174;
        const double perspective=rules_.at("backScale")+u.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
        const double other=rules_.at("backScale")+target.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
        const double reach=5*50*(perspective+other)/2;
        if (target.symbol!=1174 && std::hypot(target.position.x+target.velocity.x-u.position.x-u.velocity.x,
            target.position.y+target.velocity.y-u.position.y-u.velocity.y)>=reach) return;
        const double damage=stat(u,"damage")+stat(u,!structure && stat(target,"armoured") ? "toArmour" : "toNotArmour");
        applyMagicDamage(target,damage,u.enemy); impactSound(target);
        if (!structure && target.symbol!=giant) {
            target.stunned=int(stat(u,"stunTime")); target.velocity={7.*u.facing,0};
        }
        ++u.targetsHit; ++giantHits_;
    };
    for (auto& target:units_) {
        if (u.targetsHit>=stat(u,"maxTargetsToHit")) break;
        hit(target);
    }
    if (u.targetsHit<stat(u,"maxTargetsToHit")) hit(statues_[!u.enemy]);
    if (u.targetsHit<stat(u,"maxTargetsToHit") && u.enemy) hit(wall_);
}

void CampaignTutorial::updateGiants() {
    const auto first=std::find_if(units_.begin(),units_.end(),[](const Unit& u){return u.enemy && u.symbol==giant;});
    if (first!=units_.end() && first->hp>0) return;
    cinematic_=0; cinematicTicks_=0; sceneShade_=0;
    fastForward_=false; rangeSelected_=false; dragStart_.reset(); mapDown_=false;
    aimedSpell_=0; aimedAttackMove_=false; hint_=0; clickMarkers_.clear();
    for (auto& unit:units_) {unit.selected=false; unit.canFight=false; unit.spell=0;}
    sound("GiantDeath"+std::to_string(1+soundSequence_++%3),first==units_.end() ? Point{} : first->position);
}

void CampaignTutorial::createCinematic() {
    units_.clear(); population_=enemyPopulation_=0; flights_.clear(); magic_.clear();
    for (auto& side:production_) for (auto& queue:side) queue={};
    const double x=statues_[1].position.x;
    auto add=[&](int type,Point position,bool enemy=false) {
        const int index=spawn(type,position,enemy);
        auto& unit=units_[index]; unit.canFight=false; unit.hold=true;
        return index;
    };
    add(enslavedGiant,{x-200,depth_/2});
    for (double y:{depth_/4,3*depth_/4}) {add(sword,{x-150,y}); add(spearton,{x-250,y});}
    medusaIndex_=add(medusa,{length_-legacyWidth-100,depth_/2},true);
    units_[medusaIndex_].altitude=380;
}

void CampaignTutorial::speak(int index) {
    dialogueIndex_=index; dialogueTicks_=0;
    nextVoice_="../giants/"+dialogue_.at(index).voice;
}

void CampaignTutorial::updateCinematic() {
    ++cinematicTicks_;
    if (cinematic_==0) {
        sceneShade_=std::min(1.,cinematicTicks_/60.);
        if (cinematicTicks_>45) {units_.clear(); population_=enemyPopulation_=goldBalance_=mana_=enemyGold_=enemyMana_=0;}
        if (cinematicTicks_>60) {createCinematic(); cinematic_=1; cinematicTicks_=0;}
    } else if (cinematic_==1) {
        camera_=targetCamera_=statues_[1].position.x-350;
        sceneShade_=std::max(0.,(60-cinematicTicks_)/60.);
        if (cinematicTicks_>60) {cinematic_=2; cinematicTicks_=0; speak(0);}
    } else if (cinematic_==2 || cinematic_==7 || cinematic_==8) {
        camera_=targetCamera_=statues_[1].position.x-350;
        if (++dialogueTicks_>(dialogueIndex_==0 ? 150 : dialogue_.at(dialogueIndex_).ticks)) {
            if (dialogueIndex_==2) {
                cinematic_=3; cinematicTicks_=0;
                auto& medusaUnit=units_[medusaIndex_]; medusaUnit.garrisoned=true;
                medusaUnit.goal={length_-legacyWidth+legacyWidth/3,depth_/2}; medusaUnit.order=Order::Move;
            } else {speak(dialogueIndex_+1); cinematic_=dialogueIndex_==1 ? 7 : 8;}
        }
    } else if (cinematic_==3) {
        for (auto& u:units_) if (!u.enemy) u.facing=-1;
        const double front=statues_[1].position.x-1000;
        for (auto [type,y]:std::array<std::pair<int,double>,5>{{{ninja,depth_*3/4},{wizard,depth_/2},
            {healer,depth_/4},{archer,0},{spearton,depth_}}}) {
            const int id=spawn(type,{front-(type==wizard ? 0 : 75),y}); rebels_.push_back(id);
            auto& u=units_[id]; u.canFight=false; u.tint=75; u.order=Order::Move;
            u.goal={u.position.x+400,y}; random();
        }
        cinematic_=4; cinematicTicks_=0;
    } else if (cinematic_==4) {
        if (cinematicTicks_>100) {cinematic_=5; speak(3);}
    } else if (cinematic_==5 || cinematic_==10 || cinematic_==11 || cinematic_==12) {
        if (++dialogueTicks_>dialogue_.at(dialogueIndex_).ticks) {
            if (dialogueIndex_==6) {statues_[1].hp=0; step_=Step::Victory; counter_=0; return;}
            speak(dialogueIndex_+1); cinematic_=dialogueIndex_+6;
            if (dialogueIndex_==6) for (int id:rebels_) units_[id].tint=0;
        }
    }
    if (cinematic_==4 || cinematic_==5 || cinematic_>=10) {
        targetCamera_=statues_[1].position.x-900;
        camera_+=int((targetCamera_-camera_)*.05);
    }
    for (auto& u:units_) {
        // Scripted actors retain authored walk/idle cycles without combat or economy.
        if (u.order==Order::Attack || u.order==Order::Gather) u.order=Order::Stand;
        if (u.pose.starts_with("attack_") || u.spell) {u.spell=0; animate(u,"stand");}
        updateUnit(u);
    }
    if (medusaIndex_>=0 && medusaIndex_<int(units_.size())) units_[medusaIndex_].facing=-1;
}

void CampaignTutorial::updateCastleDead() {
    auto& u=castleDead_;
    const double speed=stat(dead,"arrowVelocity")+10,gravity=rules_.at("gravity");
    int target=-1; double best=std::pow(stat(dead,"castleRange"),2)+depth_*depth_;
    for (int i=0;i<int(units_.size());++i) {
        const auto& other=units_[i]; const double dx=u.position.x-other.position.x,dy=other.position.y-u.position.y;
        if (other.enemy || other.hp<=0 || other.cloakTicks || dx<=0 || dx>stat(dead,"castleRange")) continue;
        if (dx*dx+dy*dy<best) {best=dx*dx+dy*dy; target=i;}
    }
    const double perspective=rules_.at("backScale")+u.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
    const double z=-u.altitude*perspective;
    double theta=-1.35;
    if (target>=0) {
        const double distance=u.position.x-units_[target].position.x;
        const double discriminant=std::pow(speed,4)-gravity*(gravity*distance*distance+2*z*speed*speed);
        if (discriminant>0) theta=std::atan2(speed*speed-std::sqrt(discriminant),gravity*distance);
    }
    const double residual=std::remainder(-theta/radians-u.bowAngle,360.);
    u.bowAngle+=residual*(std::abs(residual)<10 ? .8 : .1);
    if (u.pose!="attack_1" && target>=0 && ticks_-u.lastHitTick>45 && theta!=-1.35 &&
        std::abs(std::remainder(-theta/radians-u.bowAngle,360.))<.5) {
        animate(u,"attack_1"); u.target=target; u.hit=false; u.lastHitTick=ticks_;
    }
    if (u.pose=="attack_1" && u.frame==31 && !u.hit) {
        const auto& pose=animations_.at({dead,u.pose}); const double size=scale(u);
        Flight shot; shot.enemy=true; shot.castle=false; shot.guts=true;
        shot.x=u.position.x-pose.origin.x*size; shot.depth=u.position.y; shot.z=z+pose.origin.y*size;
        shot.dx=-speed*std::cos(u.bowAngle*radians); shot.dz=speed*std::sin(u.bowAngle*radians);
        const double vz=speed*std::sin(theta);
        const double time=(vz+std::sqrt(std::max(0.,vz*vz-2*gravity*z)))/gravity;
        shot.dy=(units_[u.target].position.y-u.position.y)/(std::abs(time)<.001 ? 5 : time);
        shot.poison=stat(dead,"castlePoison"); shot.angle=soundSequence_++%360; shot.spin=int(soundSequence_++%4)-2;
        flights_.push_back(shot); ++enemyCastleShots_; u.hit=true;
    }
    if (++u.frame>=animations_.at({dead,u.pose}).length) {animate(u,"stand"); u.frame=1;}
}

void CampaignTutorial::drawCastleDead(std::vector<std::pair<double,Draw>>& layers) const {
    const auto& u=castleDead_; const double size=scale(u);
    const auto& pose=animations_.at({dead,u.pose});
    const double perspective=rules_.at("backScale")+u.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
    auto image=sprite(pose.symbol,u.frame,{u.position.x-camera_-pose.origin.x*size,
        battlefieldY_+u.position.y-u.altitude*perspective+pose.origin.y*size},size,true);
    image.redOffset=75; layers.emplace_back(u.position.y,std::move(image));
}

void CampaignTutorial::drawCinematic(std::vector<Draw>& out) const {
    if (cinematic_<0 || ended()) return;
    if (sceneShade_>0) out.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left,0,Viewport::right-Viewport::left,700},0,sceneShade_));
    if (dialogueIndex_>=0 && cinematic_>=2) {
        out.push_back(sprite(10139,1,{425,100},1.3));
        out.push_back(Draw::label({425-198.3*1.3,100-39.45*1.3,396.6*1.3,110},dialogue_.at(dialogueIndex_).text,22,0xffffff,true));
    }
}
}
