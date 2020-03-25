#include "gears.hpp"

#include "systems/rotation.hpp"
#include "components/rotation.hpp"

#include "engine/systems/renderer.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"
#include "engine/components/position.hpp"

#include "engine/material.hpp"
#include "engine/camera.hpp"
#include "engine/random.inl"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "editor.hpp"
#include "assets.inl"

#include <imgui.h>

lamp::gl::mesh_ptr gear_mesh;

lamp::gl::program_ptr model_shader;
lamp::gl::program_ptr debug_shader;

lamp::gl::buffer_ptr camera_buffer;
lamp::gl::buffer_ptr light_buffer;

constexpr lamp::v2 size(1024, 768);

lamp::Camera camera(size);

void Gears::init()
{
	auto model_vert = lamp::Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto model_frag = lamp::Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

	auto debug_vert = lamp::Assets::create("shaders/glsl/debug.vert", GL_VERTEX_SHADER);
	auto debug_frag = lamp::Assets::create("shaders/glsl/debug.frag", GL_FRAGMENT_SHADER);

	debug_shader = lamp::Assets::create(debug_vert, debug_frag);
	model_shader = lamp::Assets::create(model_vert, model_frag);

	gear g { };
	g.inner_radius = 0.7f;
	g.outer_radius = 3.0f;
	g.num_teeth    = 18;
	g.tooth_depth  = 0.7f;
	g.width        = 0.5f;

	gear_mesh = add_gear(g);

	_light.color    = lamp::rgb(1.0f);
	_light.position = lamp::v3( 10.0f, 10.0f, 15.0f);
	_light.ambient  = 0.1f;
	_light.diffuse  = 0.9f;

	_ecs.systems.add<lamp::systems::Renderer>();
	_ecs.systems.add<Rotation>();

	auto first_gear = _ecs.entities.create();
	first_gear.assign<lamp::components::transform>();
	auto first_gear_renderer = first_gear.assign<lamp::components::renderer>();
	auto first_gear_position = first_gear.assign<lamp::components::position>();
	auto first_gear_rotation = first_gear.assign<rotation>();
	first_gear_rotation->speed = 0.4f;

	first_gear_renderer->shader   = model_shader;
	first_gear_renderer->material = std::make_shared<lamp::Material>(lamp::Random::linear(glm::zero<lamp::v3>(), glm::one<lamp::v3>()));;
	first_gear_renderer->mesh     = gear_mesh;

	first_gear_position->x = -3.0f;
	first_gear_position->y =  0.0f;
	first_gear_position->z =  0.0f;

	auto second_gear = _ecs.entities.create();
	second_gear.assign<lamp::components::transform>();
	auto second_gear_renderer = second_gear.assign<lamp::components::renderer>();
	auto second_gear_position = second_gear.assign<lamp::components::position>();
	auto second_gear_rotation = second_gear.assign<rotation>();
	second_gear_rotation->speed = 0.4f;

	second_gear_renderer->shader   = model_shader;
	second_gear_renderer->material = std::make_shared<lamp::Material>(lamp::Random::linear(glm::zero<lamp::v3>(), glm::one<lamp::v3>()));;
	second_gear_renderer->mesh     = gear_mesh;

	second_gear_position->x = 3.0f;
	second_gear_position->y = 0.0f;
	second_gear_position->z = 0.0f;

	camera.view(lamp::v3(0.0f, 0.0f, -10.0f));
	camera.perspective();

	std::vector<lamp::m4> camera_uniforms = { camera.view(), camera.proj() };

	camera_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, camera_uniforms, GL_STATIC_DRAW);
	camera_buffer->bind_base(0);

	std::vector<lamp::components::light> light_uniforms = { _light };

	light_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, light_uniforms, GL_STATIC_DRAW);
	light_buffer->bind_base(1);

	lamp::Editor::init(_window);
}

void Gears::release()
{
	debug_shader->release();
	model_shader->release();

	lamp::Editor::release();
}

void Gears::update(float delta_time)
{
	_ecs.systems.update<Rotation>(delta_time);
}

void Gears::draw()
{
	lamp::gl::Renderer::clear();

	_ecs.systems.update<lamp::systems::Renderer>(0);

	if (_show_editor)
	{
		lamp::Editor::begin();

		lamp::Editor::draw(_light);

		lamp::Editor::end();

		std::vector<lamp::components::light> light_uniforms = { _light };
		light_buffer->set_data(light_uniforms);
	}
}

lamp::gl::mesh_ptr Gears::add_gear(const gear& gear)
{
	uint32_t index = 0;

	std::vector<lamp::v3> vertices;
	std::vector<uint32_t> indices;

	const float r0 = gear.inner_radius;
	const float r1 = gear.outer_radius - gear.tooth_depth / 2.0f;
	const float r2 = gear.outer_radius + gear.tooth_depth / 2.0f;
	const float da = 2.0f * glm::pi<float>() / static_cast<float>(gear.num_teeth) / 4.0f;

	const float half = gear.width * 0.5f;

	for (int32_t i = 0; i < gear.num_teeth; i++) {

		const float ta = static_cast<float>(i) * 2.0f * glm::pi<float>() / static_cast<float>(gear.num_teeth);

		const float cos_ta = cos(ta);
		const float cos_ta_1da = cos(ta + da);
		const float cos_ta_2da = cos(ta + 2.0f * da);
		const float cos_ta_3da = cos(ta + 3.0f * da);
		const float cos_ta_4da = cos(ta + 4.0f * da);

		const float sin_ta = sin(ta);
		const float sin_ta_1da = sin(ta + da);
		const float sin_ta_2da = sin(ta + 2.0f * da);
		const float sin_ta_3da = sin(ta + 3.0f * da);
		const float sin_ta_4da = sin(ta + 4.0f * da);

		const float u2 = r1 * cos_ta_3da - r2 * cos_ta_2da;
		const float v2 = r1 * sin_ta_3da - r2 * sin_ta_2da;

		float u1 = r2 * cos_ta_1da - r1 * cos_ta;
		float v1 = r2 * sin_ta_1da - r1 * sin_ta;

		const float len = std::sqrt(u1 * u1 + v1 * v1);

		u1 /= len;
		v1 /= len;

		// front face
		lamp::v3 normal = lamp::v3(0.0f, 0.0f, 1.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     half), normal,
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     half), normal,
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half), normal,
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, half), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2,
			index + 2, index + 3, index + 4,
			index + 3, index + 5, index + 4
		}); index += 6;

		// front sides of teeth
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     half), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// back face
		normal = lamp::v3(0.0f, 0.0f, -1.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half), normal,
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half), normal,
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half), normal,
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2,
			index + 2, index + 3, index + 4,
			index + 3, index + 5, index + 4
		}); index += 6;

		// back sides of teeth
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half), normal,
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// draw outward faces of teeth
		normal = lamp::v3(v1, -u1, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,      half), normal,
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(v2, -u2, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da,  half), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// draw inside radius cylinder
		vertices.insert(vertices.end(), {
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half), lamp::v3(-cos_ta,     -sin_ta,     0.0f),
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,      half), lamp::v3(-cos_ta,     -sin_ta,     0.0f),
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half), lamp::v3(-cos_ta_4da, -sin_ta_4da, 0.0f),
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da,  half), lamp::v3(-cos_ta_4da, -sin_ta_4da, 0.0f)
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;
	}

	lamp::gl::Layout layout;
	layout.add<float>(3);
	layout.add<float>(3);

	return lamp::Assets::create(vertices, indices, layout, GL_TRIANGLES, GL_STATIC_DRAW);
}

int main()
{
	Gears game; game.run({ "Gears", size, 8, false, false });

	return 0;
}