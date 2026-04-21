//
// Created by Orgest on 6/11/2025.
//

#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanCommandBuffer.h"
#include "Tools/Logger.h"

#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"
#include "VulkanDevice.h"

using namespace Renderer;

// VulkanImage
VulkanTexture::VulkanTexture(VulkanDevice* device, TextureInfo& info)
{
	Init(device, info);
}

void VulkanTexture::Init(VulkanDevice* inDevice, TextureInfo& info)
{
	this->device      = inDevice;
	this->textureInfo = info;
	this->owns        = true;

	if (info.mipLevels > 1)
	{
		info.usage |= ImageUsage::TransferSrc;
	}

	const VkImageCreateInfo imageInfo = {
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags         = (info.type == ImageType::CubeMap) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
		.imageType     = ToVkImageType(info.dimension),
		.format        = ToVkFormat(info.format),
		.extent        = {info.extent.width, info.extent.height, info.extent.depth},
		.mipLevels     = info.mipLevels,
		.arrayLayers   = info.arrayLayers,
		.samples       = ToVk(info.sampleCount),
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = ToVkImageUsage(info.usage),
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

    VmaAllocationCreateInfo createInfo = {};

    if (info.memoryMode == MemoryProperty::LazilyAllocated)
    {
        createInfo.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }
    else
    {
        createInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

	imageFormat = imageInfo.format;
	FillSubresourceInfo();

	VK_CHECK(vmaCreateImage(inDevice->allocator, &imageInfo, &createInfo, &image, &allocation, &allocInfo));

	// Image View Creation
	const VkImageViewCreateInfo viewInfo = {
		.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image            = image,
		.viewType         = ToImgViewType(info.dimension),
		.format           = ToVkFormat(info.format),
		.subresourceRange = subresourceRange
	};

	VK_CHECK(vkCreateImageView(inDevice->device, &viewInfo, nullptr, &imageView));
}


void VulkanTexture::Destroy()
{
	if (device && device->device != VK_NULL_HANDLE)
	{
		if (imageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device->device, imageView, nullptr);
			imageView = VK_NULL_HANDLE;
		}
		if (owns && image != VK_NULL_HANDLE)
		{
			vmaDestroyImage(device->allocator, image, allocation);
			image      = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
		}
	}

	imageLayout = TextureLayout::Unknown;
	owns        = false;
}

void VulkanTexture::UploadData(const void* data)
{
	UploadTextureToGPU(data, textureInfo);
}

void VulkanTexture::UploadTextureToGPU(const void* data, const TextureInfo& texInfo)
{
	const u32 bytesPerTexel = BytesPerTexel(texInfo.format);

	const VkDeviceSize dataSize = static_cast<VkDeviceSize>(texInfo.extent.width) * static_cast<VkDeviceSize>(texInfo.
			extent.height) *
		static_cast<VkDeviceSize>(texInfo.extent.depth) * bytesPerTexel;

	const u16          layers     = std::max<u16>(1, texInfo.arrayLayers);
	const VkDeviceSize actualSize = dataSize * layers;

	VulkanBuffer staging(device, BufferPreset::StagingUpload, actualSize);
	memcpy(staging.allocationInfo.pMappedData, data, actualSize);

	device->immediateSubmitter.Submit([&](VulkanCommandBuffer* cmd)
	{
		cmd->TransitionLayout(this, TextureLayout::CopyDestination);
		cmd->CopyBufferToTexture(&staging, this);

		if (textureInfo.mipLevels > 1)
		{
			cmd->GenerateMipmaps(this);
		}
		else
		{
			cmd->TransitionLayout(this, TextureLayout::ShaderReadOnly);
		}
	}, "Texture Upload");
}

void VulkanTexture::CreateImageView(VkFormat format)
{
	VkImageViewCreateInfo viewInfo = {
		.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image      = image,
		.viewType   = ToImgViewType(textureInfo.dimension),
		.format     = format,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange = subresourceRange
	};

	VK_CHECK(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
}

void VulkanTexture::FillSubresourceInfo()
{
	subresourceRange.aspectMask     = HasAny(textureInfo.usage, ImageUsage::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel   = 0;
	subresourceRange.levelCount     = textureInfo.mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount     = (textureInfo.type == ImageType::CubeMap) ? 6u : 1u;
}

void VulkanTexture::SetName(const std::string& name)
{
#if VULKAN_DEBUG_MODE
	if (image != VK_NULL_HANDLE) {
		Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), name.c_str());
	}
	if (imageView != VK_NULL_HANDLE) {
		const std::string viewName = name + " View";
		Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), viewName.c_str());
	}
#endif
}

// VulkanSampler
void VulkanSampler::Init(VulkanDevice* dev, const SamplerInfo& inputDesc)
{
	this->device = dev;

	VkSamplerCreateInfo samplerInfo = {
		.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter               = ToVk(inputDesc.magFilter),
		.minFilter               = ToVk(inputDesc.minFilter),
		.mipmapMode              = ToVk(inputDesc.mipFilter),
		.addressModeU            = ToVk(inputDesc.addressU),
		.addressModeV            = ToVk(inputDesc.addressV),
		.addressModeW            = ToVk(inputDesc.addressW),
		.mipLodBias              = inputDesc.mipLodBias,
		.anisotropyEnable        = inputDesc.anisotropyEnable ? VK_TRUE : VK_FALSE,
		.maxAnisotropy           = static_cast<f32>(inputDesc.maxAnisotropy),
		.compareEnable           = inputDesc.compareEnable ? VK_TRUE : VK_FALSE,
		.compareOp               = VK_COMPARE_OP_ALWAYS,
		.minLod                  = inputDesc.minLod,
		.maxLod                  = inputDesc.maxLod,
		.borderColor             = ToVk(inputDesc.borderColor),
		.unnormalizedCoordinates = inputDesc.unnormalizedCoords ? VK_TRUE : VK_FALSE
	};

	VK_CHECK(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler));
}

// VulkanSampler
void VulkanSampler::Destroy() const
{
	if (sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(device->device, sampler, nullptr);
	}
}
