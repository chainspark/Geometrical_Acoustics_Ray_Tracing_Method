// Embind wrapper exposing the gart tracer to JavaScript.
//
// One Simulator instance lives in each Web Worker. The page hands each worker a scene
// and ranges of ray indices; the worker traces them and posts back the partial complex
// pressure sums, which the page adds up. Because ray directions are a pure function of
// (seed, index), the total is the same however the work is split.
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "gart/analytical.hpp"
#include "gart/tracer.hpp"

using namespace gart;
using emscripten::val;

namespace {

/// Slice grid and line receivers traced together in one pass (shared radius).
/// Accumulator layout: grid cells first, then the line receivers.
struct ReceiverPair {
    GridReceivers grid;
    PointReceivers line;
    double radius = 0.05;

    [[nodiscard]] std::size_t size() const { return grid.size() + line.size(); }
    void collect(const Segment& s, const HitModel& m, Accumulator& acc) const {
        grid.collect(s, m, acc);
        const std::size_t base = grid.size();
        for (std::size_t i = 0; i < line.centers.size(); ++i)
            m.test(s, line.centers[i], acc.pressure[base + i], acc.hits[base + i]);
    }
};

template <class T>
std::vector<T> slice(const std::vector<T>& v, std::size_t begin, std::size_t end) {
    return {v.begin() + static_cast<std::ptrdiff_t>(begin), v.begin() + static_cast<std::ptrdiff_t>(end)};
}

val toFloat64Array(const std::vector<double>& v) {
    val arr = val::global("Float64Array").new_(v.size());
    arr.call<void>("set", val(emscripten::typed_memory_view(v.size(), v.data())));
    return arr;
}

val complexToArray(const std::vector<Complex>& v) {
    std::vector<double> flat;
    flat.reserve(v.size() * 2);
    for (const auto& c : v) {
        flat.push_back(c.real());
        flat.push_back(c.imag());
    }
    return toFloat64Array(flat);
}

val hitsToArray(const std::vector<std::uint64_t>& v) {
    return toFloat64Array(std::vector<double>(v.begin(), v.end()));
}

} // namespace

class Simulator {
public:
    Simulator() {
        scene_.room = {10, 10, 4};
        scene_.room.R[Bottom] = {1, 0};
        scene_.source.position = {5, 5, 1};
        setGrid(2, 2.0, 50, 50, 0.1);
    }

    // ---- scene ----
    void setRoom(double length, double width, double height) { scene_.room.length = length; scene_.room.width = width; scene_.room.height = height; }
    void setWall(int wall, double re, double im) {
        if (wall >= 0 && wall < kWallCount) scene_.room.R[static_cast<std::size_t>(wall)] = {re, im};
    }
    void setSource(double x, double y, double z, double powerDb, double frequency) {
        scene_.source = {{x, y, z}, powerDb, frequency};
    }
    void setMedium(double density, double soundSpeed) { scene_.medium = {density, soundSpeed}; }
    void setSettings(double rayCount, int maxOrder, double seed, double threshold) {
        scene_.settings.rayCount = static_cast<std::uint64_t>(rayCount);
        scene_.settings.maxOrder = maxOrder;
        scene_.settings.seed = static_cast<std::uint64_t>(seed);
        scene_.settings.threshold = threshold;
    }
    std::string validate() const { return gart::validate(scene_); }

    // ---- receivers ----
    /// Grid of nu x nv receivers covering the room cross-section at `offset` along `axis`.
    void setGrid(int axis, double offset, int nu, int nv, double radius) {
        GridReceivers& g = rx_.grid;
        g.axis = axis;
        g.offset = offset;
        g.nu = nu;
        g.nv = nv;
        g.u0 = 0;
        g.v0 = 0;
        g.du = scene_.room.size()[g.uAxis()] / nu;
        g.dv = scene_.room.size()[g.vAxis()] / nv;
        setRadius(radius);
        acc_ = Accumulator(rx_.size());
    }
    /// n receivers evenly spaced from (x0,y0,z0) to (x1,y1,z1).
    void setLine(double x0, double y0, double z0, double x1, double y1, double z1, int n, double radius) {
        rx_.line.centers.clear();
        const Vec3 a{x0, y0, z0}, b{x1, y1, z1};
        for (int i = 0; i < n; ++i) {
            const double t = n > 1 ? static_cast<double>(i) / (n - 1) : 0.0;
            rx_.line.centers.push_back(a + (b - a) * t);
        }
        setRadius(radius);
        acc_ = Accumulator(rx_.size());
    }
    int gridSize() const { return static_cast<int>(rx_.grid.size()); }
    int lineSize() const { return static_cast<int>(rx_.line.size()); }

    // ---- tracing ----
    void reset() { acc_.reset(); }
    /// Traces rays [begin, end) and adds them to the internal sums.
    void trace(double begin, double end) {
        gart::trace(scene_, rx_, static_cast<std::uint64_t>(begin), static_cast<std::uint64_t>(end), acc_);
    }
    val gridPressure() const { return complexToArray(slice(acc_.pressure, 0, rx_.grid.size())); }
    val gridHits() const { return hitsToArray(slice(acc_.hits, 0, rx_.grid.size())); }
    val linePressure() const { return complexToArray(slice(acc_.pressure, rx_.grid.size(), acc_.size())); }
    val lineHits() const { return hitsToArray(slice(acc_.hits, rx_.grid.size(), acc_.size())); }

    // ---- reference solution ----
    val analyticalGrid() const { return analytical([&](std::size_t i) { return rx_.grid.center(i); }, rx_.grid.size()); }
    val analyticalLine() const { return analytical([&](std::size_t i) { return rx_.line.centers[i]; }, rx_.line.size()); }
    int imageCount() const { return static_cast<int>(imageSources(scene_, scene_.settings.maxOrder).size()); }

    /// Paths of `count` rays spread over the ray index range, flattened as x,y,z
    /// triples with a NaN triple between rays.
    val rayPaths(int count) const {
        std::vector<double> flat;
        const std::uint64_t n = scene_.settings.rayCount;
        for (int k = 0; k < count; ++k) {
            const std::uint64_t idx = n * static_cast<std::uint64_t>(k) / static_cast<std::uint64_t>(count);
            for (const Vec3& p : rayPath(scene_, idx)) {
                flat.insert(flat.end(), {p.x, p.y, p.z});
            }
            flat.insert(flat.end(), 3, std::numeric_limits<double>::quiet_NaN());
        }
        return toFloat64Array(flat);
    }

private:
    void setRadius(double r) {
        rx_.radius = r;
        rx_.grid.radius = r;
        rx_.line.radius = r;
    }

    template <class CenterFn>
    val analytical(CenterFn center, std::size_t n) const {
        const auto images = imageSources(scene_, scene_.settings.maxOrder);
        std::vector<Complex> p(n);
        for (std::size_t i = 0; i < n; ++i) p[i] = imageSourcePressure(scene_, images, center(i));
        return complexToArray(p);
    }

    Scene scene_;
    ReceiverPair rx_;
    Accumulator acc_;
};

EMSCRIPTEN_BINDINGS(gart) {
    emscripten::class_<Simulator>("Simulator")
        .constructor<>()
        .function("setRoom", &Simulator::setRoom)
        .function("setWall", &Simulator::setWall)
        .function("setSource", &Simulator::setSource)
        .function("setMedium", &Simulator::setMedium)
        .function("setSettings", &Simulator::setSettings)
        .function("validate", &Simulator::validate)
        .function("setGrid", &Simulator::setGrid)
        .function("setLine", &Simulator::setLine)
        .function("gridSize", &Simulator::gridSize)
        .function("lineSize", &Simulator::lineSize)
        .function("reset", &Simulator::reset)
        .function("trace", &Simulator::trace)
        .function("gridPressure", &Simulator::gridPressure)
        .function("gridHits", &Simulator::gridHits)
        .function("linePressure", &Simulator::linePressure)
        .function("lineHits", &Simulator::lineHits)
        .function("analyticalGrid", &Simulator::analyticalGrid)
        .function("analyticalLine", &Simulator::analyticalLine)
        .function("imageCount", &Simulator::imageCount)
        .function("rayPaths", &Simulator::rayPaths);
}
