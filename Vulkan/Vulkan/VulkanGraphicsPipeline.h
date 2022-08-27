#pragma once

#include "Vulkan.h"
#include "VulkanUtils.h"
#include "VulkanShader.h"
#include "VulkanDescriptorManager.h"
#include "DescriptorSetData.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

struct ShaderSpecializationMapEntry
{
	uint32_t ConstantID = 0;
	uint32_t Offset = 0;
	size_t   Size = 0;
};

struct ShaderSpecializationInfo
{
	std::vector<ShaderSpecializationMapEntry> MapEntries;
	const void* Data = nullptr;
	size_t Size = 0;
};

struct GraphicsPipelineState
{
public:
	struct Attachment
	{
		VulkanImage* Image = nullptr;

		ImageLayout InitialLayout;
		ImageLayout FinalLayout;

		bool bClearEnabled = false;
		bool bBlendEnabled = false;
	};

	struct ColorAttachment : Attachment
	{
		glm::vec4 ClearColor{ 0.f };
		BlendState BlendingState;
	};

	struct DepthStencilAttachment : Attachment
	{
		float DepthClearValue = 0.f;
		uint32_t StencilClearValue = 0;
		bool bWriteDepth = false;
		CompareOperation DepthCompareOp = CompareOperation::Never;
	};

	struct VertexInputAttribute
	{
		uint32_t Location = 0;
	};

public:
	std::vector<ColorAttachment> ColorAttachments;
	std::vector<Attachment> ResolveAttachments;
	ShaderSpecializationInfo VertexSpecializationInfo;
	ShaderSpecializationInfo FragmentSpecializationInfo;
	std::vector<VertexInputAttribute> PerInstanceAttribs;
	VulkanShader* VertexShader = nullptr;
	VulkanShader* FragmentShader = nullptr;
	VulkanShader* GeometryShader = nullptr; // Optional
	DepthStencilAttachment DepthStencilAttachment;
	Topology Topology = Topology::Triangles;
	float LineWidth = 1.5f;
	bool bEnableConservativeRasterization = false;

	SamplesCount GetSamplesCount() const
	{
		SamplesCount samplesCount = s_InvalidSamplesCount;
		InitSamplesCount(DepthStencilAttachment.Image, samplesCount);
		for (auto& attachment : ColorAttachments)
		{
			InitSamplesCount(attachment.Image, samplesCount);
		}
		assert(samplesCount != s_InvalidSamplesCount);
		return samplesCount;
	}

private:
	void InitSamplesCount(const VulkanImage* image, SamplesCount& samples_count) const
	{
		if (image)
		{
			if (samples_count == s_InvalidSamplesCount)
			{
				samples_count = image->GetSamplesCount();
			}
			assert(samples_count == image->GetSamplesCount());
		}
	}

private:
	static constexpr SamplesCount s_InvalidSamplesCount = static_cast<SamplesCount>(-1);
};

class VulkanBuffer;
class VulkanImage;
class VulkanSampler;
class VulkanGraphicsPipeline
{
public:
	VulkanGraphicsPipeline(const GraphicsPipelineState& state, const VulkanGraphicsPipeline* parentPipeline = nullptr);
	virtual ~VulkanGraphicsPipeline();

	VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
	VulkanGraphicsPipeline& operator= (const VulkanGraphicsPipeline&) = delete;

	const GraphicsPipelineState& GetState() const { return m_State; }
	const void* GetRenderPassHandle() const { return m_RenderPass; }

	uint32_t GetWidth() const { return m_Width; }
	uint32_t GetHeight() const { return m_Height; }

	void SetBuffer(const VulkanBuffer* buffer, uint32_t set, uint32_t binding);
	void SetBuffer(const VulkanBuffer* buffer, size_t offset, size_t size, uint32_t set, uint32_t binding);
	void SetBufferArray(const std::vector<const VulkanBuffer*>& buffers, uint32_t set, uint32_t binding);

	void SetImage(const VulkanImage* image, uint32_t set, uint32_t binding);
	void SetImage(const VulkanImage* image, const ImageView& imageView, uint32_t set, uint32_t binding);
	void SetImageArray(const std::vector<const VulkanImage*>& images, uint32_t set, uint32_t binding);
	void SetImageArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding);

	void SetSampler(const VulkanSampler* sampler, uint32_t set, uint32_t binding);
	void SetImageSampler(const VulkanImage* image, const VulkanSampler* sampler, uint32_t set, uint32_t binding);
	void SetImageSampler(const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler, uint32_t set, uint32_t binding);
	void SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding);
	void SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding);

	const std::vector<VkDescriptorSetLayoutBinding>& GetSetBindings(uint32_t set) const { return m_SetBindings[set]; }

	VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const { assert(set < m_SetLayouts.size()); return m_SetLayouts[set]; }
	VkPipelineLayout GetVulkanPipelineLayout() const { return m_PipelineLayout; }

private:
	const std::unordered_map<uint32_t, VulkanDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }
	const std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() const { return m_DescriptorSetData; }
	std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() { return m_DescriptorSetData; }

	VulkanDescriptorSet& AllocateDescriptorSet(uint32_t set)
	{
		VulkanDescriptorSet& nonInitializedSet = m_DescriptorSets[set];
		nonInitializedSet = VulkanDescriptorManager::AllocateDescriptorSet(this, set);
		return nonInitializedSet;
	}

private:
	GraphicsPipelineState m_State;
	std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_SetBindings; // Not owned! Set -> Bindings
	std::vector<VkDescriptorSetLayout> m_SetLayouts;
	std::unordered_map<uint32_t, DescriptorSetData> m_DescriptorSetData; // Set -> Data
	std::unordered_map<uint32_t, VulkanDescriptorSet> m_DescriptorSets; // Set -> DescriptorSet
	VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
	uint32_t m_Width;
	uint32_t m_Height;

	friend class VulkanCommandBuffer;
};
