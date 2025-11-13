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
	struct VulkanImage final : GPUTexture
	{
		// RHI interface implementation
		void Init(GPUDevice* device, const TextureInfo& info) override;
		void Destroy() override;
		void UploadData(const void* data) override;
		void TransitionLayout(void* cmdBuffer, TextureLayout newLayout) override;
		void GenerateMipmaps(void* cmdBuffer) override;

		// Vulkan-specific initialization (backward compatibility)
		void Init(VulkanDevice* device, TextureInfo& info);

		// Constructors
		VulkanImage() = default;
		VulkanImage(VulkanDevice* device, TextureInfo& info);
		VulkanImage(VulkanDevice* device, VkImage image);

		// C++23: Delete copy, allow move
		VulkanImage(const VulkanImage&) = delete;
		VulkanImage& operator=(const VulkanImage&) = delete;
		VulkanImage(VulkanImage&&) noexcept = default;
		VulkanImage& operator=(VulkanImage&&) noexcept = default;

		// ~VulkanImage() override { Destroy(); }

		// Vulkan-specific helpers
		void MakeSampleable(VkCommandBuffer cmd);
		void PrepareAsRenderTarget(VkCommandBuffer cmd);
		void CopyFrom(VkCommandBuffer cmd, const VulkanImage& src) const;
		void UploadTextureToGPU(const void* data, const TextureInfo& texInfo);
		void CreateImageView(VkFormat format);
		void FillSubresourceInfo();
		void Transition(VkCommandBuffer cmd, TextureLayout newLayout);  // Single-parameter version
		void Transition(VkCommandBuffer cmd, TextureLayout oldLayout, TextureLayout newLayout);  // Two-parameter version

		static void GenerateMipmaps(VkCommandBuffer cmd, const VulkanImage& image);

		// Fallback image handlers
		static VulkanImage CreateCheckerboardTexture(VulkanDevice& device);
		static VulkanImage CreateDefaultNormalMap(VulkanDevice& device);

		// Vulkan-specific accessors
		[[nodiscard]] VkImage GetVkImage() const noexcept { return image; }
		[[nodiscard]] VkImageView GetVkImageView() const noexcept { return imageView; }
		[[nodiscard]] VkFormat GetVkFormat() const noexcept { return imageFormat; }

		// Public Vulkan handles for compatibility
		VkImage                 image = VK_NULL_HANDLE;
		VkImageView             imageView = VK_NULL_HANDLE;
		VmaAllocation           allocation = VK_NULL_HANDLE;
		VulkanDevice*           device = nullptr;
		VmaAllocationInfo       allocInfo = {};
		VkFormat                imageFormat = VK_FORMAT_UNDEFINED;
		TextureLayout           imageLayout = TextureLayout::Unknown;
		VkImageSubresourceRange subresourceRange = {};
		TextureInfo             textureInfo = {};
	};

	struct VulkanImageView
	{
		VkImageView imageView = VK_NULL_HANDLE;
		VulkanImage* image = nullptr;

		VulkanImageView(VulkanImage* device, TextureViewInfo& texViewInfo);
		~VulkanImageView();
	};

	/// Vulkan implementation of GPUSampler
	/// Marked final to enable compiler devirtualization (C++23)
	struct VulkanSampler final : GPUSampler
	{
		// RHI interface implementation
		void Init(GPUDevice* device, const SamplerDesc& inputDesc) override;
		void Destroy() override;

		// Vulkan-specific initialization (backward compatibility)
		void Init(VulkanDevice* device, const SamplerDesc& inputDesc);

		// Constructors
		VulkanSampler() = default;
		VulkanSampler(VulkanDevice* device, const SamplerDesc& desc); // Implemented in .cpp

		// C++23: Delete copy, allow move
		VulkanSampler(const VulkanSampler&) = delete;
		VulkanSampler& operator=(const VulkanSampler&) = delete;
		VulkanSampler(VulkanSampler&&) noexcept = default;
		VulkanSampler& operator=(VulkanSampler&&) noexcept = default;

		// ~VulkanSampler() override { Destroy(); }

		// Vulkan-specific accessor
		[[nodiscard]] VkSampler GetVkSampler() const noexcept { return sampler; }

		// Public Vulkan handles for compatibility
		VkSampler sampler = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		SamplerDesc desc = {};
	};

} // namespace Renderer
