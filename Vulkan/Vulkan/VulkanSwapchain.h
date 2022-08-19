#pragma once

#include "Vulkan.h"
#include "VulkanDevice.h"
#include "VulkanSemaphore.h"

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

	void Present(const VulkanSemaphore* waitSemaphore);
	const VulkanSemaphore* NextFrame();

	const std::vector<VkFramebuffer>& GetVulkanFramebuffers() const { return m_Framebuffers; }
	VkRenderPass GetVulkanRenderpass() const { return m_RenderPass; }

	uint32_t GetFrameIndex() const { return m_FrameIndex; }

private:
	void RecreateSwapchain();
	void CreateRenderPass();
	void CreateImageViews();
	void DestroyImageViews();
	void CreateFramebuffers();
	void DestroyFramebuffers();
	void CreateSyncObjects();

private:
	std::vector<VkImage> m_Images;
	std::vector<VkImageView> m_ImageViews;
	std::vector<VkFramebuffer> m_Framebuffers;
	std::vector<VulkanSemaphore> m_WaitSemaphores;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;
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
	bool m_bVSyncEnabled = true;
};
