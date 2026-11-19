#include "campaign_intermission.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sw2 {
namespace {
constexpr Point mapOrigin{-657.7,-584.9};
constexpr std::array mapStops{1,50,92,151,238,298,341,401,441,510,554,599,647,710};
std::filesystem::path mapDirectory(const std::filesystem::path& dir,int level) {
    if (level<2) return dir;
    if (level==2) return dir.parent_path()/"archidon-map";
    if (level==3) return dir.parent_path()/"shadow-map";
    if (level==4) return dir.parent_path()/"magic-map";
    return dir.parent_path()/("campaign-map-"+std::to_string(level+1));
}
}
CampaignIntermission::CampaignIntermission(MenuAssets& a,const std::filesystem::path& dir,
                                           CampaignProgress& p,Page page):art_(a),campaign_(p),page_(page) {
    if (!a.art.contains({694,1})) {a.loadTextures(dir); a.loadPlacements(dir);}
    std::ifstream upgrades(dir/"upgrades.tsv");
    std::string row;
    int count=0;
    while (std::getline(upgrades,row)) {
        std::istringstream in(row);
        int id;
        Node node;
        if (!(in>>id>>node.symbol>>node.parent) || id<1 || id>CampaignProgress::upgradeCount ||
            node.parent<0 || node.parent>CampaignProgress::upgradeCount || nodes_[id].symbol)
            throw std::runtime_error("Invalid campaign upgrade");
        in.get();
        std::getline(in,node.name,'\t'); std::getline(in,node.tip);
        nodes_[id]=std::move(node); ++count;
    }
    if (count!=CampaignProgress::upgradeCount) throw std::runtime_error("Incomplete upgrade tree");
    const int destination=mapStops.at(campaign_.level);
    const auto mapDir=mapDirectory(dir,campaign_.level);
    for (int level=2;level<=campaign_.level;++level)
        if (!a.art.contains({436,mapStops.at(level)})) a.loadTextures(mapDirectory(dir,level));
    std::ifstream movement(mapDir/"map-motion.tsv");
    Motion m;
    int frame;
    while (movement>>frame>>m.scale>>m.x>>m.y>>m.flagX>>m.flagY) {
        if (frame!=int(track_.size())+1) throw std::runtime_error("Invalid campaign map frames");
        track_.push_back(m);
    }
    if (!movement.eof() || track_.size()!=destination) throw std::runtime_error("Incomplete campaign map");
    if (page==Page::Map || campaign_.points==0) enterMap();
}
void CampaignIntermission::enterMap() {
    page_=Page::Map;
    confirm_=savePrompt_=false;
    mapFrame_=mapStops.at(std::max(0,campaign_.level-1)); flagTick_=0;
}
int CampaignIntermission::nodeFrame(int index) const {
    if (campaign_.upgrades[index]) return 3;
    const int parent=nodes_[index].parent;
    return !parent || campaign_.upgrades[parent] ? 2 : 1;
}
CampaignIntermission::Element CampaignIntermission::element(int id,Point origin,double scale,int frame) const {
    const auto& b=art_.texture(id,frame).bounds;
    return {id,{origin.x+b.x*scale,origin.y+b.y*scale,b.w*scale,b.h*scale},scale,origin};
}
CampaignIntermission::Element CampaignIntermission::child(int owner,int id,Point origin,double scale,int frame) const {
    const auto& p=art_.placement(owner,id);
    const Point at{origin.x+scale*p.x,origin.y+scale*p.y};
    const double size=scale*p.sx;
    if (art_.art.contains({id,frame})) return element(id,at,size,frame);
    const auto& b=p.bounds;
    return {id,{at.x+size*b.x,at.y+size*b.y,b.w*size,b.h*size},size,at};
}
std::vector<CampaignIntermission::Element> CampaignIntermission::controls() const {
    std::vector<Element> controls;
    if (page_==Page::Upgrades) {
        if (confirm_) {
            auto dialog=child(694,693);
            for (int id:{689,692}) controls.push_back(child(693,id,dialog.origin,1,4));
        } else {
            for (int i=1;i<=CampaignProgress::upgradeCount;++i)
                controls.push_back(child(694,nodes_[i].symbol,{},1,nodeFrame(i)));
            controls.push_back(child(694,681,{},1,4));
        }
    } else if (savePrompt_) {
        auto prompt=child(499,498,mapOrigin);
        controls.push_back(child(498,497,prompt.origin,1,4));
    } else {
        // The flag becomes interactive only at the destination's frame label.
        if (mapFrame_==int(track_.size())) {
            const auto& m=track_.back();
            controls.push_back(element(393,{m.x+m.flagX*m.scale,m.y+m.flagY*m.scale},m.scale,16));
        }
        const auto panel=child(499,478,mapOrigin);
        const auto buttons=child(478,477,panel.origin);
        for (int id:{campaign_.autoSave ? 454 : 476,463,472})
            controls.push_back(child(477,id,buttons.origin,buttons.scale,4));
    }
    return controls;
}
int CampaignIntermission::target() const {
    if (!pointer_) return 0;
    auto list=controls();
    for (auto it=list.rbegin();it!=list.rend();++it)
        if (it->bounds.contains(*pointer_)) return it->id;
    return 0;
}
void CampaignIntermission::point(std::optional<Point> p) {
    pointer_=p;
    if (page_==Page::Upgrades && !confirm_) {
        const auto symbol=target();
        for (int i=1;i<=CampaignProgress::upgradeCount;++i) if(nodes_[i].symbol==symbol) tip_=i;
    }
}
void CampaignIntermission::press() {pressed_=target();}
void CampaignIntermission::release() {
    const int id=std::exchange(pressed_,0);
    if (!id || id!=target()) return;
    if (page_==Page::Upgrades) {
        if (id==681 && campaign_.points) confirm_=true;
        else if (id==681 || id==689) enterMap();
        else if (id==692) confirm_=false;
        else {
            for (int i=1;i<=CampaignProgress::upgradeCount;++i)
                if (nodes_[i].symbol==id && nodeFrame(i)==2 && campaign_.points>0) {
                    campaign_.upgrades.set(i); --campaign_.points; events_.push_back(Event::Equip);
                }
            return;
        }
    } else {
        switch (id) {
        case 476: campaign_.autoSave=true; events_.push_back(Event::Save); break;
        case 454: campaign_.autoSave=false; break;
        case 497: savePrompt_=false; break;
        case 463: events_.push_back(Event::Guide); break;
        case 472: events_.push_back(Event::Online); break;
        case 393: events_.push_back(Event::StartLevel); break;
        default: return;
        }
    }
    events_.push_back(Event::Click);
}
void CampaignIntermission::tick() {
    if (page_!=Page::Map) return;
    if (mapFrame_<int(track_.size())) {
        if (++mapFrame_==int(track_.size())) {
            events_.push_back(Event::Arrived);
            if (campaign_.autoSave) events_.push_back(Event::Save);
        }
    } else flagTick_=(flagTick_+1)%15;
}
std::vector<CampaignIntermission::Event> CampaignIntermission::takeEvents() {
    return std::exchange(events_,{});
}
std::vector<Draw> CampaignIntermission::drawList() const {
    std::vector<Draw> out;
    const int over=target();
    auto emit=[&](Element e,int frame=1,double alpha=1.) {
        out.push_back({e.id,frame,element(e.id,e.origin,e.scale,frame).bounds,alpha});
    };
    auto button=[&](Element e) {emit(e,e.id==over ? (pressed_==over ? 3 : 2) : 1);};
    auto backdrop=[&](Element panel) {
        out.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left,0,Viewport::width,700},0,127./255));
        emit(panel);
    };
    if (page_==Page::Upgrades) {
        emit(element(507)); emit(element(694));
        for (int i=1;i<=CampaignProgress::upgradeCount;++i) {
            const int frame=nodeFrame(i);
            emit(child(694,nodes_[i].symbol),frame,frame==1 ? .5 : 1.);
        }
        auto info=child(694,671);
        emit(info);
        if (tip_) {
            auto tip=Draw::label({info.origin.x-106,info.origin.y-74.1,213,145},nodes_[tip_].tip,14,0xffc800);
            tip.alpha=153./255; out.push_back(std::move(tip));
        }
        button(child(694,681)); emit(child(694,682),1+campaign_.points);
        if (confirm_) {
            auto dialog=child(694,693);
            backdrop(dialog);
            emit(child(693,684,dialog.origin));
            for (int id:{689,692}) button(child(693,id,dialog.origin));
        }
    } else {
        const auto& m=track_.at(mapFrame_-1);
        const Point pos{m.x,m.y};
        const bool arrived=mapFrame_==int(track_.size());
        emit(element(436,pos,m.scale),campaign_.level>=2 ? mapFrame_ : arrived ? 50 : std::min(2,mapFrame_));
        if (campaign_.level<2 && mapFrame_>2 && !arrived) emit(element(436,pos,m.scale),mapFrame_);
        emit(element(393,{m.x+m.scale*m.flagX,m.y+m.scale*m.flagY},m.scale),arrived ? 1+flagTick_ : 16);
        const auto panel=child(499,478,mapOrigin);
        emit(child(478,439,panel.origin));
        const auto buttons=child(478,477,panel.origin);
        emit(child(477,442,buttons.origin,buttons.scale));
        for (int id:{campaign_.autoSave ? 454 : 476,463,472}) button(child(477,id,buttons.origin,buttons.scale));
        for (int id:{485,486,488}) emit(child(499,id,mapOrigin),id==485 ? 1 : std::max(campaign_.level,1));
        if (savePrompt_) {
            auto prompt=child(499,498,mapOrigin);
            backdrop(child(498,490,prompt.origin)); emit(child(498,491,prompt.origin),std::max(campaign_.level,1));
            button(child(498,497,prompt.origin));
        }
    }
    return out;
}
}
