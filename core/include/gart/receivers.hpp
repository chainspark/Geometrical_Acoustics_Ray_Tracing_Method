// Geometrical Acoustics Ray Tracing (gart) - spherical receivers and pressure accumulation.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

#include "gart/scene.hpp"
#include "gart/vec3.hpp"

namespace gart {

/// One straight piece of an (unfolded) ray: the line origin + t*dir, t >= 0, where
/// `origin` is the current image source and `gain` the product of reflection coefficients.
struct Segment {
    Vec3 origin;
    Vec3 dir; // unit length
    Complex gain;
};

/// Complex pressure sums and detection counts, one entry per receiver.
struct Accumulator {
    std::vector<Complex> pressure;
    std::vector<std::uint64_t> hits;

    Accumulator() = default;
    explicit Accumulator(std::size_t n) : pressure(n), hits(n) {}

    [[nodiscard]] std::size_t size() const { return pressure.size(); }
    void reset() {
        std::fill(pressure.begin(), pressure.end(), Complex{});
        std::fill(hits.begin(), hits.end(), 0);
    }
    Accumulator& operator+=(const Accumulator& o) {
        for (std::size_t i = 0; i < pressure.size(); ++i) {
            pressure[i] += o.pressure[i];
            hits[i] += o.hits[i];
        }
        return *this;
    }
};

/// Per-ray pressure contribution for a spherical receiver of radius a.
///
/// p = R_eff * P0 * exp(i k r), with
///   P0 = 2 r sqrt(rho c W pi D / V) / (N L),  V = 4/3 pi a^3,  L = sqrt(pi) a,
/// where r is the image-source distance, D the chord the ray cuts through the sphere,
/// and N the total number of rays emitted.
class HitModel {
public:
    HitModel(const Scene& scene, double radius)
        : radius2_(radius * radius), k_(scene.wavenumber()) {
        constexpr double pi = std::numbers::pi;
        const double volume = 4.0 / 3.0 * pi * radius * radius * radius; // NB: was (4/3) == 1 in the original
        const double L = std::sqrt(pi) * radius;
        const double W = scene.source.powerWatts();
        const double rhoC = scene.medium.density * scene.medium.soundSpeed;
        coef_ = 2.0 * std::sqrt(rhoC * W * pi / volume) / (static_cast<double>(scene.settings.rayCount) * L);
    }

    /// Tests the segment against a sphere centred at c; on a hit adds the ray pressure.
    void test(const Segment& s, const Vec3& c, Complex& pressure, std::uint64_t& hits) const {
        const Vec3 rel = c - s.origin;
        const double t = dot(rel, s.dir);
        if (t < 0) return; // receiver behind the image source
        const double dist2 = norm2(rel);
        const double perp2 = std::max(0.0, dist2 - t * t); // squared distance from line to centre
        if (perp2 >= radius2_) return;
        const double path = std::sqrt(dist2);
        const double chord = 2.0 * std::sqrt(radius2_ - perp2);
        pressure += s.gain * std::polar(coef_ * path * std::sqrt(chord), k_ * path);
        ++hits;
    }

private:
    double radius2_;
    double k_;
    double coef_{};
};

/// Arbitrary list of receivers with a common radius.
struct PointReceivers {
    std::vector<Vec3> centers;
    double radius = 0.025;

    [[nodiscard]] std::size_t size() const { return centers.size(); }
    [[nodiscard]] Vec3 center(std::size_t i) const { return centers[i]; }

    void collect(const Segment& s, const HitModel& m, Accumulator& acc) const {
        for (std::size_t i = 0; i < centers.size(); ++i) m.test(s, centers[i], acc.pressure[i], acc.hits[i]);
    }
};

/// Regular grid of receivers on an axis-aligned slice (e.g. axis = 2, offset = 1.2 is
/// the horizontal plane z = 1.2). The two in-plane axes are u < v (axis 2 -> u = x, v = y).
/// Cell (iu, iv) has index iv * nu + iu and centre u0 + (iu + 0.5) du, v0 + (iv + 0.5) dv.
///
/// Only cells near the ray's path are tested: the line is marched column by column,
/// so the cost per segment is proportional to the cells it passes, not the grid size.
struct GridReceivers {
    int axis = 2;
    double offset = 1.0;
    double u0 = 0, v0 = 0;
    double du = 0.1, dv = 0.1;
    int nu = 10, nv = 10;
    double radius = 0.025;

    [[nodiscard]] int uAxis() const { return axis == 0 ? 1 : 0; }
    [[nodiscard]] int vAxis() const { return axis == 2 ? 1 : 2; }
    [[nodiscard]] std::size_t size() const { return static_cast<std::size_t>(nu) * static_cast<std::size_t>(nv); }

    [[nodiscard]] Vec3 center(int iu, int iv) const {
        Vec3 c;
        c[axis] = offset;
        c[uAxis()] = u0 + (iu + 0.5) * du;
        c[vAxis()] = v0 + (iv + 0.5) * dv;
        return c;
    }
    [[nodiscard]] Vec3 center(std::size_t i) const {
        return center(static_cast<int>(i % static_cast<std::size_t>(nu)), static_cast<int>(i / static_cast<std::size_t>(nu)));
    }

    void collect(const Segment& s, const HitModel& m, Accumulator& acc) const {
        const double r = radius;
        // A hit needs t >= 0 and t <= |c - origin| for some centre c; bound by the farthest grid corner.
        double tMax = 0;
        for (int cu = 0; cu < 2; ++cu)
            for (int cv = 0; cv < 2; ++cv) {
                Vec3 corner;
                corner[axis] = offset;
                corner[uAxis()] = u0 + cu * nu * du;
                corner[vAxis()] = v0 + cv * nv * dv;
                tMax = std::max(tMax, norm(corner - s.origin));
            }
        tMax += r;

        // The closest-approach point must be within r of the slice plane.
        double t0 = 0, t1 = tMax;
        if (!clip(s.origin[axis], s.dir[axis], offset - r, offset + r, t0, t1)) return;

        // March along the in-plane axis in which the ray moves fastest.
        const bool uMajor = std::abs(s.dir[uAxis()]) >= std::abs(s.dir[vAxis()]);
        const int pA = uMajor ? uAxis() : vAxis();
        const int qA = uMajor ? vAxis() : uAxis();
        const double p0 = uMajor ? u0 : v0, dp = uMajor ? du : dv;
        const double q0 = uMajor ? v0 : u0, dq = uMajor ? dv : du;
        const int np = uMajor ? nu : nv, nq = uMajor ? nv : nu;

        const auto [pLo, pHi] = span(s.origin[pA], s.dir[pA], t0, t1);
        const int i0 = std::max(0, firstCell(pLo - r, p0, dp));
        const int i1 = std::min(np - 1, lastCell(pHi + r, p0, dp));
        for (int i = i0; i <= i1; ++i) {
            const double cp = p0 + (i + 0.5) * dp;
            double ta = t0, tb = t1;
            if (!clip(s.origin[pA], s.dir[pA], cp - r, cp + r, ta, tb)) continue;
            const auto [qLo, qHi] = span(s.origin[qA], s.dir[qA], ta, tb);
            const int j0 = std::max(0, firstCell(qLo - r, q0, dq));
            const int j1 = std::min(nq - 1, lastCell(qHi + r, q0, dq));
            for (int j = j0; j <= j1; ++j) {
                const int iu = uMajor ? i : j;
                const int iv = uMajor ? j : i;
                const std::size_t idx = static_cast<std::size_t>(iv) * static_cast<std::size_t>(nu) + static_cast<std::size_t>(iu);
                m.test(s, center(iu, iv), acc.pressure[idx], acc.hits[idx]);
            }
        }
    }

private:
    /// Intersects [t0, t1] with {t : lo <= o + d t <= hi}. Returns false if empty.
    static bool clip(double o, double d, double lo, double hi, double& t0, double& t1) {
        if (std::abs(d) < 1e-15) return o >= lo && o <= hi;
        double a = (lo - o) / d, b = (hi - o) / d;
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        return t0 <= t1;
    }
    struct Range { double lo, hi; };
    static Range span(double o, double d, double ta, double tb) {
        const double a = o + d * ta, b = o + d * tb;
        return {std::min(a, b), std::max(a, b)};
    }
    // Indices of the first / last cell whose centre lies at or beyond / before x.
    static int firstCell(double x, double c0, double h) {
        return static_cast<int>(std::max(-1.0e9, std::ceil((x - c0) / h - 0.5)));
    }
    static int lastCell(double x, double c0, double h) {
        return static_cast<int>(std::min(1.0e9, std::floor((x - c0) / h - 0.5)));
    }
};

} // namespace gart
