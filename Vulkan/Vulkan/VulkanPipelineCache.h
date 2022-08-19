#pragma once

#include "Vulkan.h"

class VulkanPipelineCache
{
public:
	static void Init();
	static void Shutdown();

	static VkPipelineCache GetCache();
};
