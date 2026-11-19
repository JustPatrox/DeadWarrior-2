#include "windows_display.hpp"

#include <algorithm>
#include <cstdlib>

using sw2::Draw;
using sw2::Rect;
using sw2::Viewport;

namespace {
constexpr Rect panel{193,120,464,512};
constexpr auto modes=std::array{"Windowed","Borderless Window","Fullscreen"};
struct Rate { int fps; const char* label; };
constexpr Rate rates[]={{30,"30"},{60,"60"},{120,"120"},{240,"240"},{0,"Unlimited"}};
constexpr int modeCount=int(modes.size()),rateCount=int(std::size(rates));
constexpr int syncChoice=modeCount+rateCount,counterChoice=syncChoice+1,choiceCount=counterChoice+1;
constexpr Rect choiceBounds(int i) {
    if (i<modeCount) return {219,220.+57*i,412,45};
    if (i==syncChoice) return {219,485,412,44};
    if (i==counterChoice) return {219,537,412,44};
    return {219.+84*(i-modeCount),428,76,40};
}
}

int DisplaySettings::frameLimit() const { return rates[rateChoice_].fps; }

void DisplaySettings::setOpen(bool open) {
    open_=open;
    focused_=int(mode_);
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
        return;
    }

    if (old==Mode::Windowed) {
        windowStyle_=GetWindowLongPtrW(hwnd,GWL_STYLE);
        GetWindowPlacement(hwnd,&windowed_);
    }
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT r=next==Mode::Fullscreen ? monitor.rcMonitor : monitor.rcWork;
    if (next==Mode::Borderless) {
        const LONG units=std::min((r.right-r.left)/16,(r.bottom-r.top)/9);
        r.left+=(r.right-r.left-16*units)/2;
        r.top+=(r.bottom-r.top-9*units)/2;
        r.right=r.left+16*units;
        r.bottom=r.top+9*units;
    }
    SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);
    SetWindowPos(hwnd,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED);
}

void DisplaySettings::apply(HWND hwnd,int choice) {
    if (choice<modeCount) setMode(hwnd,Mode(choice));
    else if (choice==syncChoice) vsync_=!vsync_;
    else if (choice==counterChoice) showFps_=!showFps_;
    else rateChoice_=choice-modeCount;
}

void DisplaySettings::key(HWND hwnd,WPARAM key) {
    switch (key) {
    case VK_DOWN:
        if (focused_==counterChoice) focused_=0;
        else if (focused_==syncChoice) focused_=counterChoice;
        else if (focused_>=modeCount) focused_=syncChoice;
        else if (focused_==modeCount-1) focused_=modeCount+rateChoice_;
        else ++focused_;
        break;
    case VK_UP:
        if (focused_==0) focused_=counterChoice;
        else if (focused_==counterChoice) focused_=syncChoice;
        else if (focused_==syncChoice) focused_=modeCount+rateChoice_;
        else focused_=std::min(focused_-1,modeCount-1);
        break;
    case VK_LEFT: case VK_RIGHT:
        if (focused_>=modeCount && focused_<syncChoice) {
            const int direction=key==VK_LEFT ? -1 : 1;
            focused_=modeCount+(focused_-modeCount+direction+rateCount)%rateCount;
        }
        break;
    case VK_TAB:
        focused_=(focused_+((GetKeyState(VK_SHIFT)&0x8000) ? -1 : 1)+choiceCount)%choiceCount;
        break;
    case VK_HOME: focused_=0; break;
    case VK_END: focused_=choiceCount-1; break;
    case VK_RETURN: case VK_SPACE: apply(hwnd,focused_); break;
    }
}

void DisplaySettings::pointer(HWND hwnd,UINT message,std::optional<sw2::Point> point) {
    if (message!=WM_LBUTTONDOWN && message!=WM_LBUTTONUP && message!=WM_CAPTURECHANGED) return;
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
        if (confirm) apply(hwnd,hit);
    } else if (message==WM_CAPTURECHANGED) {
        pressed_=-1;
    }
}

void DisplaySettings::append(std::vector<Draw>& draws) const {
    if (!open_) return;
    using Kind=Draw::Kind;
    draws.push_back(Draw::shape(Kind::Box,{Viewport::left,0,Viewport::width,Viewport::height},0,0.72));
    draws.push_back(Draw::shape(Kind::Box,panel,0x171b22));
    draws.push_back(Draw::shape(Kind::Outline,panel,0x68717e));
    draws.push_back(Draw::label({219,149,412,35},"Display",25,0xf5f6f9));
    for (int i=0;i<modeCount;++i) {
        const auto r=choiceBounds(i);
        const bool selected=i==int(mode_);
        draws.push_back(Draw::shape(Kind::Box,r,focused_==i ? 0x394350 : 0x222933));
        if (focused_==i) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x+14,r.y+10,300,26},modes[i],18,0xf5f6f9));
        if (selected) draws.push_back(Draw::label({r.x+r.w-80,r.y+14,70,23},"Active",14,0xe3b566));
    }
    draws.push_back(Draw::label({219,396,412,23},"FPS limit",16,0xf5f6f9));
    for (int i=0;i<rateCount;++i) {
        const int choice=modeCount+i;
        const auto r=choiceBounds(choice);
        const bool selected=rateChoice_==i;
        draws.push_back(Draw::shape(Kind::Box,r,selected ? 0x4a3b29 : 0x222933));
        if (focused_==choice) draws.push_back(Draw::shape(Kind::Outline,r,0xe3b566));
        draws.push_back(Draw::label({r.x,r.y+10,r.w,20},rates[i].label,14,selected ? 0xffd992 : 0xf5f6f9,true));
    }
    const auto sync=choiceBounds(syncChoice);
    draws.push_back(Draw::shape(Kind::Box,sync,focused_==syncChoice ? 0x394350 : 0x222933));
    if (focused_==syncChoice) draws.push_back(Draw::shape(Kind::Outline,sync,0xe3b566));
    draws.push_back(Draw::label({233,496,270,24},"V-Sync",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,492,66,30},vsync_ ? 0x4a3b29 : 0x171b22));
    draws.push_back(Draw::label({555,498,66,20},vsync_ ? "On" : "Off",14,vsync_ ? 0xffd992 : 0xbfc6d0,true));
    const auto counter=choiceBounds(counterChoice);
    draws.push_back(Draw::shape(Kind::Box,counter,focused_==counterChoice ? 0x394350 : 0x222933));
    if (focused_==counterChoice) draws.push_back(Draw::shape(Kind::Outline,counter,0xe3b566));
    draws.push_back(Draw::label({233,548,270,24},"Show FPS",18,0xf5f6f9));
    draws.push_back(Draw::shape(Kind::Box,{555,544,66,30},showFps_ ? 0x4a3b29 : 0x171b22));
    draws.push_back(Draw::label({555,550,66,20},showFps_ ? "On" : "Off",14,showFps_ ? 0xffd992 : 0xbfc6d0,true));
    draws.push_back(Draw::label({219,601,412,22},"16:9  |  Insert or Esc to close",14,0xbfc6d0));
}

void DisplaySettings::resize(HWND hwnd,WPARAM edge,RECT& outer) const {
    if (mode_!=Mode::Windowed) return;
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
