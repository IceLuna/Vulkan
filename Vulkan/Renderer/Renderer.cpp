#include "Renderer.h"
#include "../Core/Application.h"

#include <array>

#include "../Vulkan/VulkanContext.h"
#include "../Vulkan/VulkanDevice.h"
#include "../Vulkan/VulkanShader.h"
#include "../Vulkan/VulkanPipelineCache.h"
#include "../Vulkan/VulkanGraphicsPipeline.h"
#include "../Vulkan/VulkanSwapchain.h"
#include "../Vulkan/VulkanFramebuffer.h"
#include "../Vulkan/VulkanFence.h"
#include "../Vulkan/VulkanSemaphore.h"
#include "../Vulkan/VulkanCommandManager.h"

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
static uint32_t s_CurrentFrame = 0;

struct Data
{
	VulkanGraphicsPipeline* Pipeline = nullptr;
	VulkanCommandManager* CommandManager = nullptr;
	VulkanSwapchain* Swapchain = nullptr;
	std::vector<VulkanFramebuffer*> Framebuffers;

	std::vector<VulkanCommandBuffer> CommandBuffers;
	std::vector<VulkanFence> Fences;
	std::array<VulkanSemaphore, MAX_FRAMES_IN_FLIGHT> Semaphores;
};

static Data* s_Data = nullptr;

void Renderer::Init()
{
	VulkanAllocator::Init(VulkanContext::GetDevice());
	VulkanPipelineCache::Init();
	s_Data = new Data;
	s_Data->Swapchain = Application::GetApp().GetWindow().GetSwapchain();

	VulkanShader vertexShader("Shaders/basic.vert", ShaderType::Vertex);
	VulkanShader fragmentShader("Shaders/basic.frag", ShaderType::Fragment);

	auto& swapchainImages = s_Data->Swapchain->GetImages();

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

	s_Data->Pipeline = new VulkanGraphicsPipeline(state);
	s_Data->CommandManager = new VulkanCommandManager(CommandQueueFamily::Graphics, true);

	for (auto& image : swapchainImages)
		s_Data->Framebuffers.push_back(new VulkanFramebuffer({ image }, s_Data->Pipeline->GetRenderPassHandle(), s_Data->Swapchain->GetSize()));

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		s_Data->Fences.emplace_back(true);
		s_Data->CommandBuffers.emplace_back(s_Data->CommandManager->AllocateCommandBuffer(false));
	}
}

void Renderer::Shutdown()
{
	VulkanContext::GetDevice()->WaitIdle();

	delete s_Data->Pipeline;
	s_Data->CommandBuffers.clear();
	s_Data->Fences.clear();
	delete s_Data->CommandManager;

	for (auto& fb : s_Data->Framebuffers)
		delete fb;
	s_Data->Framebuffers.clear();

	delete s_Data;

	VulkanPipelineCache::Shutdown();
	VulkanAllocator::Shutdown();
}

void Renderer::DrawFrame()
{
	auto& semaphore = s_Data->Semaphores[s_CurrentFrame];
	auto& fence = s_Data->Fences[s_CurrentFrame];
	auto& cmd = s_Data->CommandBuffers[s_CurrentFrame];

	fence.Wait();
	fence.Reset();

	uint32_t imageIndex = 0;
	auto imageAcquireSemaphore = s_Data->Swapchain->AcquireImage(&imageIndex);

	cmd.Begin();
	cmd.BeginGraphics(*s_Data->Pipeline, *s_Data->Framebuffers[imageIndex]);
	cmd.Draw(3, 0);
	cmd.EndGraphics();
	cmd.End();

	s_Data->CommandManager->Submit(&cmd, 1, imageAcquireSemaphore, 1, &semaphore, 1, &fence);
	s_Data->Swapchain->Present(&semaphore);

	s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::OnWindowResized()
{
	for (auto& fb : s_Data->Framebuffers)
		delete fb;
	s_Data->Framebuffers.clear();

	auto& swapchainImages = s_Data->Swapchain->GetImages();
	const void* renderPassHandle = s_Data->Pipeline->GetRenderPassHandle();
	glm::uvec2 size = s_Data->Swapchain->GetSize();
	for (auto& image : swapchainImages)
		s_Data->Framebuffers.push_back(new VulkanFramebuffer({ image }, renderPassHandle, size));
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
