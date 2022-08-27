#pragma once

#include "Vulkan.h"
#include "VulkanAllocator.h"
#include "../Renderer/RendererUtils.h"

#include <unordered_map>

#include <glm/glm.hpp>

struct ImageSpecifications
{
    glm::uvec3 Size;
    ImageFormat Format = ImageFormat::Unknown;
    ImageUsage Usage = ImageUsage::None;
    ImageLayout Layout = ImageLayout();
    ImageType Type = ImageType::Type2D;
    SamplesCount SamplesCount = SamplesCount::Samples1;
    MemoryType MemoryType = MemoryType::Gpu;
    uint32_t MipsCount = 1;
    bool bIsCube = false;
};

static uint32_t CalculateMipCount(glm::uvec2 size)
{
    return (uint32_t)std::floor(std::log2(std::max(size.x, size.y))) + 1;
}

static uint32_t CalculateMipCount(const glm::uvec3& size)
{
    uint32_t maxSide = std::max(size.x, size.y);
    maxSide = std::max(maxSide, size.z);
    return (uint32_t)std::floor(std::log2(maxSide)) + 1;
}

template <class T>
inline void HashCombine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
    template<>
    struct hash<ImageView>
    {
        std::size_t operator()(const ImageView& view) const noexcept
        {
            std::size_t result = std::hash<uint32_t>()(view.MipLevel);
            HashCombine(result, view.Layer);

            return result;
        }
    };
}

class VulkanImage
{
public:
    VulkanImage(const ImageSpecifications& specs);
    VulkanImage(VkImage vulkanImage, const ImageSpecifications& specs, bool bOwns);
    virtual ~VulkanImage();

    bool HasUsage(ImageUsage usage) const { return HasFlags(m_Specs.Usage, usage); }
    VkImage GetImage() const { return m_Image; }

    ImageView GetImageView() const { return ImageView{ 0, m_Specs.MipsCount, 0 }; }
    VkImageView GetVulkanImageView() const { return m_DefaultImageView; }
    VkImageView GetVulkanImageView(const ImageView& viewInfo) const;

    const glm::uvec3& GetSize() const { return m_Specs.Size; }
    ImageFormat GetFormat() const { return m_Specs.Format; }
    ImageUsage GetUsage() const { return m_Specs.Usage; }
    ImageLayout GetLayout() const { return m_Specs.Layout; }
    ImageType GetType() const { return m_Specs.Type; }
    SamplesCount GetSamplesCount() const { return m_Specs.SamplesCount; }
    MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
    uint32_t GetMipsCount() const { return m_Specs.MipsCount; }
    uint32_t GetLayersCount() const { return m_Specs.bIsCube ? 6 : 1; }
    bool IsCube() const { return m_Specs.bIsCube; }

    void* Map();
    void Unmap();

    void Read(void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout);

    VkFormat GetVulkanFormat() const { return m_VulkanFormat; }
    VkImage GetVulkanImage() const { return m_Image; }
    VkImageAspectFlags GetDefaultAspectMask() const { return m_AspectMask; }
    VkImageAspectFlags GetTransitionAspectMask(ImageLayout oldLayout, ImageLayout newLayout) const;

private:
    void CreateImage();
    void ReleaseImage();
    void CreateImageView();
    void ReleaseImageView();

private:
    mutable std::unordered_map<ImageView, VkImageView> m_Views; // Mutable by `GetVulkanImageView(const ImageView&)`

    ImageSpecifications m_Specs;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkImage m_Image = VK_NULL_HANDLE;
    VkImageView m_DefaultImageView = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    VkFormat m_VulkanFormat = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags m_AspectMask;
    bool m_bOwns = true;
};
