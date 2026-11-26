#include "intro_movie.hpp"
#include "intro_frame_cache.hpp"
#include "chunk_movie.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace sw2 {
std::filesystem::path IntroMovie::relativeFile(const std::string& name) const {
    const std::filesystem::path file(name);
    if(file.empty() || file.has_root_path())throw std::runtime_error("Invalid intro resource path");
    for(const auto& part:file)if(part=="..")throw std::runtime_error("Invalid intro resource traversal");
    return directory_/file;
}
IntroMovie::IntroMovie(const std::filesystem::path& directory,Decoder decoder,bool prefetch)
    :prefetchEnabled_(prefetch),directory_(directory),decode_(std::move(decoder)) {
    std::ifstream input(directory/"frames.tsv");
    if(!input)return; // Older prepared resource sets can still enter the campaign.
    std::string title,music;int version{},count{},last{};
    if(!(input>>title>>version) || title!="DeadWarrior2-Intro" || (version!=1&&version!=2) ||
       !(input>>width_>>height_>>rate_>>count>>first_>>last) ||
       width_<1 || height_<1 || width_>4096 || height_>4096 || rate_!=30 ||
       count<1 || count>20000 || first_<1 || last-first_+1!=count || !(input>>music))
        throw std::runtime_error("Invalid intro frame header");
    music_=relativeFile(music);
    if(version==2) {
        std::string file;if(!(input>>file))throw std::runtime_error("Missing intro animation container");
        auto movie=std::make_shared<ChunkMovie>(relativeFile(file));
        if(movie->width()!=std::uint32_t(width_)||movie->height()!=std::uint32_t(height_)||movie->rate()!=std::uint32_t(rate_)||
           movie->count()!=std::uint32_t(count)||movie->firstSource()!=std::uint32_t(first_))
            throw std::runtime_error("Intro animation container differs from its manifest");
        chunkMovie_=movie;
        // Virtual numeric keys keep the existing bounded frame-cache contract;
        // no individual image files are opened by this decoder.
        decode_=[movie](const std::filesystem::path& key){return movie->frame(std::uint32_t(std::stoul(key.filename().string())));};
        for(int i=0;i<count;++i)frames_.push_back({true,directory_/std::to_string(i),i});
    } else {
        int key=-1;
        for(int i=0;i<count;++i) {
            int source{};char kind{};std::string file;
            if(!(input>>source>>kind>>file) || source!=first_+i || (kind!='K' && kind!='D'))
                throw std::runtime_error("Invalid intro frame row");
            if(kind=='K')key=i;
            if(key<0 || i-key>=30)throw std::runtime_error("Intro key frame interval exceeds its bound");
            frames_.push_back({kind=='K',relativeFile(file),key});
        }
    }
    std::string trailing;if(input>>trailing)throw std::runtime_error("Unexpected intro frame data");
    std::ifstream events(directory/"events.tsv");
    int source{};std::string file;int previous=first_;
    while(events>>source>>file) {
        if(source<previous || source<first_ || source>last)throw std::runtime_error("Invalid intro sound event order");
        effects_.push_back({source-first_,relativeFile(file)});previous=source;
    }
    if(events.is_open() && !events.eof())throw std::runtime_error("Invalid intro sound event row");
}
IntroMovie::~IntroMovie()=default;
IntroMovie::Statistics IntroMovie::statistics() const {
    auto result=statistics_;
    if(chunkMovie_) {
        const auto codec=chunkMovie_->statistics();
        result.codecDecodes=codec.decodes;result.codecChunkLoads=codec.chunkLoads;
        result.maximumCodecRequestDecodes=codec.maximumRequestDecodes;
        result.maximumCompressedChunkBytes=codec.maximumCompressedBytes;
    }
    return result;
}
void IntroMovie::decodeFrame(int target) {
    if(target==current_ && pixels_)return;
    const auto start=std::chrono::steady_clock::now();
    const double previousDecode=statistics_.decodeMilliseconds;
    auto canvas=std::make_shared<Raster>();
    int begin=target;
    if(pixels_ && target>current_ && frames_[target].keyIndex<=current_) {
        *canvas=*pixels_;begin=current_+1;
    } else begin=frames_[target].keyIndex;
    for(int i=begin;i<=target;++i) {
        const auto decodeStart=std::chrono::steady_clock::now();
        Raster source=frameCache_ ? frameCache_->get(std::size_t(i)) : decode_(frames_[i].file);
        statistics_.decodeMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-decodeStart).count();
        ++statistics_.decodes;
        if(source.width!=width_ || source.height!=height_ || source.rgba.size()!=std::size_t(width_)*height_*4)
            throw std::runtime_error("Intro frame size differs from its header");
        if(frames_[i].key) *canvas=std::move(source);
        else {
            if(canvas->rgba.size()!=source.rgba.size())throw std::runtime_error("Intro delta has no preceding key frame");
            for(std::size_t p=0;p<source.rgba.size();p+=4) {
                for(int channel=0;channel<3;++channel)canvas->rgba[p+channel]^=source.rgba[p+channel];
                canvas->rgba[p+3]=255;
            }
        }
    }
    pixels_=std::move(canvas);current_=target;
    statistics_.composeMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()-(statistics_.decodeMilliseconds-previousDecode);
}
void IntroMovie::start() {
    stop();if(!available())return;
    if(prefetchEnabled_&&!frameCache_) {
        std::vector<std::filesystem::path> files;files.reserve(frames_.size());
        for(const auto& frame:frames_)files.push_back(frame.file);
        frameCache_=std::make_unique<IntroFrameCache>(std::move(files),decode_,8);
    }
    finished_=false;decodeFrame(0);
}
void IntroMovie::stop() {
    if(frameCache_)frameCache_->clear();
    pixels_.reset();current_=-1;elapsed_=0;nextEffect_=0;pendingEffects_.clear();finished_=false;
}
void IntroMovie::advance(double seconds) {
    if(!available() || finished_)return;
    if(!std::isfinite(seconds) || seconds<0)throw std::runtime_error("Invalid intro presentation clock");
    elapsed_=std::max(elapsed_,seconds);
    const int frame=std::min(int(frames_.size())-1,int(std::min(elapsed_,duration())*rate_));
    decodeFrame(frame);
    while(nextEffect_<effects_.size() && effects_[nextEffect_].frame<=frame)
        pendingEffects_.push_back(effects_[nextEffect_++].file);
    finished_=elapsed_>=duration();
}
void IntroMovie::seek(double seconds) {
    if(!available())return;
    if(!std::isfinite(seconds) || seconds<0)throw std::runtime_error("Invalid intro seek time");
    pixels_.reset();current_=-1;elapsed_=0;finished_=false;pendingEffects_.clear();
    elapsed_=std::min(seconds,duration());
    const int frame=std::min(int(frames_.size())-1,int(elapsed_*rate_));
    decodeFrame(frame);
    nextEffect_=std::size_t(std::upper_bound(effects_.begin(),effects_.end(),frame,
        [](int f,const Effect& e){return f<e.frame;})-effects_.begin());
    finished_=seconds>=duration();
}
std::vector<std::filesystem::path> IntroMovie::takeEffects() {
    auto result=std::move(pendingEffects_);pendingEffects_.clear();return result;
}
Draw IntroMovie::draw(Rect viewport) const {
    Draw result;if(!pixels_)return result;
    const double size=std::min(viewport.w/width_,viewport.h/height_);
    result.target={viewport.x+(viewport.w-width_*size)/2,viewport.y+(viewport.h-height_*size)/2,width_*size,height_*size};
    result.pixels=pixels_;return result;
}
}
