#pragma once

#include "campaign_menu.hpp"

#include <windows.h>
#include <memory>

// Device resources belong to the HWND. Off-screen captures and the software
// fallback use the separate GDI+ renderer.
class HardwareRenderer {
public:
    explicit HardwareRenderer(const sw2::MenuAssets&);
    ~HardwareRenderer();
    bool draw(HWND, int width, int height, const std::vector<sw2::Draw>&, bool diagnostic, bool vsync);
    bool synchronized() const;
private:
    struct Device;
    std::unique_ptr<Device> device_;
};
