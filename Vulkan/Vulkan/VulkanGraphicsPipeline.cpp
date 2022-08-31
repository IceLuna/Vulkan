#include "VulkanGraphicsPipeline.h"
#include "VulkanContext.h"
#include "VulkanPipelineCache.h"
#include "VulkanDescriptorManager.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture2D.h"
#include "VulkanBuffer.h"
#include "VulkanSampler.h"

#include "../Renderer/Renderer.h"
#include "../Core/Application.h"

#include <array>

static const VkPipelineDepthStencilStateCreateInfo s_DefaultDepthStencilCI
{
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // sType
	nullptr, // pNext
	0, // flags
	VK_FALSE, // depthTestEnable
	VK_FALSE, // depthWriteEnable
	VK_COMPARE_OP_LESS_OR_EQUAL, // depthCompareOp
	VK_FALSE, // depthBoundsTestEnable
	VK_FALSE, // stencilTestEnable
	{}, // front
	{},	// back
	0.f, // minDepthBounds
	0.f	 // maxDepthBounds
};

static void MergeDescriptorSetLayoutBindings(std::vector<VkDescriptorSetLayoutBinding>& dstBindings,
	const std::vector<VkDescriptorSetLayoutBinding>& srcBindings)
{
	std::vector<VkDescriptorSetLayoutBinding> newBindings;
	newBindings.reserve(srcBindings.size());

	auto cmp = [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
	{
		return a.binding < b.binding;
	};

	auto dstIt = dstBindings.begin();
	for (auto& srcBinding : srcBindings)
	{
		dstIt = std::lower_bound(dstIt, dstBindings.end(), srcBinding, cmp);

		if (dstIt == dstBindings.end() || dstIt->binding > srcBinding.binding)
			newBindings.push_back(srcBinding);
		else
			dstIt->stageFlags |= srcBinding.stageFlags;
	}

	dstBindings.insert(dstBindings.end(), newBindings.begin(), newBindings.end());
	std::sort(dstBindings.begin(), dstBindings.end(), cmp);
}

static void MergeDescriptorSetLayoutBindings(std::vector<std::vector<VkDescriptorSetLayoutBinding>>& dstSetBindings,
	const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& srcSetBindings)
{
	if (srcSetBindings.size() > dstSetBindings.size())
		dstSetBindings.resize(srcSetBindings.size());

	for (std::size_t i = 0; i < srcSetBindings.size(); i++)
		MergeDescriptorSetLayoutBindings(dstSetBindings[i], srcSetBindings[i]);
}

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const GraphicsPipelineState& state, const VulkanGraphicsPipeline* parentPipeline)
	: m_State(state)
{
	const VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
	const size_t colorAttachmentsCount = state.ColorAttachments.size();
	const bool bDeviceSupportsConservativeRasterization = VulkanContext::GetDevice()->GetPhysicalDevice()->GetExtensionSupport().SupportsConservativeRasterization;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = TopologyToVulkan(state.Topology);

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.scissorCount = 1;
	viewportState.viewportCount = 1;

	VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterizationCI{};
	conservativeRasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
	conservativeRasterizationCI.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT; // TODO: Test different modes

	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.rasterizerDiscardEnable = VK_FALSE; // No geometry passes rasterization stage if set to TRUE
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.lineWidth = state.LineWidth;
	rasterization.cullMode = CullModeToVulkan(state.CullMode);
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.pNext = (state.bEnableConservativeRasterization && bDeviceSupportsConservativeRasterization) ? &conservativeRasterizationCI : nullptr;

	if (state.bEnableConservativeRasterization && !bDeviceSupportsConservativeRasterization)
		std::cerr << "[Renderer::WARN] Conservation rasterization was requested but device doesn't support it. So it was not enabled\n";

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = GetVulkanSamplesCount(state.GetSamplesCount());

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(colorAttachmentsCount);
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	for (size_t i = 0; i < colorAttachmentsCount; ++i)
	{
		colorBlendAttachment.blendEnable = state.ColorAttachments[i].bBlendEnabled;
		
		const BlendState& blendState = state.ColorAttachments[i].BlendingState;
		colorBlendAttachment.colorBlendOp = (VkBlendOp)blendState.BlendOp;
		colorBlendAttachment.alphaBlendOp = (VkBlendOp)blendState.BlendOpAlpha;
		colorBlendAttachment.srcColorBlendFactor = (VkBlendFactor)blendState.BlendSrc;
		colorBlendAttachment.dstColorBlendFactor = (VkBlendFactor)blendState.BlendDst;
		colorBlendAttachment.srcAlphaBlendFactor = (VkBlendFactor)blendState.BlendSrcAlpha;
		colorBlendAttachment.dstAlphaBlendFactor = (VkBlendFactor)blendState.BlendDstAlpha;

		colorBlendAttachmentStates[i] = colorBlendAttachment;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = (uint32_t)colorBlendAttachmentStates.size();
	colorBlending.pAttachments = colorBlendAttachmentStates.data();

	// Pipeline layout
	{
		m_SetBindings = state.VertexShader->GeLayoutSetBindings();
		if (state.GeometryShader)
			MergeDescriptorSetLayoutBindings(m_SetBindings, state.GeometryShader->GeLayoutSetBindings());
		MergeDescriptorSetLayoutBindings(m_SetBindings, state.FragmentShader->GeLayoutSetBindings());
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
		for (auto& range : state.VertexShader->GetPushConstantRanges())
			pushConstants.push_back(range);

		if (state.GeometryShader)
			for (auto& range : state.GeometryShader->GetPushConstantRanges())
				pushConstants.push_back(range);

		for (auto& range : state.FragmentShader->GetPushConstantRanges())
			pushConstants.push_back(range);

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = setsCount;
		pipelineLayoutCI.pSetLayouts = m_SetLayouts.data();
		pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pushConstants.size();
		pipelineLayoutCI.pPushConstantRanges = pushConstants.data();

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_PipelineLayout));
	}

	// Render pass
	std::vector<VkAttachmentDescription> attachmentDescs;
	std::vector<VkAttachmentReference> colorRefs(colorAttachmentsCount);
	std::vector<VkAttachmentReference> resolveRefs;
	std::vector<VkAttachmentReference> depthRef;
	std::vector<VkImageView> attachmentsImageViews;
	attachmentsImageViews.reserve(colorAttachmentsCount);

	if (state.ResolveAttachments.size())
	{
		resolveRefs.resize(colorAttachmentsCount);
		for (auto& ref : resolveRefs)
		{
			ref.attachment = VK_ATTACHMENT_UNUSED;
			ref.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	for (size_t i = 0; i < colorAttachmentsCount; ++i)
	{
		const VulkanImage* renderTarget = state.ColorAttachments[i].Image;
		if (!renderTarget)
		{
			colorRefs[i] = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
			continue;
		}

		assert(renderTarget->HasUsage(ImageUsage::ColorAttachment));

		uint32_t attachmentIndex = (uint32_t)attachmentDescs.size();
		const bool bClearEnabled = state.ColorAttachments[i].bClearEnabled;
		auto& desc = attachmentDescs.emplace_back();
		desc.samples = GetVulkanSamplesCount(renderTarget->GetSamplesCount());
		desc.loadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.format = renderTarget->GetVulkanFormat();
		desc.initialLayout = bClearEnabled ? VK_IMAGE_LAYOUT_UNDEFINED : ToVulkanLayout(state.ColorAttachments[i].InitialLayout);
		desc.finalLayout = ToVulkanLayout(state.ColorAttachments[i].FinalLayout);

		colorRefs[i] = { attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		attachmentsImageViews.push_back(renderTarget->GetVulkanImageView());
	}

	for (size_t i = 0; i < state.ResolveAttachments.size(); ++i)
	{
		const VulkanImage* renderTarget = state.ResolveAttachments[i].Image;
		if (!renderTarget)
		{
			colorRefs[i] = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
			continue;
		}

		assert(renderTarget->HasUsage(ImageUsage::ColorAttachment));

		uint32_t attachmentIndex = (uint32_t)attachmentDescs.size();
		attachmentDescs.push_back({});
		auto& desc = attachmentDescs.back();
		desc.samples = GetVulkanSamplesCount(renderTarget->GetSamplesCount());
		desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.format = renderTarget->GetVulkanFormat();
		desc.initialLayout = ToVulkanLayout(state.ResolveAttachments[i].InitialLayout);
		desc.finalLayout = ToVulkanLayout(state.ResolveAttachments[i].FinalLayout);

		resolveRefs[i] = { attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		attachmentsImageViews.push_back(renderTarget->GetVulkanImageView());
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilCI = s_DefaultDepthStencilCI;
	const VulkanImage* depthStencilImage = state.DepthStencilAttachment.Image;
	if (depthStencilImage)
	{
		assert(depthStencilImage->HasUsage(ImageUsage::DepthStencilAttachment));

		const bool bClearEnabled = state.DepthStencilAttachment.bClearEnabled;
		std::uint32_t attachmentIndex = static_cast<std::uint32_t>(attachmentDescs.size());
		auto& desc = attachmentDescs.emplace_back();
		desc.samples = GetVulkanSamplesCount(depthStencilImage->GetSamplesCount());
		desc.loadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.format = depthStencilImage->GetVulkanFormat();
		desc.initialLayout = bClearEnabled ? VK_IMAGE_LAYOUT_UNDEFINED : ToVulkanLayout(state.DepthStencilAttachment.InitialLayout);
		desc.finalLayout = ToVulkanLayout(state.DepthStencilAttachment.FinalLayout);

		const bool bDepthTestEnabled = (state.DepthStencilAttachment.DepthCompareOp != CompareOperation::Never);
		depthStencilCI.depthTestEnable = bDepthTestEnabled;
		depthStencilCI.depthCompareOp = bDepthTestEnabled ? CompareOpToVulkan(state.DepthStencilAttachment.DepthCompareOp) : VK_COMPARE_OP_NEVER;
		depthStencilCI.depthWriteEnable = state.DepthStencilAttachment.bWriteDepth;

		depthRef.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
		attachmentsImageViews.push_back(depthStencilImage->GetVulkanImageView());
	}

	assert(depthRef.size() == 0 || depthRef.size() == 1);

	VkSubpassDescription subpassDesc{};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = (uint32_t)colorAttachmentsCount;
	subpassDesc.pColorAttachments = colorRefs.data();
	subpassDesc.pResolveAttachments = state.ResolveAttachments.empty() ? nullptr : resolveRefs.data();
	subpassDesc.pDepthStencilAttachment = depthRef.empty() ? nullptr : depthRef.data();

	std::array<VkSubpassDependency, 2> dependencies{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCI{};
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCI.attachmentCount = (uint32_t)attachmentDescs.size();
	renderPassCI.pAttachments = attachmentDescs.data();
	renderPassCI.dependencyCount = (uint32_t)dependencies.size();
	renderPassCI.pDependencies = dependencies.data();
	renderPassCI.pSubpasses = &subpassDesc;
	renderPassCI.subpassCount = 1;
	VK_CHECK(vkCreateRenderPass(device, &renderPassCI, nullptr, &m_RenderPass));

	if (colorAttachmentsCount)
	{
		auto& size = state.ColorAttachments[0].Image->GetSize();
		m_Width  = size.x;
		m_Height = size.y;
	}
	else if (depthStencilImage)
	{
		auto& size = depthStencilImage->GetSize();
		m_Width  = size.x;
		m_Height = size.y;
	}
	else
	{
		m_Width = m_Height = 0;
	}

	VkFramebufferCreateInfo framebufferCI{};
	framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCI.attachmentCount = (uint32_t)attachmentsImageViews.size();
	framebufferCI.renderPass = m_RenderPass;
	framebufferCI.width = m_Width;
	framebufferCI.height = m_Height;
	framebufferCI.layers = 1;
	framebufferCI.pAttachments = attachmentsImageViews.data();
	VK_CHECK(vkCreateFramebuffer(device, &framebufferCI, nullptr, &m_Framebuffer));

	// Shaders
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.reserve(3);
	stages.push_back(state.VertexShader->GetPipelineShaderStageInfo());
	stages.push_back(state.FragmentShader->GetPipelineShaderStageInfo());
	if (state.GeometryShader)
		stages.push_back(state.GeometryShader->GetPipelineShaderStageInfo());

	VkSpecializationInfo vertexSpecializationInfo{};
	VkSpecializationInfo fragmentSpecializationInfo{};
	std::vector<VkSpecializationMapEntry> vertexMapEntries;
	std::vector<VkSpecializationMapEntry> fragmentMapEntries;

	if (state.VertexSpecializationInfo.Data && (state.VertexSpecializationInfo.Size > 0))
	{
		const size_t mapEntriesCount = state.VertexSpecializationInfo.MapEntries.size();
		vertexMapEntries.reserve(mapEntriesCount);
		for (auto& entry : state.VertexSpecializationInfo.MapEntries)
			vertexMapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

		vertexSpecializationInfo.pData = state.VertexSpecializationInfo.Data;
		vertexSpecializationInfo.dataSize = state.VertexSpecializationInfo.Size;
		vertexSpecializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
		vertexSpecializationInfo.pMapEntries = vertexMapEntries.data();
		stages[0].pSpecializationInfo = &vertexSpecializationInfo;
	}
	if (state.FragmentSpecializationInfo.Data && (state.FragmentSpecializationInfo.Size > 0))
	{
		const size_t mapEntriesCount = state.FragmentSpecializationInfo.MapEntries.size();
		fragmentMapEntries.reserve(mapEntriesCount);
		for (auto& entry : state.FragmentSpecializationInfo.MapEntries)
			fragmentMapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

		vertexSpecializationInfo.pData = state.FragmentSpecializationInfo.Data;
		vertexSpecializationInfo.dataSize = state.FragmentSpecializationInfo.Size;
		vertexSpecializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
		vertexSpecializationInfo.pMapEntries = fragmentMapEntries.data();
		stages[1].pSpecializationInfo = &vertexSpecializationInfo;
	}

	// Vertex input
	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	std::array<VkVertexInputBindingDescription, 2> vertexInputBindings;
	vertexInputBindings[0].binding = 0;
	vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexInputBindings[0].stride = 0;

	vertexInputBindings[1].binding = 1;
	vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	vertexInputBindings[1].stride = 0;

	auto vertexAttribs = state.VertexShader->GetInputAttribs();
	if (vertexAttribs.size() > 0)
	{
		for (auto& attrib : state.PerInstanceAttribs)
		{
			auto it = std::find_if(vertexAttribs.begin(), vertexAttribs.end(), [&attrib](const auto& it)
			{
				return it.location == attrib.Location;
			});

			if (it != vertexAttribs.end())
				it->binding = 1;
		}

		for (auto& attrib : vertexAttribs)
		{
			uint32_t size = attrib.offset;
			attrib.offset = vertexInputBindings[attrib.binding].stride;
			vertexInputBindings[attrib.binding].stride += size;
		}
		vertexInput.vertexBindingDescriptionCount = state.PerInstanceAttribs.empty() ? 1 : 2;
		vertexInput.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInput.vertexAttributeDescriptionCount = (uint32_t)vertexAttribs.size();
		vertexInput.pVertexAttributeDescriptions = vertexAttribs.data();
	}

	// Dynamic states
	std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStatesCI{};
	dynamicStatesCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStatesCI.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicStatesCI.pDynamicStates = dynamicStates.data();

	// Graphics Pipeline
	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.basePipelineIndex = -1;
	pipelineCI.basePipelineHandle = parentPipeline ? parentPipeline->m_GraphicsPipeline : VK_NULL_HANDLE;
	pipelineCI.layout = m_PipelineLayout;
	pipelineCI.stageCount = (uint32_t)stages.size();
	pipelineCI.pStages = stages.data();
	pipelineCI.renderPass = m_RenderPass;
	pipelineCI.pVertexInputState = &vertexInput;
	pipelineCI.pInputAssemblyState = &inputAssembly;
	pipelineCI.pRasterizationState = &rasterization;
	pipelineCI.pColorBlendState = &colorBlending;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilCI;
	pipelineCI.pMultisampleState = &multisampling;
	pipelineCI.pDynamicState = &dynamicStatesCI;

	VK_CHECK(vkCreateGraphicsPipelines(device, VulkanPipelineCache::GetCache(), 1, &pipelineCI, nullptr, &m_GraphicsPipeline));
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
	VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
	for (auto& setLayout : m_SetLayouts)
		vkDestroyDescriptorSetLayout(device, setLayout, nullptr);

	m_SetLayouts.clear();
	m_DescriptorSets.clear();
	m_DescriptorSetData.clear();

	vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);
	vkDestroyRenderPass(device, m_RenderPass, nullptr);
	vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

	m_GraphicsPipeline = VK_NULL_HANDLE;
	m_RenderPass = VK_NULL_HANDLE;
	m_Framebuffer = VK_NULL_HANDLE;
	m_PipelineLayout = VK_NULL_HANDLE;
}

void VulkanGraphicsPipeline::SetBuffer(const VulkanBuffer* buffer, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, buffer);
}

void VulkanGraphicsPipeline::SetBuffer(const VulkanBuffer* buffer, size_t offset, size_t size, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, buffer, offset, size);
}

void VulkanGraphicsPipeline::SetBufferArray(const std::vector<const VulkanBuffer*>& buffers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, buffers);
}

void VulkanGraphicsPipeline::SetImage(const VulkanImage* image, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image);
}

void VulkanGraphicsPipeline::SetImage(const VulkanImage* image, const ImageView& imageView, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, imageView);
}

void VulkanGraphicsPipeline::SetImageArray(const std::vector<const VulkanImage*>& images, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images);
}

void VulkanGraphicsPipeline::SetImageArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, imageViews);
}

void VulkanGraphicsPipeline::SetSampler(const VulkanSampler* sampler, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, sampler);
}

void VulkanGraphicsPipeline::SetImageSampler(const VulkanImage* image, const VulkanSampler* sampler, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, sampler);
}

void VulkanGraphicsPipeline::SetImageSampler(const VulkanTexture2D* texture, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), texture->GetSampler());
}

void VulkanGraphicsPipeline::SetImageSampler(const VulkanTexture2D* texture, const ImageView& imageView, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), imageView, texture->GetSampler());
}

void VulkanGraphicsPipeline::SetImageSampler(const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArg(binding, image, imageView, sampler);
}

void VulkanGraphicsPipeline::SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, samplers);
}

void VulkanGraphicsPipeline::SetImageSamplerArray(const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers, uint32_t set, uint32_t binding)
{
	m_DescriptorSetData[set].SetArgArray(binding, images, imageViews, samplers);
}

