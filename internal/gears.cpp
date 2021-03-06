#include "gears.hpp"

#include "engine/systems/rotation.hpp"
#include "engine/systems/renderer.hpp"
#include "engine/systems/editor.hpp"

#include "engine/components/transform.hpp"
#include "engine/components/renderer.hpp"
#include "engine/components/rigidbody.hpp"
#include "engine/components/selectable.hpp"
#include "engine/components/position.hpp"
#include "engine/components/camera.hpp"
#include "engine/components/rotation.hpp"
#include "engine/components/viewport.hpp"

#include "engine/editor.hpp"
#include "engine/primitives.hpp"

#include "physics/utils.hpp"
#include "common/random.hpp"

#include "gl/renderer.hpp"
#include "gl/program.hpp"

#include "importer.hpp"
#include "assets.inl"

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

gl::program_ptr shader;

const btVector3 axis(0, 0, 1);

void Gears::input(const int32_t action, const int32_t key)
{
	if (action  == GLFW_PRESS) {
		if (key == GLFW_KEY_TAB) {

            _show_menu = !_show_menu;

        } else if (key == GLFW_MOUSE_BUTTON_1) {

		    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {

                auto entities = ecs.entities.entities_with_components<components::camera>();
                auto camera   = std::find_if(entities.begin(), entities.end(), [](entityx::Entity e) {
                    return e.component<components::camera>()->main;
                });

                auto hit = physics.ray(physics::to_world((*camera), _mouse));

                if (hit.hasHit()) {

                    if (const int32_t index = hit.m_collisionObject->getUserIndex(); index != -1) {

                        ecs.entities.each<components::selectable>([](entityx::Entity,
                                          components::selectable& selectable) {

                            selectable.selected = false;
                        });

                        auto id = ecs.entities.create_id(index);
                        auto e  = ecs.entities.get(id);

                        e.component<components::selectable>()->selected = true;
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
	auto vertex   = Assets::create("shaders/glsl/model.vert", GL_VERTEX_SHADER);
	auto fragment = Assets::create("shaders/glsl/model.frag", GL_FRAGMENT_SHADER);

    shader = Assets::create(vertex, fragment);

    ecs.systems.add<systems::Rotation>();

	gear.inner = 0.7f;
	gear.outer = 3.0f;
	gear.depth = 0.7f;
	gear.width = 0.5f;
	gear.teeth = 13;

    {
        auto entity  = ecs.entities.create();
        auto camera  = entity.assign<components::camera>();
        camera->main = true;

        auto position = entity.assign<components::position>();
        position->z   = 20.0f;

        auto viewport    = entity.assign<components::viewport>();
        viewport->width  = 1280.0f;
        viewport->height = 768.0f;

        entity.assign<components::transform>();
    }

    {
        auto entity   = ecs.entities.create();
        auto rotation = entity.assign<components::rotation>();
        rotation->speed  = 2.0f;
        rotation->radius = 10.0f;

        auto object = new btCollisionObject();
        object->setUserIndex(static_cast<int32_t>(entity.id().id()));

        entity.assign<components::transform>();
        entity.assign<components::position>();
        entity.assign<components::selectable>();
        entity.assign<components::light>();

        physics.add(object, new btBoxShape({ 0.25f, 0.25f, 0.25f }), btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }

	auto down = Primitives::create_plane(physics, ecs.entities, { 0.8f, 0.4f, 0.4f }, { 0.0f, -4.0f,   0.0f }, { 0, 1, 0 }, 20.0f, { 0, 0, 1 },  0.0f);
    auto back = Primitives::create_plane(physics, ecs.entities, { 0.6f, 0.4f, 0.2f }, { 0.0f, 16.0f, -20.0f }, { 0, 0, 1 }, 20.0f, { 1, 0, 0 }, 90.0f);

    down.component<components::renderer>()->shader = shader;
    back.component<components::renderer>()->shader = shader;

	ui::Editor::init(static_cast<GLFWwindow*>(_window));
}

void Gears::release()
{
	shader->release();

	ui::Editor::release();
}

void Gears::update(const float delta_time)
{
    ecs.systems.update<systems::Rotation>(delta_time);

	constexpr float camera_speed = 6.1f;

    auto entities = ecs.entities.entities_with_components<components::camera>();
    auto entity   = std::find_if(entities.begin(), entities.begin(), [](entityx::Entity e) {
        return e.component<components::camera>()->main;
    });

    auto position = (*entity).component<components::position>();

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
	ui::Editor::begin();

	ecs.systems.update<systems::Editor>(0);

	if (_show_menu) {

		static int32_t count = 3;
		static auto position = glm::zero<v3>();

		ImGui::Begin("Generator", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::InputFloat3("Position",    glm::value_ptr(position), "%.1f");
		ImGui::InputFloat("Outer Radius", &gear.outer, 0.1f);
		ImGui::InputFloat("Inner Radius", &gear.inner, 0.1f);

		ImGui::InputInt("Teeth", &gear.teeth, 2);
        ImGui::InputInt("Count", &count,      1);

		if (ImGui::Button("Generate"))
        {
            const float offset = gear.outer * 2.0f;

            v3 new_position = position;
            new_position.x -= (count * offset) / 2.0f - gear.outer;

            auto last_one = create(new_position, Random::color(), true, static_cast<float>(count + 0.5f));

            for (int32_t i = 1; i < count; i++) {

                new_position.x += offset;

                auto new_one = create(new_position, Random::color(), i % 2 == 0);

                physics.add(new btGearConstraint(
                *last_one.component<components::rigidbody>()->body,
                 *new_one.component<components::rigidbody>()->body, axis, axis), true);

                last_one = new_one;
            }
        }

		ImGui::End();

        auto entities = ecs.entities.entities_with_components<components::selectable>();
        auto entity   = std::find_if(entities.begin(), entities.end(), [](entityx::Entity e) {

            auto   selectable = e.component<components::selectable>();
            return selectable->selected && !selectable->disabled;
        });

        if (entity != entities.end())
        {
            if (auto selected = (*entity); selected.has_component<components::rigidbody>())
            {
                ImGui::Begin("Gear", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

                if (ImGui::Button("Remove")) {

                                selected.component<components::selectable>()->disabled = true;
                    auto body = selected.component<components::rigidbody>()->body;

                    body->setLinearFactor({1, 1, 1});
                    body->setAngularFactor({1, 1, 1});

                    body->applyCentralImpulse({0, 0, -25.0f});

                    for (int32_t i = 0; i < body->getNumConstraintRefs(); i++)
                    {
                        auto constraint = body->getConstraintRef(i);

                        body->removeConstraintRef(constraint);
                    }
                }

                ImGui::End();
            }
        }
	}

	ui::Editor::end();
}

std::shared_ptr<Mesh> Gears::create_gear() const
{
	uint32_t index = 0;

	std::vector<v3> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve(static_cast<size_t>(68) * gear.teeth);
	indices.reserve(static_cast<size_t>(66)  * gear.teeth);

	const float half_depth = gear.depth * 0.5f;

	const float r0 = gear.inner;
	const float r1 = gear.outer - half_depth;
	const float r2 = gear.outer + half_depth;

	const float da = 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth) / 4.0f;
	const float hf = gear.width * 0.5f;

	for (int32_t i = 0; i < gear.teeth; i++) {

		const float ta = static_cast<float>(i) * 2.0f * glm::pi<float>() / static_cast<float>(gear.teeth);

		const float cos_ta     = std::cosf(ta);
		const float cos_ta_1da = std::cosf(ta + da);
		const float cos_ta_2da = std::cosf(ta + 2.0f * da);
		const float cos_ta_3da = std::cosf(ta + 3.0f * da);
		const float cos_ta_4da = std::cosf(ta + 4.0f * da);

		const float sin_ta     = std::sinf(ta);
		const float sin_ta_1da = std::sinf(ta + da);
		const float sin_ta_2da = std::sinf(ta + 2.0f * da);
		const float sin_ta_3da = std::sinf(ta + 3.0f * da);
		const float sin_ta_4da = std::sinf(ta + 4.0f * da);

		const float u2 = r1 * cos_ta_3da - r2 * cos_ta_2da;
		const float v2 = r1 * sin_ta_3da - r2 * sin_ta_2da;

		float u1 = r2 * cos_ta_1da - r1 * cos_ta;
		float v1 = r2 * sin_ta_1da - r1 * sin_ta;

		const float len = std::sqrtf(u1 * u1 + v1 * v1);

		u1 /= len;
		v1 /= len;

		// front face
		v3 normal(0.0f, 0.0f, 1.0f);
        vertices.insert(vertices.end(), {
            v3(r0 * cos_ta,     r0 * sin_ta,     hf), normal, // 0
            v3(r1 * cos_ta,     r1 * sin_ta,     hf), normal, // 1
            v3(r1 * cos_ta_3da, r1 * sin_ta_3da, hf), normal, // 2
            v3(r0 * cos_ta_4da, r0 * sin_ta_4da, hf), normal, // 3
            v3(r1 * cos_ta_4da, r1 * sin_ta_4da, hf), normal, // 4

            v3(r2 * cos_ta_1da, r2 * sin_ta_1da, hf), normal, // 5
            v3(r2 * cos_ta_2da, r2 * sin_ta_2da, hf), normal  // 6
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
            v3(r1 * cos_ta,     r1 * sin_ta,     -hf), normal, // 0
			v3(r0 * cos_ta,     r0 * sin_ta,     -hf), normal, // 1
			v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal, // 2
			v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -hf), normal, // 3
			v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -hf), normal, // 4

            v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal, // 5
            v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal  // 6
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
		normal = v3(v1, -u1, 0.0f);
		vertices.insert(vertices.end(), {
            v3(r1 * cos_ta,     r1 * sin_ta,      hf), normal,
            v3(r1 * cos_ta,     r1 * sin_ta,     -hf), normal,
            v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  hf), normal,
            v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
            v3(r2 * cos_ta_1da, r2 * sin_ta_1da,  hf), normal,
            v3(r2 * cos_ta_1da, r2 * sin_ta_1da, -hf), normal,
            v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  hf), normal,
            v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = v3(v2, -u2, 0.0f);
		vertices.insert(vertices.end(), {
            v3(r2 * cos_ta_2da, r2 * sin_ta_2da,  hf), normal,
            v3(r2 * cos_ta_2da, r2 * sin_ta_2da, -hf), normal,
            v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  hf), normal,
            v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

		normal = v3(cos_ta, sin_ta, 0.0f);
		vertices.insert(vertices.end(), {
            v3(r1 * cos_ta_3da, r1 * sin_ta_3da,  hf), normal,
            v3(r1 * cos_ta_3da, r1 * sin_ta_3da, -hf), normal,
            v3(r1 * cos_ta_4da, r1 * sin_ta_4da,  hf), normal,
            v3(r1 * cos_ta_4da, r1 * sin_ta_4da, -hf), normal
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;

        // draw inside radius cylinder
		const v3 normal_1(-cos_ta,     -sin_ta,     0.0f);
		const v3 normal_2(-cos_ta_4da, -sin_ta_4da, 0.0f);

		vertices.insert(vertices.end(), {
            v3(r0 * cos_ta,     r0 * sin_ta,     -hf), normal_1,
            v3(r0 * cos_ta,     r0 * sin_ta,      hf), normal_1,
            v3(r0 * cos_ta_4da, r0 * sin_ta_4da, -hf), normal_2,
            v3(r0 * cos_ta_4da, r0 * sin_ta_4da,  hf), normal_2
		});

		indices.insert(indices.end(), {
			index,     index + 1, index + 2,
			index + 1, index + 3, index + 2
		}); index += 4;
	}

	gl::Layout layout;
	layout.add<float>(3);
	layout.add<float>(3);

	return Assets::create(vertices, indices, layout, GL_TRIANGLES, GL_STATIC_DRAW);
}

entityx::Entity Gears::create(const v3& position, const math::rgb& color, const bool middle, const float speed)
{
	auto entity   = ecs.entities.create();
	auto renderer = entity.assign<components::renderer>();

	renderer->shader = shader;
    renderer->mesh   = create_gear();

	renderer->material = std::make_shared<Material>();
	renderer->material->color     = color;
    renderer->material->shininess = 128.0f;

    entity.assign<components::transform>();
    entity.assign<components::selectable>();

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

	speed > 0.0f ? body->setAngularVelocity({ 0, 0, speed }) :
	        	   body->setAngularFactor({ 0, 0, 1 });

	entity.assign<components::rigidbody>()->body = body;
	physics.add(body);

	return entity;
}

int32_t main()
{
	Gears game; game.run({ "Gears", 8, false, false, true },
	                       { 1280, 768 });
	return 0;
}