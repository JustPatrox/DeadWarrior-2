#include "work_in_progress.hpp"
#include <array>

namespace sw2 {
namespace {
constexpr std::array<Rect,3> buttons{{{-45,437,440,440.*32/212},{455,437,440,440.*32/212},{245,523,360,360.*32/212}}};
constexpr std::array<const char*,3> labels{{"Restart and delete save","Restart keeping saves","Quit"}};
}
int WorkInProgress::hit() const {
    if (pointer_) for (int i=0;i<int(buttons.size());++i) if (buttons[i].contains(*pointer_)) return i;
    return -1;
}
void WorkInProgress::press() {pressed_=hit();}
WorkInProgress::Choice WorkInProgress::release() {
    const int activated=pressed_==hit() ? pressed_ : -1; pressed_=-1;
    return activated<0 ? Choice::None : Choice(activated+1);
}
void WorkInProgress::focus(int direction) {
    pointer_.reset(); focused_=focused_<0 ? (direction>0 ? 0 : 2) : (focused_+direction+3)%3;
}
WorkInProgress::Choice WorkInProgress::selected() const {return focused_<0 ? Choice::None : Choice(focused_+1);}
std::vector<Draw> WorkInProgress::drawList() const {
    std::vector<Draw> out;
    coverBackdrop(out,{976,1,art_.texture(976).bounds},{Viewport::left,0,Viewport::width,Viewport::height});
    out.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left,0,Viewport::width,Viewport::height},0,.78));
    out.push_back(Draw::label({0,205,850,115},
        "Thanks for playing! But this part is still a work in progress. Stay tuned for an update!",27,0xffcc00,true));
    out.push_back(Draw::label({200,352,450,38},"Please select one",22,0xf2f2f2,true));
    for (int i=0;i<int(buttons.size());++i) {
        const bool active=(pointer_ ? hit() : focused_)==i;
        const int frame=10001+(active ? pressed_==i ? 2 : 1 : 0);
        out.push_back({1002,frame,buttons[i]});
        auto text=buttons[i]; text.y+=14;
        out.push_back(Draw::label(text,labels[i],19,active ? 0xffffff : 0xffcc00,true));
    }
    return out;
}
}
