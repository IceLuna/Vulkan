#include "Renderer.h"
#include "../Core/Application.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

#include "../Vulkan/VulkanContext.h"
#include "../Vulkan/VulkanDevice.h"
#include "../Vulkan/VulkanShader.h"
#include "../Vulkan/VulkanPipelineCache.h"
#include "../Vulkan/VulkanGraphicsPipeline.h"
#include "../Vulkan/VulkanSwapchain.h"
#include "../Vulkan/VulkanFramebuffer.h"

const int MAX_FRAMES_IN_FLIGHT = 3;
static uint32_t s_CurrentFrame = 0;

struct Data
{
	VulkanGraphicsPipeline* Pipeline = nullptr;
	VulkanCommandManager* CommandManager = nullptr;
	VulkanSwapchain* Swapchain = nullptr;
	std::vector<VulkanFramebuffer*> Framebuffers;
};

static Data s_Data;

void Renderer::Init()
{
	VulkanAllocator::Init(VulkanContext::GetDevice());
	VulkanPipelineCache::Init();
	s_Data.Swapchain = Application::GetApp().GetWindow().GetSwapchain();

	VulkanShader vertexShader("Shaders/basic.vert", ShaderType::Vertex);
	VulkanShader fragmentShader("Shaders/basic.frag", ShaderType::Fragment);

	auto& swapchainImages = s_Data.Swapchain->GetImages();

	GraphicsPipelineState::ColorAttachment colorAttachment;
	colorAttachment.Image = swapchainImages[0];
	colorAttachment.InitialLayout = ImageLayoutType::Unknown;
	colorAttachment.FinalLayout = ImageLayoutType::Present;
	colorAttachment.bClearEnabled = true;
	colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

	GraphicsPipelineState state;
	state.VertexShader = &vertexShader;
	state.FragmentShader = &fragmentShader;
	state.ColorAttachments.push_back(colorAttachment);

	s_Data.Pipeline = new VulkanGraphicsPipeline(state);
	s_Data.CommandManager = new VulkanCommandManager(CommandQueueFamily::Graphics);

	for (auto& image : swapchainImages)
		s_Data.Framebuffers.push_back(new VulkanFramebuffer({ image }, s_Data.Pipeline->GetRenderPassHandle(), s_Data.Swapchain->GetSize()));
}

void Renderer::Shutdown()
{
	VulkanContext::GetDevice()->WaitIdle();

	delete s_Data.Pipeline;
	delete s_Data.CommandManager;
	for (auto& fb : s_Data.Framebuffers)
		delete fb;
	s_Data.Framebuffers.clear();

	VulkanPipelineCache::Shutdown();
	VulkanAllocator::Shutdown();
}

void Renderer::DrawFrame()
{
	uint32_t frameIndex = 0;
	auto imageAcquireSemaphore = s_Data.Swapchain->AcquireImage(&frameIndex);

	auto cmd = s_Data.CommandManager->AllocateCommandBuffer();
	cmd.BeginGraphics(*s_Data.Pipeline, *s_Data.Framebuffers[frameIndex]);
	cmd.Draw(3, 0);
	cmd.EndGraphics();
	cmd.End();

	VulkanSemaphore semaphore;
	s_Data.CommandManager->Submit(&cmd, 1, imageAcquireSemaphore, 1, &semaphore, 1, nullptr);

	s_Data.Swapchain->Present(&semaphore);
	VulkanContext::GetDevice()->WaitIdle();
}

void Renderer::OnWindowResized()
{
	for (auto& fb : s_Data.Framebuffers)
		delete fb;
	s_Data.Framebuffers.clear();

	auto& swapchainImages = s_Data.Swapchain->GetImages();
	const void* renderPassHandle = s_Data.Pipeline->GetRenderPassHandle();
	glm::uvec2 size = s_Data.Swapchain->GetSize();
	for (auto& image : swapchainImages)
		s_Data.Framebuffers.push_back(new VulkanFramebuffer({ image }, renderPassHandle, size));
}

void Renderer::BeginImGui()
{
}

VulkanContext& Renderer::GetContext()
{
	return Application::GetApp().GetWindow().GetRenderContext();
}

void Renderer::EndImGui()
{
}
