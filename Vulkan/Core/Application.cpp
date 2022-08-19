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

    Renderer::Init(width, height);
}

Application::~Application()
{
    Renderer::Shutdown();
}

void Application::Run()
{
    std::chrono::time_point<std::chrono::high_resolution_clock> before, after;

    while (!m_Window.ShouldClose()) {
        before = std::chrono::high_resolution_clock::now();
        glfwPollEvents();
        if (!m_Minimized)
        {
            Renderer::DrawFrame();

            //Renderer::BeginImGui();
            //ImGui::ShowDemoWindow();
            //Renderer::EndImGui();
        }
        after = std::chrono::high_resolution_clock::now();

        auto s = std::chrono::duration_cast<std::chrono::microseconds>(after - before).count() / 1000.f / 1000.f;
        std::cout << "FPS: " << 1.f / s << "\r";
    }
}
