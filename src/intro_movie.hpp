#pragma once
#include "raster_compositor.hpp"
#include <functional>
#include <chrono>

namespace sw2 {
class IntroFrameCache;
class IntroClock {
    using Clock=std::chrono::steady_clock;
    Clock::time_point start_{},pausedAt_{};
    double lastAudio_{};
    bool paused_{};
public:
    void begin(Clock::time_point now=Clock::now()) {start_=now;paused_=false;lastAudio_=0;}
    void pause(bool value,Clock::time_point now=Clock::now()) {
        if(value&&!paused_)pausedAt_=now;
        if(!value&&paused_)start_+=now-pausedAt_;
        paused_=value;
    }
    double seconds(bool audioPlaying,double audioSeconds,double duration,Clock::time_point now=Clock::now()) {
        if(audioPlaying) {lastAudio_=audioSeconds;return audioSeconds;}
        if(lastAudio_>0 && lastAudio_>=duration-.25)return duration;
        return std::chrono::duration<double>((paused_?pausedAt_:now)-start_).count();
    }
};
// Extracted authored frames; no Flash runtime or movie script executes here.
class IntroMovie {
public:
    using Decoder=std::function<Raster(const std::filesystem::path&)>;
    IntroMovie(const std::filesystem::path&,Decoder,bool prefetch=false);
    ~IntroMovie();
    bool available() const {return !frames_.empty();}
    void start();
    void stop();
    void advance(double seconds);
    void seek(double seconds);
    bool finished() const {return finished_;}
    double duration() const {return frames_.size()/double(rate_);}
    int sourceFrame() const {return current_<0 ? first_ : first_+current_;}
    const std::filesystem::path& music() const {return music_;}
    std::vector<std::filesystem::path> takeEffects();
    Draw draw(Rect viewport) const;
    std::shared_ptr<const Raster> pixels() const {return pixels_;}
    struct Statistics {unsigned long long decodes{};double decodeMilliseconds{},composeMilliseconds{};};
    Statistics statistics() const {return statistics_;}
private:
    Statistics statistics_;
    bool prefetchEnabled_{};
    std::unique_ptr<IntroFrameCache> frameCache_;
    struct Frame {bool key{};std::filesystem::path file;int keyIndex{};};
    struct Effect {int frame{};std::filesystem::path file;};
    std::filesystem::path directory_,music_;
    Decoder decode_;
    std::vector<Frame> frames_;
    std::vector<Effect> effects_;
    std::vector<std::filesystem::path> pendingEffects_;
    std::shared_ptr<const Raster> pixels_;
    int width_{},height_{},rate_{30},first_{3},current_{-1};
    std::size_t nextEffect_{};
    double elapsed_{};
    bool finished_{};
    std::filesystem::path relativeFile(const std::string&) const;
    void decodeFrame(int);
};
}
