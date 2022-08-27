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
