#include "VulkanCommandManager.h"
#include "VulkanContext.h"
#include "VulkanGraphicsPipeline.h"

static uint32_t SelectQueueFamilyIndex(CommandQueueFamily queueFamily, const QueueFamilyIndices& indices)
{
	switch (queueFamily)
	{
		case CommandQueueFamily::Graphics: return indices.GraphicsFamily;
		case CommandQueueFamily::Compute:  return indices.ComputeFamily;
		case CommandQueueFamily::Transfer: return indices.TransferFamily;
	}
	assert(!"Unknown queue family");
	return uint32_t(-1);
}

static VkQueue SelectQueue(CommandQueueFamily queueFamily, const VulkanDevice* device)
{
	switch (queueFamily)
	{
		case CommandQueueFamily::Graphics: return device->GetGraphicsQueue();
		case CommandQueueFamily::Compute:  return device->GetComputeQueue();
		case CommandQueueFamily::Transfer: return device->GetTransferQueue();
	}
	assert(!"Unknown queue family");
	return VK_NULL_HANDLE;
}

//------------------
// COMMAND MANAGER
//------------------
VulkanCommandManager::VulkanCommandManager(CommandQueueFamily queueFamily)
{
	const VulkanDevice* device = VulkanContext::GetDevice();
	VkDevice vulkanDevice = device->GetVulkanDevice();
	m_QueueFamilyIndex = SelectQueueFamilyIndex(queueFamily, device->GetPhysicalDevice()->GetFamilyIndices());
	m_Queue = SelectQueue(queueFamily, device);

	VkCommandPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.queueFamilyIndex = m_QueueFamilyIndex;

	VK_CHECK(vkCreateCommandPool(vulkanDevice, &ci, nullptr, &m_CommandPool));
}

VulkanCommandManager::~VulkanCommandManager()
{
	if (m_CommandPool)
		vkDestroyCommandPool(VulkanContext::GetDevice()->GetVulkanDevice(), m_CommandPool, nullptr);
}

VulkanCommandBuffer VulkanCommandManager::AllocateCommandBuffer()
{
	VulkanCommandBuffer cmd(*this);
	cmd.Begin();
	return cmd;
}

void VulkanCommandManager::Submit(const VulkanCommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
	const VulkanSemaphore* waitSemaphores, uint32_t waitSemaphoresCount,
	const VulkanSemaphore* signalSemaphores, uint32_t signalSemaphoresCount,
	const VulkanFence* signalFence)
{
	std::vector<VkCommandBuffer> vkCmdBuffers(cmdBuffersCount);
	for (uint32_t i = 0; i < cmdBuffersCount; ++i)
		vkCmdBuffers[i] = (cmdBuffers + i)->m_CommandBuffer;

	std::vector<VkSemaphore> vkSignalSemaphores(signalSemaphoresCount);
	for (uint32_t i = 0; i < signalSemaphoresCount; ++i)
		vkSignalSemaphores[i] = (signalSemaphores + i)->GetVulkanSemaphore();

	std::vector<VkSemaphore> vkWaitSemaphores(waitSemaphoresCount);
	std::vector<VkPipelineStageFlags> vkDstStageMask(waitSemaphoresCount);
	for (uint32_t i = 0; i < waitSemaphoresCount; ++i)
	{
		vkWaitSemaphores[i] = (waitSemaphores + i)->GetVulkanSemaphore();
		vkDstStageMask[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	VkSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.commandBufferCount = cmdBuffersCount;
	info.pCommandBuffers = vkCmdBuffers.data();
	info.waitSemaphoreCount = waitSemaphoresCount;
	info.pWaitSemaphores = vkWaitSemaphores.data();
	info.signalSemaphoreCount = signalSemaphoresCount;
	info.pSignalSemaphores = vkSignalSemaphores.data();
	info.pWaitDstStageMask = vkDstStageMask.data();

	VK_CHECK(vkQueueSubmit(m_Queue, 1, &info, signalFence->GetVulkanFence()));
}

//------------------
// COMMAND BUFFER
//------------------

VulkanCommandBuffer::VulkanCommandBuffer(const VulkanCommandManager& manager)
{
	m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
	m_CommandPool = manager.m_CommandPool;

	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = m_CommandPool;
	info.commandBufferCount = 1;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_CHECK(vkAllocateCommandBuffers(m_Device, &info, &m_CommandBuffer));
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
	if (m_CommandBuffer)
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
}

void VulkanCommandBuffer::Begin()
{
	VkCommandBufferBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &info));
}

void VulkanCommandBuffer::End()
{
	vkEndCommandBuffer(m_CommandBuffer);
}

void VulkanCommandBuffer::BeginGraphics(const VulkanGraphicsPipeline& pipeline, uint32_t frameIndex)
{
	auto& state = pipeline.GetState();
	m_CurrentGraphicsPipeline = &pipeline;

	size_t usedResolveAttachmentsCount = std::count_if(state.ResolveAttachments.begin(), state.ResolveAttachments.end(), [](const auto& attachment) { return attachment.Image; });
	std::vector<VkClearValue> clearValues(state.ColorAttachments.size() + usedResolveAttachmentsCount);
	size_t i = 0;
	for (auto& attachment : state.ColorAttachments)
	{
		VkClearValue clearValue{};
		if (attachment.bClearEnabled)
		{
			memcpy(&clearValue, &attachment.ClearColor, sizeof(clearValue));
			clearValues[i] = clearValue;
		}
		++i;
	}

	if (state.DepthStencilAttachment.Image)
	{
		VkClearValue clearValue{};
		if (state.DepthStencilAttachment.bClearEnabled)
			clearValue.depthStencil = { state.DepthStencilAttachment.DepthClearValue, state.DepthStencilAttachment.StencilClearValue };

		clearValues.push_back(clearValue);
	}

	VkRenderPassBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = pipeline.m_RenderPass;
	beginInfo.framebuffer = pipeline.m_Framebuffers[frameIndex];
	beginInfo.clearValueCount = uint32_t(clearValues.size());
	beginInfo.pClearValues = clearValues.data();
	beginInfo.renderArea.extent = { pipeline.m_Width, pipeline.m_Height };
	vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.width  = float(pipeline.m_Width);
	viewport.height = float(pipeline.m_Height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = { pipeline.m_Width, pipeline.m_Height };
	vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GraphicsPipeline);
}

void VulkanCommandBuffer::EndGraphics()
{
	assert(m_CurrentGraphicsPipeline);

	vkCmdEndRenderPass(m_CommandBuffer);
	m_CurrentGraphicsPipeline = nullptr;
}

void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
	vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
}
