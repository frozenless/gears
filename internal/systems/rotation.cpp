#include "rotation.hpp"
#include "game.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/position.hpp"

#include "components/rotation.hpp"

void Rotation::update(entityx::EntityManager &es, entityx::EventManager &, entityx::TimeDelta dt)
{
	es.each<lamp::components::transform, lamp::components::position, rotation>([](entityx::Entity,
            lamp::components::transform& transform, lamp::components::position& position, rotation& rotation) {

		const float amount = lamp::Game::timer.elapsed() * rotation.speed;

		transform.world  = glm::translate(glm::identity<lamp::m4>(), lamp::v3(position.x, position.y, position.z));
		transform.world *= glm::mat4_cast(glm::quat(lamp::v3(0.0f, 0.0f, amount)));
	});
}
