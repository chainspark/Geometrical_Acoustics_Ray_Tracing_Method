
#define _USE_MATH_DEFINES
#pragma once

#include "gtest/gtest.h"
#include "../Ray-Tracing Method/Ray_Tracing_Algorithm.cpp"

long int ray::ray_count = 0;
int source::source_count = 0;

/// Room Test
/// * Generate Plane 
/// * Generate Room  

TEST(RoomTest, GeneratePlane) {

	plane plane;
	vec3 point1(1.0, 2.0, 3.0), point2(2.0, 2.0, 3.0), point3(-2.0, -2.0, 3.0);
	plane = generate_plane(point1, point2, point3);

	vec3 normal(0.0, 0.0, -1.0);
	
	ASSERT_TRUE(plane.normal == normal);
	}

TEST(RoomTest,GenerateRoom) {
		box room;

		float cube_side = 2.0;;
		room.height = cube_side;
		room.length = cube_side;
		room.width = cube_side;

		room.generate_box_room();
		vec3  Bottom_normal(0.0, 0.0, 1.0),
			Left_normal(1.0, 0.0, 0.0),
			Front_normal(0.0, 1.0, 0.0),
			Top_normal(0.0, 0.0, -1.0),
			Right_normal(-1.0, 0.0, 0.0),
			Back_normal(0.0, -1.0, 0.0);

		ASSERT_TRUE(room.box_plane[0].normal == Bottom_normal);
		ASSERT_TRUE(room.box_plane[1].normal == Left_normal);
		ASSERT_TRUE(room.box_plane[2].normal == Front_normal);
		ASSERT_TRUE(room.box_plane[3].normal == Right_normal);
		ASSERT_TRUE(room.box_plane[4].normal == Back_normal);
		ASSERT_TRUE(room.box_plane[5].normal == Top_normal);
	}

	
/// Source Tests
/// * Create source
/// * Create Multiple Source
/// * Delete Source
/// * Reset Source

// Source Fixture
class SourceTest : public ::testing::Test {
protected:
	source Source;
	
	void SetUp() override {
		Source.number_of_rays = 10000;
		Source.generate_source();
		vec3 position(2.0, 2.0, 2.0);
		Source.position = position;
	}

	void TearDown() override {
		Source.delete_source();
	}
};
TEST_F(SourceTest, GenerateSource) {
		ASSERT_EQ(Source.source_count, 1);
		ASSERT_NE(Source.rays, nullptr);
	}

TEST_F(SourceTest, GenerateMultipleSources) {
	source source2;
	source2.number_of_rays = 10000;
	source2.generate_source();

	ASSERT_EQ(Source.source_count, 2);
	ASSERT_NE(source2.rays, nullptr);
}

TEST_F(SourceTest, DeleteSource) {
	Source.delete_source();
	ASSERT_EQ(Source.rays, nullptr);
	ASSERT_EQ(Source.source_count, 1);
}

TEST_F(SourceTest, ResetSource) {
	vec3 actual_position = Source.position;
	vec3 new_position(3.0, 4.0, 5.0);
	
	Source.rays[1].isource_position = new_position*2;
	ASSERT_FALSE(Source.rays[1].isource_position == actual_position);
	Source.reset_rays();
	ASSERT_TRUE(Source.rays[1].isource_position == actual_position);
}

/// Receiver Test
/// * Reset Receiver 
TEST(ReceiverTest, GenerateReceiver) {
	receiver Receiver;
}

TEST(ReceiverTest, ResetReceier) {
	receiver Receiver;
	Receiver.detect_count = 725;
	ASSERT_NE(Receiver.detect_count, 0);
	Receiver.reset();
	ASSERT_EQ(Receiver.detect_count, 0);
}
/// Reflection function
/// 