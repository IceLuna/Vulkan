#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"

class VulkanFence
{
public:
	VulkanFence(bool bSignaled = false)
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

		VkFenceCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		ci.flags = bSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		VK_CHECK(vkCreateFence(m_Device, &ci, nullptr, &m_Fence));
	}

	virtual ~VulkanFence()
	{
		if (m_Fence)
			vkDestroyFence(m_Device, m_Fence, nullptr);
	}

	VulkanFence(const VulkanFence&) = delete;
	VulkanFence(VulkanFence&& other) noexcept
	{
		m_Device = other.m_Device;
		m_Fence = other.m_Fence;

		other.m_Device = VK_NULL_HANDLE;
		other.m_Fence = VK_NULL_HANDLE;
	}

	VulkanFence& operator= (VulkanFence&& other) noexcept
	{
		if (m_Fence)
			vkDestroyFence(m_Device, m_Fence, nullptr);

		m_Device = other.m_Device;
		m_Fence = other.m_Fence;

		other.m_Device = VK_NULL_HANDLE;
		other.m_Fence = VK_NULL_HANDLE;
	}

	const VkFence& GetVulkanFence() const { return m_Fence; }
	bool IsSignaled() const
	{
		VkResult result = vkGetFenceStatus(m_Device, m_Fence);
		if (result == VK_SUCCESS)
			return true;
		else if (result == VK_NOT_READY)
			return false;

		VulkanCheckResult(result);
		return false;
	}
	void Reset() { VK_CHECK(vkResetFences(m_Device, 1, &m_Fence)); }
	void Wait(uint64_t timeout = UINT64_MAX) { VK_CHECK(vkWaitForFences(m_Device, 1, &m_Fence, VK_FALSE, timeout)); }

private:
	VkDevice m_Device;
	VkFence m_Fence = VK_NULL_HANDLE;
};
