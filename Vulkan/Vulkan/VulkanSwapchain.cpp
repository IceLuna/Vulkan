#include "VulkanSwapchain.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

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
	for (auto& image : m_Images)
		delete image;
	m_Images.clear();
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
	VkPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pWaitSemaphores = waitSemaphore ? &waitSemaphore->GetVulkanSemaphore() : nullptr;
	info.waitSemaphoreCount = waitSemaphore ? 1 : 0;
	info.swapchainCount = 1;
	info.pSwapchains = &m_Swapchain;
	info.pImageIndices = &m_SwapchainPresentImageIndex;

	VkResult result = vkQueuePresentKHR(m_Device->GetPresentQueue(), &info);
	if (result != VK_SUCCESS)
	{
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			// Swap chain is no longer compatible with the surface and needs to be recreated
			OnResized();
			return;
		}
		else VK_CHECK(result);
	}
}

const VulkanSemaphore* VulkanSwapchain::AcquireImage(uint32_t* outFrameIndex)
{
	auto& semaphore = m_WaitSemaphores[m_FrameIndex];
	VK_CHECK(vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_Swapchain, UINT64_MAX, semaphore.GetVulkanSemaphore(), VK_NULL_HANDLE, &m_SwapchainPresentImageIndex));

	*outFrameIndex = m_SwapchainPresentImageIndex;
	m_FrameIndex = (m_FrameIndex + 1) % uint32_t(m_WaitSemaphores.size());
	return &semaphore;
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
	std::vector<VkImage> vkImages(imageCount);
	vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, vkImages.data());

	ImageSpecifications specs;
	specs.Size = glm::uvec3{ m_Extent.width, m_Extent.height, 1u };
	specs.Format = VulkanToImageFormat(m_Format.format);
	specs.Usage = ImageUsage::ColorAttachment;
	specs.Layout = ImageLayoutType::Present;
	specs.Type = ImageType::Type2D;

	for (auto& image : m_Images)
		delete image;
	m_Images.clear();

	const std::string debugName = "SwapchainImage";
	int i = 0;
	for (auto& image : vkImages)
		m_Images.push_back(new VulkanImage(image, specs, false, debugName + std::to_string(i++)));
}

void VulkanSwapchain::CreateSyncObjects()
{
	m_WaitSemaphores.clear();
	
	for (auto& imageView : m_Images)
		m_WaitSemaphores.emplace_back();
}
