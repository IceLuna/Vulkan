#include "VulkanComputePipeline.h"
#include "VulkanPipelineCache.h"

VulkanComputePipeline::VulkanComputePipeline(const ComputePipelineState& state, const VulkanComputePipeline* parentPipeline)
	: m_State(state)
{
	assert(m_State.ComputeShader->GetType() == ShaderType::Compute);

	VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
	// Pipeline layout
	{
		m_SetBindings = state.ComputeShader->GeLayoutSetBindings();
		const uint32_t setsCount = (uint32_t)m_SetBindings.size();

		m_SetLayouts.clear();
		m_SetLayouts.resize(setsCount);
		for (uint32_t i = 0; i < setsCount; ++i)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = (uint32_t)m_SetBindings[i].size();
			layoutInfo.pBindings = m_SetBindings[i].data();

			VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SetLayouts[i]));
		}

		std::vector<VkPushConstantRange> pushConstants;
		for (auto& range : state.ComputeShader->GetPushConstantRanges())
			pushConstants.push_back(range);

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = setsCount;
		pipelineLayoutCI.pSetLayouts = m_SetLayouts.data();
		pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pushConstants.size();
		pipelineLayoutCI.pPushConstantRanges = pushConstants.data();

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_PipelineLayout));
	}

	VkPipelineShaderStageCreateInfo shaderStage = state.ComputeShader->GetPipelineShaderStageInfo();
	VkSpecializationInfo specializationInfo{};
	std::vector<VkSpecializationMapEntry> mapEntries;
	if (state.ComputeSpecializationInfo.Data && (state.ComputeSpecializationInfo.Size > 0))
	{
		const size_t mapEntriesCount = state.ComputeSpecializationInfo.MapEntries.size();
		mapEntries.reserve(mapEntriesCount);
		for (auto& entry : state.ComputeSpecializationInfo.MapEntries)
			mapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

		specializationInfo.pData = state.ComputeSpecializationInfo.Data;
		specializationInfo.dataSize = state.ComputeSpecializationInfo.Size;
		specializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
		specializationInfo.pMapEntries = mapEntries.data();
		shaderStage.pSpecializationInfo = &specializationInfo;
	}

	VkComputePipelineCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.stage = shaderStage;
	info.layout = m_PipelineLayout;
	info.basePipelineIndex = -1;
	info.basePipelineHandle = parentPipeline ? parentPipeline->m_ComputePipeline : VK_NULL_HANDLE;
	VK_CHECK(vkCreateComputePipelines(device, VulkanPipelineCache::GetCache(), 1, &info, nullptr, &m_ComputePipeline));
}

VulkanComputePipeline::~VulkanComputePipeline()
{
	VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

	vkDestroyPipeline(device, m_ComputePipeline, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

	m_ComputePipeline = VK_NULL_HANDLE;
	m_PipelineLayout = VK_NULL_HANDLE;
}
