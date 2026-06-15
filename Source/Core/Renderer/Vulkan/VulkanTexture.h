//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

#include "RendererTypes.h"
#include "RenderInterface.h"

struct ArenaAllocator;

namespace Renderer
{
    struct VulkanTextureView;
    struct VulkanDevice;

	/// Vulkan implementation of GPUTexture
	struct VulkanTexture final : GPUTexture
	{
		// Constructors
		VulkanTexture() = default;
		VulkanTexture(VulkanDevice* device, const TextureInfo& info);
		VulkanTexture(VulkanDevice* device, const VkImage image) : image(image), device(device) {};
		~VulkanTexture() override { Destroy(); }

		// Vulkan-specific initialization (backward compatibility)
		void Init(VulkanDevice* inDevice, const TextureInfo& info);

	    void InitExternal(VulkanDevice* inDevice, VkImage inImage, const TextureInfo& info);

		// RHI interface implementation
		void Destroy() override;
		void UploadData(const void* data) override;
	    GPUTextureView* GetView(u32 layer = 0) override;

	    void SetName(const std::string& name) override;


		VulkanTexture(const VulkanTexture&) = delete;
		VulkanTexture& operator=(const VulkanTexture&) = delete;

	    VulkanTexture(VulkanTexture&& other) noexcept
		{
			*this = std::move(other);
		}

	    VulkanTexture& operator=(VulkanTexture&& other) noexcept;

		// Vulkan-specific helpers
		void UploadTextureToGPU(const void* data, const TextureInfo& texInfo);
		void CreateImageView(TextureFormat format);
		void FillSubresourceInfo();

        VkImage                 image = VK_NULL_HANDLE;
		VmaAllocation           allocation = VK_NULL_HANDLE;
		VmaAllocationInfo       allocInfo = {};
		VulkanDevice*           device = nullptr;
		VkImageSubresourceRange subresourceRange = {};
		TextureInfo             textureInfo = {};

		bool owns = false;

	private:
	    // Unified storage for all views (default view is index 0)
	    Vector<VulkanTextureView> views;
	};

    struct VulkanTextureView final : GPUTextureView
    {
        VulkanDevice* device = nullptr;
        VulkanTexture* texture = nullptr;
        VkImageView imageView = VK_NULL_HANDLE;
        u32 bindlessIndex = 0;


        VulkanTextureView(VulkanTextureView&& other) noexcept
        {
            *this = std::move(other);
        }

        VulkanTextureView& operator=(VulkanTextureView&& other) noexcept
        {
            if (this != &other)
            {
                device = other.device;
                texture = other.texture;
                imageView = other.imageView;
                bindlessIndex = other.bindlessIndex;

                other.imageView = VK_NULL_HANDLE;
                other.bindlessIndex = 0;
            }
            return *this;
        }

        VulkanTextureView() = default;
        void Init(VulkanDevice* dev, VulkanTexture* tex, const TextureViewInfo& info);
        VulkanTextureView(VulkanDevice* dev, VulkanTexture* tex, const TextureViewInfo& info);
        ~VulkanTextureView() override;
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
