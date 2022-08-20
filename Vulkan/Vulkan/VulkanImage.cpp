#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanAllocator.h"
#include "VulkanUtils.h"

VulkanImage::VulkanImage(const ImageSpecifications& specs) : m_Specs(specs)
{
	assert(specs.Size.x > 0 && specs.Size.y > 0);

	m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
	CreateImage();
	CreateImageView();
}

VulkanImage::VulkanImage(VkImage vulkanImage, const ImageSpecifications& specs, bool bOwns)
	: m_Specs(specs)
	, m_Image(vulkanImage)
	, m_bOwns(bOwns)
{
	m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
	m_VulkanFormat = ImageFormatToVulkan(m_Specs.Format);
	CreateImageView();
}

VulkanImage::~VulkanImage()
{
	ReleaseImageView();
	ReleaseImage();
}

void VulkanImage::CreateImage()
{
	assert(m_bOwns);

	m_VulkanFormat = ImageFormatToVulkan(m_Specs.Format);
	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = ImageTypeToVulkan(m_Specs.Type);
	info.format = m_VulkanFormat;
	info.arrayLayers = m_Specs.bIsCube ? 6 : 1;
	info.extent = { m_Specs.Size.x, m_Specs.Size.y, m_Specs.Size.z };
	info.mipLevels = m_Specs.MipsCount;
	info.samples = GetVulkanSamplesCount(m_Specs.SamplesCount);
	info.tiling = m_Specs.MemoryType == MemoryType::Gpu ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
	info.usage = ImageUsageToVulkan(m_Specs.Usage);
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.flags |= m_Specs.bIsCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	// This is a workaround for Vulkan Memory Allocator issue
	// The problem here is when all MemoryType::kGpu memory is allocated, the Allocator begins to put images in memory of other MemoryTypes.
	// It leads to Image and and mappable Buffers to be bound to the same VkDeviceMemory.
	// When vmaMapMemory() is called, vkMapMemory() with called for VkDeviceMemory with size = VK_WHOLE_SIZE. 
	// It could lead to device lost if VkImage is not in layout VK_IMAGE_LAYOUT_GENERAL.
	// Workarounding the issue by allocating each VkImage in its own VkDeviceMemory, so that mapping buffers can't result in mapping memory bound to VkImage.
	constexpr bool separateAllocation = true;
	m_Allocation = VulkanAllocator::AllocateImage(&info, m_Specs.MemoryType, separateAllocation, &m_Image);
}

void VulkanImage::ReleaseImage()
{
	if (m_bOwns && m_Image)
		VulkanAllocator::DestroyImage(m_Image, m_Allocation);
}

void VulkanImage::CreateImageView()
{
	ReleaseImageView();
	m_DefaultImageView = GetImageView({ 0, 0 });
}

VkImageView VulkanImage::GetImageView(const ImageView& viewInfo)
{
	auto it = m_Views.find(viewInfo);
	if (it != m_Views.end())
		return it->second;

	VkImageView& imageView = m_Views[viewInfo];
	VkImageViewCreateInfo viewCI{};
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = m_Image;
	viewCI.format = m_VulkanFormat;
	viewCI.viewType = ImageTypeToVulkanImageViewType(m_Specs.Type, m_Specs.bIsCube);
	viewCI.subresourceRange.aspectMask = GetImageAspectFlags(m_VulkanFormat);
	viewCI.subresourceRange.baseMipLevel = viewInfo.MipLevel;
	viewCI.subresourceRange.baseArrayLayer = viewInfo.Layer;
	viewCI.subresourceRange.levelCount = m_Specs.MipsCount;
	viewCI.subresourceRange.layerCount = m_Specs.bIsCube ? 6 : 1;
	VK_CHECK(vkCreateImageView(m_Device, &viewCI, nullptr, &imageView));

	return imageView;
}

void VulkanImage::ReleaseImageView()
{
	for (auto& view : m_Views)
		vkDestroyImageView(m_Device, view.second, nullptr);

	m_Views.clear();
	m_DefaultImageView = VK_NULL_HANDLE;
}
