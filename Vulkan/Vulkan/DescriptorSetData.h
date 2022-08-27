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
		const VulkanImage* Image = nullptr;
		ImageView View{};
		const VulkanSampler* Sampler = nullptr;

		ImageBinding() = default;
		ImageBinding(const VulkanImage* image) : Image(image), View(image->GetImageView()) {}
		ImageBinding(const VulkanImage* image, const ImageView& view) : Image(image), View(view) {}
		ImageBinding(const ImageView& view) : View(view) {}
		ImageBinding(const VulkanImage* image, const ImageView& view, const VulkanSampler* sampler) : Image(image), View(view), Sampler(sampler) {}
		ImageBinding(const VulkanImage* image, const VulkanSampler* sampler) : Image(image), Sampler(sampler) {}
		ImageBinding(const VulkanSampler* sampler) : Sampler(sampler) {}

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
		const VulkanBuffer* Buffer = nullptr;
		size_t Offset = 0;
		size_t Range = size_t(-1);

		BufferBinding() = default;
		BufferBinding(const VulkanBuffer* buffer) : Buffer(buffer) {}
		BufferBinding(const VulkanBuffer* buffer, size_t offset, size_t range) : Buffer(buffer), Offset(offset), Range(range) {}

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

	void SetArg(std::uint32_t idx, const VulkanSampler* sampler);
	void SetArg(std::uint32_t idx, const VulkanImage* image, const VulkanSampler* sampler);
	void SetArg(std::uint32_t idx, const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers);
	void SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers);

private:
	std::unordered_map<uint32_t, Binding> m_Bindings; // Binding -> Data
	bool m_bDirty = true;
};