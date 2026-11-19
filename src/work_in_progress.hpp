#pragma once
#include "campaign_menu.hpp"

namespace sw2 {
class WorkInProgress {
public:
    explicit WorkInProgress(const MenuAssets& art):art_(art) {}
    enum class Choice {None,RestartDelete,RestartKeep,Quit};
    void point(std::optional<Point> p) {pointer_=p;}
    void press();
    Choice release();
    void focus(int direction);
    Choice selected() const;
    std::vector<Draw> drawList() const;
private:
    const MenuAssets& art_;
    std::optional<Point> pointer_;
    int pressed_{-1},focused_{-1};
    int hit() const;
};
}
