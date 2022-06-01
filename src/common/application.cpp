/*
 * Physically Based Rendering
 * Forked from Michał Siejak PBR project
 */

#include <stdexcept>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <GLFW/glfw3.h>

#include "application.hpp"

#include <iostream>

namespace {
	const int DisplaySizeX = 960;
	const int DisplaySizeY = 540;
	const int DisplaySamples = 8;

	const float ViewDistance = 400.0f;
	const float ViewFOV      = 45.0f;
	const float OrbitSpeed   = 1.0f;
	const float ZoomSpeed    = 16.0f;
}

Application::Application()
	: m_window(nullptr)
	, m_prevCursorX(0.0)
	, m_prevCursorY(0.0)
	, m_mode(InputMode::None)
{
	if (!glfwInit())
	{
		throw std::runtime_error("Failed to initialize GLFW library");
	}

	m_viewSettings.distance = ViewDistance;
	m_viewSettings.fov      = ViewFOV;

	m_sceneSettings.lights[0] = { glm::normalize(glm::vec3{-1.0f,  0.0f, 0.0f}), glm::vec3{1.0f}, false };
	m_sceneSettings.lights[1] = { glm::normalize(glm::vec3{ 1.0f,  0.0f, 0.0f}), glm::vec3{1.0f}, false };
	m_sceneSettings.lights[2] = { glm::normalize(glm::vec3{ 0.0f, -1.0f, 0.0f}), glm::vec3{1.0f}, true };
}

Application::~Application()
{
	if (m_window)
	{
		glfwDestroyWindow(m_window);
	}
	glfwTerminate();
}

void Application::run(const std::unique_ptr<RendererInterface>& renderer)
{
	m_window = renderer->initialize(DisplaySizeX, DisplaySizeY, DisplaySamples);

	glfwSetWindowUserPointer(m_window, this);
	glfwSetCursorPosCallback(m_window, Application::mousePositionCallback);
	glfwSetMouseButtonCallback(m_window, Application::mouseButtonCallback);
	glfwSetScrollCallback(m_window, Application::mouseScrollCallback);
	glfwSetKeyCallback(m_window, Application::keyCallback);
	glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

	m_onResize = renderer->setup();

	while(!glfwWindowShouldClose(m_window)) {
		renderer->render(m_window, m_viewSettings, m_sceneSettings);
		m_viewSettings.m_diffCursorX = m_viewSettings.m_diffCursorY = 0;

		// count fps
		static int fpsCounter = 0;
		fpsCounter++;
		static auto start = std::chrono::steady_clock::now();
		auto current = std::chrono::steady_clock::now();
		std::chrono::duration<double> timeDiff = current - start;
		if (timeDiff.count() > 0.5f)
		{
			std::stringstream str;
			str << std::fixed << std::setprecision(1)
				<< "PBR asteroid (OpenGL 4.5 renderer), "
				<< fpsCounter / timeDiff.count() << " fps";
			glfwSetWindowTitle(m_window, str.str().c_str());
			fpsCounter = 0;
			start = std::chrono::steady_clock::now();
		}

		glfwPollEvents();
	}

	renderer->shutdown();
}

void Application::mousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
	self->m_viewSettings.m_diffCursorX = self->m_viewSettings.m_diffCursorY = 0;
	if (self->m_mode != InputMode::None)
		{
		const double dx = self->m_viewSettings.m_diffCursorX = xpos - self->m_prevCursorX;
		const double dy = self->m_viewSettings.m_diffCursorY = ypos - self->m_prevCursorY;

		switch(self->m_mode) {
		case InputMode::RotatingScene:
			self->m_sceneSettings.yaw   += OrbitSpeed * float(dx);
			self->m_sceneSettings.pitch += OrbitSpeed * float(dy);
			break;
		case InputMode::RotatingView:
			self->m_viewSettings.yaw   += OrbitSpeed * float(dx);
			self->m_viewSettings.pitch += OrbitSpeed * float(dy);
			break;
		default:
			break;
		}

		self->m_prevCursorX = xpos;
		self->m_prevCursorY = ypos;
	}
}
	
void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
	Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

	const InputMode oldMode = self->m_mode;
	if (action == GLFW_PRESS && self->m_mode == InputMode::None)
	{
		switch(button)
		{
		case GLFW_MOUSE_BUTTON_1:
			self->m_mode = InputMode::RotatingView;
			break;
		case GLFW_MOUSE_BUTTON_2:
			self->m_mode = InputMode::RotatingScene;
			break;
		}
	}
	if (action == GLFW_RELEASE
		&& (button == GLFW_MOUSE_BUTTON_1
			|| button == GLFW_MOUSE_BUTTON_2))
	{
		self->m_mode = InputMode::None;
	}

	if (oldMode != self->m_mode)
	{
		if (self->m_mode == InputMode::None)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			glfwGetCursorPos(window, &self->m_prevCursorX, &self->m_prevCursorY);
		}
	}
}
	
void Application::mouseScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
	Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
	self->m_viewSettings.distance += ZoomSpeed * float(-yoffset);
}

void Application::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
	Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

	if (action == GLFW_RELEASE)
	{
		auto &keysSet = self->m_viewSettings.m_keyMapping;
		keysSet.erase(keysSet.find(key));
	}
	if (action == GLFW_PRESS)
	{
		self->m_viewSettings.m_keyMapping.insert(key);

		SceneSettings::Light* light = nullptr;
		
		switch(key) {
		case GLFW_KEY_F1:
			light = &self->m_sceneSettings.lights[0];
			break;
		case GLFW_KEY_F2:
			light = &self->m_sceneSettings.lights[1];
			break;
		case GLFW_KEY_F3:
			light = &self->m_sceneSettings.lights[2];
			break;
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, 1);
			break;
		}

		if (light) {
			light->enabled = !light->enabled;
		}
	}

}

void Application::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
	self->m_onResize(width, height);
}
