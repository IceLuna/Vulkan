#pragma once

#include "Vulkan.h"
#include "VulkanUtils.h"
#include "VulkanShader.h"
#include "VulkanPipeline.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

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
	CompareOperation DepthCompareOp = CompareOperation::Never;
	bool bWriteDepth = false;
};

struct GraphicsPipelineState
{
public:
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
	CullMode CullMode = CullMode::None;
	float LineWidth = 1.0f;
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

class VulkanGraphicsPipeline : public VulkanPipeline
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

	// Resizes framebuffer
	void Resize(uint32_t width, uint32_t height);

	virtual VkPipelineLayout GetVulkanPipelineLayout() const override { return m_PipelineLayout; }

private:
	GraphicsPipelineState m_State;
	VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
	uint32_t m_Width;
	uint32_t m_Height;

	friend class VulkanCommandBuffer;
};
