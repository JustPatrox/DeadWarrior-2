#pragma once
#include <windows.h>
#include <xaudio2.h>
#include <array>
#include <filesystem>
#include <map>
#include <functional>
#include <set>
#include <string_view>
#include <string>
#include <vector>

namespace sw2 {
class WindowsAudio {
public:
    explicit WindowsAudio(const std::filesystem::path& directory,std::function<void(std::string_view)> reporter={});
    ~WindowsAudio();
    WindowsAudio(const WindowsAudio&)=delete;
    WindowsAudio& operator=(const WindowsAudio&)=delete;
    void effect(const std::string& name,double pan=0,bool spatial=true);
    void effectFile(const std::filesystem::path&,float gain=1.f);
    void narration(const std::filesystem::path&);
    void music(const std::string& name,bool loop=true);
    void musicFile(const std::filesystem::path&,bool loop=false,float gain=1.f);
    void stopMusic();
    void stopNarration();
    void stopMovieEffects();
    void setMuted(bool muted);
    void setSoundEnabled(bool);
    void setMusicEnabled(bool);
    void setPaused(bool);
    bool soundEnabled() const { return soundEnabled_; }
    bool musicEnabled() const { return musicEnabled_; }
    bool ready() const {return master_!=nullptr;}
    unsigned submitted() const {return submitted_;}
    unsigned activeVoices() const;
    bool musicPlaying() const;
    double musicSeconds() const;
    unsigned long long narrationSamplesPlayed() const;
    unsigned long long musicSamplesPlayed() const;
private:
    std::function<void(std::string_view)> reporter_;
    std::set<std::string> reportedMissing_;
    void report(std::string message) const {if (reporter_) reporter_(message);}
    struct Clip {WAVEFORMATEX format{}; std::vector<BYTE> data;};
    struct Voice {IXAudio2SourceVoice* source{}; bool narration{}; unsigned serial{}; float volume{}; bool cinematic{};};
    std::filesystem::path directory_;
    std::map<std::filesystem::path,Clip> clips_;
    std::map<std::string,std::pair<std::filesystem::path,float>> sounds_;
    std::array<Voice,32> voices_{};
    IXAudio2* engine_{};
    IXAudio2MasteringVoice* master_{};
    IXAudio2SourceVoice* music_{};
    std::string currentMusic_;
    bool musicLoop_{true};
    bool movieMusic_{};
    unsigned musicSampleRate_{};
    bool ownsCom_{};
    bool soundEnabled_{true},musicEnabled_{true},paused_{};
    float musicVolume_{1};
    unsigned submitted_{};
    Clip& load(const std::filesystem::path&);
    void play(Clip&,float volume,float pan,bool narration,bool cinematic=false);
};
}
