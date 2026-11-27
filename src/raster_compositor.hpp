#pragma once
#include "campaign_menu.hpp"
#include <array>
#include <functional>

namespace sw2 {
struct Raster {
    int width{},height{};
    std::vector<std::uint8_t> rgba;
};
void blendPixel(std::uint8_t* destination,const std::uint8_t* source,Draw::Blend);
class RasterCompositor {
    const MenuAssets& assets_;
    std::function<Raster(const std::filesystem::path&)> decode_;
    struct Entry {Raster image;unsigned used{};};
    using SampleKey=std::pair<std::filesystem::path,std::array<double,8>>;
    struct Span {int begin{},end{};bool opaque{};};
    struct Sample {
        Raster image;
        std::vector<Span> spans;
        std::vector<std::size_t> rows;
        unsigned used{};
        std::size_t storageBytes() const {
            return image.rgba.size()+spans.capacity()*sizeof(Span)+rows.capacity()*sizeof(std::size_t);
        }
    };
    std::map<std::filesystem::path,Entry> images_;
    std::map<SampleKey,Sample> samples_;
    std::shared_ptr<const Raster> last_;
    std::size_t signature_{},bytes_{};
    unsigned serial_{};
    std::size_t sampleBuilds_{};
public:
    RasterCompositor(const MenuAssets& assets,std::function<Raster(const std::filesystem::path&)> decoder)
        :assets_(assets),decode_(std::move(decoder)) {}
    std::shared_ptr<const Raster> image(const Draw&);
    std::size_t sampleBuilds() const {return sampleBuilds_;}
};
}
