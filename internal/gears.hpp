#include "game.hpp"
#include "gear.hpp"

#include "engine/mesh.hpp"
#include "math/rgb.hpp"

using namespace lamp;

class Gears : public Game
{
protected:
	void init()    final;
	void release() final;

	void update(float delta_time) final;
	void draw() final;

	void input(int32_t action, int32_t key) final;

    entityx::Entity create(const v3& position, const math::rgb& color, bool middle, float speed = 0.0f);

private:
    [[nodiscard]] std::shared_ptr<Mesh> create_gear() const;

	bool _show_menu = false;

	Gear gear;
};