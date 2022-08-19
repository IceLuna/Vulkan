#pragma once

#include "Vulkan.h"
#include "VulkanDevice.h"
#include "../Renderer/Renderer.h"

class VulkanContext
{
public:
	VulkanContext();
	~VulkanContext();

	// @ surface. Can be VK_NULL_HANDLE
	void InitDevices(VkSurfaceKHR surface, bool bRequireSurface);
	const VulkanDevice* GetContextDevice() { return m_Device.get(); }

	static uint32_t GetVulkanAPIVersion() { return VK_API_VERSION_1_2; }
	static VkInstance GetInstance() { return s_Instance; }
	static VulkanContext& Get() { return Renderer::GetContext(); }
	static const VulkanDevice* GetDevice() { return VulkanContext::Get().m_Device.get(); }

private:
	std::unique_ptr<VulkanPhysicalDevice> m_PhysicalDevice;
	std::unique_ptr<VulkanDevice> m_Device;
	static VkInstance s_Instance;
};
