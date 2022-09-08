#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanContext.h"

VulkanBuffer::VulkanBuffer(const BufferSpecifications& specs, const std::string& debugName)
	: m_DebugName(debugName)
	, m_Specs(specs)
{
	VkBufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = specs.Size;
	info.usage = BufferUsageToVulkan(specs.Usage);

	m_Allocation = VulkanAllocator::AllocateBuffer(&info, specs.MemoryType, false, &m_Buffer);
	
	if (!m_DebugName.empty())
		VulkanContext::AddResourceDebugName(m_Buffer, m_DebugName, VK_OBJECT_TYPE_BUFFER);
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
	Release();

	m_DebugName = std::move(other.m_DebugName);
	m_Specs = other.m_Specs;
	m_Buffer = other.m_Buffer;
	m_Allocation = other.m_Allocation;

	other.m_Specs = {};
	other.m_Buffer = VK_NULL_HANDLE;
	other.m_Allocation = VK_NULL_HANDLE;
	
	if (!m_DebugName.empty())
		VulkanContext::AddResourceDebugName(m_Buffer, m_DebugName, VK_OBJECT_TYPE_BUFFER);

	return *this;
}

void VulkanBuffer::Release()
{
	if (m_Buffer)
	{
		VulkanAllocator::DestroyBuffer(m_Buffer, m_Allocation);
		if (!m_DebugName.empty())
			VulkanContext::RemoveResourceDebugName(m_Buffer);
	}

	m_Buffer = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;
}
