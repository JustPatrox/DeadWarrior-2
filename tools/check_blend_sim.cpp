// Prepared Adobe BitmapData.draw samples; independent of native battle balance.
#include "raster_compositor.hpp"
#include <array>
#include <iostream>
#include <cmath>
using namespace sw2;
int main() {
    struct Sample {Draw::Blend mode;unsigned source,destination,result;};
    const Sample samples[]={
    {Draw::Blend::Normal,0xffc82496u,0xff143278u,0xffc82496u},
    {Draw::Blend::Normal,0xffc82496u,0x806428c8u,0xffc82496u},
    {Draw::Blend::Normal,0xffc82496u,0x0u,0xffc82496u},
    {Draw::Blend::Normal,0x804080c0u,0xff143278u,0xff2a599cu},
    {Draw::Blend::Normal,0x804080c0u,0x806428c8u,0xc04c62c2u},
    {Draw::Blend::Normal,0x804080c0u,0x0u,0x80407fbfu},
    {Draw::Blend::Normal,0x400000ffu,0xff143278u,0xff0f259au},
    {Draw::Blend::Normal,0x400000ffu,0x806428c8u,0xa03b18deu},
    {Draw::Blend::Normal,0x400000ffu,0x0u,0x400000ffu},
    {Draw::Blend::Normal,0xabcdefu,0xff143278u,0xff143278u},
    {Draw::Blend::Normal,0xabcdefu,0x806428c8u,0x806428c7u},
    {Draw::Blend::Normal,0xabcdefu,0x0u,0x0u},
    {Draw::Blend::Add,0xffc82496u,0xff143278u,0xffdc56ffu},
    {Draw::Blend::Add,0xffc82496u,0x806428c8u,0xfffa38fau},
    {Draw::Blend::Add,0xffc82496u,0x0u,0xffc82496u},
    {Draw::Blend::Add,0x804080c0u,0xff143278u,0xff3472d8u},
    {Draw::Blend::Add,0x804080c0u,0x806428c8u,0xff5254c4u},
    {Draw::Blend::Add,0x804080c0u,0x0u,0x80407fbfu},
    {Draw::Blend::Add,0x400000ffu,0xff143278u,0xff1432b8u},
    {Draw::Blend::Add,0x400000ffu,0x806428c8u,0xc0421bdau},
    {Draw::Blend::Add,0x400000ffu,0x0u,0x400000ffu},
    {Draw::Blend::Add,0xabcdefu,0xff143278u,0xff143278u},
    {Draw::Blend::Add,0xabcdefu,0x806428c8u,0x806428c7u},
    {Draw::Blend::Add,0xabcdefu,0x0u,0x0u},
    {Draw::Blend::Overlay,0xffc82496u,0xff143278u,0xff1f0e8du},
    {Draw::Blend::Overlay,0xffc82496u,0x806428c8u,0xffb217b3u},
    {Draw::Blend::Overlay,0xffc82496u,0x0u,0xffc82496u},
    {Draw::Blend::Overlay,0x804080c0u,0xff143278u,0xff0e3296u},
    {Draw::Blend::Overlay,0x804080c0u,0x806428c8u,0xbf4744cdu},
    {Draw::Blend::Overlay,0x804080c0u,0x0u,0x80407fbfu},
    {Draw::Blend::Overlay,0x400000ffu,0xff143278u,0xff0e2596u},
    {Draw::Blend::Overlay,0x400000ffu,0x806428c8u,0x9f3b16ddu},
    {Draw::Blend::Overlay,0x400000ffu,0x0u,0x400000ffu},
    {Draw::Blend::Overlay,0xabcdefu,0xff143278u,0xff143278u},
    {Draw::Blend::Overlay,0xabcdefu,0x806428c8u,0x806428c5u},
    {Draw::Blend::Overlay,0xabcdefu,0x0u,0x0u},
    {Draw::Blend::Screen,0xffc82496u,0xff143278u,0xffcc4ec7u},
    {Draw::Blend::Screen,0xffc82496u,0x806428c8u,0xffd235bfu},
    {Draw::Blend::Screen,0xffc82496u,0x0u,0xffc82496u},
    {Draw::Blend::Screen,0x804080c0u,0xff143278u,0xff3165aau},
    {Draw::Blend::Screen,0x804080c0u,0x806428c8u,0xbf6468d2u},
    {Draw::Blend::Screen,0x804080c0u,0x0u,0x80407fbfu},
    {Draw::Blend::Screen,0x400000ffu,0xff143278u,0xff143299u},
    {Draw::Blend::Screen,0x400000ffu,0x806428c8u,0x9f5020ddu},
    {Draw::Blend::Screen,0x400000ffu,0x0u,0x400000ffu},
    {Draw::Blend::Screen,0xabcdefu,0xff143278u,0xff143278u},
    {Draw::Blend::Screen,0xabcdefu,0x806428c8u,0x806428c7u},
    {Draw::Blend::Screen,0xabcdefu,0x0u,0x0u}
    };
    auto rgba=[](unsigned x){return std::array<std::uint8_t,4>{std::uint8_t(x>>16),std::uint8_t(x>>8),std::uint8_t(x),std::uint8_t(x>>24)};};
    for(const auto& sample:samples) {
        auto d=rgba(sample.destination),s=rgba(sample.source),expected=rgba(sample.result);
        blendPixel(d.data(),s.data(),sample.mode);
        for(int c=0;c<4;++c) if(std::abs(int(d[c])-expected[c])>3) {std::cerr<<"Original Flash blend mismatch\n";return 1;}
    }
    std::cout<<"48 original Flash alpha/blend samples passed\n";return 0;
}
