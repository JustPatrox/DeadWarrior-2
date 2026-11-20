#pragma once

#include "campaign_menu.hpp"

namespace sw2::pcSettings {
inline constexpr Rect panel{193,120,464,578};

inline Point offset() {
    return {Viewport::left+Viewport::width/2-(panel.x+panel.w/2),
            Viewport::height/2-(panel.y+panel.h/2)};
}

inline std::optional<Point> local(std::optional<Point> point) {
    if(point) {const auto shift=offset();point->x-=shift.x;point->y-=shift.y;}
    return point;
}

inline void center(std::vector<Draw>& draws,std::size_t first) {
    const auto shift=offset();
    for(auto i=first;i<draws.size();++i) {
        draws[i].target.x+=shift.x;draws[i].target.y+=shift.y;
    }
}
}
