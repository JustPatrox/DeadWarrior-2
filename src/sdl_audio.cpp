#include "sdl_platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sw2 {
namespace {constexpr int mixerRate=48000,mixerChannels=2;}
SdlAudio::SdlAudio(const std::filesystem::path& dir,std::function<void(std::string)> report)
    :directory_(dir),report_(std::move(report)) {
    std::ifstream input(dir/"sounds.tsv");
    std::string name,file;float gain;
    while(input>>name>>file>>gain) sounds_[name]={dir/file,gain};
    if(!input.is_open() || !input.eof()) throw std::runtime_error("Invalid audio manifest");
    SDL_AudioSpec format{SDL_AUDIO_F32,mixerChannels,mixerRate};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&format,callback,this);
    if(stream_) SDL_ResumeAudioStreamDevice(stream_);
    report_(stream_ ? "AUDIO ready stereo=48000" : std::string("AUDIO unavailable: ")+SDL_GetError());
}
SdlAudio::~SdlAudio() {if(stream_) SDL_DestroyAudioStream(stream_);}
SdlAudio::Clip& SdlAudio::load(const std::filesystem::path& path) {
    if(auto it=clips_.find(path);it!=clips_.end()) return it->second;
    SDL_AudioSpec source{},target{SDL_AUDIO_F32,mixerChannels,mixerRate};
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
void SdlAudio::play(Clip& clip,float gain,float pan,bool narration,bool music,bool loop,bool cinematic) {
    std::lock_guard lock(mutex_);
    if(narration || music) std::erase_if(voices_,[&](const Voice& v){return narration ? v.narration : v.music;});
    if(voices_.size()>=66) {
        auto it=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return !v.music&&!v.narration;});
        if(it!=voices_.end()) voices_.erase(it);else return;
    }
    pan=std::clamp(pan,-1.f,1.f);
    voices_.push_back({&clip,0,gain*(1-std::max(0.f,pan)),gain*(1+std::min(0.f,pan)),loop,narration,music,cinematic});
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
    {
        std::lock_guard lock(mutex_);
        if(currentMusic_==name && std::any_of(voices_.begin(),voices_.end(),[](const Voice& v){return v.music&&v.loop;}))return;
    }
    play(load(found->second.first),.2f*found->second.second,0,false,true,true);
    {std::lock_guard lock(mutex_);currentMusic_=name;}
    report_("AUDIO music="+name);
}
void SdlAudio::effectFile(const std::filesystem::path& path,float gain) {
    play(load(path),gain,0,false,false,false,true);
}
void SdlAudio::musicFile(const std::filesystem::path& path,bool loop,float gain) {
    stopMusic();
    play(load(path),gain,0,false,true,loop,true);
    report_("AUDIO movie music loop="+std::to_string(loop)+" gain="+std::to_string(gain));
}
void SdlAudio::narration(const std::filesystem::path& path) {
    auto found=sounds_.find(path.stem().string());
    play(load(path),found==sounds_.end() ? 1.f : found->second.second,0,true,false,false);
}
void SdlAudio::stopNarration() {std::lock_guard lock(mutex_);std::erase_if(voices_,[](const Voice& v){return v.narration;});}
void SdlAudio::stopMusic() {std::lock_guard lock(mutex_);std::erase_if(voices_,[](const Voice& v){return v.music;});currentMusic_.clear();}
void SdlAudio::stopMovieEffects() {
    std::lock_guard lock(mutex_);
    std::erase_if(voices_,[](const Voice& v){return v.cinematic&&!v.music&&!v.narration;});
}
void SdlAudio::paused(bool value) {std::lock_guard lock(mutex_);paused_=value;}
void SdlAudio::channels(bool sound,bool music) {std::lock_guard lock(mutex_);sound_=sound;music_=music;}
size_t SdlAudio::musicSamplesPlayed() {
    std::lock_guard lock(mutex_);
    const auto v=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.music;});
    return v==voices_.end()?0:v->cursor;
}
bool SdlAudio::musicPlaying() {
    if(!stream_)return false;
    std::lock_guard lock(mutex_);
    return std::any_of(voices_.begin(),voices_.end(),[](const Voice& v){return v.music&&(v.loop||v.cursor<v.clip->samples.size());});
}
double SdlAudio::musicSeconds() {
    std::lock_guard lock(mutex_);
    const auto voice=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.music;});
    return voice==voices_.end()?0.:double(voice->cursor)/(mixerChannels*mixerRate);
}
void SdlAudio::callback(void* context,SDL_AudioStream* stream,int additional,int) {
    auto& self=*static_cast<SdlAudio*>(context);
    if(additional<=0) return;
    std::vector<float> mixed(size_t((additional+7)/8)*2,0);
    {
        std::lock_guard lock(self.mutex_);
        for(auto& voice:self.voices_) {
            if(self.paused_ && (!voice.music || voice.cinematic)) continue;
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
    size_t before=0;
    {std::lock_guard lock(mutex_);for(const auto& v:voices_)if(v.music)before=v.cursor;}
    music("loginMusic");
    {std::lock_guard lock(mutex_);
        const auto v=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.music;});
        if(v==voices_.end()||!before||v->cursor<before)return false;
    }
    // The existing long victory WAV is a reusable file fixture; no cinematic
    // assets are required to check independent file playback and cleanup.
    const auto path=sounds_.at("OrderVictory").first;
    const auto* fixture=&load(path);
    musicFile(path);SDL_Delay(100);
    SDL_AudioSpec input{},output{};
    if(!stream_ || !SDL_GetAudioStreamFormat(stream_,&input,&output) || input.freq<=0 || input.channels<=0)return false;
    const size_t fileBefore=musicSamplesPlayed();const double seconds=musicSeconds();const size_t fileAfter=musicSamplesPlayed();
    const double valuesPerSecond=double(input.freq)*input.channels;
    if(!musicPlaying() || !fileBefore || seconds<double(fileBefore)/valuesPerSecond || seconds>double(fileAfter)/valuesPerSecond)return false;
    {std::lock_guard lock(mutex_);
        const auto v=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.music;});
        if(v==voices_.end() || !v->cinematic || v->loop || v->left!=1 || v->right!=1)return false;
    }
    paused(true);SDL_Delay(60);
    const double frozen=musicSeconds();SDL_Delay(120);
    if(musicSeconds()!=frozen || !musicPlaying())return false;
    paused(false);SDL_Delay(100);
    if(musicSeconds()<=frozen)return false;
    const double replay=musicSeconds();musicFile(path);
    if(musicSeconds()>=replay)return false;
    effect("OrderVictory",0,false);narration(path);effectFile(path);
    auto categoryPresent=[&](int type) {
        std::lock_guard lock(mutex_);
        return std::any_of(voices_.begin(),voices_.end(),[&](const Voice& v){
            return v.clip==fixture && (type==0 ? v.music : type==1 ? v.narration : type==2 ?
                v.cinematic&&!v.music&&!v.narration : !v.music&&!v.narration&&!v.cinematic);
        });
    };
    if(!categoryPresent(0)||!categoryPresent(1)||!categoryPresent(2)||!categoryPresent(3))return false;
    stopMovieEffects();
    if(!categoryPresent(0)||!categoryPresent(1)||categoryPresent(2)||!categoryPresent(3))return false;
    paused(true);musicFile(path);SDL_Delay(100);
    if(musicSeconds()!=0)return false;
    paused(false);SDL_Delay(100);
    if(musicSeconds()<=0)return false;
    stopMusic();
    if(musicPlaying() || musicSeconds()!=0)return false;
    stopNarration();
    {std::lock_guard lock(mutex_);
        std::erase_if(voices_,[&](const Voice& v){return v.clip==fixture&&!v.music;});
    }
    music("loginMusic");
    return stream_ && nonzero;
}
}
