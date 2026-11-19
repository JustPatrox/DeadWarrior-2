#pragma once
#include "campaign_menu.hpp"

namespace sw2 {
class PauseMenu {
public:
    enum class Action { None, Resume, Guide, Restart, MainMenu, Sound, Music };
    explicit PauseMenu(const MenuAssets& art):art_(art) {}
    bool isOpen() const { return open_; }
    void setOpen(bool);
    void point(std::optional<Point> p) { pointer_=p; }
    void press() { pressed_=hit(); }
    void cancelPress() { pressed_=-1; }
    Action release();
    Action key(int key);
    std::vector<Draw> drawList(bool sound,bool music) const;
private:
    Rect bounds(int) const;
    int hit() const;
    Action activate(int);
    const MenuAssets& art_;
    bool open_{},help_{};
    Action confirmation_{Action::None};
    int focused_{},pressed_{-1};
    std::optional<Point> pointer_;
};
}
