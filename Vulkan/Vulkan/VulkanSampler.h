#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "../Renderer/RendererUtils.h"

class VulkanSampler
{
public:
	VulkanSampler(FilterMode filterMode, AddressMode addressMode, CompareOperation compareOp, float minLod, float maxLod, float maxAnisotropy = 1.f)
		: m_Device(VulkanContext::GetDevice()->GetVulkanDevice())
		, m_FilterMode(filterMode)
		, m_AddressMode(addressMode)
		, m_CompareOp(compareOp)
		, m_MinLod(minLod)
		, m_MaxLod(maxLod)
		, m_MaxAnisotropy(maxAnisotropy)
	{
		VkSamplerAddressMode vkAddressMode = AddressModeToVulkan(m_AddressMode);

		VkSamplerCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.addressModeU = vkAddressMode;
		info.addressModeV = vkAddressMode;
		info.addressModeW = vkAddressMode;
		info.anisotropyEnable = m_MaxAnisotropy > 1.f ? VK_TRUE : VK_FALSE;
		info.maxAnisotropy = m_MaxAnisotropy;
		info.minLod = m_MinLod;
		info.maxLod = m_MaxLod;
		info.borderColor = BorderColorForAddressMode(m_AddressMode);
		info.compareOp = CompareOpToVulkan(m_CompareOp);
		info.compareEnable = m_CompareOp != CompareOperation::Never;
		FilterModeToVulkan(m_FilterMode, &info.minFilter, &info.magFilter, &info.mipmapMode);

		VK_CHECK(vkCreateSampler(m_Device, &info, nullptr, &m_Sampler));
	}

	VulkanSampler(const VulkanSampler&) = delete;
	VulkanSampler(VulkanSampler&& other) noexcept
	{
		m_Device = other.m_Device;
		m_Sampler = other.m_Sampler;
		m_FilterMode = other.m_FilterMode;
		m_AddressMode = other.m_AddressMode;
		m_CompareOp = other.m_CompareOp;
		m_MinLod = other.m_MinLod;
		m_MaxLod = other.m_MaxLod;
		m_MaxAnisotropy = other.m_MaxAnisotropy;

		other.m_Device = VK_NULL_HANDLE;
		other.m_Sampler = VK_NULL_HANDLE;
		other.m_AddressMode = AddressMode::Wrap;
		other.m_CompareOp = CompareOperation::Never;
		other.m_MinLod = 0.f;
		other.m_MaxLod = 0.f;
		other.m_MaxAnisotropy = 1.f;
	}

	VulkanSampler& operator= (const VulkanSampler&) = delete;
	VulkanSampler& operator= (VulkanSampler&& other) noexcept
	{
		if (m_Sampler)
			vkDestroySampler(m_Device, m_Sampler, nullptr);

		m_Device = other.m_Device;
		m_Sampler = other.m_Sampler;
		m_FilterMode = other.m_FilterMode;
		m_AddressMode = other.m_AddressMode;
		m_CompareOp = other.m_CompareOp;
		m_MinLod = other.m_MinLod;
		m_MaxLod = other.m_MaxLod;
		m_MaxAnisotropy = other.m_MaxAnisotropy;

		other.m_Device = VK_NULL_HANDLE;
		other.m_Sampler = VK_NULL_HANDLE;
		other.m_AddressMode = AddressMode::Wrap;
		other.m_CompareOp = CompareOperation::Never;
		other.m_MinLod = 0.f;
		other.m_MaxLod = 0.f;
		other.m_MaxAnisotropy = 1.f;

		return *this;
	}

	virtual ~VulkanSampler()
	{
		if (m_Sampler)
			vkDestroySampler(m_Device, m_Sampler, nullptr);
	}

	VkSampler GetVulkanSampler() const { return m_Sampler; }
	FilterMode GetFilterMode() const { return m_FilterMode; }
	AddressMode GetAddressMode() const { return m_AddressMode; }
	CompareOperation GetCompareOperation() const { return m_CompareOp; }
	float GetMinLod() const { return m_MinLod; }
	float GetMaxLod() const { return m_MaxLod; }
	float GetMaxAnisotropy() const { return m_MaxAnisotropy; }

private:
	VkDevice m_Device = VK_NULL_HANDLE;
	VkSampler m_Sampler = VK_NULL_HANDLE;
	FilterMode m_FilterMode = FilterMode::Point;
	AddressMode m_AddressMode = AddressMode::Wrap;
	CompareOperation m_CompareOp = CompareOperation::Never;
	float m_MinLod = 0.f;
	float m_MaxLod = 0.f;
	float m_MaxAnisotropy = 1.f;
};
