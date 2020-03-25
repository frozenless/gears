#pragma once

#include "game.hpp"
#include "gear.hpp"

class Gears : public lamp::Game
{
protected:
	void init()    final;
	void release() final;

	void update(float delta_time) final;
	void draw() final;

public:
	lamp::gl::mesh_ptr add_gear(const gear& gear);
};