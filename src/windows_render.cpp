#include "windows_render.hpp"

#include <objidl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <tuple>

using Microsoft::WRL::ComPtr;
using sw2::Draw;
using sw2::Rect;
using sw2::Viewport;
namespace gd = Gdiplus;

namespace {
D2D1_RECT_F rectangle(Rect r) {
    return D2D1::RectF(float(r.x),float(r.y),float(r.x+r.w),float(r.y+r.h));
}
}

struct HardwareRenderer::Device {
    struct Texture {
        ComPtr<ID2D1Bitmap> bitmap;
        size_t bytes{};
        unsigned used{};
    };
    const sw2::MenuAssets& assets;
    ComPtr<ID2D1Factory> factory;
    ComPtr<IDWriteFactory> fonts;
    ComPtr<ID2D1HwndRenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    std::map<std::tuple<int,int,int>,Texture> textures;
    std::map<std::pair<double,bool>,ComPtr<IDWriteTextFormat>> formats;
    size_t textureBytes{};
    unsigned frame{};
    bool available{};
    bool synchronized{};

    explicit Device(const sw2::MenuAssets& a):assets(a) {
        available=SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf())) &&
            SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
                                          reinterpret_cast<IUnknown**>(fonts.GetAddressOf())));
    }

    void discardTarget() {
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
        const auto key=std::tuple(d.symbol,d.frame,d.redOffset);
        if (const auto found=textures.find(key); found!=textures.end()) {
            found->second.used=frame;
            return found->second.bitmap.Get();
        }
        gd::Bitmap image(assets.texture(d.symbol,d.frame).file.c_str());
        if (image.GetLastStatus()!=gd::Ok) return nullptr;
        const auto size=D2D1::SizeU(image.GetWidth(),image.GetHeight());
        if (std::max(size.width,size.height)>target->GetMaximumBitmapSize()) return nullptr;
        const gd::Rect region(0,0,int(size.width),int(size.height));
        gd::BitmapData data;
        const auto format=d.redOffset ? PixelFormat32bppARGB : PixelFormat32bppPARGB;
        if (image.LockBits(&region,gd::ImageLockModeRead,format,&data)!=gd::Ok) return nullptr;
        const size_t pitch=size_t(size.width)*4;
        auto* pixels=static_cast<const BYTE*>(data.Scan0);
        std::vector<BYTE> converted;
        if (data.Stride<0 || d.redOffset) {
            converted.resize(pitch*size.height);
            for (UINT y=0;y<size.height;++y) {
                const BYTE* row=pixels+ptrdiff_t(y)*data.Stride;
                BYTE* out=converted.data()+y*pitch;
                if (!d.redOffset) std::copy_n(row,pitch,out);
                else for (UINT x=0;x<size.width;++x,row+=4,out+=4) {
                    const int a=row[3];
                    out[0]=BYTE((row[0]*a+127)/255);
                    out[1]=BYTE((row[1]*a+127)/255);
                    out[2]=BYTE((std::min(255,row[2]+d.redOffset)*a+127)/255);
                    out[3]=row[3];
                }
            }
            pixels=converted.data();
        }
        Texture entry;
        const auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
        const auto stride=converted.empty() ? UINT(data.Stride) : UINT(pitch);
        const HRESULT hr=target->CreateBitmap(size,pixels,stride,properties,entry.bitmap.GetAddressOf());
        image.UnlockBits(&data);
        if (FAILED(hr)) return nullptr;
        entry.bytes=pitch*size.height;
        entry.used=frame;
        textureBytes+=entry.bytes;
        return textures.emplace(key,std::move(entry)).first->second.bitmap.Get();
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

HardwareRenderer::HardwareRenderer(const sw2::MenuAssets& assets):device_(std::make_unique<Device>(assets)) {}
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
    const float s=float(view.scale);
    using Matrix=D2D1::Matrix3x2F;
    const auto stage=Matrix::Scale(s,s)*Matrix::Translation(float(view.pixels.x-Viewport::left*view.scale),
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
        if (!item.symbol) { d.shape(item,r); continue; }
        auto* bitmap=d.texture(item);
        if (!bitmap) { complete=false; break; }
        if (item.rotation || item.mirrored) {
            const auto center=D2D1::Point2F((r.left+r.right)/2,(r.top+r.bottom)/2);
            const auto mirror=Matrix::Scale(item.mirrored ? -1.f : 1.f,1.f,center);
            target->SetTransform(mirror*Matrix::Rotation(float(item.rotation),center)*stage);
        }
        target->DrawBitmap(bitmap,r,float(item.alpha));
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
