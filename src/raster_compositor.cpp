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
void blendPixel(std::uint8_t* destination, const std::uint8_t* source, Draw::Blend mode) {
    stick::blend_rgba(destination, source, static_cast<stick::Blend>(mode));
}
namespace {
void sample(const Raster& r,double x,double y,std::uint8_t* out,Rect clip) {
    const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;
    double values[4]{};
    for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
        if(ix+dx<clip.x || iy+dy<clip.y || ix+dx>=clip.x+clip.w || iy+dy>=clip.y+clip.h) continue;
        const auto* p=r.rgba.data()+4*((iy+dy)*r.width+ix+dx);
        const double weight=(dx ? fx : 1-fx)*(dy ? fy : 1-fy),alpha=weight*p[3]/255.;
        for(int c=0;c<3;++c) values[c]+=alpha*p[c];values[3]+=alpha;
    }
    for(int c=0;c<3;++c) out[c]=values[3] ? std::uint8_t(std::clamp(std::lround(values[c]/values[3]),0l,255l)) : 0;
    out[3]=std::uint8_t(std::clamp(std::lround(255*values[3]),0l,255l));
}
}
std::shared_ptr<const Raster> RasterCompositor::image(const Draw& group) {
    if(group.pixels)return group.pixels;
    if(!group.children) throw std::runtime_error("Missing composite draw list");
    std::size_t hash=0;
    auto mix=[&](std::size_t h){hash^=h+0x9e3779b9+(hash<<6)+(hash>>2);};
    mix(std::hash<double>{}(group.target.w));mix(std::hash<double>{}(group.target.h));
    for(const auto& d:*group.children) {
        const auto& texture=assets_.texture(d.symbol,d.frame);
        mix(std::filesystem::hash_value(texture.file));
        const auto region=textureRegion(texture,d.source);
        for(double value:{region.x,region.y,region.w,region.h})mix(std::hash<double>{}(value));
        for(double v:{d.target.x,d.target.y,d.target.w,d.target.h,d.alpha}) mix(std::hash<double>{}(v));
        mix(std::size_t(d.blend));
    }
    if(last_ && signature_==hash) return last_;
    auto result=std::make_shared<Raster>();result->width=int(group.target.w);result->height=int(group.target.h);
    if(result->width<=0 || result->height<=0 || result->width>4096 || result->height>4096)
        throw std::runtime_error("Invalid original castle composite dimensions");
    result->rgba.resize(std::size_t(result->width)*result->height*4);++serial_;
    bool emptyDestination=true;
    for(const auto& d:*group.children) {
        const auto& texture=assets_.texture(d.symbol,d.frame);
        const auto& file=texture.file;
        auto found=images_.find(file);
        if(found==images_.end()) {
            auto image=decode_(file);bytes_+=image.rgba.size();Entry entry;entry.image=std::move(image);entry.used=serial_;
            found=images_.emplace(file,std::move(entry)).first;
        }
        auto& entry=found->second;entry.used=serial_;const auto& src=entry.image;
        const auto uv=textureRegion(texture,d.source);
        const Rect region{uv.x*src.width,uv.y*src.height,uv.w*src.width,uv.h*src.height};
        const int originX=int(std::floor(d.target.x)),originY=int(std::floor(d.target.y));
        const double fx=d.target.x-originX,fy=d.target.y-originY;
        // A sheet contains several layers and poses. One scratch sample per
        // physical PNG made those layers overwrite one another's cached work.
        const SampleKey key{file,{fx,fy,d.target.w,d.target.h,region.x,region.y,region.w,region.h}};
        auto cached=samples_.find(key);
        if(cached==samples_.end()) {
            Sample sampled;auto& cache=sampled.image;
            cache.width=int(std::ceil(fx+d.target.w));cache.height=int(std::ceil(fy+d.target.h));
            cache.rgba.resize(size_t(cache.width)*cache.height*4);
            for(int y=0;y<cache.height;++y) for(int x=0;x<cache.width;++x)
                sample(src,region.x+(x+.5-fx)*region.w/d.target.w-.5,region.y+(y+.5-fy)*region.h/d.target.h-.5,
                       cache.rgba.data()+4*(size_t(y)*cache.width+x),region);
            // Keep the original sampled RGBA unchanged. In particular, a very
            // small nonzero weighted alpha can round to zero while RGB stays
            // nonzero. blendPixel ignores that pixel, so never copy it.
            sampled.rows.resize(size_t(cache.height)+1);
            for(int y=0;y<cache.height;++y) {
                sampled.rows[size_t(y)]=sampled.spans.size();
                const auto* row=cache.rgba.data()+4*size_t(y)*cache.width;
                int x=0;
                while(x<cache.width) {
                    if(!row[4*size_t(x)+3]) {++x;continue;}
                    const int begin=x;const bool opaque=row[4*size_t(x)+3]==255;
                    do {++x;}while(x<cache.width && row[4*size_t(x)+3] && (row[4*size_t(x)+3]==255)==opaque);
                    sampled.spans.push_back({begin,x,opaque});
                }
            }
            sampled.rows[size_t(cache.height)]=sampled.spans.size();
            bytes_+=sampled.storageBytes();
            ++sampleBuilds_;cached=samples_.emplace(key,std::move(sampled)).first;
        }
        auto& cachedSample=cached->second;cachedSample.used=serial_;const auto& sampled=cachedSample.image;
        const int left=std::max(0,originX),top=std::max(0,originY);
        const int right=std::min(result->width,int(std::ceil(d.target.x+d.target.w)));
        const int bottom=std::min(result->height,int(std::ceil(d.target.y+d.target.h)));
        const double opacity=d.alpha;const auto mode=d.blend;
        for(int y=top;y<bottom;++y) {
            const auto row=size_t(y-originY);
            const auto* sourceRow=sampled.rgba.data()+4*row*sampled.width;
            auto* destinationRow=result->rgba.data()+4*size_t(y)*result->width;
            for(auto index=cachedSample.rows[row];index<cachedSample.rows[row+1];++index) {
                const auto& span=cachedSample.spans[index];
                const int begin=std::max(span.begin,left-originX),end=std::min(span.end,right-originX);
                if(begin>=end)continue;
                const auto* source=sourceRow+4*size_t(begin);
                auto* destination=destinationRow+4*size_t(originX+begin);
                if(opacity==1 && (emptyDestination || (mode==Draw::Blend::Normal && span.opaque))) {
                    std::copy_n(source,4*size_t(end-begin),destination);
                } else if(opacity==1) {
                    for(int x=begin;x<end;++x,source+=4,destination+=4)blendPixel(destination,source,mode);
                } else {
                    for(int x=begin;x<end;++x,source+=4,destination+=4) {
                        std::uint8_t rgba[4];std::copy_n(source,4,rgba);
                        rgba[3]=std::uint8_t(std::clamp(std::lround(rgba[3]*opacity),0l,255l));
                        blendPixel(destination,rgba,mode);
                    }
                }
            }
        }
        emptyDestination=false;
    }
    constexpr std::size_t budget=128*1024*1024;
    if(bytes_>budget) for(auto it=samples_.begin();it!=samples_.end() && bytes_>budget;) {
        if(it->second.used!=serial_) {bytes_-=it->second.storageBytes();it=samples_.erase(it);}else ++it;
    }
    if(bytes_>budget) for(auto it=images_.begin();it!=images_.end() && bytes_>budget;) {
        if(it->second.used!=serial_) {bytes_-=it->second.image.rgba.size();it=images_.erase(it);}else ++it;
    }
    signature_=hash;last_=result;return last_;
}
}
