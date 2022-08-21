#include "VulkanBuffer.h"
#include "VulkanUtils.h"

VulkanBuffer::VulkanBuffer(const BufferSpecifications& specs)
	: m_Specs(specs)
{
	VkBufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = specs.Size;
	info.usage = BufferUsageToVulkan(specs.Usage);

	m_Allocation = VulkanAllocator::AllocateBuffer(&info, specs.MemoryType, false, &m_Buffer);
}

VulkanBuffer::~VulkanBuffer()
{
	if (m_Buffer)
		VulkanAllocator::DestroyBuffer(m_Buffer, m_Allocation);
}

void* VulkanBuffer::Map()
{
	assert(VulkanAllocator::IsHostVisible(m_Allocation));
	return VulkanAllocator::MapMemory(m_Allocation);
}

void VulkanBuffer::Unmap()
{
	if (m_Specs.MemoryType == MemoryType::Cpu || m_Specs.MemoryType == MemoryType::CpuToGpu)
		VulkanAllocator::FlushMemory(m_Allocation);
	VulkanAllocator::UnmapMemory(m_Allocation);
}
