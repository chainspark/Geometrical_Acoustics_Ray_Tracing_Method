#include <gtest/gtest.h>

#include <cmath>

#include "gart/parallel.hpp"
#include "gart/tracer.hpp"

using namespace gart;

namespace {

Scene reverberantScene() {
    Scene s;
    s.room = {4, 5, 3};
    s.room.R.fill({0.8, 0.1});
    s.source.position = {1.3, 2.1, 1.7};
    s.source.frequency = 500;
    s.settings.rayCount = 200'000;
    s.settings.maxOrder = 3;
    s.settings.seed = 99;
    return s;
}

PointReceivers asPoints(const GridReceivers& g) {
    PointReceivers p;
    p.radius = g.radius;
    for (std::size_t i = 0; i < g.size(); ++i) p.centers.push_back(g.center(i));
    return p;
}

void expectSameResult(const Accumulator& a, const Accumulator& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a.hits[i], b.hits[i]) << "receiver " << i;
        EXPECT_NEAR(std::abs(a.pressure[i] - b.pressure[i]), 0.0, 1e-9 * (1.0 + std::abs(a.pressure[i])))
            << "receiver " << i;
    }
}

} // namespace

TEST(Scene, Validation) {
    Scene s = reverberantScene();
    EXPECT_EQ(validate(s), "");
    s.source.position = {5, 1, 1};
    EXPECT_NE(validate(s), "");
}

TEST(Accumulator, ResetAndMerge) {
    Accumulator a(2), b(2);
    a.pressure[0] = {1, 2};
    a.hits[0] = 3;
    b.pressure[0] = {1, 1};
    b.hits[0] = 1;
    a += b;
    EXPECT_EQ(a.pressure[0], (Complex{2, 3}));
    EXPECT_EQ(a.hits[0], 4u);
    a.reset();
    EXPECT_EQ(a.hits[0], 0u);
    EXPECT_EQ(a.pressure[0], Complex{});
}

// The marching grid must find exactly the same hits as brute-force testing every receiver.
class GridMatchesBruteForce : public ::testing::TestWithParam<int> {};

TEST_P(GridMatchesBruteForce, AllSliceAxes) {
    const Scene scene = reverberantScene();
    GridReceivers grid;
    grid.axis = GetParam();
    const Vec3 size = scene.room.size();
    grid.offset = size[grid.axis] * 0.4;
    grid.nu = 23;
    grid.nv = 17;
    grid.u0 = 0;
    grid.v0 = 0;
    grid.du = size[grid.uAxis()] / grid.nu;
    grid.dv = size[grid.vAxis()] / grid.nv;
    grid.radius = 0.12; // larger than half the spacing in places: spheres overlap

    Accumulator fast(grid.size()), slow(grid.size());
    trace(scene, grid, 0, scene.settings.rayCount, fast);
    trace(scene, asPoints(grid), 0, scene.settings.rayCount, slow);
    expectSameResult(fast, slow);

    std::uint64_t total = 0;
    for (auto h : fast.hits) total += h;
    EXPECT_GT(total, 1000u);
}

INSTANTIATE_TEST_SUITE_P(Axes, GridMatchesBruteForce, ::testing::Values(0, 1, 2));

TEST(Parallel, ResultIndependentOfThreadCount) {
    const Scene scene = reverberantScene();
    PointReceivers rx;
    rx.radius = 0.1;
    rx.centers = {{2, 2, 1}, {3, 4, 2}, {0.5, 0.5, 0.5}};
    const Accumulator one = traceParallel(scene, rx, 1);
    const Accumulator many = traceParallel(scene, rx, 7);
    expectSameResult(one, many);
}

TEST(Parallel, RangesComposeToFullTrace) {
    const Scene scene = reverberantScene();
    PointReceivers rx;
    rx.radius = 0.1;
    rx.centers = {{2, 2, 1}};
    Accumulator full(1), parts(1);
    trace(scene, rx, 0, scene.settings.rayCount, full);
    trace(scene, rx, 0, 12345, parts);
    trace(scene, rx, 12345, scene.settings.rayCount, parts);
    expectSameResult(full, parts);
}

TEST(Tracer, AbsorbedRaysStopAfterDirectSound) {
    Scene scene = reverberantScene();
    scene.room.R.fill({0, 0});
    PointReceivers rx;
    rx.radius = 0.2;
    rx.centers = {{2, 2, 1}};
    Accumulator anechoic(1);
    trace(scene, rx, 0, scene.settings.rayCount, anechoic);

    scene.settings.maxOrder = 0;
    Accumulator direct(1);
    trace(scene, rx, 0, scene.settings.rayCount, direct);
    expectSameResult(anechoic, direct);
}
