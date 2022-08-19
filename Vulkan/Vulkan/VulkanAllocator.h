#pragma once

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

struct GPUMemoryStats
{
	uint64_t Used = 0;
	uint64_t Free = 0;
};

class VulkanDevice;
enum class MemoryType;

// TODO: Add pools
class VulkanAllocator
{
public:
	static void Init(const VulkanDevice* device);
	static void Shutdown();

	[[nodiscard]] static VmaAllocation AllocateBuffer(const VkBufferCreateInfo* bufferCI, MemoryType usage, bool bSeparateAllocation, VkBuffer* outBuffer);
	[[nodiscard]] static VmaAllocation AllocateImage(const VkImageCreateInfo* imageCI, MemoryType usage, bool bSeparateAllocation, VkImage* outImage);

	static void Free(VmaAllocation allocation);
	static void DestroyImage(VkImage image, VmaAllocation allocation);
	static void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

	static bool IsHostVisible(VmaAllocation allocation);
	static void* MapMemory(VmaAllocation allocation);
	static void UnmapMemory(VmaAllocation allocation);

	GPUMemoryStats GetStats();
};
