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
    [[nodiscard]] lamp::gl::mesh_ptr create_gear()               const;
    [[nodiscard]] lamp::gl::mesh_ptr create_rail(int32_t length) const;

	bool _show_menu = false;

    void create_plane();

	Gear gear;
};