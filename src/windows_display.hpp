#pragma once

#include "campaign_menu.hpp"

#include <windows.h>

class DisplaySettings {
public:
    enum class Mode { Windowed, Borderless, Fullscreen };
    bool isOpen() const { return open_; }
    Mode mode() const { return mode_; }
    int frameLimit() const;
    bool vsync() const { return vsync_; }
    bool showFps() const { return showFps_; }
    void setOpen(bool open);
    void setMode(HWND, Mode);
    void key(HWND, WPARAM);
    void pointer(HWND, UINT message, std::optional<sw2::Point>);
    void append(std::vector<sw2::Draw>&) const;
    void resize(HWND, WPARAM edge, RECT& outer) const;
private:
    void apply(HWND,int choice);
    Mode mode_{Mode::Windowed};
    bool open_{},vsync_{},showFps_{};
    int rateChoice_{1};
    int focused_{}, pressed_{-1};
    LONG_PTR windowStyle_{WS_OVERLAPPEDWINDOW};
    WINDOWPLACEMENT windowed_{sizeof(WINDOWPLACEMENT)};
};
