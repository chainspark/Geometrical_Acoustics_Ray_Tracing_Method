// Geometrical Acoustics Ray Tracing (gart) - shoe-box room geometry.
#pragma once

#include <array>
#include <complex>
#include <string_view>

#include "gart/vec3.hpp"

namespace gart {

using Complex = std::complex<double>;

/// Infinite plane n.p + D = 0 with unit normal n.
struct Plane {
    Vec3 normal{};
    double D{};

    /// Signed distance of p along the normal (positive on the side the normal points to).
    [[nodiscard]] constexpr double signedDistance(const Vec3& p) const { return dot(normal, p) + D; }

    /// Mirror image of point p across the plane.
    [[nodiscard]] constexpr Vec3 mirror(const Vec3& p) const { return p - normal * (2.0 * signedDistance(p)); }
};

/// Plane through three points; normal = (p3 - p2) x (p1 - p2), normalised.
[[nodiscard]] inline Plane makePlane(const Vec3& p1, const Vec3& p2, const Vec3& p3) {
    const Vec3 n = normalized(cross(p3 - p2, p1 - p2));
    return {n, -dot(n, p1)};
}

/// Wall indices. Order matches the original implementation.
enum Wall : int { Bottom = 0, Left = 1, Front = 2, Right = 3, Back = 4, Top = 5 };
inline constexpr int kWallCount = 6;

[[nodiscard]] constexpr std::string_view wallName(int w) {
    constexpr std::array<std::string_view, kWallCount> names{"bottom", "left", "front", "right", "back", "top"};
    return names.at(static_cast<std::size_t>(w));
}

/// Axis-aligned shoe-box room with one corner at the origin.
///   x in [0, length], y in [0, width], z in [0, height]  (z is up)
/// Each wall has a complex, angle-independent pressure reflection coefficient R.
struct Room {
    double length{1.0}; // along x
    double width{1.0};  // along y
    double height{1.0}; // along z
    std::array<Complex, kWallCount> R{}; // default: fully absorbing (anechoic)

    [[nodiscard]] constexpr Vec3 size() const { return {length, width, height}; }

    [[nodiscard]] constexpr bool contains(const Vec3& p) const {
        return p.x > 0 && p.x < length && p.y > 0 && p.y < width && p.z > 0 && p.z < height;
    }

    /// Wall hit when travelling along +/- the given axis.
    [[nodiscard]] static constexpr int wallFor(int axis, bool positive) {
        constexpr int lo[3] = {Left, Front, Bottom};
        constexpr int hi[3] = {Right, Back, Top};
        return positive ? hi[axis] : lo[axis];
    }

    /// Inward-facing planes in wall order.
    [[nodiscard]] std::array<Plane, kWallCount> planes() const {
        const Vec3 c1{0, 0, 0}, c2{0, width, 0}, c3{length, 0, 0}, c4{length, width, 0};
        const Vec3 c5{0, 0, height}, c6{0, width, height}, c7{length, 0, height}, c8{length, width, height};
        return {makePlane(c1, c3, c2), makePlane(c1, c2, c5), makePlane(c1, c5, c7),
                makePlane(c3, c7, c8), makePlane(c4, c8, c6), makePlane(c7, c5, c6)};
    }
};

} // namespace gart
