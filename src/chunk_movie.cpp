#include "chunk_movie.hpp"
#include <webp/decode.h>
#include <webp/demux.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace sw2 {
namespace {
[[noreturn]] void invalid(const char* text) {throw std::runtime_error(std::string("DW2MOV1: ")+text);}
std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;
}
std::uint64_t u64(const std::uint8_t* p) {return u32(p)|(std::uint64_t(u32(p+4))<<32);}
void read(std::ifstream& file,std::uint8_t* destination,std::size_t size) {
    if(size>std::size_t(std::numeric_limits<std::streamsize>::max()))invalid("read exceeds stream limit");
    if(!file.read(reinterpret_cast<char*>(destination),std::streamsize(size)))invalid("truncated file");
}
struct DecoderDelete {void operator()(WebPAnimDecoder* p) const {WebPAnimDecoderDelete(p);}};
struct DemuxDelete {void operator()(WebPDemuxer* p) const {WebPDemuxDelete(p);}};
}
struct ChunkMovie::Impl {
    struct Chunk {std::uint32_t first{},count{};std::uint64_t offset{},length{};};
    std::ifstream file;
    std::uint32_t width{},height{},rate{},count{},firstSource{};
    std::vector<Chunk> chunks;
    std::vector<std::uint8_t> compressed;
    // Declared after compressed: the decoder is destroyed before its bitstream.
    std::unique_ptr<WebPAnimDecoder,DecoderDelete> decoder;
    std::size_t activeChunk=std::numeric_limits<std::size_t>::max();
    std::uint32_t decodedInChunk{};
    bool staticSingleFrame{};
    std::vector<int> endTimestamps;
    const std::uint8_t* pixels{};
    mutable std::mutex mutex;
    Statistics statistics;
    // Reading telemetry on the presentation thread must not wait for a worker
    // decoding the next frame. Publish a small snapshot after each request.
    mutable std::mutex statisticsMutex;
    Statistics published;

    void publishStatistics() {
        std::lock_guard guard(statisticsMutex);published=statistics;
    }

    explicit Impl(const std::filesystem::path& path):file(path,std::ios::binary) {
        if(!file)invalid("cannot open movie");
        file.seekg(0,std::ios::end);const auto end=file.tellg();
        if(end<32)invalid("missing header");
        const auto size=std::uint64_t(end);file.seekg(0);
        std::array<std::uint8_t,32> header{};read(file,header.data(),header.size());
        const std::array<std::uint8_t,8> magic={'D','W','2','M','O','V','1',0};
        if(!std::equal(magic.begin(),magic.end(),header.begin()))invalid("invalid magic/version");
        width=u32(header.data()+8);height=u32(header.data()+12);rate=u32(header.data()+16);
        count=u32(header.data()+20);firstSource=u32(header.data()+24);
        const std::uint32_t chunkCount=u32(header.data()+28);
        // Explicit limits bound allocation before parsing or decoding input.
        if(!width||!height||width>16383||height>16383||std::uint64_t(width)*height>16777216)
            invalid("invalid canvas dimensions");
        if(rate!=30||!count||count>1000000||!chunkCount||chunkCount>count)
            invalid("invalid rate/frame/chunk count");
        if(std::uint64_t(firstSource)+count-1>std::numeric_limits<std::uint32_t>::max())invalid("source frame overflow");
        const std::uint64_t tableEnd=32+std::uint64_t(chunkCount)*24;
        if(tableEnd>size)invalid("truncated chunk index");
        chunks.reserve(chunkCount);
        std::uint64_t nextOffset=tableEnd;
        std::uint32_t nextFrame=0;
        for(std::uint32_t i=0;i<chunkCount;++i) {
            std::array<std::uint8_t,24> bytes{};read(file,bytes.data(),bytes.size());
            const Chunk c{u32(bytes.data()),u32(bytes.data()+4),u64(bytes.data()+8),u64(bytes.data()+16)};
            if(c.first!=nextFrame||!c.count||c.count>30||c.count>count-nextFrame)invalid("noncontiguous/invalid frame index");
            if(c.offset!=nextOffset||c.offset>size||c.length<20||c.length>134217728||c.length>size-c.offset)
                invalid("invalid/overlapping chunk extent");
            nextFrame+=c.count;nextOffset+=c.length;chunks.push_back(c);
        }
        if(nextFrame!=count||nextOffset!=size)invalid("incomplete frames or trailing bytes");
        statistics.outputRasterBytes=std::uint64_t(width)*height*4;
        publishStatistics();
    }
    void load(std::size_t index) {
        pixels=nullptr;decoder.reset();activeChunk=std::numeric_limits<std::size_t>::max();decodedInChunk=0;
        std::vector<std::uint8_t>().swap(compressed);
        const auto& chunk=chunks[index];
        compressed.resize(std::size_t(chunk.length));
        file.clear();file.seekg(std::streamoff(chunk.offset));
        if(!file)invalid("chunk seek failed");
        read(file,compressed.data(),compressed.size());
        const WebPData data{compressed.data(),compressed.size()};
        // Parse dimensions/count before AnimDecoder allocates its canvases.
        std::unique_ptr<WebPDemuxer,DemuxDelete> demux(WebPDemux(&data));
        if(!demux)invalid("invalid WebP chunk");
        const auto visualCount=WebPDemuxGetI(demux.get(),WEBP_FF_FRAME_COUNT);
        if(WebPDemuxGetI(demux.get(),WEBP_FF_CANVAS_WIDTH)!=width||WebPDemuxGetI(demux.get(),WEBP_FF_CANVAS_HEIGHT)!=height||
           !visualCount||visualCount>chunk.count)
            invalid("WebP canvas/frame count differs from index");
        // Identical source slots can coalesce, including an entire static chunk.
        // Source slot count belongs to the container; WebP contains visual spans.
        staticSingleFrame=visualCount==1&&!(WebPDemuxGetI(demux.get(),WEBP_FF_FORMAT_FLAGS)&ANIMATION_FLAG);
        endTimestamps.clear();endTimestamps.reserve(visualCount);
        int endTimestamp=0;
        for(std::uint32_t i=1;i<=visualCount;++i) {
            WebPIterator iterator{};
            if(!WebPDemuxGetFrame(demux.get(),int(i),&iterator))invalid("WebP frame missing");
            WebPBitstreamFeatures features{};
            const bool lossless=iterator.complete&&WebPGetFeatures(iterator.fragment.bytes,iterator.fragment.size,&features)==VP8_STATUS_OK&&features.format==2;
            const int duration=iterator.duration;
            WebPDemuxReleaseIterator(&iterator);
            if(!lossless)invalid("chunk contains non-lossless frame");
            if(staticSingleFrame) {
                if(duration!=0)invalid("invalid static WebP duration");
                endTimestamp=int((std::uint64_t(chunk.count)*1000+15)/30);
            }else {
                if(duration<=0||duration>1000)invalid("WebP frame duration differs from 30 Hz");
                endTimestamp+=duration;
                bool sourceBoundary=false;
                for(std::uint32_t slot=1;slot<=chunk.count;++slot)
                    if(endTimestamp==int((std::uint64_t(slot)*1000+15)/30)){sourceBoundary=true;break;}
                if(!sourceBoundary)invalid("WebP frame duration differs from 30 Hz");
            }
            endTimestamps.push_back(endTimestamp);
        }
        if(endTimestamp!=int((std::uint64_t(chunk.count)*1000+15)/30))invalid("WebP duration differs from source slot count");
        demux.reset();
        WebPAnimDecoderOptions options{};
        if(!WebPAnimDecoderOptionsInit(&options))invalid("libwebp decoder ABI mismatch");
        options.color_mode=MODE_RGBA;options.use_threads=0;
        decoder.reset(WebPAnimDecoderNew(&data,&options));
        if(!decoder)invalid("invalid WebP animation");
        activeChunk=index;++statistics.chunkLoads;
        statistics.currentCompressedBytes=chunk.length;
        statistics.maximumCompressedBytes=std::max(statistics.maximumCompressedBytes,chunk.length);
    }
    Raster decodeFrame(std::uint32_t index) {
        const auto it=std::upper_bound(chunks.begin(),chunks.end(),index,[](std::uint32_t value,const Chunk& c){return value<c.first;});
        const auto chunkIndex=std::size_t(std::distance(chunks.begin(),it)-1);
        if(activeChunk!=chunkIndex)load(chunkIndex);
        const auto localSlot=index-chunks[chunkIndex].first;
        // The encoder quantizes source boundaries with round(), not floor().
        const int sourceTime=int((std::uint64_t(localSlot)*1000+15)/30);
        const auto localVisual=std::uint32_t(std::distance(endTimestamps.begin(),std::upper_bound(endTimestamps.begin(),endTimestamps.end(),sourceTime)));
        if(localVisual>=endTimestamps.size())invalid("source slot outside visual spans");
        if(decodedInChunk>localVisual+1) {
            WebPAnimDecoderReset(decoder.get());decodedInChunk=0;pixels=nullptr;++statistics.decoderResets;
        }
        statistics.lastRequestDecodes=0;
        while(decodedInChunk<=localVisual) {
            std::uint8_t* canvas=nullptr;int timestamp=0;
            if(!WebPAnimDecoderGetNext(decoder.get(),&canvas,&timestamp)||!canvas)invalid("WebP frame decode failed");
            ++decodedInChunk;++statistics.decodes;++statistics.lastRequestDecodes;
            const int expectedTimestamp=endTimestamps[decodedInChunk-1];
            if(timestamp!=(staticSingleFrame?0:expectedTimestamp))invalid("WebP frame duration differs from 30 Hz");
            pixels=canvas;
        }
        if(statistics.lastRequestDecodes>30)invalid("chunk seek exceeded 30 frame decodes");
        statistics.maximumRequestDecodes=std::max(statistics.maximumRequestDecodes,statistics.lastRequestDecodes);
        Raster result;result.width=int(width);result.height=int(height);
        result.rgba.assign(pixels,pixels+std::size_t(statistics.outputRasterBytes));
        return result;
    }
    Raster frame(std::uint32_t index) {
        std::lock_guard guard(mutex);
        if(index>=count)throw std::out_of_range("DW2MOV1: frame index out of range");
        try {auto result=decodeFrame(index);publishStatistics();return result;}
        catch(...) {
            pixels=nullptr;decoder.reset();activeChunk=std::numeric_limits<std::size_t>::max();decodedInChunk=0;
            std::vector<std::uint8_t>().swap(compressed);statistics.currentCompressedBytes=0;
            publishStatistics();
            throw;
        }
    }
};
ChunkMovie::ChunkMovie(const std::filesystem::path& path):impl_(std::make_unique<Impl>(path)){}
ChunkMovie::~ChunkMovie()=default;
std::uint32_t ChunkMovie::width() const {return impl_->width;}
std::uint32_t ChunkMovie::height() const {return impl_->height;}
std::uint32_t ChunkMovie::rate() const {return impl_->rate;}
std::uint32_t ChunkMovie::count() const {return impl_->count;}
std::uint32_t ChunkMovie::firstSource() const {return impl_->firstSource;}
Raster ChunkMovie::frame(std::uint32_t index) {return impl_->frame(index);}
ChunkMovie::Statistics ChunkMovie::statistics() const {std::lock_guard guard(impl_->statisticsMutex);return impl_->published;}
}
