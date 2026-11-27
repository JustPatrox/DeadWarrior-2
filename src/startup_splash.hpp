#pragma once
#include <algorithm>

namespace sw2 {
struct StartupSplash {bool engine{};double alpha{};};
inline constexpr double startupDuration=3.0;
inline StartupSplash startupSplash(double elapsed) {
    elapsed=std::clamp(elapsed,0.,startupDuration);
    const bool engine=elapsed>=1.5;
    const double phase=elapsed-(engine ? 1.5 : 0.);
    return {engine,std::clamp(std::min(phase/.22,(1.5-phase)/.22),0.,1.)};
}
}
