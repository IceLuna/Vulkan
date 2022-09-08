#include "VulkanPipeline.h"

#include "VulkanTexture2D.h"

void VulkanPipeline::SetBuffer(const VulkanBuffer* buffer, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, buffer);
}

void VulkanPipeline::SetBuffer(const VulkanBuffer* buffer, size_t offset, size_t size, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, buffer, offset, size);
}

void VulkanPipeline::SetBufferArray(const std::vector<const VulkanBuffer*>& buffers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, buffers);
}

void VulkanPipeline::SetImage(const VulkanImage* image, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image);
}

void VulkanPipeline::SetImage(const VulkanImage* image, const ImageView& imageView, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, imageView);
}

void VulkanPipeline::SetImageArray(const std::vector<const VulkanImage*>& images, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images);
}

void VulkanPipeline::SetImageArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, imageViews);
}

void VulkanPipeline::SetImageSampler(const VulkanImage* image, const VulkanSampler* sampler, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, sampler);
}

void VulkanPipeline::SetImageSampler(const VulkanTexture2D* texture, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), texture->GetSampler());
}

void VulkanPipeline::SetImageSampler(const VulkanTexture2D* texture, const ImageView& imageView, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), imageView, texture->GetSampler());
}

void VulkanPipeline::SetImageSampler(const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, imageView, sampler);
}

void VulkanPipeline::SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, samplers);
}

void VulkanPipeline::SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, imageViews, samplers);
}
