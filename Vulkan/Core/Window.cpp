#include "Window.h"

#include "../Vulkan/VulkanContext.h"
#include "../Vulkan/VulkanSwapchain.h"

static std::function<void(uint32_t, uint32_t)> s_ResizeCallback;

static void OnWindowResizedCallback(GLFWwindow* window, int width, int height)
{
    if (s_ResizeCallback)
        s_ResizeCallback((uint32_t)width, (uint32_t)height);
        
    void* userData = glfwGetWindowUserPointer(window);
    if (userData)
    {
        Window* window = (Window*)userData;
        window->OnResized();
    }
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
    delete m_Swapchain;
    m_Swapchain = nullptr;

    delete m_RenderContext;
    m_RenderContext = nullptr;

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::InitContext()
{
    m_RenderContext = new VulkanContext;
    m_Swapchain = new VulkanSwapchain(m_RenderContext->GetInstance(), m_Window);
    m_RenderContext->InitDevices(m_Swapchain->GetSurface(), true);
    m_Swapchain->Init(m_RenderContext->GetContextDevice());
}

void Window::SetResizeCallback(std::function<void(uint32_t, uint32_t)> func)
{
    s_ResizeCallback = func;
}

void Window::OnResized()
{
    int width, height;
    glfwGetWindowSize(m_Window, &width, &height);
    if (width != 0 && height != 0)
        m_Swapchain->OnResized();
}
