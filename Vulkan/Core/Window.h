#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <string>
#include <functional>

class Window
{
public:
	Window(uint32_t width, uint32_t height, const std::string& title);
	~Window();

	bool ShouldClose() const { return glfwWindowShouldClose(m_Window); }
	HWND GetHandler() const { return glfwGetWin32Window(m_Window); }

	void SetResizeCallback(std::function<void(uint32_t, uint32_t)> func);

	GLFWwindow* GetNativeWindow() { return m_Window; }

private:
	GLFWwindow* m_Window;
};