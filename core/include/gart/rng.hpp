// Geometrical Acoustics Ray Tracing (gart) - counter-based random ray directions.
//
// Ray i's direction is a pure function of (seed, i). Rays never need to be stored,
// any sub-range can be traced independently, and the result does not depend on how
// the work is split across threads or Web Workers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "gart/vec3.hpp"

namespace gart {

/// SplitMix64 finaliser: a high-quality 64-bit mixing function.
[[nodiscard]] constexpr std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

/// Uniform double in [0, 1) from 64 random bits.
[[nodiscard]] constexpr double toUnit(std::uint64_t bits) {
    return static_cast<double>(bits >> 11) * 0x1.0p-53;
}

/// Uniformly distributed unit direction for ray `index`.
/// Same sampling as the original code: theta = 2*pi*u, cos(phi) = 2v - 1.
[[nodiscard]] inline Vec3 rayDirection(std::uint64_t seed, std::uint64_t index) {
    // XOR with a fixed key is a bijection, so (2i, 2i+1) never collide across rays.
    const std::uint64_t key = splitmix64(seed);
    const double u = toUnit(splitmix64(key ^ (index * 2)));
    const double v = toUnit(splitmix64(key ^ (index * 2 + 1)));
    const double theta = 2.0 * std::numbers::pi * u;
    const double cosPhi = 2.0 * v - 1.0;
    const double sinPhi = std::sqrt(std::max(0.0, 1.0 - cosPhi * cosPhi));
    return {std::cos(theta) * sinPhi, std::sin(theta) * sinPhi, cosPhi};
}

} // namespace gart
