#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"

class VulkanSemaphore
{
public:
	VulkanSemaphore()
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		VkSemaphoreCreateInfo ci { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(vkCreateSemaphore(m_Device, &ci, nullptr, &m_Semaphore));
	}

	VulkanSemaphore(const VulkanSemaphore&) = delete;
	VulkanSemaphore(VulkanSemaphore&& other) noexcept
	{
		m_Device = other.m_Device;
		m_Semaphore = other.m_Semaphore;

		other.m_Semaphore = VK_NULL_HANDLE;
		other.m_Device = VK_NULL_HANDLE;
	}

	~VulkanSemaphore()
	{
		if (m_Semaphore)
			vkDestroySemaphore(m_Device, m_Semaphore, nullptr);
	}

	const VkSemaphore& GetVulkanSemaphore() const { return m_Semaphore; }

private:
	VkDevice m_Device;
	VkSemaphore m_Semaphore = VK_NULL_HANDLE;
};
