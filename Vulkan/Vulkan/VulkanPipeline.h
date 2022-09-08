#pragma once

#include "DescriptorSetData.h"
#include "VulkanDescriptorManager.h"

#include <vector>
#include <unordered_map>

class VulkanTexture2D;
class VulkanPipeline
{
public:
	
	virtual ~VulkanPipeline()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

		for (auto& setLayout : m_SetLayouts)
			vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
		m_SetLayouts.clear();

		m_DescriptorSetData.clear();
		m_DescriptorSets.clear();
		m_SetBindings.clear();
	}

	void SetBuffer(const VulkanBuffer* buffer, uint32_t set, uint32_t binding);
	void SetBuffer(const VulkanBuffer* buffer, size_t offset, size_t size, uint32_t set, uint32_t binding);
	void SetBufferArray(const std::vector<const VulkanBuffer*>& buffers, uint32_t set, uint32_t binding);

	void SetImage(const VulkanImage* image, uint32_t set, uint32_t binding);
	void SetImage(const VulkanImage* image, const ImageView& imageView, uint32_t set, uint32_t binding);
	void SetImageArray(const std::vector<const VulkanImage*>& images, uint32_t set, uint32_t binding);
	void SetImageArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding);

	void SetImageSampler(const VulkanImage* image, const VulkanSampler* sampler, uint32_t set, uint32_t binding);
	void SetImageSampler(const VulkanTexture2D* texture, uint32_t set, uint32_t binding);
	void SetImageSampler(const VulkanTexture2D* texture, const ImageView& imageView, uint32_t set, uint32_t binding);
	void SetImageSampler(const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler, uint32_t set, uint32_t binding);
	void SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding);
	void SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding);

	const std::vector<VkDescriptorSetLayoutBinding>& GetSetBindings(uint32_t set) const { return m_SetBindings[set]; }
	VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const { assert(set < m_SetLayouts.size()); return m_SetLayouts[set]; }

	virtual VkPipelineLayout GetVulkanPipelineLayout() const = 0;

private:
	const std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() const { return m_DescriptorSetData; }
	std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() { return m_DescriptorSetData; }
	const std::unordered_map<uint32_t, VulkanDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }

	VulkanDescriptorSet& AllocateDescriptorSet(uint32_t set)
	{
		assert(m_DescriptorSets.find(set) == m_DescriptorSets.end()); // Should not be present
		VulkanDescriptorSet& nonInitializedSet = m_DescriptorSets[set];
		nonInitializedSet = VulkanDescriptorManager::AllocateDescriptorSet(this, set);
		return nonInitializedSet;
	}

protected:
	std::vector<VkDescriptorSetLayout> m_SetLayouts;
	std::unordered_map<uint32_t, DescriptorSetData> m_DescriptorSetData; // Set -> Data
	std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_SetBindings; // Not owned! Set -> Bindings
	std::unordered_map<uint32_t, VulkanDescriptorSet> m_DescriptorSets; // Set -> DescriptorSet

	friend class VulkanCommandBuffer;
};
