#include "pause_menu.hpp"
#include <array>

namespace sw2 {
namespace {
constexpr std::array<int,8> symbols{9915,9919,9958,9957,9928,9928,9939,9946};
constexpr std::array<Point,8> origins{{{429,386.1},{428.25,416.6},{429,446.6},{429,477.1},
    {348,548.5},{502,548.5},{383,492},{467,492}}};
}
void PauseMenu::setOpen(bool open) {
    open_=open; help_=false; confirmation_=Action::None;
    pointer_.reset(); focused_=0; pressed_=-1;
}
Rect PauseMenu::bounds(int index) const {
    // The original text buttons have an empty up state over the static label.
    // Their hover state supplies the visible and interactive bounds.
    auto box=art_.texture(symbols[index],index<4 ? 2 : 1).bounds;
    const auto at=help_ && index==0 ? Point{429,510} : origins[index];
    if (index==5) box.x=-box.x-box.w;
    box.x+=at.x; box.y+=at.y;
    return box;
}
int PauseMenu::hit() const {
    if (!pointer_) return -1;
    const int start=confirmation_!=Action::None ? 6 : 0;
    const int end=confirmation_!=Action::None ? 8 : 6;
    for (int i=start;i<end;++i) {
        if (help_ && i>0 && i<4) continue;
        if (bounds(i).contains(*pointer_)) return i;
    }
    return -1;
}
PauseMenu::Action PauseMenu::activate(int index) {
    if (index<0) return Action::None;
    if (index==6) {const auto action=confirmation_; confirmation_=Action::None; return action;}
    if (index==7) {confirmation_=Action::None; focused_=0; return Action::None;}
    if (index==1) {help_=true; focused_=0; return Action::Guide;}
    if (index==2 || index==3) {
        confirmation_=index==2 ? Action::Restart : Action::MainMenu;
        focused_=7; return Action::None;
    }
    return index==0 ? Action::Resume : index==4 ? Action::Sound : Action::Music;
}
PauseMenu::Action PauseMenu::release() {
    const int chosen=pressed_==hit() ? pressed_ : -1;
    pressed_=-1; return activate(chosen);
}
PauseMenu::Action PauseMenu::key(int key) {
    pointer_.reset();
    if (key==27) {
        if (confirmation_!=Action::None || help_) {confirmation_=Action::None;help_=false;focused_=0;return Action::None;}
        return Action::Resume;
    }
    const int start=confirmation_!=Action::None ? 6 : 0;
    const int count=confirmation_!=Action::None ? 2 : 6;
    if (key==9 || key==39 || key==40) focused_=start+(focused_-start+1)%count;
    else if (key==37 || key==38) focused_=start+(focused_-start+count-1)%count;
    else if (key==13 || key==32) return activate(focused_);
    if (help_ && focused_>0 && focused_<4) focused_=key==37 || key==38 ? 0 : 4;
    return Action::None;
}
std::vector<Draw> PauseMenu::drawList(bool sound,bool music) const {
    std::vector<Draw> out;
    if (!open_) return out;
    out.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left,0,Viewport::width,Viewport::height},0,.7));
    out.push_back({9964,1,art_.texture(9964).bounds});
    const int start=confirmation_!=Action::None ? 6 : 0;
    const int end=confirmation_!=Action::None ? 8 : 6;
    if (confirmation_!=Action::None) {
        out.push_back(Draw::shape(Draw::Kind::Box,{275,352,300,183},0,.96));
        auto box=art_.texture(9963).bounds;box.x+=290;box.y+=362;
        out.push_back({9963,1,box});
    }
    if (help_) {
        out.push_back(Draw::shape(Draw::Kind::Box,{218,352,414,183},0,.96));
        out.push_back(Draw::label({230,362,390,132},
            "Left click / drag: select units\nRight click: issue an order\nMinimap: navigate or issue orders\nQ / W / E: selected unit abilities\nEsc / P: pause  |  Insert: PC settings\nF11: fullscreen",16,0xffe4a0,true));
    }
    for (int i=start;i<end;++i) {
        if (help_ && i>0 && i<4) continue;
        const bool active=(pointer_ ? hit() : focused_)==i;
        const int frame=i==4 ? (sound ? 1 : 3) : i==5 ? (music ? 1 : 3) : active ? pressed_==i ? 3 : 2 : help_ ? 2 : 1;
        auto box=art_.texture(symbols[i],frame).bounds;
        const auto origin=help_ && i==0 ? Point{429,510} : origins[i];
        if (i==5) box.x=-box.x-box.w;
        box.x+=origin.x;box.y+=origin.y;
        Draw draw{symbols[i],frame,box};draw.mirrored=i==5;out.push_back(draw);
        if (active && i>=4 && i<6) out.push_back(Draw::shape(Draw::Kind::Outline,bounds(i),0xffcc00));
    }
    return out;
}
}
