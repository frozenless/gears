#include "gears.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include "engine/systems/renderer.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"

#include "engine/material.hpp"
#include "engine/camera.hpp"
#include "engine/random.inl"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "editor.hpp"
#include "assets.inl"

lamp::gl::program_ptr model_shader;
lamp::gl::program_ptr debug_shader;

lamp::material_ptr gear_material;
lamp::gl::mesh_ptr gear_mesh;

lamp::gl::buffer_ptr camera_buffer;
lamp::gl::buffer_ptr light_buffer;

constexpr lamp::v2 size(1024, 768);

lamp::Camera camera(size);

void keyboard_actions(GLFWwindow* ptr, const int key, int, const int action, int)
{
	if (action == GLFW_PRESS)
	{
		auto gears = static_cast<Gears*>(glfwGetWindowUserPointer(ptr));

		switch (key) {
			case GLFW_KEY_ESCAPE: {
				gears->window().close();
				break;
			}
			case GLFW_KEY_E: {
				gears->toggle_editor();
				break;
			}
			case GLFW_KEY_W: {
				gears->toggle_wires();
				break;
			}
			default:
				break;
		}
	}
}

void Gears::init()
{
	glfwSetKeyCallback(_window, keyboard_actions);

	auto model_vert = lamp::Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto model_frag = lamp::Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

	auto debug_vert = lamp::Assets::create("shaders/glsl/debug.vert", GL_VERTEX_SHADER);
	auto debug_frag = lamp::Assets::create("shaders/glsl/debug.frag", GL_FRAGMENT_SHADER);

	debug_shader   = lamp::Assets::create(debug_vert,   debug_frag);
	model_shader   = lamp::Assets::create(model_vert,   model_frag);

	gear_material = std::make_shared<lamp::Material>(lamp::Random::linear(glm::zero<lamp::v3>(), glm::one<lamp::v3>()));

	gear g { };
	g.inner_radius = 1.0f;
	g.outer_radius = 4.0f;
	g.width = 0.5f;
	g.num_teeth = 20;
	g.tooth_depth = 0.7f;

	gear_mesh = add_gear(g);

	_light.color    = lamp::rgb(1.0f);
	_light.position = lamp::v3( 10.0f, 10.0f, 15.0f);
	_light.ambient  = 0.1f;
	_light.diffuse  = 0.9f;

	_ecs.systems.add<lamp::systems::Renderer>();

	auto gear = _ecs.entities.create();
	auto gear_renderer  = gear.assign<lamp::components::renderer>();
	auto gear_transform = gear.assign<lamp::components::transform>();
	gear_transform->world = glm::identity<lamp::m4>();

	gear_renderer->shader   = model_shader;
	gear_renderer->material = gear_material;
	gear_renderer->mesh     = gear_mesh;

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

int32_t Gears::new_vertex(std::vector<Vertex> *vBuffer, float x, float y, float z, const glm::vec3& normal)
{
	Vertex v(glm::vec3(x, y, z), normal);
	vBuffer->push_back(v);

	return static_cast<int32_t>(vBuffer->size()) - 1;
}

lamp::gl::mesh_ptr Gears::add_gear(const gear& gear)
{
	float    u1, v1;
	uint32_t ix0, ix1, ix2, ix3, ix4, ix5;

	const float r0 = gear.inner_radius;
	const float r1 = gear.outer_radius - gear.tooth_depth / 2.0f;
	const float r2 = gear.outer_radius + gear.tooth_depth / 2.0f;
	const float da = 2.0f * glm::pi<float>() / static_cast<float>(gear.num_teeth) / 4.0f;

	glm::vec3 normal;
	std::vector<Vertex> vBuffer;
	vBuffer.reserve(500);

	std::vector<float>  vertices;
	std::vector<uint32_t> indices;

	for (int i = 0; i < gear.num_teeth; i++) {

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

		u1 = r2 * cos_ta_1da - r1 * cos_ta;
		v1 = r2 * sin_ta_1da - r1 * sin_ta;

		const float len = sqrt(u1 * u1 + v1 * v1);

		u1 /= len;
		v1 /= len;

		const float u2 = r1 * cos_ta_3da - r2 * cos_ta_2da;
		const float v2 = r1 * sin_ta_3da - r2 * sin_ta_2da;

		// front face
		normal = glm::vec3(0.0f, 0.0f, 1.0f);
		ix0 = new_vertex(&vBuffer, r0 * cos_ta,     r0 * sin_ta,     gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r1 * cos_ta,     r1 * sin_ta,     gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r0 * cos_ta,     r0 * sin_ta,     gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gear.width * 0.5f, normal);
		ix4 = new_vertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, gear.width * 0.5f, normal);
		ix5 = new_vertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2,
			ix2, ix3, ix4,
			ix3, ix5, ix4
		});

		// front sides of teeth
		ix0 = new_vertex(&vBuffer, r1 * cos_ta,     r1 * sin_ta,     gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		// back face
		normal = glm::vec3(0.0f, 0.0f, -1.0f);
		ix0 = new_vertex(&vBuffer, r1 * cos_ta,     r1 * sin_ta, -gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r0 * cos_ta,     r0 * sin_ta, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r0 * cos_ta,     r0 * sin_ta, -gear.width * 0.5f, normal);
		ix4 = new_vertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, -gear.width * 0.5f, normal);
		ix5 = new_vertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2,
			ix2, ix3, ix4,
			ix3, ix5, ix4
		});

		// back sides of teeth
		ix0 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, -gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		// draw outward faces of teeth
		normal = glm::vec3(v1, -u1, 0.0f);
		ix0 = new_vertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		normal = glm::vec3(cos_ta, sin_ta, 0.0f);
		ix0 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		normal = glm::vec3(v2, -u2, 0.0f);
		ix0 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		normal = glm::vec3(cos_ta, sin_ta, 0.0f);
		ix0 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gear.width * 0.5f, normal);
		ix1 = new_vertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gear.width * 0.5f, normal);
		ix2 = new_vertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, gear.width * 0.5f, normal);
		ix3 = new_vertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, -gear.width * 0.5f, normal);

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});

		// draw inside radius cylinder
		ix0 = new_vertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, -gear.width * 0.5f, glm::vec3(-cos_ta, -sin_ta, 0.0f));
		ix1 = new_vertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, gear.width * 0.5f, glm::vec3(-cos_ta, -sin_ta, 0.0f));
		ix2 = new_vertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, -gear.width * 0.5f, glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));
		ix3 = new_vertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, gear.width * 0.5f, glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));

		indices.insert(indices.end(), {
			ix0, ix1, ix2,
			ix1, ix3, ix2
		});
	}

	vertices.reserve(4000);

	for (auto v : vBuffer) {
		vertices.insert(vertices.end(), { v.pos.x, v.pos.y, v.pos.z, v.normal.x, v.normal.y, v.normal.z });
	}

	lamp::gl::Layout layout;
	layout.add<float>(3, GL_FLOAT);
	layout.add<float>(3, GL_FLOAT);

	return lamp::Assets::create(vertices, indices, layout, GL_TRIANGLES, GL_UNSIGNED_INT, GL_STATIC_DRAW);
}

int main()
{
	Gears game; game.run({ "Gears", size, 8, false, false });

	return 0;
}