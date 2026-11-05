// Geometrical Acoustics Ray Tracing (gart) - exact image-source reference solution.
//
// For a shoe-box room with angle-independent reflection coefficients, the coherent
// sum over image sources up to a given reflection order is the exact answer the ray
// tracer converges to. With only the floor reflecting and order 1 this reduces to
// the closed-form "direct + one image" formula used for Case I and Case II.
#pragma once

#include <vector>

#include "gart/scene.hpp"

namespace gart {

struct ImageSource {
    Vec3 position;
    Complex gain; // product of the reflection coefficients along the path
    int order;
};

/// All image sources with reflection order <= maxOrder and non-zero gain
/// (Allen & Berkley lattice; the direct source is order 0).
[[nodiscard]] std::vector<ImageSource> imageSources(const Scene& scene, int maxOrder);

/// Complex pressure at `point` from the given image sources:
/// p = sum gain * sqrt(rho c W / 4 pi) * exp(i k r) / r.
[[nodiscard]] Complex imageSourcePressure(const Scene& scene, const std::vector<ImageSource>& images, const Vec3& point);

/// Convenience: SPL at `point` using images up to scene.settings.maxOrder.
[[nodiscard]] double analyticalSpl(const Scene& scene, const Vec3& point);

} // namespace gart
