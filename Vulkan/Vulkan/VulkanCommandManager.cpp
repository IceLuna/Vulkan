#include "VulkanCommandManager.h"
#include "VulkanContext.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"
#include "VulkanStagingManager.h"
#include "VulkanShader.h"

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

static VkQueueFlags GetQueueFlags(CommandQueueFamily queueFamily)
{
	switch (queueFamily)
	{
		case CommandQueueFamily::Graphics: return VK_QUEUE_GRAPHICS_BIT;
		case CommandQueueFamily::Compute:  return VK_QUEUE_COMPUTE_BIT;
		case CommandQueueFamily::Transfer: return VK_QUEUE_TRANSFER_BIT;
	}
	assert(!"Unknown queue family");
	return VK_QUEUE_GRAPHICS_BIT;
}

//------------------
// COMMAND MANAGER
//------------------
VulkanCommandManager::VulkanCommandManager(CommandQueueFamily queueFamily, bool bAllowReuse)
{
	const VulkanDevice* device = VulkanContext::GetDevice();
	VkDevice vulkanDevice = device->GetVulkanDevice();
	m_QueueFamilyIndex = SelectQueueFamilyIndex(queueFamily, device->GetPhysicalDevice()->GetFamilyIndices());
	m_Queue = SelectQueue(queueFamily, device);
	m_QueueFlags = GetQueueFlags(queueFamily);

	VkCommandPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.queueFamilyIndex = m_QueueFamilyIndex;
	ci.flags = bAllowReuse ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;

	VK_CHECK(vkCreateCommandPool(vulkanDevice, &ci, nullptr, &m_CommandPool));
}

VulkanCommandManager::~VulkanCommandManager()
{
	if (m_CommandPool)
		vkDestroyCommandPool(VulkanContext::GetDevice()->GetVulkanDevice(), m_CommandPool, nullptr);
}

VulkanCommandBuffer VulkanCommandManager::AllocateCommandBuffer(bool bBegin)
{
	VulkanCommandBuffer cmd(*this);
	if (bBegin)
		cmd.Begin();
	return cmd;
}

void VulkanCommandManager::Submit(VulkanCommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
	const VulkanSemaphore* waitSemaphores, uint32_t waitSemaphoresCount,
	const VulkanSemaphore* signalSemaphores, uint32_t signalSemaphoresCount,
	const VulkanFence* signalFence)
{
	bool bOwnsFence = signalFence == nullptr;
	if (bOwnsFence)
		signalFence = new VulkanFence();

	std::vector<VkCommandBuffer> vkCmdBuffers(cmdBuffersCount);
	for (uint32_t i = 0; i < cmdBuffersCount; ++i)
	{
		VulkanCommandBuffer* cmdBuffer = cmdBuffers + i;
		vkCmdBuffers[i] = cmdBuffer->m_CommandBuffer;

		for (auto& staging : cmdBuffer->m_UsedStagingBuffers)
		{
			if (staging->GetState() == StagingBufferState::Pending)
			{
				staging->SetFence(signalFence);
				staging->SetState(StagingBufferState::InFlight);
			}
		}
	}

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
	m_QueueFlags = manager.m_QueueFlags;

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

void VulkanCommandBuffer::BeginGraphics(VulkanGraphicsPipeline& pipeline)
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
	beginInfo.framebuffer = pipeline.m_Framebuffer;
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

void VulkanCommandBuffer::BeginGraphics(VulkanGraphicsPipeline& pipeline, const VulkanFramebuffer& framebuffer)
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

	glm::uvec2 size = framebuffer.GetSize();
	VkRenderPassBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = pipeline.m_RenderPass;
	beginInfo.framebuffer = framebuffer.GetVulkanFramebuffer();
	beginInfo.clearValueCount = uint32_t(clearValues.size());
	beginInfo.pClearValues = clearValues.data();
	beginInfo.renderArea.extent = { size.x, size.y };
	vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GraphicsPipeline);

	VkViewport viewport{};
	viewport.width = float(size.x);
	viewport.height = float(size.y);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = { size.x, size.y };
	vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
}

void VulkanCommandBuffer::EndGraphics()
{
	assert(m_CurrentGraphicsPipeline);

	vkCmdEndRenderPass(m_CommandBuffer);
	m_CurrentGraphicsPipeline = nullptr;
}

void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
	CommitDescriptors(m_CurrentGraphicsPipeline);
	vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
}

void VulkanCommandBuffer::SetGraphicsRootConstants(const void* vertexRootConstants, const void* fragmentRootConstants)
{
	assert(m_CurrentGraphicsPipeline);
	const auto& pipelineState = m_CurrentGraphicsPipeline->GetState();
	
	const VulkanShader* vs = pipelineState.VertexShader;
	const VulkanShader* fs = pipelineState.FragmentShader;
	VkPipelineLayout pipelineLayout = m_CurrentGraphicsPipeline->GetVulkanPipelineLayout();

	// Also sets fragmentRootConstants if present
	if (vertexRootConstants)
	{
		auto& ranges = vs->GetPushConstantRanges();
		assert(ranges.size());

		VkPushConstantRange range = ranges[0];
		range.stageFlags |= (vertexRootConstants == fragmentRootConstants) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
		vkCmdPushConstants(m_CommandBuffer, pipelineLayout, range.stageFlags, range.offset, range.size, vertexRootConstants);
	}
	if (fragmentRootConstants && (vertexRootConstants != fragmentRootConstants))
	{
		auto& ranges = fs->GetPushConstantRanges();
		assert(ranges.size());

		VkPushConstantRange range = ranges[0];
		if (vertexRootConstants)
		{
			auto& vsRanges = vs->GetPushConstantRanges();
			assert(vsRanges.size());

			range.offset += vsRanges[0].size;
			range.size -= vsRanges[0].size;
		}
		vkCmdPushConstants(m_CommandBuffer, pipelineLayout, range.stageFlags, range.offset, range.size, fragmentRootConstants);
	}
}

void VulkanCommandBuffer::TransitionLayout(VulkanImage* image, ImageLayout oldLayout, ImageLayout newLayout)
{
	TransitionLayout(image, ImageView{ 0, image->GetMipsCount(), 0 }, oldLayout, newLayout);
}

void VulkanCommandBuffer::TransitionLayout(VulkanImage* image, const ImageView& imageView, ImageLayout oldLayout, ImageLayout newLayout)
{
	const VkImageLayout vkOldLayout = ToVulkanLayout(oldLayout);
	const VkImageLayout vkNewLayout = ToVulkanLayout(newLayout);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vkOldLayout;
	barrier.newLayout = vkNewLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->GetVulkanImage();

	barrier.subresourceRange.baseMipLevel = imageView.MipLevel;
	barrier.subresourceRange.baseArrayLayer = imageView.Layer;
	barrier.subresourceRange.levelCount = imageView.MipLevels;
	barrier.subresourceRange.layerCount = image->GetLayersCount();
	barrier.subresourceRange.aspectMask = image->GetTransitionAspectMask(oldLayout, newLayout);

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	GetTransitionStagesAndAccesses(vkOldLayout, m_QueueFlags, vkNewLayout, m_QueueFlags, &srcStage, &barrier.srcAccessMask, &dstStage, &barrier.dstAccessMask);

	vkCmdPipelineBarrier(m_CommandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

void VulkanCommandBuffer::ClearColorImage(VulkanImage* image, const glm::vec4& color)
{
	assert(image->GetDefaultAspectMask() == VK_IMAGE_ASPECT_COLOR_BIT);
	VkClearColorValue clearColor{};
	clearColor.float32[0] = color.r;
	clearColor.float32[1] = color.g;
	clearColor.float32[2] = color.b;
	clearColor.float32[3] = color.a;

	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = image->GetMipsCount();
	range.layerCount = image->GetLayersCount();

	vkCmdClearColorImage(m_CommandBuffer, image->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
}

void VulkanCommandBuffer::ClearDepthStencilImage(VulkanImage* image, float depthValue, uint32_t stencilValue)
{
	VkImageAspectFlags aspectMask = image->GetDefaultAspectMask();
	VkFormat format = image->GetVulkanFormat();
	assert((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) > 0 || (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT));

	VkClearDepthStencilValue clearValue{};
	clearValue.depth = depthValue;
	clearValue.stencil = stencilValue;

	VkImageSubresourceRange range{};
	range.aspectMask = aspectMask;
	range.levelCount = image->GetMipsCount();
	range.layerCount = image->GetLayersCount();

	vkCmdClearDepthStencilImage(m_CommandBuffer, image->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
}

void VulkanCommandBuffer::CopyImage(const VulkanImage* src, const ImageView& srcView,
	VulkanImage* dst, const ImageView& dstView,
	const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
	const glm::uvec3& size)
{
	assert(src->GetDefaultAspectMask() == dst->GetDefaultAspectMask());
	assert(src->HasUsage(ImageUsage::TransferSrc));
	assert(dst->HasUsage(ImageUsage::TransferDst));

	VkImage vkSrcImage = src->GetVulkanImage();
	VkImage vkDstImage = dst->GetVulkanImage();
	VkImageAspectFlags aspectMask = src->GetDefaultAspectMask();

	VkImageCopy copyRegion{};
	copyRegion.srcOffset = { srcOffset.x, srcOffset.y, srcOffset.z };
	copyRegion.srcSubresource.aspectMask = aspectMask;
	copyRegion.srcSubresource.mipLevel = srcView.MipLevel;
	copyRegion.srcSubresource.baseArrayLayer = srcView.Layer;
	copyRegion.srcSubresource.layerCount = src->GetLayersCount();

	copyRegion.srcOffset = { dstOffset.x, dstOffset.y, dstOffset.z };
	copyRegion.srcSubresource.aspectMask = aspectMask;
	copyRegion.srcSubresource.mipLevel = dstView.MipLevel;
	copyRegion.srcSubresource.baseArrayLayer = dstView.Layer;
	copyRegion.srcSubresource.layerCount = dst->GetLayersCount();

	copyRegion.extent = { size.x, size.y, size.z };
	vkCmdCopyImage(m_CommandBuffer, vkSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vkDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copyRegion);
}

void VulkanCommandBuffer::TransitionLayout(VulkanBuffer* buffer, BufferLayout oldLayout, BufferLayout newLayout)
{
	VkPipelineStageFlags srcStage, dstStage;
	VkAccessFlags srcAccess, dstAccess;

	GetStageAndAccess(oldLayout, m_QueueFlags, &srcStage, &srcAccess);
	GetStageAndAccess(newLayout, m_QueueFlags, &dstStage, &dstAccess);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer->GetVulkanBuffer();
	barrier.size = buffer->GetSize();

	vkCmdPipelineBarrier(m_CommandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr);
}

void VulkanCommandBuffer::CopyBuffer(const VulkanBuffer* src, VulkanBuffer* dst, size_t srcOffset, size_t dstOffset, size_t size)
{
	assert(src->HasUsage(BufferUsage::TransferSrc));
	assert(dst->HasUsage(BufferUsage::TransferDst));

	VkBufferCopy region{};
	region.size = size;
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;

	vkCmdCopyBuffer(m_CommandBuffer, src->GetVulkanBuffer(), dst->GetVulkanBuffer(), 1, &region);
}

void VulkanCommandBuffer::FillBuffer(VulkanBuffer* dst, uint32_t data, size_t offset, size_t numBytes)
{
	assert(dst->HasUsage(BufferUsage::TransferDst));
	assert(numBytes % 4 == 0);

	vkCmdFillBuffer(m_CommandBuffer, dst->GetVulkanBuffer(), offset, numBytes ? numBytes : VK_WHOLE_SIZE, data);
}

void VulkanCommandBuffer::CopyBufferToImage(const VulkanBuffer* src, VulkanImage* dst, const std::vector<BufferImageCopy>& regions)
{
	const size_t regionsCount = regions.size();
	assert(src->HasUsage(BufferUsage::TransferSrc));
	assert(dst->HasUsage(ImageUsage::TransferDst));
	assert(regionsCount > 0);

	std::vector<VkBufferImageCopy> imageCopyRegions;
	imageCopyRegions.reserve(regionsCount);
	VkImageAspectFlags aspectMask = dst->GetDefaultAspectMask();

	for (auto& region : regions)
	{
		VkBufferImageCopy copyRegion = imageCopyRegions.emplace_back();
		copyRegion = {};

		copyRegion.bufferOffset = region.BufferOffset;
		copyRegion.bufferRowLength = region.BufferRowLength;
		copyRegion.bufferImageHeight = region.BufferImageHeight;

		copyRegion.imageSubresource.aspectMask = aspectMask;
		copyRegion.imageSubresource.baseArrayLayer = region.ImageArrayLayer;
		copyRegion.imageSubresource.layerCount = region.ImageArrayLayers;
		copyRegion.imageSubresource.mipLevel = region.ImageMipLevel;

		copyRegion.imageOffset = { region.ImageOffset.x, region.ImageOffset.y, region.ImageOffset.z };
		copyRegion.imageExtent = { region.ImageExtent.x, region.ImageExtent.y, region.ImageExtent.z };
	}

	vkCmdCopyBufferToImage(m_CommandBuffer, src->GetVulkanBuffer(), dst->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uint32_t(regionsCount), imageCopyRegions.data());
}

void VulkanCommandBuffer::CopyImageToBuffer(const VulkanImage* src, VulkanBuffer* dst, const std::vector<BufferImageCopy>& regions)
{
	const size_t regionsCount = regions.size();
	assert(src->HasUsage(ImageUsage::TransferSrc));
	assert(dst->HasUsage(BufferUsage::TransferDst));
	assert(regionsCount > 0);

	std::vector<VkBufferImageCopy> imageCopyRegions;
	imageCopyRegions.reserve(regionsCount);
	VkImageAspectFlags aspectMask = src->GetDefaultAspectMask();

	for (auto& region : regions)
	{
		VkBufferImageCopy copyRegion = imageCopyRegions.emplace_back();
		copyRegion = {};

		copyRegion.bufferOffset = region.BufferOffset;
		copyRegion.bufferRowLength = region.BufferRowLength;
		copyRegion.bufferImageHeight = region.BufferImageHeight;

		copyRegion.imageSubresource.aspectMask = aspectMask;
		copyRegion.imageSubresource.baseArrayLayer = region.ImageArrayLayer;
		copyRegion.imageSubresource.layerCount = region.ImageArrayLayers;
		copyRegion.imageSubresource.mipLevel = region.ImageMipLevel;

		copyRegion.imageOffset = { region.ImageOffset.x, region.ImageOffset.y, region.ImageOffset.z };
		copyRegion.imageExtent = { region.ImageExtent.x, region.ImageExtent.y, region.ImageExtent.z };
	}

	vkCmdCopyImageToBuffer(m_CommandBuffer, src->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetVulkanBuffer(), uint32_t(regionsCount), imageCopyRegions.data());
}

void VulkanCommandBuffer::Write(VulkanImage* image, const void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout)
{
	assert(image->HasUsage(ImageUsage::TransferDst));
	assert(!image->HasUsage(ImageUsage::DepthStencilAttachment)); // Writing to depth-stencil is not supported

	VulkanStagingBuffer* stagingBuffer = VulkanStagingManager::AcquireBuffer(size, false);
	m_UsedStagingBuffers.insert(stagingBuffer);
	void* mapped = stagingBuffer->Map();
	memcpy(mapped, data, size);
	stagingBuffer->Unmap();

	if (initialLayout != ImageLayoutType::CopyDest)
		TransitionLayout(image, initialLayout, ImageLayoutType::CopyDest);

	auto& imageSize = image->GetSize();
	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = image->GetDefaultAspectMask();
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = image->GetLayersCount();
	region.imageExtent = { imageSize.x, imageSize.y, imageSize.z };

	vkCmdCopyBufferToImage(m_CommandBuffer, stagingBuffer->GetVulkanBuffer(), image->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if (finalLayout != ImageLayoutType::CopyDest)
		TransitionLayout(image, ImageLayoutType::CopyDest, finalLayout);
}

void VulkanCommandBuffer::GenerateMips(VulkanImage* image, ImageLayout initialLayout, ImageLayout finalLayout)
{
	assert(image->HasUsage(ImageUsage::TransferSrc | ImageUsage::TransferDst));
	assert(!image->IsCube()); // Not supported
	assert(image->GetSamplesCount() == SamplesCount::Samples1); // Multisampled images not supported

	if (!VulkanContext::GetDevice()->GetPhysicalDevice()->IsMipGenerationSupported(image->GetFormat()))
	{
		std::cerr << "Physical Device does NOT support mips generation!\n";
		return;
	}

	TransitionLayout(image, initialLayout, ImageLayoutType::CopyDest);
	glm::ivec3 currentMipSize = image->GetSize();
	VkImage vkImage = image->GetVulkanImage();
	const uint32_t mipCount = image->GetMipsCount();
	const uint32_t layersCount = image->GetLayersCount();

	for (uint32_t i = 1; i < mipCount; ++i)
	{
		ImageView imageView{ i - 1, 1, layersCount };
		TransitionLayout(image, imageView, ImageLayoutType::CopyDest, ImageReadAccess::CopySource);

		glm::ivec3 nextMipSize = { currentMipSize.x >> 1, currentMipSize.y >> 1, currentMipSize.z >> 1 };
		nextMipSize = glm::max(glm::ivec3(1), nextMipSize);

		VkImageBlit imageBlit{};
		imageBlit.srcOffsets[0] = { 0, 0, 0 };
		imageBlit.srcOffsets[1] = { currentMipSize.x, currentMipSize.y, currentMipSize.z };
		imageBlit.dstOffsets[0] = { 0, 0, 0 };
		imageBlit.dstOffsets[1] = { nextMipSize.x, nextMipSize.y, nextMipSize.z };

		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = layersCount;

		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = layersCount;

		currentMipSize = nextMipSize;

		vkCmdBlitImage(m_CommandBuffer,
			vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &imageBlit,
			VK_FILTER_LINEAR);

		TransitionLayout(image, imageView, ImageReadAccess::CopySource, finalLayout);
	}

	ImageView lastImageView{ mipCount - 1, 1, 0 };
	TransitionLayout(image, lastImageView, ImageLayoutType::CopyDest, finalLayout);
}

void VulkanCommandBuffer::CommitDescriptors(VulkanGraphicsPipeline* pipeline)
{
	struct SetData
	{
		DescriptorSetData* Data;
		uint32_t Set;
	};

	auto& descriptorSetsData = pipeline->GetDescriptorSetsData();
	std::vector<SetData> dirtyDatas;
	dirtyDatas.reserve(descriptorSetsData.size());
	for (auto& it : descriptorSetsData)
	{
		if (it.second.IsDirty())
			dirtyDatas.push_back({ &it.second, it.first });
	}

	const size_t dirtyDataCount = dirtyDatas.size();
	std::vector<DescriptorWriteData> writeDatas;
	writeDatas.reserve(dirtyDataCount);

	// Populating writeData. Allocating DescriptorSet if neccessary
	auto& descriptorSets = pipeline->GetDescriptorSets();
	for (size_t i = 0; i < dirtyDataCount; ++i)
	{
		uint32_t set = dirtyDatas[i].Set;
		const VulkanDescriptorSet* currentDescriptorSet = nullptr;

		auto it = descriptorSets.find(set);
		if (it == descriptorSets.end())
			currentDescriptorSet = &pipeline->AllocateDescriptorSet(set);
		else
			currentDescriptorSet = &it->second;

		assert(currentDescriptorSet);
		writeDatas.push_back({ currentDescriptorSet, dirtyDatas[i].Data });
	}

	if (!dirtyDatas.empty())
		VulkanDescriptorManager::WriteDescriptors(pipeline, writeDatas);

	VkPipelineLayout vkPipelineLayout = pipeline->GetVulkanPipelineLayout();
	for (auto& it : descriptorSetsData)
	{
		uint32_t set = it.first;
		auto it = descriptorSets.find(set);
		assert(it != descriptorSets.end());
		
		vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout,
			set, 1, &it->second.GetVulkanDescriptorSet(), 0, nullptr);
	}
}
