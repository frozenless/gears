#pragma once

struct gear
{
	float width;

	float inner_radius;
	float outer_radius;

	float   tooth_depth;
	int32_t teeth;

	lamp::v3 position;
};