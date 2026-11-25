#include "campaign_menu.hpp"
#include "raster_compositor.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace sw2 {
void Viewport::configure(Aspect mode,int w,int h) {
    aspect=mode;height=700;width=700.*16/9;
    if(mode==Aspect::Dynamic && w>0 && h>0) {
        const double ratio=double(w)/h;
        width=std::max(1000.,700*ratio);height=std::max(700.,1000/ratio);
    }
    left=(850-width)/2;right=left+width;
}
Viewport::Viewport(int w,int h):scale(aspect==Aspect::Stretched ? h/height : std::min(w/width,h/height)),
    scaleX(aspect==Aspect::Stretched ? w/width : scale),
    pixels{(w-width*scaleX)/2,(h-height*scale)/2,width*scaleX,height*scale} {}

std::optional<Point> Viewport::toStage(Point p) const {
    if (!pixels.contains(p) || scale <= 0) return {};
    return Point{left + (p.x - pixels.x)/scaleX,(p.y-pixels.y)/scale};
}
std::optional<Point> Viewport::battlePoint(std::optional<Point> p) {
    if(p && height>700) {
        const double hud=height-125;
        if(p->y>=hud) p->y-=height-700;
        else if(p->y>=575) return {};
    }
    return p;
}
void coverBackdrop(std::vector<Draw>& out,Draw d,Rect area) {
    const auto r=d.target;
    const double xs[]={std::min(area.x,r.x),r.x,r.x+r.w,std::max(area.x+area.w,r.x+r.w)};
    const double ys[]={std::min(area.y,r.y),r.y,r.y+r.h,std::max(area.y+area.h,r.y+r.h)};
    for(int y=0;y<3;++y) for(int x=0;x<3;++x) {
        if(x==1 && y==1) continue;
        if(xs[x+1]<=xs[x] || ys[y+1]<=ys[y]) continue;
        auto edge=d;edge.target={xs[x],ys[y],xs[x+1]-xs[x],ys[y+1]-ys[y]};
        edge.source=Rect{x==0 ? 0. : x==2 ? .999 : 0.,y==0 ? 0. : y==2 ? .999 : 0.,x==1 ? 1. : .001,y==1 ? 1. : .001};
        out.push_back(std::move(edge));
    }
    out.push_back(std::move(d));
}

void MenuAssets::loadTextures(const std::filesystem::path& dir) {
    int id, frame;
    std::string filename;
    Rect bounds;
    std::ifstream textures(dir/"textures.tsv");
    while (textures >> id >> frame >> filename >> bounds.x >> bounds.y >> bounds.w >> bounds.h)
        art[{id,frame}] = Art{dir/filename,bounds};
    if (!textures.eof() || !textures.is_open()) throw std::runtime_error("Invalid textures: "+dir.string());
}
void MenuAssets::loadPlacements(const std::filesystem::path& dir) {
    std::ifstream positions(dir/"placements.tsv");
    Placement p;
    double xmax, ymax;
    while (positions >> p.owner >> p.symbol >> p.frame >> p.depth >> p.name >> p.sx >> p.sy
                     >> p.x >> p.y >> p.bounds.x >> p.bounds.y >> xmax >> ymax) {
        p.bounds.w = xmax - p.bounds.x;
        p.bounds.h = ymax - p.bounds.y;
        placements[p.owner].push_back(p);
    }
    if (!positions.is_open() || !positions.eof()) throw std::runtime_error("Invalid placements: "+dir.string());
}
MenuAssets::MenuAssets(const std::filesystem::path& dir) {
    loadTextures(dir);
    loadPlacements(dir);
    for(const char* folder:{"interface","tooltips","castle","pc-button","toolbar","tactical"}) {
        const auto extra=dir.parent_path()/folder;
        if(std::filesystem::exists(extra/"textures.tsv")) loadTextures(extra);
    }
    std::ifstream opacity(dir/"alpha.tsv");
    int id, frame;
    double value;
    while (opacity >> id >> frame >> value) alphas[{id,frame}] = value;
    if (art.empty() || placements.empty() || alphas.empty() ||
        !opacity.eof())
        throw std::runtime_error("Invalid menu asset manifest");
}

Draw Draw::shape(Kind kind,Rect r,unsigned color,double opacity) {
    Draw d{}; d.kind=kind; d.target=r; d.color=color; d.alpha=opacity;
    return d;
}
Draw Draw::label(Rect r,std::string value,double size,unsigned color,bool centered) {
    auto d=shape(Kind::Text,r,color);
    d.text=std::move(value); d.fontSize=size; d.centered=centered;
    return d;
}

const Placement& MenuAssets::placement(int owner, int id) const {
    const auto& list = placements.at(owner);
    auto it = std::find_if(list.begin(),list.end(),[id](const auto& p){ return p.symbol == id; });
    if (it == list.end()) throw std::runtime_error("Missing original symbol placement");
    return *it;
}
const Art& MenuAssets::texture(int symbol,int frame) const { return art.at({symbol,frame}); }
double MenuAssets::alpha(int symbol,int frame) const { return alphas.at({symbol,frame}); }

constexpr std::array panels{1006,1011,1024,1028};
CampaignMenu::CampaignMenu(const MenuAssets& a,bool save) : assets_(a),hasSave_(save) {
    restart(save);
}
void CampaignMenu::restart(bool hasSave) {
    introPixels_.reset();
    hasSave_=hasSave; page_=Page::Opening; difficulty_=Difficulty::Normal;
    frame_=1; hover_=pressed_=delay_=pending_=introTicks_=0;
    credits_=false; overlay_=0; actions_.clear();
    for (size_t i=0;i<panels.size();++i) panelY_[i]=assets_.placement(1045,panels[i]).y;
}
int CampaignMenu::panel() const {
    switch (page_) {
    case Page::Main: return 1006;
    case Page::NewOrContinue: return 1011;
    case Page::Difficulty: return 1024;
    case Page::Intro: return 1028;
    default: return 0;
    }
}
void CampaignMenu::tick() {
    if (page_ == Page::Opening) {
        if (frame_ < 174) { ++frame_; return; }
        page_ = Page::Main;
    }
    for (size_t i=0;i<panels.size();++i) {
        const auto& p = assets_.placement(1045,panels[i]);
        // Flash measures mainPanel at construction, while SimpleButton 708
        // is in its 32-unit up state. Its later roll-over glow is 60 units.
        const double height = p.symbol==1006 ? assets_.texture(708,1).bounds.h : p.bounds.h;
        const double target = panel()==p.symbol ? 700-height-20 : 750;
        panelY_[i] += (target-panelY_[i])*0.2;
    }
    overlay_ = page_ == Page::Difficulty ? overlay_+(1-overlay_)*0.2 : 0;
    if (page_ == Page::Intro) ++introTicks_;
    if (pending_ && --delay_ <= 0) {
        const int id = pending_;
        pending_ = 0;
        activate(id);
    }
}
void CampaignMenu::activate(int id) {
    switch (id) {
    case 1002: page_=Page::NewOrContinue; break;
    case 1009: page_=Page::Difficulty; break;
    case 1015: case 1021: case 1018:
        difficulty_ = id==1015 ? Difficulty::Normal : id==1021 ? Difficulty::Hard : Difficulty::Insane;
        page_=Page::Intro; introTicks_=0; introPixels_.reset(); break;
    case 1010: page_=Page::CampaignMap; actions_.push_back(Action::LoadCampaign); break;
    case 1027: page_=Page::CampaignMap; actions_.push_back(Action::Map); break;
    case 1032: music_=!music_; actions_.push_back(Action::MusicToggle); break;
    case 1036: credits_=true; break;
    case 1043:
        pending_=delay_=pressed_=0;
        if (credits_) credits_=false; else page_=Page::Main;
        break;
    case 708: actions_.push_back(Action::Online); break;
    case 1005: actions_.push_back(Action::FirstGame); break;
    case 1044: actions_.push_back(Action::Stickpage); break;
    case 998: actions_.push_back(Action::IntroLink); break;
    default: if (id) actions_.push_back(Action::Unavailable); break;
    }
}
static bool delayedButton(int id) {
    return id==1002 || id==1009 || id==1010 || id==1015 || id==1018 || id==1021 || id==1027;
}
void CampaignMenu::press(int id) {
    pressed_=id;
    if (delayedButton(id) && !pending_) {
        pending_=id; delay_=15; actions_.push_back(Action::ClickSound);
    }
}
void CampaignMenu::release(int id) {
    if (id && id == pressed_ && !delayedButton(id)) activate(id);
    pressed_=0;
}
int CampaignMenu::buttonFrame(int id) const {
    if (id==1032) return music_ ? 1 : 2;
    if (!assets_.art.contains({id,3})) return 1;
    if (pending_==id || (pressed_==id && hover_==id)) return 3;
    return hover_==id ? 2 : 1;
}
std::vector<Action> CampaignMenu::takeActions() {
    std::vector<Action> result;
    result.swap(actions_);
    return result;
}

void CampaignMenu::compose(std::vector<Draw>& draws,std::vector<Hit>& hits) const {
    const Rect stage{Viewport::left,0,Viewport::width,Viewport::height};
    auto sprite = [&](int id,int frame,double x,double y,double sx,double sy,double opacity=1.0) {
        if (opacity<=0) return;
        const auto& b=assets_.texture(id,frame).bounds;
        Draw d{id,frame,{x+sx*b.x,y+sy*b.y,sx*b.w,sy*b.h},opacity};
        if(id==976) coverBackdrop(draws,std::move(d),{Viewport::left,0,Viewport::width,Viewport::height});
        else draws.push_back(std::move(d));
    };
    auto placed = [&](const Placement& p,double x,double y,bool interactive) {
        sprite(p.symbol,interactive ? buttonFrame(p.symbol) : 1,x,y,p.sx,p.sy);
        if (interactive) {
            const auto& b=p.bounds;
            hits.push_back({p.symbol,{x+p.sx*b.x,y+p.sy*b.y,b.w*p.sx,b.h*p.sy},
                            assets_.art.contains({p.symbol,4}) ? 4 : 0});
        }
    };
    auto root = [&](int id,double dx,bool interactive,double dy=0.) {
        const auto& p=assets_.placement(1045,id);
        placed(p,p.x+dx,p.y+dy,interactive);
    };
    const bool opening=page_==Page::Opening;
    if (opening) {
        draws.push_back({0,1,stage,1}); // shape 720
        if (frame_<=97) sprite(979,frame_,425,Viewport::height/2,1,1);
    }
    if (!opening || frame_>=124)
        sprite(976,1,0,0,1,1,opening ? assets_.alpha(979,frame_) : 1);
    if (!opening && overlay_>0) {
        draws.push_back({0,1,stage,overlay_*127/255});
        const int button=pending_ ? pending_ : hover_;
        const int info=button==1015 ? 993 : button==1021 ? 987 : button==1018 ? 990 : 0;
        if (info) {
            const auto& p=assets_.placement(994,info);
            sprite(info,1,p.x,p.y,1,1,overlay_);
        }
    }
    sprite(1044,1,422,156.5,1,1,opening ? assets_.alpha(982,std::min(frame_,60)) : 1);
    sprite(699,1,430,209,1,1,opening ? assets_.alpha(983,std::min(frame_,120)) : 1);
    if (!opening && page_!=Page::Intro) hits.push_back({1044,{234,142,376,29},0});
    if (page_ == Page::Intro) {
        draws.push_back({0,1,stage,1});
        if(introPixels_) {
            const double size=std::min(Viewport::width/introPixels_->width,Viewport::height/introPixels_->height);
            Draw picture;picture.target={Viewport::left+(Viewport::width-introPixels_->width*size)/2,
                (Viewport::height-introPixels_->height*size)/2,introPixels_->width*size,introPixels_->height*size};
            picture.pixels=introPixels_;draws.push_back(std::move(picture));
        }
    }
    for (size_t i=0;i<panels.size();++i) {
        const auto& panelPlacement=assets_.placement(1045,panels[i]);
        for (const auto& p:assets_.placements.at(panels[i])) {
            if (p.symbol==1010 && !hasSave_) continue;
            const double x=p.symbol==1009 && !hasSave_ ? 245 : p.x;
            // The original panels animate relative to the 700-high stage.
            // Keep that authored motion below the current viewport's bottom,
            // so inactive pages cannot leak into a taller Dynamic window.
            placed(p,panelPlacement.x+x,panelY_[i]+Viewport::height-700+p.y,p.name!="-");
        }
    }
    root(1032,Viewport::left,true);
    root(1033,Viewport::left,false,Viewport::height-700);
    if (page_==Page::Main && !credits_) root(1036,-Viewport::left,true);
    if (credits_) {
        hits.clear();
        root(1039,0,false);
    }
    if ((page_!=Page::Main && !opening && !(page_==Page::Intro && introTicks_<=90)) || credits_)
        root(1043,-Viewport::left,true);
}
std::vector<Draw> CampaignMenu::drawList() const {
    std::vector<Draw> draws; std::vector<Hit> hits;
    compose(draws,hits); return draws;
}
void CampaignMenu::finishIntro() {
    if(page_==Page::Intro)activate(1027);
}
std::vector<Hit> CampaignMenu::hits() const {
    std::vector<Draw> draws; std::vector<Hit> hits;
    compose(draws,hits); return hits;
}
}
