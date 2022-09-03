#pragma once

#include "Vulkan.h"
#include "VulkanPipeline.h"
#include "VulkanShader.h"

struct ComputePipelineState
{
	VulkanShader* ComputeShader = nullptr;
	ShaderSpecializationInfo ComputeSpecializationInfo;
};

inline uint32_t CalcNumGroups(uint32_t size, uint32_t groupSize)
{
	return (size + groupSize - 1) / groupSize;
}

inline glm::uvec2 CalcNumGroups(glm::uvec2 size, uint32_t groupSize)
{
	return (size + groupSize - 1u) / groupSize;
}

inline glm::uvec3 CalcNumGroups(glm::uvec3 size, uint32_t groupSize)
{
	return (size + groupSize - 1u) / groupSize;
}

class VulkanComputePipeline : public VulkanPipeline
{
public:
	VulkanComputePipeline(const ComputePipelineState& state, const VulkanComputePipeline* parentPipeline = nullptr);
	virtual ~VulkanComputePipeline();

	VkPipeline GetVulkanPipeline() const { return m_ComputePipeline; }
	virtual VkPipelineLayout GetVulkanPipelineLayout() const override { return m_PipelineLayout; }

private:
	ComputePipelineState m_State;
	VkPipeline m_ComputePipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

	friend class VulkanCommandBuffer;
};
