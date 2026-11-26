#pragma once
#include "raster_compositor.hpp"
#include <memory>

namespace sw2 {
// DW2MOV1: one indexed file of independent lossless WebP chunks (<=30 frames).
// frame() is serialized for IntroFrameCache workers. Its result owns its pixels;
// no returned image refers to libwebp's mutable reconstruction canvas.
class ChunkMovie {
public:
    explicit ChunkMovie(const std::filesystem::path&);
    ~ChunkMovie();
    ChunkMovie(const ChunkMovie&)=delete;
    ChunkMovie& operator=(const ChunkMovie&)=delete;
    std::uint32_t width() const;
    std::uint32_t height() const;
    std::uint32_t rate() const;
    std::uint32_t count() const;
    std::uint32_t firstSource() const;
    Raster frame(std::uint32_t index);
    struct Statistics {
        std::uint64_t decodes{},chunkLoads{},decoderResets{};
        std::uint32_t lastRequestDecodes{},maximumRequestDecodes{};
        std::uint64_t currentCompressedBytes{},maximumCompressedBytes{},outputRasterBytes{};
    };
    Statistics statistics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
