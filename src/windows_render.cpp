#include "windows_render.hpp"

#include <objidl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

using Microsoft::WRL::ComPtr;
using sw2::Draw;
using sw2::Rect;
using sw2::Viewport;
namespace gd = Gdiplus;
sw2::Raster loadWindowsPng(const std::filesystem::path& path) {
    gd::Bitmap image(path.c_str());
    if(image.GetLastStatus()!=gd::Ok) throw std::runtime_error("Cannot decode castle texture");
    sw2::Raster result;result.width=int(image.GetWidth());result.height=int(image.GetHeight());
    result.rgba.resize(size_t(result.width)*result.height*4);
    gd::Rect bounds(0,0,result.width,result.height);gd::BitmapData data;
    if(image.LockBits(&bounds,gd::ImageLockModeRead,PixelFormat32bppARGB,&data)!=gd::Ok) throw std::runtime_error("Cannot read castle texture");
    for(int y=0;y<result.height;++y) for(int x=0;x<result.width;++x) {
        auto* p=static_cast<BYTE*>(data.Scan0)+ptrdiff_t(y)*data.Stride+4*x;
        auto* d=result.rgba.data()+4*(size_t(y)*result.width+x);d[0]=p[2];d[1]=p[1];d[2]=p[0];d[3]=p[3];
    }
    image.UnlockBits(&data);return result;
}

namespace {
D2D1_RECT_F rectangle(Rect r) {
    return D2D1::RectF(float(r.x),float(r.y),float(r.x+r.w),float(r.y+r.h));
}
using TextureKey=std::pair<std::filesystem::path,int>;
struct PngPixels {UINT width{},height{};std::vector<BYTE> bgra;};
bool decodePixels(const TextureKey& key,PngPixels& out) {
    gd::Bitmap image(key.first.c_str());
    if(image.GetLastStatus()!=gd::Ok)return false;
    out.width=image.GetWidth();out.height=image.GetHeight();
    if(!out.width || !out.height || size_t(out.width)*out.height>64*1024*1024)return false;
    const gd::Rect region(0,0,int(out.width),int(out.height));gd::BitmapData data;
    if(image.LockBits(&region,gd::ImageLockModeRead,key.second ? PixelFormat32bppARGB : PixelFormat32bppPARGB,&data)!=gd::Ok)return false;
    out.bgra.resize(size_t(out.width)*out.height*4);
    for(UINT y=0;y<out.height;++y) {
        auto* source=static_cast<const BYTE*>(data.Scan0)+ptrdiff_t(y)*data.Stride;
        auto* dest=out.bgra.data()+size_t(y)*out.width*4;
        if(!key.second)std::copy_n(source,size_t(out.width)*4,dest);
        else for(UINT x=0;x<out.width;++x,source+=4,dest+=4) {
            const int alpha=source[3];
            dest[0]=BYTE((source[0]*alpha+127)/255);dest[1]=BYTE((source[1]*alpha+127)/255);
            dest[2]=BYTE((std::min(255,source[2]+key.second)*alpha+127)/255);dest[3]=source[3];
        }
    }
    image.UnlockBits(&data);return true;
}
// Only decoding and the original color transform run here. All Direct2D
// resources stay on the window thread; a missed prediction still draws the
// exact requested frame synchronously, never skips or substitutes a pose.
class PngPrefetch {
    std::mutex mutex_;std::condition_variable changed_;bool stop_{};
    std::deque<TextureKey> jobs_;std::set<TextureKey> pending_;
    std::map<TextureKey,PngPixels> ready_;size_t bytes_{};
    std::thread worker_;
    void run() {
        for(;;) {
            TextureKey key;
            {std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return stop_ || !jobs_.empty();});
             if(stop_)return;key=jobs_.front();jobs_.pop_front();}
            PngPixels pixels;bool ok=false;
            try {ok=decodePixels(key,pixels);}catch(const std::exception&){}
            std::lock_guard lock(mutex_);pending_.erase(key);if(stop_)return;
            if(!ok)continue;
            constexpr size_t budget=64*1024*1024; // One maximum 4096x4096 atlas, or several smaller pages.
            while(bytes_+pixels.bgra.size()>budget && !ready_.empty()) {
                bytes_-=ready_.begin()->second.bgra.size();ready_.erase(ready_.begin());
            }
            if(pixels.bgra.size()<=budget) {bytes_+=pixels.bgra.size();ready_.emplace(std::move(key),std::move(pixels));}
        }
    }
public:
    explicit PngPrefetch(bool enabled) {
        if(enabled)try {worker_=std::thread([this]{run();});}catch(const std::system_error&){}
    }
    ~PngPrefetch() {{std::lock_guard lock(mutex_);stop_=true;}changed_.notify_one();if(worker_.joinable())worker_.join();}
    void request(TextureKey key) {
        if(!worker_.joinable())return; // Decoding remains available synchronously.
        {std::lock_guard lock(mutex_);
         if(jobs_.size()>=64 || pending_.contains(key) || ready_.contains(key))return;
         pending_.insert(key);jobs_.push_back(std::move(key));}
        changed_.notify_one();
    }
    bool take(const TextureKey& key,PngPixels& pixels) {
        std::lock_guard lock(mutex_);auto found=ready_.find(key);if(found==ready_.end())return false;
        bytes_-=found->second.bgra.size();pixels=std::move(found->second);ready_.erase(found);return true;
    }
};
}

struct HardwareRenderer::Device {
    struct Texture {
        ComPtr<ID2D1Bitmap> bitmap;
        size_t bytes{};
        unsigned used{};
    };
    const sw2::MenuAssets& assets;
    sw2::RasterCompositor compositor;
    std::shared_ptr<const sw2::Raster> compositePixels;
    ComPtr<ID2D1Bitmap> compositeBitmap;
    ComPtr<ID2D1Factory> factory;
    ComPtr<IDWriteFactory> fonts;
    ComPtr<ID2D1HwndRenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    std::map<std::pair<std::filesystem::path,int>,Texture> textures;
    std::map<std::pair<double,bool>,ComPtr<IDWriteTextFormat>> formats;
    size_t textureBytes{};
    unsigned frame{};
    PngPrefetch prefetch;
    bool prefetchEnabled{};
    size_t loads{},evictions{},prefetched{};
    double decodeMs{};
    bool available{};
    bool synchronized{};

    explicit Device(const sw2::MenuAssets& a,bool predict):assets(a),compositor(a,loadWindowsPng),prefetch(predict),prefetchEnabled(predict) {
        available=SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf())) &&
            SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
                                          reinterpret_cast<IUnknown**>(fonts.GetAddressOf())));
    }

    void discardTarget() {
        compositeBitmap.Reset();compositePixels.reset();
        textures.clear();
        textureBytes=0;
        brush.Reset();
        target.Reset();
    }

    bool prepare(HWND hwnd,D2D1_SIZE_U size,bool vsync) {
        if (!available) return false;
        // A HWND target's presentation options are set at creation.
        if (target && synchronized!=vsync) discardTarget();
        if (target) {
            const auto current=target->GetPixelSize();
            if (current.width==size.width && current.height==size.height) return true;
            if (SUCCEEDED(target->Resize(size))) return true;
            discardTarget();
        }
        const auto properties=D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_HARDWARE,D2D1::PixelFormat(),96,96);
        const auto present=vsync ? D2D1_PRESENT_OPTIONS_NONE : D2D1_PRESENT_OPTIONS_IMMEDIATELY;
        HRESULT hr=factory->CreateHwndRenderTarget(properties,
            D2D1::HwndRenderTargetProperties(hwnd,size,present),target.GetAddressOf());
        if (SUCCEEDED(hr)) hr=target->CreateSolidColorBrush(D2D1::ColorF(0),brush.GetAddressOf());
        if (FAILED(hr)) {
            discardTarget();
            if (hr!=D2DERR_RECREATE_TARGET) available=false;
            return false;
        }
        synchronized=vsync;
        return true;
    }

    ID2D1Bitmap* texture(const Draw& d) {
        if(d.children || d.pixels) {
            const auto pixels=compositor.image(d);
            if(compositePixels==pixels && compositeBitmap) return compositeBitmap.Get();
            std::vector<BYTE> bgra(pixels->rgba.size());
            for(size_t i=0;i<bgra.size();i+=4) {
                const auto* s=pixels->rgba.data()+i;const int a=s[3];
                bgra[i]=BYTE((s[2]*a+127)/255);bgra[i+1]=BYTE((s[1]*a+127)/255);bgra[i+2]=BYTE((s[0]*a+127)/255);bgra[i+3]=s[3];
            }
            const auto size=D2D1::SizeU(UINT(pixels->width),UINT(pixels->height));
            if(compositeBitmap && (compositeBitmap->GetPixelSize().width!=size.width || compositeBitmap->GetPixelSize().height!=size.height)) compositeBitmap.Reset();
            HRESULT hr;
            if(compositeBitmap) hr=compositeBitmap->CopyFromMemory(nullptr,bgra.data(),size.width*4);
            else hr=target->CreateBitmap(size,bgra.data(),size.width*4,D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED)),compositeBitmap.GetAddressOf());
            if(FAILED(hr)) return nullptr;compositePixels=pixels;return compositeBitmap.Get();
        }
        const auto key=std::pair{assets.texture(d.symbol,d.frame).file,d.redOffset};
        if (const auto found=textures.find(key); found!=textures.end()) {
            found->second.used=frame;
            if(assets.texture(d.symbol,d.frame).region)predict(d);
            return found->second.bitmap.Get();
        }
        const auto started=std::chrono::steady_clock::now();
        PngPixels pixels;
        const bool predicted=prefetchEnabled && prefetch.take(key,pixels);
        if(!predicted && !decodePixels(key,pixels))return nullptr;
        if(std::max(pixels.width,pixels.height)>target->GetMaximumBitmapSize())return nullptr;
        Texture entry;
        const auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
        const HRESULT hr=target->CreateBitmap(D2D1::SizeU(pixels.width,pixels.height),pixels.bgra.data(),pixels.width*4,properties,entry.bitmap.GetAddressOf());
        decodeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();++loads;
        if(predicted)++prefetched;
        if(FAILED(hr))return nullptr;
        entry.bytes=pixels.bgra.size();entry.used=frame;textureBytes+=entry.bytes;
        const auto inserted=textures.emplace(key,std::move(entry)).first;
        predict(d);
        return inserted->second.bitmap.Get();
    }

    void predict(const Draw& d) {
        if(!prefetchEnabled || d.children || d.pixels)return;
        if(!assets.texture(d.symbol,d.frame).region)switch(d.symbol) {
        case 6465:case 6550:case 6730:case 6815:case 7024:
        case 7388:case 7519:case 7525:case 7514:break;
        default:return;
        }
        int next=d.frame;
        for(int i=0;i<8;++i) {
            auto found=assets.art.find({d.symbol,++next});
            if(found==assets.art.end()) {next=d.frame>=50001 ? 50001 : 1;found=assets.art.find({d.symbol,next});}
            if(found==assets.art.end())break;
            TextureKey key{found->second.file,d.redOffset};
            if(!textures.contains(key))prefetch.request(std::move(key));
        }
    }

    void trimTextures() {
        // Keep nearby animation frames hot without loading an entire campaign
        // into video memory. Never trim textures drawn in the current frame.
        constexpr size_t budget=192*1024*1024;
        if (textureBytes<=budget) return;
        std::vector<decltype(textures)::iterator> candidates;
        for (auto it=textures.begin();it!=textures.end();++it)
            if (it->second.used!=frame) candidates.push_back(it);
        std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a->second.used<b->second.used;});
        for (auto it:candidates) {
            textureBytes-=it->second.bytes;
            textures.erase(it);
            ++evictions;
            if (textureBytes<=budget) break;
        }
    }

    void shape(const Draw& d,D2D1_RECT_F rect) {
        brush->SetColor(D2D1::ColorF(d.color,float(d.alpha)));
        switch (d.kind) {
        case Draw::Kind::Text: {
            auto& format=formats[{d.fontSize,d.centered}];
            if (!format) {
                if (FAILED(fonts->CreateTextFormat(L"Verdana",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,float(d.fontSize),L"en",
                    format.GetAddressOf()))) return;
                format->SetTextAlignment(d.centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            const std::wstring message(d.text.begin(),d.text.end());
            target->DrawTextW(message.c_str(),UINT(message.size()),format.Get(),rect,brush.Get());
            break;
        }
        case Draw::Kind::Outline:
            target->DrawRectangle(rect,brush.Get());
            break;
        case Draw::Kind::Ellipse:
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(float(d.target.x+d.target.w/2),
                float(d.target.y+d.target.h/2)),float(d.target.w/2),float(d.target.h/2)),brush.Get());
            break;
        case Draw::Kind::Line: case Draw::Kind::Polygon: {
            auto point=[&](sw2::Point p){return D2D1::Point2F(float(d.target.x+p.x),float(d.target.y+p.y));};
            if (d.kind==Draw::Kind::Line) {
                for (size_t i=1;i<d.vertices.size();++i)
                    target->DrawLine(point(d.vertices[i-1]),point(d.vertices[i]),brush.Get(),float(d.lineWidth));
            } else if (d.vertices.size()>=3) {
                ComPtr<ID2D1PathGeometry> path;
                ComPtr<ID2D1GeometrySink> sink;
                if (FAILED(factory->CreatePathGeometry(path.GetAddressOf())) || FAILED(path->Open(sink.GetAddressOf()))) return;
                sink->BeginFigure(point(d.vertices.front()),D2D1_FIGURE_BEGIN_FILLED);
                for (size_t i=1;i<d.vertices.size();++i) sink->AddLine(point(d.vertices[i]));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                if (SUCCEEDED(sink->Close())) target->FillGeometry(path.Get(),brush.Get());
            }
            break;
        }
        default: target->FillRectangle(rect,brush.Get()); break;
        }
    }
};

HardwareRenderer::HardwareRenderer(const sw2::MenuAssets& assets,bool predict):device_(std::make_unique<Device>(assets,predict)) {}
HardwareRenderer::Statistics HardwareRenderer::statistics() const {
    return {device_->textureBytes,device_->textures.size(),device_->loads,device_->evictions,device_->prefetched,device_->decodeMs};
}
HardwareRenderer::~HardwareRenderer()=default;
bool HardwareRenderer::synchronized() const { return device_->target && device_->synchronized; }
SIZE HardwareRenderer::pixelSize() const {
    if (!device_->target) return {};
    const auto size=device_->target->GetPixelSize();
    return {LONG(size.width),LONG(size.height)};
}

bool HardwareRenderer::draw(HWND hwnd,int w,int h,const std::vector<Draw>& draws,bool diagnostic,bool vsync) {
    auto& d=*device_;
    if (!d.prepare(hwnd,D2D1::SizeU(UINT(w),UINT(h)),vsync)) return false;
    const Viewport view(w,h);
    const float s=float(view.scale),sx=float(view.scaleX);
    using Matrix=D2D1::Matrix3x2F;
    const auto stage=Matrix::Scale(sx,s)*Matrix::Translation(float(view.pixels.x-Viewport::left*view.scaleX),
                                                           float(view.pixels.y));
    auto* target=d.target.Get();
    ++d.frame;
    target->BeginDraw();
    target->SetTransform(stage);
    target->Clear(D2D1::ColorF(diagnostic ? 0xff00ff : 0x000000));
    target->PushAxisAlignedClip(rectangle({Viewport::left,0,Viewport::width,Viewport::height}),
                                D2D1_ANTIALIAS_MODE_ALIASED);
    bool complete=true;
    for (const auto& item:draws) {
        const auto r=rectangle(item.target);
        if (r.bottom<=0 || r.top>=Viewport::height || r.right<=Viewport::left || r.left>=Viewport::right ||
            r.right<=r.left || r.bottom<=r.top || item.alpha<=0) continue;
        if (!item.symbol && !item.children && !item.pixels) { d.shape(item,r); continue; }
        auto* bitmap=d.texture(item);
        if (!bitmap) { complete=false; break; }
        if (item.rotation || item.mirrored) {
            const auto center=D2D1::Point2F((r.left+r.right)/2,(r.top+r.bottom)/2);
            const auto mirror=Matrix::Scale(item.mirrored ? -1.f : 1.f,1.f,center);
            target->SetTransform(mirror*Matrix::Rotation(float(item.rotation),center)*stage);
        }
        if(item.source || (item.symbol && !item.children && !item.pixels && d.assets.texture(item.symbol,item.frame).region)) {
            const auto b=bitmap->GetSize();
            const auto a=item.children||item.pixels?item.source.value_or(Rect{0,0,1,1}):textureRegion(d.assets.texture(item.symbol,item.frame),item.source);
            const auto source=D2D1::RectF(float(a.x*b.width),float(a.y*b.height),float((a.x+a.w)*b.width),float((a.y+a.h)*b.height));
            target->DrawBitmap(bitmap,r,float(item.alpha),D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
        } else target->DrawBitmap(bitmap,r,float(item.alpha));
        if (item.rotation || item.mirrored) target->SetTransform(stage);
    }
    target->PopAxisAlignedClip();
    const HRESULT status=target->EndDraw();
    if (FAILED(status) || !complete) {
        d.discardTarget();
        if (status!=D2DERR_RECREATE_TARGET) d.available=false;
        return false;
    }
    d.trimTextures();
    return true;
}
