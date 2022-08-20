#include "VulkanFramebuffer.h"

#include "VulkanContext.h"
#include "VulkanImage.h"

VulkanFramebuffer::VulkanFramebuffer(const std::vector<VulkanImage*>& images, const void* renderPassHandle, glm::uvec2 size)
{
	assert(images.size());

	m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
	m_Size = images[0]->GetSize();

	const size_t imagesCount = images.size();
	std::vector<VkImageView> imageViews(imagesCount);
	for (size_t i = 0; i < imagesCount; ++i)
		imageViews[i] = images[i]->GetImageView();

	VkFramebufferCreateInfo framebufferCI{};
	framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCI.attachmentCount = (uint32_t)imageViews.size();
	framebufferCI.pAttachments = imageViews.data();
	framebufferCI.renderPass = (VkRenderPass)renderPassHandle;
	framebufferCI.width = size.x;
	framebufferCI.height = size.y;
	framebufferCI.layers = 1;
	VK_CHECK(vkCreateFramebuffer(m_Device, &framebufferCI, nullptr, &m_Framebuffer));
}
