#pragma once

#include "../Core/EnumUtils.h"
#include <glm/glm.hpp>

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
    R8G8B8_UNorm,
    R8G8B8_UNorm_SRGB,
    R8G8B8_UInt,
    R8G8B8_SNorm,
    R8G8B8_SInt,
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
    R8G8_UNorm_SRGB,
    R8G8_UInt,
    R8G8_SNorm,
    R8G8_SInt,
    R16_Float,
    D16_UNorm,
    R16_UNorm,
    R16_UInt,
    R16_SNorm,
    R16_SInt,
    R8_UNorm_SRGB,
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

struct ImageLayout
{
    ImageLayoutType LayoutType = ImageLayoutType::Unknown;
    ImageReadAccess ReadAccessFlags = ImageReadAccess::None;

    ImageLayout() = default;
    ImageLayout(ImageLayoutType type) : LayoutType(type) {}
    ImageLayout(ImageReadAccess readAccessFlags) : LayoutType(ImageLayoutType::ReadOnly), ReadAccessFlags(readAccessFlags) {}

    bool operator==(const ImageLayout& other) const { return LayoutType == other.LayoutType && ReadAccessFlags == other.ReadAccessFlags; }
    bool operator!=(const ImageLayout& other) const { return !(*this == other); }
};

struct ImageView
{
    // MipLevel Index of start mip level.Base mip level is 0.
    // MipLevelsCount - Number of mip levels starting from start_mip.

    uint32_t MipLevel = 0;
    uint32_t MipLevels = 1;
    uint32_t Layer = 0;

    bool operator== (const ImageView& other) const noexcept
    {
        return MipLevel == other.MipLevel && Layer == other.Layer && MipLevels == other.MipLevels;
    }
    bool operator!= (const ImageView& other) const noexcept
    {
        return !((*this) == other);
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

enum class BufferUsage
{
    None                            = 0,
    TransferSrc                     = 1 << 0,
    TransferDst                     = 1 << 1,
    UniformTexelBuffer              = 1 << 2,
    StorageTexelBuffer              = 1 << 3,
    UniformBuffer                   = 1 << 4,
    StorageBuffer                   = 1 << 5,
    IndexBuffer                     = 1 << 6,
    VertexBuffer                    = 1 << 7,
    IndirectBuffer                  = 1 << 8,
    RayTracing                      = 1 << 9,
    AccelerationStructure           = 1 << 10,
    AccelerationStructureBuildInput = 1 << 11
};
DECLARE_FLAGS(BufferUsage);

enum class BufferLayoutType
{
    Unknown,
    ReadOnly,
    CopyDest,
    StorageBuffer,
    AccelerationStructure ///< For DXR only.
};

enum class BufferReadAccess
{
    None                = 0,
    CopySource          = 1 << 0,
    Vertex              = 1 << 1,
    Index               = 1 << 2,
    Uniform             = 1 << 3,
    IndirectArgument    = 1 << 4,
    PixelShaderRead     = 1 << 5,
    NonPixelShaderRead  = 1 << 6
};
DECLARE_FLAGS(BufferReadAccess);

struct BufferLayout
{
    BufferLayoutType LayoutType;
    BufferReadAccess ReadAccessFlags;

    BufferLayout() : LayoutType(BufferLayoutType::Unknown), ReadAccessFlags(BufferReadAccess::None) {}
    BufferLayout(BufferLayoutType layoutType) : LayoutType(layoutType), ReadAccessFlags(BufferReadAccess::None) {}
    BufferLayout(BufferReadAccess readAccessFlags) : LayoutType(BufferLayoutType::ReadOnly), ReadAccessFlags(readAccessFlags) {}

    bool operator== (const BufferLayout& other) const
    {
        return LayoutType == other.LayoutType && ReadAccessFlags == other.ReadAccessFlags;
    }

    bool operator!= (const BufferLayout& other) const
    {
        return !(*this == other);
    }
};

enum class BlendOperation
{
    Add,
    Substract,
    ReverseSubstract,
    Min,
    Max
};

enum class BlendFactor
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    AlphaSaturate,
    Src1Color,
    OneMinusSrc1Color,
    Src1Alpha,
    OneMinusSrc1Alpha
};

struct BlendState
{
    BlendState(BlendOperation blendOp = BlendOperation::Add, BlendFactor blendSrc = BlendFactor::One, BlendFactor blendDst = BlendFactor::Zero,
        BlendOperation blendOpAlpha = BlendOperation::Add, BlendFactor blendSrcAlpha = BlendFactor::One, BlendFactor blendDstAlpha = BlendFactor::Zero)
        : BlendOp(blendOp)
        , BlendSrc(blendSrc)
        , BlendDst(blendDst)
        , BlendOpAlpha(blendOpAlpha)
        , BlendSrcAlpha(blendSrcAlpha)
        , BlendDstAlpha(blendDstAlpha) {}

    BlendOperation BlendOp;
    BlendFactor BlendSrc;
    BlendFactor BlendDst;

    BlendOperation BlendOpAlpha;
    BlendFactor BlendSrcAlpha;
    BlendFactor BlendDstAlpha;
};

enum class FilterMode
{
    Point,
    Bilinear,
    Trilinear,
    Anisotropic
};

enum class AddressMode
{
    Wrap,
    Mirror,
    Clamp,
    ClampToOpaqueBlack,
    ClampToOpaqueWhite,
    MirrorOnce
};

enum class CompareOperation
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class Topology
{
    Triangles,
    Lines,
    Points
};

enum class CullMode
{
    None,
    Front,
    Back,
    FrontAndBack
};

struct BufferImageCopy
{
    // Buffer offset, in bytes.
    size_t BufferOffset = 0;

    /**
     * Buffer row length, in texels. Used to interpret the buffer as an image.\n
     * If value is 0, image rows should be tightly packed.\n
     * If value is non-zero, it specifies the width of image buffer is interpreted as.
     */
    uint32_t BufferRowLength = 0;

    /**
     * Buffer image height, in texels. Used to interpret the buffer as an image.\n
     * If value is 0, image 2D layers should be tightly packed.\n
     * If value is non-zero, it specifies the height of 2D layer image buffer is interpreted as.
     * Must be 0 for non-3D images.
     */
    uint32_t BufferImageHeight = 0;

    // Specifies image mip level.
    uint32_t ImageMipLevel = 0;

    // Specifies image first array layer.
    std::uint32_t ImageArrayLayer = 0;

    // Specifies number of image array layers.
    std::uint32_t ImageArrayLayers = 1;

    // Specifies offset of image region in pixels.
    glm::ivec3 ImageOffset;

    // Specifies extent of image region in pixels.
    glm::uvec3 ImageExtent;
};

// Returns bits
static uint32_t GetImageFormatBPP(ImageFormat format)
{
	switch (format)
	{
        case ImageFormat::R32G32B32A32_Float :      return 4 * 32;
        case ImageFormat::R32G32B32A32_UInt :       return 4 * 32;
        case ImageFormat::R32G32B32A32_SInt :       return 4 * 32;
        case ImageFormat::R32G32B32_Float :         return 3 * 32;
        case ImageFormat::R32G32B32_UInt :          return 3 * 32;
        case ImageFormat::R32G32B32_SInt :          return 3 * 32;
        case ImageFormat::R16G16B16A16_Float :      return 4 * 16;
        case ImageFormat::R16G16B16A16_UNorm :      return 4 * 16;
        case ImageFormat::R16G16B16A16_UInt :       return 4 * 16;
        case ImageFormat::R16G16B16A16_SNorm :      return 4 * 16;
        case ImageFormat::R16G16B16A16_SInt :       return 4 * 16;
        case ImageFormat::R32G32_Float :            return 2 * 32;
        case ImageFormat::R32G32_UInt :             return 2 * 32;
        case ImageFormat::R32G32_SInt :             return 2 * 32;
        case ImageFormat::D32_Float_S8X24_UInt :    return 64;
        case ImageFormat::R10G10B10A2_UNorm :       return 32;
        case ImageFormat::R10G10B10A2_UInt :        return 32;
        case ImageFormat::R11G11B10_Float :         return 32;
        case ImageFormat::R8G8B8A8_UNorm :          return 4 * 8;
        case ImageFormat::R8G8B8A8_UNorm_SRGB :     return 4 * 8;
        case ImageFormat::R8G8B8A8_UInt :           return 4 * 8;
        case ImageFormat::R8G8B8A8_SNorm :          return 4 * 8;
        case ImageFormat::R8G8B8A8_SInt :           return 4 * 8;
        case ImageFormat::R8G8B8_UNorm :            return 3 * 8;
        case ImageFormat::R8G8B8_UNorm_SRGB :       return 3 * 8;
        case ImageFormat::R8G8B8_UInt :             return 3 * 8;
        case ImageFormat::R8G8B8_SNorm :            return 3 * 8;
        case ImageFormat::R8G8B8_SInt :             return 3 * 8;
        case ImageFormat::R16G16_Float :            return 2 * 16;
        case ImageFormat::R16G16_UNorm :            return 2 * 16;
        case ImageFormat::R16G16_UInt :             return 2 * 16;
        case ImageFormat::R16G16_SNorm :            return 2 * 16;
        case ImageFormat::R16G16_SInt :             return 2 * 16;
        case ImageFormat::D32_Float :               return 32;
        case ImageFormat::R32_Float :               return 32;
        case ImageFormat::R32_UInt :                return 32;
        case ImageFormat::R32_SInt :                return 32;
        case ImageFormat::D24_UNorm_S8_UInt :       return 32;
        case ImageFormat::R8G8_UNorm :              return 2 * 8;
        case ImageFormat::R8G8_UNorm_SRGB :         return 2 * 8;
        case ImageFormat::R8G8_UInt :               return 2 * 8;
        case ImageFormat::R8G8_SNorm :              return 2 * 8;
        case ImageFormat::R8G8_SInt :               return 2 * 8;
        case ImageFormat::R16_Float :               return 16;
        case ImageFormat::D16_UNorm :               return 16;
        case ImageFormat::R16_UNorm :               return 16;
        case ImageFormat::R16_UInt :                return 16;
        case ImageFormat::R16_SNorm :               return 16;
        case ImageFormat::R16_SInt :                return 16;
        case ImageFormat::R8_UNorm_SRGB:            return 8;
        case ImageFormat::R8_UNorm :                return 8;
        case ImageFormat::R8_UInt :                 return 8;
        case ImageFormat::R8_SNorm :                return 8;
        case ImageFormat::R8_SInt :                 return 8;
        case ImageFormat::R9G9B9E5_SharedExp :      return 32;
        case ImageFormat::R8G8_B8G8_UNorm :         return 4 * 8;
        case ImageFormat::G8R8_G8B8_UNorm :         return 4 * 8;
        case ImageFormat::BC1_UNorm :               return 4;
        case ImageFormat::BC1_UNorm_SRGB :          return 4;
        case ImageFormat::BC2_UNorm :               return 8;
        case ImageFormat::BC2_UNorm_SRGB :          return 8;
        case ImageFormat::BC3_UNorm :               return 8;
        case ImageFormat::BC3_UNorm_SRGB :          return 8;
        case ImageFormat::BC4_UNorm :               return 4;
        case ImageFormat::BC4_SNorm :               return 4;
        case ImageFormat::BC5_UNorm :               return 8;
        case ImageFormat::BC5_SNorm :               return 8;
        case ImageFormat::B5G6R5_UNorm :            return 16;
        case ImageFormat::B5G5R5A1_UNorm :          return 16;
        case ImageFormat::B8G8R8A8_UNorm :          return 4 * 8;
        case ImageFormat::B8G8R8A8_UNorm_SRGB :     return 4 * 8;
        case ImageFormat::BC6H_UFloat16 :           return 8;
        case ImageFormat::BC6H_SFloat16 :           return 8;
        case ImageFormat::BC7_UNorm :               return 8;
        case ImageFormat::BC7_UNorm_SRGB:           return 8;
	}
    assert(!"Unknown format");
	return 0;
}

static uint32_t CalculateMipCount(uint32_t width, uint32_t height)
{
    return (uint32_t)std::floor(std::log2(glm::max(width, height))) + 1;
}

static uint32_t CalculateMipCount(glm::uvec2 size)
{
    return (uint32_t)std::floor(std::log2(glm::max(size.x, size.y))) + 1;
}

static uint32_t CalculateMipCount(const glm::uvec3& size)
{
    uint32_t maxSide = glm::max(size.x, size.y);
    maxSide = glm::max(maxSide, size.z);
    return (uint32_t)std::floor(std::log2(maxSide)) + 1;
}

static size_t CalculateImageMemorySize(ImageFormat format, uint32_t width, uint32_t height)
{
    return ((size_t)GetImageFormatBPP(format) / 8) * (size_t)width * (size_t)height;
}

