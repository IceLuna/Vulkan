#pragma once

#include "../Renderer/RendererUtils.h"
#include "VulkanAllocator.h"

#include <string>

struct BufferSpecifications
{
	size_t Size = 0;
	MemoryType MemoryType = MemoryType::Gpu;
	BufferUsage Usage = BufferUsage::None;
};

class VulkanBuffer
{
public:
	VulkanBuffer(const BufferSpecifications& specs, const std::string& debugName = "");
	VulkanBuffer(VulkanBuffer&& other) noexcept
	{
		m_Specs = other.m_Specs;
		m_Buffer = other.m_Buffer;
		m_Allocation = other.m_Allocation;

		other.m_Specs = {};
		other.m_Buffer = VK_NULL_HANDLE;
		other.m_Allocation = VK_NULL_HANDLE;
	}

	virtual ~VulkanBuffer()
	{
		Release();
	}

	VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

	[[nodiscard]] void* Map()
	{
		assert(VulkanAllocator::IsHostVisible(m_Allocation));
		return VulkanAllocator::MapMemory(m_Allocation);
	}
	void Unmap()
	{
		if (m_Specs.MemoryType == MemoryType::Cpu || m_Specs.MemoryType == MemoryType::CpuToGpu)
			VulkanAllocator::FlushMemory(m_Allocation);
		VulkanAllocator::UnmapMemory(m_Allocation);
	}

	size_t GetSize() const { return m_Specs.Size; }
	MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
	BufferUsage GetUsage() const { return m_Specs.Usage; }

	bool HasUsage(BufferUsage usage) const { return HasFlags(m_Specs.Usage, usage); }

	VkBuffer GetVulkanBuffer() const { return m_Buffer; }

private:
	void Release();

private:
	std::string m_DebugName;
	BufferSpecifications m_Specs;
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;
};
