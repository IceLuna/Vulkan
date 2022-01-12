#include "Window.h"

static std::function<void(uint32_t, uint32_t)> s_ResizeCallback;

static void OnWindowResizedCallback(GLFWwindow* window, int width, int height)
{
    if (s_ResizeCallback)
        s_ResizeCallback((uint32_t)width, (uint32_t)height);
        
}

Window::Window(uint32_t width, uint32_t height, const std::string& title)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, OnWindowResizedCallback);
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::SetResizeCallback(std::function<void(uint32_t, uint32_t)> func)
{
    s_ResizeCallback = func;
}
