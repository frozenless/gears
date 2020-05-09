#include "gears.hpp"
#include "components/selectable.hpp"

#include "engine/systems/renderer.hpp"
#include "engine/systems/physics.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"
#include "engine/components/rigidbody.hpp"

#include "engine/material.hpp"
#include "engine/random.inl"

#include "physics/utils.hpp"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "importer.hpp"
#include "editor.hpp"
#include "assets.inl"

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

const btVector3 axis(0, 0, 1);

lamp::gl::program_ptr model_shader;

lamp::gl::buffer_ptr camera_position_buffer;
lamp::gl::buffer_ptr camera_buffer;
lamp::gl::buffer_ptr light_buffer;

lamp::v3 camera_position;

void Gears::input(const int32_t action, const int32_t key)
{
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_TAB) {

            _show_menu = !_show_menu;

        } else if (key == GLFW_MOUSE_BUTTON_1) {

			auto hit = _physics.ray(_camera.to_world(_mouse));

			if (hit.hasHit()) {

                if (int32_t index = hit.m_collisionObject->getUserIndex();
                            index != -1) {

                    _ecs.entities.each<selectable>([](entityx::Entity entity, selectable &selectable) {
                        selectable.selected = false;
                    });

                    auto id     = _ecs.entities.create_id(hit.m_collisionObject->getUserIndex());
                    auto entity = _ecs.entities.get(id);

                    entity.component<selectable>()->selected = true;
                }
			}

		} else {
			Game::input(action, key);
		}
	}
}

void Gears::init()
{
    glfwSetScrollCallback(static_cast<GLFWwindow*>(_window), [](GLFWwindow* ptr, const double, const double offset) noexcept {

        static float value = 60.0f;

        value -= static_cast<float>(offset * 2.0f);
        value  = glm::clamp(value, 1.0f, 60.0f);

        auto& camera = static_cast<Game*>(glfwGetWindowUserPointer(ptr))->camera();

        camera.fov(value);
        camera.update();

        const std::array<lamp::m4, 1> uniforms = { camera.proj() };
        camera_buffer->sub_data(std::make_pair(uniforms.data(), uniforms.size()), 1);
    });

	auto model_vert = lamp::Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto model_frag = lamp::Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

	model_shader = lamp::Assets::create(model_vert, model_frag);

	_light.position = { 0.0f, 0.0f, 10.0f };
	_light.ambient  = 0.4f;
	_light.diffuse  = 0.7f;
	_light.specular = 0.65f;

	_ecs.systems.add<lamp::systems::Renderer>()->init();
	_ecs.systems.add<lamp::systems::Physics>();

	gear.inner = 0.7f;
	gear.outer = 3.0f;
	gear.depth = 0.7f;
	gear.width = 0.5f;
	gear.teeth = 13;

	create_plane();

	camera_position = { 0.0f, 0.0f, 20.0f };

	_camera.view(camera_position);
	_camera.update();

	const std::array<lamp::m4, 2> u_camera = { _camera.view(), _camera.proj() };

	camera_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, std::make_pair(u_camera.data(), u_camera.size()), GL_STATIC_DRAW);
	camera_buffer->bind(0);

	const std::array<lamp::components::light, 1> u_light = { _light };

	light_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, std::make_pair(u_light.data(), u_light.size()), GL_STATIC_DRAW);
	light_buffer->bind(1);

	const std::array<lamp::v3, 1> u_camera_position = { camera_position };

	camera_position_buffer = lamp::Assets::create(GL_UNIFORM_BUFFER, std::make_pair(u_camera_position.data(), u_camera_position.size()), GL_STATIC_DRAW);
    camera_position_buffer->bind(4);

	lamp::Editor::init(static_cast<GLFWwindow*>(_window));
}

void Gears::release()
{
	model_shader->release();

	lamp::Editor::release();
}

void Gears::update(float delta_time)
{
	_ecs.systems.update<lamp::systems::Physics>(delta_time);

	bool upload = false;

	constexpr float camera_speed = 6.1f;

	if (glfwGetKey(static_cast<GLFWwindow*>(_window), GLFW_KEY_LEFT) == GLFW_PRESS) {

         camera_position.x -= camera_speed * delta_time;
        _camera.view(camera_position);

        upload = true;
    }

	if (glfwGetKey(static_cast<GLFWwindow*>(_window), GLFW_KEY_RIGHT) == GLFW_PRESS) {

         camera_position.x += camera_speed * delta_time;
        _camera.view(camera_position);

        upload = true;
	}

	if (upload)
    {
        const std::array<lamp::m4, 1> uniforms = { _camera.view() };
        camera_buffer->sub_data(std::make_pair(uniforms.data(), uniforms.size()), 0);

        const std::array<lamp::v3, 1> u_camera_position = { camera_position };
        camera_position_buffer->data(u_camera_position);
	}

	const float value  = Game::timer.elapsed() * 2.0f;
	const float radius = 10.0f;

    _light.position.x = radius * std::cosf(value);
	_light.position.z = radius * std::sinf(value);

    const std::array<lamp::components::light, 1> uniforms = { _light };
    light_buffer->data(uniforms);
}

void Gears::draw()
{
	lamp::gl::Renderer::clear();

	_ecs.systems.update<lamp::systems::Renderer>(0);

	lamp::Editor::begin();

	if (_show_editor) {

		lamp::Editor::draw(_light);

		const std::array<lamp::components::light, 1> uniforms = { _light };
		light_buffer->data(uniforms);

		_ecs.entities.each<selectable>([](entityx::Entity entity, selectable& selectable) {

			if (selectable.selected)
			{
				lamp::Editor::draw(entity.component<lamp::components::renderer>()->material);
			}
		});
	}

	if (_show_menu) {

		static int32_t count = 3;
		static auto position = glm::zero<lamp::v3>();

		ImGui::Begin("Generator", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::InputFloat3("Position",    glm::value_ptr(position), 1);
		ImGui::InputFloat("Outer Radius", &gear.outer, 0.1f);
		ImGui::InputFloat("Inner Radius", &gear.inner, 0.1f);

		ImGui::InputInt("Teeth", &gear.teeth, 2);
        ImGui::InputInt("Count", &count,      1);

		if (ImGui::Button("Generate"))
        {
            const float offset = gear.outer * 2.0f;

            auto last_one = create(position, lamp::Random::color(), true, static_cast<float>(count + 0.5f));

            for (uint32_t i = 1; i < count; i++) {

                bool middle = false;

                if (i % 2 == 0) middle = true;

                auto new_one = create({ position.x + (i * offset), position.y, position.z }, lamp::Random::color(), middle);

                _physics.add_constraint(new btGearConstraint(
                *last_one.component<lamp::components::rigidbody>()->body,
                 *new_one.component<lamp::components::rigidbody>()->body, axis, axis), true);

                last_one = new_one;
            }
        }

		ImGui::End();

        _ecs.entities.each<selectable>([](entityx::Entity entity, selectable& selectable) {

            if (selectable.selected)
            {
                ImGui::Begin("Gear", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

                if (ImGui::Button("Remove"))
                {
                    auto body = entity.component<lamp::components::rigidbody>()->body;
                    auto constraint = body->getConstraintRef(0);

                    body->setLinearFactor({ 1, 1, 1 });
                    body->setAngularFactor({ 1, 1, 1 });

                    body->applyCentralImpulse({ 0, 0, -25.0});
                    body->removeConstraintRef(constraint);
                }

                ImGui::End();
            }
        });
	}

	lamp::Editor::end();
}

lamp::gl::mesh_ptr Gears::create_gear() const
{
	uint32_t index = 0;

	std::vector<lamp::v3> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve(static_cast<size_t>(80) * gear.teeth);
	indices.reserve(static_cast<size_t>(66)  * gear.teeth);

	const float half_depth = gear.depth * 0.5f;

	const float r0 = gear.inner;
	const float r1 = gear.outer - half_depth;
	const float r2 = gear.outer + half_depth;

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
		lamp::v3 normal(0.0f, 0.0f, 1.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     half_width), normal, // 0
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     half_width), normal, // 1
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, half_width), normal, // 2
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, half_width), normal, // 3
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, half_width), normal, // 4

            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, half_width), normal, // 5
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, half_width), normal  // 6
		});

		indices.insert(indices.end(), {
			index,     index + 1, index,
			index + 1, index + 2, index,
			index,     index + 2, index + 3,
			index + 2, index + 4, index + 3,

            index + 1, index + 5, index + 2,
            index + 5, index + 6, index + 2
		}); index += 7;

		// back face
		normal.z = -1.0f;
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal, // 0
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal, // 1
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal, // 2
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal, // 3
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half_width), normal, // 4
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half_width), normal, // 5

            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal, // 6
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal, // 7
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal  // 8
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2,
			index + 2, index + 3, index + 4,
			index + 3, index + 5, index + 4,

            index + 6, index + 7, index,
            index + 7, index + 8, index
		}); index += 9;

		// draw outward faces of teeth
		normal = lamp::v3(v1, -u1, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,      half_width), normal,
			lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half_width), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half_width), normal,
			lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half_width), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(v2, -u2, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half_width), normal,
			lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half_width), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half_width), normal,
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da,  half_width), normal,
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half_width), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

        // draw inside radius cylinder
		const lamp::v3 normal_1(-cos_ta,     -sin_ta,     0.0f);
		const lamp::v3 normal_2(-cos_ta_4da, -sin_ta_4da, 0.0f);

		vertices.insert(vertices.end(), {
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal_1,
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,      half_width), normal_1,
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half_width), normal_2,
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da,  half_width), normal_2
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

entityx::Entity Gears::create(const lamp::v3& position, const lamp::math::rgb& color, const bool middle, const float speed)
{
	auto entity   = _ecs.entities.create();
	auto renderer = entity.assign<lamp::components::renderer>();
	entity.assign<lamp::components::transform>();
    entity.assign<selectable>();

	renderer->shader = model_shader;
    renderer->mesh   = create_gear();

	renderer->material = std::make_shared<lamp::Material>();
	renderer->material->color = color;

	btVector3 inertia;
	constexpr float mass = 6.28f;

	auto shape = new btCylinderShapeZ(btVector3(gear.outer, gear.outer, gear.width / 2.0f));
	shape->calculateLocalInertia(mass,inertia);

	btRigidBody::btRigidBodyConstructionInfo info(mass, nullptr, shape, inertia);
	info.m_startWorldTransform.setOrigin({ position.x, position.y, position.z });

	if (middle)
	{
		const float offset = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;

		info.m_startWorldTransform.setRotation({ axis, offset });
	}

	auto body = new btRigidBody(info);
	body->setLinearFactor({ 0, 0, 0 });
	body->setUserIndex(static_cast<int32_t>(entity.id().id()));

	if (speed > 0.0f)
	{
		body->setAngularVelocity({ 0, 0, speed });
		// body->setLinearVelocity(btVector3(0.3f, 0.0f, 0.0f));
	}
	else
	{
		body->setAngularFactor({ 0, 0, 1 });
	}

	entity.assign<lamp::components::rigidbody>()->body = body;

	_physics.add_rigidbody(body);

	return entity;
}

lamp::gl::mesh_ptr Gears::create_rail(const int32_t length) const
{
    uint32_t index = 0;

    std::vector<lamp::v3> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(static_cast<size_t>(80) * gear.teeth);
    indices.reserve(static_cast<size_t>(66) * gear.teeth);

    const float half_depth = gear.depth * 0.5f;

    const float r0 = gear.outer - 1.0f;
    const float r1 = gear.outer - half_depth;
    const float r2 = gear.outer + half_depth;

    const float da         = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;
    const float half_width = gear.width * 0.5f;

    for (int32_t i = 0; i < 1; i++) {

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
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,     half_width), normal,
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     half_width), normal,
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,     half_width), normal,
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
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     half_width), normal,
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
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,      half_width), normal,
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -half_width), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half_width), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal
        });

        indices.insert(indices.end(), {
            index,     index + 1, index + 2,
            index + 1, index + 3, index + 2
        }); index += 4;

        normal = lamp::v3(cos_ta, sin_ta, 0.0f);
        vertices.insert(vertices.end(), {
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  half_width), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -half_width), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half_width), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal
        });

        indices.insert(indices.end(), {
            index,     index + 1, index + 2,
            index + 1, index + 3, index + 2
        }); index += 4;

        normal = lamp::v3(v2, -u2, 0.0f);
        vertices.insert(vertices.end(), {
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  half_width), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -half_width), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half_width), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal
        });

        indices.insert(indices.end(), {
            index,     index + 1, index + 2,
            index + 1, index + 3, index + 2
        }); index += 4;

        normal = lamp::v3(cos_ta, sin_ta, 0.0f);
        vertices.insert(vertices.end(), {
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  half_width), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -half_width), normal,
            lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da,  half_width), normal,
            lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -half_width), normal
        });

        indices.insert(indices.end(), {
            index,     index + 1, index + 2,
            index + 1, index + 3, index + 2
        }); index += 4;

        // draw inside radius cylinder
        const lamp::v3 normal_1(-cos_ta,     -sin_ta,     0.0f);
        const lamp::v3 normal_2(-cos_ta_4da, -sin_ta_4da, 0.0f);

        vertices.insert(vertices.end(), {
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -half_width), normal_1,
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,      half_width), normal_1,
            lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -half_width), normal_2,
            lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da,  half_width), normal_2
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

void Gears::create_plane()
{
    constexpr lamp::v3 position(0.0f, -4.0f, 0.0f);

    auto plane     = _ecs.entities.create();
    auto renderer  = plane.assign<lamp::components::renderer>();

    renderer->material = std::make_shared<lamp::Material>();
    renderer->material->color = { 0.8f, 0.4f, 0.4f };
    renderer->shader   = model_shader;
    renderer->mesh     = lamp::Importer::import("models/plane.obj");

    plane.assign<lamp::components::transform>()->world = glm::translate(glm::identity<lamp::m4>(), position);

    btRigidBody::btRigidBodyConstructionInfo info(0.0f,
                                                  new btDefaultMotionState(lamp::utils::from(position, glm::identity<lamp::quat>())),
                                                  new btStaticPlaneShape({ 0, 1.0f, 0 }, 0));

    _physics.add_rigidbody(new btRigidBody(info));
}

int main()
{
	Gears game; game.run({ "Gears", 8, false, false, true }, { 1280, 768 });

	return 0;
}