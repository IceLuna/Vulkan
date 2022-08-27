#pragma once

#include "Vulkan.h"
#include "DescriptorSetData.h"

#include <vector>

class VulkanGraphicsPipeline;
class VulkanDescriptorSet;

struct DescriptorWriteData
{
	const VulkanDescriptorSet* DescriptorSet = nullptr;
	DescriptorSetData* DescriptorSetData = nullptr;
};

class VulkanDescriptorManager
{
private:
	VulkanDescriptorManager() = default;

public:
	static void Init();
	static void Shutdown();
	static VulkanDescriptorSet AllocateDescriptorSet(const VulkanGraphicsPipeline* pipeline, uint32_t set);
	static void WriteDescriptors(const VulkanGraphicsPipeline* pipeline, const std::vector<DescriptorWriteData>& writeDatas);
};

class VulkanDescriptorSet
{
public:
	VulkanDescriptorSet() = default;
	VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
	{
		m_Device = other.m_Device;
		m_DescriptorSet = other.m_DescriptorSet;
		m_DescriptorPool = other.m_DescriptorPool;

		other.m_Device = VK_NULL_HANDLE;
		other.m_DescriptorSet = VK_NULL_HANDLE;
		other.m_DescriptorPool = VK_NULL_HANDLE;
	}
	VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept
	{
		if (m_DescriptorSet)
			vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &m_DescriptorSet);

		m_Device = other.m_Device;
		m_DescriptorSet = other.m_DescriptorSet;
		m_DescriptorPool = other.m_DescriptorPool;

		other.m_Device = VK_NULL_HANDLE;
		other.m_DescriptorSet = VK_NULL_HANDLE;
		other.m_DescriptorPool = VK_NULL_HANDLE;

		return *this;
	}

	virtual ~VulkanDescriptorSet()
	{
		if (m_DescriptorSet)
			vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &m_DescriptorSet);
	}

	uint32_t GetSetIndex() const { return m_SetIndex; }
	const VkDescriptorSet& GetVulkanDescriptorSet() const { return m_DescriptorSet; }

private:
	VulkanDescriptorSet(const VulkanGraphicsPipeline* pipeline, VkDescriptorPool pool, uint32_t set);

private:
	VkDevice m_Device = VK_NULL_HANDLE;
	VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	uint32_t m_SetIndex = 0;
	friend class VulkanDescriptorManager;
};
