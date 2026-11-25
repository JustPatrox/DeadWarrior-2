#pragma once
#include "campaign_menu.hpp"
#include "raster_compositor.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <fstream>
#include <functional>
#include <mutex>

namespace sw2 {
Raster loadSdlPng(const std::filesystem::path&);
#ifdef __APPLE__
inline constexpr bool optionSettingsKey=true;
inline constexpr SDL_Keycode settingsKeyCode=SDLK_LALT;
inline constexpr const char* settingsKeyLabel="Option";
#else
inline constexpr bool optionSettingsKey=false;
inline constexpr SDL_Keycode settingsKeyCode=SDLK_INSERT;
inline constexpr const char* settingsKeyLabel="Insert";
#endif
class SdlRenderer {
    struct Cached {SDL_Texture* image{}; size_t bytes{}; unsigned serial{};};
    SDL_Renderer* renderer_{};
    const MenuAssets& art_;
    RasterCompositor compositor_;
    std::shared_ptr<const Raster> compositePixels_;
    SDL_Texture* compositeTexture_{};
    SDL_Texture* composite(const Draw&);
    std::filesystem::path root_;
    std::map<std::pair<std::filesystem::path,int>,Cached> images_;
    SDL_Texture* font_{};
    std::array<std::array<float,8>,95> glyphs_{};
    unsigned serial_{};
    size_t bytes_{};
    SDL_Texture* image(const std::filesystem::path&,int tint=0);
    void text(const Draw&,const Viewport&);
public:
    SdlRenderer(SDL_Window*,const MenuAssets&,const std::filesystem::path&);
    ~SdlRenderer();
    SDL_Renderer* native() const {return renderer_;}
    void draw(const std::vector<Draw>&,int width,int height);
    int hit(const CampaignMenu&,Point stage);
    void png(const std::filesystem::path&);
    void picture(const std::filesystem::path&,Rect,const Viewport&);
};

class SdlAudio {
    struct Clip {std::vector<float> samples;};
    struct Voice {const Clip* clip{}; size_t cursor{}; float left{1},right{1}; bool loop{},narration{},music{},cinematic{};};
    std::filesystem::path directory_;
    std::function<void(std::string)> report_;
    std::map<std::string,std::pair<std::filesystem::path,float>> sounds_;
    std::map<std::filesystem::path,Clip> clips_;
    std::vector<Voice> voices_;
    std::mutex mutex_;
    SDL_AudioStream* stream_{};
    bool sound_{true},music_{true},paused_{};
    std::string currentMusic_;
    Clip& load(const std::filesystem::path&);
    void play(Clip&,float,float,bool,bool,bool,bool cinematic=false);
    static void callback(void*,SDL_AudioStream*,int,int);
public:
    SdlAudio(const std::filesystem::path&,std::function<void(std::string)>);
    ~SdlAudio();
    void effect(const std::string&,double pan=0,bool spatial=true);
    void effectFile(const std::filesystem::path&,float gain=1.f);
    void music(const std::string&);
    void musicFile(const std::filesystem::path&,bool loop=false,float gain=1.f);
    void narration(const std::filesystem::path&);
    void stopNarration();
    void stopMusic();
    void stopMovieEffects();
    void paused(bool);
    void channels(bool sound,bool music);
    bool soundEnabled() const {return sound_;}
    bool musicEnabled() const {return music_;}
    size_t musicSamplesPlayed();
    bool musicPlaying();
    double musicSeconds();
    bool testMix();
};

class SdlDisplay {
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    std::filesystem::path path_;
    int mode_{},rate_{1},resolution_{},focus_{},pressed_{-1},aspect_{2};
    bool open_{},vsync_{},fps_{},mouseLock_{};
    std::array<int,4> windowed_{100,100,1280,720};
    void apply(int choice);
    void mode(int);
public:
    SdlDisplay(SDL_Window*,SDL_Renderer*,const std::filesystem::path&);
    bool isOpen() const {return open_;}
    int displayMode() const {return mode_;}
    void setOpen(bool value) {open_=value;focus_=mode_;pressed_=-1;}
    int frameLimit() const;
    bool showFps() const {return fps_;}
    bool mouseLock() const {return mouseLock_;}
    void configureViewport(int w,int h) const {Viewport::configure(Viewport::Aspect(aspect_),w,h);}
    void fullscreen() {mode(mode_==2 ? 0 : 2);save();}
    void key(SDL_Keycode);
    void pointer(int action,std::optional<Point>);
    void append(std::vector<Draw>&) const;
    std::pair<int,int> renderSize(int,int) const;
    void save() const;
};
}
