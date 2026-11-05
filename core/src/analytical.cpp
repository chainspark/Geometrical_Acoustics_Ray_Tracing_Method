#include "gart/analytical.hpp"

#include <cmath>
#include <cstdlib>
#include <numbers>

namespace gart {

namespace {

struct AxisImage {
    double coord;
    Complex gain;
    int order;
};

Complex ipow(Complex base, int n) {
    Complex r{1.0, 0.0};
    for (int i = 0; i < n; ++i) r *= base;
    return r;
}

// Images along one axis of length L with source coordinate s: coord = (1 - 2q) s + 2 n L.
// Reflections: |n - q| on the low wall (coordinate 0) and |n| on the high wall.
std::vector<AxisImage> axisImages(double s, double L, Complex rLow, Complex rHigh, int maxOrder) {
    std::vector<AxisImage> out;
    for (int n = -maxOrder; n <= maxOrder; ++n) {
        for (int q = 0; q <= 1; ++q) {
            const int lo = std::abs(n - q);
            const int hi = std::abs(n);
            if (lo + hi > maxOrder) continue;
            const Complex g = ipow(rLow, lo) * ipow(rHigh, hi);
            if (g == Complex{}) continue;
            out.push_back({(1 - 2 * q) * s + 2.0 * n * L, g, lo + hi});
        }
    }
    return out;
}

} // namespace

std::vector<ImageSource> imageSources(const Scene& scene, int maxOrder) {
    const Room& room = scene.room;
    const Vec3 s = scene.source.position;
    const auto xs = axisImages(s.x, room.length, room.R[Left], room.R[Right], maxOrder);
    const auto ys = axisImages(s.y, room.width, room.R[Front], room.R[Back], maxOrder);
    const auto zs = axisImages(s.z, room.height, room.R[Bottom], room.R[Top], maxOrder);

    std::vector<ImageSource> out;
    for (const auto& x : xs)
        for (const auto& y : ys) {
            if (x.order + y.order > maxOrder) continue;
            for (const auto& z : zs) {
                const int order = x.order + y.order + z.order;
                if (order > maxOrder) continue;
                out.push_back({{x.coord, y.coord, z.coord}, x.gain * y.gain * z.gain, order});
            }
        }
    return out;
}

Complex imageSourcePressure(const Scene& scene, const std::vector<ImageSource>& images, const Vec3& point) {
    const double amplitude = std::sqrt(scene.medium.density * scene.medium.soundSpeed * scene.source.powerWatts() /
                                       (4.0 * std::numbers::pi));
    const double k = scene.wavenumber();
    Complex p{};
    for (const auto& img : images) {
        const double r = norm(point - img.position);
        if (r <= 0) continue;
        p += img.gain * std::polar(amplitude / r, k * r);
    }
    return p;
}

double analyticalSpl(const Scene& scene, const Vec3& point) {
    return toSpl(imageSourcePressure(scene, imageSources(scene, scene.settings.maxOrder), point));
}

} // namespace gart
