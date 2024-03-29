#pragma once

#include "Vulkan.h"
#include "VulkanSemaphore.h"
#include "VulkanFence.h"
#include "../Renderer/RendererUtils.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>

enum class CommandQueueFamily
{
	Graphics,
	Compute,
	Transfer
};

class VulkanCommandBuffer;
class VulkanGraphicsPipeline;
class VulkanComputePipeline;
class VulkanPipeline;
class VulkanFramebuffer;
class VulkanImage;
class VulkanBuffer;
class VulkanStagingBuffer;

class VulkanCommandManager
{
public:
	// @bAllowReuse. If set to true, allows already allocated command buffers to be rerecorded.
	VulkanCommandManager(CommandQueueFamily queueFamily, bool bAllowReuse);
	virtual ~VulkanCommandManager();

	VulkanCommandManager(const VulkanCommandManager&) = delete;
	VulkanCommandManager(VulkanCommandManager&& other) noexcept
	{
		m_CommandPool = other.m_CommandPool;
		m_Queue = other.m_Queue;
		m_QueueFlags = other.m_QueueFlags;
		m_QueueFamilyIndex = other.m_QueueFamilyIndex;

		other.m_CommandPool = VK_NULL_HANDLE;
		other.m_Queue = VK_NULL_HANDLE;
		other.m_QueueFlags = 0;
		other.m_QueueFamilyIndex = uint32_t(-1);
	}

	VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;
	VulkanCommandManager& operator=(VulkanCommandManager&& other) noexcept = delete;

	VkQueue GetVulkanQueue() const { return m_Queue; }

	VulkanCommandBuffer AllocateCommandBuffer(bool bBegin = true);
	VulkanCommandBuffer AllocateSecondaryCommandbuffer(bool bBegin = true);

	void Submit(VulkanCommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
		const Ref<VulkanFence>& signalFence,
		const VulkanSemaphore* waitSemaphores = nullptr,   uint32_t waitSemaphoresCount = 0,
		const VulkanSemaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0);
	void Submit(VulkanCommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
		const VulkanSemaphore* waitSemaphores = nullptr,   uint32_t waitSemaphoresCount = 0,
		const VulkanSemaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0);

private:
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	VkQueue m_Queue = VK_NULL_HANDLE;
	VkQueueFlags m_QueueFlags = 0;
	uint32_t m_QueueFamilyIndex = uint32_t(-1);

	friend class VulkanCommandBuffer;
};

class VulkanCommandBuffer
{
private:
	VulkanCommandBuffer(const VulkanCommandManager& manager, bool bSecondary);

public:
	VulkanCommandBuffer() = default;
	VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
	VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
	{
		m_Device = other.m_Device;
		m_CommandPool = other.m_CommandPool;
		m_CommandBuffer = other.m_CommandBuffer;
		m_CurrentGraphicsPipeline = other.m_CurrentGraphicsPipeline;
		m_QueueFlags = other.m_QueueFlags;

		other.m_Device = VK_NULL_HANDLE;
		other.m_CommandBuffer = VK_NULL_HANDLE;
		other.m_CommandPool = VK_NULL_HANDLE;
		other.m_CurrentGraphicsPipeline = nullptr;
		other.m_QueueFlags = 0;
	}
	virtual ~VulkanCommandBuffer();

	VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
	VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept
	{
		if (m_CommandBuffer)
			vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);

		m_Device = other.m_Device;
		m_CommandPool = other.m_CommandPool;
		m_CommandBuffer = other.m_CommandBuffer;
		m_CurrentGraphicsPipeline = other.m_CurrentGraphicsPipeline;
		m_QueueFlags = other.m_QueueFlags;

		other.m_Device = VK_NULL_HANDLE;
		other.m_CommandBuffer = VK_NULL_HANDLE;
		other.m_CommandPool = VK_NULL_HANDLE;
		other.m_CurrentGraphicsPipeline = nullptr;
		other.m_QueueFlags = 0;

		return *this;
	}

	void Begin();
	void End();

	void Dispatch(VulkanComputePipeline* pipeline, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, const void* pushConstants);

	void BeginGraphics(VulkanGraphicsPipeline* pipeline);
	void BeginGraphics(VulkanGraphicsPipeline* pipeline, const VulkanFramebuffer& framebuffer);
	void EndGraphics();
	void Draw(uint32_t vertexCount, uint32_t firstVertex);
	void DrawIndexedInstanced(const VulkanBuffer* vertexBuffer, const VulkanBuffer* indexBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset,
		uint32_t instanceCount, uint32_t firstInstance, const VulkanBuffer* perInstanceBuffer);
	void DrawIndexed(const VulkanBuffer* vertexBuffer, const VulkanBuffer* indexBuffer, uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset);

	void SetGraphicsRootConstants(const void* vertexRootConstants, const void* fragmentRootConstants);

	void StorageImageBarrier(VulkanImage* image) { TransitionLayout(image, ImageLayoutType::StorageImage, ImageLayoutType::StorageImage); }
	void TransitionLayout(VulkanImage* image, ImageLayout oldLayout, ImageLayout newLayout);
	void TransitionLayout(VulkanImage* image, const ImageView& imageView, ImageLayout oldLayout, ImageLayout newLayout);
	void ClearColorImage(VulkanImage* image, const glm::vec4& color);
	void ClearDepthStencilImage(VulkanImage* image, float depthValue, uint32_t stencilValue);
	void CopyImage(const VulkanImage* src, const ImageView& srcView,
		VulkanImage* dst, const ImageView& dstView,
		const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
		const glm::uvec3& size);

	void StorageBufferBarrier(VulkanBuffer* buffer) { TransitionLayout(buffer, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer); }
	void TransitionLayout(VulkanBuffer* buffer, BufferLayout oldLayout, BufferLayout newLayout);
	void CopyBuffer(const VulkanBuffer* src, VulkanBuffer* dst, size_t srcOffset, size_t dstOffset, size_t size);
	void CopyBuffer(const VulkanStagingBuffer* src, VulkanBuffer* dst, size_t srcOffset, size_t dstOffset, size_t size);
	void FillBuffer(VulkanBuffer* dst, uint32_t data, size_t offset = 0, size_t numBytes = 0);

	void CopyBufferToImage(const VulkanBuffer* src, VulkanImage* dst, const std::vector<BufferImageCopy>& regions);
	void CopyImageToBuffer(const VulkanImage* src, VulkanBuffer* dst, const std::vector<BufferImageCopy>& regions);

	// TODO: Implement writing to all mips
	void Write(VulkanImage* image, const void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout);
	void Write(VulkanBuffer* buffer, const void* data, size_t size, size_t offset, BufferLayout initialLayout, BufferLayout finalLayout);

	void GenerateMips(VulkanImage* image, ImageLayout initialLayout, ImageLayout finalLayout);

	VkCommandBuffer GetVulkanCommandBuffer() const { return m_CommandBuffer; }

private:
	void CommitDescriptors(VulkanPipeline* pipeline, VkPipelineBindPoint bindPoint);

private:
	std::unordered_set<VulkanStagingBuffer*> m_UsedStagingBuffers;
	VkDevice m_Device = VK_NULL_HANDLE;
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
	VkQueueFlags m_QueueFlags;
	VulkanGraphicsPipeline* m_CurrentGraphicsPipeline = nullptr;

	friend class VulkanCommandManager;
};
