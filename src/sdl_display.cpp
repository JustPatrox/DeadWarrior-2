#include "sdl_platform.hpp"
#include "pc_settings_layout.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace sw2 {
namespace {
using pcSettings::panel;
constexpr auto aspects=std::array{"16:9","Stretched","Dynamic [Experimental]"};
constexpr auto aspectNames=std::array{"16:9","stretched","dynamic"};
constexpr auto modes=std::array{"Windowed","Borderless Window","Fullscreen"};
struct Rate { int fps; const char* label; };
constexpr Rate rates[]={{30,"30"},{60,"60"},{120,"120"},{240,"240"},{0,"Unlimited"}};
struct Resolution { int width,height; const char* label; };
constexpr Resolution resolutions[]={{0,0,"Auto"},{1280,720,"1280 x 720"},{1600,900,"1600 x 900"},
    {1920,1080,"1920 x 1080"},{2560,1440,"2560 x 1440"},{3840,2160,"3840 x 2160"}};
constexpr int resolutionCount=int(std::size(resolutions));
constexpr int modeCount=int(modes.size()),rateCount=int(std::size(rates));
constexpr int syncChoice=modeCount+rateCount,counterChoice=syncChoice+1,
    resolutionChoice=counterChoice+1,mouseChoice=resolutionChoice+1,aspectChoice=mouseChoice+1,choiceCount=aspectChoice+1;
constexpr Rect choiceBounds(int i) {
    if(i==mouseChoice) return {219,585,412,34};
    if(i==aspectChoice) return {219,627,412,34};
    if (i==resolutionChoice) return {219,185,412,30};
    if (i<modeCount) return {219,220.+57*i,412,45};
    if (i==syncChoice) return {219,485,412,44};
    if (i==counterChoice) return {219,537,412,44};
    return {219.+84*(i-modeCount),428,76,40};
}
}

SdlDisplay::SdlDisplay(SDL_Window* window,SDL_Renderer* renderer,const std::filesystem::path& path)
    :window_(window),renderer_(renderer),path_(path) {
    std::ifstream input(path);std::string line;int startup=0;
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        const auto at=line.find('=');if(at==std::string::npos) continue;
        const auto key=line.substr(0,at),value=line.substr(at+1);
        int number{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
        const bool numeric=parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size();
        if(key=="mode") startup=value=="fullscreen" ? 2 : value=="borderless" ? 1 : 0;
        else if(key=="resolution") {for(int i=0;i<resolutionCount;++i)
            if(value==(i ? std::to_string(resolutions[i].width)+"x"+std::to_string(resolutions[i].height) : "auto")) resolution_=i;}
        else if(key=="fps_limit"&&numeric) {for(int i=0;i<rateCount;++i) {if(number==rates[i].fps) rate_=i;}}
        else if(key=="vsync"&&numeric) vsync_=number==1;
        else if(key=="show_fps"&&numeric) fps_=number==1;
        else if(key=="mouse_lock"&&numeric) mouseLock_=number==1;
        else if(key=="aspect_mode") {for(int i=0;i<3;++i) if(value==aspectNames[i]) aspect_=i;}
    }
    SDL_SetRenderVSync(renderer_,vsync_ ? 1 : 0);
    SDL_SetWindowAspectRatio(window_,aspect_==0 ? 16.f/9 : 0,aspect_==0 ? 16.f/9 : 0);
    if(startup==0&&resolution_>0) apply(resolutionChoice);
    mode(startup);save();
}
int SdlDisplay::frameLimit() const {return rates[rate_].fps;}
void SdlDisplay::mode(int next) {
    if(next==mode_) return;
    if(mode_==0) {SDL_GetWindowPosition(window_,&windowed_[0],&windowed_[1]);SDL_GetWindowSize(window_,&windowed_[2],&windowed_[3]);}
    SDL_SetWindowFullscreen(window_,false);
    SDL_SetWindowBordered(window_,next==0);
    if(next==2 || next==1) SDL_SetWindowFullscreen(window_,true);
    else {SDL_SetWindowSize(window_,windowed_[2],windowed_[3]);SDL_SetWindowPosition(window_,windowed_[0],windowed_[1]);}
    mode_=next;focus_=next;
}
void SdlDisplay::apply(int choice) {
    if(choice<modeCount) mode(choice);
    else if(choice==syncChoice) {vsync_=!vsync_;SDL_SetRenderVSync(renderer_,vsync_ ? 1 : 0);}
    else if(choice==counterChoice) fps_=!fps_;
    else if(choice==mouseChoice) mouseLock_=!mouseLock_;
    else if(choice==aspectChoice) {aspect_=(aspect_+1)%3;SDL_SetWindowAspectRatio(window_,aspect_==0 ? 16.f/9 : 0,aspect_==0 ? 16.f/9 : 0);}
    else if(choice!=resolutionChoice) rate_=choice-modeCount;
    if(choice==resolutionChoice&&mode_==0&&resolution_>0) {
        SDL_Rect area{};SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window_),&area);
        const int units=std::max(1,std::min({resolutions[resolution_].width/16,area.w/16,std::max(1,(area.h-60)/9)}));
        SDL_SetWindowSize(window_,units*16,units*9);
    }
    save();
}
void SdlDisplay::key(SDL_Keycode key) {
    if(key==SDLK_DOWN) {
        if(focus_==counterChoice) focus_=mouseChoice;
        else if(focus_==mouseChoice) focus_=aspectChoice;
        else if(focus_==aspectChoice) focus_=resolutionChoice;
        else if(focus_==resolutionChoice) focus_=0;
        else if(focus_==syncChoice) focus_=counterChoice;
        else if(focus_>=modeCount) focus_=syncChoice;
        else if(focus_==modeCount-1) focus_=modeCount+rate_;else ++focus_;
    } else if(key==SDLK_UP) {
        if(focus_==0) focus_=resolutionChoice;
        else if(focus_==resolutionChoice) focus_=aspectChoice;
        else if(focus_==aspectChoice) focus_=mouseChoice;
        else if(focus_==mouseChoice) focus_=counterChoice;
        else if(focus_==counterChoice) focus_=syncChoice;
        else if(focus_==syncChoice) focus_=modeCount+rate_;else focus_=std::min(focus_-1,modeCount-1);
    } else if(key==SDLK_LEFT||key==SDLK_RIGHT) {
        const int dir=key==SDLK_LEFT ? -1 : 1;
        if(focus_==aspectChoice) {aspect_=(aspect_+dir+3)%3;SDL_SetWindowAspectRatio(window_,aspect_==0 ? 16.f/9 : 0,aspect_==0 ? 16.f/9 : 0);save();}
        else if(focus_==resolutionChoice) {resolution_=(resolution_+dir+resolutionCount)%resolutionCount;apply(resolutionChoice);focus_=resolutionChoice;}
        else if(focus_>=modeCount&&focus_<syncChoice) focus_=modeCount+(focus_-modeCount+dir+rateCount)%rateCount;
    } else if(key==SDLK_TAB) focus_=(focus_+((SDL_GetModState()&SDL_KMOD_SHIFT) ? -1 : 1)+choiceCount)%choiceCount;
    else if(key==SDLK_HOME) focus_=0;
    else if(key==SDLK_END) focus_=counterChoice;
    else if(key==SDLK_RETURN||key==SDLK_SPACE) apply(focus_);
}
void SdlDisplay::pointer(int action,std::optional<Point> point) {
    point=pcSettings::local(point);
    int hit=-1;if(point) for(int i=0;i<choiceCount;++i) if(choiceBounds(i).contains(*point)) hit=i;
    if(action==1) {pressed_=hit;if(hit>=0) focus_=hit;}
    else if(action==2) {
        const bool confirmed=pressed_==hit&&hit>=0;pressed_=-1;
        if(confirmed) {
            if(hit==resolutionChoice&&point->x>=366) resolution_=(resolution_+(point->x<398 ? -1 : 1)+resolutionCount)%resolutionCount;
            apply(hit);
        }
    }
}
std::pair<int,int> SdlDisplay::renderSize(int w,int h) const {
    if(!resolution_||w<=0||h<=0) return {w,h};
    const double viewHeight=aspect_==0 ? std::min(double(h),w*9./16) : h;
    const double scale=std::min({resolutions[resolution_].height/viewHeight,8192./w,8192./h});
    return {std::max(1,int(std::lround(w*scale))),std::max(1,int(std::lround(h*scale)))};
}
void SdlDisplay::save() const {
    const char* names[]={"windowed","borderless","fullscreen"};
    std::error_code error;std::filesystem::create_directories(path_.parent_path(),error);if(error) return;
    auto temporary=path_;temporary+=".tmp";std::ofstream output(temporary);
    output<<"[Display]\nmode="<<names[mode_]<<"\nresolution=";
    if(resolution_) output<<resolutions[resolution_].width<<'x'<<resolutions[resolution_].height;else output<<"auto";
    output<<"\nfps_limit="<<frameLimit()<<"\nvsync="<<int(vsync_)<<"\nshow_fps="<<int(fps_)<<"\nmouse_lock="<<int(mouseLock_)<<"\naspect_mode="<<aspectNames[aspect_]<<'\n';output.close();
    if(output) std::filesystem::rename(temporary,path_,error);
}

void SdlDisplay::append(std::vector<Draw>& draws) const {
    if (!open_) return;
    using Kind=Draw::Kind;
    draws.push_back(Draw::shape(Kind::Box,{Viewport::left,0,Viewport::width,Viewport::height},0,0.72));
    const auto first=draws.size();
    auto bevel=[&](Rect bounds,double corner,unsigned color) {
        auto frame=Draw::shape(Kind::Polygon,bounds,color);
        frame.vertices={{corner,0},{bounds.w-corner,0},{bounds.w,corner},
            {bounds.w,bounds.h-corner},{bounds.w-corner,bounds.h},{corner,bounds.h},
            {0,bounds.h-corner},{0,corner}};
        draws.push_back(frame);
    };
    bevel(panel,12,0xb18a32);
    bevel({panel.x+3,panel.y+3,panel.w-6,panel.h-6},10,0x100c05);
    draws.push_back(Draw::label({219,149,412,35},"PC Settings",25,0xffcc00,true));
    const auto resolution=choiceBounds(resolutionChoice);
    draws.push_back(Draw::shape(Kind::Box,resolution,focus_==resolutionChoice ? 0x523b1c : 0x2a1d0e));
    if (focus_==resolutionChoice) draws.push_back(Draw::shape(Kind::Outline,resolution,0xe3b566));
    draws.push_back(Draw::label({233,190,130,22},"Resolution",15,0xf5f6f9));
    draws.push_back(Draw::label({366,190,32,22},"<",15,0xe3b566,true));
    draws.push_back(Draw::label({398,190,201,22},resolutions[focus_==resolutionChoice ? resolution_ : resolution_].label,15,0xf5f6f9,true));
    draws.push_back(Draw::label({599,190,32,22},">",15,0xe3b566,true));
    for (int i=0;i<modeCount;++i) {
        const auto r=choiceBounds(i);
        const bool selected=i==int(mode_);
        draws.push_back(Draw::shape(Kind::Box,r,focus_==i ? 0x523b1c : 0x2a1d0e));
        if (focus_==i) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x+14,r.y+10,300,26},modes[i],18,0xf5f6f9));
        if (selected) draws.push_back(Draw::label({r.x+r.w-80,r.y+14,70,23},"Active",14,0xe3b566));
    }
    draws.push_back(Draw::label({219,396,412,23},"FPS limit",16,0xf5f6f9));
    for (int i=0;i<rateCount;++i) {
        const int choice=modeCount+i;
        const auto r=choiceBounds(choice);
        const bool selected=rate_==i;
        draws.push_back(Draw::shape(Kind::Box,r,selected ? 0x654515 : 0x2a1d0e));
        if (focus_==choice) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x,r.y+10,r.w,20},rates[i].label,14,selected ? 0xffd992 : 0xf5f6f9,true));
    }
    const auto sync=choiceBounds(syncChoice);
    draws.push_back(Draw::shape(Kind::Box,sync,focus_==syncChoice ? 0x523b1c : 0x2a1d0e));
    if (focus_==syncChoice) draws.push_back(Draw::shape(Kind::Outline,sync,0xe3b566));
    draws.push_back(Draw::label({233,496,270,24},"V-Sync",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,492,66,30},vsync_ ? 0x654515 : 0x100c05));
    draws.push_back(Draw::label({555,498,66,20},vsync_ ? "On" : "Off",14,vsync_ ? 0xffd992 : 0xbfc6d0,true));
    const auto counter=choiceBounds(counterChoice);
    draws.push_back(Draw::shape(Kind::Box,counter,focus_==counterChoice ? 0x523b1c : 0x2a1d0e));
    if (focus_==counterChoice) draws.push_back(Draw::shape(Kind::Outline,counter,0xe3b566));
    draws.push_back(Draw::label({233,548,270,24},"Show FPS",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,544,66,30},fps_ ? 0x654515 : 0x100c05));
    draws.push_back(Draw::label({555,550,66,20},fps_ ? "On" : "Off",14,fps_ ? 0xffd992 : 0xbfc6d0,true));
    const auto mouse=choiceBounds(mouseChoice),aspect=choiceBounds(aspectChoice);
    draws.push_back(Draw::shape(Kind::Box,mouse,focus_==mouseChoice ? 0x523b1c : 0x2a1d0e));
    draws.push_back(Draw::label({233,592,270,22},"Lock mouse to game",16,0xf5f6f9));
    draws.push_back(Draw::label({555,592,66,22},mouseLock_ ? "On" : "Off",14,0xffd992,true));
    draws.push_back(Draw::shape(Kind::Box,aspect,focus_==aspectChoice ? 0x523b1c : 0x2a1d0e));
    draws.push_back(Draw::label({233,635,120,22},"Aspect mode",16,0xf5f6f9));
    draws.push_back(Draw::label({355,635,265,22},aspects[aspect_],14,0xffd992,true));
    draws.push_back(Draw::label({219,673,412,22},std::string(settingsKeyLabel)+" or Esc to close",14,0xbfc6d0));
    pcSettings::center(draws,first);
}


}
