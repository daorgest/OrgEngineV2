//
// Created by Orgest on 6/11/2025.
//

#include "VulkanTexture.h"

#include "MathFuncs.h"
#include "Vec4.h"
#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanInit.h"
#include "Tools/Array.h"
#include "Tools/Logger.h"
#include "Tools/Vector.h"

#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"

using namespace Renderer;

// VulkanImage

void VulkanImage::TransitionLayout(void* cmdBuffer, TextureLayout newLayout)
{
	auto* cmd = static_cast<VkCommandBuffer>(cmdBuffer);
	Transition(cmd, newLayout);
}

void VulkanImage::GenerateMipmaps(void* cmdBuffer)
{
	auto* cmd = static_cast<VkCommandBuffer>(cmdBuffer);
	VulkanImage::GenerateMipmaps(cmd, *this);
}
VulkanImage::VulkanImage(VulkanDevice* device, TextureInfo& info)
{
	Init(device, info);
}

VulkanImage::VulkanImage(VulkanDevice* device, VkImage image) : image(image), device(device) {}

void VulkanImage::Init(GPUDevice* gpuDevice, const TextureInfo& info)
{
	// Cast to VulkanDevice (safe because we control the backend)
	auto* vkDevice = static_cast<VulkanDevice*>(gpuDevice);
	TextureInfo mutableInfo = info; // Need mutable copy for existing implementation
	Init(vkDevice, mutableInfo);
}


void VulkanImage::Init(VulkanDevice* device, TextureInfo& info)
{
	this->device = device;
	this->textureInfo = info;

	const VkFormat vkFormat = ToVkFormat(info.format);
	const VkImageAspectFlags aspectFlag =
		vkFormat == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	if (info.mipLevels > 1)
	{
		info.usage |= ImageUsage::TransferSrc;
	}

	subresourceRange = {
		.aspectMask = aspectFlag,
		.baseMipLevel = 0,
		.levelCount = info.mipLevels,
		.baseArrayLayer = 0,
		.layerCount = (info.type == ImageType::CubeMap) ? 6u : 1u
	};

	const VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = (info.type == ImageType::CubeMap) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
		.imageType = ToVkImageType(info.dimension),
		.format = vkFormat,
		.extent = {info.extent.width, info.extent.height, info.extent.depth},
		.mipLevels = info.mipLevels,
		.arrayLayers = subresourceRange.layerCount,
		.samples = ToVk(info.sampleCount),
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = ToVkImageUsage(info.usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo createInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
	};

	imageFormat = imageInfo.format;

	VK_CHECK(vmaCreateImage(device->allocator, &imageInfo, &createInfo, &image, &allocation, &allocInfo));
	NameObject(device->device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(image), "AllocatedImage");

	const VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = ToImgViewType(info.dimension),
		.format = vkFormat,
		.subresourceRange = subresourceRange
	};

	VK_CHECK(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
}

void VulkanImage::Destroy()
{
	if (imageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device->device, imageView, nullptr);
	}
	if (image != VK_NULL_HANDLE)
	{
		vmaDestroyImage(device->allocator, image, allocation);
	}

	device = nullptr;
	imageFormat = VK_FORMAT_UNDEFINED;
	imageLayout = TextureLayout::Unknown;
}

void VulkanImage::MakeSampleable(VkCommandBuffer cmd)
{
	if (imageFormat == VK_FORMAT_D32_SFLOAT)
	{
		Transition(cmd, TextureLayout::DepthReadOnly);
		imageLayout = TextureLayout::DepthReadOnly;
	}
	else
	{
		Transition(cmd, TextureLayout::ShaderReadOnly);
		imageLayout = TextureLayout::ShaderReadOnly;
	}
}

void VulkanImage::PrepareAsRenderTarget(VkCommandBuffer cmd)
{
	Transition(cmd, imageLayout, TextureLayout::ColorWrite);
	imageLayout = TextureLayout::ColorWrite;
}

void VulkanImage::CopyFrom(VkCommandBuffer cmd, const VulkanImage& src) const
{
	VkImageCopy2 copyRegion{};
	copyRegion.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
	copyRegion.srcSubresource = {
		.aspectMask = subresourceRange.aspectMask,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = subresourceRange.layerCount
	};
	copyRegion.dstSubresource = copyRegion.srcSubresource;
	copyRegion.extent = {
		textureInfo.extent.width,
		textureInfo.extent.height,
		textureInfo.extent.depth
	};

	VkCopyImageInfo2 copy = {
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
		.srcImage = src.image,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = image,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	vkCmdCopyImage2(cmd, &copy);
}


void VulkanImage::UploadTextureToGPU(const void* data, const TextureInfo& texInfo)
{
	const u32 bytesPerTexel = BytesPerTexel(texInfo.format);

	const VkDeviceSize dataSize = static_cast<VkDeviceSize>(texInfo.extent.width) * static_cast<VkDeviceSize>(texInfo.extent.height) *
		static_cast<VkDeviceSize>(texInfo.extent.depth) * bytesPerTexel;

	const u16 layers = std::max<u16>(1, texInfo.arrayLayers);
	const VkDeviceSize actualSize = dataSize * layers;

	VulkanBuffer staging(device, BufferPreset::StagingUpload, actualSize);
	memcpy(staging.allocationInfo.pMappedData, data, actualSize);

	device->immediateSubmitter.Submit([&](VkCommandBuffer cmd) {
		Transition(cmd, imageLayout, TextureLayout::CopyDestination);

		Vector<VkBufferImageCopy2> copyRegions(layers); // now we can pass in array layers : D (cube maps, texture arrays)

		for (u32 i = 0; i < layers; i++)
		{
			VkBufferImageCopy2& r = copyRegions[i];
			r.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			r.bufferOffset = dataSize * i;
			r.bufferRowLength = 0;
			r.bufferImageHeight = 0;

			r.imageSubresource.aspectMask = this->subresourceRange.aspectMask;
			r.imageSubresource.mipLevel = 0;
			r.imageSubresource.baseArrayLayer = i;
			r.imageSubresource.layerCount = 1;

			r.imageOffset = {0, 0, 0};
			r.imageExtent = {texInfo.extent.width, texInfo.extent.height, texInfo.extent.depth};
		}

		VkCopyBufferToImageInfo2 copyInfo =
		{
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			.srcBuffer = staging.buffer,
			.dstImage = this->image,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = static_cast<u32>(copyRegions.size()),
			.pRegions = copyRegions.data()
		};

		vkCmdCopyBufferToImage2(cmd, &copyInfo);

		if (texInfo.mipLevels > 1) {
			GenerateMipmaps(cmd, *this);
			imageLayout = TextureLayout::ShaderReadOnly;
		} else {
			MakeSampleable(cmd);
		}
	});

	staging.Destroy();
}

void VulkanImage::UploadData(const void* data)
{
	UploadTextureToGPU(data, textureInfo);
}

void VulkanImage::CreateImageView(VkFormat format)
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

void VulkanImage::FillSubresourceInfo()
{
	subresourceRange.aspectMask = HasAny(textureInfo.usage, ImageUsage::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = textureInfo.mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = textureInfo.arrayLayers;
}


void VulkanImage::Transition(VkCommandBuffer cmd, TextureLayout newLayout)
{
	if (imageLayout == newLayout)
		return;

	Transition(cmd, imageLayout, newLayout);
}

void VulkanImage::Transition(VkCommandBuffer cmd, TextureLayout oldLayout, TextureLayout newLayout)
{
	if (oldLayout == newLayout)
		return;

	const VkImageAspectFlags aspectMask =
		(imageFormat == VK_FORMAT_D32_SFLOAT || imageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
		? VK_IMAGE_ASPECT_DEPTH_BIT
		: VK_IMAGE_ASPECT_COLOR_BIT;

	const VkImageLayout vkOldLayout = ToVk(oldLayout);
	const VkImageLayout vkNewLayout = ToVk(newLayout);

	// Determine src/dst stage and access masks
	VkPipelineStageFlags2 srcStageMask  = 0;
	VkAccessFlags2        srcAccessMask = 0;
	VkPipelineStageFlags2 dstStageMask  = 0;
	VkAccessFlags2        dstAccessMask = 0;

	switch (oldLayout)
	{
		case TextureLayout::DepthWrite:
			srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case TextureLayout::ColorWrite:
			srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case TextureLayout::ShaderReadOnly:
		case TextureLayout::DepthReadOnly:
			srcStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			break;
		default:
			srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			break;
	}

	switch (newLayout)
	{
		case TextureLayout::DepthWrite:
			dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
			dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case TextureLayout::ColorWrite:
			dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case TextureLayout::ShaderReadOnly:
		case TextureLayout::DepthReadOnly:
			dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			break;
		default:
			dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			break;
	}

	VkImageMemoryBarrier2 imageBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = srcStageMask,
		.srcAccessMask = srcAccessMask,
		.dstStageMask = dstStageMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = vkOldLayout,
		.newLayout = vkNewLayout,
		.image = image,
		.subresourceRange = {
			.aspectMask = aspectMask,
			.baseMipLevel = 0,
			.levelCount = textureInfo.mipLevels,
			.baseArrayLayer = 0,
			.layerCount = textureInfo.arrayLayers,
		}
	};

	VkDependencyInfo depInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &imageBarrier
	};

	vkCmdPipelineBarrier2(cmd, &depInfo);

	imageLayout = newLayout;
}


void VulkanImage::GenerateMipmaps(VkCommandBuffer cmd, const VulkanImage& image)
{
	const u32 mipLevels = image.textureInfo.mipLevels;
	i32 mipWidth = static_cast<i32>(image.textureInfo.extent.width);
	i32 mipHeight = static_cast<i32>(image.textureInfo.extent.height);

	if (mipLevels <= 1)
	{
		LOG(Warning, "No need to generate mipmaps if only 1 level exists, skipping... ");
		return;
	}

    // Reuse this for barriers
	VkImageMemoryBarrier2 barriers[2]{};

    for (u32 mipLevel = 1; mipLevel < mipLevels; mipLevel++)
    {
        // Transition previous mip level to TRANSFER_SRC_OPTIMAL
        barriers[0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = image.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipLevel - 1,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        // Transition next mip level to TRANSFER_DST_OPTIMAL
        barriers[1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = image.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipLevel,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = barriers
        };
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Blit operation for downscaling
        VkImageBlit2 blit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1, 0, 1},
            .srcOffsets = { { 0, 0, 0 }, { mipWidth, mipHeight, 1 } },
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1},
            .dstOffsets = { { 0, 0, 0 }, { std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1 } }
        };

        VkBlitImageInfo2 blitInfo{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = image.image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = image.image,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR
        };
        vkCmdBlitImage2(cmd, &blitInfo);

        // Prepare for next mip level
        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
    }

    // Transition all mip levels to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	Vector<VkImageMemoryBarrier2> finalBarriers(mipLevels);
	for (u32 mipLevel = 0; mipLevel < mipLevels; mipLevel++)
	{
		finalBarriers[mipLevel] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = image.image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = mipLevel,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
	}

    VkDependencyInfo finalDepInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<u32>(finalBarriers.size()),
        .pImageMemoryBarriers = finalBarriers.data()
    };
    vkCmdPipelineBarrier2(cmd, &finalDepInfo);
}

u32 PackUnorm4x8(const Vec4& v)
{
	union
	{
		u8 in[4];
		u32 out;
	} u{};

	const Vec4 clamped = Clamp(v, 0.0f, 1.0f);
	const Vec4 scaled = Round(clamped * 255.0f);

	u.in[0] = static_cast<u8>(scaled.x);
	u.in[1] = static_cast<u8>(scaled.y);
	u.in[2] = static_cast<u8>(scaled.z);
	u.in[3] = static_cast<u8>(scaled.w);

	return u.out;
}

auto VulkanImage::CreateCheckerboardTexture(VulkanDevice& device) -> VulkanImage
{
	constexpr size_t size = 16;
	const u32 magenta = PackUnorm4x8(Vec4(1, 0, 1, 1)); // Magenta
	const u32 black = PackUnorm4x8(Vec4(0, 0, 0, 1)); // Black

	Array<u32, size * size> pixels;
	for (u32 y = 0; y < size; y++)
	{
		for (u32 x = 0; x < size; x++)
		{
			pixels[y * size + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	TextureInfo texInfo = {
		.extent		= {size, size, 1},
		.mipLevels  = 1,
		.type		= ImageType::Image2D,
		.format		= TextureFormat::RGBA8_UNORM,
		.dimension  = TextureDimension::Texture2D,
		.usage      = ImageUsage::TransferDst | ImageUsage::Sampled
	};

	VulkanImage image{&device, texInfo};
	image.UploadTextureToGPU(pixels.data(), texInfo);

	return image;
}

auto VulkanImage::CreateDefaultNormalMap(VulkanDevice& device) -> VulkanImage
{
	constexpr size_t size = 4;
	const u32 flat = PackUnorm4x8({0.5f, 0.5f, 1.0f, 1.0f}); // Flat TS normal: (0.5, 0.5, 1.0, 1.0) → [128,128,255,255]

	Array<u32, size * size> pixels;
	for (u32 i = 0; i < size * size; i++) pixels[i] = flat;

	TextureInfo texInfo = {
		.extent     = { size, size, 1 },
		.mipLevels  = 1,
		.type       = ImageType::Image2D,
		.format     = TextureFormat::RGBA8_UNORM,
		.dimension  = TextureDimension::Texture2D,
		.usage      = ImageUsage::TransferDst | ImageUsage::Sampled
	};

	VulkanImage image{&device, texInfo};
	image.UploadTextureToGPU(pixels.data(), texInfo);
	return image;
}

VulkanImageView::VulkanImageView(VulkanImage* image, TextureViewInfo& viewInfo)
{
	if (!image) return;
	assert(image && "where the hell is the image");
	const TextureInfo& texInfo = image->textureInfo;

	// Validate mip slice bounds
	assert(viewInfo.mipSlice < texInfo.mipLevels && "VulkanImageView: base mip slice out of range");

	assert(viewInfo.mipSlice + viewInfo.mipCount <= texInfo.mipLevels && "VulkanImageView: mip slice range exceeds available mip levels");

	// Validate array slice bounds
	assert(viewInfo.arraySlice < texInfo.arrayLayers && "VulkanImageView: base array slice out of range");

	assert(viewInfo.arraySlice + viewInfo.arrayCount <= texInfo.arrayLayers && "VulkanImageView: array slice range exceeds available layers");

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


// VulkanSampler
void VulkanSampler::Init(GPUDevice* gpuDevice, const SamplerDesc& inputDesc)
{
	// Cast to VulkanDevice (safe because we control the backend)
	auto* vkDevice = static_cast<VulkanDevice*>(gpuDevice);
	Init(vkDevice, inputDesc);
}

void VulkanSampler::Init(VulkanDevice* vkDevice, const SamplerDesc& inputDesc)
{
	this->device = vkDevice;
	this->desc = inputDesc;

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

// VulkanSampler Legacy Constructor
VulkanSampler::VulkanSampler(VulkanDevice* device, const SamplerDesc& desc)
{
	Init(device, desc);
}

void VulkanSampler::Destroy()
{
	if (device != nullptr && sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(device->device, sampler, nullptr);
		sampler = VK_NULL_HANDLE;
	}
}