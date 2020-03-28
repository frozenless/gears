#pragma once

#include "game.hpp"
#include "gear.hpp"

class Gears : public lamp::Game
{
public:
	Gears();

protected:
	void init()    final;
	void release() final;

	void update(float delta_time) final;
	void draw() final;

	void input(int32_t action, int32_t key) override;

	static lamp::gl::mesh_ptr create(const Gear& gear);

private:
	bool _show_menu;
	Gear _gear;
};