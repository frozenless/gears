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

#include "physics/utils.hpp"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "editor.hpp"
#include "assets.inl"

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

lamp::gl::program_ptr model_shader;
lamp::gl::program_ptr debug_shader;

lamp::gl::buffer_ptr camera_buffer;
lamp::gl::buffer_ptr light_buffer;

lamp::gl::mesh_ptr debug_mesh;

constexpr lamp::v2  size(1280, 768);

lamp::Camera camera(size);
lamp::v2     mouse;

entityx::Entity::Id entity_id;

void mouse_actions(GLFWwindow* ptr, const int32_t button, const int32_t  action, int)
{
	if (button == GLFW_MOUSE_BUTTON_1 &&
	    action == GLFW_PRESS) {

		auto game = static_cast<Gears*>(glfwGetWindowUserPointer(ptr));
		auto hit  = game->physics().ray(camera.screen_to_world(mouse));

		if (hit.hasHit()) {
			auto entity = static_cast<entityx::Entity*>(hit.m_collisionObject->getUserPointer());
			entity_id = entity->id();
		}
	}
}

Gears::Gears()
	: _show_menu(false)
{
}

void Gears::input(int32_t action, int32_t key) {

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_TAB) {
			_show_menu = !_show_menu;
		} else {
			Game::input(action, key);
		}
	}
}

void Gears::init()
{
	glfwSetMouseButtonCallback(_window, mouse_actions);
	glfwSetCursorPosCallback(  _window, [](GLFWwindow*, const double x, const double y) noexcept {
		mouse = lamp::v2(x, y);
	});

	auto model_vert = lamp::Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto model_frag = lamp::Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

	auto debug_vert = lamp::Assets::create("shaders/glsl/debug.vert", GL_VERTEX_SHADER);
	auto debug_frag = lamp::Assets::create("shaders/glsl/debug.frag", GL_FRAGMENT_SHADER);

	debug_shader = lamp::Assets::create(debug_vert, debug_frag);
	model_shader = lamp::Assets::create(model_vert, model_frag);

	lamp::gl::Layout layout;
	layout.add<float>(3);
	layout.add<float>(3);

	std::vector<lamp::v3> vertices;
	std::vector<uint32_t> indices;

	debug_mesh = lamp::Assets::create(vertices, indices, layout, GL_LINES, GL_DYNAMIC_DRAW);

	_physics.init_renderer(debug_mesh, btIDebugDraw::DBG_DrawWireframe);

	_gear.position     = glm::zero<lamp::v3>();
	_gear.inner_radius = 0.7f;
	_gear.outer_radius = 3.0f;
	_gear.teeth        = 18;
	_gear.tooth_depth  = 0.7f;
	_gear.width        = 0.5f;

	_light.color    = lamp::rgb(1.0f);
	_light.position = lamp::v3( 10.0f, 10.0f, 15.0f);
	_light.ambient  = 0.1f;
	_light.diffuse  = 0.9f;

	_ecs.systems.add<lamp::systems::Renderer>();
	_ecs.systems.add<Rotation>();

	auto debug = _ecs.entities.create();
	auto debug_renderer = debug.assign<lamp::components::renderer>();
	debug.assign<lamp::components::transform>()->world = glm::identity<lamp::m4>();

	debug_renderer->shader   = debug_shader;
	debug_renderer->mesh     = debug_mesh;
	debug_renderer->material = nullptr;

	camera.view(lamp::v3(0.0f, 0.0f, -10.0f));
	camera.perspective();

	std::array<lamp::m4, 2> camera_uniforms = { camera.view(), camera.proj() };

	camera_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, camera_uniforms.data(), camera_uniforms.size(), GL_STATIC_DRAW);
	camera_buffer->bind_base(0);

	std::array<lamp::components::light, 1> light_uniforms = { _light };

	light_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, light_uniforms.data(), light_uniforms.size(), GL_STATIC_DRAW);
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

	lamp::Editor::begin();

	if (_show_editor) {

		lamp::Editor::draw(_light);

		std::array<lamp::components::light, 1> uniforms = { _light };
		light_buffer->set_data(uniforms);

		auto entity    = _ecs.entities.get(entity_id);
		if (entity_id != entityx::Entity::INVALID)
		{
			lamp::Editor::draw("Material", entity.component<lamp::components::renderer>()->material);
		}
	}

	if (_show_menu) {

		ImGui::Begin("Gear");
		ImGui::InputFloat3("Position", glm::value_ptr(_gear.position), 1);
		ImGui::InputFloat("Outer Radius", &_gear.outer_radius, 0.1f);
		ImGui::InputFloat("Inner Radius", &_gear.inner_radius, 0.1f);

		ImGui::InputInt("Teeth", &_gear.teeth, 1);

		if (ImGui::Button("Create")) {

			auto gear = _ecs.entities.create();
			auto gear_renderer = gear.assign<lamp::components::renderer>();
			auto gear_position = gear.assign<lamp::components::position>();
			gear.assign<lamp::components::transform>();
			gear.assign<rotation>()->speed = 0.4f;

			gear_renderer->shader   = model_shader;
			gear_renderer->material = std::make_shared<lamp::Material>(lamp::Random::linear(glm::zero<lamp::v3>(), glm::one<lamp::v3>()));;
			gear_renderer->mesh = create(_gear);

			gear_position->x = _gear.position.x;
			gear_position->y = _gear.position.y;
			gear_position->z = _gear.position.z;

			const float radius = _gear.outer_radius;
			auto shape = new btBoxShape(btVector3(radius, radius, 0.55f));
			auto object = new btCollisionObject();

			object->setWorldTransform(lamp::utils::from(_gear.position, glm::identity<glm::quat>()));
			object->setCollisionShape(shape);
			object->setUserPointer(&gear);

			_physics.add_collision(object, btCollisionObject::CF_NO_CONTACT_RESPONSE);
		}

		ImGui::End();
	}

	lamp::Editor::end();
}

lamp::gl::mesh_ptr Gears::create(const Gear& gear)
{
	uint32_t index = 0;

	std::vector<lamp::v3> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve(80 * gear.teeth);
	indices.reserve( 66 * gear.teeth);

	const float r0 = gear.inner_radius;
	const float r1 = gear.outer_radius - gear.tooth_depth / 2.0f;
	const float r2 = gear.outer_radius + gear.tooth_depth / 2.0f;

	const float da   = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;
	const float half = gear.width * 0.5f;

	for (int32_t i = 0; i < gear.teeth; i++) {

		const float ta = static_cast<float>(i) * 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth);

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
	Gears game; game.run({ "Gears", size, 8, false, false, true });

	return 0;
}