#pragma once

#include "Vulkan.h"
#include "VulkanCommandManager.h"
#include "VulkanAllocator.h"
#include "../Core/EnumUtils.h"

#include <unordered_map>

#include <glm/glm.hpp>

enum class ImageFormat
{
    Unknown,
    R32G32B32A32_Float,
    R32G32B32A32_UInt,
    R32G32B32A32_SInt,
    R32G32B32_Float,
    R32G32B32_UInt,
    R32G32B32_SInt,
    R16G16B16A16_Float,
    R16G16B16A16_UNorm,
    R16G16B16A16_UInt,
    R16G16B16A16_SNorm,
    R16G16B16A16_SInt,
    R32G32_Float,
    R32G32_UInt,
    R32G32_SInt,
    D32_Float_S8X24_UInt,
    R10G10B10A2_UNorm,
    R10G10B10A2_UInt,
    R11G11B10_Float,
    R8G8B8A8_UNorm,
    R8G8B8A8_UNorm_SRGB,
    R8G8B8A8_UInt,
    R8G8B8A8_SNorm,
    R8G8B8A8_SInt,
    R16G16_Float,
    R16G16_UNorm,
    R16G16_UInt,
    R16G16_SNorm,
    R16G16_SInt,
    D32_Float,
    R32_Float,
    R32_UInt,
    R32_SInt,
    D24_UNorm_S8_UInt,
    R8G8_UNorm,
    R8G8_UInt,
    R8G8_SNorm,
    R8G8_SInt,
    R16_Float,
    D16_UNorm,
    R16_UNorm,
    R16_UInt,
    R16_SNorm,
    R16_SInt,
    R8_UNorm,
    R8_UInt,
    R8_SNorm,
    R8_SInt,
    R9G9B9E5_SharedExp,
    R8G8_B8G8_UNorm,
    G8R8_G8B8_UNorm,
    BC1_UNorm,
    BC1_UNorm_SRGB,
    BC2_UNorm,
    BC2_UNorm_SRGB,
    BC3_UNorm,
    BC3_UNorm_SRGB,
    BC4_UNorm,
    BC4_SNorm,
    BC5_UNorm,
    BC5_SNorm,
    B5G6R5_UNorm,
    B5G5R5A1_UNorm,
    B8G8R8A8_UNorm,
    B8G8R8A8_UNorm_SRGB,
    BC6H_UFloat16,
    BC6H_SFloat16,
    BC7_UNorm,
    BC7_UNorm_SRGB
};

enum class ImageUsage
{
    None                    = 0,
    TransferSrc             = 1 << 0,
    TransferDst             = 1 << 1,
    Sampled                 = 1 << 2,
    Storage                 = 1 << 3,
    ColorAttachment         = 1 << 4,
    DepthStencilAttachment  = 1 << 5,
    TransientAttachment     = 1 << 6,
    InputAttachment         = 1 << 7
};
DECLARE_FLAGS(ImageUsage);

enum class ImageLayoutType
{
    Unknown,
    ReadOnly,
    CopyDest,
    RenderTarget,
    StorageImage,
    DepthStencilWrite,
    Present
};

enum class ImageReadAccess
{
    None               = 0,
    CopySource         = 1 << 0,
    DepthStencilRead   = 1 << 1,
    PixelShaderRead    = 1 << 2,
    NonPixelShaderRead = 1 << 3
};
DECLARE_FLAGS(ImageReadAccess);

enum class ImageType
{
    Type1D,
    Type2D,
    Type3D
};

struct ImageLayout
{
    ImageLayoutType LayoutType = ImageLayoutType::Unknown;
    ImageReadAccess ReadAccessFlags = ImageReadAccess::None;

    ImageLayout() = default;
    ImageLayout(ImageLayoutType type) : LayoutType(type) {}
    ImageLayout(ImageReadAccess readAccessFlags) : LayoutType(ImageLayoutType::ReadOnly), ReadAccessFlags(readAccessFlags) {}
};

enum class SamplesCount
{
    Samples1  = 1,
    Samples2  = 2,
    Samples4  = 4,
    Samples8  = 8,
    Samples16 = 16,
    Samples32 = 32,
    Samples64 = 64
};

enum class MemoryType
{
    Gpu,         /**<
                 * Device-local GPU (video) memory.\n
                 * This is D3D12_HEAP_TYPE_DEFAULT in Direct3D 12.\n
                 * This memory can't be mapped.
                 */

    Cpu,        /**<
                 * CPU (system) memory. Use it for staging resources as transfer source.\n
                 * This is D3D12_HEAP_TYPE_UPLOAD in Direct3D 12.\n
                 * This memory can be mapped for writing.
                 */


    CpuToGpu,   /**<
                 * Memory that is both mappable on host and preferably fast to access by GPU. Use it for GPU resources which are written by CPU every frame.\n
                 * This is D3D12_HEAP_TYPE_UPLOAD in Direct3D 12.\n
                 * This memory can be mapped for writing.
                 */

    GpuToCpu    /**< 
                 * Memory mappable on host and cached. Use it for GPU resources which are read by CPU every frame.\n
                 * This is D3D12_HEAP_TYPE_READBACK in Direct3D 12.\n
                 * This memory can be mapped for reading.
                 */
};

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

static uint32_t CalculateMipCount(uint32_t width, uint32_t height)
{
    return (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
}

static uint32_t CalculateMipCount(uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t maxSide = std::max(width, height);
    maxSide = std::max(maxSide, depth);
    return (uint32_t)std::floor(std::log2(maxSide)) + 1;
}

struct ImageView
{
    uint32_t MipLevel = 0;
    uint32_t Layer = 0;

    bool operator== (const ImageView& other) const noexcept
    {
        return MipLevel == other.MipLevel && Layer == other.Layer;
    }
};

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
    ~VulkanImage();

    bool HasUsage(ImageUsage usage) const { return HasFlags(m_Specs.Usage, usage); }
    VkImage GetImage() const { return m_Image; }
    VkImageView GetImageView() const { return m_DefaultImageView; }
    VkImageView GetImageView(const ImageView& viewInfo);

    const glm::uvec3& GetSize() const { return m_Specs.Size; }
    ImageFormat GetFormat() const { return m_Specs.Format; }
    ImageUsage GetUsage() const { return m_Specs.Usage; }
    ImageLayout GetLayout() const { return m_Specs.Layout; }
    ImageType GetType() const { return m_Specs.Type; }
    SamplesCount GetSamplesCount() const { return m_Specs.SamplesCount; }
    MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
    uint32_t GetMipsCount() const { return m_Specs.MipsCount; }
    bool IsCube() const { return m_Specs.bIsCube; }


    void* Map();
    void Unmap();

    void Read(void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout);
    void GenerateMips(VulkanCommandBuffer* cmd, ImageLayout initialLayout, ImageLayout finalLayout);

    void TransitionToLayout(VulkanCommandBuffer* cmd, ImageLayout oldLayout, ImageLayout newLayout);

    VkFormat GetVulkanFormat() const { return m_VulkanFormat; }

private:
    void CreateImage();
    void ReleaseImage();
    void CreateImageView();
    void ReleaseImageView();

private:
    std::unordered_map<ImageView, VkImageView> m_Views;

    ImageSpecifications m_Specs;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkImage m_Image = VK_NULL_HANDLE;
    VkImageView m_DefaultImageView = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    VkFormat m_VulkanFormat = VK_FORMAT_UNDEFINED;
    bool m_bOwns = true;
};
