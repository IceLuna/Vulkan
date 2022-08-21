#pragma once

#include "Vulkan.h"

#include <vector>
#include "glm/glm.hpp"

class VulkanImage;

class VulkanFramebuffer
{
public:
	VulkanFramebuffer(const std::vector<VulkanImage*>& images, const void* renderPassHandle, glm::uvec2 size);
	virtual ~VulkanFramebuffer()
	{
		if (m_Framebuffer)
			vkDestroyFramebuffer(m_Device, m_Framebuffer, nullptr);
	}

	VulkanFramebuffer(const VulkanFramebuffer&) = delete;
	VulkanFramebuffer(VulkanFramebuffer&& other) noexcept
	{
		m_Device = other.m_Device;
		m_Framebuffer = other.m_Framebuffer;

		other.m_Device = VK_NULL_HANDLE;
		other.m_Framebuffer = VK_NULL_HANDLE;
	}

	VkFramebuffer GetVulkanFramebuffer() const { return m_Framebuffer; }
	glm::uvec2 GetSize() const { return m_Size; }

private:
	VkDevice m_Device = VK_NULL_HANDLE;
	VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	glm::uvec2 m_Size;
};
