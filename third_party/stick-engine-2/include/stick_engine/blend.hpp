#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace stick {
enum class Blend { normal = 0, add = 1, overlay = 2, screen = 3 };

// Composite one straight-alpha RGBA8 sample. Additive layers accumulate
// premultiplied color and alpha, as in Flash BitmapData.draw.
inline void blend_rgba(std::uint8_t* destination, const std::uint8_t* source, Blend mode) {
    const int alpha = source[3];
    if (!alpha) return;
    if (!destination[3] || (mode == Blend::normal && alpha == 255)) {
        std::copy_n(source, 4, destination);
        return;
    }
    if (destination[3] == 255) {
        for (int c = 0; c < 3; ++c) {
            const int s = source[c], d = destination[c];
            if (mode == Blend::add) {
                destination[c] = static_cast<std::uint8_t>(std::min(255, d + (s * alpha + 127) / 255));
                continue;
            }
            int color = s;
            if (mode == Blend::overlay) {
                color = d <= 127 ? (2 * s * d + 127) / 255 : 255 - (2 * (255 - s) * (255 - d) + 127) / 255;
            } else if (mode == Blend::screen) {
                color = 255 - ((255 - s) * (255 - d) + 127) / 255;
            }
            destination[c] = static_cast<std::uint8_t>((color * alpha + d * (255 - alpha) + 127) / 255);
        }
        return;
    }
    const double a = alpha / 255., b = destination[3] / 255.;
    const double out = mode == Blend::add ? std::min(1., a + b) : a + b * (1 - a);
    for (int c = 0; c < 3; ++c) {
        const double s = source[c] / 255., d = destination[c] / 255.;
        double value;
        if (mode == Blend::add) {
            value = std::min(1., s * a + d * b);
        } else {
            double color = s;
            if (mode == Blend::overlay) color = d <= .5 ? 2 * s * d : 1 - 2 * (1 - s) * (1 - d);
            else if (mode == Blend::screen) color = 1 - (1 - s) * (1 - d);
            value = a * (1 - b) * s + a * b * color + (1 - a) * b * d;
        }
        destination[c] = static_cast<std::uint8_t>(std::clamp(std::lround(255 * value / out), 0l, 255l));
    }
    destination[3] = static_cast<std::uint8_t>(std::clamp(std::lround(255 * out), 0l, 255l));
}
}
