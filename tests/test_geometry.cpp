#include <gtest/gtest.h>

#include <cmath>

#include "gart/room.hpp"
#include "gart/rng.hpp"
#include "gart/tracer.hpp"
#include "gart/vec3.hpp"

using namespace gart;

// ---- Vec3 -----------------------------------------------------------------

TEST(Vec3, Arithmetic) {
    constexpr Vec3 a{1, 2, 3}, b{4, 5, 6};
    static_assert(dot(a, b) == 32);
    static_assert(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}) == Vec3{0, 0, 1});
    EXPECT_EQ(a + b, (Vec3{5, 7, 9}));
    EXPECT_EQ(b - a, (Vec3{3, 3, 3}));
    EXPECT_EQ(a * 2.0, (Vec3{2, 4, 6}));
    EXPECT_DOUBLE_EQ(norm(Vec3{3, 4, 0}), 5.0);
    EXPECT_DOUBLE_EQ(a[0], 1);
    EXPECT_DOUBLE_EQ(a[2], 3);
}

TEST(Vec3, Reflect) {
    EXPECT_EQ(reflect(Vec3{1, -1, 0}, Vec3{0, 1, 0}), (Vec3{1, 1, 0}));
}

// ---- Room (ported from the original unit tests) ------------------------------

TEST(RoomTest, GeneratePlane) {
    const Plane p = makePlane({1, 2, 3}, {2, 2, 3}, {-2, -2, 3});
    EXPECT_TRUE(approxEqual(p.normal, {0, 0, -1}));
    EXPECT_NEAR(p.signedDistance({0, 0, 3}), 0.0, 1e-12);
}

TEST(RoomTest, GenerateRoomInwardNormals) {
    Room room{2, 2, 2};
    const auto planes = room.planes();
    EXPECT_TRUE(approxEqual(planes[Bottom].normal, {0, 0, 1}));
    EXPECT_TRUE(approxEqual(planes[Left].normal, {1, 0, 0}));
    EXPECT_TRUE(approxEqual(planes[Front].normal, {0, 1, 0}));
    EXPECT_TRUE(approxEqual(planes[Right].normal, {-1, 0, 0}));
    EXPECT_TRUE(approxEqual(planes[Back].normal, {0, -1, 0}));
    EXPECT_TRUE(approxEqual(planes[Top].normal, {0, 0, -1}));
    // Centre is on the positive side of every wall.
    for (const auto& p : planes) EXPECT_GT(p.signedDistance({1, 1, 1}), 0);
}

TEST(RoomTest, ImageSourceIsMirror) {
    Room room{4, 5, 3};
    const auto planes = room.planes();
    EXPECT_TRUE(approxEqual(planes[Bottom].mirror({1, 2, 1}), {1, 2, -1}));
    EXPECT_TRUE(approxEqual(planes[Top].mirror({1, 2, 1}), {1, 2, 5}));
    EXPECT_TRUE(approxEqual(planes[Right].mirror({1, 2, 1}), {7, 2, 1}));
}

TEST(RoomTest, Contains) {
    Room room{4, 5, 3};
    EXPECT_TRUE(room.contains({1, 1, 1}));
    EXPECT_FALSE(room.contains({0, 1, 1}));
    EXPECT_FALSE(room.contains({1, 6, 1}));
}

// ---- Wall hits and reflection ---------------------------------------------------

TEST(Tracer, ExitWallPicksNearest) {
    Room room{4, 4, 4};
    EXPECT_EQ(exitWall(room, {2, 2, 3}, {0, 0, -1}).wall, Bottom);
    EXPECT_DOUBLE_EQ(exitWall(room, {2, 2, 3}, {0, 0, -1}).t, 3.0);
    EXPECT_EQ(exitWall(room, {2, 2, 3}, {0, 0, 1}).wall, Top);
    EXPECT_EQ(exitWall(room, {1, 2, 2}, {-1, 0, 0}).wall, Left);
    EXPECT_EQ(exitWall(room, {1, 2, 2}, {1, 0, 0}).wall, Right);
    EXPECT_EQ(exitWall(room, {2, 1, 2}, {0, -1, 0}).wall, Front);
    EXPECT_EQ(exitWall(room, {2, 1, 2}, {0, 1, 0}).wall, Back);
    const Vec3 diag = normalized({1, 1, 1});
    EXPECT_NEAR(exitWall(room, {2, 2, 2}, diag).t, std::sqrt(12.0), 1e-12); // corner hit, any wall OK
}

TEST(Tracer, ExitWallFromImageSource) {
    // Image below the floor heading up must leave through the top, not the floor.
    Room room{4, 4, 4};
    const auto hit = exitWall(room, {2, 2, -3}, normalized({0.1, 0, 1}));
    EXPECT_EQ(hit.wall, Top);
}

TEST(Tracer, ReflectMirrorsOriginDirectionAndGain) {
    Room room{4, 4, 4};
    room.R[Bottom] = {0.5, 0.25};
    Segment s{{2, 2, 3}, normalized({0.2, 0, -1}), {1, 0}};
    const auto hit = exitWall(room, s.origin, s.dir);
    ASSERT_EQ(hit.wall, Bottom);
    reflect(room, hit, s);
    EXPECT_TRUE(approxEqual(s.origin, {2, 2, -3}));
    EXPECT_GT(s.dir.z, 0);
    EXPECT_EQ(s.gain, (Complex{0.5, 0.25}));
}

TEST(Tracer, RayPathStaysOnWalls) {
    Scene scene;
    scene.room = {3, 4, 2.5};
    scene.room.R.fill({0.9, 0});
    scene.source.position = {1, 1.5, 1};
    scene.settings.maxOrder = 6;
    for (std::uint64_t i = 0; i < 50; ++i) {
        const auto pts = rayPath(scene, i);
        ASSERT_EQ(pts.size(), 8u);
        for (std::size_t k = 1; k < pts.size(); ++k) {
            const Vec3& p = pts[k];
            const bool onWall = std::abs(p.x) < 1e-9 || std::abs(p.x - 3) < 1e-9 || std::abs(p.y) < 1e-9 ||
                                std::abs(p.y - 4) < 1e-9 || std::abs(p.z) < 1e-9 || std::abs(p.z - 2.5) < 1e-9;
            EXPECT_TRUE(onWall) << "ray " << i << " point " << k;
            EXPECT_GE(p.x, -1e-9); EXPECT_LE(p.x, 3 + 1e-9);
            EXPECT_GE(p.y, -1e-9); EXPECT_LE(p.y, 4 + 1e-9);
            EXPECT_GE(p.z, -1e-9); EXPECT_LE(p.z, 2.5 + 1e-9);
        }
    }
}

TEST(Tracer, AnechoicRayPathEndsAtFirstWall) {
    Scene scene;
    scene.room = {4, 4, 4};
    scene.source.position = {2, 2, 2};
    scene.settings.maxOrder = 5;
    EXPECT_EQ(rayPath(scene, 7).size(), 2u);
}

// ---- Random directions --------------------------------------------------------------

TEST(Rng, DirectionsAreUnitAndDeterministic) {
    for (std::uint64_t i = 0; i < 1000; ++i) {
        const Vec3 d = rayDirection(42, i);
        EXPECT_NEAR(norm(d), 1.0, 1e-12);
        EXPECT_EQ(d, rayDirection(42, i));
    }
    EXPECT_NE(rayDirection(1, 0), rayDirection(2, 0));
}

TEST(Rng, DirectionsAreUniformOnSphere) {
    constexpr int n = 200000;
    Vec3 mean{};
    double zz = 0;
    for (int i = 0; i < n; ++i) {
        const Vec3 d = rayDirection(7, static_cast<std::uint64_t>(i));
        mean += d;
        zz += d.z * d.z;
    }
    mean = mean / n;
    EXPECT_NEAR(mean.x, 0, 0.01);
    EXPECT_NEAR(mean.y, 0, 0.01);
    EXPECT_NEAR(mean.z, 0, 0.01);
    EXPECT_NEAR(zz / n, 1.0 / 3.0, 0.01); // E[z^2] = 1/3 for a uniform sphere
}
