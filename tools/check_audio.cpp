#include "windows_audio.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <wrl/client.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cmath>

struct TemporaryTone {
    std::filesystem::path file=std::filesystem::temp_directory_path()/
        ("DeadWarrior2-audio-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+".wav");
    static constexpr unsigned rate=22050;
    TemporaryTone() {
        std::ofstream out(file,std::ios::binary);
        auto integer=[&](unsigned value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(8*i)));};
        const unsigned samples=rate*8,bytes=samples*2;
        out.write("RIFF",4);integer(36+bytes,4);out.write("WAVEfmt ",8);integer(16,4);
        integer(1,2);integer(1,2);integer(rate,4);integer(rate*2,4);integer(2,2);integer(16,2);
        out.write("data",4);integer(bytes,4);
        for(unsigned i=0;i<samples;++i)integer(unsigned(short(4096*std::sin(i*440.*6.283185307179586/rate))),2);
        if(!out)throw std::runtime_error("Cannot prepare temporary PCM clock fixture");
    }
    ~TemporaryTone(){std::error_code error;std::filesystem::remove(file,error);}
};

void movieFileProbe(const std::filesystem::path& root) {
    TemporaryTone tone;
    sw2::WindowsAudio movie(root/"audio");movie.setMuted(true);
    movie.musicFile(tone.file);Sleep(140);
    const auto before=movie.musicSamplesPlayed();const double seconds=movie.musicSeconds();const auto after=movie.musicSamplesPlayed();
    if(!movie.musicPlaying() || before==0 || seconds<double(before)/tone.rate || seconds>double(after)/tone.rate)
        throw std::runtime_error("File music clock did not use its native PCM sample rate");
    movie.musicFile(tone.file);
    if(movie.musicSeconds()>=seconds)throw std::runtime_error("Explicit musicFile replay retained the previous playhead");
    Sleep(80);movie.setPaused(true);Sleep(60);
    const double paused=movie.musicSeconds();Sleep(180);
    if(movie.musicSeconds()!=paused)throw std::runtime_error("Cinematic file music advanced while paused");
    movie.setPaused(false);Sleep(120);
    if(movie.musicSeconds()<=paused)throw std::runtime_error("Cinematic file music failed to resume");
    movie.effect("OrderVictory",0,false);movie.narration(tone.file);movie.effectFile(tone.file);
    if(movie.activeVoices()!=4)throw std::runtime_error("Cinematic, UI, narration and music voices did not overlap");
    movie.stopMovieEffects();
    // XAudio2 applies Stop/Flush on its processing pass; queued-buffer state
    // can still include the canceled voice immediately after the API returns.
    Sleep(80);
    std::cout<<"Cinematic cleanup retained "<<movie.activeVoices()<<" voices, music "<<movie.musicPlaying()<<'\n';
    if(movie.activeVoices()!=3 || !movie.musicPlaying())throw std::runtime_error("Stopping movie effects cut another audio category");
    movie.stopMovieEffects();
    if(movie.activeVoices()!=3)throw std::runtime_error("Stopping movie effects was not idempotent");
    movie.stopNarration();Sleep(80);
    if(movie.activeVoices()!=2)throw std::runtime_error("Narration cleanup did not leave music and the regular effect");
    movie.setPaused(true);movie.musicFile(tone.file);Sleep(100);
    if(movie.musicSeconds()!=0)throw std::runtime_error("File music started advancing in an already paused client");
    movie.setPaused(false);Sleep(100);
    if(movie.musicSeconds()<=0)throw std::runtime_error("Initially paused file music did not start on resume");
    movie.stopMusic();
    if(movie.musicPlaying() || movie.musicSeconds()!=0)throw std::runtime_error("Stopped file music retained an active clock");
    std::cout<<"File music native22050 clock/restart/pause/resume and selective cinematic FX cleanup passed\n";
}

using Microsoft::WRL::ComPtr;
float ownPeak(DWORD wanted=GetCurrentProcessId()) {
    ComPtr<IMMDeviceEnumerator> devices;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioSessionManager2> sessions;
    ComPtr<IAudioSessionEnumerator> list;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,
                               IID_PPV_ARGS(devices.GetAddressOf())))) return 0;
    if (FAILED(devices->GetDefaultAudioEndpoint(eRender,eConsole,&device))) return 0;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,
                               reinterpret_cast<void**>(sessions.GetAddressOf())))) return 0;
    if (FAILED(sessions->GetSessionEnumerator(&list))) return 0;
    int count=0; list->GetCount(&count);
    for (int i=0;i<count;++i) {
        ComPtr<IAudioSessionControl> control;
        ComPtr<IAudioSessionControl2> details;
        ComPtr<IAudioMeterInformation> meter;
        DWORD process=0;
        if (SUCCEEDED(list->GetSession(i,&control)) && SUCCEEDED(control.As(&details)) &&
            SUCCEEDED(details->GetProcessId(&process)) && process==wanted && SUCCEEDED(control.As(&meter))) {
            float peak=0; meter->GetPeakValue(&peak); return peak;
        }
    }
    return 0;
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--movie-files-only") {movieFileProbe(argv[2]);return 0;}
        if (argc==3 && std::string(argv[1])=="--process-peak") {
            CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            const DWORD process=DWORD(std::stoul(argv[2])); float peak=0;
            for (int i=0;i<100;++i) {Sleep(20); peak=std::max(peak,ownPeak(process));}
            std::cout<<"Native menu music-only process "<<process<<" peak "<<peak<<'\n';
            CoUninitialize(); return peak>.04 ? 0 : 1;
        }
        const auto root=std::filesystem::path(argc>1 ? argv[1] : "assets");
        sw2::WindowsAudio audio(root/"audio");
        if (!audio.ready()) throw std::runtime_error("No audio rendering device");
        audio.music("loginMusic");
        float menuPeak=0;
        for (int i=0;i<50;++i) {Sleep(20); menuPeak=std::max(menuPeak,ownPeak());}
        if (menuPeak<=.04) throw std::runtime_error("Main-menu music alone remained inaudible");
        const auto musicPosition=audio.musicSamplesPlayed();
        audio.music("loginMusic");
        if(!musicPosition||audio.musicSamplesPlayed()<musicPosition||audio.submitted()!=1)
            throw std::runtime_error("Requesting the current track reset its playhead");
        std::cout<<"Main-menu music-only process peak "<<menuPeak<<'\n';
        audio.narration(root/"tutorial/voiceTutorial1.wav");
        audio.effect("CommandMove1",0,false);
        audio.effect("hitOnFlesh1",-.5);
        if (audio.submitted()!=4 || audio.activeVoices()<3) throw std::runtime_error("Music, narration and effects failed to overlap");
        float peak=0;
        for (int i=0;i<30;++i) {Sleep(20); peak=std::max(peak,ownPeak());}
        if (peak<=.001) throw std::runtime_error("The process audio session remained silent");
        audio.setMuted(true); Sleep(200);
        const float muted=ownPeak();
        if (muted>.001) throw std::runtime_error("Mute left the effects audible");
        std::cout<<"Submitted "<<audio.submitted()<<", active voices "<<audio.activeVoices()
                 <<", process peak "<<peak<<", muted peak "<<muted<<'\n';
        audio.setPaused(true);Sleep(60);
        const auto pausedPosition=audio.narrationSamplesPlayed();const auto backgroundPosition=audio.musicSamplesPlayed();Sleep(200);
        if (audio.narrationSamplesPlayed()!=pausedPosition) throw std::runtime_error("Narration advanced during pause");
        if (audio.musicSamplesPlayed()<=backgroundPosition) throw std::runtime_error("Named background music stopped during gameplay pause");
        audio.setPaused(false);Sleep(120);
        if (audio.narrationSamplesPlayed()<=pausedPosition) throw std::runtime_error("Narration did not resume");
        audio.setMuted(false);audio.setSoundEnabled(false);audio.setMusicEnabled(false);Sleep(150);
        if (ownPeak()>.001) throw std::runtime_error("Independent sound/music switches left audible output");
        audio.setMusicEnabled(true);Sleep(180);
        if (ownPeak()<=.001) throw std::runtime_error("Music switch did not restore output");
        audio.setSoundEnabled(true);audio.setMuted(true);
        std::cout<<"Narration pause/resume and independent sound/music switches passed\n";
        movieFileProbe(root);
        std::ifstream manifest(root/"audio/sounds.tsv");
        std::string name,file; float gain; unsigned decoded=0;
        while (manifest>>name>>file>>gain) {
            const auto before=audio.submitted(); audio.effect(name,0,false);
            if (audio.submitted()!=before+1) throw std::runtime_error("Original audio did not submit: "+name);
            ++decoded;
        }
        if (decoded!=187) throw std::runtime_error("Original SoundLoader registry is incomplete");
        audio.music("OrderVictory"); Sleep(23000);
        if (!audio.musicPlaying()) throw std::runtime_error("Victory music stopped after its first loop");
        std::cout<<"Decoded "<<decoded<<" original sounds; victory music continued past 21.94 seconds\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
