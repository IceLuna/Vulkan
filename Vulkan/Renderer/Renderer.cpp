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

const int MAX_FRAMES_IN_FLIGHT = 3;
static uint32_t s_CurrentFrame = 0;

struct Data
{
	VulkanImage* ColorAttachment = nullptr;
	VulkanGraphicsPipeline* Pipeline = nullptr;
	VulkanCommandManager* CommandManager = nullptr;
	VulkanSwapchain* Swapchain = nullptr;
};

static Data s_Data;

void Renderer::Init(uint32_t width, uint32_t height)
{
	VulkanAllocator::Init(VulkanContext::GetDevice());
	VulkanPipelineCache::Init();

	VulkanShader vertexShader("Shaders/basic.vert", ShaderType::Vertex);
	VulkanShader fragmentShader("Shaders/basic.frag", ShaderType::Fragment);

	ImageSpecifications imageSpecs;
	imageSpecs.Size = { width, height, 1 };
	imageSpecs.Format = ImageFormat::B8G8R8A8_UNorm_SRGB;
	imageSpecs.Usage = ImageUsage::ColorAttachment;
	imageSpecs.Layout = ImageLayoutType::RenderTarget;
	s_Data.ColorAttachment = new VulkanImage(imageSpecs);

	GraphicsPipelineState::ColorAttachment colorAttachment;
	colorAttachment.Image = s_Data.ColorAttachment;
	colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
	colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
	colorAttachment.bClearEnabled = true;

	GraphicsPipelineState state;
	state.VertexShader = &vertexShader;
	state.FragmentShader = &fragmentShader;
	state.ColorAttachments.push_back(colorAttachment);
	state.bUsedToPresentToSwapchain = true;

	s_Data.Pipeline = new VulkanGraphicsPipeline(state, width, height);
	s_Data.CommandManager = new VulkanCommandManager(CommandQueueFamily::Graphics);
	s_Data.Swapchain = Application::GetApp().GetWindow().GetSwapchain();
}

void Renderer::Shutdown()
{
	VulkanContext::GetDevice()->WaitIdle();

	delete s_Data.ColorAttachment;
	delete s_Data.Pipeline;
	delete s_Data.CommandManager;

	VulkanPipelineCache::Shutdown();
	VulkanAllocator::Shutdown();
}

void Renderer::DrawFrame()
{
	auto imageAcquireSemaphore = s_Data.Swapchain->NextFrame();

	auto cmd = s_Data.CommandManager->AllocateCommandBuffer();
	cmd.BeginGraphics(*s_Data.Pipeline, s_Data.Swapchain->GetFrameIndex());
	cmd.Draw(0, 0);
	cmd.EndGraphics();
	cmd.End();

	VulkanFence fence;
	VulkanSemaphore semaphore;
	s_Data.CommandManager->Submit(&cmd, 1, imageAcquireSemaphore, 1, &semaphore, 1, &fence);
	fence.Wait();
	VulkanContext::GetDevice()->WaitIdle();

	s_Data.Swapchain->Present(&semaphore);
}

void Renderer::OnWindowResized()
{
	s_Data.Pipeline->InvalidateSwapchainRenderPass();
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
