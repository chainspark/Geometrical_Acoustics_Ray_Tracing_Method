// Geometrical Acoustics Ray Tracing (gart) - 3D vector primitives.
#pragma once

#include <cmath>

namespace gart {

struct Vec3 {
    double x{};
    double y{};
    double z{};

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& b) const { return {x + b.x, y + b.y, z + b.z}; }
    constexpr Vec3 operator-(const Vec3& b) const { return {x - b.x, y - b.y, z - b.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3& operator+=(const Vec3& b) { x += b.x; y += b.y; z += b.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& b) { x -= b.x; y -= b.y; z -= b.z; return *this; }
    constexpr bool operator==(const Vec3& b) const = default;

    /// Component access by axis index (0 = x, 1 = y, 2 = z).
    [[nodiscard]] constexpr double operator[](int axis) const {
        return axis == 0 ? x : (axis == 1 ? y : z);
    }
    constexpr double& operator[](int axis) {
        return axis == 0 ? x : (axis == 1 ? y : z);
    }
};

constexpr Vec3 operator*(double s, const Vec3& v) { return v * s; }

[[nodiscard]] constexpr double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] constexpr double norm2(const Vec3& v) { return dot(v, v); }
[[nodiscard]] inline double norm(const Vec3& v) { return std::sqrt(norm2(v)); }
[[nodiscard]] inline Vec3 normalized(const Vec3& v) { return v / norm(v); }

/// Mirror direction d about a surface with unit normal n.
[[nodiscard]] constexpr Vec3 reflect(const Vec3& d, const Vec3& n) {
    return d - n * (2.0 * dot(n, d));
}

[[nodiscard]] inline bool approxEqual(const Vec3& a, const Vec3& b, double eps = 1e-9) {
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
}

} // namespace gart
