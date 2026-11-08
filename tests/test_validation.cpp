// Physics validation: the ray tracer against the exact image-source solution.
#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <numbers>

#include "gart/analytical.hpp"
#include "gart/parallel.hpp"

using namespace gart;

namespace {

struct Comparison {
    double rmsError = 0;     // dB
    double maxError = 0;     // dB
    double meanError = 0;    // dB (bias)
    int points = 0;
};

/// Compares ray-traced and analytical SPL at each receiver. Receivers in deep
/// interference nulls (total level more than `nullDepth` dB below the direct sound
/// alone) are skipped: a dB error there measures noise in a near-zero sum.
Comparison compare(const Scene& scene, const PointReceivers& rx, double nullDepth = 10.0) {
    const Accumulator acc = traceParallel(scene, rx);
    const auto images = imageSources(scene, scene.settings.maxOrder);
    const auto direct = imageSources(scene, 0);
    Comparison c;
    double sum2 = 0, sum = 0;
    for (std::size_t i = 0; i < rx.size(); ++i) {
        const double ref = toSpl(imageSourcePressure(scene, images, rx.centers[i]));
        if (ref < toSpl(imageSourcePressure(scene, direct, rx.centers[i])) - nullDepth) continue;
        const double err = toSpl(acc.pressure[i]) - ref;
        sum += err;
        sum2 += err * err;
        c.maxError = std::max(c.maxError, std::abs(err));
        ++c.points;
    }
    c.meanError = sum / c.points;
    c.rmsError = std::sqrt(sum2 / c.points);
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::cout << "[ metrics  ] " << info->name() << ": points=" << c.points << " rms=" << c.rmsError
              << " dB max=" << c.maxError << " dB mean=" << c.meanError << " dB\n";
    return c;
}

std::ostream& operator<<(std::ostream& os, const Comparison& c) {
    return os << "points=" << c.points << " rms=" << c.rmsError << " dB, max=" << c.maxError
              << " dB, mean=" << c.meanError << " dB";
}

} // namespace

TEST(ImageSources, CountsAndOrders) {
    Scene s;
    s.room = {4, 5, 3};
    s.room.R.fill({1, 0});
    s.source.position = {1, 1, 1};
    // Number of lattice images with order <= N in 3D: (2N+1)(2N^2+2N+3)/3.
    for (int n = 0; n <= 4; ++n) {
        EXPECT_EQ(static_cast<int>(imageSources(s, n).size()), (2 * n + 1) * (2 * n * n + 2 * n + 3) / 3) << n;
    }
    s.room.R.fill({0, 0});
    s.room.R[Bottom] = {1, 0};
    const auto floorOnly = imageSources(s, 1);
    ASSERT_EQ(floorOnly.size(), 2u);
    EXPECT_TRUE(approxEqual(floorOnly[0].position, {1, 1, -1}) || approxEqual(floorOnly[1].position, {1, 1, -1}));
}

TEST(ImageSources, FreeFieldLevel) {
    Scene s;
    s.room = {4, 4, 4};
    s.source.position = {2, 2, 3};
    s.source.powerDb = 100;
    s.settings.maxOrder = 0;
    // Lp = Lw - 10 log10(4 pi r^2) + 10 log10(rho c / 400)
    const double r = 2.0;
    const double expected = 100.0 - 10 * std::log10(4 * std::numbers::pi * r * r) +
                            10 * std::log10(s.medium.density * s.medium.soundSpeed / 400.0);
    EXPECT_NEAR(analyticalSpl(s, {2, 2, 1}), expected, 1e-9);
}

// Anechoic room, receiver 2 m below the source (the original "Point" scenario).
// The original code over-predicted by ~+1.1 dB because (4 / 3) evaluated to 1.
TEST(Validation, AnechoicMatchesFreeField) {
    Scene s;
    s.room = {4, 4, 4};
    s.source.position = {2, 2, 3};
    s.settings.rayCount = 4'000'000;
    s.settings.maxOrder = 0;
    PointReceivers rx;
    rx.radius = 0.1;
    rx.centers = {{2, 2, 1}};
    const Comparison c = compare(s, rx);
    SCOPED_TRACE(::testing::Message() << c);
    EXPECT_LT(c.maxError, 0.5);
}

// Case I: rigid floor, receiver moved vertically below the source.
TEST(Validation, CaseOneRigidFloorVerticalSweep) {
    Scene s;
    s.room = {4, 4, 4};
    s.room.R[Bottom] = {1, 0};
    s.source.position = {2, 2, 3};
    s.settings.rayCount = 4'000'000;
    s.settings.maxOrder = 1;
    PointReceivers rx;
    rx.radius = 0.05;
    for (double d = 0.1; d < 2.9; d += 0.1) rx.centers.push_back({2, 2, d});
    const Comparison c = compare(s, rx);
    SCOPED_TRACE(::testing::Message() << c);
    EXPECT_LT(c.rmsError, 1.0);
    EXPECT_LT(std::abs(c.meanError), 0.5);
}

// Case II: rigid floor, receiver moved horizontally away from the source.
TEST(Validation, CaseTwoRigidFloorHorizontalSweep) {
    Scene s;
    s.room = {10, 10, 4};
    s.room.R[Bottom] = {1, 0};
    s.source.position = {5, 5, 1};
    s.settings.rayCount = 4'000'000;
    s.settings.maxOrder = 1;
    PointReceivers rx;
    rx.radius = 0.05;
    for (double d = 0.1; d < 3.0; d += 0.1) rx.centers.push_back({5 + d, 5, 2});
    const Comparison c = compare(s, rx);
    SCOPED_TRACE(::testing::Message() << c);
    EXPECT_LT(c.rmsError, 1.0);
    EXPECT_LT(std::abs(c.meanError), 0.5);
}

// All six walls reflecting with a complex coefficient, up to 3rd order reflections.
TEST(Validation, ReverberantComplexWallsHigherOrder) {
    Scene s;
    s.room = {3, 4, 2.5};
    s.room.R.fill({0.6, 0.3});
    s.source.position = {1.1, 1.7, 1.2};
    s.source.frequency = 125;
    s.settings.rayCount = 4'000'000;
    s.settings.maxOrder = 3;
    PointReceivers rx;
    rx.radius = 0.1;
    rx.centers = {{2.0, 2.5, 1.0}, {0.7, 3.1, 1.8}, {2.4, 0.8, 0.6}, {1.5, 2.0, 2.0}};
    const Comparison c = compare(s, rx);
    SCOPED_TRACE(::testing::Message() << c);
    EXPECT_LT(c.maxError, 1.5);
}
