#include "sdl_platform.hpp"
#include "sdl_ellipse.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace sw2 {
namespace {
SDL_FRect rect(Rect r,const Viewport& v) {return {float(v.pixels.x+(r.x-Viewport::left)*v.scaleX),float(v.pixels.y+r.y*v.scale),float(r.w*v.scaleX),float(r.h*v.scale)};}
SDL_Color color(const Draw& d) {return {Uint8(d.color>>16),Uint8(d.color>>8),Uint8(d.color),Uint8(std::clamp(d.alpha,0.,1.)*255)};}
}
Raster loadSdlPng(const std::filesystem::path& path) {
    auto loaded=SDL_LoadPNG(path.string().c_str());if(!loaded) throw std::runtime_error(SDL_GetError());
    auto surface=SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(loaded);
    if(!surface) throw std::runtime_error(SDL_GetError());
    Raster r;r.width=surface->w;r.height=surface->h;r.rgba.resize(size_t(r.width)*r.height*4);
    for(int y=0;y<r.height;++y) std::copy_n(static_cast<Uint8*>(surface->pixels)+y*surface->pitch,r.width*4,r.rgba.data()+size_t(y)*r.width*4);
    SDL_DestroySurface(surface);return r;
}
SdlRenderer::SdlRenderer(SDL_Window* window,const MenuAssets& art,const std::filesystem::path& root):art_(art),compositor_(art,loadSdlPng),root_(root) {
    renderer_=SDL_CreateRenderer(window,nullptr);
    if(!renderer_) throw std::runtime_error(std::string("Renderer initialization failed: ")+SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    auto fontFile=root/"assets/fonts/NotoSans-Regular.ttf";
    if(!std::filesystem::exists(fontFile)) fontFile=root/"art/fonts/NotoSans-Regular.ttf";
    std::ifstream input(fontFile,std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
    if(bytes.empty()) throw std::runtime_error("Missing Noto Sans font: "+fontFile.string());
    std::vector<unsigned char> alpha(1024*1024);
    stbtt_bakedchar chars[95];
    if(stbtt_BakeFontBitmap(bytes.data(),0,48,alpha.data(),1024,1024,32,95,chars)<=0) throw std::runtime_error("Font atlas is full");
    for(int i=0;i<95;++i) glyphs_[i]={float(chars[i].x0),float(chars[i].y0),float(chars[i].x1),float(chars[i].y1),chars[i].xoff,chars[i].yoff,chars[i].xadvance,0};
    auto surface=SDL_CreateSurface(1024,1024,SDL_PIXELFORMAT_RGBA32);
    if(!surface) throw std::runtime_error(SDL_GetError());
    for(int y=0;y<1024;++y) for(int x=0;x<1024;++x) {
        auto pixel=static_cast<Uint8*>(surface->pixels)+y*surface->pitch+4*x;
        pixel[0]=pixel[1]=pixel[2]=255;pixel[3]=alpha[y*1024+x];
    }
    font_=SDL_CreateTextureFromSurface(renderer_,surface);SDL_DestroySurface(surface);
    if(!font_) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureScaleMode(font_,SDL_SCALEMODE_LINEAR);
}
SdlRenderer::~SdlRenderer() {for(auto& [_,cached]:images_) SDL_DestroyTexture(cached.image);SDL_DestroyTexture(compositeTexture_);SDL_DestroyTexture(font_);SDL_DestroyRenderer(renderer_);}
SDL_Texture* SdlRenderer::composite(const Draw& d) {
    const auto pixels=compositor_.image(d);
    if(pixels!=compositePixels_) {
        if(compositePixels_ && (pixels->width!=compositePixels_->width || pixels->height!=compositePixels_->height)) {SDL_DestroyTexture(compositeTexture_);compositeTexture_=nullptr;}
        if(!compositeTexture_) {
            compositeTexture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,pixels->width,pixels->height);
            if(!compositeTexture_) throw std::runtime_error(SDL_GetError());
            SDL_SetTextureBlendMode(compositeTexture_,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(compositeTexture_,SDL_SCALEMODE_LINEAR);
        }
        if(!SDL_UpdateTexture(compositeTexture_,nullptr,pixels->rgba.data(),pixels->width*4)) throw std::runtime_error(SDL_GetError());
        compositePixels_=pixels;
    }
    return compositeTexture_;
}
SDL_Texture* SdlRenderer::image(const std::filesystem::path& file,int tint) {
    const auto key=std::pair{file,tint};
    if(auto it=images_.find(key);it!=images_.end()) {it->second.serial=++serial_;return it->second.image;}
    SDL_Surface* surface=SDL_LoadPNG(file.string().c_str());
    if(!surface) throw std::runtime_error("Cannot decode "+file.string()+": "+SDL_GetError());
    if(tint) {
        auto rgba=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(surface);surface=rgba;
        if(!surface) throw std::runtime_error(SDL_GetError());
        for(int y=0;y<surface->h;++y) for(int x=0;x<surface->w;++x) {
            auto pixel=static_cast<Uint8*>(surface->pixels)+y*surface->pitch+4*x;
            pixel[0]=Uint8(std::clamp(int(pixel[0])+tint,0,255));
        }
    }
    const size_t used=size_t(surface->w)*surface->h*4;
    while(bytes_+used>384*1024*1024 && !images_.empty()) {
        auto oldest=std::min_element(images_.begin(),images_.end(),[](const auto& a,const auto& b){return a.second.serial<b.second.serial;});
        bytes_-=oldest->second.bytes;SDL_DestroyTexture(oldest->second.image);images_.erase(oldest);
    }
    auto texture=SDL_CreateTextureFromSurface(renderer_,surface);SDL_DestroySurface(surface);
    if(!texture) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    images_[key]={texture,used,++serial_};bytes_+=used;return texture;
}
void SdlRenderer::text(const Draw& d,const Viewport& v) {
    const float scale=float(d.fontSize/48*v.scale);
    const float scaleX=float(d.fontSize/48*v.scaleX);
    auto width=[&](const std::string& s){float w=0;for(unsigned char c:s) if(c>=32&&c<127) w+=glyphs_[c-32][6]*scaleX;return w;};
    auto target=rect(d.target,v);
    std::vector<std::string> lines;std::istringstream words(d.text);std::string line,word;
    // Authored newlines are retained; long labels wrap within their layout box.
    std::istringstream paragraphs(d.text);std::string paragraph;
    while(std::getline(paragraphs,paragraph)) {
        std::istringstream tokens(paragraph);line.clear();
        while(tokens>>word) {
            if(!line.empty() && width(line+" "+word)>target.w) {lines.push_back(line);line.clear();}
            if(!line.empty()) line+=' ';line+=word;
        }
        lines.push_back(line);
    }
    const auto c=color(d);SDL_SetTextureColorMod(font_,c.r,c.g,c.b);SDL_SetTextureAlphaMod(font_,c.a);
    float y=target.y+float(d.fontSize*v.scale*.84);
    for(const auto& row:lines) {
        float x=target.x+(d.centered ? (target.w-width(row))/2 : 0);
        for(unsigned char ch:row) {
            if(ch<32||ch>=127) continue;
            const auto& g=glyphs_[ch-32];SDL_FRect source{g[0],g[1],g[2]-g[0],g[3]-g[1]};
            SDL_FRect dst{x+g[4]*scaleX,y+g[5]*scale,source.w*scaleX,source.h*scale};
            SDL_RenderTexture(renderer_,font_,&source,&dst);x+=g[6]*scaleX;
        }
        y+=float(d.fontSize*v.scale*1.2);
    }
}
void SdlRenderer::draw(const std::vector<Draw>& draws,int w,int h) {
    SDL_SetRenderDrawColor(renderer_,0,0,0,255);SDL_RenderClear(renderer_);
    Viewport v(w,h);const auto view=rect({Viewport::left,0,Viewport::width,Viewport::height},v);
    SDL_Rect clip{int(view.x),int(view.y),int(view.w),int(view.h)};SDL_SetRenderClipRect(renderer_,&clip);
    for(const auto& d:draws) {
        auto dst=rect(d.target,v);
        if(dst.w<=0||dst.h<=0||dst.x+dst.w<0||dst.x>w||dst.y+dst.h<0||dst.y>h) continue;
        if(d.symbol || d.children || d.pixels) {
            auto texture=d.children || d.pixels ? composite(d) : image(art_.texture(d.symbol,d.frame).file,d.redOffset);
            SDL_SetTextureAlphaMod(texture,Uint8(std::clamp(d.alpha,0.,1.)*255));
            const auto region=d.children||d.pixels?d.source.value_or(Rect{0,0,1,1}):textureRegion(art_.texture(d.symbol,d.frame),d.source);
            float tw{},th{};SDL_GetTextureSize(texture,&tw,&th);
            const SDL_FRect source{float(region.x*tw),float(region.y*th),float(region.w*tw),float(region.h*th)};
            SDL_RenderTextureRotated(renderer_,texture,&source,&dst,d.rotation,nullptr,d.mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        } else {
            const auto c=color(d);SDL_SetRenderDrawColor(renderer_,c.r,c.g,c.b,c.a);
            if(d.kind==Draw::Kind::Text) text(d,v);
            else if(d.kind==Draw::Kind::Outline) SDL_RenderRect(renderer_,&dst);
            else if(d.kind==Draw::Kind::Ellipse) {
                const SDL_FColor shade{c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f};
                const auto vertices=detail::ellipseVertices(dst,shade);
                const auto& indices=detail::ellipseIndices();
                SDL_RenderGeometry(renderer_,nullptr,vertices.data(),int(vertices.size()),indices.data(),int(indices.size()));
            } else if(d.kind==Draw::Kind::Polygon) {
                std::vector<SDL_Vertex> vertices;std::vector<int> indices;
                const SDL_FColor shade{c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f};
                for(auto p:d.vertices) vertices.push_back({{dst.x+float(p.x*v.scaleX),dst.y+float(p.y*v.scale)},shade,{0,0}});
                for(int i=1;i+1<int(vertices.size());++i) {indices.push_back(0);indices.push_back(i);indices.push_back(i+1);}
                if(!indices.empty()) SDL_RenderGeometry(renderer_,nullptr,vertices.data(),int(vertices.size()),indices.data(),int(indices.size()));
            } else if(d.kind==Draw::Kind::Line) {
                for(size_t i=1;i<d.vertices.size();++i) {
                    const auto a=d.vertices[i-1],b=d.vertices[i];
                    const float ax=dst.x+float(a.x*v.scaleX),ay=dst.y+float(a.y*v.scale),bx=dst.x+float(b.x*v.scaleX),by=dst.y+float(b.y*v.scale);
                    const float dx=bx-ax,dy=by-ay,length=std::max(.001f,std::hypot(dx,dy));
                    const float nx=-dy/length*float(d.lineWidth*v.scale/2),ny=dx/length*float(d.lineWidth*v.scale/2);
                    SDL_FColor shade{c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f};
                    SDL_Vertex quad[]={{{ax+nx,ay+ny},shade,{}},{{bx+nx,by+ny},shade,{}},{{bx-nx,by-ny},shade,{}},{{ax-nx,ay-ny},shade,{}}};
                    const int indices[]={0,1,2,0,2,3};SDL_RenderGeometry(renderer_,nullptr,quad,4,indices,6);
                }
            } else SDL_RenderFillRect(renderer_,&dst);
        }
    }
    SDL_SetRenderClipRect(renderer_,nullptr);
}
void SdlRenderer::picture(const std::filesystem::path& file,Rect r,const Viewport& v,double alpha) {
    const auto dst=rect(r,v);auto* texture=image(file);
    SDL_SetTextureAlphaModFloat(texture,float(alpha));SDL_RenderTexture(renderer_,texture,nullptr,&dst);
    SDL_SetTextureAlphaModFloat(texture,1);
}
void SdlRenderer::png(const std::filesystem::path& file) {
    auto surface=SDL_RenderReadPixels(renderer_,nullptr);
    if(!surface) throw std::runtime_error(SDL_GetError());
    const bool ok=SDL_SavePNG(surface,file.string().c_str());SDL_DestroySurface(surface);
    if(!ok) throw std::runtime_error(SDL_GetError());
}
const SdlRenderer::AlphaMask& SdlRenderer::mask(const std::filesystem::path& file) {
    if(auto found=masks_.find(file);found!=masks_.end()) {
        found->second.serial=++maskSerial_;return found->second;
    }
    // A menu pointer event must not decode a whole sheet again. Keep only the
    // alpha channel for interactive masks, separately from GPU texture pages.
    auto pixels=loadSdlPng(file);
    AlphaMask value{pixels.width,pixels.height,{},++maskSerial_};
    value.alpha.resize(size_t(value.width)*value.height);
    for(size_t i=0;i<value.alpha.size();++i)value.alpha[i]=pixels.rgba[i*4+3];
    constexpr size_t budget=32*1024*1024;
    while(maskBytes_+value.alpha.size()>budget&&!masks_.empty()) {
        auto oldest=std::min_element(masks_.begin(),masks_.end(),[](const auto& a,const auto& b){return a.second.serial<b.second.serial;});
        maskBytes_-=oldest->second.alpha.size();masks_.erase(oldest);
    }
    maskBytes_+=value.alpha.size();return masks_.emplace(file,std::move(value)).first->second;
}
int SdlRenderer::hit(const CampaignMenu& menu,Point p) {
    const auto hits=menu.hits();
    for(auto it=hits.rbegin();it!=hits.rend();++it) {
        if(!it->bounds.contains(p)) continue;
        if(it->maskFrame) {
            const auto& art=art_.texture(it->symbol,it->maskFrame);const Placement* place=nullptr;
            for(const auto& [_,uses]:art_.placements) for(const auto& use:uses) if(use.symbol==it->symbol) place=&use;
            if(!place) continue;
            const double x=(p.x-it->bounds.x)/place->sx+place->bounds.x-art.bounds.x,y=(p.y-it->bounds.y)/place->sy+place->bounds.y-art.bounds.y;
            if(x<0||y<0||x>=art.bounds.w||y>=art.bounds.h) continue;
            const auto& alpha=mask(art.file);
            const auto region=textureRegion(art);
            const int px=int((region.x+x/art.bounds.w*region.w)*alpha.width),py=int((region.y+y/art.bounds.h*region.h)*alpha.height);
            if(px<0||py<0||px>=alpha.width||py>=alpha.height||!alpha.alpha[size_t(py)*alpha.width+px])continue;
        }
        return it->symbol;
    }
    return 0;
}
}
