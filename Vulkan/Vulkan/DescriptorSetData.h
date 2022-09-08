#pragma once

#include "VulkanImage.h"
#include "VulkanSampler.h"
#include "VulkanBuffer.h"

#include <vector>

class DescriptorSetData
{
// Additional Structs
public:
	struct ImageBinding
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View{};
		VkSampler Sampler = VK_NULL_HANDLE;

		ImageBinding() = default;
		ImageBinding(const VulkanImage* image) : Image(image->GetImage()), View(image->GetVulkanImageView()) {}
		ImageBinding(const VulkanImage* image, const ImageView& view) : Image(image->GetImage()), View(image->GetVulkanImageView(view)) {}
		ImageBinding(const VulkanImage* image, const ImageView& view, const VulkanSampler* sampler)
			: Image(image->GetImage())
			, View(image->GetVulkanImageView(view))
			, Sampler(sampler->GetVulkanSampler()) {}
		ImageBinding(const VulkanImage* image, const VulkanSampler* sampler)
			: Image(image->GetImage())
			, View(image->GetVulkanImageView())
			, Sampler(sampler ? sampler->GetVulkanSampler() : VK_NULL_HANDLE) {}

		bool operator!=(const ImageBinding& other) const
		{
			return Image != other.Image || View != other.View || Sampler != other.Sampler;
		}

		friend bool operator!=(const std::vector<ImageBinding>& left, const std::vector<ImageBinding>& right)
		{
			if (left.size() != right.size())
				return true;

			for (std::uint32_t i = 0; i < left.size(); i++)
			{
				if (left[i] != right[i])
					return true;
			}
			return false;
		}
	};

	struct BufferBinding
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		size_t Offset = 0;
		size_t Range = size_t(-1);

		BufferBinding() = default;
		BufferBinding(const VulkanBuffer* buffer) : Buffer(buffer->GetVulkanBuffer()) {}
		BufferBinding(const VulkanBuffer* buffer, size_t offset, size_t range) : Buffer(buffer->GetVulkanBuffer()), Offset(offset), Range(range) {}

		bool operator != (const BufferBinding& other) const
		{
			return Buffer != other.Buffer || Offset != other.Offset || Range != other.Range;
		}

		friend bool operator!=(const std::vector<BufferBinding>& left, const std::vector<BufferBinding>& right)
		{
			if (left.size() != right.size())
				return true;

			for (std::uint32_t i = 0; i < left.size(); i++)
			{
				if (left[i] != right[i])
					return true;
			}
			return false;
		}
	};

	struct Binding
	{
		std::vector<ImageBinding> ImageBindings = { {} };
		std::vector<BufferBinding> BufferBindings = { {} };
	};

public:
	const std::unordered_map<uint32_t, Binding>& GetBindings() const { return m_Bindings; }
	bool IsDirty() const { return m_bDirty; }
	void OnFlushed() { m_bDirty = false; }

	void SetArg(std::uint32_t idx, const VulkanBuffer* buffer);
	void SetArg(std::uint32_t idx, const VulkanBuffer* buffer, std::size_t offset, std::size_t size);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanBuffer*>& buffers);

	void SetArg(std::uint32_t idx, const VulkanImage* image);
	void SetArg(std::uint32_t idx, const VulkanImage* image, const ImageView& imageView);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews);

	void SetArg(std::uint32_t idx, const VulkanImage* image, const VulkanSampler* sampler);
	void SetArg(std::uint32_t idx, const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers);

private:
	std::unordered_map<uint32_t, Binding> m_Bindings; // Binding -> Data
	bool m_bDirty = true;
};