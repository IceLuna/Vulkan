#pragma once

#include "VulkanBuffer.h"
#include "VulkanFence.h"

enum class StagingBufferState
{
	Free, // Not used
	Pending, // Used in cmd buffer but wasn't submitted yet
	InFlight, // Submitted
};

class VulkanStagingBuffer
{
private:
	VulkanStagingBuffer(size_t size, bool bIsCPURead);

public:
	virtual ~VulkanStagingBuffer()
	{
		if (m_Buffer)
			delete m_Buffer;
	}

	VulkanStagingBuffer(VulkanStagingBuffer&& other) noexcept
	{
		m_Buffer = other.m_Buffer;
		m_Fence = other.m_Fence;
		m_State = other.m_State;
		m_bIsCPURead = other.m_bIsCPURead;

		other.m_Buffer = nullptr;
		other.m_Fence.reset();
		other.m_State = StagingBufferState::Free;
		other.m_bIsCPURead = false;
	}
	VulkanStagingBuffer& operator=(VulkanStagingBuffer&& other) noexcept
	{
		if (m_Buffer)
			delete m_Buffer;

		m_Buffer = other.m_Buffer;
		m_Fence = other.m_Fence;
		m_State = other.m_State;
		m_bIsCPURead = other.m_bIsCPURead;

		other.m_Buffer = nullptr;
		other.m_Fence.reset();
		other.m_State = StagingBufferState::Free;
		other.m_bIsCPURead = false;

		return *this;
	}

	[[nodiscard]] void* Map() { return m_Buffer->Map(); }
	void Unmap() { m_Buffer->Unmap(); }

	void SetState(StagingBufferState state) { m_State = state; }
	void SetFence(const Ref<VulkanFence>& fence) { m_Fence = fence; }
	Ref<VulkanFence>& GetFence() { return m_Fence; }
	const Ref<VulkanFence>& GetFence() const { return m_Fence; }

	StagingBufferState GetState() const { return m_State; }
	size_t GetSize() const { return m_Buffer->GetSize(); }
	bool IsCPURead() const { return m_bIsCPURead; }

	VulkanBuffer* GetBuffer() { return m_Buffer; }
	const VulkanBuffer* GetBuffer() const { return m_Buffer; }

	VkBuffer GetVulkanBuffer() const { return m_Buffer->GetVulkanBuffer(); }

private:
	VulkanBuffer* m_Buffer = nullptr;
	Ref<VulkanFence> m_Fence = nullptr;
	StagingBufferState m_State = StagingBufferState::Free;
	bool m_bIsCPURead = false;

	friend class VulkanStagingManager;
};

class VulkanStagingManager
{
public:
	VulkanStagingManager() = delete;

	static VulkanStagingBuffer* AcquireBuffer(size_t size, bool bIsCPURead);
	static void ReleaseBuffers();
};
