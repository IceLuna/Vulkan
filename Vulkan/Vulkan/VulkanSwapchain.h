#pragma once

#include "Vulkan.h"
#include "VulkanDevice.h"
#include "VulkanSemaphore.h"
#include "VulkanImage.h"

class VulkanDevice;
struct GLFWwindow;

class VulkanSwapchain
{
public:
	VulkanSwapchain(VkInstance instance, GLFWwindow* window);
	~VulkanSwapchain();

	void Init(const VulkanDevice* device);
	VkSurfaceKHR GetSurface() const { return m_Surface; }
	bool IsVSyncEnabled() const { return m_bVSyncEnabled; }

	void SetVSyncEnabled(bool bEnabled)
	{
		if (m_bVSyncEnabled != bEnabled)
		{
			m_bVSyncEnabled = bEnabled;
			RecreateSwapchain();
		}
	}

	void OnResized() { RecreateSwapchain(); }

	const std::vector<VulkanImage*>& GetImages() const { return m_Images; }

	void Present(const VulkanSemaphore* waitSemaphore);

	// Returns a semaphore that will be signaled when image is ready
	const VulkanSemaphore* AcquireImage(uint32_t* outFrameIndex);

	uint32_t GetFrameIndex() const { return m_FrameIndex; }
	glm::uvec2 GetSize() const { return { m_Extent.width, m_Extent.height }; }

private:
	void RecreateSwapchain();
	void CreateSyncObjects();

private:
	std::vector<VulkanImage*> m_Images;
	std::vector<VulkanSemaphore> m_WaitSemaphores;
	SwapchainSupportDetails m_SupportDetails;
	VkExtent2D m_Extent;
	VkSurfaceFormatKHR m_Format;
	VkInstance m_Instance;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
	const VulkanDevice* m_Device = nullptr;
	GLFWwindow* m_Window = nullptr;
	uint32_t m_FrameIndex = 0;
	uint32_t m_SwapchainPresentImageIndex = 0;
	bool m_bVSyncEnabled = false;
};
