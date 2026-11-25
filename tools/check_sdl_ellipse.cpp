#include "sdl_ellipse.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct PreviousGeometry {std::vector<SDL_Vertex> vertices;std::vector<int> indices;};
PreviousGeometry previous(SDL_FRect dst,SDL_FColor shade) {
    PreviousGeometry shape;
    auto& vertices=shape.vertices;auto& indices=shape.indices;
    vertices.push_back({{dst.x+dst.w/2,dst.y+dst.h/2},shade,{0,0}});
    for(int i=0;i<=40;++i) {
        const double a=i*2*std::numbers::pi/40;
        vertices.push_back({{dst.x+dst.w/2+dst.w/2*float(std::cos(a)),dst.y+dst.h/2+dst.h/2*float(std::sin(a))},shade,{0,0}});
    }
    for(int i=1;i+1<int(vertices.size());++i) {indices.push_back(0);indices.push_back(i);indices.push_back(i+1);}
    return shape;
}
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
struct Case {SDL_FRect target;SDL_FColor shade;};
std::vector<Case> cases() {
    std::vector<Case> result;
    std::uint32_t seed=31;
    auto number=[&] {seed=1664525*seed+1013904223;return seed;};
    for(int i=0;i<16000;++i) {
        SDL_FRect target{(int(number()%240000)-60000)/64.f,(int(number()%150000)-30000)/64.f,
                        (number()%60000+1)/64.f,(number()%30000+1)/64.f};
        SDL_FColor shade{(number()%256)/255.f,(number()%256)/255.f,(number()%256)/255.f,(number()%256)/255.f};
        result.push_back({target,shade});
    }
    for(float size:{.00001f,.25f,.5f,1.f,12.f,80.f,16384.f})
        result.push_back({{-1.125f,-.5f,size,size/7},{0,1,0,.3f}});
    return result;
}
std::uint64_t geometry(const std::vector<Case>& inputs) {
    std::uint64_t total=0;
    for(const auto& sample:inputs) {
        const auto old=previous(sample.target,sample.shade);
        const auto next=sw2::detail::ellipseVertices(sample.target,sample.shade);
        const auto& indices=sw2::detail::ellipseIndices();
        require(old.vertices.size()==next.size()&&old.indices.size()==indices.size(),"Ellipse topology changed");
        require(std::memcmp(old.vertices.data(),next.data(),next.size()*sizeof(SDL_Vertex))==0,"Ellipse vertices changed");
        require(std::memcmp(old.indices.data(),indices.data(),indices.size()*sizeof(int))==0,"Ellipse triangle order changed");
        total+=next.size();
    }
    return total;
}
std::uint64_t render(const std::vector<Case>& inputs,int width,int height,const std::filesystem::path& out) {
    SDL_Surface* before=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* after=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32);
    require(before&&after,SDL_GetError());
    for(auto pair:{std::pair{before,false},std::pair{after,true}}) {
        auto* renderer=SDL_CreateSoftwareRenderer(pair.first);require(renderer,SDL_GetError());
        require(SDL_SetRenderDrawColor(renderer,9,17,33,255)&&SDL_RenderClear(renderer)&&SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND),SDL_GetError());
        const SDL_Rect clip{13,19,width-26,height-38};require(SDL_SetRenderClipRect(renderer,&clip),SDL_GetError());
        for(size_t n=0;n<384;++n) {
            const auto& sample=inputs[n];
            auto dst=sample.target;dst.x*=width/1920.f;dst.y*=height/1080.f;dst.w*=width/1920.f;dst.h*=height/1080.f;
            if(pair.second) {
                const auto vertices=sw2::detail::ellipseVertices(dst,sample.shade);const auto& indices=sw2::detail::ellipseIndices();
                require(SDL_RenderGeometry(renderer,nullptr,vertices.data(),int(vertices.size()),indices.data(),int(indices.size())),SDL_GetError());
            } else {
                const auto shape=previous(dst,sample.shade);
                require(SDL_RenderGeometry(renderer,nullptr,shape.vertices.data(),int(shape.vertices.size()),shape.indices.data(),int(shape.indices.size())),SDL_GetError());
            }
        }
        require(SDL_RenderPresent(renderer),SDL_GetError());SDL_DestroyRenderer(renderer);
    }
    for(int y=0;y<height;++y)
        require(std::memcmp(static_cast<const std::uint8_t*>(before->pixels)+y*before->pitch,
                            static_cast<const std::uint8_t*>(after->pixels)+y*after->pitch,size_t(width)*4)==0,"Software-rendered ellipse pixels changed");
    const auto prefix=out/(std::to_string(width)+"x"+std::to_string(height));
    require(SDL_SavePNG(before,(prefix.string()+"-before.png").c_str())&&SDL_SavePNG(after,(prefix.string()+"-after.png").c_str()),SDL_GetError());
    SDL_DestroySurface(before);SDL_DestroySurface(after);
    return std::uint64_t(width)*height;
}
std::uint64_t sink{};
template<bool optimized> double measure(const std::vector<Case>& inputs,int iterations) {
    const auto start=std::chrono::steady_clock::now();std::uint64_t checksum=0;
    for(int n=0;n<iterations;++n) {
        const auto& sample=inputs[size_t(n)%inputs.size()];
        if constexpr(optimized) {
            const auto vertices=sw2::detail::ellipseVertices(sample.target,sample.shade);
            const auto point=vertices[1+n%41].position;
            checksum+=std::bit_cast<std::uint32_t>(point.x)+std::bit_cast<std::uint32_t>(point.y);
        } else {
            const auto shape=previous(sample.target,sample.shade);
            const auto point=shape.vertices[1+n%41].position;
            checksum+=std::bit_cast<std::uint32_t>(point.x)+std::bit_cast<std::uint32_t>(point.y);
        }
    }
    sink+=checksum;
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
}
int main(int argc,char** argv) {
    try {
        const auto out=std::filesystem::absolute(argc>1?argv[1]:"work/ellipse-checks");std::filesystem::create_directories(out);
        const auto inputs=cases();const auto count=geometry(inputs);
        require(SDL_Init(0),SDL_GetError());
        const auto pixels=render(inputs,1280,720,out)+render(inputs,1920,1080,out);SDL_Quit();
        std::vector<double> baseline,optimized;constexpr int iterations=200000;
        measure<false>(inputs,1000);measure<true>(inputs,1000);
        for(int round=0;round<7;++round) {
            if(round%2){optimized.push_back(measure<true>(inputs,iterations));baseline.push_back(measure<false>(inputs,iterations));}
            else {baseline.push_back(measure<false>(inputs,iterations));optimized.push_back(measure<true>(inputs,iterations));}
        }
        auto median=[](auto values){std::sort(values.begin(),values.end());return values[values.size()/2];};
        const double before=median(baseline),after=median(optimized);
        std::ofstream report(out/"checks.json");
        report<<"{\n  \"geometryCases\": "<<inputs.size()<<",\n  \"bitExactVertices\": "<<count
              <<",\n  \"bitExactIndices\": true,\n  \"softwarePixelsEqual\": "<<pixels
              <<",\n  \"resolutions\": [[1280,720],[1920,1080]],\n  \"benchmarkIterationsPerRound\": "<<iterations
              <<",\n  \"benchmarkRounds\": 7,\n  \"beforeMedianMs\": "<<before<<",\n  \"afterMedianMs\": "<<after
              <<",\n  \"checksum\": "<<sink<<",\n  \"scope\": \"geometry construction only; no game FPS claim\"\n}\n";
        std::cout<<"Bit-exact "<<inputs.size()<<" ellipse cases / "<<count<<" vertices and "<<pixels<<" rendered pixels.\n"
                 <<"Geometry construction, 200000 ellipses: "<<before<<" ms before / "<<after<<" ms after (median of 7).\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';SDL_Quit();return 1;}
}
