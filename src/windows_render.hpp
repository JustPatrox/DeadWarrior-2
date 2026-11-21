#pragma once

#include "campaign_menu.hpp"
#include "raster_compositor.hpp"

#include <windows.h>
#include <memory>
sw2::Raster loadWindowsPng(const std::filesystem::path&);

// Device resources belong to the HWND. Off-screen captures and the software
// fallback use the separate GDI+ renderer.
class HardwareRenderer {
public:
    explicit HardwareRenderer(const sw2::MenuAssets&,bool prefetchAnimations=true);
    ~HardwareRenderer();
    bool draw(HWND, int width, int height, const std::vector<sw2::Draw>&, bool diagnostic, bool vsync);
    bool synchronized() const;
    SIZE pixelSize() const;
    struct Statistics {size_t textureBytes{},textureCount{},loads{},evictions{},prefetched{};double decodeMs{};};
    Statistics statistics() const;
private:
    struct Device;
    std::unique_ptr<Device> device_;
};
