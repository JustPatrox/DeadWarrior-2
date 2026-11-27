#include "windows_display.hpp"
#include "pc_settings_layout.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <charconv>
#include <fstream>
#include <sstream>

using sw2::Draw;
using sw2::Rect;
using sw2::Viewport;

namespace {
using sw2::pcSettings::panel;
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

int DisplaySettings::frameLimit() const { return rates[rateChoice_].fps; }
std::string DisplaySettings::resolutionLabel() const { return resolutions[resolutionChoice_].label; }

std::string DisplaySettings::optionsText() const {
    const auto& resolution=resolutions[resolutionChoice_];
    const char* names[]={"windowed","borderless","fullscreen"};
    std::ostringstream out;
    out<<"[Display]\nmode="<<names[int(mode_)]<<"\nresolution=";
    if (!resolutionChoice_) out<<"auto";
    else out<<resolution.width<<'x'<<resolution.height;
    out<<"\nfps_limit="<<frameLimit()<<"\nvsync="<<int(vsync_)<<"\nshow_fps="<<int(showFps_)<<"\nmouse_lock="<<int(mouseLock_)<<"\naspect_mode="<<aspectNames[aspectChoice_]<<'\n';
    return out.str();
}

bool DisplaySettings::loadOptions(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return false;
    auto trim=[](std::string value) {
        const auto begin=value.find_first_not_of(" \t\r");
        return begin==std::string::npos ? std::string{} : value.substr(begin,value.find_last_not_of(" \t\r")-begin+1);
    };
    bool valid=true; std::string line;
    while (std::getline(input,line)) {
        line=trim(line);
        if (line.empty() || line[0]=='#' || line[0]==';' || line[0]=='[') continue;
        const auto separator=line.find('=');
        if (separator==std::string::npos) {valid=false;continue;}
        const auto key=trim(line.substr(0,separator)),value=trim(line.substr(separator+1));
        if (key=="mode") {
            if (value=="windowed") startupMode_=Mode::Windowed;
            else if (value=="borderless") startupMode_=Mode::Borderless;
            else if (value=="fullscreen") startupMode_=Mode::Fullscreen;
            else valid=false;
        } else if (key=="resolution") {
            bool found=false;
            for (int i=0;i<resolutionCount;++i) {
                const auto name=i ? std::to_string(resolutions[i].width)+"x"+std::to_string(resolutions[i].height) : "auto";
                if (value==name) {resolutionChoice_=resolutionFocus_=i;found=true;break;}
            }
            if (!found) valid=false;
        } else if(key=="aspect_mode") {
            bool found=false;for(int i=0;i<3;++i) if(value==aspectNames[i]) {aspectChoice_=i;found=true;}
            if(!found) valid=false;
        } else if (key=="fps_limit" || key=="vsync" || key=="show_fps" || key=="mouse_lock") {
            int number{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
            if (parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) {valid=false;continue;}
            if (key=="fps_limit") {
                bool found=false;
                for (int i=0;i<rateCount;++i) if (rates[i].fps==number) {rateChoice_=i;found=true;break;}
                if (!found) valid=false;
            } else if (number!=0 && number!=1) valid=false;
            else if (key=="vsync") vsync_=number!=0;
            else if(key=="show_fps") showFps_=number!=0;
            else mouseLock_=number!=0;
        }
    }
    return valid && !input.bad();
}

bool DisplaySettings::saveOptions(const std::filesystem::path& path) {
    const auto text=optionsText();
    std::error_code error;
    if (text==savedOptions_ && std::filesystem::exists(path,error)) return true;
    std::filesystem::create_directories(path.parent_path(),error);
    if (error) return false;
    const auto temporary=path.parent_path()/L"options.ini.tmp";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    output<<text;output.close();
    if (!output || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return false;
    savedOptions_=text;return true;
}

void DisplaySettings::restore(HWND hwnd) {
    fitWindow(hwnd);
    setMode(hwnd,startupMode_);
}

std::pair<int,int> DisplaySettings::renderSize(int width,int height) const {
    if (!resolutionChoice_ || width<=0 || height<=0) return {width,height};
    // Include letterbox pixels in the buffer so presentation to the HWND stays
    // uniform even when its client area is not 16:9. Input still uses the client.
    const double viewHeight=aspectChoice_==0 ? std::min(double(height),width*9./16.) : height;
    const double scale=std::min({resolutions[resolutionChoice_].height/viewHeight,
                              8192./width,8192./height});
    return {std::max(1,int(std::lround(width*scale))),std::max(1,int(std::lround(height*scale)))};
}

void DisplaySettings::fitWindow(HWND hwnd) {
    if (mode_!=Mode::Windowed || !resolutionChoice_) return;
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT border{};
    AdjustWindowRectExForDpi(&border,DWORD(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,
                           DWORD(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)),GetDpiForWindow(hwnd));
    const int extraW=border.right-border.left,extraH=border.bottom-border.top;
    const auto& selected=resolutions[resolutionChoice_];
    const int units=std::max(1,std::min({selected.width/16,
        int((monitor.rcWork.right-monitor.rcWork.left-extraW)/16),
        int((monitor.rcWork.bottom-monitor.rcWork.top-extraH)/9)}));
    const int width=units*16+extraW,height=units*9+extraH;
    if (IsZoomed(hwnd) || IsIconic(hwnd)) ShowWindow(hwnd,SW_RESTORE);
    SetWindowPos(hwnd,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,
        monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
}

void DisplaySettings::applyResolution(HWND hwnd,int choice) {
    resolutionChoice_=resolutionFocus_=choice;
    fitWindow(hwnd);
    InvalidateRect(hwnd,nullptr,FALSE);
}

void DisplaySettings::setOpen(bool open) {
    open_=open;
    focused_=int(mode_);
    resolutionFocus_=resolutionChoice_;
    pressed_=-1;
}

void DisplaySettings::setMode(HWND hwnd,Mode next) {
    if (next==mode_) return;
    const auto old=mode_;
    mode_=next;
    focused_=int(next);
    if (next==Mode::Windowed) {
        SetWindowLongPtrW(hwnd,GWL_STYLE,windowStyle_);
        SetWindowPlacement(hwnd,&windowed_);
        SetWindowPos(hwnd,nullptr,0,0,0,0,
            SWP_FRAMECHANGED|SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE);
        fitWindow(hwnd);
        return;
    }

    if (old==Mode::Windowed) {
        windowStyle_=GetWindowLongPtrW(hwnd,GWL_STYLE)|WS_VISIBLE;
        GetWindowPlacement(hwnd,&windowed_);
        if (windowed_.showCmd==SW_HIDE) windowed_.showCmd=SW_SHOWNORMAL;
    }
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT r=monitor.rcMonitor;
    const auto visible=GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_VISIBLE;
    SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|visible);
    SetWindowPos(hwnd,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,
                 SWP_FRAMECHANGED|(visible ? 0 : SWP_NOACTIVATE));
}

void DisplaySettings::apply(HWND hwnd,int choice) {
    if (choice<modeCount) setMode(hwnd,Mode(choice));
    else if (choice==syncChoice) vsync_=!vsync_;
    else if (choice==counterChoice) showFps_=!showFps_;
    else if(choice==mouseChoice) mouseLock_=!mouseLock_;
    else if(choice==aspectChoice) aspectChoice_=(aspectChoice_+1)%3;
    else if (choice==resolutionChoice) applyResolution(hwnd,resolutionFocus_);
    else rateChoice_=choice-modeCount;
}

void DisplaySettings::key(HWND hwnd,WPARAM key) {
    switch (key) {
    case VK_DOWN:
        if (focused_==counterChoice) focused_=mouseChoice;
        else if(focused_==mouseChoice) focused_=aspectChoice;
        else if(focused_==aspectChoice) focused_=resolutionChoice;
        else if (focused_==resolutionChoice) focused_=0;
        else if (focused_==syncChoice) focused_=counterChoice;
        else if (focused_>=modeCount) focused_=syncChoice;
        else if (focused_==modeCount-1) focused_=modeCount+rateChoice_;
        else ++focused_;
        break;
    case VK_UP:
        if (focused_==0) focused_=resolutionChoice;
        else if (focused_==resolutionChoice) focused_=aspectChoice;
        else if(focused_==aspectChoice) focused_=mouseChoice;
        else if(focused_==mouseChoice) focused_=counterChoice;
        else if (focused_==counterChoice) focused_=syncChoice;
        else if (focused_==syncChoice) focused_=modeCount+rateChoice_;
        else focused_=std::min(focused_-1,modeCount-1);
        break;
    case VK_LEFT: case VK_RIGHT:
        if(focused_==aspectChoice) aspectChoice_=(aspectChoice_+(key==VK_LEFT ? -1 : 1)+3)%3;
        else if (focused_==resolutionChoice) {
            resolutionFocus_=(resolutionFocus_+(key==VK_LEFT ? -1 : 1)+resolutionCount)%resolutionCount;
        } else if (focused_>=modeCount && focused_<syncChoice) {
            const int direction=key==VK_LEFT ? -1 : 1;
            focused_=modeCount+(focused_-modeCount+direction+rateCount)%rateCount;
        }
        break;
    case VK_TAB:
        focused_=(focused_+((GetKeyState(VK_SHIFT)&0x8000) ? -1 : 1)+choiceCount)%choiceCount;
        break;
    case VK_HOME: focused_=0; break;
    case VK_END: focused_=counterChoice; break;
    case VK_RETURN: case VK_SPACE: apply(hwnd,focused_); break;
    }
}

void DisplaySettings::pointer(HWND hwnd,UINT message,std::optional<sw2::Point> point) {
    if (message!=WM_LBUTTONDOWN && message!=WM_LBUTTONUP && message!=WM_CAPTURECHANGED) return;
    point=sw2::pcSettings::local(point);
    int hit=-1;
    if (point) for (int i=0;i<choiceCount;++i)
        if (choiceBounds(i).contains(*point)) hit=i;
    if (message==WM_LBUTTONDOWN) {
        pressed_=hit;
        if (hit>=0) {
            focused_=hit;
            SetCapture(hwnd);
        }
    } else if (message==WM_LBUTTONUP) {
        const bool confirm=hit>=0 && pressed_==hit;
        pressed_=-1;
        if (GetCapture()==hwnd) ReleaseCapture();
        if (confirm && hit==resolutionChoice && point && point->x>=366) {
            const int direction=point->x<398 ? -1 : 1;
            applyResolution(hwnd,(resolutionChoice_+direction+resolutionCount)%resolutionCount);
        } else if (confirm) apply(hwnd,hit);
    } else if (message==WM_CAPTURECHANGED) {
        pressed_=-1;
    }
}

void DisplaySettings::append(std::vector<Draw>& draws) const {
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
    draws.push_back(Draw::shape(Kind::Box,resolution,focused_==resolutionChoice ? 0x523b1c : 0x2a1d0e));
    if (focused_==resolutionChoice) draws.push_back(Draw::shape(Kind::Outline,resolution,0xe3b566));
    draws.push_back(Draw::label({233,190,130,22},"Resolution",15,0xf5f6f9));
    draws.push_back(Draw::label({366,190,32,22},"<",15,0xe3b566,true));
    draws.push_back(Draw::label({398,190,201,22},resolutions[focused_==resolutionChoice ? resolutionFocus_ : resolutionChoice_].label,15,0xf5f6f9,true));
    draws.push_back(Draw::label({599,190,32,22},">",15,0xe3b566,true));
    for (int i=0;i<modeCount;++i) {
        const auto r=choiceBounds(i);
        const bool selected=i==int(mode_);
        draws.push_back(Draw::shape(Kind::Box,r,focused_==i ? 0x523b1c : 0x2a1d0e));
        if (focused_==i) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x+14,r.y+10,300,26},modes[i],18,0xf5f6f9));
        if (selected) draws.push_back(Draw::label({r.x+r.w-80,r.y+14,70,23},"Active",14,0xe3b566));
    }
    draws.push_back(Draw::label({219,396,412,23},"FPS limit",16,0xf5f6f9));
    for (int i=0;i<rateCount;++i) {
        const int choice=modeCount+i;
        const auto r=choiceBounds(choice);
        const bool selected=rateChoice_==i;
        draws.push_back(Draw::shape(Kind::Box,r,selected ? 0x654515 : 0x2a1d0e));
        if (focused_==choice) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x,r.y+10,r.w,20},rates[i].label,14,selected ? 0xffd992 : 0xf5f6f9,true));
    }
    const auto sync=choiceBounds(syncChoice);
    draws.push_back(Draw::shape(Kind::Box,sync,focused_==syncChoice ? 0x523b1c : 0x2a1d0e));
    if (focused_==syncChoice) draws.push_back(Draw::shape(Kind::Outline,sync,0xe3b566));
    draws.push_back(Draw::label({233,496,270,24},"V-Sync",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,492,66,30},vsync_ ? 0x654515 : 0x100c05));
    draws.push_back(Draw::label({555,498,66,20},vsync_ ? "On" : "Off",14,vsync_ ? 0xffd992 : 0xbfc6d0,true));
    const auto counter=choiceBounds(counterChoice);
    draws.push_back(Draw::shape(Kind::Box,counter,focused_==counterChoice ? 0x523b1c : 0x2a1d0e));
    if (focused_==counterChoice) draws.push_back(Draw::shape(Kind::Outline,counter,0xe3b566));
    draws.push_back(Draw::label({233,548,270,24},"Show FPS",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,544,66,30},showFps_ ? 0x654515 : 0x100c05));
    draws.push_back(Draw::label({555,550,66,20},showFps_ ? "On" : "Off",14,showFps_ ? 0xffd992 : 0xbfc6d0,true));
    const auto mouse=choiceBounds(mouseChoice),aspect=choiceBounds(aspectChoice);
    draws.push_back(Draw::shape(Kind::Box,mouse,focused_==mouseChoice ? 0x523b1c : 0x2a1d0e));
    draws.push_back(Draw::label({233,592,270,22},"Lock mouse to game",16,0xf5f6f9));
    draws.push_back(Draw::label({555,592,66,22},mouseLock_ ? "On" : "Off",14,0xffd992,true));
    draws.push_back(Draw::shape(Kind::Box,aspect,focused_==aspectChoice ? 0x523b1c : 0x2a1d0e));
    draws.push_back(Draw::label({233,635,120,22},"Aspect mode",16,0xf5f6f9));
    draws.push_back(Draw::label({355,635,265,22},aspects[aspectChoice_],14,0xffd992,true));
    draws.push_back(Draw::label({219,673,412,22},"Insert or Esc to close",14,0xbfc6d0));
    sw2::pcSettings::center(draws,first);
}

void DisplaySettings::resize(HWND hwnd,WPARAM edge,RECT& outer) const {
    if (mode_!=Mode::Windowed || aspectChoice_!=0) return;
    RECT border{};
    AdjustWindowRectExForDpi(&border,DWORD(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,
                             DWORD(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)),GetDpiForWindow(hwnd));
    const LONG extraW=border.right-border.left,extraH=border.bottom-border.top;
    const LONG clientW=outer.right-outer.left-extraW,clientH=outer.bottom-outer.top-extraH;
    const LONG fromWidth=std::max(40L,(clientW+8)/16);
    const LONG fromHeight=std::max(40L,(clientH+4)/9);
    const bool left=edge==WMSZ_LEFT || edge==WMSZ_TOPLEFT || edge==WMSZ_BOTTOMLEFT;
    const bool top=edge==WMSZ_TOP || edge==WMSZ_TOPLEFT || edge==WMSZ_TOPRIGHT;
    LONG units=fromWidth;
    if (edge==WMSZ_TOP || edge==WMSZ_BOTTOM ||
        ((edge!=WMSZ_LEFT && edge!=WMSZ_RIGHT) &&
         std::abs(16*fromHeight-clientW)<std::abs(9*fromWidth-clientH))) units=fromHeight;
    const LONG width=units*16+extraW,height=units*9+extraH;
    if (left) outer.left=outer.right-width;
    else outer.right=outer.left+width;
    if (top) outer.top=outer.bottom-height;
    else outer.bottom=outer.top+height;
}
