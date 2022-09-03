#include "Renderer.h"

#include "../Core/Application.h"

#include "../Vulkan/VulkanContext.h"
#include "../Vulkan/VulkanDevice.h"
#include "../Vulkan/VulkanShader.h"
#include "../Vulkan/VulkanPipelineCache.h"
#include "../Vulkan/VulkanGraphicsPipeline.h"
#include "../Vulkan/VulkanSwapchain.h"
#include "../Vulkan/VulkanFramebuffer.h"
#include "../Vulkan/VulkanTexture2D.h"
#include "../Vulkan/VulkanFence.h"
#include "../Vulkan/VulkanSemaphore.h"
#include "../Vulkan/VulkanCommandManager.h"
#include "../Vulkan/VulkanStagingManager.h"

#include "../Core/Mesh.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan_with_textures.h"
#include "imgui/backends/imgui_impl_glfw.h"

#include "glm/gtc/matrix_transform.hpp"

#include <array>

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
static uint32_t s_CurrentFrame = 0;

struct Data
{
	VulkanGraphicsPipeline* Pipeline = nullptr;
	VulkanCommandManager* GraphicsCommandManager = nullptr;
	VulkanSwapchain* Swapchain = nullptr;
	std::vector<VulkanFramebuffer*> Framebuffers;

	std::vector<VulkanCommandBuffer> CommandBuffers;
	std::vector<Ref<VulkanFence>> Fences;
	std::array<VulkanSemaphore, MAX_FRAMES_IN_FLIGHT> Semaphores;

	VulkanShader* VertexShader = nullptr;
	VulkanShader* FragmentShader = nullptr;

	VulkanImage* DepthImage = nullptr;

	glm::uvec2 Size = {800, 600};

	Mesh* Mesh = nullptr;
	VulkanTexture2D* Texture = nullptr;
	VulkanBuffer* VertexBuffer = nullptr;
	VulkanBuffer* IndexBuffer = nullptr;
	float RotationSpeed = 0.5f;
};

struct ImGuiData
{
	VkDescriptorPool PersistantPool; // Used to init resources during ImGui initialization.
	std::array<VkDescriptorPool, MAX_FRAMES_IN_FLIGHT> Pools; // Per frame pools to init and reset our resources
};

static Data* s_Data = nullptr;
static ImGuiData* s_ImGuiData = nullptr;

static void InitImGui()
{
	s_ImGuiData = new ImGuiData();
	const VulkanDevice* device = VulkanContext::GetDevice();
	VkDevice vkDevice = device->GetVulkanDevice();

	constexpr VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	constexpr VkDescriptorPoolSize persistantPoolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	for (auto& pool : s_ImGuiData->Pools)
		VK_CHECK(vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &pool));

	poolInfo.poolSizeCount = (uint32_t)std::size(persistantPoolSizes);
	poolInfo.pPoolSizes = persistantPoolSizes;
	VK_CHECK(vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &s_ImGuiData->PersistantPool));

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(Application::GetApp().GetWindow().GetNativeWindow(), true);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = VulkanContext::GetInstance();
	init_info.PhysicalDevice = device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
	init_info.Device = vkDevice;
	init_info.Queue = device->GetGraphicsQueue();
	init_info.DescriptorPool = s_ImGuiData->PersistantPool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, (VkRenderPass)s_Data->Pipeline->GetRenderPassHandle());

	// execute a gpu command to upload imgui font textures
	Ref<VulkanFence> fence = MakeRef<VulkanFence>();
	auto cmd = s_Data->GraphicsCommandManager->AllocateCommandBuffer();
	ImGui_ImplVulkan_CreateFontsTexture(cmd.GetVulkanCommandBuffer());
	cmd.End();
	s_Data->GraphicsCommandManager->Submit(&cmd, 1, fence, nullptr, 0, nullptr, 0);
	fence->Wait();

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

static void ShutdownImGui()
{
	const VulkanDevice* device = VulkanContext::GetDevice();
	VkDevice vkDevice = device->GetVulkanDevice();

	for (auto& pool : s_ImGuiData->Pools)
		vkDestroyDescriptorPool(vkDevice, pool, nullptr);
	vkDestroyDescriptorPool(vkDevice, s_ImGuiData->PersistantPool, nullptr);

	ImGui_ImplVulkan_Shutdown();
	delete s_ImGuiData;
}

void Renderer::Init()
{
	VulkanAllocator::Init();
	VulkanPipelineCache::Init();
	VulkanDescriptorManager::Init();

	s_Data = new Data;
	s_Data->Swapchain = Application::GetApp().GetWindow().GetSwapchain();
	s_Data->Size = s_Data->Swapchain->GetSize();

	s_Data->GraphicsCommandManager = new VulkanCommandManager(CommandQueueFamily::Graphics, true);

	s_Data->VertexShader = new VulkanShader("Shaders/basic.vert", ShaderType::Vertex);
	s_Data->FragmentShader = new VulkanShader("Shaders/basic.frag", ShaderType::Fragment);

	auto& swapchainImages = s_Data->Swapchain->GetImages();

	ImageSpecifications depthSpecs;
	depthSpecs.Format = ImageFormat::D32_Float;
	depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
	depthSpecs.Size = { s_Data->Size.x, s_Data->Size.y, 1 };
	depthSpecs.Usage = ImageUsage::DepthStencilAttachment;
	s_Data->DepthImage = new VulkanImage(depthSpecs);

	ColorAttachment colorAttachment;
	colorAttachment.Image = swapchainImages[0];
	colorAttachment.InitialLayout = ImageLayoutType::Unknown;
	colorAttachment.FinalLayout = ImageLayoutType::Present;
	colorAttachment.bClearEnabled = true;
	colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

	DepthStencilAttachment depthAttachment;
	depthAttachment.InitialLayout = ImageLayoutType::Unknown;
	depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
	depthAttachment.Image = s_Data->DepthImage;
	depthAttachment.bClearEnabled = true;
	depthAttachment.bWriteDepth = true;
	depthAttachment.DepthClearValue = 1.f;
	depthAttachment.DepthCompareOp = CompareOperation::Less;

	GraphicsPipelineState state;
	state.VertexShader = s_Data->VertexShader;
	state.FragmentShader = s_Data->FragmentShader;
	state.ColorAttachments.push_back(colorAttachment);
	state.DepthStencilAttachment = depthAttachment;
	state.CullMode = CullMode::None;

	s_Data->Pipeline = new VulkanGraphicsPipeline(state);

	for (auto& image : swapchainImages)
		s_Data->Framebuffers.push_back(new VulkanFramebuffer({ image, s_Data->DepthImage }, s_Data->Pipeline->GetRenderPassHandle(), s_Data->Size));

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		s_Data->Fences.push_back(MakeRef<VulkanFence>(true));
		s_Data->CommandBuffers.emplace_back(s_Data->GraphicsCommandManager->AllocateCommandBuffer(false));
	}

	s_Data->Mesh = new Mesh("Models/viking_room.obj");
	s_Data->Texture = new VulkanTexture2D("Textures/viking_room.png");

	auto& vertices = s_Data->Mesh->GetVertices();
	auto& indices = s_Data->Mesh->GetIndices();

	BufferSpecifications vertexSpecs { vertices.size() * sizeof(Vertex), MemoryType::Gpu, BufferUsage::VertexBuffer | BufferUsage::TransferDst};
	BufferSpecifications indexSpecs  { indices.size() * sizeof(uint32_t),   MemoryType::Gpu, BufferUsage::IndexBuffer | BufferUsage::TransferDst};
	s_Data->VertexBuffer = new VulkanBuffer(vertexSpecs);
	s_Data->IndexBuffer  = new VulkanBuffer(indexSpecs);

	Ref<VulkanFence> writeBuffersFence = MakeRef<VulkanFence>();
	auto cmd = s_Data->GraphicsCommandManager->AllocateCommandBuffer();
	cmd.Write(s_Data->VertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
	cmd.Write(s_Data->IndexBuffer, indices.data(), indices.size() * sizeof(uint32_t), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
	cmd.End();
	s_Data->GraphicsCommandManager->Submit(&cmd, 1, writeBuffersFence, nullptr, 0, nullptr, 0);

	InitImGui();
	writeBuffersFence->Wait();
}

void Renderer::Shutdown()
{
	VulkanContext::GetDevice()->WaitIdle();
	
	ShutdownImGui();
	VulkanStagingManager::ReleaseBuffers();

	delete s_Data->Pipeline;
	delete s_Data->DepthImage;
	delete s_Data->VertexShader;
	delete s_Data->FragmentShader;

	s_Data->CommandBuffers.clear();
	s_Data->Fences.clear();
	delete s_Data->GraphicsCommandManager;

	for (auto& fb : s_Data->Framebuffers)
		delete fb;
	s_Data->Framebuffers.clear();

	delete s_Data->VertexBuffer;
	delete s_Data->IndexBuffer;
	delete s_Data->Mesh;
	delete s_Data->Texture;

	delete s_Data;
	s_Data = nullptr;

	VulkanDescriptorManager::Shutdown();
	VulkanPipelineCache::Shutdown();
	VulkanAllocator::Shutdown();
}

void Renderer::DrawFrame(float ts)
{
	auto& semaphore = s_Data->Semaphores[s_CurrentFrame];
	auto& fence = s_Data->Fences[s_CurrentFrame];
	auto& cmd = s_Data->CommandBuffers[s_CurrentFrame];

	fence->Wait();
	fence->Reset();

	uint32_t imageIndex = 0;
	auto imageAcquireSemaphore = s_Data->Swapchain->AcquireImage(&imageIndex);

	Renderer::BeginImGui();
	Renderer::DrawImGui();

	struct PushConstant
	{
		glm::mat4 model;
		glm::mat4 view_proj;
	} pushData;

	glm::mat4 view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0, 0, 1));
	glm::mat4 proj = glm::perspective(glm::radians(45.f), float(s_Data->Size.x) / s_Data->Size.y, 0.1f, 10.f);
	proj[1][1] *= -1;

	static float angle = 0.f;
	angle += s_Data->RotationSpeed * ts * glm::radians(90.0f);
	pushData.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
	pushData.view_proj = proj * view;

	s_Data->Pipeline->SetImageSampler(s_Data->Texture, 0, 0);
	cmd.Begin();
	cmd.BeginGraphics(s_Data->Pipeline, *s_Data->Framebuffers[imageIndex]);
	cmd.SetGraphicsRootConstants(&pushData, nullptr);
	cmd.DrawIndexed(s_Data->VertexBuffer, s_Data->IndexBuffer, (uint32_t)s_Data->Mesh->GetIndices().size(), 0, 0);
	EndImGui(&cmd);
	cmd.EndGraphics();
	cmd.End();

	s_Data->GraphicsCommandManager->Submit(&cmd, 1, fence, imageAcquireSemaphore, 1, &semaphore, 1);
	s_Data->Swapchain->Present(&semaphore);

	s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::DrawImGui()
{
	ImGui::Begin("Params");
	ImGui::DragFloat("Rotation speed", &s_Data->RotationSpeed, 0.05f, 0.0f, 5.f);
	
	VkSampler sampler = s_Data->Texture->GetSampler()->GetVulkanSampler();
	VkImageView imageView = s_Data->Texture->GetImage()->GetVulkanImageView();
	VkImageLayout layout = ImageLayoutToVulkan(s_Data->Texture->GetImage()->GetLayout());

	const auto textureID = ImGui_ImplVulkan_AddTexture(sampler, imageView, layout);

	ImGui::Image(textureID, { 256, 256 });
	ImGui::End();
}

void Renderer::OnWindowResized()
{
	for (auto& fb : s_Data->Framebuffers)
		delete fb;
	s_Data->Framebuffers.clear();

	auto& swapchainImages = s_Data->Swapchain->GetImages();
	const void* renderPassHandle = s_Data->Pipeline->GetRenderPassHandle();
	glm::uvec2 size = s_Data->Swapchain->GetSize();
	s_Data->Size = size;
	s_Data->DepthImage->Resize({ size, 1 });
	for (auto& image : swapchainImages)
		s_Data->Framebuffers.push_back(new VulkanFramebuffer({ image, s_Data->DepthImage }, renderPassHandle, size));
}

void Renderer::BeginImGui()
{
	//imgui new frame
	ImGui_SetPerFrameDescriptorPool(s_ImGuiData->Pools[s_CurrentFrame]);
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Renderer::EndImGui(VulkanCommandBuffer* cmd)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->GetVulkanCommandBuffer());
}

VulkanContext& Renderer::GetContext()
{
	return Application::GetApp().GetWindow().GetRenderContext();
}

VulkanCommandManager* Renderer::GetGraphicsCommandManager()
{
	return s_Data->GraphicsCommandManager;
}
