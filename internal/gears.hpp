#include "game.hpp"
#include "gear.hpp"

#include "engine/mesh.hpp"

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
    void create_plane(const lamp::math::rgb& color, const lamp::v3& position, const lamp::v3& normal, const lamp::v3& axes, float angle);

    [[nodiscard]] std::shared_ptr<lamp::Mesh> create_gear()               const;
    [[nodiscard]] std::shared_ptr<lamp::Mesh> create_rail(int32_t length) const;

	bool _show_menu = false;

	Gear gear;
};