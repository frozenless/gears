#pragma once

#include "game.hpp"
#include "gear.hpp"

struct Vertex
{
	lamp::v3 pos;
	lamp::v3 normal;

	Vertex(const glm::vec3& p, const glm::vec3& n)
		: pos(p)
		, normal(n)
	{
	}
};

class Gears : public lamp::Game
{
protected:
	void init()    final;
	void release() final;

	void update(float delta_time) final;
	void draw() final;

public:
	static int32_t new_vertex(std::vector<Vertex>* vBuffer, float x, float y, float z, const glm::vec3& normal);

	lamp::gl::mesh_ptr add_gear(const gear& gear);
};