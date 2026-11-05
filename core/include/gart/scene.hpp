// Geometrical Acoustics Ray Tracing (gart) - scene description.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>

#include "gart/room.hpp"
#include "gart/vec3.hpp"

namespace gart {

inline constexpr double kReferencePower = 1.0e-12;   // W
inline constexpr double kReferencePressure = 2.0e-5; // Pa

/// Properties of air at 20 degrees Celsius.
struct Medium {
    double density = 1.2041;   // kg/m^3
    double soundSpeed = 343.22; // m/s
};

/// Omnidirectional point source.
struct Source {
    Vec3 position{};
    double powerDb = 100.0;   // sound power level re 1e-12 W
    double frequency = 250.0; // Hz

    [[nodiscard]] double powerWatts() const { return kReferencePower * std::pow(10.0, powerDb / 10.0); }
};

struct TraceSettings {
    std::uint64_t rayCount = 1'000'000; // total rays used for normalisation
    int maxOrder = 1;                    // highest reflection order traced
    double threshold = 0.0;              // stop a ray once |R_eff| <= threshold
    std::uint64_t seed = 1;
};

struct Scene {
    Room room;
    Source source;
    Medium medium;
    TraceSettings settings;

    [[nodiscard]] double wavenumber() const {
        return 2.0 * std::numbers::pi * source.frequency / medium.soundSpeed;
    }
    [[nodiscard]] double wavelength() const { return medium.soundSpeed / source.frequency; }
};

/// Returns an empty string when the scene is valid, otherwise a description of the problem.
[[nodiscard]] inline std::string validate(const Scene& s) {
    if (!(s.room.length > 0 && s.room.width > 0 && s.room.height > 0)) return "room dimensions must be positive";
    if (!s.room.contains(s.source.position)) return "source must be strictly inside the room";
    if (!(s.source.frequency > 0)) return "frequency must be positive";
    if (s.settings.rayCount == 0) return "ray count must be positive";
    if (s.settings.maxOrder < 0) return "reflection order must be >= 0";
    if (!(s.medium.density > 0 && s.medium.soundSpeed > 0)) return "medium properties must be positive";
    return {};
}

/// Sound pressure level (dB re 20 uPa) of a complex pressure. -inf when silent.
[[nodiscard]] inline double toSpl(const Complex& p) {
    const double mag = std::abs(p);
    return mag > 0 ? 20.0 * std::log10(mag / kReferencePressure) : -std::numeric_limits<double>::infinity();
}

} // namespace gart
