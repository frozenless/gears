#pragma once

#include <entityx/entityx.h>

class Rotation : public entityx::System<Rotation>
{
public:
	void update(entityx::EntityManager& es, entityx::EventManager& ev, entityx::TimeDelta dt) final;
};
