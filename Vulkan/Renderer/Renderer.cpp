#include "Renderer.h"
#include "../Core/Application.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <iostream>
#include <vector>
#include <array>
#include <set>
#include <optional>
#include <cstdint> // Necessary for UINT32_MAX
#include <algorithm> // Necessary for std::clamp
#include <fstream>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui_internal.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

const std::vector<const char*> s_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static constexpr uint32_t s_MaxFramesInFlight = 2;

struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily;
	std::optional<uint32_t> PresentFamily;

	bool IsComplete()
	{
		return GraphicsFamily.has_value() && PresentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities;
	std::vector<VkSurfaceFormatKHR> Formats;
	std::vector<VkPresentModeKHR> PresentModes;
};

struct VulkanData
{
	VkInstance Instance;
	VkDebugUtilsMessengerEXT DebugMessenger;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	QueueFamilyIndices QueueFamily;
	VkDevice Device;
	VkQueue GraphicsQueue;
	VkQueue PresentQueue;
	VkSurfaceKHR Surface; //Don't need in Eagle, cuz we're rendering to off-screen framebuffer
	SwapChainSupportDetails SwapChainDetails;
	VkSwapchainKHR SwapChain;
	VkFormat SwapChainImageFormat;
	VkExtent2D SwapChainExtent;
	std::vector<VkImage> SwapChainImages;
	std::vector<VkImageView> SwapChainImageViews;
	VkRenderPass RenderPass;
	VkDescriptorSetLayout DescriptorSetLayout;
	VkDescriptorPool DescriptorPool;
	std::vector<VkDescriptorSet> DescriptorSets;
	VkPipelineLayout PipelineLayout;
	VkPipeline Pipeline;
	std::vector<VkFramebuffer> SwapChainFramebuffers;
	VkCommandPool CommandPool;
	std::vector<VkCommandBuffer> CommandBuffers;
	std::vector<VkSemaphore> ImageAvailableSemaphores;
	std::vector<VkSemaphore> RenderFinishedSemaphores;
	std::vector<VkFence> InFlightFences;
	std::vector<VkFence> InFlightImages;
	VkBuffer VertexBuffer;
	VkBuffer IndexBuffer;
	VkDeviceMemory VertexBufferMemory;
	VkDeviceMemory IndexBufferMemory;
	VkBuffer VBStagingBuffer;
	VkDeviceMemory VBStagingBufferMemory;
	std::vector<VkBuffer> UniformBuffers;
	std::vector<VkDeviceMemory> UniformBuffersMemory;
	VkImage TextureImage;
	VkImageView TextureImageView;
	VkSampler TextureSampler;
	VkDeviceMemory TextureImageMemory;
};

struct Vertex
{
	glm::vec2 Position = glm::vec2(0.f);
	glm::vec3 Color = glm::vec3(0.f);
	glm::vec2 TexCoord = glm::vec2(0.f);

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		// @InputRate
		//		VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
		//		VK_VERTEX_INPUT_RATE_INSTANCE : Move to the next data entry after each instance
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDesctiption()
	{
		std::array<VkVertexInputAttributeDescription, 3> attribs{};

		attribs[0].binding = 0;
		attribs[0].location = 0;
		attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribs[0].offset = offsetof(Vertex, Position);

		attribs[1].binding = 0;
		attribs[1].location = 1;
		attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribs[1].offset = offsetof(Vertex, Color);

		attribs[2].binding = 0;
		attribs[2].location = 2;
		attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
		attribs[2].offset = offsetof(Vertex, TexCoord);

		return attribs;
	}
};

static std::vector<Vertex> vertices =
{
	{ { -0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f } },
	{ {  0.5f, -0.5f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
	{ {  0.5f,  0.5f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },
	{ { -0.5f,  0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } }
};

static std::vector<uint32_t> indices =
{
	0, 1, 2,
	2, 3, 0
};

struct UniformBufferObject
{
	alignas(16) glm::mat4 Model = glm::mat4(1.f);
	alignas(16) glm::mat4 View  = glm::mat4(1.f);
	alignas(16) glm::mat4 Proj  = glm::mat4(1.f);
};

static VulkanData s_Data;
static bool s_EnableValidationLayers = false;
static std::vector<VkCommandBuffer> s_ImGuiCommandBuffers;
static VkDescriptorPool s_ImGui_DescriptorPool;
static uint32_t currentFrame = s_MaxFramesInFlight - 1;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	/* FIRST PARAM
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: Informational message like the creation of a resource
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: Message about behavior that is not necessarily an error, but very likely a bug in your application
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: Message about behavior that is invalid and may cause crashes
	*/
	
	/* SECOND PARAM
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: Something has happened that violates the specification or indicates a possible mistake
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: Potential non-optimal use of Vulkan
	*/
	std::cerr << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

static std::vector<const char*> GetRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (s_EnableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // To provide our own debug-message-callback

	return extensions;
}

static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& outCreateInfo)
{
	outCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	outCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	outCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	outCreateInfo.pfnUserCallback = DebugCallback;
	outCreateInfo.pUserData = nullptr;
}

static VkCommandBuffer GetSingleTimeCmdBuffer(bool begin)
{
	VkCommandBuffer cmdBuffer;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = s_Data.CommandPool;
	allocInfo.commandBufferCount = 1;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	vkAllocateCommandBuffers(s_Data.Device, &allocInfo, &cmdBuffer);

	if (begin)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmdBuffer, &beginInfo);
	}

	return cmdBuffer;
}

static VkCommandBuffer GetCommandBuffer(bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = s_Data.CommandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	vkAllocateCommandBuffers(s_Data.Device, &cmdBufAllocateInfo, &cmdBuffer);

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo);
	}

	return cmdBuffer;
}

static void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
{
	const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

	//EG_CORE_ASSERT(commandBuffer != VK_NULL_HANDLE);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	VkFence fence;
	vkCreateFence(s_Data.Device, &fenceCreateInfo, nullptr, &fence);

	// Submit to the queue
	vkQueueSubmit(queue, 1, &submitInfo, fence);
	// Wait for the fence to signal that command buffer has finished executing
	vkWaitForFences(s_Data.Device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);

	vkDestroyFence(s_Data.Device, fence, nullptr);
	vkFreeCommandBuffers(s_Data.Device, s_Data.CommandPool, 1, &commandBuffer);
}

static VkCommandBuffer CreateSecondaryCommandBuffer()
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = s_Data.CommandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	vkAllocateCommandBuffers(s_Data.Device, &cmdBufAllocateInfo, &cmdBuffer);
	return cmdBuffer;
}

static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Data.Surface, &details.Capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Data.Surface, &formatCount, nullptr);
	if (formatCount)
	{
		details.Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Data.Surface, &formatCount, details.Formats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Data.Surface, &presentModeCount, nullptr);
	if (presentModeCount)
	{
		details.PresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Data.Surface, &presentModeCount, details.PresentModes.data());
	}

	return details;
}

static void SetupDebugMessenger()
{
	if (!s_EnableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(s_Data.Instance, &createInfo, nullptr, &s_Data.DebugMessenger) != VK_SUCCESS)
	{
		std::cout << "Failed to create debug utils messenger!\n";
		exit(-1);
	}
}

static void CreateInstance()
{
	std::vector<const char*> extensions = GetRequiredExtensions();

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Eagle Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 6, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};

	if (s_EnableValidationLayers)
	{
		PopulateDebugMessengerCreateInfo(debugMessengerCreateInfo);

		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
		createInfo.pNext = &debugMessengerCreateInfo;
	}

	if (vkCreateInstance(&createInfo, nullptr, &s_Data.Instance) != VK_SUCCESS)
	{
		std::cout << "Failed to create VkInstance!\n";
		exit(-1);
	}
}

static VkImageView CreateImageView(VkImage image, VkFormat format)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.format = format;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(s_Data.Device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		std::cerr << "Failed to create Image view!\n";
		exit(-1);
	}
	return imageView;
}

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	VkBool32 bPresetSupport = false;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	uint32_t idx = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.GraphicsFamily = idx;

		vkGetPhysicalDeviceSurfaceSupportKHR(device, idx, s_Data.Surface, &bPresetSupport);
		if (bPresetSupport)
			indices.PresentFamily = idx;

		if (indices.IsComplete())
			break;

		++idx;
	}

	return indices;
}

static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availFormats)
{
	for (auto& format : availFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	return availFormats[0];
}

static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availModes)
{
	//TODO: if mobile, return VK_PRESENT_MODE_FIFO_KHR.
	for (auto& mode : availModes)
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;
	else
	{
		int w, h;
		glfwGetFramebufferSize(Application::GetApp().GetWindow().GetNativeWindow(), &w, &h);
		
		VkExtent2D actualExtent = 
		{
			(uint32_t)w,
			(uint32_t)h
		};
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

static void CreateSwapChain()
{
	s_Data.SwapChainDetails = QuerySwapChainSupport(s_Data.PhysicalDevice);
	const SwapChainSupportDetails& swapChainDetails = s_Data.SwapChainDetails;
	
	// Simply sticking to this minimum means that we may sometimes
	// have to wait on the driver to complete internal operations before
	// we can acquire another image to render to. Therefore it is recommended
	// to request at least one more image than the minimum
	uint32_t imageCount = swapChainDetails.Capabilities.minImageCount + 1;

	VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(s_Data.SwapChainDetails.Formats);
	VkPresentModeKHR presentMode = ChoosePresentMode(s_Data.SwapChainDetails.PresentModes);
	VkExtent2D extent = ChooseSwapChainExtent(s_Data.SwapChainDetails.Capabilities);
	
	s_Data.SwapChainExtent = extent;
	s_Data.SwapChainImageFormat = surfaceFormat.format;

	if (swapChainDetails.Capabilities.maxImageCount > 0 && imageCount > swapChainDetails.Capabilities.maxImageCount)
		imageCount = swapChainDetails.Capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = s_Data.Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // See: VK_IMAGE_USAGE_TRANSFER_DST_BIT

	/*
		VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a
			timeand ownership must be explicitly transferred before using 
			it in another queue family.This option offers the best performance.
	
		VK_SHARING_MODE_CONCURRENT : Images can be used across multiple queue 
			families without explicit ownership transfers.
	*/

	uint32_t queueFamilyIndices[] = { s_Data.QueueFamily.GraphicsFamily.value(), s_Data.QueueFamily.PresentFamily.value() };
	if (queueFamilyIndices[0] != queueFamilyIndices[1])
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; //Optional
		createInfo.pQueueFamilyIndices = nullptr; //Optional
	}

	createInfo.preTransform = swapChainDetails.Capabilities.currentTransform;

	//The compositeAlpha field specifies if the alpha channel should be used
	//for blending with other windows in the window system.
	//You'll almost always want to simply ignore the alpha channel, hence
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.presentMode = presentMode;

	//If the clipped member is set to VK_TRUE then that means that we don't
	//care about the color of pixels that are obscured, for example because 
	//another window is in front of them. Unless you really need to be able
	//to read these pixels back and get predictable results, 
	//you'll get the best performance by enabling clipping.
	createInfo.clipped = VK_TRUE;

	//With Vulkan it's possible that your swap chain becomes invalid or unoptimized
	//while your application is running, for example because the window was resized.
	//In that case the swap chain actually needs to be recreated from scratch and a
	//reference to the old one must be specified in this field. For now we'll assume
	//that we'll only ever create one swap chain.
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(s_Data.Device, &createInfo, nullptr, &s_Data.SwapChain) != VK_SUCCESS)
	{
		std::cerr << "Failed to create swap chain!\n";
		exit(-1);
	}

	vkGetSwapchainImagesKHR(s_Data.Device, s_Data.SwapChain, &imageCount, nullptr);
	s_Data.SwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(s_Data.Device, s_Data.SwapChain, &imageCount, s_Data.SwapChainImages.data());
}

static bool DoesDeviceSupportExtensions(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availExtensions.data());

	std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());
	
	for(auto& extension : availExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

static const char* PhysicalDeviceTypeToString(VkPhysicalDeviceType type)
{
	switch (type)
	{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU";
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
	}
	return "Unknown";
}

static void PrintPhysicalDeviceInfo(VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	std::cout << "\n--- Selected Physical Device ---\n";
	std::cout << "Name: " << props.deviceName << std::endl;
	std::cout << "Type: " << PhysicalDeviceTypeToString(props.deviceType) << std::endl;
	std::cout << "--------------------------------\n\n";
}

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice device)
{
	// Check for example: https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
	//VkPhysicalDeviceProperties props;
	//VkPhysicalDeviceFeatures features;
	//
	//vkGetPhysicalDeviceProperties(device, &props);
	//vkGetPhysicalDeviceFeatures(device, &features);
	
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(device, &features);

	s_Data.QueueFamily = FindQueueFamilies(device);
	bool bSupportsExtensions = DoesDeviceSupportExtensions(device);
	bool bFiniteSwapChain = false;

	if (bSupportsExtensions)
	{
		s_Data.SwapChainDetails = QuerySwapChainSupport(device);
		bFiniteSwapChain = !s_Data.SwapChainDetails.Formats.empty() && !s_Data.SwapChainDetails.PresentModes.empty();
	}

	return s_Data.QueueFamily.IsComplete() && bSupportsExtensions && bFiniteSwapChain && features.samplerAnisotropy;
}

static void SelectPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(s_Data.Instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		std::cerr << "Failed to find GPUs with Vulkan Support!\n";
		exit(-1);
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(s_Data.Instance, &deviceCount, physicalDevices.data());

	for (const auto& device : physicalDevices)
		if (IsPhysicalDeviceSuitable(device))
		{
			s_Data.PhysicalDevice = device;
			break;
		}

	if (s_Data.PhysicalDevice == VK_NULL_HANDLE)
	{
		std::cerr << "Failed to find suitable GPU!\n";
		exit(-1);
	}
	PrintPhysicalDeviceInfo(s_Data.PhysicalDevice);
}

static void CreateLogicalDevice()
{
	const float queuePriority = 1.f;
	const std::set<uint32_t> queueFamilies = { s_Data.QueueFamily.GraphicsFamily.value(), s_Data.QueueFamily.PresentFamily.value() };
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	for (uint32_t queueFamily : queueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledLayerCount = 0;
	createInfo.enabledExtensionCount = (uint32_t)s_DeviceExtensions.size();
	createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

	if (vkCreateDevice(s_Data.PhysicalDevice, &createInfo, nullptr, &s_Data.Device) != VK_SUCCESS)
	{
		std::cerr << "Failed to create logical device!\n";
		exit(-1);
	}

	vkGetDeviceQueue(s_Data.Device, s_Data.QueueFamily.GraphicsFamily.value(), 0, &s_Data.GraphicsQueue);
	vkGetDeviceQueue(s_Data.Device, s_Data.QueueFamily.PresentFamily.value(), 0, &s_Data.PresentQueue);
}

static void CreateSurface()
{
	/*
	-------glfwCreateWindowSurface does this in the background--------

	VkWin32SurfaceCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = Application::GetApp().GetWindowHandle();
	createInfo.hinstance = GetModuleHandle(nullptr);

	if (vkCreateWin32SurfaceKHR(s_Data.Instance, &createInfo, nullptr, &s_Data.Surface) != VK_SUCCESS)
	{
		std::cerr << "Failed to create surface\n";
		exit(-1);
	}
	*/

	if (glfwCreateWindowSurface(s_Data.Instance, Application::GetApp().GetWindow().GetNativeWindow(), nullptr, &s_Data.Surface) != VK_SUCCESS)
	{
		std::cerr << "Failed to create window surface\n";
		exit(-1);
	}
}

static void CreateImageViews()
{
	size_t imageCount = s_Data.SwapChainImages.size();
	s_Data.SwapChainImageViews.resize(imageCount);

	for (size_t i = 0; i < imageCount; ++i)
		s_Data.SwapChainImageViews[i] = CreateImageView(s_Data.SwapChainImages[i], s_Data.SwapChainImageFormat);
}

static std::vector<char> ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	
	if (!file)
	{
		std::cerr << "Failed to open file '" << filename << "'\n";
		exit(-1);
	}

	size_t fileSize = file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

static VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = (const uint32_t*)code.data();

	VkShaderModule shader;
	if (vkCreateShaderModule(s_Data.Device, &createInfo, nullptr, &shader) != VK_SUCCESS)
	{
		std::cerr << "Failed to create shader module!\n";
		exit(-1);
	}

	return shader;
}

static void CreateGraphicsPipeline()
{
	auto vertCode = ReadFile("Shaders/vert.spv");
	auto fragCode = ReadFile("Shaders/frag.spv");

	VkShaderModule vertShader = CreateShaderModule(vertCode);
	VkShaderModule fragShader = CreateShaderModule(fragCode);
	VkVertexInputBindingDescription bindingDesc = Vertex::GetBindingDescription();
	std::array attribDescs = Vertex::GetAttributeDesctiption();

	VkPipelineShaderStageCreateInfo vertCreateInfo{};
	vertCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertCreateInfo.module = vertShader;
	vertCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragCreateInfo{};
	fragCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragCreateInfo.module = fragShader;
	fragCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertCreateInfo, fragCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)attribDescs.size();
	vertexInputCreateInfo.pVertexAttributeDescriptions = attribDescs.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = (float)s_Data.SwapChainExtent.width;
	viewport.height = (float)s_Data.SwapChainExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = s_Data.SwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportCreateInfo{};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.pScissors = &scissor;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.viewportCount = 1;


	// @If depthClampEnable is set to VK_TRUE, then fragments that are beyond the nearand
	// 	 far planes are clamped to them as opposed to discarding them.This is useful in
	// 	 some special cases like shadow maps.Using this requires enabling a GPU feature.
	// @If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through
	//	 the rasterizer stage.This basically disables any output to the framebuffer.
	// @The lineWidth member is straightforward, it describes the thickness of lines in 
	//	 terms of number of fragments.The maximum line width that is supported depends on 
	//	 the hardwareand any line thicker than 1.0f requires you to enable the wideLines GPU feature
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //Using any mode other than fill requires enabling a GPU feature.
	rasterizer.lineWidth = 1.f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	//The rasterizer can alter the depth values by adding a constant 
	//value or biasing them based on a fragment's slope. 
	//This is sometimes used for shadow mapping,
	rasterizer.depthBiasConstantFactor = 0.f;
	rasterizer.depthBiasClamp = 0.f;
	rasterizer.depthBiasSlopeFactor = 0.f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor= VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	//If you want to use the second method of blending(bitwise combination),
	//then you should set logicOpEnable to VK_TRUE.The bitwise operation can
	//then be specified in the logicOp field.Note that this will automatically
	//disable the first method, as if you had set blendEnable to VK_FALSE for
	//every attached framebuffer!The colorWriteMask will also be used in this
	//mode to determine which channels in the framebuffer will actually be affected.
	//It is also possible to disable both modes, as we've done here, in which
	//case the fragment colors will be written to the framebuffer unmodified.
	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
	colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
	colorBlendCreateInfo.blendConstants[0] = 0.f;
	colorBlendCreateInfo.blendConstants[1] = 0.f;
	colorBlendCreateInfo.blendConstants[2] = 0.f;
	colorBlendCreateInfo.blendConstants[3] = 0.f;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &s_Data.DescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(s_Data.Device, &pipelineLayoutCreateInfo, nullptr, &s_Data.PipelineLayout) != VK_SUCCESS)
	{
		std::cerr << "Failed to create pipeline layout!\n";
		exit(-1);
	}

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.stageCount = 2;
	createInfo.pStages = shaderStages;
	createInfo.pVertexInputState = &vertexInputCreateInfo;
	createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	createInfo.pViewportState = &viewportCreateInfo;
	createInfo.pRasterizationState = &rasterizer;
	createInfo.pMultisampleState = &multisampling;
	createInfo.pDepthStencilState = nullptr;
	createInfo.pColorBlendState = &colorBlendCreateInfo;
	createInfo.pDynamicState = nullptr;
	createInfo.layout = s_Data.PipelineLayout;
	createInfo.renderPass = s_Data.RenderPass;
	createInfo.subpass = 0;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(s_Data.Device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &s_Data.Pipeline) != VK_SUCCESS)
	{
		std::cerr << "Failed to create graphics pipeline!\n";
		exit(-1);
	}

	vkDestroyShaderModule(s_Data.Device, vertShader, nullptr);
	vkDestroyShaderModule(s_Data.Device, fragShader, nullptr);
}

static void CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = s_Data.SwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(s_Data.Device, &createInfo, nullptr, &s_Data.RenderPass) != VK_SUCCESS)
	{
		std::cerr << "Failed to create render pass!\n";
		exit(-1);
	}
}

static void CreateFramebuffers()
{
	const size_t imageViewsCount = s_Data.SwapChainImageViews.size();
	s_Data.SwapChainFramebuffers.resize(imageViewsCount);

	for (size_t i = 0; i < imageViewsCount; ++i)
	{
		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = s_Data.RenderPass;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &s_Data.SwapChainImageViews[i];
		createInfo.width = s_Data.SwapChainExtent.width;
		createInfo.height = s_Data.SwapChainExtent.height;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(s_Data.Device, &createInfo, nullptr, &s_Data.SwapChainFramebuffers[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to create framebuffer!\n";
			exit(-1);
		}
	}
}

static void CreateCommandPool()
{
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = s_Data.QueueFamily.GraphicsFamily.value();
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	if (vkCreateCommandPool(s_Data.Device, &createInfo, nullptr, &s_Data.CommandPool) != VK_SUCCESS)
	{
		std::cerr << "Failed to create command pool!\n";
		exit(-1);
	}
}

static void PrepareCommandBuffers()
{
	size_t buffersCount = s_Data.SwapChainFramebuffers.size();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	VkClearValue clearColor = { 0.f, 0.f, 0.f, 1.f };
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = s_Data.RenderPass;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = s_Data.SwapChainExtent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	VkBuffer vertexBuffers[] = { s_Data.VertexBuffer };
	VkDeviceSize offsets[] = { 0 };

	//Read the end of: https://vulkan-tutorial.com/Vertex_buffers/Index_buffer

	for (size_t i = 0; i < buffersCount; ++i)
	{
		if (vkBeginCommandBuffer(s_Data.CommandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			std::cerr << "Failed to begin command buffer!\n";
			exit(-1);
		}

		renderPassInfo.framebuffer = s_Data.SwapChainFramebuffers[i];

		vkCmdBeginRenderPass(s_Data.CommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(s_Data.CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data.Pipeline);
		vkCmdBindVertexBuffers(s_Data.CommandBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(s_Data.CommandBuffers[i], s_Data.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(s_Data.CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data.PipelineLayout, 0, 1, s_Data.DescriptorSets.data(), 0, nullptr);
		vkCmdDrawIndexed(s_Data.CommandBuffers[i], (uint32_t)indices.size(), 1, 0, 0, 0);
		vkCmdEndRenderPass(s_Data.CommandBuffers[i]);

		if (vkEndCommandBuffer(s_Data.CommandBuffers[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to end command buffer!\n";
			exit(-1);
		}
	}
}

static void CreateCommandBuffers()
{
	size_t buffersCount = s_Data.SwapChainFramebuffers.size();
	s_Data.CommandBuffers.resize(buffersCount);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = s_Data.CommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)buffersCount;

	if (vkAllocateCommandBuffers(s_Data.Device, &allocInfo, s_Data.CommandBuffers.data()) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate command buffers!\n";
		exit(-1);
	}

	PrepareCommandBuffers();
}

static void CreateSyncObjects()
{
	s_Data.ImageAvailableSemaphores.resize(s_MaxFramesInFlight);
	s_Data.RenderFinishedSemaphores.resize(s_MaxFramesInFlight);
	s_Data.InFlightFences.resize(s_MaxFramesInFlight);
	s_Data.InFlightImages.resize(s_Data.SwapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < s_MaxFramesInFlight; ++i)
		if (vkCreateSemaphore(s_Data.Device, &semaphoreInfo, nullptr, &s_Data.ImageAvailableSemaphores[i]) != VK_SUCCESS
			|| vkCreateSemaphore(s_Data.Device, &semaphoreInfo, nullptr, &s_Data.RenderFinishedSemaphores[i]) != VK_SUCCESS
			|| vkCreateFence(s_Data.Device, &fenceInfo, nullptr, &s_Data.InFlightFences[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to sync objects!\n";
			exit(-1);
		}
}

static void CleanupSwapChain()
{
	vkDeviceWaitIdle(s_Data.Device);

	for (auto& framebuffer : s_Data.SwapChainFramebuffers)
		vkDestroyFramebuffer(s_Data.Device, framebuffer, nullptr);

	for (auto& imageView : s_Data.SwapChainImageViews)
		vkDestroyImageView(s_Data.Device, imageView, nullptr);

	for (auto& ub : s_Data.UniformBuffers)
		vkDestroyBuffer(s_Data.Device, ub, nullptr);

	for (auto& ubm : s_Data.UniformBuffersMemory)
		vkFreeMemory(s_Data.Device, ubm, nullptr);

	vkDestroyDescriptorPool(s_Data.Device, s_Data.DescriptorPool, nullptr);

	vkFreeCommandBuffers(s_Data.Device, s_Data.CommandPool, (uint32_t)s_Data.CommandBuffers.size(), s_Data.CommandBuffers.data());

	vkDestroyPipeline(s_Data.Device, s_Data.Pipeline, nullptr);
	vkDestroyPipelineLayout(s_Data.Device, s_Data.PipelineLayout, nullptr);
	vkDestroyRenderPass(s_Data.Device, s_Data.RenderPass, nullptr);
	vkDestroySwapchainKHR(s_Data.Device, s_Data.SwapChain, nullptr);
}

static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// TODO: Different command pool for these kind of operations. See: https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
	VkCommandBuffer cmdBuffer = GetSingleTimeCmdBuffer(true);

	VkBufferCopy copyRegion{};
	copyRegion.dstOffset = 0;
	copyRegion.srcOffset = 0;
	copyRegion.size = size;

	vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	FlushCommandBuffer(cmdBuffer, s_Data.GraphicsQueue);
	// TODO: Use fences to wait. 
	// A fence would allow you to schedule multiple transfers simultaneously and
	// wait for all of them complete, instead of executing one at a time.
	// That may give the driver more opportunities to optimize.
}

static void UpdateVertexBuffer()
{
	uint64_t size = sizeof(Vertex) * vertices.size();
	float newColor = (float)pow(sin(glfwGetTime()), 2);
	vertices[0].Color.r = newColor;
	vertices[1].Color.g = newColor;
	vertices[2].Color.b = newColor;

	void* data = nullptr;
	vkMapMemory(s_Data.Device, s_Data.VBStagingBufferMemory, 0, size, 0, &data);
	memcpy(data, vertices.data(), size);
	vkUnmapMemory(s_Data.Device, s_Data.VBStagingBufferMemory);

	CopyBuffer(s_Data.VBStagingBuffer, s_Data.VertexBuffer, size);
}

static void UpdateUniformBuffer(uint32_t index)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	
	float passedTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo;
	ubo.Model = glm::rotate(glm::mat4(1.f), passedTime * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.View = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.Proj = glm::perspective(glm::radians(45.f), (float)s_Data.SwapChainExtent.width / s_Data.SwapChainExtent.height, 0.1f, 10.f);
	ubo.Proj[1][1] *= -1.f;

	void* data;
	vkMapMemory(s_Data.Device, s_Data.UniformBuffersMemory[index], 0, sizeof(UniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(s_Data.Device, s_Data.UniformBuffersMemory[index]);
}

static void RecreateSwapChain();

void Renderer::DrawFrame()
{
	currentFrame = (currentFrame + 1) % s_MaxFramesInFlight;
	UpdateVertexBuffer();
	vkWaitForFences(s_Data.Device, 1, &s_Data.InFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(s_Data.Device, s_Data.SwapChain, UINT64_MAX, s_Data.ImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		std::cerr << "Failed to acquire swap chain image!\n";
		exit(-1);
	}

	if (s_Data.InFlightImages[imageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(s_Data.Device, 1, &s_Data.InFlightImages[imageIndex], VK_TRUE, UINT64_MAX);

	s_Data.InFlightImages[imageIndex] = s_Data.InFlightFences[currentFrame];

	VkSemaphore waitSemaphores[] = { s_Data.ImageAvailableSemaphores[currentFrame] };
	VkSemaphore signalSemaphores[] = { s_Data.RenderFinishedSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	static bool b = false;
	if (!b)
	{
		UpdateUniformBuffer(imageIndex);
		b = true;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &s_Data.CommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(s_Data.Device, 1, &s_Data.InFlightFences[currentFrame]);

	if (vkQueueSubmit(s_Data.GraphicsQueue, 1, &submitInfo, s_Data.InFlightFences[currentFrame]) != VK_SUCCESS)
	{
		std::cerr << "Failed to submit queue!\n";
		exit(-1);
	}

	VkSwapchainKHR swapChains[] = { s_Data.SwapChain };
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(s_Data.PresentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		RecreateSwapChain();
	else if (result != VK_SUCCESS)
	{
		std::cerr << "Failed to acquire swap chain image!\n";
		exit(-1);
	}
}

void Renderer::BeginImGui()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Renderer::EndImGui()
{
	vkDeviceWaitIdle(s_Data.Device);
	ImGui::Render();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	uint32_t width = s_Data.SwapChainExtent.width;
	uint32_t height = s_Data.SwapChainExtent.height;

	uint32_t commandBufferIndex = currentFrame;

	VkCommandBufferBeginInfo drawCmdBufInfo = {};
	drawCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	drawCmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	drawCmdBufInfo.pNext = nullptr;

	VkCommandBuffer drawCommandBuffer = s_Data.CommandBuffers[commandBufferIndex];
	vkBeginCommandBuffer(drawCommandBuffer, &drawCmdBufInfo);

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = s_Data.RenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2; // Color + depth
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.framebuffer = s_Data.SwapChainFramebuffers[commandBufferIndex];

	vkCmdBeginRenderPass(drawCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = s_Data.RenderPass;
	inheritanceInfo.framebuffer = s_Data.SwapChainFramebuffers[commandBufferIndex];

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	cmdBufInfo.pInheritanceInfo = &inheritanceInfo;

	vkBeginCommandBuffer(s_ImGuiCommandBuffers[commandBufferIndex], &cmdBufInfo);

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(s_ImGuiCommandBuffers[commandBufferIndex], 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(s_ImGuiCommandBuffers[commandBufferIndex], 0, 1, &scissor);

	ImDrawData* main_draw_data = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(main_draw_data, s_ImGuiCommandBuffers[commandBufferIndex]);

	vkEndCommandBuffer(s_ImGuiCommandBuffers[commandBufferIndex]);

	std::vector<VkCommandBuffer> commandBuffers;
	commandBuffers.push_back(s_ImGuiCommandBuffers[commandBufferIndex]);

	vkCmdExecuteCommands(drawCommandBuffer, uint32_t(commandBuffers.size()), commandBuffers.data());

	vkCmdEndRenderPass(drawCommandBuffer);

	vkEndCommandBuffer(drawCommandBuffer);

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

static void InitImGui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoDecoration = false;
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	//io.Fonts->AddFontFromFileTTF("Resources/Fonts/opensans/OpenSans-Bold.ttf", 18.0f);
	//io.Fonts->AddFontFromFileTTF("Resources/Fonts/opensans/OpenSans-Regular.ttf", 24.0f);
	//io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/opensans/OpenSans-Regular.ttf", 18.0f);

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, style.Colors[ImGuiCol_WindowBg].w);

	Application& app = Application::GetApp();
	GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

	auto device = s_Data.Device;

	// Create Descriptor Pool
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 100 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &s_ImGui_DescriptorPool);

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = s_Data.Instance;
	init_info.PhysicalDevice = s_Data.PhysicalDevice;
	init_info.Device = device;
	init_info.QueueFamily = s_Data.QueueFamily.GraphicsFamily.value();
	init_info.Queue = s_Data.GraphicsQueue;
	init_info.PipelineCache = nullptr;
	init_info.DescriptorPool = s_ImGui_DescriptorPool;
	init_info.Allocator = nullptr;
	init_info.MinImageCount = 2;
	init_info.ImageCount = (uint32_t)s_Data.SwapChainImages.size();
	ImGui_ImplVulkan_Init(&init_info, s_Data.RenderPass);

	// Upload Fonts
	{
		// Use any command queue

		VkCommandBuffer commandBuffer = GetCommandBuffer(true);
		ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		FlushCommandBuffer(commandBuffer, s_Data.GraphicsQueue);

		vkDeviceWaitIdle(device);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	s_ImGuiCommandBuffers.resize(s_MaxFramesInFlight);
	for (uint32_t i = 0; i < s_MaxFramesInFlight; i++)
		s_ImGuiCommandBuffers[i] = CreateSecondaryCommandBuffer();
}

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propFlags)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(s_Data.PhysicalDevice, &memProps);

	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & propFlags) == propFlags)
			return i;
	}

	std::cerr << "Failed to find required memory type!\n";
	exit(-1);
	return 0;
}

static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propFlags, VkBuffer& outBuffer, VkDeviceMemory& outBufferMemory)
{
	VkBufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(s_Data.Device, &createInfo, nullptr, &outBuffer) != VK_SUCCESS)
	{
		std::cerr << "Failed to create vertex buffer!\n";
		exit(-1);
	}

	VkMemoryRequirements memRequirements{};
	vkGetBufferMemoryRequirements(s_Data.Device, outBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, propFlags);

	// It should be noted that in a real world application, you're not supposed to actually call vkAllocateMemory for every individual buffer
	// Use this lib https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
	if (vkAllocateMemory(s_Data.Device, &allocInfo, nullptr, &outBufferMemory) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate memory\n";
		exit(-1);
	}

	vkBindBufferMemory(s_Data.Device, outBuffer, outBufferMemory, 0);
}

static void CreateVertexBuffer()
{
	VkDeviceSize size = sizeof(Vertex) * vertices.size();

	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, s_Data.VBStagingBuffer, s_Data.VBStagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(s_Data.Device, s_Data.VBStagingBufferMemory, 0, size, 0, &data);
	memcpy(data, vertices.data(), size);
	vkUnmapMemory(s_Data.Device, s_Data.VBStagingBufferMemory);

	CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.VertexBuffer, s_Data.VertexBufferMemory);
	CopyBuffer(s_Data.VBStagingBuffer, s_Data.VertexBuffer, size);
}

static void CreateIndexBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(s_Data.Device, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, indices.data(), size);
	vkUnmapMemory(s_Data.Device, stagingBufferMemory);

	CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.IndexBuffer, s_Data.IndexBufferMemory);
	CopyBuffer(stagingBuffer, s_Data.IndexBuffer, size);

	vkDestroyBuffer(s_Data.Device, stagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, stagingBufferMemory, nullptr);
}

static void CreateUniformBuffers()
{
	VkDeviceSize size = sizeof(UniformBufferObject);
	const size_t imageCount = s_Data.SwapChainImages.size();

	s_Data.UniformBuffers.resize(imageCount);
	s_Data.UniformBuffersMemory.resize(imageCount);
	
	for (size_t i = 0; i < imageCount; ++i)
	{
		CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			s_Data.UniformBuffers[i], s_Data.UniformBuffersMemory[i]);
	}
}

static void CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboBinding{};
	uboBinding.binding = 0;
	uboBinding.descriptorCount = 1;
	uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array bindings = { uboBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = (uint32_t)bindings.size();
	createInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(s_Data.Device, &createInfo, nullptr, &s_Data.DescriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Failed to create descriptor set layout!\n";
		exit(-1);
	}
}

static void CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = (uint32_t)s_Data.SwapChainImages.size();
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = (uint32_t)s_Data.SwapChainImages.size();

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = (uint32_t)poolSizes.size();
	createInfo.pPoolSizes = poolSizes.data();
	createInfo.maxSets = (uint32_t)s_Data.SwapChainImages.size();

	if (vkCreateDescriptorPool(s_Data.Device, &createInfo, nullptr, &s_Data.DescriptorPool) != VK_SUCCESS)
	{
		std::cerr << "Failed to create descriptor pool!\n";
		exit(-1);
	}
}

static void CreateDescriptorSets()
{
	// Read the end of: https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets

	const size_t size = s_Data.SwapChainImages.size();
	std::vector<VkDescriptorSetLayout> layouts(size, s_Data.DescriptorSetLayout);
	VkDescriptorSetAllocateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	createInfo.descriptorPool = s_Data.DescriptorPool;
	createInfo.descriptorSetCount = (uint32_t)size;
	createInfo.pSetLayouts = layouts.data();

	s_Data.DescriptorSets.resize(size);
	if (vkAllocateDescriptorSets(s_Data.Device, &createInfo, s_Data.DescriptorSets.data()) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate descriptor sets!\n";
		exit(-1);
	}

	for (size_t i = 0; i < size; ++i)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = s_Data.UniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = s_Data.TextureImageView;
		imageInfo.sampler = s_Data.TextureSampler;

		std::array<VkWriteDescriptorSet, 2> descWrites{};
		descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrites[0].dstSet = s_Data.DescriptorSets[i];
		descWrites[0].dstBinding = 0;
		descWrites[0].dstArrayElement = 0;
		descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descWrites[0].descriptorCount = 1;
		descWrites[0].pBufferInfo = &bufferInfo;

		descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrites[1].dstSet = s_Data.DescriptorSets[i];
		descWrites[1].dstBinding = 1;
		descWrites[1].dstArrayElement = 0;
		descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descWrites[1].descriptorCount = 1;
		descWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(s_Data.Device, (uint32_t)descWrites.size(), descWrites.data(), 0, nullptr);
	}
}

static void RecreateSwapChain()
{
	vkDeviceWaitIdle(s_Data.Device);
	CleanupSwapChain();

	CreateSwapChain(); //if it fails, check: https://vulkan-tutorial.com/FAQ
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline(); //To add SPIR-V library see: https://github.com/google/shaderc
	CreateFramebuffers();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
	PrepareCommandBuffers();
}

void Renderer::OnWindowResized()
{
	RecreateSwapChain();
}

static void ChangeImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkPipelineStageFlags srcStage = 0, dstStage = 0;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		std::cerr << "Unknown layouts!\n";
		exit(-1);
	}

	VkCommandBuffer cmdBuffer = GetSingleTimeCmdBuffer(true);
	vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	FlushCommandBuffer(cmdBuffer, s_Data.GraphicsQueue);
}

static void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	VkCommandBuffer cmd = GetSingleTimeCmdBuffer(true);
	vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	FlushCommandBuffer(cmd, s_Data.GraphicsQueue);
}

static void CreateTexture(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags flags,
						  VkMemoryPropertyFlags memPropFlags, VkImage& outImage, VkDeviceMemory& outImageMemory)
{
	VkImageCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.extent.width = width;
	createInfo.extent.height = height;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.format = format;
	createInfo.tiling = tiling;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage = flags;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.flags = 0;

	if (vkCreateImage(s_Data.Device, &createInfo, nullptr, &outImage) != VK_SUCCESS)
	{
		std::cerr << "Failed to create image!\n";
		exit(-1);
	}

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(s_Data.Device, outImage, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, memPropFlags);

	if (vkAllocateMemory(s_Data.Device, &allocInfo, nullptr, &outImageMemory) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate image memory!\n";
		exit(-1);
	}

	vkBindImageMemory(s_Data.Device, outImage, outImageMemory, 0);
}

static void CreateTextureImage()
{
	int width, height, channels;
	stbi_uc* pixels = stbi_load("Textures/pika.png", &width, &height, &channels, STBI_rgb_alpha);

	VkDeviceSize size = width * height * channels;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(s_Data.Device, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, pixels, size);
	vkUnmapMemory(s_Data.Device, stagingBufferMemory);

	stbi_image_free(pixels);

	CreateTexture(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.TextureImage, s_Data.TextureImageMemory);

	ChangeImageLayout(s_Data.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer, s_Data.TextureImage, (uint32_t)width, (uint32_t)height);
	ChangeImageLayout(s_Data.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(s_Data.Device, stagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, stagingBufferMemory, nullptr);
}

static void CreateTextureImageView()
{
	s_Data.TextureImageView = CreateImageView(s_Data.TextureImage, VK_FORMAT_R8G8B8A8_SRGB);
}

static void CreateTextureSampler()
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(s_Data.PhysicalDevice, &props);

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.f;
	createInfo.minLod = 0.f;
	createInfo.maxLod = 0.f;

	if (vkCreateSampler(s_Data.Device, &createInfo, nullptr, &s_Data.TextureSampler) != VK_SUCCESS)
	{
		std::cerr << "Failed to create sampler!\n";
		exit(-1);
	}
}

void Renderer::Init()
{
	#ifdef NDEBUG
		s_EnableValidationLayers = false;
	#else
		s_EnableValidationLayers = true;
	#endif

	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	SelectPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain(); //if it fails, check: https://vulkan-tutorial.com/FAQ
	CreateImageViews();
	CreateRenderPass();
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline(); //To add SPIR-V library see: https://github.com/google/shaderc
	CreateFramebuffers();
	CreateCommandPool();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
	CreateSyncObjects();

	InitImGui();
}

void Renderer::Shutdown()
{
	vkDeviceWaitIdle(s_Data.Device);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(s_Data.Device, s_ImGui_DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(s_Data.Device, s_Data.DescriptorSetLayout, nullptr);	

	CleanupSwapChain();
	vkDestroyBuffer(s_Data.Device, s_Data.VertexBuffer, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.VertexBufferMemory, nullptr);
	vkDestroyBuffer(s_Data.Device, s_Data.IndexBuffer, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.IndexBufferMemory, nullptr);

	vkDestroyBuffer(s_Data.Device, s_Data.VBStagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.VBStagingBufferMemory, nullptr);

	vkDestroySampler(s_Data.Device, s_Data.TextureSampler, nullptr);
	vkDestroyImageView(s_Data.Device, s_Data.TextureImageView, nullptr);
	vkDestroyImage(s_Data.Device, s_Data.TextureImage, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.TextureImageMemory, nullptr);

	for (uint32_t i = 0; i < s_MaxFramesInFlight; ++i)
	{
		vkDestroySemaphore(s_Data.Device, s_Data.ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(s_Data.Device, s_Data.RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(s_Data.Device, s_Data.InFlightFences[i], nullptr);
	}
	
	vkDestroyCommandPool(s_Data.Device, s_Data.CommandPool, nullptr);
	vkDestroySurfaceKHR(s_Data.Instance, s_Data.Surface, nullptr);
	vkDestroyDevice(s_Data.Device, nullptr);

	if (s_EnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(s_Data.Instance, s_Data.DebugMessenger, nullptr);

	vkDestroyInstance(s_Data.Instance, nullptr);
}
