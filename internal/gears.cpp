#include "gears.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include "engine/camera.hpp"

#include "gl/renderer.hpp"

constexpr lamp::v2 size(1024, 768);

lamp::Camera camera(size);

void keyboard_actions(GLFWwindow* ptr, const int key, int, const int action, int)
{
	if (action == GLFW_PRESS)
	{
		auto cubes = static_cast<Gears*>(glfwGetWindowUserPointer(ptr));

		switch (key) {
			case GLFW_KEY_ESCAPE: {
				cubes->window().close();
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
}

void Gears::release()
{
}

void Gears::update(lamp::f32 delta_time)
{
}

void Gears::draw()
{
	lamp::gl::Renderer::clear();
}

int main()
{
	Gears game; game.run({ "Gears", size, 8, false, false });

	return 0;
}