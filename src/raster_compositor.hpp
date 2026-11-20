#pragma once
#include "campaign_menu.hpp"
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
    struct Entry {Raster image,sampled;const Raster* source{};double x{-1},y{},w{},h{};unsigned used{};};
    std::map<std::filesystem::path,Entry> images_;
    std::shared_ptr<const Raster> last_;
    std::size_t signature_{},bytes_{};
    unsigned serial_{};
public:
    RasterCompositor(const MenuAssets& assets,std::function<Raster(const std::filesystem::path&)> decoder)
        :assets_(assets),decode_(std::move(decoder)) {}
    std::shared_ptr<const Raster> image(const Draw&);
};
}
