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
    bool mouseLock() const {return mouseLock_;}
    void configureViewport(int w,int h) const {sw2::Viewport::configure(sw2::Viewport::Aspect(aspectChoice_),w,h);}
    std::pair<int,int> renderSize(int width,int height) const;
    std::string resolutionLabel() const;
    bool loadOptions(const std::filesystem::path&);
    bool saveOptions(const std::filesystem::path&);
    void restore(HWND);
    void setOpen(bool open);
    void setMode(HWND, Mode);
    void key(HWND, WPARAM);
    void pointer(HWND, UINT message, std::optional<sw2::Point>);
    void append(std::vector<sw2::Draw>&) const;
    void resize(HWND, WPARAM edge, RECT& outer) const;
private:
    void apply(HWND,int choice);
    void applyResolution(HWND,int choice);
    void fitWindow(HWND);
    std::string optionsText() const;
    Mode mode_{Mode::Windowed};
    Mode startupMode_{Mode::Windowed};
    std::string savedOptions_;
    bool open_{},vsync_{},showFps_{},mouseLock_{};
    int aspectChoice_{2};
    int rateChoice_{1};
    int resolutionChoice_{}, resolutionFocus_{};
    int focused_{}, pressed_{-1};
    LONG_PTR windowStyle_{WS_OVERLAPPEDWINDOW};
    WINDOWPLACEMENT windowed_{sizeof(WINDOWPLACEMENT)};
};
