#include "raster_compositor.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sw2 {
Draw Draw::group(std::vector<Draw> list,Point origin) {
    // Some authored particle frames park objects millions of units off stage.
    // Cull them before allocating a surface, as the ordinary renderers do.
    std::erase_if(list,[](const Draw& d){return d.target.x>16384 || d.target.y>16384 || d.target.x+d.target.w< -16384 || d.target.y+d.target.h< -16384;});
    double left=0,top=0,right=0,bottom=0;
    for(const auto& d:list) {
        left=std::min(left,d.target.x);top=std::min(top,d.target.y);
        right=std::max(right,d.target.x+d.target.w);bottom=std::max(bottom,d.target.y+d.target.h);
    }
    left=std::floor(left);top=std::floor(top);right=std::ceil(right);bottom=std::ceil(bottom);
    for(auto& d:list) {d.target.x-=left;d.target.y-=top;}
    Draw result;result.target={origin.x+left,origin.y+top,right-left,bottom-top};
    result.children=std::make_shared<const std::vector<Draw>>(std::move(list));return result;
}
void blendPixel(std::uint8_t* d,const std::uint8_t* s,Draw::Blend mode) {
    if(!s[3]) return;
    if(!d[3]) {std::copy_n(s,4,d);return;}
    if(mode==Draw::Blend::Normal && s[3]==255) {std::copy_n(s,4,d);return;}
    if(d[3]==255) {
        const int alpha=s[3];
        for(int c=0;c<3;++c) {
            const int cs=s[c],cb=d[c];int value=cs;
            if(mode==Draw::Blend::Add) {d[c]=std::uint8_t(std::min(255,cb+(cs*alpha+127)/255));continue;}
            if(mode==Draw::Blend::Overlay) value=cb<=127 ? (2*cs*cb+127)/255 : 255-(2*(255-cs)*(255-cb)+127)/255;
            else if(mode==Draw::Blend::Screen) value=255-((255-cs)*(255-cb)+127)/255;
            d[c]=std::uint8_t((value*alpha+cb*(255-alpha)+127)/255);
        }
        return;
    }
    const double a=s[3]/255.,b=d[3]/255.;
    const double out=mode==Draw::Blend::Add ? std::min(1.,a+b) : a+b*(1-a);
    for(int c=0;c<3;++c) {
        const double cs=s[c]/255.,cb=d[c]/255.;double premult;
        if(mode==Draw::Blend::Add) premult=std::min(1.,cs*a+cb*b);
        else {
            double blended=cs;
            if(mode==Draw::Blend::Overlay) blended=cb<=.5 ? 2*cs*cb : 1-2*(1-cs)*(1-cb);
            else if(mode==Draw::Blend::Screen) blended=1-(1-cs)*(1-cb);
            premult=a*(1-b)*cs+a*b*blended+(1-a)*b*cb;
        }
        d[c]=std::uint8_t(std::clamp(std::lround(255*premult/out),0l,255l));
    }
    d[3]=std::uint8_t(std::clamp(std::lround(out*255),0l,255l));
}
namespace {
void sample(const Raster& r,double x,double y,std::uint8_t* out) {
    const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;
    double values[4]{};
    for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
        if(ix+dx<0 || iy+dy<0 || ix+dx>=r.width || iy+dy>=r.height) continue;
        const auto* p=r.rgba.data()+4*((iy+dy)*r.width+ix+dx);
        const double weight=(dx ? fx : 1-fx)*(dy ? fy : 1-fy),alpha=weight*p[3]/255.;
        for(int c=0;c<3;++c) values[c]+=alpha*p[c];values[3]+=alpha;
    }
    for(int c=0;c<3;++c) out[c]=values[3] ? std::uint8_t(std::clamp(std::lround(values[c]/values[3]),0l,255l)) : 0;
    out[3]=std::uint8_t(std::clamp(std::lround(255*values[3]),0l,255l));
}
}
std::shared_ptr<const Raster> RasterCompositor::image(const Draw& group) {
    if(!group.children) throw std::runtime_error("Missing composite draw list");
    std::size_t hash=0;
    auto mix=[&](std::size_t h){hash^=h+0x9e3779b9+(hash<<6)+(hash>>2);};
    mix(std::hash<double>{}(group.target.w));mix(std::hash<double>{}(group.target.h));
    for(const auto& d:*group.children) {
        mix(std::filesystem::hash_value(assets_.texture(d.symbol,d.frame).file));
        for(double v:{d.target.x,d.target.y,d.target.w,d.target.h,d.alpha}) mix(std::hash<double>{}(v));
        mix(std::size_t(d.blend));
    }
    if(last_ && signature_==hash) return last_;
    auto result=std::make_shared<Raster>();result->width=int(group.target.w);result->height=int(group.target.h);
    if(result->width<=0 || result->height<=0 || result->width>4096 || result->height>4096)
        throw std::runtime_error("Invalid original castle composite dimensions");
    result->rgba.resize(std::size_t(result->width)*result->height*4);++serial_;
    for(const auto& d:*group.children) {
        const auto& file=assets_.texture(d.symbol,d.frame).file;
        auto found=images_.find(file);
        if(found==images_.end()) {
            auto image=decode_(file);bytes_+=image.rgba.size();Entry entry;entry.image=std::move(image);entry.used=serial_;
            found=images_.emplace(file,std::move(entry)).first;
        }
        auto& entry=found->second;entry.used=serial_;const auto& src=entry.image;const Raster* raster=&src;
        const double fx=d.target.x-std::floor(d.target.x),fy=d.target.y-std::floor(d.target.y);
        if(entry.source!=raster || entry.x!=fx || entry.y!=fy || entry.w!=d.target.w || entry.h!=d.target.h) {
            entry.source=raster;
            bytes_-=entry.sampled.rgba.size();entry.x=fx;entry.y=fy;entry.w=d.target.w;entry.h=d.target.h;
            auto& cache=entry.sampled;cache.width=int(std::ceil(fx+d.target.w));cache.height=int(std::ceil(fy+d.target.h));
            cache.rgba.resize(size_t(cache.width)*cache.height*4);
            for(int y=0;y<cache.height;++y) for(int x=0;x<cache.width;++x)
                sample(src,(x+.5-fx)*src.width/d.target.w-.5,(y+.5-fy)*src.height/d.target.h-.5,cache.rgba.data()+4*(size_t(y)*cache.width+x));
            bytes_+=cache.rgba.size();
        }
        const int left=std::max(0,int(std::floor(d.target.x))),top=std::max(0,int(std::floor(d.target.y)));
        const int right=std::min(result->width,int(std::ceil(d.target.x+d.target.w)));
        const int bottom=std::min(result->height,int(std::ceil(d.target.y+d.target.h)));
        for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) {
            const auto* source=entry.sampled.rgba.data()+4*(size_t(y-int(std::floor(d.target.y)))*entry.sampled.width+x-int(std::floor(d.target.x)));
            std::uint8_t rgba[4];if(d.alpha!=1) {std::copy_n(source,4,rgba);rgba[3]=std::uint8_t(std::clamp(std::lround(rgba[3]*d.alpha),0l,255l));source=rgba;}
            blendPixel(result->rgba.data()+4*(std::size_t(y)*result->width+x),source,d.blend);
        }
    }
    constexpr std::size_t budget=128*1024*1024;
    if(bytes_>budget) for(auto it=images_.begin();it!=images_.end() && bytes_>budget;) {
        if(it->second.used!=serial_) {bytes_-=it->second.image.rgba.size()+it->second.sampled.rgba.size();it=images_.erase(it);}else ++it;
    }
    signature_=hash;last_=result;return last_;
}
}
