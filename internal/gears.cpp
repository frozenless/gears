#include "gears.hpp"

#include "engine/systems/rotation.hpp"
#include "engine/systems/camera.hpp"
#include "engine/systems/renderer.hpp"
#include "engine/systems/physics.hpp"
#include "engine/systems/light.hpp"
#include "engine/systems/editor.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"
#include "engine/components/rigidbody.hpp"
#include "engine/components/selectable.hpp"
#include "engine/components/position.hpp"
#include "engine/components/camera.hpp"
#include "engine/components/rotation.hpp"
#include "engine/components/viewport.hpp"

#include "engine/events/camera.hpp"
#include "engine/events/light.hpp"

#include "engine/editor.hpp"
#include "engine/camera.hpp"

#include "physics/utils.hpp"
#include "common/random.hpp"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "importer.hpp"
#include "assets.inl"

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

const btVector3 axis(0, 0, 1);

lamp::gl::program_ptr model_shader;

void Gears::input(const int32_t action, const int32_t key)
{
	if (action  == GLFW_PRESS) {
		if (key == GLFW_KEY_TAB) {

            _show_menu = !_show_menu;

        } else if (key == GLFW_MOUSE_BUTTON_1) {

		    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {

                auto entities = ecs.entities.entities_with_components<lamp::components::camera>();
                auto camera   = std::find_if(entities.begin(), entities.begin(), [](entityx::Entity e) {
                    return e.component<lamp::components::camera>()->main;
                });

                auto hit = physics.ray(lamp::Camera::to_world((*camera), _mouse));

                if (hit.hasHit()) {

                    if (const int32_t index = hit.m_collisionObject->getUserIndex(); index != -1) {

                        ecs.entities.each<lamp::components::selectable>([](entityx::Entity entity,
                                          lamp::components::selectable& selectable) {

                            selectable.selected = false;
                        });

                        auto id     = ecs.entities.create_id(hit.m_collisionObject->getUserIndex());
                        auto entity = ecs.entities.get(id);

                        entity.component<lamp::components::selectable>()->selected = true;
                    }
                }
			}
		} else {
			Game::input(action, key);
		}
	}
}

void Gears::init()
{
	auto model_vert = lamp::Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto model_frag = lamp::Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

	model_shader = lamp::Assets::create(model_vert, model_frag);

    ecs.systems.add<lamp::systems::Rotation>();

	gear.inner = 0.7f;
	gear.outer = 3.0f;
	gear.depth = 0.7f;
	gear.width = 0.5f;
	gear.teeth = 13;

    {
        auto entity = ecs.entities.create();
        entity.assign<lamp::components::transform>();
        auto position = entity.assign<lamp::components::position>();
        auto camera = entity.assign<lamp::components::camera>();
        camera->main = true;

        position->x = 0.0f;
        position->y = 0.0f;
        position->z = 20.0f;

        auto viewport = entity.assign<lamp::components::viewport>();
        viewport->width  = 1280.0f;
        viewport->height = 768.0f;
    }

    {
        auto entity = ecs.entities.create();
        entity.assign<lamp::components::position>();

        auto light  = entity.assign<lamp::components::light>();
        light->position = { 0.0f, 0.0f, 10.0f };
        light->ambient  = 0.4f;
        light->diffuse  = 0.7f;
        light->specular = 0.65f;

        auto rotation = entity.assign<lamp::components::rotation>();
        rotation->speed  = 2.0f;
        rotation->radius = 10.0f;
    }

	create_plane({ 0.8f, 0.4f, 0.4f }, { 0.0f, -4.0f,   0.0f }, { 0, 1, 0 }, { 0, 0, 1 },  0.0f);
    create_plane({ 0.6f, 0.4f, 0.2f }, { 0.0f, 16.0f, -20.0f }, { 0, 0, 1 }, { 1, 0, 0 }, 90.0f);

	lamp::ui::Editor::init(static_cast<GLFWwindow*>(_window));
}

void Gears::release()
{
	model_shader->release();

	lamp::ui::Editor::release();
}

void Gears::update(const float delta_time)
{
	ecs.systems.update<lamp::systems::Physics>(delta_time);
	ecs.systems.update<lamp::systems::Rotation>(delta_time);
	ecs.systems.update<lamp::systems::Camera>(delta_time);
	ecs.systems.update<lamp::systems::Light>(delta_time);

	constexpr float camera_speed = 6.1f;

    auto entities = ecs.entities.entities_with_components<lamp::components::camera>();
    auto entity   = std::find_if(entities.begin(), entities.begin(), [](entityx::Entity e) {
        return e.component<lamp::components::camera>()->main;
    });

    auto position = (*entity).component<lamp::components::position>();

	if (glfwGetKey(static_cast<GLFWwindow*>(_window), GLFW_KEY_LEFT) == GLFW_PRESS)
	{
        position->x -= camera_speed * delta_time;
    }

	if (glfwGetKey(static_cast<GLFWwindow*>(_window), GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
        position->x += camera_speed * delta_time;
	}
}

void Gears::draw()
{
	lamp::gl::Renderer::clear();

	ecs.systems.update<lamp::systems::Renderer>(0);

	lamp::ui::Editor::begin();

	ecs.systems.update<lamp::systems::Editor>(0);

	if (_show_menu) {

		static int32_t count = 3;
		static auto position = glm::zero<lamp::v3>();

		ImGui::Begin("Generator", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::InputFloat3("Position",    glm::value_ptr(position), "%.1f");
		ImGui::InputFloat("Outer Radius", &gear.outer, 0.1f);
		ImGui::InputFloat("Inner Radius", &gear.inner, 0.1f);

		ImGui::InputInt("Teeth", &gear.teeth, 2);
        ImGui::InputInt("Count", &count,      1);

		if (ImGui::Button("Generate"))
        {
            const float offset = gear.outer * 2.0f;

            lamp::v3 new_position = position;
            new_position.x -= (count * offset) / 2.0f - gear.outer;

            auto last_one = create(new_position, lamp::Random::color(), true, static_cast<float>(count + 0.5f));

            for (int32_t i = 1; i < count; i++) {

                new_position.x += offset;

                auto new_one = create(new_position, lamp::Random::color(), i % 2 == 0);

                physics.add_constraint(new btGearConstraint(
                *last_one.component<lamp::components::rigidbody>()->body,
                 *new_one.component<lamp::components::rigidbody>()->body, axis, axis), true);

                last_one = new_one;
            }
        }

		ImGui::End();

        auto entities = ecs.entities.entities_with_components<lamp::components::selectable>();
        auto entity   = std::find_if(entities.begin(), entities.begin(), [](entityx::Entity e) {
            return e.component<lamp::components::selectable>()->selected;
        });

        if ((*entity).has_component<lamp::components::rigidbody>())
        {
            ImGui::Begin("Gear", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            if (ImGui::Button("Remove"))
            {
                auto body = (*entity).component<lamp::components::rigidbody>()->body;

                body->setLinearFactor({ 1, 1, 1 });
                body->setAngularFactor({ 1, 1, 1 });

                body->applyCentralImpulse({ 0, 0, -25.0f });

                for (int32_t i = 0; i < body->getNumConstraintRefs(); i++)
                {
                    auto constraint = body->getConstraintRef(i);

                    body->removeConstraintRef(constraint);
                }
            }

            ImGui::End();
        }
	}

	lamp::ui::Editor::end();
}

std::shared_ptr<lamp::Mesh> Gears::create_gear() const
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

	const float da = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;
	const float hf = gear.width * 0.5f;

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
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,     hf), normal, // 0
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     hf), normal, // 1
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, hf), normal, // 2
            lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, hf), normal, // 3
            lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, hf), normal, // 4

            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, hf), normal, // 5
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, hf), normal  // 6
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
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -hf), normal, // 0
			lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -hf), normal, // 1
			lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal, // 2
			lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -hf), normal, // 3
			lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -hf), normal, // 4

            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal, // 5
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal  // 6
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 1, index + 2,
			index + 2, index + 1, index + 3,
			index + 1, index + 4, index + 3,

            index + 2, index + 5, index,
            index + 5, index + 6, index
		}); index += 7;

		// draw outward faces of teeth
		normal = lamp::v3(v1, -u1, 0.0f);
		vertices.insert(vertices.end(), {
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,      hf), normal,
            lamp::v3(r1 * cos_ta,     r1 * sin_ta,     -hf), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  hf), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  hf), normal,
            lamp::v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  hf), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(v2, -u2, 0.0f);
		vertices.insert(vertices.end(), {
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  hf), normal,
            lamp::v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  hf), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = lamp::v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  hf), normal,
            lamp::v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal,
            lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da,  hf), normal,
            lamp::v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

        // draw inside radius cylinder
		const lamp::v3 normal_1(-cos_ta,     -sin_ta,     0.0f);
		const lamp::v3 normal_2(-cos_ta_4da, -sin_ta_4da, 0.0f);

		vertices.insert(vertices.end(), {
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,     -hf), normal_1,
            lamp::v3(r0 * cos_ta,     r0 * sin_ta,      hf), normal_1,
            lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -hf), normal_2,
            lamp::v3(r0 * cos_ta_4da, r0 * sin_ta_4da,  hf), normal_2
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
	auto entity   = ecs.entities.create();
	auto renderer = entity.assign<lamp::components::renderer>();

	entity.assign<lamp::components::transform>();
    entity.assign<lamp::components::selectable>();

	renderer->shader = model_shader;
    renderer->mesh   = create_gear();

	renderer->material = std::make_shared<lamp::Material>();
	renderer->material->color     = color;
    renderer->material->shininess = 128.0f;

	btVector3 inertia;
	constexpr float mass = 6.28f;

	auto shape = new btCylinderShapeZ({ gear.outer, gear.outer, gear.width / 2.0f });
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
	}
	else
	{
		body->setAngularFactor({ 0, 0, 1 });
	}

	entity.assign<lamp::components::rigidbody>()->body = body;

	physics.add_rigidbody(body);

	return entity;
}

std::shared_ptr<lamp::Mesh> Gears::create_rail(const int32_t length) const
{
    std::vector<lamp::v3> vertices;
    std::vector<uint32_t> indices;

    lamp::gl::Layout layout;
    layout.add<float>(3);
    layout.add<float>(3);

    return lamp::Assets::create(vertices, indices, layout, GL_TRIANGLES, GL_STATIC_DRAW);
}

void Gears::create_plane(const lamp::math::rgb& color, const lamp::v3& position, const lamp::v3& normal, const lamp::v3& axes, const float angle)
{
    auto plane    = ecs.entities.create();
    auto renderer = plane.assign<lamp::components::renderer>();

    renderer->shader   = model_shader;
    renderer->mesh     = lamp::Importer::import("models/plane.obj");
    renderer->material = std::make_shared<lamp::Material>();
    renderer->material->color     = color;
    renderer->material->shininess = 128.0f;

    auto world = glm::translate(glm::identity<lamp::m4>(), position);
    plane.assign<lamp::components::transform>()->world = glm::rotate(world, glm::radians(angle), axes);
    plane.assign<lamp::components::selectable>();

    btRigidBody::btRigidBodyConstructionInfo info(0.0f,
                                                  new btDefaultMotionState(lamp::utils::from(position, glm::identity<lamp::quat>())),
                                                  new btStaticPlaneShape({ normal.x, normal.y, normal.z }, 0));
    auto body = new btRigidBody(info);
    body->setUserIndex(static_cast<int32_t>(plane.id().id()));

    physics.add_rigidbody(body);
}

int32_t main()
{
	Gears game; game.run({ "Gears", 8, false, false, true }, { 1280, 768 });

	return 0;
}