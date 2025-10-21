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
	struct VulkanImage : GPUTexture
	{
		VkImage                 image = VK_NULL_HANDLE;            // The Vulkan image handle. This is the actual image resource.
		VkImageView             imageView = VK_NULL_HANDLE;        // The image view, used for accessing the image in shaders.
		VmaAllocation           allocation = VK_NULL_HANDLE;        // Memory allocation for the image, managed by VMA (Vulkan Memory Allocator).
		VulkanDevice*           device = nullptr;
		VmaAllocationInfo       allocInfo = {};
		VkFormat                imageFormat = VK_FORMAT_UNDEFINED;
		TextureLayout			imageLayout = TextureLayout::Unknown;
		VkImageSubresourceRange subresourceRange = {};
		TextureInfo				textureInfo;

		VulkanImage() = default;
		VulkanImage(VulkanDevice* device, TextureInfo& info) { Init(device, info); };
		VulkanImage(VulkanDevice* device, VkImage image) : image(image), device(device) {};
		void Init(VulkanDevice* device, TextureInfo& info);
		void Destroy();
		void MakeSampleable(VkCommandBuffer cmd);
		void UploadTextureToGPU(const void* data, const TextureInfo& texInfo);
		void CreateImageView(VkFormat format);
		void FillSubresoruceInfo();
		void Transition(VkCommandBuffer cmd, TextureLayout newLayout);

		// Transitions
		void Transition(VkCommandBuffer cmd, TextureLayout oldLayout, TextureLayout newLayout);
		static void GenerateMipmaps(VkCommandBuffer cmd, const VulkanImage& image);

		// Fallback image handlers
		static VulkanImage CreateCheckerboardTexture(VulkanDevice& device);
		static VulkanImage CreateDefaultNormalMap(VulkanDevice& device);
	};

	struct VulkanSampler final : GPUSampler
	{
		VkSampler sampler = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;

		VulkanSampler() = default;
		VulkanSampler(VulkanDevice* device, const SamplerDesc& desc);
		void Destroy();
	};

}
