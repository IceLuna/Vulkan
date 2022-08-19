#include "VulkanContext.h"
#include "VulkanDevice.h"
#include <vector>
#include <set>

#ifdef _DEBUG
static constexpr bool s_EnableValidation = true;
#else
static constexpr bool s_EnableValidation = false;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cout << pCallbackData->pMessage;
	return VK_FALSE; //To not abort caller
}

static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* outCI)
{
	outCI->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	outCI->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	outCI->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	outCI->pfnUserCallback = DebugCallback;
	outCI->pUserData = nullptr;
	outCI->pNext = nullptr;
	outCI->flags = 0;
}

static bool AreLayersSupported(const std::vector<const char*>& layers)
{
	uint32_t count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> layerProps(count);
	vkEnumerateInstanceLayerProperties(&count, layerProps.data());

	std::set<std::string> layersSet(layers.begin(), layers.end());

	for (auto& layerProp : layerProps)
	{
		if (layersSet.find(layerProp.layerName) != layersSet.end())
			layersSet.erase(layerProp.layerName);

		if (layersSet.empty())
			return true;
	}

	std::cout << "Some layers are not supported:\n" << std::endl;
	for (auto& layer : layersSet)
		std::cout << '\t' << layer << '\n';

	return false;
}

static VkInstance CreateInstance(const std::vector<const char*>& extensions)
{
	const std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VulkanContext::GetVulkanAPIVersion();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pApplicationName = "Testing";
	appInfo.pEngineName = "Testing";

	VkInstanceCreateInfo instanceCI{};
	instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCI.pApplicationInfo = &appInfo;
	instanceCI.enabledExtensionCount = (uint32_t)extensions.size();
	instanceCI.ppEnabledExtensionNames = extensions.data();
	VkDebugUtilsMessengerCreateInfoEXT debugCI{};
	if (s_EnableValidation && AreLayersSupported(validationLayers))
	{
		PopulateDebugMessengerCreateInfo(&debugCI);
		instanceCI.enabledLayerCount = (uint32_t)validationLayers.size();
		instanceCI.ppEnabledLayerNames = validationLayers.data();
		instanceCI.pNext = &debugCI;
	}

	VkInstance result = nullptr;
	VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &result));
	return result;
}

static std::vector<const char*> GetRequiredExtensions()
{
	std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface" };
	if constexpr (s_EnableValidation)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return instanceExtensions;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func)
		return func(instance, pCreateInfo, pAllocator, pMessenger);
	else
	{
		std::cout << "Failed to get vkCreateDebugUtilsMessengerEXT\n";
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func)
		func(instance, messenger, pAllocator);
	else
		std::cout << "Failed to get vkDestroyDebugUtilsMessengerEXT\n";
}

static VkDebugUtilsMessengerEXT CreateDebugUtils(VkInstance instance)
{
	if constexpr (!s_EnableValidation)
		return VK_NULL_HANDLE;

	VkDebugUtilsMessengerCreateInfoEXT ci;
	PopulateDebugMessengerCreateInfo(&ci);

	VkDebugUtilsMessengerEXT result{};
	VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &result));
	return result;
}

VkInstance VulkanContext::s_Instance = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT s_DebugMessenger = VK_NULL_HANDLE;

VulkanContext::VulkanContext()
{
	assert(!s_Instance); // Instance already created

	s_Instance = CreateInstance(GetRequiredExtensions());
	s_DebugMessenger = CreateDebugUtils(s_Instance);
}

VulkanContext::~VulkanContext()
{
	m_Device.reset();
	m_PhysicalDevice.reset();

	if (s_DebugMessenger)
	{
		DestroyDebugUtilsMessengerEXT(s_Instance, s_DebugMessenger, nullptr);
		s_DebugMessenger = VK_NULL_HANDLE;
	}

	vkDestroyInstance(s_Instance, nullptr);
}

void VulkanContext::InitDevices(VkSurfaceKHR surface, bool bRequireSurface)
{
	m_PhysicalDevice = VulkanPhysicalDevice::Select(surface, bRequireSurface);

	VkPhysicalDeviceFeatures features{};
	features.wideLines = VK_TRUE;
	m_Device = VulkanDevice::Create(m_PhysicalDevice, features);
}
