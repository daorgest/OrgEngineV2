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

void VulkanTexture::Init(GPUDevice* gpuDevice, const TextureInfo& info)
{
	// Cast to VulkanDevice (safe because we control the backend)
	auto* vkDevice = static_cast<VulkanDevice*>(gpuDevice);
	TextureInfo mutableInfo = info; // Need mutable copy for existing implementation
	Init(vkDevice, mutableInfo);
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
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }

    imageLayout = TextureLayout::Unknown;
    owns = false;
}

void VulkanTexture::UploadData(const void* data)
{
	UploadTextureToGPU(data, textureInfo);
}


void VulkanTexture::Init(VulkanDevice* device, TextureInfo& info)
{
	this->device = device;
	this->textureInfo = info;
	this->owns = true;

	if (info.mipLevels > 1)
	{
		info.usage |= ImageUsage::TransferSrc;
	}

	const VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = (info.type == ImageType::CubeMap) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
		.imageType = ToVkImageType(info.dimension),
		.format = ToVkFormat(info.format),
		.extent = {info.extent.width, info.extent.height, info.extent.depth},
		.mipLevels = info.mipLevels,
		.arrayLayers = info.arrayLayers,
		.samples = ToVk(info.sampleCount),
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = ToVkImageUsage(info.usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo createInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
	};

	imageFormat = imageInfo.format;
	FillSubresourceInfo();

	VK_CHECK(vmaCreateImage(device->allocator, &imageInfo, &createInfo, &image, &allocation, &allocInfo));

	// Image View Creation
	const VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = ToImgViewType(info.dimension),
		.format = ToVkFormat(info.format),
		.subresourceRange = subresourceRange
	};

	VK_CHECK(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
}

// VulkanImage
VulkanTexture::VulkanTexture(VulkanDevice* device, TextureInfo& info)
{
	Init(device, info);
}

VulkanTexture::VulkanTexture(VulkanDevice* device, VkImage image) : image(image), device(device) {}

void VulkanTexture::UploadTextureToGPU(const void* data, const TextureInfo& texInfo)
{
	const u32 bytesPerTexel = BytesPerTexel(texInfo.format);

	const VkDeviceSize dataSize = static_cast<VkDeviceSize>(texInfo.extent.width) * static_cast<VkDeviceSize>(texInfo.
			extent.height) *
		static_cast<VkDeviceSize>(texInfo.extent.depth) * bytesPerTexel;

	const u16 layers = std::max<u16>(1, texInfo.arrayLayers);
	const VkDeviceSize actualSize = dataSize * layers;

	VulkanBuffer staging(device, BufferPreset::StagingUpload, actualSize);
	memcpy(staging.allocationInfo.pMappedData, data, actualSize);

	device->immediateSubmitter.Submit([&](VkCommandBuffer cmd)
	{
		VulkanCommandBuffer wrapper;
		wrapper.InitFromHandle(device, cmd);

		wrapper.TransitionLayout(this, TextureLayout::CopyDestination);
		wrapper.CopyBufferToTexture(&staging, this);

		if (texInfo.mipLevels > 1)
		{
			wrapper.GenerateMipmaps(this);
		}
		else
		{
			wrapper.TransitionLayout(this, TextureLayout::ShaderReadOnly);
		}
	});
}

void VulkanTexture::CreateImageView(VkFormat format)
{
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = ToImgViewType(textureInfo.dimension),
		.format = format,
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
	subresourceRange.aspectMask = HasAny(textureInfo.usage, ImageUsage::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = textureInfo.mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = (textureInfo.type == ImageType::CubeMap) ? 6u : 1u;
}

void VulkanTexture::InitializeLayout(VkCommandBuffer cmd)
{
	// Use the flag we just set up in the device
	const VkImageLayout targetLayout = device->useUnifiedLayout
		                                   ? VK_IMAGE_LAYOUT_GENERAL
		                                   : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkImageMemoryBarrier2 barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = targetLayout,
		.image = this->image,
		.subresourceRange = this->subresourceRange
	};

	const VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier
	};

	vkCmdPipelineBarrier2(cmd, &dep);
	this->imageLayout = device->useUnifiedLayout
		                    ? TextureLayout::General
		                    : TextureLayout::ShaderReadOnly;
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

VulkanImageView::VulkanImageView(VulkanTexture* image, TextureViewInfo& viewInfo)
{
	if (!image) return;
	assert(image && "where the hell is the image");
	const TextureInfo& texInfo = image->textureInfo;

	// Validate mip slice bounds
	assert(viewInfo.baseMip < texInfo.mipLevels && "VulkanImageView: base mip slice out of range");

	assert(viewInfo.baseMip + viewInfo.mipCount <= texInfo.mipLevels && "VulkanImageView: mip slice range exceeds available mip levels");

	// Validate array slice bounds
	assert(viewInfo.baseLayer < texInfo.arrayLayers && "VulkanImageView: base array slice out of range");

	assert(viewInfo.baseLayer + viewInfo.arrayCount <= texInfo.arrayLayers && "VulkanImageView: array slice range exceeds available layers");

	// this->image = image;
	// VkFormat vkFormat = (viewInfo.format == TextureFormat::UNKNOWN)
	// 					? ToVkFormat(texInfo.format)
	// 					: ToVkFormat(viewInfo.format);
	//
	// TextureViewDimension dim = viewInfo.dimension;
	//
	// if (dim == TextureViewDimension::Auto)
	// {
	// 	switch (texInfo.dimension)
	// 	{
	// 		case TextureDimension::Texture2D:
	// 			dim = (texInfo.arrayLayers > 1)
	// 					? TextureViewDimension::Texture2DArray
	// 					: TextureViewDimension::Texture2D;
	// 			break;
	//
	// 		case TextureDimension::Texture3D:
	// 			dim = TextureViewDimension::Texture2DArray;
	// 			break;
	//
	// 		case TextureDimension::CubeMap:
	// 			dim = (texInfo.arrayLayers > 6)
	// 					? TextureViewDimension::Cube
	// 					: TextureViewDimension::Cube;
	// 			break;
	//
	// 		default:
	// 			dim = TextureViewDimension::Texture2D;
	// 			break;
	// 	}
	// }
	//
	// VkComponentMapping swiz{};
	// swiz.r = ToVk(viewInfo.swizzle.r);
	// swiz.g = ToVk(viewInfo.swizzle.g);
	// swiz.b = ToVk(viewInfo.swizzle.b);
	// swiz.a = ToVk(viewInfo.swizzle.a);

	// VkImageSubresourceRange subresourceRange{
	// 	.aspectMask = HasAny(texInfo.usage, ImageUsage::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
	// 	.baseMipLevel = viewInfo.mipSlice,
	// 	.levelCount = viewInfo.mipCount,
	// 	.baseArrayLayer = viewInfo.arraySlice,
	// 	.layerCount = viewInfo.arrayCount
	// };

}

void VulkanSampler::Init(VulkanDevice* dev, const SamplerInfo& inputDesc)
{
	this->device = dev;

	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = ToVk(inputDesc.magFilter),
		.minFilter = ToVk(inputDesc.minFilter),
		.mipmapMode = ToVk(inputDesc.mipFilter),
		.addressModeU = ToVk(inputDesc.addressU),
		.addressModeV = ToVk(inputDesc.addressV),
		.addressModeW = ToVk(inputDesc.addressW),
		.mipLodBias = inputDesc.mipLodBias,
		.anisotropyEnable = inputDesc.anisotropyEnable ? VK_TRUE : VK_FALSE,
		.maxAnisotropy = static_cast<f32>(inputDesc.maxAnisotropy),
		.compareEnable = inputDesc.compareEnable ? VK_TRUE : VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = inputDesc.minLod,
		.maxLod = inputDesc.maxLod,
		.borderColor = ToVk(inputDesc.borderColor),
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
