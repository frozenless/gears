#include "game.hpp"
#include "gear.hpp"

class Gears : public lamp::Game
{
protected:
	void init()    final;
	void release() final;

	void update(float delta_time) final;
	void draw() final;

	void input(int32_t action, int32_t key) final;

    entityx::Entity create(const lamp::v3& position, const lamp::math::rgb& color, bool middle, float speed = 0.0f);

private:
    [[nodiscard]] lamp::gl::mesh_ptr create()               const;
    [[nodiscard]] lamp::gl::mesh_ptr create(int32_t length) const;

	bool _show_menu = false;

	Gear gear;
};