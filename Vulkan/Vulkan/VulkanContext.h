#pragma once

#include "Vulkan.h"
#include "VulkanDevice.h"
#include "../Renderer/Renderer.h"

#include <unordered_map>

struct VulkanFunctions
{
	PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT;
};

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

	static VulkanFunctions& GetFunctions() { return VulkanContext::Get().m_Functions; }

	static void AddResourceDebugName(void* resourceID, const std::string& name, VkObjectType objectType)
	{
		auto& context = VulkanContext::Get();
		context.m_ResourcesDebugNames[resourceID] = name;

		if (context.m_Functions.setDebugUtilsObjectNameEXT)
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = objectType;
			nameInfo.pObjectName = name.c_str();
			nameInfo.objectHandle = (size_t)resourceID;
			context.m_Functions.setDebugUtilsObjectNameEXT(context.m_Device->GetVulkanDevice(), &nameInfo);
		}
	}

	static void RemoveResourceDebugName(void* resourceID)
	{
		auto& context = VulkanContext::Get();
		context.m_ResourcesDebugNames.erase(resourceID);
	}

	static const std::string& GetResourceDebugName(void* resourceID)
	{
		static const std::string UNKNOWN_NAME = "UNKNOWN_NAME";
		auto it = VulkanContext::Get().m_ResourcesDebugNames.find(resourceID);
		if (it == VulkanContext::Get().m_ResourcesDebugNames.end())
			return UNKNOWN_NAME;
		return it->second;
	}

private:
	void InitFunctions();

private:
	std::unordered_map<void*, std::string> m_ResourcesDebugNames;
	VulkanFunctions m_Functions;
	std::unique_ptr<VulkanPhysicalDevice> m_PhysicalDevice;
	std::unique_ptr<VulkanDevice> m_Device;
	static VkInstance s_Instance;
};
