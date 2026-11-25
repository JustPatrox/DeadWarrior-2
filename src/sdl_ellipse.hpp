#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <numbers>

namespace sw2::detail {
inline constexpr int ellipseSegments=40;

inline const std::array<SDL_FPoint,ellipseSegments+1>& ellipseUnitCircle() {
    static const auto points=[] {
        std::array<SDL_FPoint,ellipseSegments+1> result;
        for(int i=0;i<=ellipseSegments;++i) {
            const double angle=i*2*std::numbers::pi/ellipseSegments;
            result[i]={float(std::cos(angle)),float(std::sin(angle))};
        }
        return result;
    }();
    return points;
}

inline const std::array<int,ellipseSegments*3>& ellipseIndices() {
    static constexpr auto indices=[] {
        std::array<int,ellipseSegments*3> result{};
        for(int i=1;i<=ellipseSegments;++i) {
            result[(i-1)*3]=0;
            result[(i-1)*3+1]=i;
            result[(i-1)*3+2]=i+1;
        }
        return result;
    }();
    return indices;
}

inline std::array<SDL_Vertex,ellipseSegments+2> ellipseVertices(SDL_FRect target,SDL_FColor shade) {
    std::array<SDL_Vertex,ellipseSegments+2> vertices;
    vertices[0]={{target.x+target.w/2,target.y+target.h/2},shade,{0,0}};
    const auto& circle=ellipseUnitCircle();
    for(int i=0;i<=ellipseSegments;++i) {
        // Keep the previous arithmetic order, including the float trig values.
        vertices[i+1]={{target.x+target.w/2+target.w/2*circle[i].x,
                        target.y+target.h/2+target.h/2*circle[i].y},shade,{0,0}};
    }
    return vertices;
}
}
