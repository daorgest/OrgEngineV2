//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include "RendererTypes.h"
#include "vk_mem_alloc.h"

struct ArenaAllocator;

namespace Renderer
{
	struct VulkanDevice;
	struct VulkanImage
	{
		VkImage                 image = VK_NULL_HANDLE;            // The Vulkan image handle. This is the actual image resource.
		VkImageView             imageView = VK_NULL_HANDLE;        // The image view, used for accessing the image in shaders.
		VkFormat                imageFormat = VK_FORMAT_UNDEFINED; // The format of the image (e.g., VK_FORMAT_R8G8B8A8_UNORM).
		TextureLayout			imageLayout = TextureLayout::Unknown;
		VmaAllocation           allocation = VK_NULL_HANDLE;        // Memory allocation for the image, managed by VMA (Vulkan Memory Allocator).
		VmaAllocationInfo       allocInfo = {};
		VkImageSubresourceRange subresourceRange = {};
		VulkanDevice*           device = nullptr;
		TextureInfo				textureInfo;

		VulkanImage() = default;
		VulkanImage(VulkanDevice* device, TextureInfo& info) { Init(device, info); };
		VulkanImage(VulkanDevice* device, VkImage image) : image(image), device(device), textureInfo() {};
		void Init(VulkanDevice* device, TextureInfo& info);
		void Destroy();
		void MakeSampleable(VkCommandBuffer cmd);
		void UploadTextureToGPU(const void* data, TextureInfo& texInfo);
		void CreateImageView(VkFormat format);
		void FillSubresoruceInfo();
		void Transition(VkCommandBuffer cmd, TextureLayout newLayout);

		// Transitions
		void Transition(VkCommandBuffer cmd, TextureLayout oldLayout, TextureLayout newLayout);
		static void GenerateMipmaps(VkCommandBuffer cmd, const VulkanImage& image);

		// Fallback image handler
		static VulkanImage* CreateCheckerboardTexture(VulkanDevice* device, ArenaAllocator& arena);
	};

	struct VulkanSampler
	{
		VkSampler sampler{VK_NULL_HANDLE};
		VulkanDevice *device{nullptr};

		VulkanSampler(VulkanDevice* device, const SamplerDesc& desc);
		void Destroy();
	};

}
