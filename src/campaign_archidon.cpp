#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace sw2 {
namespace {
constexpr double radians=std::numbers::pi/180;
}

CampaignTutorial::CampaignTutorial(const MenuAssets& art,const std::filesystem::path& root,const CampaignProgress& progress)
    :CampaignTutorial(art,root/"tutorial",progress.difficulty,progress.records.at(progress.level).attempts) {
    if (progress.level<1 || progress.level>=CampaignProgress::playableLevelCount) throw std::runtime_error("Unsupported campaign mission");
    configureCampaign(root/"archidon",progress);
}

void CampaignTutorial::configureCampaign(const std::filesystem::path& dir,const CampaignProgress& progress) {
    mission_=progress.level; upgrades_=progress.upgrades;
    backgroundLayers_={4119,4117,4114,4124};
    enemyStatueFrame_=6;
    const auto levelDir=mission_==8 ? dir.parent_path()/"deads" : mission_==7 ? dir.parent_path()/"knight" : mission_==6 ? dir.parent_path()/"bomber" : mission_==5 ? dir.parent_path()/"giants" : mission_==4 ? dir.parent_path()/"rebels" : mission_==3 ? dir.parent_path()/"magic" :
        mission_==2 ? dir.parent_path()/"shadow" : dir;
    std::ifstream starting(levelDir/"starting.tsv");
    std::string key;
    double value;
    Animation pose;
    int owner;
    auto readRules=[&](const std::filesystem::path& folder,bool loadAnimations=true){
        std::ifstream settings(folder/"rules.tsv"),poses(folder/"animations.tsv");
        while (settings>>key>>value) rules_[key]=value;
        while (loadAnimations && poses>>owner>>key>>pose.symbol>>pose.origin.x>>pose.origin.y>>pose.length>>pose.frameBase)
            animations_[{owner,key}]=pose;
        if (!settings.eof() || (loadAnimations && !poses.eof())) throw std::runtime_error("Invalid campaign manifest");
    };
    readRules(dir);
    if (mission_>=2) {
        readRules(dir.parent_path()/"shadow");
        backgroundLayers_={4044,4048,4053}; enemyStatueFrame_=1;
        if (mission_>=3) {
            readRules(dir.parent_path()/"magic"); backgroundLayers_={4102,4098}; enemyStatueFrame_=7;
            if (mission_>=4) {
                readRules(dir.parent_path()/"rebels",false); backgroundLayers_={4058,4056,4062,4067}; enemyStatueFrame_=1;
            }
            if (mission_>=5) {
                readRules(dir.parent_path()/"giants");
                for (const auto& [binding,animation]:animations_)
                    if (binding.first==dead) castleDeadAnimations_[binding.second]=animation;
                backgroundLayers_={4094,4080,4087,4091,4084}; ownStatueFrame_=8; enemyStatueFrame_=11;
                chaosOpponent_=true;
            }
            if (mission_>=6) {
                readRules(dir.parent_path()/"bomber");backgroundLayers_={4102,4098};
            }
            if (mission_>=7) {
                readRules(dir.parent_path()/"knight");backgroundLayers_={4094,4080,4087,4091,4084};
                if (mission_>=8) {
                    readRules(levelDir);backgroundLayers_={4102,4098};enemyStatueFrame_=3;
                }
                std::ifstream available(levelDir/"available.tsv");
                std::vector<std::string> names;
                while (available>>key) names.push_back(key);
                if (!available.is_open() || !available.eof()) throw std::runtime_error("Invalid Chaos recruitment manifest");
                for (const auto& entry:std::array<EnemyBuild,4>{{{giant,5,"Giant"},{knight,1,"Knight"},{dead,2,"Dead"},{bomber,1,"Bomber"}}})
                    if (std::find(names.begin(),names.end(),entry.name)!=names.end()) enemyBuild_.push_back(entry);
                std::ifstream knightTips(dir.parent_path()/"knight"/"tips.tsv");
                int body,frame;Point tip;
                while (knightTips>>body>>frame>>tip.x>>tip.y) tips_[{body,frame}]=tip;
                if (!knightTips.is_open() || !knightTips.eof()) throw std::runtime_error("Invalid Juggerknight hit markers");
            }
            enemyCastleArcher_=rules_.at("enemy.castleArchers")>0;
        }
        std::ifstream tips(dir.parent_path()/"shadow"/"tips.tsv");
        int symbol,frame; Point tip;
        while (tips>>symbol>>frame>>tip.x>>tip.y) tips_[{symbol,frame}]=tip;
        if (!tips.eof()) throw std::runtime_error("Invalid Shadowrath hit markers");
        if (mission_>=3) {
            std::ifstream magicTips(dir.parent_path()/"magic"/"tips.tsv");
            while (magicTips>>symbol>>frame>>tip.x>>tip.y) tips_[{symbol,frame}]=tip;
            if (!magicTips.eof()) throw std::runtime_error("Invalid magic hit markers");
        }
    }
    std::ifstream bows(dir/"bows.tsv");
    int symbol,frame;
    Point bow;
    while (bows>>symbol>>frame>>bow.x>>bow.y) bows_[{symbol,frame}]=bow;
    if (!bows.eof() || bows_.empty()) throw std::runtime_error("Invalid archer bow placements");
    length_=rules_.at("map.length");
    createMiningBlocks();
    units_.clear(); population_=enemyPopulation_=0;
    for (int side=0;side<2;++side) {
        auto& s=statues_[side];
        s.position.x=side ? length_-legacyWidth-500 : legacyWidth+500;
        s.hp=s.maxHp=rules_.at(side ? "oponent.statueHealth" : "player.statueHealth");
        median_[side]=side ? length_-legacyWidth : legacyWidth;
    }
    goldBalance_=int(rules_.at("player.gold"))+(difficulty_==Difficulty::Normal ? 200 : 0);
    mana_=int(rules_.at("player.mana"))+(difficulty_==Difficulty::Normal ? 200 : 0);
    enemyGold_=int(rules_.at("oponent.gold")); enemyMana_=int(rules_.at("oponent.mana"));
    wall_.symbol=1174; wall_.position={legacyWidth+1200,depth_/2}; wall_.width=100; wall_.height=300;
    wall_.hp=wall_.maxHp=difficulty_==Difficulty::Normal ? rules_.at("wall.health") : 0;
    std::array<std::vector<int>,2> groups;
    std::string team,name;
    while (starting>>team>>name) {
        if (team!="player" && team!="oponent") throw std::runtime_error("Invalid starting team");
        const int type=name=="Miner" ? miner : name=="Swordwrath" ? sword : name=="Archidon" ? archer :
            name=="Monk" ? healer : name=="Ninja" ? ninja : name=="Magikill" ? wizard : name=="Spearton" ? spearton :
            name=="ChaosMiner" ? chaosMiner : name=="Giant" ? giant : name=="Bomber" ? bomber : name=="EnslavedGiant" ? enslavedGiant : name=="Knight" ? knight : name=="Dead" ? dead : 0;
        if (!type) throw std::runtime_error("Unsupported starting unit");
        groups[team=="oponent"].push_back(type);
    }
    if (!starting.eof() || groups[0].empty() || groups[1].empty()) throw std::runtime_error("Missing starting army");
    for (int side=0;side<2;++side) spawnGroup(side!=0,groups[side]);
    for (auto& u:units_) if (isWorker(u.symbol)) assignFreeMine(u);
    step_=Step::Free; trainingAvailable_=true; canSkip_=false;
    camera_=targetCamera_=850;
    if (mission_>=5) loadGiants(dir.parent_path()/"giants",mission_==5);
}

Point CampaignTutorial::updateArcher(Unit& u) {
    if (u.bowFrame==46) sound("BowReady",u.position);
    if (u.bowFrame!=1 && ++u.bowFrame>=int(rules_.at("archer.bowFrames"))) u.bowFrame=1;
    const auto& target=combatTarget(u.target!=-1 ? u.target : (u.enemy ? ownStatue : opposingStatue));
    const double dx=target.position.x-u.position.x,dy=target.position.y-u.position.y;
    const double velocity=stat(u,"arrowVelocity"),gravity=rules_.at("gravity");
    const double distance=std::abs(dx);
    const double discriminant=std::pow(velocity,4)-gravity*gravity*distance*distance;
    const double theta=discriminant>0 && distance<=stat(u,"maximumRange") ?
        std::atan2(velocity*velocity-std::sqrt(discriminant),gravity*distance) : -1.35;
    const double residual=std::remainder(-theta/radians-u.bowAngle,360.);
    u.bowAngle+=residual*(std::abs(residual)<10 ? .8 : .1);
    const bool engage=u.canFight && u.order==Order::Attack && target.hp>0 && !u.garrisoned;
    if (engage && theta!=-1.35) {
        u.facing=dx<0 ? -1 : 1;
        if (u.bowFrame==1 && std::abs(std::remainder(-theta/radians-u.bowAngle,360.))<.5) {
            const auto& pose=animations_.at({archer,u.pose});
            const auto bow=bows_.at({pose.symbol,u.frame+pose.frameBase});
            const double size=scale(u);
            Flight arrow;
            arrow.castle=false; arrow.enemy=u.enemy;
            arrow.depth=u.position.y;
            arrow.x=u.position.x+u.facing*(pose.origin.x+bow.x)*size;
            arrow.z=(pose.origin.y+bow.y)*size;
            const double angle=u.facing<0 ? 180-u.bowAngle : u.bowAngle;
            arrow.dx=velocity*std::cos(angle*radians); arrow.dz=velocity*std::sin(angle*radians);
            const double time=(velocity*std::sin(theta)+std::sqrt(std::max(0.,
                std::pow(velocity*std::sin(theta),2)-2*gravity*arrow.z)))/gravity;
            arrow.dy=dy/(std::abs(time)<.001 ? 5 : time);
            advanceArrow(arrow); advanceArrow(arrow);
            flights_.push_back(arrow); ++mobileShots_; ++u.bowFrame;
            sound("launchArrow"+std::to_string(1+soundSequence_++%4),u.position);
        }
        // EnemyGoodTeamAi enables kiting for the Archidons after acquiring a target.
        if (u.enemy && distance<350 && u.bowFrame>=35) return {dx<0 ? 1. : -1.,0};
        return {};
    }
    if (!u.hold && (u.order==Order::Move || engage))
        return {(u.goal.x-u.position.x)/100,(u.goal.y-u.position.y)/100};
    return {};
}

void CampaignTutorial::unitAbility(int slot) {
    if (cinematic_>=0) return;
    aimedAttackMove_=false;
    if (mission_>=4) for (auto& u:units_) if (u.hp>0 && u.selected && !u.enemy) {
        if (u.symbol==wizard) {
            const int spell=slot==0 ? 1 : slot==1 ? 3 : 2;
            if (spell==1 || (spell==2 && upgrades_[13]) || (spell==3 && upgrades_[17])) aimedSpell_=spell;
        } else if (u.symbol==healer) {
            if (slot==0) u.autoHeal=!u.autoHeal;
            if (slot==1 && upgrades_[8]) u.autoCure=!u.autoCure;
        }
    }
    if (slot) return;
    if (mission_>=3) for (auto& u:units_) if (u.hp>0 && u.selected && !u.enemy && u.symbol==ninja && !u.cloakCooldown &&
        mana_>=stat(u,"stealth.mana")) {
        mana_-=int(stat(u,"stealth.mana")); u.cloakTicks=int(stat(u,"stealth.effect"));
        u.cloakCooldown=int(stat(u,"stealth.cooldown")); sound("ninjaCloakSound",u.position);
    }
    if (!mission_ || !upgrades_[2]) return;
    for (auto& u:units_) if (u.hp>10 && u.selected && !u.enemy && u.symbol==sword &&
        u.rageCounter>=rules_.at("rage.cooldown")) {
        u.hp-=rules_.at("rage.healthLoss"); u.rageCounter=0;
        sound("Rage"+std::to_string(1+soundSequence_++%3),u.position);
    }
}

void CampaignTutorial::updateMobileArrows() {
    for (auto& arrow:flights_) advanceArrow(arrow);
    std::erase_if(flights_,[](const Flight& a){return a.hidden || (a.struck && a.age>1200);});
}

void CampaignTutorial::drawArcher(std::vector<std::pair<double,Draw>>& layers,const Unit& u) const {
    const auto& pose=animations_.at({archer,u.pose});
    const double size=scale(u);
    Point body{u.position.x-camera_+u.facing*pose.origin.x*size,
               battlefieldY_+u.position.y+pose.origin.y*size};
    auto image=sprite(pose.symbol,pose.frameBase+u.frame,body,size,u.facing<0);
    image.redOffset=u.tint>=0 ? u.tint : u.enemy ? 75 : 0;
    layers.emplace_back(u.position.y,std::move(image));
    const auto bowLocal=bows_.at({pose.symbol,pose.frameBase+u.frame});
    const Point bow{body.x+u.facing*bowLocal.x*size,body.y+bowLocal.y*size};
    auto arms=rotatedSprite(4168,u.bowFrame,bow,size,u.facing*u.bowAngle);
    if (u.facing<0) {
        arms.mirrored=true;
        arms.target.x=2*bow.x-arms.target.x-arms.target.w;
        arms.rotation=-u.bowAngle;
    }
    arms.redOffset=u.tint>=0 ? u.tint : u.enemy ? 75 : 0;
    layers.emplace_back(u.position.y,std::move(arms));
}

void CampaignTutorial::updateArchidon() {
    if (researchTicks_>=0) --researchTicks_;
    else if (researchStarted_) castleArcher_=true;
    for (auto& u:units_) if (!u.enemy && isWorker(u.symbol) && u.hp>0 && !u.garrisoned &&
        u.resource==-2 && std::abs(u.position.x-u.goal.x)<40) assignFreeMine(u);
    std::array<bool,2> atTower{};
    const double previousControl=towerControl_;
    for (const auto& u:units_) if (u.hp>0 && !u.garrisoned &&
        std::abs(u.position.x-length_/2)<=150 && std::abs(u.position.y-depth_/2)<=150) atTower[u.enemy]=true;
    if (atTower[0]!=atTower[1])
        towerControl_=std::clamp(towerControl_+(atTower[0] ? -1 : 1)*50/rules_.at("towerCaptureTime"),0.,100.);
    if (previousControl!=0 && towerControl_==0) sound("SelectRaceSound",{},false);
    if (ticks_%int(rules_.at("hill.frequency"))==0) {
        if (towerControl_==0) goldBalance_+=int(rules_.at("hill.goldCollectionRate"));
        if (towerControl_==100) enemyGold_+=int(rules_.at("hill.goldCollectionRate"));
    }
    if (mission_>=2) {
        if (mission_==2 && !hint_ && std::any_of(units_.begin(),units_.end(),[](const Unit& u){return !u.enemy && u.poison>0;}))
            hint_=3;
        if (hint_) ++hintTicks_;
        return;
    }
    const bool selected=std::any_of(units_.begin(),units_.end(),[](const Unit& u){
        return u.selected && u.hp>0 && !u.enemy && !isWorker(u.symbol);
    });
    if (!hint_ && ticks_>30 && selected) {hint_=1; hintTicks_=0;}
    if (hint_) hintTicks_+=hint_==2 ? 2 : 1;
    if (hint_==1 && frontLine()>length_/2) {hint_=2; hintTicks_=0;}
    if (hint_==2 && hintTicks_<45) targetCamera_=length_/2-425;
}
}
