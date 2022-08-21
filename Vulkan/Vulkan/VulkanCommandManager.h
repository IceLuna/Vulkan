#pragma once

#include "Vulkan.h"
#include "VulkanSemaphore.h"
#include "VulkanFence.h"

enum class CommandQueueFamily
{
	Graphics,
	Compute,
	Transfer
};

class VulkanCommandBuffer;
class VulkanGraphicsPipeline;
class VulkanFramebuffer;

class VulkanCommandManager
{
public:
	// @bAllowReuse. If set to true, allows already allocated command buffers to be rerecorded.
	VulkanCommandManager(CommandQueueFamily queueFamily, bool bAllowReuse);
	~VulkanCommandManager();

	VulkanCommandManager(const VulkanCommandManager&) = delete;
	VulkanCommandManager(VulkanCommandManager&& other) noexcept { m_CommandPool = other.m_CommandPool; other.m_CommandPool = VK_NULL_HANDLE; }

	VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;
	VulkanCommandManager& operator=(VulkanCommandManager&& other) noexcept = delete;

	VulkanCommandBuffer AllocateCommandBuffer(bool bBegin = true);

	void Submit(const VulkanCommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
		const VulkanSemaphore* waitSemaphores = nullptr,   uint32_t waitSemaphoresCount = 0,
		const VulkanSemaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0,
		const VulkanFence* signalFence = nullptr);

private:
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	VkQueue m_Queue = VK_NULL_HANDLE;
	uint32_t m_QueueFamilyIndex = uint32_t(-1);

	friend class VulkanCommandBuffer;
};

class VulkanCommandBuffer
{
private:
	VulkanCommandBuffer(const VulkanCommandManager& manager);

public:
	VulkanCommandBuffer() = default;
	VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
	VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
	{
		m_Device = other.m_Device;
		m_CommandPool = other.m_CommandPool;
		m_CommandBuffer = other.m_CommandBuffer;
		m_CurrentGraphicsPipeline = other.m_CurrentGraphicsPipeline;

		other.m_Device = VK_NULL_HANDLE;
		other.m_CommandBuffer = VK_NULL_HANDLE;
		other.m_CommandPool = VK_NULL_HANDLE;
		other.m_CurrentGraphicsPipeline = nullptr;
	}
	~VulkanCommandBuffer();

	VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
	VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept
	{
		if (m_CommandBuffer)
			vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);

		m_Device = other.m_Device;
		m_CommandPool = other.m_CommandPool;
		m_CommandBuffer = other.m_CommandBuffer;
		m_CurrentGraphicsPipeline = other.m_CurrentGraphicsPipeline;

		other.m_Device = VK_NULL_HANDLE;
		other.m_CommandBuffer = VK_NULL_HANDLE;
		other.m_CommandPool = VK_NULL_HANDLE;
		other.m_CurrentGraphicsPipeline = nullptr;

		return *this;
	}

	void Begin();
	void End();

	void BeginGraphics(const VulkanGraphicsPipeline& pipeline);
	void BeginGraphics(const VulkanGraphicsPipeline& pipeline, const VulkanFramebuffer& framebuffer);
	void EndGraphics();
	void Draw(uint32_t vertexCount, uint32_t firstVertex);

	void GenerateMips();

private:
	VkDevice m_Device = VK_NULL_HANDLE;
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
	const VulkanGraphicsPipeline* m_CurrentGraphicsPipeline = nullptr;

	friend class VulkanCommandManager;
};
