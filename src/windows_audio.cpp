#include "windows_audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <climits>
#include <fstream>
#include <stdexcept>

namespace sw2 {
WindowsAudio::WindowsAudio(const std::filesystem::path& directory,std::function<void(std::string_view)> reporter)
    :reporter_(std::move(reporter)),directory_(directory) {
    std::ifstream manifest(directory_/"sounds.tsv");
    std::string name,file; float gain;
    while (manifest>>name>>file>>gain) {
        const auto path=directory_/file;
        sounds_[name]={path,gain};
    }
    if (!manifest.eof()) throw std::runtime_error("Invalid sound manifest");
    ownsCom_=SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    const auto engineResult=XAudio2Create(&engine_);
    if (FAILED(engineResult)) {report("AUDIO ERROR XAudio2Create HRESULT="+std::to_string(engineResult)); return;}
    const auto masterResult=engine_->CreateMasteringVoice(&master_,2,44100);
    if (FAILED(masterResult)) {report("AUDIO ERROR CreateMasteringVoice HRESULT="+std::to_string(masterResult)); return;}
    report("AUDIO ready stereo=44100 sounds="+std::to_string(sounds_.size()));
}
WindowsAudio::~WindowsAudio() {
    stopMusic();
    for (auto& voice:voices_) if (voice.source) voice.source->DestroyVoice();
    if (master_) master_->DestroyVoice();
    if (engine_) engine_->Release();
    if (ownsCom_) CoUninitialize();
}
WindowsAudio::Clip& WindowsAudio::load(const std::filesystem::path& path) {
    if (auto it=clips_.find(path);it!=clips_.end()) return it->second;
    std::ifstream input(path,std::ios::binary);
    char riff[4]{},wave[4]{}; unsigned size{};
    input.read(riff,4); input.read(reinterpret_cast<char*>(&size),4); input.read(wave,4);
    if (!input || std::memcmp(riff,"RIFF",4) || std::memcmp(wave,"WAVE",4))
        throw std::runtime_error("Invalid audio file: "+path.string());
    Clip clip;
    char chunk[4]{};
    while (input.read(chunk,4) && input.read(reinterpret_cast<char*>(&size),4)) {
        if (size>64*1024*1024) throw std::runtime_error("Oversized audio chunk");
        const auto next=input.tellg()+std::streamoff(size+(size&1));
        if (!std::memcmp(chunk,"fmt ",4)) {
            if (size<16) throw std::runtime_error("Invalid WAV format");
            input.read(reinterpret_cast<char*>(&clip.format),16);
        } else if (!std::memcmp(chunk,"data",4)) {
            clip.data.resize(size); input.read(reinterpret_cast<char*>(clip.data.data()),size);
        }
        if (!input) throw std::runtime_error("Truncated audio file");
        input.seekg(next);
    }
    if (clip.format.wFormatTag!=WAVE_FORMAT_PCM || clip.format.wBitsPerSample!=16 ||
        clip.format.nChannels<1 || clip.format.nChannels>2 || clip.data.empty())
        throw std::runtime_error("Unsupported audio format");
    return clips_.emplace(path,std::move(clip)).first->second;
}
void WindowsAudio::play(Clip& clip,float volume,float pan,bool narration) {
    if (!ready()) return;
    if (narration) stopNarration();
    auto free=std::find_if(voices_.begin(),voices_.end(),[](const Voice& voice){
        if (!voice.source) return true;
        XAUDIO2_VOICE_STATE state{}; voice.source->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        return state.BuffersQueued==0;
    });
    if (free==voices_.end()) {
        free=std::min_element(voices_.begin(),voices_.end(),[](const Voice& a,const Voice& b){
            return (a.narration ? UINT_MAX : a.serial)<(b.narration ? UINT_MAX : b.serial);
        });
        if (free->narration) return;
    }
    if (free->source) {free->source->DestroyVoice(); free->source=nullptr;}
    const auto result=engine_->CreateSourceVoice(&free->source,&clip.format);
    if (FAILED(result)) {report("AUDIO ERROR CreateSourceVoice HRESULT="+std::to_string(result)); return;}
    XAUDIO2_BUFFER buffer{};
    buffer.Flags=XAUDIO2_END_OF_STREAM; buffer.AudioBytes=UINT32(clip.data.size()); buffer.pAudioData=clip.data.data();
    free->source->SetVolume(volume);
    pan=std::clamp(pan,-1.f,1.f);
    const float left=1-std::max(0.f,pan),right=1+std::min(0.f,pan);
    const float mono[]{left,right},stereo[]{left,0,0,right};
    free->source->SetOutputMatrix(master_,clip.format.nChannels,2,clip.format.nChannels==1 ? mono : stereo);
    if (FAILED(free->source->SubmitSourceBuffer(&buffer)) || FAILED(free->source->Start())) {
        free->source->DestroyVoice(); free->source=nullptr; return;
    }
    free->narration=narration; free->serial=++submitted_;
}
void WindowsAudio::effect(const std::string& name,double pan,bool spatial) {
    if (!ready()) return;
    auto sound=sounds_.find(name);
    if (sound==sounds_.end()) {
        if (reportedMissing_.insert(name).second) report("AUDIO MISSING effect="+name);
        return;
    }
    const double volume=sound->second.second*(spatial ? .3/std::max(1.,std::pow(std::abs(pan),4)) : 1.);
    play(load(sound->second.first),float(volume),float(pan),false);
}
void WindowsAudio::narration(const std::filesystem::path& path) {
    if (!ready()) return;
    const auto sound=sounds_.find(path.stem().string());
    const float gain=sound==sounds_.end() ? 1.f : sound->second.second;
    play(load(path),gain,0,true);
}
void WindowsAudio::music(const std::string& name,bool loop) {
    stopMusic();
    if (!ready()) return;
    auto sound=sounds_.find(name);
    if (sound==sounds_.end()) {report("AUDIO MISSING music="+name); return;}
    auto& clip=load(sound->second.first);
    const auto result=engine_->CreateSourceVoice(&music_,&clip.format);
    if (FAILED(result)) {report("AUDIO ERROR music voice HRESULT="+std::to_string(result)); return;}
    XAUDIO2_BUFFER buffer{};
    buffer.Flags=XAUDIO2_END_OF_STREAM; buffer.AudioBytes=UINT32(clip.data.size()); buffer.pAudioData=clip.data.data();
    buffer.LoopCount=loop ? XAUDIO2_LOOP_INFINITE : 0;
    music_->SetVolume(.2f*sound->second.second);
    if (FAILED(music_->SubmitSourceBuffer(&buffer)) || FAILED(music_->Start())) {report("AUDIO ERROR music playback="+name); stopMusic(); return;}
    report("AUDIO music="+name+" loop="+std::to_string(loop)+" gain="+std::to_string(.2f*sound->second.second));
    ++submitted_;
}
void WindowsAudio::stopMusic() {
    if (music_) {music_->DestroyVoice(); music_=nullptr;}
}
void WindowsAudio::stopNarration() {
    for (auto& voice:voices_) if (voice.source && voice.narration) {
        voice.source->Stop(); voice.source->FlushSourceBuffers(); voice.narration=false;
    }
}
void WindowsAudio::setMuted(bool muted) {
    if (master_) master_->SetVolume(muted ? 0.f : 1.f);
    report("AUDIO muted="+std::to_string(muted));
}
unsigned WindowsAudio::activeVoices() const {
    unsigned active=0;
    if (music_) {
        XAUDIO2_VOICE_STATE state{}; music_->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued) ++active;
    }
    for (const auto& voice:voices_) if (voice.source) {
        XAUDIO2_VOICE_STATE state{}; voice.source->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued) ++active;
    }
    return active;
}
bool WindowsAudio::musicPlaying() const {
    if (!music_) return false;
    XAUDIO2_VOICE_STATE state{}; music_->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued!=0;
}
}
