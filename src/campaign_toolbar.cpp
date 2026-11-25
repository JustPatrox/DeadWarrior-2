#include "campaign_tutorial.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace sw2 {
void CampaignTutorial::loadToolbar(const std::filesystem::path& dir) {
    if(!std::filesystem::exists(dir/"states.tsv")) return;
    std::ifstream states(dir/"states.tsv"),layout(dir/"layout.tsv"),hits(dir/"hits.tsv");
    int owner,state,first,count;ToolbarPart part;Rect span;
    while(states>>owner>>state>>first>>count) {
        if(count<1) throw std::runtime_error("Invalid original toolbar animation length");
        toolbarFrames_[{owner,state}]={first,count};
    }
    while(layout>>part.name>>part.owner>>part.origin.x>>part.origin.y>>part.alpha>>part.flipY) {
        part.key=part.name.starts_with("action") ? 20000+std::stoi(part.name.substr(6)) :
            part.name.ends_with("Button") ? part.owner : 0;
        toolbarParts_.push_back(part);
    }
    while(hits>>owner>>span.x>>span.y>>span.w>>span.h) toolbarMasks_[owner].push_back(span);
    if(!states.eof() || !layout.eof() || !hits.eof()) throw std::runtime_error("Invalid original toolbar manifest");
}
bool CampaignTutorial::actionEnabled(int slot) const {
    if(rangeSelected_) return slot==1;
    const auto selected=std::find_if(units_.begin(),units_.end(),[](const Unit& u){return u.selected && !u.enemy && u.hp>0;});
    if(selected==units_.end()) return false;
    if(slot==6 || slot>=7) return true;
    if(slot==1) return (mission_ && upgrades_[2] && selected->symbol==sword) ||
        (mission_ && upgrades_[4] && selected->symbol==spearton) ||
        (mission_>=3 && selected->symbol==ninja) || (mission_>=4 && (selected->symbol==wizard || selected->symbol==healer));
    if(slot==2) return mission_>=4 && ((selected->symbol==wizard && upgrades_[17]) || (selected->symbol==healer && upgrades_[8]));
    return slot==3 && mission_>=4 && selected->symbol==wizard && upgrades_[13];
}
bool CampaignTutorial::actionHit(int slot,Point p) const {
    const auto part=std::find_if(toolbarParts_.begin(),toolbarParts_.end(),[&](const ToolbarPart& b){return b.key==20000+slot;});
    if(part!=toolbarParts_.end()) return sprite(part->owner,70001,part->origin).target.contains(p);
    constexpr Point origin[]={{675,606},{744,604.5},{813,606},{675,642},{744,642},{813,642},{675,678},{744,679.5},{813,678}};
    const auto at=origin[slot-1];return Rect{at.x-33,at.y-16,66,32}.contains(p);
}
bool CampaignTutorial::toolbarControlHit(const char* name,Point p) const {
    const auto part=std::find_if(toolbarParts_.begin(),toolbarParts_.end(),[&](const ToolbarPart& b){return b.name==name;});
    if(part!=toolbarParts_.end()) {
        const auto mask=toolbarMasks_.find(part->owner);const Point local{p.x-part->origin.x,p.y-part->origin.y};
        if(mask!=toolbarMasks_.end()) return std::any_of(mask->second.begin(),mask->second.end(),[&](const Rect& row){return row.contains(local);});
    }
    const std::string_view key=name;
    if(key=="menuButton") return Rect{482,674,50,22}.contains(p);
    if(key=="leftMinerButton") return Rect{309,648,94,25}.contains(p);
    if(key=="rightMinerButton") return Rect{444,648,94,25}.contains(p);
    const auto mask=controlMasks_.find(name);
    return mask!=controlMasks_.end() && std::any_of(mask->second.begin(),mask->second.end(),[&](const Rect& row){return row.contains(p);});
}
int CampaignTutorial::toolbarAt() const {
    if(!cursor_ || ended() || cinematic_>=0) return 0;
    for(const auto& part:toolbarParts_) {
        if(!part.key) continue;
        if(part.key>=20001) {if(actionEnabled(part.key-20000) && actionHit(part.key-20000,*cursor_)) return part.key;}
        else {
            if(toolbarControlHit(part.name.c_str(),*cursor_)) return part.key;
        }
    }
    return 0;
}
void CampaignTutorial::updateToolbarHover() {
    const int key=toolbarAt();
    if(key!=toolbarHover_) {toolbarHover_=key;toolbarHoverStart_=uiTicks_;}
}
void CampaignTutorial::toolbarMouseDown(bool value) {
    updateToolbarHover();
    const int next=value ? toolbarHover_ : 0;
    if(next!=toolbarPressed_ && toolbarHover_) toolbarHoverStart_=uiTicks_;
    toolbarPressed_=next;
}
void CampaignTutorial::drawToolbar(std::vector<Draw>& out) const {
    for(const auto& part:toolbarParts_) {
        if(!part.key) continue;
        const int state=toolbarHover_==part.key ? toolbarPressed_==part.key ? 3 : 2 : 1;
        const auto frame=toolbarFrames_.find({part.owner,state});
        if(frame==toolbarFrames_.end()) continue;
        const auto [first,count]=frame->second;
        const int age=state==1 ? uiTicks_ : uiTicks_-toolbarHoverStart_;
        out.push_back(toolbarPart(part,first+std::max(0,age)%count));
    }
}
void CampaignTutorial::drawRecruitHighlight(std::vector<Draw>& out) const {
    if(toolbarParts_.empty()) return;
    const int unit=tooltipAt();
    const char* name=unit==miner ? "bankHighlight" : unit==sword || unit==spearton || unit==ninja ? "barracksHighlight" :
        unit==archer ? "archerHighlight" : unit==wizard ? "magikillHighlight" : unit==healer ? "templeHighlight" :
        unit==enslavedGiant ? "giantHighlight" : nullptr;
    if(!name) return;
    const auto part=std::find_if(toolbarParts_.begin(),toolbarParts_.end(),[&](const ToolbarPart& b){return b.name==name;});
    if(part!=toolbarParts_.end()) out.push_back(toolbarPart(*part,74001));
}
Draw CampaignTutorial::recruitOverlay(int unit,Rect fallback) const {
    const char* name=unit==miner ? "minerOverlay" : unit==sword ? "swordwrathOverlay" : unit==spearton ? "speartonOverlay" :
        unit==archer ? "archerOverlay" : unit==ninja ? "ninjaOverlay" : unit==wizard ? "magikillOverlay" : unit==healer ? "monkOverlay" : "giantOverlay";
    const auto part=std::find_if(toolbarParts_.begin(),toolbarParts_.end(),[&](const ToolbarPart& b){return b.name==name;});
    return part!=toolbarParts_.end() ? toolbarPart(*part,74001) : sprite(9855,74001,{fallback.x+fallback.w/2,fallback.y+fallback.h/2});
}
Draw CampaignTutorial::toolbarPart(const ToolbarPart& part,int frame) const {
    auto d=sprite(part.owner,frame,part.origin);d.alpha=part.alpha;
    if(part.flipY) {const auto b=art_.texture(part.owner,frame).bounds;d.target.y=part.origin.y-b.y-b.h;d.mirrored=true;d.rotation=180;}
    return d;
}
}
