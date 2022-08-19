#include "VulkanSwapchain.h"
#include "VulkanDevice.h"

#include <vector>
#include <array>
#include <algorithm>

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Utils
{
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		for (auto& format : formats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}

		// Return first available format
		return formats[0];
	}

	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes)
	{
		auto it = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
		if (it != modes.end())
			return VK_PRESENT_MODE_MAILBOX_KHR;

		it = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR);
		if (it != modes.end())
			return VK_PRESENT_MODE_IMMEDIATE_KHR;

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
	{
		if (capabilities.currentExtent.width != UINT_MAX)
			return capabilities.currentExtent;

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D extent = { uint32_t(width), uint32_t(height) };
		extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return extent;
	}
}

VulkanSwapchain::VulkanSwapchain(VkInstance instance, GLFWwindow* window)
	: m_Instance(instance)
	, m_Window(window)
{
	VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface));
}

VulkanSwapchain::~VulkanSwapchain()
{
	VkDevice device = m_Device->GetVulkanDevice();

	m_WaitSemaphores.clear();
	DestroyImageViews();
	DestroyFramebuffers();
	vkDestroyRenderPass(device, m_RenderPass, nullptr);
	vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
}

void VulkanSwapchain::Init(const VulkanDevice* device)
{
	m_Device = device;
	const VulkanPhysicalDevice* physicalDevice = m_Device->GetPhysicalDevice();
	if (!physicalDevice->RequiresPresentQueue())
	{
		std::cout << "Physical device either does NOT support presenting or does NOT request its support!\n";
		assert(false);
		std::exit(-1);
	}

	RecreateSwapchain();
	CreateSyncObjects();
}

void VulkanSwapchain::Present(const VulkanSemaphore* waitSemaphore)
{
	//auto& waitSemaphore = m_WaitSemaphores[m_FrameIndex].GetVulkanSemaphore();
	VkPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pWaitSemaphores = &waitSemaphore->GetVulkanSemaphore();
	info.waitSemaphoreCount = 1;
	info.swapchainCount = 1;
	info.pSwapchains = &m_Swapchain;
	info.pImageIndices = &m_SwapchainPresentImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_Device->GetPresentQueue(), &info));

	m_FrameIndex = (m_FrameIndex + 1) % uint32_t(m_WaitSemaphores.size());
}

const VulkanSemaphore* VulkanSwapchain::NextFrame()
{
	auto& waitSemaphore = m_WaitSemaphores[m_FrameIndex].GetVulkanSemaphore();
	VK_CHECK(vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_Swapchain, UINT64_MAX, waitSemaphore, VK_NULL_HANDLE, &m_SwapchainPresentImageIndex));
	return &m_WaitSemaphores[m_FrameIndex];
}

void VulkanSwapchain::RecreateSwapchain()
{
	VkDevice device = m_Device->GetVulkanDevice();
	VkSwapchainKHR oldSwapchain = m_Swapchain;
	m_Swapchain = VK_NULL_HANDLE;
	m_SupportDetails = m_Device->GetPhysicalDevice()->QuerySwapchainSupportDetails(m_Surface);

	const auto& capabilities = m_SupportDetails.Capabilities;

	m_Format = Utils::ChooseSwapSurfaceFormat(m_SupportDetails.Formats);
	VkPresentModeKHR presentMode = m_bVSyncEnabled ? VK_PRESENT_MODE_FIFO_KHR : Utils::ChooseSwapPresentMode(m_SupportDetails.PresentModes);
	m_Extent = Utils::ChooseSwapExtent(capabilities, m_Window);

	uint32_t imageCount = capabilities.minImageCount + 1;
	if ((capabilities.maxImageCount > 0) && (imageCount > capabilities.maxImageCount))
		imageCount = capabilities.maxImageCount;

	const auto& familyIndices = m_Device->GetPhysicalDevice()->GetFamilyIndices();
	uint32_t queueFamilyIndices[] = { familyIndices.GraphicsFamily, familyIndices.PresentFamily };

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageArrayLayers = 1;
	createInfo.imageExtent = m_Extent;
	createInfo.imageColorSpace = m_Format.colorSpace;
	createInfo.imageFormat = m_Format.format;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.presentMode = presentMode;
	createInfo.oldSwapchain = oldSwapchain;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;

	if (familyIndices.GraphicsFamily != familyIndices.PresentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain));

	if (oldSwapchain)
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

	// Swapchain can create more images than we requested. So we should retrive image count
	vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
	m_Images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_Images.data());

	if (m_RenderPass)
		vkDestroyRenderPass(device, m_RenderPass, nullptr);

	CreateRenderPass();
	CreateImageViews();
	CreateFramebuffers();
}

void VulkanSwapchain::CreateRenderPass()
{
	// Render pass
	std::array<VkAttachmentDescription, 1> attachments{};
	//Color Attachment
	attachments[0].format = m_Format.format;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc{};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCI{};
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCI.attachmentCount = (uint32_t)attachments.size();
	renderPassCI.dependencyCount = 1;
	renderPassCI.pAttachments = attachments.data();
	renderPassCI.pDependencies = &dependency;
	renderPassCI.pSubpasses = &subpassDesc;
	renderPassCI.subpassCount = 1;

	VK_CHECK(vkCreateRenderPass(m_Device->GetVulkanDevice(), &renderPassCI, nullptr, &m_RenderPass));
}

void VulkanSwapchain::CreateImageViews()
{
	DestroyImageViews();
	uint32_t imageCount = uint32_t(m_Images.size());
	m_ImageViews.resize(imageCount);

	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = m_Format.format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.subresourceRange.levelCount = 1;

	VkDevice device = m_Device->GetVulkanDevice();
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		viewCreateInfo.image = m_Images[i];
		VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, &m_ImageViews[i]));
	}
}

void VulkanSwapchain::DestroyImageViews()
{
	VkDevice device = m_Device->GetVulkanDevice();
	for (auto& view : m_ImageViews)
		vkDestroyImageView(device, view, nullptr);
	m_ImageViews.clear();
}

void VulkanSwapchain::CreateFramebuffers()
{
	DestroyFramebuffers();
	
	VkDevice device = m_Device->GetVulkanDevice();
	size_t count = m_ImageViews.size();
	m_Framebuffers.resize(count);


	VkFramebufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	ci.attachmentCount = 1;
	ci.layers = 1;
	ci.width = m_Extent.width;
	ci.height = m_Extent.height;
	ci.renderPass = m_RenderPass;
	for (size_t i = 0; i < count; ++i)
	{
		ci.pAttachments = &m_ImageViews[i];
		VK_CHECK(vkCreateFramebuffer(device, &ci, nullptr, &m_Framebuffers[i]));
	}
}

void VulkanSwapchain::DestroyFramebuffers()
{
	VkDevice device = m_Device->GetVulkanDevice();
	for (auto& framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	m_Framebuffers.clear();
}

void VulkanSwapchain::CreateSyncObjects()
{
	m_WaitSemaphores.clear();
	
	for (auto& imageView : m_ImageViews)
		m_WaitSemaphores.emplace_back();
}
