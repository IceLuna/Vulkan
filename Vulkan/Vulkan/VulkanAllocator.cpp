#include "VulkanAllocator.h"
#include "VulkanContext.h"
#include "VulkanImage.h"

struct VulkanAllocatorData
{
	const VulkanDevice* Device;
    VmaAllocator Allocator;
    uint64_t TotalAllocatedBytes = 0;
    uint64_t TotalFreedBytes = 0;
};

static VmaMemoryUsage MemoryTypeToVmaUsage(MemoryType memory_type)
{
	switch (memory_type)
	{
		case MemoryType::Gpu: return VMA_MEMORY_USAGE_GPU_ONLY;
		case MemoryType::Cpu: return VMA_MEMORY_USAGE_CPU_ONLY;
		case MemoryType::CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
		case MemoryType::GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
		default:
			assert(!"Unknown memory type");
			return VMA_MEMORY_USAGE_UNKNOWN;
	}
}

static VulkanAllocatorData* s_AllocatorData = nullptr;

void VulkanAllocator::Init(const VulkanDevice* device)
{
	s_AllocatorData = new VulkanAllocatorData;
	s_AllocatorData->Device = device;

	VmaAllocatorCreateInfo ci{};
	ci.vulkanApiVersion = VulkanContext::GetVulkanAPIVersion();
	ci.physicalDevice = device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
	ci.device = device->GetVulkanDevice();
	ci.instance = VulkanContext::GetInstance();

	vmaCreateAllocator(&ci, &s_AllocatorData->Allocator);
}

void VulkanAllocator::Shutdown()
{
	vmaDestroyAllocator(s_AllocatorData->Allocator);

	if (s_AllocatorData->TotalAllocatedBytes != s_AllocatorData->TotalFreedBytes)
	{
		std::cerr << "[Vulkan allocator] Memory leak detected! Totally allocated bytes = "
			<< s_AllocatorData->TotalAllocatedBytes
			<< "; Totally freed bytes = " << s_AllocatorData->TotalFreedBytes << '\n';
	}

	delete s_AllocatorData;
	s_AllocatorData = nullptr;
}

VmaAllocation VulkanAllocator::AllocateBuffer(const VkBufferCreateInfo* bufferCI, MemoryType usage, bool bSeparateAllocation, VkBuffer* outBuffer)
{
	VmaAllocationCreateInfo ci{};
	ci.usage = MemoryTypeToVmaUsage(usage);
	ci.flags = bSeparateAllocation ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

	VmaAllocation allocation;
	vmaCreateBuffer(s_AllocatorData->Allocator, bufferCI, &ci, outBuffer, &allocation, nullptr);

	VmaAllocationInfo allocationInfo{};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
	s_AllocatorData->TotalAllocatedBytes += allocationInfo.size;

	return allocation;
}

VmaAllocation VulkanAllocator::AllocateImage(const VkImageCreateInfo* imageCI, MemoryType usage, bool bSeparateAllocation, VkImage* outImage)
{
	VmaAllocationCreateInfo ci{};
	ci.usage = MemoryTypeToVmaUsage(usage);
	ci.flags = bSeparateAllocation ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

	VmaAllocation allocation;
	vmaCreateImage(s_AllocatorData->Allocator, imageCI, &ci, outImage, &allocation, nullptr);

	VmaAllocationInfo allocationInfo{};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
	s_AllocatorData->TotalAllocatedBytes += allocationInfo.size;

	return allocation;
}

void VulkanAllocator::Free(VmaAllocation allocation)
{
	VmaAllocationInfo allocationInfo{};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
	s_AllocatorData->TotalFreedBytes += allocationInfo.size;

	vmaFreeMemory(s_AllocatorData->Allocator, allocation);
}

void VulkanAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
{
	VmaAllocationInfo allocationInfo{};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
	s_AllocatorData->TotalFreedBytes += allocationInfo.size;

	vmaDestroyImage(s_AllocatorData->Allocator, image, allocation);
}

void VulkanAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
{
	VmaAllocationInfo allocationInfo{};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
	s_AllocatorData->TotalFreedBytes += allocationInfo.size;

	vmaDestroyBuffer(s_AllocatorData->Allocator, buffer, allocation);
}

bool VulkanAllocator::IsHostVisible(VmaAllocation allocation)
{
	VmaAllocationInfo allocationInfo = {};
	vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);

	VkMemoryPropertyFlags memFlags = 0;
	vmaGetMemoryTypeProperties(s_AllocatorData->Allocator, allocationInfo.memoryType, &memFlags);
	return (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

void* VulkanAllocator::MapMemory(VmaAllocation allocation)
{
	void* mappedMemory;
	vmaMapMemory(s_AllocatorData->Allocator, allocation, (void**)&mappedMemory);
	return mappedMemory;
}

void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
{
	vmaUnmapMemory(s_AllocatorData->Allocator, allocation);
}

GPUMemoryStats VulkanAllocator::GetStats()
{
	const auto& memoryProps = s_AllocatorData->Device->GetPhysicalDevice()->GetMemoryProperties();
	std::vector<VmaBudget> budgets(memoryProps.memoryHeapCount);
	vmaGetHeapBudgets(s_AllocatorData->Allocator, budgets.data());

	uint64_t usage = 0;
	uint64_t budget = 0;

	for (VmaBudget& b : budgets)
	{
		usage += b.usage;
		budget += b.budget;
	}

	return { usage, budget - usage };
}
