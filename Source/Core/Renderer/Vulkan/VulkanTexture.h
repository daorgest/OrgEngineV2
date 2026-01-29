//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include "RendererTypes.h"
#include "RenderInterface.h"
#include "vk_mem_alloc.h"

struct ArenaAllocator;

namespace Renderer
{
	struct VulkanDevice;

	/// Vulkan implementation of GPUTexture
	struct VulkanTexture final : GPUTexture
	{
		// RHI interface implementation
		void Init(GPUDevice* device, const TextureInfo& info) override;
		void Destroy() override;
		void UploadData(const void* data) override;

		// Vulkan-specific initialization (backward compatibility)
		void Init(VulkanDevice* device, TextureInfo& info);

		// Constructors
		VulkanTexture() = default;
		VulkanTexture(VulkanDevice* device, TextureInfo& info);
		VulkanTexture(VulkanDevice* device, VkImage image);

		VulkanTexture(const VulkanTexture&) = delete;
		VulkanTexture& operator=(const VulkanTexture&) = delete;
		VulkanTexture(VulkanTexture&& other) noexcept
		{
			*this = std::move(other);
		}
		VulkanTexture& operator=(VulkanTexture&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();

				// Transfer all handles and state
				image = other.image;
				imageView = other.imageView;
				allocation = other.allocation;
				device = other.device;
				allocInfo = other.allocInfo;
				imageFormat = other.imageFormat;
				imageLayout = other.imageLayout;
				subresourceRange = other.subresourceRange;
				textureInfo = other.textureInfo;
				owns = other.owns;

				// IMPORTANT: Null out the source so its destructor is a no-op
				other.image = VK_NULL_HANDLE;
				other.imageView = VK_NULL_HANDLE;
				other.allocation = VK_NULL_HANDLE;
				other.owns = false;
			}
			return *this;
		}

		~VulkanTexture() override { Destroy(); }

		// Vulkan-specific helpers
		void UploadTextureToGPU(const void* data, const TextureInfo& texInfo);
		void CreateImageView(VkFormat format);
		void FillSubresourceInfo();
		void SetName(const std::string& name) override;

		// Public Vulkan handles for compatibility
		VkImage                 image = VK_NULL_HANDLE;
		VkImageView             imageView = VK_NULL_HANDLE;
		VmaAllocation           allocation = VK_NULL_HANDLE;
		VmaAllocationInfo       allocInfo = {};
		VulkanDevice*           device = nullptr;
		VkFormat                imageFormat = VK_FORMAT_UNDEFINED;
		TextureLayout           imageLayout = TextureLayout::Unknown;
		VkImageSubresourceRange subresourceRange = {};
		TextureInfo             textureInfo = {};
		bool owns = false;
	};

	struct VulkanImageView
	{
		VkImageView imageView = VK_NULL_HANDLE;
		VulkanTexture* image = nullptr;

		VulkanImageView(VulkanTexture* device, TextureViewInfo& texViewInfo);
		~VulkanImageView();
	};


	struct VulkanSampler final : GPUSampler
	{
		VulkanSampler() = default;

		VulkanSampler(VulkanDevice* dev, const SamplerInfo& desc) {
			Init(dev, desc);
		}

		void Init(VulkanDevice* dev, const SamplerInfo& desc);

		void Destroy() const;
		~VulkanSampler() override { Destroy(); }

		VulkanSampler(VulkanSampler&& other) noexcept
		{
			*this = std::move(other);
		}

		VulkanSampler& operator=(VulkanSampler&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();

				sampler = other.sampler;
				device  = other.device;

				other.sampler = VK_NULL_HANDLE;
				other.device  = nullptr;
			}
			return *this;
		}

		// Public Vulkan handles for compatibility
		VkSampler sampler = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
	};

} // namespace Renderer
