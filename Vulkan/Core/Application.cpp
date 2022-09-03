#include "Application.h"
#include "../Renderer/Renderer.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#include "../Vulkan/VulkanContext.h"

#include <iostream>
#include <chrono>

Application* Application::s_App = nullptr;

Application::Application(uint32_t width, uint32_t height, const std::string& title)
: m_Window(width, height, title)
{
    s_App = this;
    auto func = [this](uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            m_Minimized = true;
        else
        {
            m_Minimized = false;
            Renderer::OnWindowResized();
        }
    };
    m_Window.SetResizeCallback(func);
    m_Window.InitContext();

    Renderer::Init();
}

Application::~Application()
{
    Renderer::Shutdown();
}

void Application::Run()
{
    float lastFrameTime = (float)glfwGetTime();

    while (!m_Window.ShouldClose())
    {
        const float currentFrameTime = (float)glfwGetTime();
        float ts = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        glfwPollEvents();
        if (!m_Minimized)
        {
            Renderer::DrawFrame(ts);
        }

        std::cout << "\33[2K\r" << "FPS: " << 1.f / ts;
    }
    std::cout << '\r';
}
