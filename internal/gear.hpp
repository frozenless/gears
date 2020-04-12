#pragma once

#include "types.hpp"

struct Gear
{
	float width;

	float inner_radius;
	float outer_radius;

	float   depth;
	int32_t teeth;

	lamp::v3 position;
};