#pragma once

#include "game.hpp"

class Gears : public lamp::Game
{
protected:
	void init()    final;
	void release() final;

	void update(lamp::f32 delta_time) final;
	void draw() final;
};