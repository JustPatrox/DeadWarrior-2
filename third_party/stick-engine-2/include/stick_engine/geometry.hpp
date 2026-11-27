#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace stick {
struct Vec2 {
    double x{}, y{};
};
using Point = Vec2;
struct PixelSize {
    int width{}, height{};
};
struct Rect {
    double x{}, y{}, w{}, h{};
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    bool intersects(Rect r) const {
        return x < r.x + r.w && x + w > r.x && y < r.y + r.h && y + h > r.y;
    }
};
inline bool finite(double v) { return std::isfinite(v); }
inline bool valid(Rect r) {
    return finite(r.x) && finite(r.y) && finite(r.w) && finite(r.h) && r.w > 0 && r.h > 0;
}
struct Matrix {
    double a{1}, b{}, c{}, d{1}, x{}, y{};
    Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + x, b * p.x + d * p.y + y}; }
    Matrix operator*(Matrix m) const {
        return {a * m.a + c * m.b, b * m.a + d * m.b,     a * m.c + c * m.d,
                b * m.c + d * m.d, a * m.x + c * m.y + x, b * m.x + d * m.y + y};
    }
    Matrix inverse() const {
        const double det = a * d - b * c;
        if (!finite(det) || std::abs(det) < 1e-12)
            throw std::invalid_argument("Singular transform");
        return {d / det, -b / det, -c / det, a / det, (c * y - d * x) / det, (b * x - a * y) / det};
    }
    Rect bounds(Rect r) const {
        Vec2 p[4] = {apply({r.x, r.y}), apply({r.x + r.w, r.y}), apply({r.x + r.w, r.y + r.h}),
                     apply({r.x, r.y + r.h})};
        double l = p[0].x, t = p[0].y, rr = l, bb = t;
        for (auto q : p) {
            l = std::min(l, q.x);
            t = std::min(t, q.y);
            rr = std::max(rr, q.x);
            bb = std::max(bb, q.y);
        }
        return {l, t, rr - l, bb - t};
    }
};
struct Transform {
    Vec2 position{}, scale{1, 1};
    double rotation{};
    Matrix matrix() const {
        if (!finite(position.x) || !finite(position.y) || !finite(scale.x) || !finite(scale.y) ||
            !finite(rotation))
            throw std::invalid_argument("Nonfinite transform");
        const double co = std::cos(rotation), si = std::sin(rotation);
        return {co * scale.x, si * scale.x, -si * scale.y, co * scale.y, position.x, position.y};
    }
};
class Viewport {
    Vec2 logical_;
    PixelSize pixels_;
    double scale_{};
    Vec2 offset_{};

  public:
    Viewport(Vec2 logical, PixelSize pixels) : logical_(logical), pixels_(pixels) {
        if (!finite(logical.x) || !finite(logical.y) || logical.x <= 0 || logical.y <= 0 ||
            pixels.width <= 0 || pixels.height <= 0)
            throw std::invalid_argument("Invalid viewport");
        scale_ = std::min(pixels.width / logical.x, pixels.height / logical.y);
        offset_ = {(pixels.width - logical.x * scale_) / 2, (pixels.height - logical.y * scale_) / 2};
    }
    std::optional<Vec2> to_logical(Vec2 p) const {
        if (!finite(p.x) || !finite(p.y))
            return {};
        Vec2 q{(p.x - offset_.x) / scale_, (p.y - offset_.y) / scale_};
        return Rect{0, 0, logical_.x, logical_.y}.contains(q) ? std::optional(q) : std::nullopt;
    }
    Vec2 to_pixels(Vec2 p) const { return {offset_.x + p.x * scale_, offset_.y + p.y * scale_}; }
    double scale() const { return scale_; }
};
} // namespace stick
