#include "gears.hpp"

#include "engine/systems/renderer.hpp"
#include "engine/systems/physics.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"
#include "engine/components/rigidbody.hpp"

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

void mouse_actions(GLFWwindow* ptr, const int32_t button, const int32_t  action, int)
{
	if (button == GLFW_MOUSE_BUTTON_1 &&
	    action == GLFW_PRESS) {

		auto game = static_cast<Gears*>(glfwGetWindowUserPointer(ptr));
		auto hit  = game->physics().ray(camera.screen_to_world(mouse));

		if (hit.hasHit()) {

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
	glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(_window), mouse_actions);
	glfwSetCursorPosCallback(static_cast<GLFWwindow*>(_window), [](GLFWwindow*, const double x, const double y) noexcept {
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

	_light.position = lamp::v3( 2.5f, 2.5f, 5.0f);
	_light.ambient  = 0.1f;
	_light.diffuse  = 0.7f;
	_light.specular = 0.3f;

	_ecs.systems.add<lamp::systems::Renderer>()->init();
	_ecs.systems.add<lamp::systems::Physics>();

	gear.position     = glm::zero<lamp::v3>();
	gear.inner_radius = 0.7f;
	gear.outer_radius = 3.0f;
	gear.depth = 0.7f;
	gear.width = 0.5f;
	gear.teeth = 13;

	const btVector3 axis(0, 0, 1);
	const float offset = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;

	auto right  = create(lamp::v3( 6.0f, 0.0f, 0.0f), lamp::Random::color(), 0.0f, 3.5f);
	auto center = create(lamp::v3( 0.0f, 0.0f, 0.0f), lamp::Random::color(),     offset, 0);
	auto left   = create(lamp::v3(-6.0f, 0.0f, 0.0f), lamp::Random::color(), 0.0f, 0);

	_physics.add_constraint(new btGearConstraint(
	*right.component<lamp::components::rigidbody>()->body,
	*center.component<lamp::components::rigidbody>()->body, axis, axis), true);

	_physics.add_constraint(new btGearConstraint(
	*center.component<lamp::components::rigidbody>()->body,
	*left.component<lamp::components::rigidbody>()->body, axis, axis), true);

	auto debug    = _ecs.entities.create();
	auto renderer = debug.assign<lamp::components::renderer>();
	debug.assign<lamp::components::transform>()->world = glm::identity<lamp::m4>();

	renderer->shader   = debug_shader;
	renderer->mesh     = debug_mesh;
	renderer->material = nullptr;

	camera.view(lamp::v3(0.0f, 0.0f, -15.0f));
	camera.perspective();

	std::array<lamp::m4, 2> camera_uniforms = { camera.view(), camera.proj() };

	camera_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, camera_uniforms.data(), camera_uniforms.size(), GL_STATIC_DRAW);
	camera_buffer->bind_base(0);

	std::array<lamp::components::light, 1> light_uniforms = { _light };

	light_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, light_uniforms.data(), light_uniforms.size(), GL_STATIC_DRAW);
	light_buffer->bind_base(1);

	lamp::Editor::init(static_cast<GLFWwindow*>(_window));
}

void Gears::release()
{
	debug_shader->release();
	model_shader->release();

	lamp::Editor::release();
}

void Gears::update(float delta_time)
{
	_ecs.systems.update<lamp::systems::Physics>(delta_time);
}

void Gears::draw()
{
	lamp::gl::Renderer::clear();

	_ecs.systems.update<lamp::systems::Renderer>(0);

	lamp::Editor::begin();

	if (_show_editor) {

		lamp::Editor::draw(_light);

		std::array<lamp::components::light, 1> uniforms = { _light };
		light_buffer->data(uniforms);

		/*auto entity    = _ecs.entities.get(entity_id);
		if (entity_id != entityx::Entity::INVALID)
		{
			lamp::Editor::draw("Material", entity.component<lamp::components::renderer>()->material);
		}*/
	}

	if (_show_menu) {

		static auto color = lamp::Random::color();

		ImGui::Begin("Gear", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::InputFloat3("Position", glm::value_ptr(gear.position), 1);
		ImGui::ColorEdit3("Color",        static_cast<float*>(color));
		ImGui::InputFloat("Outer Radius", &gear.outer_radius, 0.1f);
		ImGui::InputFloat("Inner Radius", &gear.inner_radius, 0.1f);

		ImGui::InputInt("Teeth", &gear.teeth, 2);

		if (ImGui::Button("Create"))
		{
			create(gear.position, color, 3, 0.0f);

			color = lamp::Random::color();
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

	const float half_depth = gear.depth * 0.5f;

	const float r0 = gear.inner_radius;
	const float r1 = gear.outer_radius - half_depth;
	const float r2 = gear.outer_radius + half_depth;

	const float da         = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;
	const float half_width = gear.width * 0.5f;

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
				lamp::v3(r0 * cos_ta,     r0 * sin_ta, half_width), normal,
				lamp::v3(r1 * cos_ta,     r1 * sin_ta, half_width), normal,
				lamp::v3(r0 * cos_ta,     r0 * sin_ta, half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half_width), normal,
				lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, half_width), normal,
				lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2,
			index + 2, index + 3, index + 4,
			index + 3, index + 5, index + 4
		}); index += 6;

		// front sides of teeth
		vertices.insert(vertices.end(), {
				lamp::v3(r1 * cos_ta,     r1 * sin_ta, half_width), normal,
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half_width), normal,
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// back face
		normal = lamp::v3(0.0f, 0.0f, -1.0f);
		vertices.insert(vertices.end(), {
				lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal,
				lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal,
				lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal,
				lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half_width), normal,
				lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2,
			index + 2, index + 3, index + 4,
			index + 3, index + 5, index + 4
		}); index += 6;

		// back sides of teeth
		vertices.insert(vertices.end(), {
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal,
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal,
				lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal,
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// draw outward faces of teeth
		normal = lamp::v3(v1, -u1, 0.0f);
		vertices.insert(vertices.end(), {
				lamp::v3(r1 * cos_ta,     r1 * sin_ta, half_width), normal,
				lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal,
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, half_width), normal,
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, half_width), normal,
				lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal,
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, half_width), normal,
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(v2, -u2, 0.0f);
		vertices.insert(vertices.end(), {
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, half_width), normal,
				lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half_width), normal,
				lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal,
				lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, half_width), normal,
				lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		// draw inside radius cylinder
		vertices.insert(vertices.end(), {
				lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), lamp::v3(-cos_ta, -sin_ta, 0.0f),
				lamp::v3(r0 * cos_ta,     r0 * sin_ta, half_width), lamp::v3(-cos_ta, -sin_ta, 0.0f),
				lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half_width), lamp::v3(-cos_ta_4da, -sin_ta_4da, 0.0f),
				lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, half_width), lamp::v3(-cos_ta_4da, -sin_ta_4da, 0.0f)
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

entityx::Entity Gears::create(const lamp::v3& position, const lamp::math::rgb& color, const float offset, const float speed)
{
	auto entity   = _ecs.entities.create();
	auto renderer = entity.assign<lamp::components::renderer>();
	entity.assign<lamp::components::transform>();

	renderer->shader   = model_shader;
	renderer->material = std::make_shared<lamp::Material>();
	renderer->material->color     = color;
	renderer->material->shininess = 32.0f;
	renderer->mesh = Gears::create(gear);

	btVector3 inertia;

	constexpr float mass   = 6.28f;
	const     float radius = gear.outer_radius;

	auto shape = new btCylinderShapeZ(btVector3(radius, radius, 0.5f));
	shape->calculateLocalInertia(mass,inertia);

	btRigidBody::btRigidBodyConstructionInfo info(mass, nullptr, shape, inertia);
	info.m_startWorldTransform.setOrigin(lamp::utils::from(position));

	if (offset > 0.0f)
	{
		info.m_startWorldTransform.setRotation(btQuaternion(btVector3(0, 0, 1), offset));
	}

	auto body  = new btRigidBody(info);
	body->setLinearFactor(btVector3(1, 0, 0));

	if (speed > 0.0f)
	{
		body->setAngularVelocity(btVector3(0, 0, speed));
		// body->setLinearVelocity(btVector3(0.3f, 0.0f, 0.0f));
	}
	else
	{
		body->setAngularFactor(btVector3(0, 0, 1));
	}

	entity.assign<lamp::components::rigidbody>()->body = body;

	_physics.add_rigidbody(body);

	return entity;
}

int main()
{
	Gears game; game.run({ "Gears", size, 8, false, false, true });

	return 0;
}