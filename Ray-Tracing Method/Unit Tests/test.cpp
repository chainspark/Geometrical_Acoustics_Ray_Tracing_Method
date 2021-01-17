
#define _USE_MATH_DEFINES
#pragma once

#include "gtest/gtest.h"
#include "../Ray-Tracing Method/Ray_Tracing_Algorithm.cpp"

long int ray::ray_count = 0;
int source::source_count = 0;

//TODO : Add all unit tests
	TEST(GenerateRoom, RoomTest) {

		box room;

		float cube_side = 4.0;;
		room.height = cube_side;
		room.length = cube_side;
		room.width = cube_side;

		room.generate_box_room();
	}

	TEST(GenerateSource, SourceTest) {
		source source1;
		source1.number_of_rays = 10000;
		ASSERT_EQ(source1.source_count, 1);
		source1.generate_source();
	}

