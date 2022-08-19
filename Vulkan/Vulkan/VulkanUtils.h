#pragma once

#include "Vulkan.h"
#include "VulkanImage.h"

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

static inline VkPrimitiveTopology TopologyToVulkan(Topology topology)
{
	switch (topology)
	{
	case Topology::Triangles:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case Topology::Lines:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case Topology::Points:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	default:
		assert(false);
		break;
	}

	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

inline VkCompareOp CompareOpToVulkan(CompareOperation compareOp)
{
	switch (compareOp)
	{
	case CompareOperation::Never:
		return VK_COMPARE_OP_NEVER;
	case CompareOperation::Less:
		return VK_COMPARE_OP_LESS;
	case CompareOperation::Equal:
		return VK_COMPARE_OP_EQUAL;
	case CompareOperation::LessEqual:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case CompareOperation::Greater:
		return VK_COMPARE_OP_GREATER;
	case CompareOperation::NotEqual:
		return VK_COMPARE_OP_NOT_EQUAL;
	case CompareOperation::GreaterEqual:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case CompareOperation::Always:
		return VK_COMPARE_OP_ALWAYS;
	default:
		assert(false);
		return VK_COMPARE_OP_NEVER;
	}
}

inline VkImageLayout GetVulkanLayout(ImageLayout imageLayout)
{
	switch (imageLayout.LayoutType)
	{
		case ImageLayoutType::ReadOnly:
		{
			ImageReadAccess read_access_flags = imageLayout.ReadAccessFlags;
			if (read_access_flags == ImageReadAccess::None)
			{
				assert(!"Unknown read access");
				return VK_IMAGE_LAYOUT_UNDEFINED;
			}
			if (HasFlags(ImageReadAccess::DepthStencilRead, read_access_flags))
			{
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			if (HasFlags(ImageReadAccess::PixelShaderRead | ImageReadAccess::NonPixelShaderRead, read_access_flags))
			{
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			if (HasFlags(ImageReadAccess::CopySource, read_access_flags))
			{
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			assert(!"Unknown read access");
			return VK_IMAGE_LAYOUT_UNDEFINED;
		}
		case ImageLayoutType::CopyDest:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case ImageLayoutType::RenderTarget:			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case ImageLayoutType::StorageImage:			return VK_IMAGE_LAYOUT_GENERAL;
		case ImageLayoutType::DepthStencilWrite:	return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case ImageLayoutType::Present:				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		case ImageLayoutType::Unknown:				return VK_IMAGE_LAYOUT_UNDEFINED;
		default: assert(!"Unknown layout");			return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

inline VkSampleCountFlagBits GetVulkanSamplesCount(SamplesCount samples)
{
	switch (samples)
	{
		case SamplesCount::Samples1:
			return VK_SAMPLE_COUNT_1_BIT;
		case SamplesCount::Samples2:
			return VK_SAMPLE_COUNT_2_BIT;
		case SamplesCount::Samples4:
			return VK_SAMPLE_COUNT_4_BIT;
		case SamplesCount::Samples8:
			return VK_SAMPLE_COUNT_8_BIT;
		case SamplesCount::Samples16:
			return VK_SAMPLE_COUNT_16_BIT;
		case SamplesCount::Samples32:
			return VK_SAMPLE_COUNT_32_BIT;
		case SamplesCount::Samples64:
			return VK_SAMPLE_COUNT_64_BIT;
		default:
			assert(!"Uknown samples");
			return VK_SAMPLE_COUNT_1_BIT;
	}
}

inline VkImageUsageFlags ImageUsageToVulkan(ImageUsage usage)
{
	VkImageUsageFlags res = 0;

	if (HasFlags(usage, ImageUsage::TransferSrc))
	{
		res |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		usage &= ~ImageUsage::TransferSrc;
	}

	if (HasFlags(usage, ImageUsage::TransferDst))
	{
		res |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		usage &= ~ImageUsage::TransferDst;
	}

	if (HasFlags(usage, ImageUsage::Sampled))
	{
		res |= VK_IMAGE_USAGE_SAMPLED_BIT;
		usage &= ~ImageUsage::Sampled;
	}

	if (HasFlags(usage, ImageUsage::Storage))
	{
		res |= VK_IMAGE_USAGE_STORAGE_BIT;
		usage &= ~ImageUsage::Storage;
	}

	if (HasFlags(usage, ImageUsage::ColorAttachment))
	{
		res |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		usage &= ~ImageUsage::ColorAttachment;
	}

	if (HasFlags(usage, ImageUsage::DepthStencilAttachment))
	{
		res |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		usage &= ~ImageUsage::DepthStencilAttachment;
	}

	if (HasFlags(usage, ImageUsage::TransientAttachment))
	{
		res |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		usage &= ~ImageUsage::TransientAttachment;
	}

	if (HasFlags(usage, ImageUsage::InputAttachment))
	{
		res |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		usage &= ~ImageUsage::InputAttachment;
	}

	assert(usage == ImageUsage::None);

	return res;
}

inline VkFormat ImageFormatToVulkan(ImageFormat format)
{
	switch (format)
	{
		case ImageFormat::Unknown: return VK_FORMAT_UNDEFINED;
		case ImageFormat::R32G32B32A32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ImageFormat::R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
		case ImageFormat::R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
		case ImageFormat::R32G32B32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
		case ImageFormat::R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
		case ImageFormat::R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
		case ImageFormat::R16G16B16A16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case ImageFormat::R16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
		case ImageFormat::R16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
		case ImageFormat::R16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
		case ImageFormat::R16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
		case ImageFormat::R32G32_Float: return VK_FORMAT_R32G32_SFLOAT;
		case ImageFormat::R32G32_UInt: return VK_FORMAT_R32G32_UINT;
		case ImageFormat::R32G32_SInt: return VK_FORMAT_R32G32_SINT;
		case ImageFormat::D32_Float_S8X24_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case ImageFormat::R10G10B10A2_UNorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case ImageFormat::R10G10B10A2_UInt: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case ImageFormat::R11G11B10_Float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case ImageFormat::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case ImageFormat::R8G8B8A8_UNorm_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case ImageFormat::R8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
		case ImageFormat::R8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
		case ImageFormat::R8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
		case ImageFormat::R16G16_Float: return VK_FORMAT_R16G16_SFLOAT;
		case ImageFormat::R16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
		case ImageFormat::R16G16_UInt: return VK_FORMAT_R16G16_UINT;
		case ImageFormat::R16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
		case ImageFormat::R16G16_SInt: return VK_FORMAT_R16G16_SINT;
		case ImageFormat::D32_Float: return VK_FORMAT_D32_SFLOAT;
		case ImageFormat::R32_Float: return VK_FORMAT_R32_SFLOAT;
		case ImageFormat::R32_UInt: return VK_FORMAT_R32_UINT;
		case ImageFormat::R32_SInt: return VK_FORMAT_R32_SINT;
		case ImageFormat::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
		case ImageFormat::R8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case ImageFormat::R8G8_UInt: return VK_FORMAT_R8G8_UINT;
		case ImageFormat::R8G8_SNorm: return VK_FORMAT_R8G8_SNORM;
		case ImageFormat::R8G8_SInt: return VK_FORMAT_R8G8_SINT;
		case ImageFormat::R16_Float: return VK_FORMAT_R16_SFLOAT;
		case ImageFormat::D16_UNorm: return VK_FORMAT_D16_UNORM;
		case ImageFormat::R16_UNorm: return VK_FORMAT_R16_UNORM;
		case ImageFormat::R16_UInt: return VK_FORMAT_R16_UINT;
		case ImageFormat::R16_SNorm: return VK_FORMAT_R16_SNORM;
		case ImageFormat::R16_SInt: return VK_FORMAT_R16_SINT;
		case ImageFormat::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case ImageFormat::R8_UInt: return VK_FORMAT_R8_UINT;
		case ImageFormat::R8_SNorm: return VK_FORMAT_R8_SNORM;
		case ImageFormat::R8_SInt: return VK_FORMAT_R8_SINT;
		case ImageFormat::R9G9B9E5_SharedExp: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		case ImageFormat::R8G8_B8G8_UNorm: return VK_FORMAT_B8G8R8G8_422_UNORM;
		case ImageFormat::G8R8_G8B8_UNorm: return VK_FORMAT_G8B8G8R8_422_UNORM;
		case ImageFormat::BC1_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case ImageFormat::BC1_UNorm_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case ImageFormat::BC2_UNorm: return VK_FORMAT_BC2_UNORM_BLOCK;
		case ImageFormat::BC2_UNorm_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
		case ImageFormat::BC3_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case ImageFormat::BC3_UNorm_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
		case ImageFormat::BC4_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case ImageFormat::BC4_SNorm: return VK_FORMAT_BC4_SNORM_BLOCK;
		case ImageFormat::BC5_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case ImageFormat::BC5_SNorm: return VK_FORMAT_BC5_SNORM_BLOCK;
		case ImageFormat::B5G6R5_UNorm: return VK_FORMAT_B5G6R5_UNORM_PACK16;
		case ImageFormat::B5G5R5A1_UNorm: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case ImageFormat::B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
		case ImageFormat::B8G8R8A8_UNorm_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
		case ImageFormat::BC6H_UFloat16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case ImageFormat::BC6H_SFloat16: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case ImageFormat::BC7_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
		case ImageFormat::BC7_UNorm_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
	}
	assert(!"Unknown format");
	return VK_FORMAT_UNDEFINED;
}

inline VkImageType ImageTypeToVulkan(ImageType type)
{
	switch (type)
	{
		case ImageType::Type1D: return VK_IMAGE_TYPE_1D;
		case ImageType::Type2D: return VK_IMAGE_TYPE_2D;
		case ImageType::Type3D: return VK_IMAGE_TYPE_3D;
	}
	assert(!"Unknown type");
	return VK_IMAGE_TYPE_MAX_ENUM;
}

inline VkImageViewType ImageTypeToVulkanImageViewType(ImageType type, bool bIsCube)
{
	if (bIsCube)
		return VK_IMAGE_VIEW_TYPE_CUBE;
	
	switch (type)
	{
		case ImageType::Type1D: return VK_IMAGE_VIEW_TYPE_1D;
		case ImageType::Type2D: return VK_IMAGE_VIEW_TYPE_2D;
		case ImageType::Type3D: return VK_IMAGE_VIEW_TYPE_3D;
	}
	
	assert(!"Unknown type");
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}

inline bool HasDepth(VkFormat format)
{
	static const std::vector<VkFormat> formats =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	return std::find(formats.begin(), formats.end(), format) != std::end(formats);
}

inline bool HasStencil(VkFormat format)
{
	static const std::vector<VkFormat> formats =
	{
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	return std::find(formats.begin(), formats.end(), format) != std::end(formats);
}

inline VkImageAspectFlags GetImageAspectFlags(VkFormat format)
{
	const bool bHasDepth = HasDepth(format);
	const bool bHasStencil = HasStencil(format);

	if (bHasDepth && bHasStencil) return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (bHasDepth) return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (bHasStencil) return VK_IMAGE_ASPECT_STENCIL_BIT;
	else return VK_IMAGE_ASPECT_COLOR_BIT;
}
