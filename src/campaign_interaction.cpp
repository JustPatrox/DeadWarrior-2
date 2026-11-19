#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace sw2 {
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
        if (!u.enemy || u.hp<=0 || u.cloakTicks) continue;
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
