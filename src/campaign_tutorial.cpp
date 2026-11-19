#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sw2 {
namespace {
double normalized(double value) { return std::clamp(value,-1.,1.); }
Rect between(Point a,Point b) {
    return {std::min(a.x,b.x),std::min(a.y,b.y),std::abs(a.x-b.x),std::abs(a.y-b.y)};
}
}

CampaignTutorial::CampaignTutorial(const MenuAssets& art,const std::filesystem::path& dir,Difficulty difficulty,int retries)
    :art_(art),difficulty_(difficulty),canSkip_(difficulty!=Difficulty::Normal || retries>0) {
    std::ifstream rules(dir/"rules.tsv"),poses(dir/"animations.tsv"),messages(dir/"messages.tsv"),markers(dir/"markers.tsv");
    std::string key;
    double value{};
    while (rules>>key>>value) rules_[key]=value;
    Animation pose;
    int owner{};
    while (poses>>owner>>key>>pose.symbol>>pose.origin.x>>pose.origin.y>>pose.length) animations_[{owner,key}]=pose;
    for (std::string line;std::getline(messages,line);) {
        std::istringstream fields(line);
        std::string voice,seconds;
        Message m;
        std::getline(fields,voice,'\t'); std::getline(fields,seconds,'\t');
        std::getline(fields,m.step,'\t'); std::getline(fields,m.text);
        m.seconds=std::stod(seconds);
        messages_[std::stoi(voice.substr(13))]=std::move(m);
    }
    Point tip;
    int id{},frame{};
    while (markers>>id>>frame>>key>>tip.x>>tip.y) if (key=="tip") tips_[{id,frame}]=tip;
    if (!rules.eof() || !poses.eof() || !markers.eof() || messages_.empty())
        throw std::runtime_error("Invalid tutorial manifest");
    std::ifstream masks(dir/"control-masks.tsv");
    double y{},left{},right{};
    while (masks>>key>>y>>left>>right) controlMasks_[key].push_back({left,y,right-left,1});
    if (!masks.eof() || controlMasks_.size()!=4) throw std::runtime_error("Invalid campaign controls");
    std::ifstream cursors(dir.parent_path()/"interaction/cursors.tsv");
    int symbol{},count{};
    while (cursors>>symbol>>count) cursorFrames_[symbol]=count;
    if (!cursors.eof() || cursorFrames_.size()!=6) throw std::runtime_error("Invalid contextual cursors");
    length_=art_.texture(4098).bounds.w;
    depth_=rules_.at("battlefieldHeight"); battlefieldY_=rules_.at("battlefieldY");
    for (int i=0;i<2;++i) {
        auto& s=statues_[i];
        s.symbol=1904; s.enemy=i!=0;
        s.position={i ? length_-legacyWidth-500 : legacyWidth+500,depth_/2};
        s.maxHp=s.hp=rules_.at(i ? "oponent.statueHealth" : "player.statueHealth");
        // Statue.init captures the initial skin before Team.updateStatue.
        // Changing its artwork does not recalculate this original collider.
        s.width=175.2; s.height=175;
        median_[i]=i ? length_-legacyWidth : legacyWidth;
    }
    // com.brockw.random.Random, campaign Main.seed = 0.
    int previous=161803398, next=1;
    seed_[55]=previous;
    for (int i=1;i<55;++i) {
        const int index=21*i%55;
        seed_[index]=next;
        next=previous-next;
        if (next<0) next+=2147483647;
        previous=seed_[index];
    }
    for (int pass=0;pass<4;++pass) for (int k=1;k<56;++k) {
        seed_[k]-=seed_[1+(k+30)%55];
        if (seed_[k]<0) seed_[k]+=2147483647;
    }
    spawn(sword,{legacyWidth+2000,depth_/3});
    spawn(sword,{legacyWidth+2000,2*depth_/3});
    spawn(miner,{legacyWidth+400,depth_/2});
    createMiningBlocks();
}
void CampaignTutorial::createMiningBlocks() {
    gold_.clear();
    // SwampLands and Forest place these same four mirrored mining blocks.
    for (int distance:{670,1200}) for (int direction:{1,-1}) {
        const double x=direction==1 ? legacyWidth+distance : length_-legacyWidth-distance;
        for (auto p:std::array<Point,4>{{{-10,-100},{-10,100},{70,-60},{70,60}}})
            gold_.push_back({x+p.x*direction,depth_/2+p.y});
    }
    oreRemaining_.assign(gold_.size(),int(rules_.at("goldAtStartOfMap")));
    mineOrder_.resize(gold_.size());
    std::iota(mineOrder_.begin(),mineOrder_.end(),0);
    std::stable_sort(mineOrder_.begin(),mineOrder_.end(),[this](int a,int b){return gold_[a].x<gold_[b].x;});
}
int CampaignTutorial::random() {
    if (++randomA_==56) randomA_=1;
    if (++randomB_==56) randomB_=1;
    int v=seed_[randomA_]-seed_[randomB_];
    if (v<0) v+=2147483647;
    return seed_[randomA_]=v;
}
int CampaignTutorial::spawn(int symbol,Point p,bool enemy) {
    Unit u;
    u.symbol=symbol; u.position=u.goal=p; u.enemy=enemy; u.facing=enemy ? -1 : 1;
    u.maxHp=u.hp=stat(u,"health");
    const auto& pose=animations_.at({symbol,"stand"});
    const auto& bounds=art_.texture(pose.symbol,pose.frameBase+1).bounds;
    u.width=bounds.w; u.height=bounds.h;
    units_.push_back(u);
    if (symbol==enslavedGiant || (symbol==giant && mission_>=6))
        sound("GiantGrowl"+std::to_string(1+soundSequence_++%6),p);
    return int(units_.size())-1;
}
double CampaignTutorial::stat(const Unit& u,const char* name) const {
    return stat(u.symbol,name);
}
double CampaignTutorial::stat(int symbol,const char* name) const {
    const char* prefix=symbol==miner ? "miner." : symbol==spearton ? "spearton." : symbol==archer ? "archer." : symbol==ninja ? "ninja." :
        symbol==wizard ? "magikill." : symbol==healer ? "monk." : symbol==chaosMiner ? "chaosMiner." :
        symbol==giant ? "giant." : symbol==enslavedGiant ? "enslavedGiant." : symbol==medusa ? "medusa." : symbol==dead ? "dead." : symbol==bomber ? "bomber." : symbol==knight ? "knight." : "swordwrath.";
    if (std::string_view(name)=="hitWidth" || std::string_view(name)=="hitHeight") {
        if (const auto found=rules_.find(std::string(prefix)+name);found!=rules_.end()) return found->second;
        const auto& pose=animations_.at({symbol,"stand"});
        const auto box=art_.texture(pose.symbol,pose.frameBase+1).bounds;
        return (std::string_view(name)=="hitWidth" ? box.w : box.h)*stat(symbol,"scale");
    }
    if (symbol==enslavedGiant && (std::string_view(name)=="health" || std::string_view(name)=="scale")) {
        const bool health=std::string_view(name)=="health";
        if (upgrades_[20]) return rules_.at(health ? "enslavedGiant.healthII" : "enslavedGiant.growthIIScale");
        if (upgrades_[16]) return rules_.at(health ? "enslavedGiant.healthI" : "enslavedGiant.growthIScale");
    }
    return rules_.at(std::string(prefix)+name);
}
double CampaignTutorial::scale(const Unit& u) const {
    return stat(u,"scale")*(rules_.at("backScale")+u.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale")));
}
void CampaignTutorial::animate(Unit& u,const std::string& name) {
    if (u.pose!=name) { u.pose=name; u.frame=1; }
}
Point CampaignTutorial::workMiner(Unit& u) {
    const bool praying=u.resource<0;
    const int direction=u.enemy ? -1 : 1;
    const double home=u.enemy ? length_-legacyWidth : legacyWidth;
    int& gold=u.enemy ? enemyGold_ : goldBalance_;
    int& mana=u.enemy ? enemyMana_ : mana_;
    const Point ore=praying ? statues_[u.enemy].position : gold_.at(u.resource);
    if (std::abs(u.position.x-home)<220 && u.bag>0) {
        gold+=u.bag; u.bag=0;
    }
    if (u.bag>=stat(u,"bagSize")) {
        const double dy=depth_/2-u.position.y;
        u.facing=-direction;
        return {(home-u.position.x)/20,std::abs(dy)<40 ? 0 : dy/20};
    }
    if (!praying && !oreRemaining_[u.resource]) {
        u.resource=-2; u.miningSlot=-1; u.order=Order::Stand; animate(u,"stand"); return {};
    }
    const double dx=ore.x+direction*(praying ? -50 : (2*u.miningSlot-1)*50)-u.position.x;
    const double dy=ore.y+(praying ? (rules_.at("maxWorshipers")-u.miningSlot)*20-50 : 0)-u.position.y;
    u.facing=ore.x<u.position.x ? -1 : 1;
    if (praying && std::abs(dx)>=150) return {(ore.x-direction*125-u.position.x)/20,0};
    if (std::abs(dx)>=30 || std::abs(dy)>=20) return {dx/20,dy/20};
    if (praying) {
        if (u.pose!="pray" && u.pose!="bendDownToPray") animate(u,"bendDownToPray");
        if (u.frame==animations_.at({u.symbol,u.pose}).length) {
            if (u.pose=="pray") mana+=int(stat(u,"manaMineRate"));
            else animate(u,"pray");
        } else if (u.pose=="pray") {
            // Miner.update advances the child a second time while effects are enabled.
            if (++u.frame==animations_.at({u.symbol,"pray"}).length) {
                mana+=int(stat(u,"manaMineRate")); u.frame=1;
            }
        }
    } else {
        animate(u,"mine");
        if (u.frame==animations_.at({u.symbol,"mine"}).length) {
            const int amount=std::min(int(stat(u,"miningRate")),oreRemaining_[u.resource]);
            oreRemaining_[u.resource]-=amount;
            u.bag=std::min(int(stat(u,"bagSize")),u.bag+amount);
        }
    }
    return {};
}
void CampaignTutorial::updateUnit(Unit& u) {
    ++u.rageCounter;
    if (u.garrisoned && (u.enemy ? u.position.x>=length_-legacyWidth : u.position.x<=legacyWidth)) u.poison=0;
    if (u.poison>0 && ticks_%90==0) applyMagicDamage(u,u.poison,!u.enemy);
    if (u.hp>0 && u.healTicks>0) {u.hp=std::min(u.maxHp,u.hp+u.healAmount); --u.healTicks;}
    if (u.cloakTicks>0) --u.cloakTicks;
    if (u.cloakCooldown>0) --u.cloakCooldown;
    if (u.symbol==giant || u.symbol==enslavedGiant) u.stunned=0;
    else if (u.stunned>0) --u.stunned;
    if (u.hp<=0) {
        if (u.symbol==bomber) {
            if (!u.deadCounted) {
                (u.enemy ? enemyPopulation_ : population_)-=int(stat(u,"population"));
                u.deadCounted=true;detonate(u);
            }
            return;
        }
        if (!u.deadCounted) {
            (u.enemy ? enemyPopulation_ : population_)-=int(stat(u,"population")); u.deadCounted=true;
            sound(u.symbol==wizard ? "MagikillDeath" : "Pain"+std::to_string(1+soundSequence_++%8),u.position);
        }
        if (!u.pose.starts_with("death_")) {
            int variant=u.symbol==sword ? random()%3+1 : 1;
            animate(u,"death_"+std::to_string(variant));
        }
        u.frame=std::min(u.frame+1,animations_.at({u.symbol,u.pose}).length);
        return;
    }
    Point force{};
    const bool casting=u.canFight && (u.spell || !u.stunned) && (u.symbol==wizard || u.symbol==healer) && updateCaster(u);
    bool attacking=!casting && u.pose.starts_with("attack_");
    if (casting) {
        // The cast animation advances below and retains its authored event frame.
    } else if (u.symbol==archer) {
        force=updateArcher(u);
    } else if (u.symbol==enslavedGiant) {
        force=updateEnslavedGiant(u);attacking=u.pose.starts_with("attack_");
    } else if (u.canFight && u.order==Order::Attack && !attacking && !u.stunned) {
        auto& enemy=combatTarget(u.target);
        if (enemy.hp<=0) { u.order=u.engageOnMove ? Order::Move : Order::Stand; }
        else {
            const double dx=enemy.position.x-u.position.x,dy=enemy.position.y-u.position.y;
            const double reach=stat(u,"weaponReach");
            if (u.symbol==ninja && u.enemy && !u.cloakCooldown && std::abs(dx)<500) {
                u.cloakTicks=int(stat(u,"stealth.effect")); u.cloakCooldown=int(stat(u,"stealth.cooldown"));
                sound("ninjaCloakSound",u.position);
            }
            const int facing=dx<0 ? -1 : 1;
            if (std::abs(dx)<reach && std::abs(dy)<40 && u.facing==facing) {
                if (u.symbol==bomber) {u.hp=0;return;}
                animate(u,u.symbol==knight ? "attack_"+std::to_string(random()%3+1) :
                    (u.symbol!=sword && u.symbol!=ninja && u.symbol!=giant) || random()%2==0 ? "attack_1" : "attack_2"); u.hit=false;
                u.targetsHit=0;
                if (u.symbol==ninja) sound("ninjaSwipe"+std::to_string(1+soundSequence_++%4),u.position);
                else if (u.symbol==sword || u.symbol==spearton || isWorker(u.symbol)) sound("swordwrathSwing1",u.position);
                attacking=true;
            } else if (!u.hold) {
                u.facing=facing;
                if (u.symbol==bomber && dx*dx+dy*dy<150000) force={dx/30,dy};
                else {
                    force.x=std::abs(dx)<reach*.9 ? 0 : (dx-facing*reach*.5)/100;
                    if (std::abs(dx)<200) force.y=dy;
                }
            }
        }
    } else if (!u.stunned && u.order==Order::Gather) {
        force=workMiner(u);
    } else if (!u.stunned && u.order==Order::Move) {
        const int divisor=isWorker(u.symbol) ? 20 : 100;
        force={(u.goal.x-u.position.x)/divisor,(u.goal.y-u.position.y)/divisor};
        if (std::abs(u.velocity.x)>stat(u,"maxVelocity")*.4) u.facing=u.velocity.x<0 ? -1 : 1;
    }
    const double acceleration=stat(u,"maxForce")/stat(u,"mass");
    force={normalized(force.x)*acceleration,normalized(force.y)*acceleration*.8};
    if (attacking) {
        const auto& seq=animations_.at({u.symbol,u.pose});
        auto marker=tips_.find({seq.symbol,u.frame});
        if (u.symbol==giant) giantStrike(u);
        else if (u.symbol==enslavedGiant) {
            if (u.frame==1) sound("BoulderThrowSound",u.position);
            if (u.frame==33 && !u.hit) {throwBoulder(u);u.hit=true;}
        }
        else if (!u.hit && marker!=tips_.end()) {
            auto& target=combatTarget(u.target);
            const double size=scale(u);
            const Point p{u.position.x+u.facing*(seq.origin.x+marker->second.x)*size,
                          u.position.y+(seq.origin.y+marker->second.y)*size};
            const bool statue=target.symbol==1904;
            const bool wall=target.symbol==1174;
            const double dx=p.x-target.position.x,dy=p.y-target.position.y;
            const bool hit=wall ? std::abs(dx)<100 && std::abs(dy)<300 :
                statue ? dx>-target.width && dx<target.width*2 && dy<0 && dy>-target.height :
                         std::abs(dx)<target.width && std::abs(dy)<target.height;
            if (hit && target.hp>0) {
                const double modifier=(!statue && target.enemy && difficulty_==Difficulty::Normal) ? rules_.at("normalHealthScale") : 1;
                const double bonus=stat(u,!statue && !wall && stat(target,"armoured")!=0 ? "toArmour" : "toNotArmour");
                double damage=stat(u,"damage")+bonus;
                if (u.symbol==ninja) {
                    if (u.lastVictim!=u.target || ticks_-u.lastHitTick>stat(u,"fury.furyEffect")) u.furyStacks=0;
                    damage+=u.furyStacks*stat(u,"fury.bonus");
                    u.furyStacks=std::min(u.furyStacks+1,int(stat(u,"fury.stacks")));
                    u.lastVictim=u.target; u.lastHitTick=ticks_;
                    if (u.cloakTicks && !statue && !wall) {
                        damage+=stat(u,stat(target,"armoured") ? "stealth.damageToArmour" : "stealth.damageToNotArmour");
                        if (target.poison<stat(u,"stealth.poison")) sound("PoisonedSound",target.position);
                        target.poison=std::max(target.poison,stat(u,"stealth.poison"));
                    }
                    u.cloakTicks=0;
                }
                if (mission_>=3) applyMagicDamage(target,damage,u.enemy);
                else target.hp=std::max(0.,target.hp-damage/modifier);
                impactSound(target);
                if (u.symbol==knight) sound("sword1",u.position);
                u.hit=true;
            }
        }
        if (u.frame>=seq.length) animate(u,"run");
    } else if (!casting && (std::abs(force.x)>.03 || std::abs(force.y)>.03)) animate(u,u.cloakTicks ? "stealth" : "run");
    else if (!casting && (u.pose=="run" || u.pose=="stealth")) animate(u,u.symbol==sword && random()*4.656612875245797e-10>=.1 ? "stand_breath" : "stand");
    const double drag=u.stunned ? .7 : stat(u,"dragForce");
    double limit=mission_ && u.symbol==sword && u.rageCounter<rules_.at("rage.effect") ?
        rules_.at("rage.rageMaxVelocity") : stat(u,u.cloakTicks ? "stealth.maxVelocity" : "maxVelocity");
    if (u.stunned) limit*=8;
    u.velocity.x=std::clamp((u.velocity.x+force.x)*drag,-limit,limit);
    u.velocity.y=std::clamp((u.velocity.y+force.y)*drag,-limit*.5,limit*.5);
    const double nextX=u.position.x+u.velocity.x,nextY=u.position.y+u.velocity.y;
    const double boundary=u.garrisoned ? 0 : legacyWidth+200;
    if ((nextX<=boundary && u.velocity.x<0) || (nextX>=length_-boundary && u.velocity.x>0))
        u.velocity.x=0;
    if (nextY<0 || nextY>depth_) u.velocity.y=0;
    u.position.x+=u.velocity.x; u.position.y+=u.velocity.y;
    const int end=animations_.at({u.symbol,u.pose}).length;
    u.frame=u.frame>=end ? 2 : u.frame+1;
    if (mission_ && u.symbol==sword && attacking && u.rageCounter<rules_.at("rage.effect"))
        u.frame=std::min(end,u.frame+1);
}
void CampaignTutorial::setFormation(Point p) {
    std::vector<Unit*> selected;
    for (auto& u:units_) if (!u.enemy && u.selected && u.hp>0) {
        u.garrisoned=false; u.engageOnMove=false;
        if (isWorker(u.symbol)) {u.resource=-2; u.miningSlot=-1;}
        selected.push_back(&u);
    }
    moveFormation(selected,p);
}
void CampaignTutorial::moveFormation(const std::vector<Unit*>& selected,Point p) {
    const int count=int(selected.size());
    for (int i=0;i<count;++i) {
        auto& u=*selected[i];
        constexpr int rows=6;
        const int col=i/rows,k=i%rows,n=std::min(rows,count-col*rows);
        const double y=std::clamp(p.y,0.,depth_);
        const int offset=std::clamp(int(std::round(y/(depth_/rows)))-(n+1)/2,0,rows-n);
        const int row=(n-1)/2+(k%2*2-1)*((k+1)/2)+offset;
        const int dir=u.enemy ? -1 : 1;
        const double x=p.x-dir*(row*8+col*50);
        const bool castle=x<legacyWidth || x>length_-legacyWidth || u.position.x<legacyWidth || u.position.x>length_-legacyWidth;
        const double available=castle ? depth_/3 : depth_,top=(depth_-available)/2;
        u.goal={x,count==1 ? std::clamp(y,top,top+available) : top+(row+.5)*available/rows};
        u.order=Order::Move; u.target=-1; u.hold=false;
        if (u.pose.starts_with("attack_")) animate(u,"run");
    }
}
void CampaignTutorial::point(std::optional<Point> p) {
    cursor_=p;
    if (mapDown_ && p && minimap.contains(*p))
        targetCamera_=(p->x-minimap.x)/minimap.w*length_-legacyWidth/2;
    if (dragStart_ && p && p->y<575) updateSelection(*p);
}
void CampaignTutorial::press(Point p) {
    cursor_=p;
    if (step_>=Step::Victory) {
        pressedResult_=resultHit(p);
        pressedCard_=cardHit(p);
        return;
    }
    if (cinematic_>=0) return;
    if (aimedSpell_ && p.y<575) {castAt({p.x+camera_,p.y-battlefieldY_}); return;}
    if (aimedAttackMove_ && (p.y<575 || minimap.contains(p))) {
        aimedAttackMove_=false;
        const Point destination=minimap.contains(p) ?
            Point{(p.x-minimap.x)/minimap.w*length_,(p.y-minimap.y)/minimap.h*depth_} :
            Point{p.x+camera_,p.y-battlefieldY_};
        setFormation(destination);
        for (auto& u:units_) if (u.selected && !u.enemy && u.hp>0) u.engageOnMove=true;
        clickMarkers_.push_back({destination,1});
        sound("CommandMove"+std::to_string(1+soundSequence_++%3),{},false);
        return;
    }
    if (canSkip_ && step_==Step::Select &&
        sprite(1928,1,{442,100+art_.texture(10139).bounds.h*1.3-140}).target.contains(p)) {
        skipTutorial(); return;
    }
    // The bitmap's transparent lower-left corner overlaps the minimap. Only
    // the authored round face is interactive, not that entire rectangle.
    if (step_==Step::Free && std::hypot(p.x-550.6,p.y-584.6)<=16.6) {
        fastForward_=!fastForward_; return;
    }
    if (defencePress(p)) return;
    const bool unitSelected=std::any_of(units_.begin(),units_.end(),[](const Unit& u){return u.selected && !u.enemy && u.hp>0;});
    if (unitSelected && sprite(2311,1,{813,642}).target.contains(p)) {
        aimedSpell_=0; aimedAttackMove_=true; return;
    }
    if (unitSelected && (sprite(6457,1,{813,678}).target.contains(p) || sprite(6458,1,{744,679.5}).target.contains(p))) {
        aimedAttackMove_=false; aimedSpell_=0;
        const bool hold=sprite(6457,1,{813,678}).target.contains(p);
        for (auto& u:units_) if (u.selected && !u.enemy && u.hp>0) {
            u.order=Order::Stand; u.engageOnMove=true; u.hold=hold; u.velocity={}; u.goal=u.position;
        }
        sound(hold ? "CommandHoldSound" : "CommandStopSound",{},false);
        return;
    }
    if (unitSelected && sprite(8808,1,{675,678}).target.contains(p)) {
        aimedAttackMove_=false; aimedSpell_=0;
        std::vector<Unit*> group;
        bool garrison=false;
        for (auto& u:units_) if (u.selected && !u.enemy && u.hp>0) {
            group.push_back(&u);
            garrison=garrison || !u.garrisoned;
        }
        moveFormation(group,{garrison ? legacyWidth-legacyWidth/3 : legacyWidth+900,depth_/2});
        for (auto* u:group) {
            u->garrisoned=garrison; u->engageOnMove=false;
            if (!garrison && u->symbol==miner && u->resource!=-2) u->order=Order::Gather;
        }
        sound(garrison ? "GarrisonEnter" : "GarrisonExit",{},false);
        return;
    }
    if (mission_ && upgrades_[2] && sprite(8788,1,{675,606}).target.contains(p)) {unitAbility(); return;}
    if (mission_>=3 && sprite(8807,1,{675,606}).target.contains(p)) {unitAbility(); return;}
    if (mission_>=4) {
        if (Rect{716,582,56,48}.contains(p)) {unitAbility(1); return;}
        if (Rect{785,582,56,48}.contains(p)) {unitAbility(2); return;}
    }
    mapDown_=minimap.contains(p);
    if (mapDown_) { point(p); return; }
    if (productionPress(p)) return;
    if (p.y>=575) {
        if (Rect{482,674,50,22}.contains(p)) unavailableControl_="HUD.menuButton symbol=10084";
        else if (Rect{320,674,48,22}.contains(p)) unavailableControl_="HUD.highButton symbol=10102";
        else if (Rect{309,648,94,25}.contains(p)) unavailableControl_="HUD.leftMinerButton symbol=10067";
        else if (Rect{444,648,94,25}.contains(p)) unavailableControl_="HUD.rightMinerButton symbol=10077";
        else if (rangeSelected_ && Rect{643,626,64,70}.contains(p)) unavailableControl_="HUD.CastleArcher additional research";
        return;
    }
    dragStart_=p; updateSelection(p);
}
void CampaignTutorial::release(std::optional<Point> p) {
    if (cinematic_>=0 && !ended()) return;
    if (p && pressedCard_ && cardHit(*p)) {
        if (won()) closingCard_=true;
        else resultCard_=false;
    } else if (showingResults() && p && pressedResult_==resultHit(*p)) resultAction_=pressedResult_;
    pressedCard_=false;
    pressedResult_=ResultAction::None;
    point(p); dragStart_.reset(); mapDown_=false;
}
void CampaignTutorial::updateSelection(Point point) {
    const auto box=between(*dragStart_,point);
    for (auto& u:units_) {
        if (u.enemy || u.hp<=0) continue;
        const double x=u.position.x-camera_,y=battlefieldY_+u.position.y,h=u.height*scale(u);
        u.selected=(x>box.x && x<box.x+box.w && box.y<y && y-h/2<box.y+box.h)
                  || (std::abs(point.x-x)<u.width*scale(u)/2 && point.y>y-h && point.y<y);
    }
}
void CampaignTutorial::command(Point p) {
    if (cinematic_>=0) return;
    if (aimedSpell_) {aimedSpell_=0; return;}
    if (aimedAttackMove_) {aimedAttackMove_=false; return;}
    const bool fromMap=minimap.contains(p);
    if (fromMap) p={(p.x-minimap.x)/minimap.w*length_-camera_,
                   battlefieldY_+(p.y-minimap.y)/minimap.h*depth_};
    const double worldX=p.x+camera_;
    if (p.y>=575 || worldX<0 || worldX>length_ || step_>=Step::Victory) return;
    if (std::none_of(units_.begin(),units_.end(),[](const Unit& u){return u.selected && !u.enemy && u.hp>0;})) return;
    const auto target=fromMap ? CommandTarget{} : pointerTarget(p);
    clickMarkers_.push_back({{worldX,p.y-battlefieldY_},1});
    if (target.kind==CommandTarget::Mine || target.kind==CommandTarget::Pray) {
        for (auto& u:units_) if (!u.enemy && u.selected && isWorker(u.symbol) && u.hp>0) reserveOre(u,target.id);
        sound(target.kind==CommandTarget::Mine ? "ClickGold" : "ClickMana",{},false);
        return;
    }
    sound("CommandMove"+std::to_string(1+soundSequence_++%6),{},false);
    if (target.kind==CommandTarget::Enemy) {
        for (auto& u:units_) if (u.selected && u.hp>0 && !u.enemy) {
            u.garrisoned=false; u.engageOnMove=false; u.target=target.id; u.order=Order::Attack;
        }
        return;
    }
    setFormation({worldX,p.y-battlefieldY_});
}
int CampaignTutorial::requestedMessage() const {
    if (step_==Step::Pray) return 8;
    if (step_==Step::PrayInfo) return 7;
    return stepNumber();
}
void CampaignTutorial::updateMessage() {
    if (voiceTicks_>0) --voiceTicks_;
    const int requested=requestedMessage();
    if (!transitioning_) {
        if (requested!=displayed_ && !voiceTicks_) {
            if (checkFrame_<26 && !messages_.at(displayed_).step.empty()) ++checkFrame_;
            else transitioning_=true;
        }
        return;
    }
    const double offscreen=Viewport::width/2+art_.texture(10139).bounds.w/2;
    const double destination=displayed_==requested ? 0 : -offscreen;
    messageX_+=(destination-messageX_)*.4;
    if (std::abs(messageX_-destination)>=1) return;
    if (displayed_==requested) { transitioning_=false; return; }
    displayed_=requested; checkFrame_=1; messageX_=offscreen;
    nextVoice_="voiceTutorial"+std::to_string(requested);
    voiceTicks_=int(std::ceil(messages_.at(requested).seconds*30));
}
std::string CampaignTutorial::takeVoice() { return std::exchange(nextVoice_,{}); }
std::string CampaignTutorial::takeUnavailableControl() {return std::exchange(unavailableControl_,{});}
void CampaignTutorial::tick() {
    ++ticks_;
    for (auto& mark:clickMarkers_) ++mark.frame;
    std::erase_if(clickMarkers_,[&](const ClickMarker& mark){return mark.frame>cursorFrames_.at(2310);});
    if (step_>=Step::Victory) { tickOutcome(); return; }
    if ((ticks_-1)%60==0) recordArmies();
    if (mission_==5) {
        statues_[1].hp=180; enemyGold_=0;
        if (cinematic_>=0) {updateCinematic(); return;}
    }
    if (step_<=Step::Objective) updateMessage();
    if (cursor_ && cursor_->y<battlefieldY_+240) {
        constexpr double edge=120;
        const double right=cursor_->x-(Viewport::right-edge),left=Viewport::left+edge-cursor_->x;
        targetCamera_+=rules_.at("screenScrollSpeed")*(std::max(0.,right)-std::max(0.,left))/edge;
    }
    if (step_==Step::Select || step_==Step::Move) targetCamera_=camera_=2200;
    if (step_==Step::Scroll) targetCamera_=std::clamp(targetCamera_,1800.,2900.);
    if (step_==Step::Attack) targetCamera_=2800;
    const bool atBase=step_>=Step::Miner && step_<=Step::Mine;
    if (atBase) targetCamera_=legacyWidth;
    if (step_==Step::Enemy || step_==Step::Defend) targetCamera_=units_[invader_].position.x-legacyWidth/2;
    if (step_==Step::Garrison || step_==Step::Retreat) targetCamera_=frontLine()-legacyWidth/2;
    if (step_==Step::Range || step_==Step::Upgrade) targetCamera_=0;
    targetCamera_=std::clamp(targetCamera_,-Viewport::left,length_-Viewport::right);
    const bool slow=atBase || (step_>=Step::Enemy && step_<=Step::Range) || step_==Step::Defend;
    camera_+=int((targetCamera_-camera_)*(slow ? .05 : rules_.at("screenScrollGain")));
    camera_=std::clamp(camera_,-Viewport::left,length_-Viewport::right);
    if ((step_==Step::Move || step_==Step::Scroll || step_==Step::Attack) && !units_[0].selected)
        units_[0].selected=units_[1].selected=true;
    if (!mission_ && step_>=Step::Enemy) updateDefence();
    simulateActors(true);
    if (fastForward()) {
        ++ticks_;
        if ((ticks_-1)%60==0) recordArmies();
        simulateActors(false);
    }
    if (step_>=Step::Victory) return;
    if (mission_) {updateArchidon(); if (mission_==5) updateGiants(); return;}
    if (step_==Step::GoldInfo) ++counter_;
    if (step_>=Step::Enemy) { advanceDefence(); return; }
    if (displayed_!=requestedMessage() || voiceTicks_>0) return;
    switch (step_) {
    case Step::Select:
        if (units_[0].selected && units_[1].selected) step_=Step::Move;
        break;
    case Step::Move:
        if (units_[0].position.x<2500 && units_[1].position.x<2500) {
            spawn(sword,{3350,depth_/2},true); enemyPopulation_+=int(stat(sword,"population")); step_=Step::Scroll;
        }
        break;
    case Step::Scroll: if (camera_>2800) step_=Step::Attack; break;
    case Step::Attack:
        if (units_.back().hp<=0) {
            step_=Step::Return; setFormation({legacyWidth+1000,depth_/2});
            for (auto& u:units_) u.selected=false;
        }
        break;
    case Step::Return: if (camera_<legacyWidth+300) step_=Step::Miner; break;
    case Step::Miner:
        if (units_[2].selected) step_=Step::Pray;
        break;
    case Step::Pray:
        units_[2].selected=true;
        if (units_[2].order==Order::Gather && units_[2].resource<0) step_=Step::PrayInfo;
        break;
    case Step::PrayInfo: if (mana_>10) step_=Step::Mine; break;
    case Step::Mine:
        units_[2].selected=true;
        if (units_[2].order==Order::Gather && units_[2].resource>=0) {
            step_=Step::GoldInfo; goldBalance_=150; counter_=0; trainingAvailable_=true;
        }
        break;
    case Step::GoldInfo:
        if (counter_>150) { step_=production_[0][1].units.empty() ? Step::Train : Step::Training; counter_=0; }
        break;
    case Step::Train: break;
    case Step::Training:
        if (!trainingAvailable_ && ++counter_>150) {
            const auto id=spawn(spearton,{length_-legacyWidth-200,depth_/2},true);
            enemyPopulation_+=int(stat(spearton,"population"));
            auto& foe=units_[id];
            foe.goal={length_-legacyWidth-1000,depth_/2+random()*4.656612875245797e-10*60-30};
            foe.order=Order::Move;
            invader_=id;
            step_=Step::Enemy;
        }
        break;
    default: break;
    }
}
Draw CampaignTutorial::sprite(int symbol,int frame,Point at,double size,bool mirror) const {
    const auto r=art_.texture(symbol,frame).bounds;
    Draw d{symbol,frame,{at.x+(mirror ? -r.x-r.w : r.x)*size,at.y+r.y*size,r.w*size,r.h*size}};
    d.mirrored=mirror;
    return d;
}
std::vector<Draw> CampaignTutorial::drawList() const {
    if (showingResults()) return drawOutcome();
    std::vector<Draw> out;
    for (int symbol:backgroundLayers_) {
        const auto key="layer."+std::to_string(symbol)+".width";
        const double width=rules_.contains(key) ? rules_.at(key) : art_.texture(symbol).bounds.w;
        const double offset=camera_/(length_-legacyWidth)*(width-legacyWidth);
        if (symbol==4094) {
            // Desert's flat sky includes one moon near its right edge.
            // Continue its authored color without reflecting a second moon.
            out.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left,0,Viewport::right-Viewport::left,700},0x2d2d4b));
            out.push_back(sprite(symbol,1,{-offset,0}));
        } else for (int plate:{2,3,1}) out.push_back(sprite(symbol,plate,{-offset,0}));
    }
    std::vector<std::pair<double,Draw>> layers;
    for (int direction:{1,-1}) {
        const double home=direction==1 ? legacyWidth : length_-legacyWidth;
        const bool mirror=direction<0;
        const double x=home-camera_;
        const bool chaos=mirror && chaosOpponent_;
        layers.emplace_back(-battlefieldY_,sprite(chaos ? 2318 : 1872,1,{chaos ? x+legacyWidth : x,0},1,mirror));
        layers.emplace_back(0,sprite(chaos ? 2423 : 1866,1,{x-direction*legacyWidth,0},1,mirror));
        layers.emplace_back(depth_/2+40,sprite(chaos ? 2426 : 1621,1,{chaos ? x+legacyWidth : x,0},1,mirror));
        const double statueX=x+direction*500,statueY=battlefieldY_+depth_/2;
        // Statue.init first scales _statueMc to .8. Team.init scales the
        // enclosing entity once more. 1904 sits at (-160.5,-495.1) in 1922.
        constexpr double size=.8*.8;
        const auto& statue=statues_[mirror];
        if (statue.hp<=0 && step_>=Step::Victory) {
            const int frame=std::clamp(counter_+1,1,89)+(!mirror && ownStatueFrame_==8 ? 356 :
                mirror && enemyStatueFrame_!=1 ? (enemyStatueFrame_==11 ? 445 : mission_==3 ? 267 : mission_ ? 178 : 89) : 0);
            layers.emplace_back(depth_/2,sprite(1922,frame,{statueX,statueY},size,mirror));
            continue;
        }
        layers.emplace_back(depth_/2,sprite(1904,mirror ? enemyStatueFrame_ : ownStatueFrame_,
            {statueX-direction*160.5*size,statueY-495.1*size},size,mirror));
        const double hy=statueY-art_.texture(1904,1).bounds.h*.8*size;
        const Rect bar{statueX-33.6,hy-6,67.2,12};
        layers.emplace_back(depth_/2,Draw::shape(Draw::Kind::Box,bar,0));
        layers.emplace_back(depth_/2,Draw::shape(Draw::Kind::Box,{bar.x+1,bar.y+1,(bar.w-2)*statue.hp/statue.maxHp,bar.h-2},0x00ff00));
        const int boxes=int(std::ceil(rules_.at(mirror ? "oponent.statueHealth" : "player.statueHealth")/75));
        for (int i=1;i<boxes;++i) layers.emplace_back(depth_/2,Draw::shape(Draw::Kind::Box,
                                                {bar.x+i*bar.w/boxes,bar.y,1,bar.h},0));
    }
    for (size_t i=0;i<gold_.size();++i) {
        const auto p=gold_[i];
        const double size=.4*(rules_.at("backScale")+p.y/depth_*(rules_.at("frontScale")-rules_.at("backScale")));
        layers.emplace_back(p.y-3,sprite(1078,1,{p.x-camera_,battlefieldY_+p.y-3},
                                         size*oreRemaining_[i]/rules_.at("goldAtStartOfMap")));
        layers.emplace_back(p.y+3,sprite(1051,1,{p.x-camera_,battlefieldY_+p.y+3},size));
    }
    for (const auto& u:units_) {
        if (u.symbol==bomber && u.deadCounted) continue;
        const double size=scale(u),x=u.position.x-camera_,y=battlefieldY_+u.position.y;
        const auto& seq=animations_.at({u.symbol,u.pose});
        auto layer=[&](Draw d){layers.emplace_back(u.position.y,std::move(d));};
        if (u.hp>0) layer(Draw::shape(Draw::Kind::Ellipse,{x-40*size,y-4.5*size,80*size,15*size},
                                     u.selected ? 0x00ff00 : 0,u.selected ? .3 : .2));
        if (u.symbol==archer && u.hp>0) drawArcher(layers,u);
        else {
            const double perspective=rules_.at("backScale")+u.position.y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
            auto character=sprite(seq.symbol,u.frame+seq.frameBase,{x+u.facing*seq.origin.x*size,y-u.altitude*perspective+seq.origin.y*size},size,u.facing<0);
            character.redOffset=u.tint>=0 ? u.tint : u.enemy ? 75 : 0;
            if (u.symbol==ninja && u.pose=="run") {
                for (int copy:{2,1}) {
                    auto trail=character;
                    const double offset=copy*stat(u,"ninjaCopyDistance")*size;
                    trail.target.x-=u.facing*std::abs(u.velocity.x)*10*offset;
                    trail.target.y-=u.velocity.y*5*offset;
                    trail.alpha=(copy==2 ? 51. : 154.)/256;
                    layer(std::move(trail));
                }
            }
            layer(std::move(character));
        }
        if (u.hp>0) {
            const double w=30*1.3*size,h=7.5*1.2*size;
            const Rect bar{x-w/2,y-u.height*size*1.2-h/2,w,h};
            if (u.poison>0) layer(sprite(1168,1+ticks_%int(rules_.at("poison.frames")),{x,bar.y-20*size},size));
            layer(Draw::shape(Draw::Kind::Box,bar,0));
            layer(Draw::shape(Draw::Kind::Box,{bar.x+.7,bar.y+.7,(w-1.4)*u.hp/u.maxHp,h-1.4},0x00ff00));
            const int segments=int(std::ceil(u.maxHp/75));
            for (int i=1;i<segments;++i) layer(Draw::shape(Draw::Kind::Box,{bar.x+i*w/segments,bar.y,.75,h},0));
        }
    }
    if (mission_) {
        const double towerSize=(rules_.at("backScale")+rules_.at("frontScale"))/2;
        const Point tower{length_/2-camera_,battlefieldY_+depth_/2};
        layers.emplace_back(depth_/2,sprite(1162,1,tower,towerSize));
        auto flame=sprite(1158,1+ticks_%int(rules_.at("hill.flameFrames")),
            {tower.x+2.5*towerSize,tower.y-10.3*towerSize},towerSize);
        flame.alpha=std::abs(towerControl_-50)/50; flame.redOffset=towerControl_>50 ? 255 : 0;
        layers.emplace_back(depth_/2,std::move(flame));
        if (wall_.hp>0) for (int i=0;i<5;++i) {
            const double y=i*depth_/5,size=rules_.at("backScale")+y/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
            layers.emplace_back(y,sprite(1174,int(rules_.at("wall.frame")),{wall_.position.x-camera_,battlefieldY_+y},size));
        }
    }
    if (castleArcher_) drawCastleArcher(layers);
    if (enemyCastleArcher_) {
        if (chaosOpponent_) drawCastleDead(layers);else drawCastleArcher(layers,true);
    }
    drawArrows(layers);
    drawBoulders(layers);
    drawMagic(layers);
    std::stable_sort(layers.begin(),layers.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    for (auto& item:layers) out.push_back(std::move(item.second));
    double vision=legacyWidth;
    for (auto& u:units_) if (!u.enemy && u.hp>0) vision=std::max(vision,u.position.x);
    vision=std::min(length_,vision+rules_.at("visionSize"));
    auto fog=sprite(1595,1,{std::max(vision-camera_,Viewport::left),0});
    if (cinematic_<0 && step_!=Step::Enemy && step_<Step::Victory) { fog.alpha=.7; out.push_back(std::move(fog)); }
    if (dragStart_ && cursor_) {
        const Rect box=between(*dragStart_,*cursor_);
        out.push_back(Draw::shape(Draw::Kind::Box,box,0x00ff00,.2));
        out.push_back(Draw::shape(Draw::Kind::Outline,box,0x00ff00,.75));
    }
    out.push_back(sprite(10118,1,{0,0}));
    if (step_==Step::Free && cinematic_<0) out.push_back(sprite(10094,fastForward_ ? 2 : 1,{530,568}));
    drawProduction(out);
    auto dot=[&](Point p,unsigned color) {
        out.push_back(Draw::shape(Draw::Kind::Box,{minimap.x+p.x/length_*minimap.w,
                                  minimap.y+p.y/depth_*minimap.h,2.5,2.5},color));
    };
    for (auto p:gold_) dot(p,0x666666);
    for (double statueX:{legacyWidth+500,length_-legacyWidth-500}) {
        out.push_back(Draw::shape(Draw::Kind::Box,{minimap.x+statueX/length_*minimap.w-2,
                                  minimap.y+minimap.h/2-2,5,5},0x666666));
    }
    for (auto& u:units_) if (u.hp>0) dot(u.position,u.enemy ? 0xff0000 : u.selected ? 0x00ff00 : 0);
    const double visibleLeft=std::max(0.,camera_+Viewport::left);
    const double visibleRight=std::min(length_,camera_+Viewport::right);
    out.push_back(Draw::shape(Draw::Kind::Outline,{minimap.x+visibleLeft/length_*minimap.w,
                              minimap.y,minimap.w*(visibleRight-visibleLeft)/length_,minimap.h},0));
    vision/=length_;
    if (step_!=Step::Enemy && step_<Step::Victory) out.push_back(Draw::shape(Draw::Kind::Box,{minimap.x+minimap.w*vision,minimap.y,
                                          minimap.w*(1-vision),minimap.h},0,.8));
    auto selected=std::find_if(units_.begin(),units_.end(),[](const Unit& u){return u.selected && u.hp>0;});
    if (rangeSelected_) {
        out.push_back(sprite(castleArcher_ ? 10130 : 10126,1,{675,606}));
        for (auto [id,y]:std::array<std::pair<int,int>,2>{{{8804,642},{10123,678}}}) {
            auto icon=sprite(id,1,{675.,double(y)}); icon.alpha=.2; out.push_back(icon);
        }
        if (researchTicks_>=0) {
            const double ratio=researchTicks_/rules_.at("castleArchers1.time");
            out.push_back(Draw::shape(Draw::Kind::Box,{643,622-32*ratio,64,32*ratio},0,.6));
        }
    } else if (selected!=units_.end()) {
        const int portrait=selected->symbol==miner ? 1447 : selected->symbol==spearton ? 1366 :
            selected->symbol==archer ? 1509 : selected->symbol==ninja ? 1272 : selected->symbol==wizard ? 1531 :
            selected->symbol==healer ? 1480 : selected->symbol==enslavedGiant ? 1314 : 1244;
        out.push_back(sprite(portrait,1,{598,645}));
        out.push_back(sprite(2311,1,{813,642}));
        out.push_back(sprite(6457,1,{813,678}));
        out.push_back(sprite(6458,1,{744,679.5}));
        out.push_back(sprite(selected->garrisoned ? 8780 : 8808,1,{675,678}));
        if (mission_ && upgrades_[2] && selected->symbol==sword) out.push_back(sprite(8788,1,{675,606}));
        if (mission_>=3 && selected->symbol==ninja) out.push_back(sprite(8807,1,{675,606}));
        if (mission_>=4 && selected->symbol==wizard) {
            out.push_back(sprite(8791,1,{675,606}));
            if (upgrades_[17]) out.push_back(sprite(8789,1,{744,606}));
            if (upgrades_[13]) out.push_back(sprite(8801,1,{813,606}));
        }
        if (mission_>=4 && selected->symbol==healer) {
            out.push_back(sprite(8787,1,{675,606}));
            if (upgrades_[8]) out.push_back(sprite(8783,1,{744,606}));
        }
    }
    const double economyX=Viewport::right-329;
    out.push_back(sprite(9726,1,{economyX,-2.25}));
    out.push_back(Draw::label({economyX+57,11,56,22},std::to_string(goldBalance_),12,0xffffff));
    out.push_back(Draw::label({economyX+175,11,56,22},std::to_string(mana_),12,0xffffff));
    out.push_back(Draw::label({economyX+284,11,56,22},std::to_string(population_),12,0xffffff));
    auto digits=[](int v){return std::to_string(v/10)+" "+std::to_string(v%10);};
    out.push_back(Draw::label({378,673,46,26},digits(ticks_/1800),19,0xff0000,true));
    out.push_back(Draw::label({433,673,46,26},digits(ticks_/30%60),19,0xff0000,true));
    if (mission_ && hint_ && hintTicks_<(hint_==1 ? 300 : hint_==3 ? 152 : 225)) {
        const double scale=hint_==1 ? .9 : 1.3;
        const Point at=hint_==1 ? Point{630,510} : Point{425,100};
        out.push_back(sprite(10139,1,at,scale));
        const char* text=hint_==1 ? "Gain full control over individual units by clicking commands such as hold position." :
            hint_==3 ? "A unit has been poisoned. Garrisoning this unit will cure its poison" :
            "Capturing the center tower will award you a continuous stream of gold for as long as you hold the tower";
        out.push_back(Draw::label({at.x-198.3*scale,at.y-39.45*scale,396.6*scale,100*scale},text,17*scale,0xffffff,true));
        if (hint_==1) out.push_back(sprite(1924,1+ticks_%32,{817,665}));
    }
    if (knightHintTicks_>=0) {
        constexpr double size=1.3;
        out.push_back(sprite(10139,1,{425,100},size));
        out.push_back(Draw::label({425-198.3*size,100-39.45*size,396.6*size,100*size},
            "Press SPACE to select all of your attacking units",17*size,0xffffff,true));
    }
    if (displayed_ && step_<=Step::Objective) {
        const double yOffset=displayed_==10 ? 75 : displayed_==16 ? 250 : 0;
        const double x=425+messageX_*1.3,y=100+yOffset*1.3;
        auto& msg=messages_.at(displayed_);
        out.push_back(sprite(10139,1,{x,y},1.3));
        if (!msg.step.empty()) out.push_back(sprite(10138,checkFrame_,{x+195.65*1.3,y+39.4*1.3},1.3));
        out.push_back(Draw::label({x-202.5*1.3,y-49.25*1.3,180,26},msg.step,15,0xffffff));
        out.push_back(Draw::label({x-198.3*1.3,y-39.45*1.3,396.6*1.3,110},msg.text,22,0xffffff,true));
    }
    if (!transitioning_ && step_!=Step::PrayInfo && step_<Step::Training &&
        !(step_==Step::Scroll && targetCamera_>2850)) {
        const Unit& u=units_[step_==Step::Attack ? 3 : step_==Step::Miner ? 2 : 0];
        Point at{u.position.x-camera_,battlefieldY_+u.position.y-u.height*.8};
        if (step_==Step::Move) at={2350-camera_,100+battlefieldY_};
        if (step_==Step::Scroll) at={Viewport::right-50,175};
        if (step_==Step::Return) at={335,585};
        if (step_==Step::Pray) at={legacyWidth+500-camera_,battlefieldY_+depth_/2-175};
        if (step_==Step::Mine) at={gold_[3].x-camera_,battlefieldY_+gold_[3].y-48};
        if (step_==Step::GoldInfo) at={675-Viewport::left,40};
        if (step_==Step::Train) at={95,600};
        const double angle=step_==Step::Scroll ? -90 : step_==Step::GoldInfo ? 180 : 0;
        out.push_back(rotatedSprite(1924,1+ticks_%32,at,1,angle));
    }
    if (!transitioning_ && displayed_==requestedMessage() && step_>=Step::Garrison) {
        std::optional<Point> tip;
        switch (step_) {
        case Step::Garrison: tip=Point{335,625}; break;
        case Step::Range: tip=Point{532-camera_,battlefieldY_-150}; break;
        case Step::Upgrade: tip=Point{680,600}; break;
        case Step::Defend: tip=Point{425,625}; break;
        case Step::Fight: {
            auto& u=units_[invader_];
            tip=Point{u.position.x-camera_,battlefieldY_+u.position.y-u.height*.8};
            break;
        }
        default: break;
        }
        if (tip) out.push_back(sprite(1924,1+ticks_%32,*tip));
    }
    if (step_==Step::Select && canSkip_) out.push_back(sprite(1928,1,{442,100+art_.texture(10139).bounds.h*1.3-140}));
    if (step_==Step::AttackHint || step_==Step::QualityHint) {
        const double slide=std::pow(.6,std::max(0,counter_-14))*Viewport::width*.8;
        const double x=425+slide,y=100+525*.8;
        out.push_back(sprite(10139,1,{x,y},.8));
        const char* hint=step_==Step::AttackHint ? "When you're ready, click here to Attack the enemy!" :
                            "Click here to toggle the graphics quality if the game is running slow for you.";
        out.push_back(Draw::label({x-159,y-31.5,318,80},hint,14,0xffffff,true));
        if (slide<1) out.push_back(sprite(1924,1+ticks_%32,step_==Step::AttackHint ? Point{515,625} : Point{335,680}));
    }
    drawCinematic(out);
    drawPointer(out);
    return out;
}
}
