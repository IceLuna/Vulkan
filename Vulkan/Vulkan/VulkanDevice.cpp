#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include <set>

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, bool bRequirePresent)
{
	QueueFamilyIndices result;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> familyProps(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, familyProps.data());

	constexpr uint32_t queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < familyProps.size(); ++i)
		{
			auto& queueProps = familyProps[i];
			if ((queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				result.ComputeFamily = i;
				break;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < familyProps.size(); ++i)
		{
			auto& queueProps = familyProps[i];
			if ((queueProps.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				result.TransferFamily = i;
				break;
			}
		}
	}

	VkBool32 supportsPresent = VK_FALSE;
	for (uint32_t i = 0; i < familyProps.size(); ++i)
	{
		auto& queueProps = familyProps[i];

		if ((queueFlags & VK_QUEUE_COMPUTE_BIT) && (result.ComputeFamily == -1))
			if (queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT)
				result.ComputeFamily = i;

		if ((queueFlags & VK_QUEUE_TRANSFER_BIT) && (result.TransferFamily == -1))
			if (queueProps.queueFlags & VK_QUEUE_TRANSFER_BIT)
				result.TransferFamily = i;

		if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) && (result.GraphicsFamily == -1))
			if (queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				result.GraphicsFamily = i;

		if (surface != VK_NULL_HANDLE)
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supportsPresent);
		if (supportsPresent)
			result.PresentFamily = i;

		if (result.IsComplete(bRequirePresent))
			break;
	}

	return result;
}

static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails result;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.Capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	result.Formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, result.Formats.data());

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	result.PresentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, result.PresentModes.data());

	return result;
}

static bool AreExtensionsSupported(VkPhysicalDevice device, const std::vector<const char*>& extensions)
{
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> props(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, props.data());

	std::set<std::string> requestedExtensions(extensions.begin(), extensions.end());

	for (auto& prop : props)
		requestedExtensions.erase(prop.extensionName);

	return requestedExtensions.empty();
}

static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, bool bRequirePresent, const std::vector<const char*>& extensions,
	QueueFamilyIndices* outFamilyIndices, SwapchainSupportDetails* outSwapchainSupportDetails)
{
	*outFamilyIndices = FindQueueFamilies(device, surface, bRequirePresent);
	bool bExtensionsSupported = AreExtensionsSupported(device, extensions);
	bool bSwapchainAdequate = true;

	if (bRequirePresent && bExtensionsSupported)
	{
		*outSwapchainSupportDetails = QuerySwapchainSupport(device, surface);
		bSwapchainAdequate = outSwapchainSupportDetails->Formats.size() > 0 && outSwapchainSupportDetails->PresentModes.size() > 0;
	}

	return bSwapchainAdequate && bExtensionsSupported && outFamilyIndices->IsComplete(bRequirePresent);
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VkSurfaceKHR surface, bool bRequirePresentSupport)
	: m_RequiresPresentQueue(bRequirePresentSupport)
{
	VkInstance instance = VulkanContext::GetInstance();
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(count);
	vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());

	assert(count != 0);

	if (bRequirePresentSupport)
		m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	SwapchainSupportDetails supportDetails;
	for (auto& device : physicalDevices)
	{
		vkGetPhysicalDeviceProperties(device, &m_Properties);
		if (IsDeviceSuitable(device, surface, bRequirePresentSupport, m_DeviceExtensions, &m_FamilyIndices, &supportDetails))
		{
			m_PhysicalDevice = device;
			if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				break;
		}
	}

	if (!m_PhysicalDevice)
	{
		std::cout << "No gpu found!\n";
		std::exit(-1);
	}

	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);
	if (AreExtensionsSupported(m_PhysicalDevice, std::vector<const char*>{ VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME }))
	{
		m_ExtensionSupport.SupportsConservativeRasterization = true;
		m_DeviceExtensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
	}
}

SwapchainSupportDetails VulkanPhysicalDevice::QuerySwapchainSupportDetails(VkSurfaceKHR surface) const
{
	return QuerySwapchainSupport(m_PhysicalDevice, surface);
}

bool VulkanPhysicalDevice::IsMipGenerationSupported(ImageFormat format) const
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, ImageFormatToVulkan(format), &props);
	return props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
}

VulkanDevice::VulkanDevice(const std::unique_ptr<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures& enabledFeatures)
	: m_PhysicalDevice(physicalDevice.get())
{
	constexpr float queuePriority = 1.f;
	auto& queueFamilyIndices = physicalDevice->GetFamilyIndices();
	const auto& deviceExtensions = physicalDevice->GetDeviceExtensions();

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(1);
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].queueCount = 1;
	queueCreateInfos[0].queueFamilyIndex = queueFamilyIndices.GraphicsFamily;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;

	const bool bPresentSameAsGraphics  = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.PresentFamily;
	const bool bComputeSameAsGraphics  = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.ComputeFamily;
	const bool bTransferSameAsGraphics = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.TransferFamily;
	const bool bTransferSameAsCompute  = queueFamilyIndices.ComputeFamily == queueFamilyIndices.TransferFamily;
	const bool bRequiresPresentQueue = physicalDevice->RequiresPresentQueue();

	VkDeviceQueueCreateInfo additionalQueueCI{};
	additionalQueueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	additionalQueueCI.queueCount = 1;
	additionalQueueCI.queueFamilyIndex = queueFamilyIndices.PresentFamily;
	additionalQueueCI.pQueuePriorities = &queuePriority;
	if (bRequiresPresentQueue && !bPresentSameAsGraphics)
	{
		additionalQueueCI.queueFamilyIndex = queueFamilyIndices.PresentFamily;
		queueCreateInfos.push_back(additionalQueueCI);
	}
	if (!bComputeSameAsGraphics)
	{
		additionalQueueCI.queueFamilyIndex = queueFamilyIndices.ComputeFamily;
		queueCreateInfos.push_back(additionalQueueCI);
	}
	if (!bTransferSameAsGraphics && !bTransferSameAsCompute)
	{
		additionalQueueCI.queueFamilyIndex = queueFamilyIndices.TransferFamily;
		queueCreateInfos.push_back(additionalQueueCI);
	}

	VkDeviceCreateInfo deviceCI{};
	deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.pEnabledFeatures = &enabledFeatures;
	deviceCI.pQueueCreateInfos = queueCreateInfos.data();
	deviceCI.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCI.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	deviceCI.ppEnabledExtensionNames = deviceExtensions.data();

	VK_CHECK(vkCreateDevice(physicalDevice->GetVulkanPhysicalDevice(), &deviceCI, nullptr, &m_Device));
	vkGetDeviceQueue(m_Device, queueFamilyIndices.GraphicsFamily, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, queueFamilyIndices.ComputeFamily, 0, &m_ComputeQueue);
	vkGetDeviceQueue(m_Device, queueFamilyIndices.TransferFamily, 0, &m_TransferQueue);
	if (bRequiresPresentQueue)
	{
		if (!bPresentSameAsGraphics)
			vkGetDeviceQueue(m_Device, queueFamilyIndices.PresentFamily, 0, &m_PresentQueue);
		else
			m_PresentQueue = m_GraphicsQueue;
	}
}

VulkanDevice::~VulkanDevice()
{
	vkDestroyDevice(m_Device, nullptr);
}
