// Prepared Adobe BitmapData.draw samples; independent of native battle balance.
#include "raster_compositor.hpp"
#include <array>
#include <iostream>
#include <cmath>
#include <chrono>
#include <fstream>
#include <stdexcept>
using namespace sw2;
namespace {
void checkDirectRaster() {
    const auto folder=std::filesystem::temp_directory_path()/("DeadWarrior2-raster-probe-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(folder))throw std::runtime_error("Cannot create raster probe fixture");
    const auto menu=folder/"menu";std::filesystem::create_directory(menu);
    std::ofstream(menu/"textures.tsv")<<"1 1 unused.png 0 0 1 1\n";
    std::ofstream(menu/"placements.tsv")<<"1 1 1 1 dummy 1 1 0 0 0 0 1 1\n";
    std::ofstream(menu/"alpha.tsv")<<"1 1 1\n";
    try {
        MenuAssets art(menu);int decoded=0;
        RasterCompositor compositor(art,[&](const std::filesystem::path&)->Raster {
            ++decoded;throw std::runtime_error("Direct Raster used asset decoding");
        });
        auto first=std::make_shared<Raster>(Raster{2,1,{9,17,33,255,101,202,13,255}});
        Draw draw;draw.target={123,45,200,100};draw.pixels=first;
        const auto result=compositor.image(draw);
        if(result!=first || result->rgba!=first->rgba || result->width!=2 || result->height!=1)
            throw std::runtime_error("Direct Raster was copied or changed");
        draw.target={-50,700,400,200};
        if(compositor.image(draw)!=first)throw std::runtime_error("Direct Raster changed with target geometry");
        auto second=std::make_shared<Raster>(*first);second->rgba[0]=247;draw.pixels=second;
        if(compositor.image(draw)!=second || first->rgba[0]!=9 || decoded)
            throw std::runtime_error("Direct Raster frame replacement or shared ownership failed");
    } catch(...) {
        // Remove only the four exact paths created by this fixture.
        for(const char* name:{"textures.tsv","placements.tsv","alpha.tsv"})std::filesystem::remove(menu/name);
        std::filesystem::remove(menu);std::filesystem::remove(folder);throw;
    }
    for(const char* name:{"textures.tsv","placements.tsv","alpha.tsv"})std::filesystem::remove(menu/name);
    std::filesystem::remove(menu);std::filesystem::remove(folder);
}
}
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
    try {checkDirectRaster();}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"48 original Flash alpha/blend samples and direct Raster ownership/frame replacement passed\n";return 0;
}
