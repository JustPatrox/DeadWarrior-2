// Windows CLI verification of the actual packed intro through the product codec.
#include "intro_movie.hpp"
#include "windows_render.hpp"
#include <bcrypt.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
using sw2::IntroMovie;
namespace {
void require(bool value,const std::string& reason) {if(!value)throw std::runtime_error(reason);}
double ms(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
struct GdiStartup {
    ULONG_PTR token{};
    GdiStartup() {Gdiplus::GdiplusStartupInput options;require(Gdiplus::GdiplusStartup(&token,&options,nullptr)==Gdiplus::Ok,"GDI+ startup failed");}
    ~GdiStartup(){if(token)Gdiplus::GdiplusShutdown(token);}
};
struct Sha256 {
    BCRYPT_ALG_HANDLE algorithm{};
    std::vector<UCHAR> object;
    Sha256() {
        require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider failed");
        DWORD length{},copied{};
        require(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&copied,0)>=0,"SHA256 property failed");
        object.resize(length);
    }
    ~Sha256(){if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    std::string hash(const std::vector<std::uint8_t>& data) {
        BCRYPT_HASH_HANDLE handle{};
        require(data.size()<=std::numeric_limits<ULONG>::max(),"SHA256 input is too large");
        require(BCryptCreateHash(algorithm,&handle,object.data(),ULONG(object.size()),nullptr,0,0)>=0,"SHA256 initialization failed");
        const NTSTATUS added=BCryptHashData(handle,const_cast<PUCHAR>(data.data()),ULONG(data.size()),0);
        std::array<UCHAR,32> digest{};
        const NTSTATUS finished=added>=0?BCryptFinishHash(handle,digest.data(),ULONG(digest.size()),0):added;
        BCryptDestroyHash(handle);require(added>=0&&finished>=0,"SHA256 calculation failed");
        std::ostringstream out;out<<std::hex<<std::setfill('0');
        for(unsigned char byte:digest)out<<std::setw(2)<<unsigned(byte);
        return out.str();
    }
};
std::map<int,std::string> hashes(const fs::path& path) {
    std::ifstream input(path);require(bool(input),"Frame hash manifest missing");
    const std::string text((std::istreambuf_iterator<char>(input)),{});
    const std::regex field("\"([0-9]+)\"\\s*:\\s*\"([a-f0-9]{64})\"");
    std::map<int,std::string> result;
    for(auto it=std::sregex_iterator(text.begin(),text.end(),field);it!=std::sregex_iterator();++it)
        require(result.emplace(std::stoi((*it)[1]),(*it)[2]).second,"Duplicate frame hash");
    return result;
}
struct Row {int source{};char kind{};fs::path path;};
std::vector<Row> frames(const fs::path& directory) {
    std::ifstream input(directory/"frames.tsv");std::string title,music;int version{},width{},height{},fps{},count{},first{},last{};
    require(bool(input>>title>>version>>width>>height>>fps>>count>>first>>last>>music),"Invalid frame table");
    require(title=="DeadWarrior2-Intro"&&version==1&&width==1280&&height==720&&fps==30&&count==3233&&first==3&&last==3235,"Unexpected full intro header");
    std::vector<Row> result;Row row;std::string path;
    while(input>>row.source>>row.kind>>path) {row.path=directory/path;result.push_back(row);}
    require(input.eof()&&result.size()==3233,"Invalid complete frame table");
    return result;
}
double percentile(std::vector<double> values,double fraction) {
    require(!values.empty(),"Missing timing data");std::sort(values.begin(),values.end());
    return values[std::min(values.size()-1,size_t(fraction*double(values.size()-1)))];
}
}
int main(int argc,char** argv) {
    try {
        const fs::path directory=fs::absolute(argc>1?argv[1]:"assets/intro");
        const fs::path output=fs::absolute(argc>2?argv[2]:"work/intro-asset-check");fs::create_directories(output);
        const bool prefetch=argc>3&&std::string(argv[3])=="--prefetch";
        const auto expected=hashes(directory/"frame-hashes.json");const auto table=frames(directory);
        require(expected.size()==3233,"Expected 3233 source-frame hashes");
        int firstDelta=-1,longest=0,run=0,longestEnd=0,keyAfterDelta=-1;
        for(size_t i=0;i<table.size();++i) {
            require(table[i].source==3+int(i)&&expected.contains(table[i].source),"Source frame sequence differs");
            if(table[i].kind=='D') {if(firstDelta<0)firstDelta=int(i);if(++run>longest){longest=run;longestEnd=int(i);}}
            else {require(table[i].kind=='K',"Unexpected frame codec");if(run&&keyAfterDelta<0)keyAfterDelta=int(i);run=0;}
        }
        require(firstDelta>0&&longest>0&&longest<=29&&keyAfterDelta>=0,"Expected bounded delta/key interval not found");
        std::vector<std::pair<int,fs::path>> events;
        std::ifstream eventFile(directory/"events.tsv");int eventFrame{};std::string eventName;
        while(eventFile>>eventFrame>>eventName)events.emplace_back(eventFrame,(directory/eventName).lexically_normal());
        require(eventFile.eof()&&events.size()==19,"Expected all nineteen sound events");
        require(events[9].first==2634&&events[10].first==2634&&events[9].second.filename()=="1458.wav"&&events[10].second.filename()=="1459.wav","Original double sound event order differs");
        for(const auto& event:events)require(event.second.extension()==".wav"&&event.second.parent_path()==directory/"audio"&&fs::is_regular_file(event.second),"Invalid event type/path");

        GdiStartup gdi;Sha256 sha;
        size_t decoded=0,maxRaster=0;std::mutex decodeMutex;
        std::vector<double> decodeTimes,advanceTimes;
        IntroMovie movie(directory,[&](const fs::path& path) {
            const auto begin=Clock::now();auto raster=loadWindowsPng(path);
            std::lock_guard guard(decodeMutex);decodeTimes.push_back(ms(begin));
            ++decoded;maxRaster=std::max(maxRaster,raster.rgba.size());return raster;
        },prefetch);
        require(movie.available()&&std::abs(movie.duration()-3233/30.)<1e-12,"Native movie duration differs");
        require(movie.music()==directory/"music.wav"&&fs::is_regular_file(movie.music()),"Movie music path differs");
        const auto began=Clock::now();movie.start();
        std::vector<std::pair<int,fs::path>> played;
        std::ofstream actual(output/"all-frames.json");actual<<"{\n";
        for(int i=0;i<3233;++i) {
            const double seconds=(i+.25)/30;
            const auto before=Clock::now();movie.advance(seconds);advanceTimes.push_back(ms(before));
            require(movie.sourceFrame()==i+3&&!movie.finished(),"Native frame clock/early finish differs at "+std::to_string(i));
            const auto pixels=movie.pixels();require(pixels&&pixels->width==1280&&pixels->height==720&&pixels->rgba.size()==3686400,"Native pixel dimensions differ");
            const std::string digest=sha.hash(pixels->rgba);
            require(digest==expected.at(i+3),"Actual native RGBA differs at source frame "+std::to_string(i+3));
            if(i)actual<<",\n";actual<<"  \""<<i+3<<"\": \""<<digest<<'"';
            for(const auto& path:movie.takeEffects())played.emplace_back(i+3,path.lexically_normal());
            const auto beforeRepeat=movie.statistics().decodes;const auto drawing=movie.draw({0,0,1920,1080});
            require(drawing.pixels==pixels,"Drawing replaced the decoded frame");
            movie.advance(seconds);require(movie.pixels()==pixels&&movie.statistics().decodes==beforeRepeat&&movie.takeEffects().empty(),"Repeated same-frame render/advance decoded or replayed audio");
            if(i%300==0)std::cout<<"Verified source frame "<<i+3<<" / 3235\n"<<std::flush;
        }
        actual<<"\n}\n";actual.close();
        require(played==events,"Native event frame/path/order differs");
        require(movie.statistics().decodes==3233,"Sequential movie did not reconstruct exactly once per frame");
        const double sequentialMs=ms(began);const auto sequentialDecodes=movie.statistics().decodes;
        movie.advance(movie.duration());require(movie.finished()&&movie.sourceFrame()==3235&&movie.statistics().decodes==sequentialDecodes,"Native finish at 3233/30 differs");

        const std::vector<int> seeks={firstDelta,firstDelta-1,firstDelta+1,longestEnd,keyAfterDelta,2700,0,3232,2631,1000,2500,10};
        size_t maxSeekDecodes=0;
        for(int index:seeks) {
            const auto before=movie.statistics().decodes;movie.seek((index+.25)/30.);
            const size_t count=std::size_t(movie.statistics().decodes-before);maxSeekDecodes=std::max(maxSeekDecodes,count);
            require(count<=30&&movie.sourceFrame()==index+3&&sha.hash(movie.pixels()->rgba)==expected.at(index+3),"Backward/random seek reconstruction differs");
            require(movie.takeEffects().empty(),"Seek replayed historical sound events");
            if(index==longestEnd)require(count==std::size_t(longest+1),"Longest delta seek did not use its key plus delta chain");
            if(index==keyAfterDelta)require(count==1,"Key transition seek decoded preceding frames");
        }
        movie.seek((2633-3+.25)/30.);require(movie.sourceFrame()==2633,"Pre-event seek frame differs");
        movie.advance((2634-3+.25)/30.);auto pair=movie.takeEffects();
        require(pair.size()==2&&pair[0].filename()=="1458.wav"&&pair[1].filename()=="1459.wav","Same-frame sound ordering differs after seek");
        movie.stop();require(!movie.pixels()&&!movie.finished()&&movie.takeEffects().empty(),"Stop retained movie state");
        movie.start();movie.advance(movie.duration());pair=movie.takeEffects();
        require(pair.size()==events.size()&&movie.finished(),"Restart/clock catch-up dropped events");
        for(size_t i=0;i<pair.size();++i)require(pair[i].lexically_normal()==events[i].second,"Restart reordered sound events");

        std::lock_guard decodeGuard(decodeMutex);
        std::ofstream report(output/"checks.json");report<<std::setprecision(12)<<std::boolalpha;
        report<<"{\n  \"sourceFrames\":3233,\n  \"firstSourceFrame\":3,\n  \"lastSourceFrame\":3235,\n"
              <<"  \"allRgbaHashesMatch\":true,\n  \"sequentialDecodeCount\":"<<sequentialDecodes<<",\n"
              <<"  \"prefetchEnabled\":"<<prefetch<<",\n  \"configuredCacheFrames\":"<<(prefetch?8:0)<<",\n  \"actualDecoderCallsAtReport\":"<<decoded<<",\n"
              <<"  \"backwardAndRandomSeeks\":"<<seeks.size()<<",\n  \"maximumSeekDecodeCount\":"<<maxSeekDecodes<<",\n"
              <<"  \"maximumConsecutiveDeltas\":"<<longest<<",\n  \"firstDeltaSourceFrame\":"<<table[std::size_t(firstDelta)].source<<",\n"
              <<"  \"sameFramePointerAndDecodeCountStable\":true,\n  \"eventCount\":19,\n  \"eventFramePathAndOrderMatch\":true,\n"
              <<"  \"doubleEvent2634OrderMatch\":true,\n  \"finishedAt3233Over30\":true,\n  \"restartAndCatchUpMatch\":true,\n"
              <<"  \"sequentialVerificationMilliseconds\":"<<sequentialMs<<",\n  \"pngDecodeP50Milliseconds\":"<<percentile(decodeTimes,.5)<<",\n"
              <<"  \"pngDecodeP95Milliseconds\":"<<percentile(decodeTimes,.95)<<",\n  \"pngDecodeMaxMilliseconds\":"<<percentile(decodeTimes,1)<<",\n"
              <<"  \"advanceP95Milliseconds\":"<<percentile(advanceTimes,.95)<<",\n  \"rasterBytes\":"<<maxRaster<<",\n"
              <<"  \"forwardDeltaWorkingRasterUpperBound\":3,\n  \"forwardDeltaWorkingPixelBytesUpperBound\":"<<maxRaster*3<<",\n"
              <<"  \"measurementScope\":\"Prepared CPU PNG decode/copy/XOR only; hash verification timing is separate from decode percentiles. Not game FPS or a universal performance claim.\",\n"
              <<"  \"memoryScope\":\"Three movie working pixel vectors; optional prefetch holds up to eight additional decoded/in-flight frames. Excludes codec scratch, metadata, GPU copies and extra shared consumers. Sequential/seek decode counts above are reconstruction requests, not worker I/O calls.\"\n}\n";
        require(bool(report),"Cannot write intro verification report");
        std::cout<<"All 3233 native RGBA hashes, bounded seeks, nineteen events and finish clock passed\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
