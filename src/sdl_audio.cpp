#include "sdl_platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sw2 {
SdlAudio::SdlAudio(const std::filesystem::path& dir,std::function<void(std::string)> report)
    :directory_(dir),report_(std::move(report)) {
    std::ifstream input(dir/"sounds.tsv");
    std::string name,file;float gain;
    while(input>>name>>file>>gain) sounds_[name]={dir/file,gain};
    if(!input.is_open() || !input.eof()) throw std::runtime_error("Invalid audio manifest");
    SDL_AudioSpec format{SDL_AUDIO_F32,2,48000};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&format,callback,this);
    if(stream_) SDL_ResumeAudioStreamDevice(stream_);
    report_(stream_ ? "AUDIO ready stereo=48000" : std::string("AUDIO unavailable: ")+SDL_GetError());
}
SdlAudio::~SdlAudio() {if(stream_) SDL_DestroyAudioStream(stream_);}
SdlAudio::Clip& SdlAudio::load(const std::filesystem::path& path) {
    if(auto it=clips_.find(path);it!=clips_.end()) return it->second;
    SDL_AudioSpec source{},target{SDL_AUDIO_F32,2,48000};
    Uint8 *data=nullptr,*converted=nullptr;Uint32 length{};int convertedLength{};
    if(!SDL_LoadWAV(path.string().c_str(),&source,&data,&length))
        throw std::runtime_error("Cannot load audio "+path.string()+": "+SDL_GetError());
    const bool ok=SDL_ConvertAudioSamples(&source,data,int(length),&target,&converted,&convertedLength);
    SDL_free(data);
    if(!ok) throw std::runtime_error(std::string("Audio conversion failed: ")+SDL_GetError());
    Clip clip;clip.samples.resize(convertedLength/sizeof(float));
    std::memcpy(clip.samples.data(),converted,convertedLength);SDL_free(converted);
    return clips_.emplace(path,std::move(clip)).first->second;
}
void SdlAudio::play(Clip& clip,float gain,float pan,bool narration,bool music,bool loop) {
    std::lock_guard lock(mutex_);
    if(narration || music) std::erase_if(voices_,[&](const Voice& v){return narration ? v.narration : v.music;});
    if(voices_.size()>=66) {
        auto it=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return !v.music&&!v.narration;});
        if(it!=voices_.end()) voices_.erase(it);else return;
    }
    pan=std::clamp(pan,-1.f,1.f);
    voices_.push_back({&clip,0,gain*(1-std::max(0.f,pan)),gain*(1+std::min(0.f,pan)),loop,narration,music});
}
void SdlAudio::effect(const std::string& name,double pan,bool spatial) {
    auto found=sounds_.find(name);
    if(found==sounds_.end()) {report_("AUDIO MISSING effect="+name);return;}
    const float volume=float(found->second.second*(spatial ? .3/std::max(1.,std::pow(std::abs(pan),4)) : 1.));
    play(load(found->second.first),volume,float(pan),false,false,false);
}
void SdlAudio::music(const std::string& name) {
    auto found=sounds_.find(name);
    if(found==sounds_.end()) {report_("AUDIO MISSING music="+name);return;}
    play(load(found->second.first),.2f*found->second.second,0,false,true,true);
    report_("AUDIO music="+name);
}
void SdlAudio::narration(const std::filesystem::path& path) {
    auto found=sounds_.find(path.stem().string());
    play(load(path),found==sounds_.end() ? 1.f : found->second.second,0,true,false,false);
}
void SdlAudio::stopNarration() {std::lock_guard lock(mutex_);std::erase_if(voices_,[](const Voice& v){return v.narration;});}
void SdlAudio::stopMusic() {std::lock_guard lock(mutex_);std::erase_if(voices_,[](const Voice& v){return v.music;});}
void SdlAudio::paused(bool value) {std::lock_guard lock(mutex_);paused_=value;}
void SdlAudio::channels(bool sound,bool music) {std::lock_guard lock(mutex_);sound_=sound;music_=music;}
void SdlAudio::callback(void* context,SDL_AudioStream* stream,int additional,int) {
    auto& self=*static_cast<SdlAudio*>(context);
    if(additional<=0) return;
    std::vector<float> mixed(size_t((additional+7)/8)*2,0);
    {
        std::lock_guard lock(self.mutex_);
        for(auto& voice:self.voices_) {
            if(!voice.music && self.paused_) continue;
            const bool enabled=voice.music ? self.music_ : self.sound_;
            for(size_t i=0;i<mixed.size();i+=2) {
                if(voice.cursor+1>=voice.clip->samples.size()) {
                    if(voice.loop) voice.cursor=0;else break;
                }
                if(enabled) {mixed[i]+=voice.clip->samples[voice.cursor]*voice.left;mixed[i+1]+=voice.clip->samples[voice.cursor+1]*voice.right;}
                voice.cursor+=2;
            }
        }
        std::erase_if(self.voices_,[](const Voice& v){return !v.loop&&v.cursor>=v.clip->samples.size();});
    }
    for(auto& sample:mixed) sample=std::clamp(sample,-1.f,1.f);
    SDL_PutAudioStreamData(stream,mixed.data(),int(mixed.size()*sizeof(float)));
}
bool SdlAudio::testMix() {
    // Decode the same manifest and PCM converter used by playback, including
    // menu music and narration. Dummy devices exercise the actual callback.
    bool nonzero=false;
    for(const char* name:{"loginMusic","clickButton","orderInGame","OrderVictory"}) {
        const auto found=sounds_.find(name);
        if(found==sounds_.end()) return false;
        const auto& pcm=load(found->second.first).samples;
        if(pcm.empty() || !std::all_of(pcm.begin(),pcm.end(),[](float n){return std::isfinite(n);})) return false;
        nonzero|=std::any_of(pcm.begin(),pcm.end(),[](float n){return std::abs(n)>.001f;});
    }
    music("loginMusic");effect("clickButton",0,false);
    SDL_Delay(100);
    return stream_ && nonzero;
}
}
