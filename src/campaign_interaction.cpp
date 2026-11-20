#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <tuple>

namespace sw2 {
bool CampaignTutorial::castleHit(const CastlePart& part,Point p) const {
    const auto mask=castleMasks_.find(part.symbol);
    return mask==castleMasks_.end() ? part.hit.contains(p) : std::any_of(mask->second.begin(),mask->second.end(),[&](const Rect& row){return row.contains(p);});
}
double CampaignTutorial::visionBoundary() const {
    return std::min(length_,std::max(legacyWidth,frontLine())+rules_.at("visionSize"));
}
bool CampaignTutorial::visible(const Unit& unit) const {
    return !unit.enemy || cinematic_>=0 || ended() || step_==Step::Enemy || unit.position.x<=visionBoundary();
}
int CampaignTutorial::tooltipAt() const {
    if(!cursor_ || ended() || cinematic_>=0) return 0;
    struct Entry {int unit,icon;Point at;};
    const Entry entries[]={{miner,10036,{4,589.35}},{sword,10039,{61.6,590}},
        {spearton,10051,{32.85,643.35}},{archer,10042,{119.2,590}},{ninja,10054,{90.4,643.35}},
        {wizard,10048,{234.4,589.35}},{healer,10045,{176.8,589.35}},{enslavedGiant,9852,{205.65,643.35}}};
    for(const auto& entry:entries) {
        if(!trainingAvailable_ || (entry.unit==spearton && !mission_) ||
           (entry.unit==miner && step_<Step::Commands)) continue;
        if((entry.unit==enslavedGiant && mission_<6) || (entry.unit==archer && mission_<2) ||
           (entry.unit==ninja && mission_<3) || ((entry.unit==wizard || entry.unit==healer)&&mission_<4)) continue;
        if(!art_.art.contains({entry.icon,1}) || !sprite(entry.icon,1,entry.at).target.contains(*cursor_)) continue;
        return entry.unit;
    }
    if(rangeSelected_ && Rect{643,590,64,32}.contains(*cursor_)) return -1;
    if(hitControl("garrisonButton",*cursor_)) return -10;
    if(hitControl("defendButton",*cursor_)) return -11;
    if(hitControl("attackButton",*cursor_)) return -12;
    if(toolbarControlHit("leftMinerButton",*cursor_)) return -13;
    if(toolbarControlHit("rightMinerButton",*cursor_)) return -14;
    const auto selected=std::find_if(units_.begin(),units_.end(),[](const Unit& u){return u.selected && !u.enemy && u.hp>0;});
    if(selected!=units_.end() && !rangeSelected_) {
        for(auto [symbol,key,at]:{std::tuple{2311,-20,Point{813,642}},std::tuple{6458,-21,Point{744,679.5}},
                                 std::tuple{6457,-22,Point{813,678}},std::tuple{8808,selected->garrisoned ? -24 : -23,Point{675,678}}})
            if(sprite(symbol,1,at).target.contains(*cursor_)) return key;
        if(Rect{647,582,56,48}.contains(*cursor_)) {
            if(selected->symbol==sword && mission_ && upgrades_[2]) return 90001;
            if(selected->symbol==ninja && mission_>=3) return 90002;
            if(selected->symbol==wizard && mission_>=4) return 90003;
            if(selected->symbol==healer && mission_>=4) return 90006;
        }
        if(Rect{716,582,56,48}.contains(*cursor_)) {
            if(selected->symbol==wizard && mission_>=4 && upgrades_[17]) return 90004;
            if(selected->symbol==healer && mission_>=4 && upgrades_[8]) return 90007;
        }
        if(Rect{785,582,56,48}.contains(*cursor_) && selected->symbol==wizard && mission_>=4 && upgrades_[13]) return 90005;
    }
    return 0;
}
void CampaignTutorial::updateTooltip() {
    if(tooltipRemaining_>0) --tooltipRemaining_;
    const int key=tooltipAt();
    if(key!=hoveredTooltip_) {hoveredTooltip_=key;tooltipHoverTicks_=0;}
    const auto tip=tooltips_.find(key);
    if(tip!=tooltips_.end() && ++tooltipHoverTicks_>=tip->second.delay) {
        shownTooltip_=key;tooltipRemaining_=2;
    }
}
void CampaignTutorial::drawTooltips(std::vector<Draw>& out) const {
    const auto tip=tooltips_.find(shownTooltip_);
    if(tooltipRemaining_>0 && tip!=tooltips_.end() && !ended() && cinematic_<0)
        out.push_back(sprite(1119,tip->second.frame,{0,Viewport::height-700}));
}
void CampaignTutorial::sound(std::string name,Point position,bool spatial) {
    sounds_.push_back({std::move(name),position,spatial});
}
std::vector<CampaignTutorial::SoundEvent> CampaignTutorial::takeSounds() {return std::exchange(sounds_,{});}
void CampaignTutorial::impactSound(const Unit& target) {
    if (target.symbol==1904) sound("StatueHit"+std::to_string(1+soundSequence_++%5),target.position);
    else {
        const bool armour=target.symbol==1174 || stat(target,"armoured")!=0;
        sound(std::string(armour ? "hitOnArmour" : "hitOnFlesh")+std::to_string(1+soundSequence_++%(armour ? 7 : 12)),target.position);
    }
}
CampaignTutorial::CommandTarget CampaignTutorial::pointerTarget(Point point) const {
    if (point.y>=575 || point.x+camera_<0 || point.x+camera_>length_ || ended()) return {};
    const Point plane{point.x+camera_,point.y-battlefieldY_};
    const bool workers=std::any_of(units_.begin(),units_.end(),[](const Unit& u){return !u.enemy && u.selected && u.hp>0 && u.symbol==miner;});
    if (workers) {
        if (std::abs(plane.x-(legacyWidth+500))<125 && plane.y<depth_/2+64 && plane.y>depth_/2-320)
            return {CommandTarget::Pray,-1};
        for (int i=int(gold_.size())-1;i>=0;--i) {
            if (oreRemaining_[i]<=0) continue;
            const auto ore=gold_[i];
            const double size=.4*(rules_.at("backScale")+ore.y/depth_*(rules_.at("frontScale")-rules_.at("backScale")));
            const auto b=art_.texture(1078).bounds;
            if (Rect{ore.x+b.x*size,ore.y-3+b.y*size,b.w*size,b.h*size+6}.contains(plane))
                return {CommandTarget::Mine,i};
        }
    }
    for (int i=int(units_.size())-1;i>=0;--i) {
        const auto& u=units_[i];
        if (!u.enemy || u.hp<=0 || u.cloakTicks || !visible(u)) continue;
        const double size=scale(u);
        if (std::abs(plane.x-u.position.x)<u.width*size/2 && plane.y<=u.position.y+4 && plane.y>=u.position.y-u.height*size)
            return {CommandTarget::Enemy,i};
    }
    const auto& statue=statues_[1];
    if (step_>=Step::Objective && std::abs(plane.x-statue.position.x)<statue.width &&
        plane.y>statue.position.y-statue.height && plane.y<statue.position.y)
        return {CommandTarget::Enemy,opposingStatue};
    return {};
}
int CampaignTutorial::cursorSymbol() const {
    if (!cursor_ || dragStart_ || ended()) return 0;
    if (cursor_->y>=575 && !(aimedAttackMove_ && minimap.contains(*cursor_))) return 0;
    const bool selected=std::any_of(units_.begin(),units_.end(),[](const Unit& u){return u.selected && !u.enemy && u.hp>0;});
    if (!selected) return 0;
    if (aimedSpell_) return aimedSpell_==2 ? 8800 : 8785;
    if (aimedAttackMove_) return 2313;
    switch (pointerTarget(*cursor_).kind) {
    case CommandTarget::Mine: return 8824;
    case CommandTarget::Pray: return 8821;
    case CommandTarget::Enemy: return 2313;
    default: return 0;
    }
}
void CampaignTutorial::drawPointer(std::vector<Draw>& out) const {
    for (const auto& mark:clickMarkers_)
        out.push_back(sprite(2310,mark.frame,{mark.position.x-camera_,battlefieldY_+mark.position.y}));
    const int symbol=cursorSymbol();
    if (!symbol) return;
    const double perspective=rules_.at("backScale")+(cursor_->y-battlefieldY_)/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
    double size=1.3*perspective;
    if (symbol==8821) size*=1.5;
    if (aimedSpell_) {
        auto image=sprite(symbol,1,*cursor_);
        if (aimedSpell_==2) image.target={cursor_->x-stat(wizard,"electricWall.area")/2,battlefieldY_,
                                         stat(wizard,"electricWall.area"),depth_};
        else {
            const double width=aimedSpell_==1 ? stat(wizard,"nuke.area") : stat(wizard,"poisonSpray.range")/2;
            const double height=(aimedSpell_==1 ? .7*width : stat(wizard,"poisonSpray.area"))*perspective;
            image.target={cursor_->x-width/2,cursor_->y-height/2,width,height};
        }
        out.push_back(std::move(image));
    } else out.push_back(sprite(symbol,1+ticks_%cursorFrames_.at(symbol),*cursor_,size));
}
}
