#pragma once

#include "../Renderer/RendererUtils.h"
#include "VulkanAllocator.h"

struct BufferSpecifications
{
	size_t Size = 0;
	MemoryType MemoryType = MemoryType::Gpu;
	BufferUsage Usage = BufferUsage::None;
};

class VulkanBuffer
{
public:
	VulkanBuffer(const BufferSpecifications& specs);
	virtual ~VulkanBuffer();

	void* Map();
	void Unmap();

	size_t GetSize() const { return m_Specs.Size; }
	MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
	BufferUsage GetUsage() const { return m_Specs.Usage; }

	bool HasUsage(BufferUsage usage) const { return HasFlags(m_Specs.Usage, usage); }

	VkBuffer GetVulkanBuffer() const { return m_Buffer; }

private:
	BufferSpecifications m_Specs;
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;
};
