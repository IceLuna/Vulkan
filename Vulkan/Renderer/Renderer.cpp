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
#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui_internal.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../tiny_obj_loader.h"

#define mEXIT(message) { std::cerr << message << std::endl; exit(-1); }

const std::vector<const char*> s_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const std::vector<const char*> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsQueue;
	std::optional<uint32_t> PresentQueue;

	bool IsComplete() const
	{
		return GraphicsQueue.has_value() && PresentQueue.has_value();
	}
};

struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities;
	std::vector<VkSurfaceFormatKHR> Formats;
	std::vector<VkPresentModeKHR> PresentModes;

	bool IsSupported() const
	{
		return Formats.size() && PresentModes.size();
	}
};

struct VulkanData
{
	VkInstance Instance;
	VkDebugUtilsMessengerEXT DebugMessanger;
	VkPhysicalDevice PhysicalDevice;
	QueueFamilyIndices FamilyIndices;
	VkDevice Device;
	VkQueue GraphicsQueue;
	VkQueue PresentQueue;
	VkSurfaceKHR Surface;
	SwapchainSupportDetails SwapchainDetails;
	VkSwapchainKHR Swapchain;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	VkExtent2D SwapchainExtent;
	VkFormat SwapchainImageFormat;
	VkPipelineLayout PipelineLayout{};
	VkRenderPass RenderPass;
	VkPipeline GraphicsPipeline;
	std::vector<VkFramebuffer> SwapchainFramebuffers;
	VkCommandPool CommandPool;
	std::vector<VkCommandBuffer> CommandBuffers;
	std::vector<VkSemaphore> ImageAvailableSemaphores;
	std::vector<VkSemaphore> RenderFinishedSemaphores;
	std::vector<VkFence> InFlightFences;
	std::vector<VkFence> ImagesInFlightFences;
	VkBuffer VertexBuffer;
	VkDeviceMemory VertexBufferMemory;
	VkBuffer IndexBuffer;
	VkDeviceMemory IndexBufferMemory;
	VkDescriptorSetLayout DescriptorSetLayout;
	std::vector<VkBuffer> UniformBuffers;
	std::vector<VkDeviceMemory> UniformBuffersMemory;
	VkDescriptorPool DescriptorPool;
	std::vector<VkDescriptorSet> DescriptorSets;
	VkImage TextureImage;
	VkDeviceMemory TextureImageMemory;
	VkImageView TextureImageView;
	VkSampler TextureSampler;
	VkImage DepthImage;
	VkDeviceMemory DepthImageMemory;
	VkImageView DepthImageView;
};

static const char* s_ModelPath = "Models/viking_room.obj";
static const char* s_TexturePath = "Textures/viking_room.png";

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Color;
	glm::vec2 TexCoords;

	bool operator==(const Vertex& other) const {
		return Position == other.Position && Color == other.Color && TexCoords == other.TexCoords;
	}

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription desc{};
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		desc.binding = 0;
		desc.stride = sizeof(Vertex);

		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttribDescription()
	{
		VkVertexInputAttributeDescription desc1{};
		desc1.binding = 0;
		desc1.location = 0;
		desc1.format = VK_FORMAT_R32G32B32_SFLOAT;
		desc1.offset = offsetof(Vertex, Position);

		VkVertexInputAttributeDescription desc2{};
		desc2.binding = 0;
		desc2.location = 1;
		desc2.format = VK_FORMAT_R32G32B32_SFLOAT;
		desc2.offset = offsetof(Vertex, Color);

		VkVertexInputAttributeDescription desc3{};
		desc3.binding = 0;
		desc3.location = 2;
		desc3.format = VK_FORMAT_R32G32_SFLOAT;
		desc3.offset = offsetof(Vertex, TexCoords);

		return { desc1, desc2, desc3 };
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.Position) ^
				(hash<glm::vec3>()(vertex.Color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.TexCoords) << 1);
		}
	};
}

static std::vector<Vertex> s_Vertices;
static std::vector<uint32_t> s_Indices;

struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

static VulkanData s_Data{};
static bool s_EnableValidationLayers = false;
const int MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t s_CurrentFrame = 0;

static std::vector<char> ReadFile(const std::filesystem::path& path)
{
	std::vector<char> data;
	std::ifstream fin(path, std::ios::binary | std::ios::ate);
	if (fin)
	{
		size_t size = fin.tellg();
		data.resize(size);
		fin.seekg(0);
		fin.read(data.data(), size);
		fin.close();
	}
	else
		std::cerr << "Failed to read file: " << path << std::endl;

	return data;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << pCallbackData->pMessage << std::endl;

	return VK_FALSE; //To not abort caller
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func)
		return func(instance, createInfo, pAllocator, pDebugMessenger);
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func)
		func(instance, debugMessenger, pAllocator);
}

static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& outCreateInfo)
{
	outCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	outCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	outCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	outCreateInfo.pfnUserCallback = DebugCallback;
	outCreateInfo.pUserData = nullptr;
	outCreateInfo.pNext = nullptr;
	outCreateInfo.flags = 0;
}

static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (auto format : candidates)
	{
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(s_Data.PhysicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			return format;
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			return format;
	}

	mEXIT("Failed to find format support!");
	
	return VK_FORMAT_R8G8B8A8_SINT;
}


static VkFormat FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static bool HasStancilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static std::vector<const char*> GetRequiredExtensions()
{
	uint32_t count = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + count);

	if (s_EnableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

static bool AreValidationLayersSupported()
{
	uint32_t count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> props(count);
	vkEnumerateInstanceLayerProperties(&count, props.data());
	std::set<std::string> validationLayers(s_ValidationLayers.begin(), s_ValidationLayers.end());

	for (auto& prop : props)
	{
		for (auto& layer : validationLayers)
			if (strcmp(prop.layerName, layer.c_str()) == 0)
			{
				validationLayers.erase(layer);
				break;
			}
	}

	if (validationLayers.size())
	{
		std::cerr << "Validation layers are not supported!\n";
		for (auto& layer : validationLayers)
			std::cerr << "\t" << layer << std::endl;
	}

	return validationLayers.size() == 0;
}

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties memProps{};
	vkGetPhysicalDeviceMemoryProperties(s_Data.PhysicalDevice, &memProps);

	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
		if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
			return i;

	mEXIT("Failed to find required memory type!");
}

static void CreateImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
	VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.extent = { (uint32_t)w, (uint32_t)h, 1 };
	createInfo.format = format;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.arrayLayers = 1;
	createInfo.mipLevels = 1;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.tiling = tiling;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage = usage;

	if (vkCreateImage(s_Data.Device, &createInfo, nullptr, &image) != VK_SUCCESS)
		mEXIT("Failed to create texture image!");

	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(s_Data.Device, image, &memReq);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, props);

	if (vkAllocateMemory(s_Data.Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
		mEXIT("Failed to allocate memory for a texture!");

	vkBindImageMemory(s_Data.Device, image, imageMemory, 0);
}

static VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.format = format;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.layerCount = 1;
	createInfo.subresourceRange.levelCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(s_Data.Device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
		mEXIT("Failed to create image view!");

	return imageView;
}

static VkCommandBuffer CreateSingleTimeCommandBuffer()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = s_Data.CommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;

	if (vkAllocateCommandBuffers(s_Data.Device, &allocInfo, &cmdBuffer) != VK_SUCCESS)
		mEXIT("Failed to allocate a single time command buffer!");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	return cmdBuffer;
}

static void EndCommandBuffer(VkCommandBuffer cmdBuffer)
{
	vkEndCommandBuffer(cmdBuffer);
	
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	
	vkQueueSubmit(s_Data.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(s_Data.GraphicsQueue);

	vkFreeCommandBuffers(s_Data.Device, s_Data.CommandPool, 1, &cmdBuffer);
}

static void CreateInstance()
{
	std::vector extensions = GetRequiredExtensions();

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pApplicationName = "Learning";
	appInfo.pEngineName = "Learning";

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

	if (s_EnableValidationLayers && AreValidationLayersSupported())
	{
		PopulateDebugMessengerCreateInfo(debugCreateInfo);

		createInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
		createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
		createInfo.pNext = &debugCreateInfo;
	}

	if (vkCreateInstance(&createInfo, nullptr, &s_Data.Instance) != VK_SUCCESS)
		mEXIT("Failed to create instance!");
}

static void SetupDebugMessenger()
{
	if (!s_EnableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(s_Data.Instance, &createInfo, nullptr, &s_Data.DebugMessanger) != VK_SUCCESS)
		mEXIT("Failed to create debug utils messenger!");
}

static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(s_Data.Device, &createInfo, nullptr, &buffer) != VK_SUCCESS)
		mEXIT("Failed to create buffer!");

	VkMemoryRequirements memReq{};
	vkGetBufferMemoryRequirements(s_Data.Device, buffer, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, props);

	if (vkAllocateMemory(s_Data.Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		mEXIT("Failed to allocate buffer memory!");

	vkBindBufferMemory(s_Data.Device, buffer, bufferMemory, 0);
}

static void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
	VkBufferCopy copyRegion{};
	copyRegion.size = size;

	VkCommandBuffer cmdBuffer = CreateSingleTimeCommandBuffer();
	vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copyRegion);
	EndCommandBuffer(cmdBuffer);
}

static void CopyBufferToImage(VkBuffer src, VkImage dst, uint32_t w, uint32_t h)
{
	VkBufferImageCopy copy{};
	copy.imageExtent = { w, h, 1 };
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.mipLevel = 0;
	copy.imageSubresource.layerCount = 1;

	VkCommandBuffer cmd = CreateSingleTimeCommandBuffer();
	vkCmdCopyBufferToImage(cmd, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	EndCommandBuffer(cmd);
}

static void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
		mEXIT("Unknown image transition layouts!");

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (HasStancilComponent(format))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


	VkCommandBuffer cmd = CreateSingleTimeCommandBuffer();
	vkCmdPipelineBarrier(cmd,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
	EndCommandBuffer(cmd);
}

static QueueFamilyIndices FindFamilyIndices(VkPhysicalDevice device)
{
	QueueFamilyIndices indices{};
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, queueFamilies.data());

	VkBool32 presentSupport = false;

	uint32_t i = 0;
	for (auto& family : queueFamilies)
	{
		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.GraphicsQueue = i;
		
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, s_Data.Surface, &presentSupport);
		if (presentSupport)
			indices.PresentQueue = i;

		if (indices.IsComplete())
			break;

		++i;
	}

	return indices;
}

static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t count = 0;
	
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, availableExtensions.data());

	std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());
	for (auto& extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device)
{
	SwapchainSupportDetails details;
	uint32_t formatCount = 0, presentModesCount = 0;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Data.Surface, &details.Capabilities);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Data.Surface, &formatCount, nullptr);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Data.Surface, &presentModesCount, nullptr);

	if (formatCount)
	{
		details.Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Data.Surface, &formatCount, details.Formats.data());
	}
	if (presentModesCount)
	{
		details.PresentModes.resize(presentModesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Data.Surface, &presentModesCount, details.PresentModes.data());
	}

	return details;
}

static VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availFormats)
{
	for (auto& format : availFormats)
		if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;

	return availFormats[0];
}

static VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR> modes)
{
	for (auto& mode : modes)
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(Application::GetApp().GetWindow().GetNativeWindow(), &width, &height);
	VkExtent2D extent{ (uint32_t)width, (uint32_t)height };
	extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return extent;
}

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(device, &props);

	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceFeatures(device, &features);

	s_Data.FamilyIndices = FindFamilyIndices(device);
	s_Data.SwapchainDetails = QuerySwapchainSupport(device);
	bool bSwapChainSupported = s_Data.SwapchainDetails.IsSupported();

	return s_Data.FamilyIndices.IsComplete() && CheckDeviceExtensionSupport(device) && bSwapChainSupported && features.samplerAnisotropy;
}

static void SelectPhysicalDevice()
{
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(s_Data.Instance, &count, nullptr);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(s_Data.Instance, &count, devices.data());

	for (auto& device : devices)
	{
		if (IsPhysicalDeviceSuitable(device))
		{
			s_Data.PhysicalDevice = device;
			break;
		}
	}

	if (s_Data.PhysicalDevice == VK_NULL_HANDLE)
		mEXIT("Failed to find suitable physical device!");
}

static void CreateLogicalDevice()
{
	const float queuePriority = 1.f;

	const std::set<uint32_t> familyIndices = { s_Data.FamilyIndices.GraphicsQueue.value(), s_Data.FamilyIndices.PresentQueue.value() };
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
	queueCreateInfos.reserve(2);

	for (auto& index : familyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = index;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfo.queueCount = 1;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &features;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.enabledExtensionCount = (uint32_t)s_DeviceExtensions.size();
	createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

	if (s_EnableValidationLayers)
	{
		createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
		createInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
	}

	if (vkCreateDevice(s_Data.PhysicalDevice, &createInfo, nullptr, &s_Data.Device) != VK_SUCCESS)
		mEXIT("Failed to create logical device!");

	vkGetDeviceQueue(s_Data.Device, s_Data.FamilyIndices.GraphicsQueue.value(), 0, &s_Data.GraphicsQueue);
	vkGetDeviceQueue(s_Data.Device, s_Data.FamilyIndices.PresentQueue.value(), 0, &s_Data.PresentQueue);
}

static void CreateSurface()
{
	if (glfwCreateWindowSurface(s_Data.Instance, Application::GetApp().GetWindow().GetNativeWindow(), nullptr, &s_Data.Surface) != VK_SUCCESS)
		mEXIT("Failed to create surface");
}

static void CreateSwapchain()
{
	s_Data.SwapchainDetails = QuerySwapchainSupport(s_Data.PhysicalDevice);

	auto& details = s_Data.SwapchainDetails;
	VkSurfaceFormatKHR format = ChooseSwapchainFormat(details.Formats);
	VkPresentModeKHR mode = ChooseSwapchainPresentMode(details.PresentModes);
	VkExtent2D extent = ChooseSwapchainExtent(details.Capabilities);
	uint32_t familyIndices[] = { s_Data.FamilyIndices.GraphicsQueue.value(), s_Data.FamilyIndices.PresentQueue.value() };
	
	uint32_t imageCount = details.Capabilities.minImageCount + 1;
	if (details.Capabilities.maxImageCount > 0 && imageCount > details.Capabilities.maxImageCount)
		imageCount = details.Capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = s_Data.Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = details.Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = mode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (familyIndices[0] != familyIndices[1])
	{
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = familyIndices;
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}
	else
	{
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (vkCreateSwapchainKHR(s_Data.Device, &createInfo, nullptr, &s_Data.Swapchain) != VK_SUCCESS)
		mEXIT("Failed to create swapchain!");

	s_Data.SwapchainImageFormat = format.format;
	s_Data.SwapchainExtent = extent;
	vkGetSwapchainImagesKHR(s_Data.Device, s_Data.Swapchain, &imageCount, nullptr);
	s_Data.SwapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(s_Data.Device, s_Data.Swapchain, &imageCount, s_Data.SwapchainImages.data());
}

static void CreateImageViews()
{
	const uint32_t imageCount = (uint32_t)s_Data.SwapchainImages.size();
	s_Data.SwapchainImageViews.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; ++i)
		s_Data.SwapchainImageViews[i] = CreateImageView(s_Data.SwapchainImages[i], s_Data.SwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

static VkShaderModule CreateShaderModule(const std::vector<char> shaderBinary)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = shaderBinary.size();
	createInfo.pCode = (uint32_t*)shaderBinary.data();

	VkShaderModule shader{};
	if (vkCreateShaderModule(s_Data.Device, &createInfo, nullptr, &shader) != VK_SUCCESS)
		mEXIT("Failed to create shader module!");

	return shader;
}

static void CreateGraphicsPipeline()
{
	auto vertShaderCode = ReadFile("Shaders/vert.spv");
	auto fragShaderCode = ReadFile("Shaders/frag.spv");

	VkShaderModule vertShader = CreateShaderModule(vertShaderCode);
	VkShaderModule fragShader = CreateShaderModule(fragShaderCode);

	//pSpecializationInfo - for passing constants
	VkPipelineShaderStageCreateInfo vertCreateInfo{};
	vertCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertCreateInfo.module = vertShader;
	vertCreateInfo.pName = "main";
	vertCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	
	VkPipelineShaderStageCreateInfo fragCreateInfo{};
	fragCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragCreateInfo.module = fragShader;
	fragCreateInfo.pName = "main";
	fragCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertCreateInfo, fragCreateInfo};

	auto bindingDesc = Vertex::GetBindingDescription();
	auto attribDesc = Vertex::GetAttribDescription();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pVertexAttributeDescriptions = attribDesc.data();
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attribDesc.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputInfo.vertexBindingDescriptionCount = 1;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width  = (float)s_Data.SwapchainExtent.width;
	viewport.height = (float)s_Data.SwapchainExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = s_Data.SwapchainExtent;

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.pScissors = &scissor;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1.f;
	rasterizerInfo.depthBiasEnable = VK_FALSE;
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleCreateInfo.minSampleShading = 1.f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = &s_Data.DescriptorSetLayout;
	layoutCreateInfo.setLayoutCount = 1;
	
	if (vkCreatePipelineLayout(s_Data.Device, &layoutCreateInfo, nullptr, &s_Data.PipelineLayout) != VK_SUCCESS)
		mEXIT("Failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.stageCount = 2;
	createInfo.pStages = shaderStages;
	createInfo.pVertexInputState = &vertexInputInfo;
	createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	createInfo.pViewportState = &viewportInfo;
	createInfo.pRasterizationState = &rasterizerInfo;
	createInfo.pMultisampleState = &multisampleCreateInfo;
	createInfo.pColorBlendState = &colorBlendInfo;
	createInfo.layout = s_Data.PipelineLayout;
	createInfo.renderPass = s_Data.RenderPass;
	createInfo.subpass = 0;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = -1;
	createInfo.pDepthStencilState = &depthStencil;

	if (vkCreateGraphicsPipelines(s_Data.Device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &s_Data.GraphicsPipeline) != VK_SUCCESS)
		mEXIT("Failed to create graphics pipeline!");

	vkDestroyShaderModule(s_Data.Device, vertShader, nullptr);
	vkDestroyShaderModule(s_Data.Device, fragShader, nullptr);
}

static void CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = s_Data.SwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.format = FindDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array attachments = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pAttachments = attachments.data();
	createInfo.attachmentCount = (uint32_t)attachments.size();
	createInfo.pSubpasses = &subpass;
	createInfo.subpassCount = 1;
	createInfo.pDependencies = &dependency;
	createInfo.dependencyCount = 1;

	if (vkCreateRenderPass(s_Data.Device, &createInfo, nullptr, &s_Data.RenderPass) != VK_SUCCESS)
		mEXIT("Failed to create render pass!");
}

static void CreateFramebuffers()
{
	size_t size = s_Data.SwapchainImages.size();
	s_Data.SwapchainFramebuffers.resize(size);

	for (size_t i = 0; i < size; ++i)
	{
		std::array attachments = { s_Data.SwapchainImageViews[i], s_Data.DepthImageView };

		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.pAttachments = attachments.data();
		createInfo.attachmentCount = (uint32_t)attachments.size();
		createInfo.renderPass = s_Data.RenderPass;
		createInfo.height = s_Data.SwapchainExtent.height;
		createInfo.width = s_Data.SwapchainExtent.width;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(s_Data.Device, &createInfo, nullptr, &s_Data.SwapchainFramebuffers[i]) != VK_SUCCESS)
			mEXIT("Failed to create framebuffer");
	}
}

static void CreateCommandPool()
{
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = s_Data.FamilyIndices.GraphicsQueue.value();

	if (vkCreateCommandPool(s_Data.Device, &createInfo, nullptr, &s_Data.CommandPool) != VK_SUCCESS)
		mEXIT("Failed to create command pool!");
}

static void CreateCommandBuffers()
{
	const size_t count = s_Data.SwapchainFramebuffers.size();
	s_Data.CommandBuffers.resize(count);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = s_Data.CommandPool;
	allocInfo.commandBufferCount = (uint32_t)count;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	
	if (vkAllocateCommandBuffers(s_Data.Device, &allocInfo, s_Data.CommandBuffers.data()) != VK_SUCCESS)
		mEXIT("Failed to allocate command buffers!");

	VkRect2D renderArea{};
	renderArea.extent = s_Data.SwapchainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
	clearValues[1].depthStencil = {1.f, 0};

	for (size_t i = 0; i < count; ++i)
	{
		const VkCommandBuffer& commandBuffer = s_Data.CommandBuffers[i];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkDeviceSize offsets[] = { 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = s_Data.RenderPass;
		renderPassInfo.framebuffer = s_Data.SwapchainFramebuffers[i];
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
		renderPassInfo.renderArea = renderArea;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data.GraphicsPipeline);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &s_Data.VertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, s_Data.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data.PipelineLayout, 0, 1, &s_Data.DescriptorSets[i], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, (uint32_t)s_Indices.size(), 1, 0, 0, 0);
		vkCmdEndRenderPass(commandBuffer);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			mEXIT("Failed to end command buffer!");
	}
}

static void CreateSyncObjects()
{
	s_Data.ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	s_Data.RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	s_Data.InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	s_Data.ImagesInFlightFences.resize(s_Data.SwapchainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;


	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateSemaphore(s_Data.Device, &semaphoreCreateInfo, nullptr, &s_Data.ImageAvailableSemaphores[i]) != VK_SUCCESS)
			mEXIT("Failed to create semaphore");
		if (vkCreateSemaphore(s_Data.Device, &semaphoreCreateInfo, nullptr, &s_Data.RenderFinishedSemaphores[i]) != VK_SUCCESS)
			mEXIT("Failed to create semaphore");
		if (vkCreateFence(s_Data.Device, &fenceCreateInfo, nullptr, &s_Data.InFlightFences[i]) != VK_SUCCESS)
			mEXIT("Failed to create fence");
	}
}

static void CleanSwapchain()
{
	for (auto& framebuffer : s_Data.SwapchainFramebuffers)
		vkDestroyFramebuffer(s_Data.Device, framebuffer, nullptr);

	vkDestroyImageView(s_Data.Device, s_Data.DepthImageView, nullptr);
	vkDestroyImage(s_Data.Device, s_Data.DepthImage, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.DepthImageMemory, nullptr);

	vkFreeCommandBuffers(s_Data.Device, s_Data.CommandPool, (uint32_t)s_Data.CommandBuffers.size(), s_Data.CommandBuffers.data());

	vkDestroyPipeline(s_Data.Device, s_Data.GraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(s_Data.Device, s_Data.PipelineLayout, nullptr);
	vkDestroyRenderPass(s_Data.Device, s_Data.RenderPass, nullptr);

	for (auto& ub : s_Data.UniformBuffers)
		vkDestroyBuffer(s_Data.Device, ub, nullptr);
	for (auto& mem : s_Data.UniformBuffersMemory)
		vkFreeMemory(s_Data.Device, mem, nullptr);

	for (auto& imageView : s_Data.SwapchainImageViews)
		vkDestroyImageView(s_Data.Device, imageView, nullptr);

	vkDestroySwapchainKHR(s_Data.Device, s_Data.Swapchain, nullptr);
	vkDestroyDescriptorPool(s_Data.Device, s_Data.DescriptorPool, nullptr);
}

static void CreateUniformBuffers()
{
	constexpr VkDeviceSize size = sizeof(UniformBufferObject);
	size_t count = s_Data.SwapchainImages.size();
	s_Data.UniformBuffers.resize(size);
	s_Data.UniformBuffersMemory.resize(size);

	for (size_t i = 0; i < count; ++i)
		CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			s_Data.UniformBuffers[i], s_Data.UniformBuffersMemory[i]);
}

static void CreateVertexBuffer()
{
	VkDeviceSize size = s_Vertices.size() * sizeof(Vertex);
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);
	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.VertexBuffer, s_Data.VertexBufferMemory);

	void* data = nullptr;
	vkMapMemory(s_Data.Device, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, s_Vertices.data(), size);
	vkUnmapMemory(s_Data.Device, stagingBufferMemory);

	CopyBuffer(stagingBuffer, s_Data.VertexBuffer, size);

	vkDestroyBuffer(s_Data.Device, stagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, stagingBufferMemory, nullptr);
}

static void CreateIndexBuffer()
{
	VkDeviceSize size = s_Indices.size() * sizeof(uint32_t);
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.IndexBuffer, s_Data.IndexBufferMemory);
	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(s_Data.Device, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, s_Indices.data(), size);
	vkUnmapMemory(s_Data.Device, stagingBufferMemory);

	CopyBuffer(stagingBuffer, s_Data.IndexBuffer, size);

	vkDestroyBuffer(s_Data.Device, stagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, stagingBufferMemory, nullptr);
}

static void CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboBinding{};
	uboBinding.binding = 0;
	uboBinding.descriptorCount = 1;
	uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding samplerBinding{};
	samplerBinding.binding = 1;
	samplerBinding.descriptorCount = 1;
	samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array bindings = { uboBinding, samplerBinding };

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = (uint32_t)bindings.size();
	createInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(s_Data.Device, &createInfo, nullptr, &s_Data.DescriptorSetLayout) != VK_SUCCESS)
		mEXIT("Failed to create descriptor set layout!");
}

static void UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	
	UniformBufferObject ubo;
	ubo.model = glm::rotate(glm::mat4(1.f), glm::radians(90.f) * time, glm::vec3(0.f, 0.f, 1.f));
	ubo.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.proj = glm::perspective(glm::radians(45.f), (float)s_Data.SwapchainExtent.width / s_Data.SwapchainExtent.height, 0.1f, 10.f);
	ubo.proj[1][1] *= -1.f;

	void* data = nullptr;
	vkMapMemory(s_Data.Device, s_Data.UniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(s_Data.Device, s_Data.UniformBuffersMemory[currentImage]);
}

static void CreateDescriptorPool()
{
	VkDescriptorPoolSize uboSize{};
	uboSize.descriptorCount = (uint32_t)s_Data.SwapchainImages.size();
	uboSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorPoolSize samplerSize{};
	samplerSize.descriptorCount = (uint32_t)s_Data.SwapchainImages.size();
	samplerSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	std::array sizes = { uboSize, samplerSize };

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.pPoolSizes = sizes.data();
	createInfo.poolSizeCount = (uint32_t)sizes.size();
	createInfo.maxSets = (uint32_t)s_Data.SwapchainImages.size();

	if (vkCreateDescriptorPool(s_Data.Device, &createInfo, nullptr, &s_Data.DescriptorPool) != VK_SUCCESS)
		mEXIT("Failed to create descriptor pool!");
}

static void CreateDescriptorSets()
{
	size_t size = s_Data.SwapchainImages.size();
	std::vector<VkDescriptorSetLayout> layouts(size, s_Data.DescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = s_Data.DescriptorPool;
	allocInfo.descriptorSetCount = (uint32_t)size;
	allocInfo.pSetLayouts = layouts.data();

	s_Data.DescriptorSets.resize(size);
	if (vkAllocateDescriptorSets(s_Data.Device, &allocInfo, s_Data.DescriptorSets.data()) != VK_SUCCESS)
		mEXIT("Failed to allocate descriptor sets!");

	for (size_t i = 0; i < size; ++i)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = s_Data.UniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = s_Data.TextureSampler;
		imageInfo.imageView = s_Data.TextureImageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet uboDescWrite{};
		uboDescWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboDescWrite.dstSet = s_Data.DescriptorSets[i];
		uboDescWrite.dstBinding = 0;
		uboDescWrite.dstArrayElement = 0;
		uboDescWrite.descriptorCount = 1;
		uboDescWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboDescWrite.pBufferInfo = &bufferInfo;

		VkWriteDescriptorSet imageDescWrite{};
		imageDescWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescWrite.dstSet = s_Data.DescriptorSets[i];
		imageDescWrite.dstBinding = 1;
		imageDescWrite.dstArrayElement = 0;
		imageDescWrite.descriptorCount = 1;
		imageDescWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageDescWrite.pImageInfo = &imageInfo;

		std::array writeDescriptors = { uboDescWrite, imageDescWrite };

		vkUpdateDescriptorSets(s_Data.Device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
	}

}

static void CreateTextureImage()
{
	int w, h, channels;
	stbi_uc* imageData = stbi_load(s_TexturePath, &w, &h, &channels, 4);

	VkDeviceSize size = (size_t)w * h * 4;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);
	void* data = nullptr;
	vkMapMemory(s_Data.Device, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, imageData, size);
	vkUnmapMemory(s_Data.Device, stagingBufferMemory);

	stbi_image_free(imageData);
	VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;

	CreateImage((uint32_t)w, (uint32_t)h, textureFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.TextureImage, s_Data.TextureImageMemory);
	TransitionImageLayout(s_Data.TextureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer, s_Data.TextureImage, (uint32_t)w, (uint32_t)h);
	TransitionImageLayout(s_Data.TextureImage, textureFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(s_Data.Device, stagingBuffer, nullptr);
	vkFreeMemory(s_Data.Device, stagingBufferMemory, nullptr);
}

static void CreateTextureImageView()
{
	s_Data.TextureImageView = CreateImageView(s_Data.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void CreateTextureSampler()
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(s_Data.PhysicalDevice, &props);

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.minLod = 0.f;
	createInfo.maxLod = 0.f;
	createInfo.mipLodBias = 0.f;

	if (vkCreateSampler(s_Data.Device, &createInfo, nullptr, &s_Data.TextureSampler) != VK_SUCCESS)
		mEXIT("Failed to create sampler!");
}

static void CreateDepthResources()
{
	VkFormat depthFormat = FindDepthFormat();
	uint32_t w = s_Data.SwapchainExtent.width;
	uint32_t h = s_Data.SwapchainExtent.height;
	CreateImage(w, h, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_Data.DepthImage, s_Data.DepthImageMemory);
	s_Data.DepthImageView = CreateImageView(s_Data.DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	TransitionImageLayout(s_Data.DepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

static void RecreateSwapchain()
{
	vkDeviceWaitIdle(s_Data.Device);

	CleanSwapchain();

	CreateSwapchain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateDepthResources();
	CreateFramebuffers();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
}

static void LoadModel()
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, s_ModelPath))
		mEXIT("Failed to load the model!");
	
	s_Vertices.reserve(shapes.size() * 2);
	s_Indices.reserve(shapes.size() * 2);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes)
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.Position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.TexCoords = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.Color = { 1.0f, 1.0f, 1.0f };

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(s_Vertices.size());
				s_Vertices.push_back(vertex);
			}

			s_Indices.push_back(uniqueVertices[vertex]);
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
	CreateSwapchain();
	CreateImageViews();
	CreateRenderPass();
	CreateCommandPool();
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	CreateDepthResources();
	CreateFramebuffers();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	LoadModel();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void Renderer::Shutdown()
{
	vkDeviceWaitIdle(s_Data.Device);
	
	CleanSwapchain();

	vkDestroySampler(s_Data.Device, s_Data.TextureSampler, nullptr);
	vkDestroyImageView(s_Data.Device, s_Data.TextureImageView, nullptr);
	vkDestroyImage(s_Data.Device, s_Data.TextureImage, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.TextureImageMemory, nullptr);
	vkDestroyDescriptorSetLayout(s_Data.Device, s_Data.DescriptorSetLayout, nullptr);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(s_Data.Device, s_Data.ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(s_Data.Device, s_Data.RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(s_Data.Device, s_Data.InFlightFences[i], nullptr);
	}

	vkDestroyBuffer(s_Data.Device, s_Data.VertexBuffer, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.VertexBufferMemory, nullptr);
	vkDestroyBuffer(s_Data.Device, s_Data.IndexBuffer, nullptr);
	vkFreeMemory(s_Data.Device, s_Data.IndexBufferMemory, nullptr);
	
	vkDestroyCommandPool(s_Data.Device, s_Data.CommandPool, nullptr);
	vkDestroyDevice(s_Data.Device, nullptr);
	vkDestroySurfaceKHR(s_Data.Instance, s_Data.Surface, nullptr);

	if (s_EnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(s_Data.Instance, s_Data.DebugMessanger, nullptr);

	vkDestroyInstance(s_Data.Instance, nullptr);
}

void Renderer::DrawFrame()
{
	vkWaitForFences(s_Data.Device, 1, &s_Data.InFlightFences[s_CurrentFrame], VK_FALSE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(s_Data.Device, s_Data.Swapchain, UINT64_MAX, s_Data.ImageAvailableSemaphores[s_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return;
	}

	if (s_Data.ImagesInFlightFences[imageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(s_Data.Device, 1, &s_Data.ImagesInFlightFences[imageIndex], VK_FALSE, UINT64_MAX);
	s_Data.ImagesInFlightFences[imageIndex] = s_Data.InFlightFences[s_CurrentFrame];

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	UpdateUniformBuffer(imageIndex);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pCommandBuffers = &s_Data.CommandBuffers[imageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = &s_Data.RenderFinishedSemaphores[s_CurrentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &s_Data.ImageAvailableSemaphores[s_CurrentFrame];
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitDstStageMask = waitStages;

	vkResetFences(s_Data.Device, 1, &s_Data.InFlightFences[s_CurrentFrame]);

	if (vkQueueSubmit(s_Data.GraphicsQueue, 1, &submitInfo, s_Data.InFlightFences[s_CurrentFrame]) != VK_SUCCESS)
		mEXIT("Failed to submit draw queue!");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pWaitSemaphores = &s_Data.RenderFinishedSemaphores[s_CurrentFrame];
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &s_Data.Swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(s_Data.PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		RecreateSwapchain();

	s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::OnWindowResized()
{
	RecreateSwapchain();
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

	uint32_t width = s_Data.SwapchainExtent.width;
	uint32_t height = s_Data.SwapchainExtent.height;

	uint32_t commandBufferIndex = s_CurrentFrame;

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
	renderPassBeginInfo.framebuffer = s_Data.SwapchainFramebuffers[commandBufferIndex];

	vkCmdBeginRenderPass(drawCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = s_Data.RenderPass;
	inheritanceInfo.framebuffer = s_Data.SwapchainFramebuffers[commandBufferIndex];

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	cmdBufInfo.pInheritanceInfo = &inheritanceInfo;

	//vkBeginCommandBuffer(s_ImGuiCommandBuffers[commandBufferIndex], &cmdBufInfo);

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	//vkCmdSetViewport(s_ImGuiCommandBuffers[commandBufferIndex], 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	//vkCmdSetScissor(s_ImGuiCommandBuffers[commandBufferIndex], 0, 1, &scissor);

	ImDrawData* main_draw_data = ImGui::GetDrawData();
	//ImGui_ImplVulkan_RenderDrawData(main_draw_data, s_ImGuiCommandBuffers[commandBufferIndex]);

	//vkEndCommandBuffer(s_ImGuiCommandBuffers[commandBufferIndex]);

	std::vector<VkCommandBuffer> commandBuffers;
	//commandBuffers.push_back(s_ImGuiCommandBuffers[commandBufferIndex]);

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
	//vkCreateDescriptorPool(device, &pool_info, nullptr, &s_ImGui_DescriptorPool);

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = s_Data.Instance;
	init_info.PhysicalDevice = s_Data.PhysicalDevice;
	init_info.Device = device;
	init_info.QueueFamily = s_Data.FamilyIndices.GraphicsQueue.value();
	init_info.Queue = s_Data.GraphicsQueue;
	init_info.PipelineCache = nullptr;
	//init_info.DescriptorPool = s_ImGui_DescriptorPool;
	init_info.Allocator = nullptr;
	init_info.MinImageCount = 2;
	init_info.ImageCount = (uint32_t)s_Data.SwapchainImages.size();
	ImGui_ImplVulkan_Init(&init_info, s_Data.RenderPass);

	// Upload Fonts
	{
		// Use any command queue

		//VkCommandBuffer commandBuffer = GetCommandBuffer(true);
		//ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		//FlushCommandBuffer(commandBuffer, s_Data.GraphicsQueue);

		vkDeviceWaitIdle(device);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	//s_ImGuiCommandBuffers.resize(s_MaxFramesInFlight);
	//for (uint32_t i = 0; i < s_MaxFramesInFlight; i++)
	//	s_ImGuiCommandBuffers[i] = CreateSecondaryCommandBuffer();
}

