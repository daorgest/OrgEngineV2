//
// Created by Orgest on 6/11/2025.
//

#include "VulkanTexture.h"

#include <format>

#include "Arena.h"
#include "Array.h"
#include "Logger.h"
#include "MathFuncs.h"
#include "Vec4.h"
#include "Vector.h"
#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanInit.h"

#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"

using namespace Renderer;

void VulkanImage::Init(VulkanDevice* device, TextureInfo& info)
{
	this->device = device;
	this->textureInfo = info;

	const VkFormat vkFormat = ToVkFormat(info.format);
	const VkImageAspectFlags aspectFlag =
		vkFormat == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	if (info.mipLevels > 1)
		info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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
		.imageType = ToImgType(info.dimension),
		.format = vkFormat,
		.extent = {info.extent.width, info.extent.height, info.extent.depth},
		.mipLevels = info.mipLevels,
		.arrayLayers = subresourceRange.layerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
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
	NameObject(device->device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), "AllocatedImage");

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



void VulkanImage::UploadTextureToGPU(const void* data, TextureInfo& texInfo)
{
	VkDeviceSize dataSize = texInfo.extent.depth * texInfo.extent.width * texInfo.extent.height *
		(texInfo.format == TextureFormat::D32_SFLOAT ? 1 : 4);

	VulkanBuffer staging(device, BufferPreset::StagingUpload, dataSize);
	memcpy(staging.allocationInfo.pMappedData, data, dataSize);

	device->ImmediateSubmit([&](VkCommandBuffer cmd) {
		Transition(cmd, imageLayout, TextureLayout::CopyDestination);

		VkBufferImageCopy copyRegion = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask     = this->subresourceRange.aspectMask,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1
			},
			.imageOffset = { 0, 0, 0 },
			.imageExtent = {
				.width  = texInfo.extent.width,
				.height = texInfo.extent.height,
				.depth  = texInfo.extent.depth
			}
		};

		vkCmdCopyBufferToImage(cmd, staging.buffer, this->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		if (texInfo.mipLevels > 1) {
			GenerateMipmaps(cmd, *this);
			imageLayout = TextureLayout::ShaderReadOnly;
		} else {
			MakeSampleable(cmd);
		}
	});

	staging.Destroy();
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

void VulkanImage::FillSubresoruceInfo()
{
	subresourceRange.aspectMask = (textureInfo.usage & ImageUsage::DEPTH_STENCIL_ATTACHMENT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = textureInfo.mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;
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
	VkPipelineStageFlags2 srcStageMask = 0;
	VkAccessFlags2         srcAccessMask = 0;
	VkPipelineStageFlags2 dstStageMask = 0;
	VkAccessFlags2         dstAccessMask = 0;

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
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
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

    // Reuse this vector for barriers
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
			.oldLayout = (mipLevel == mipLevels - 1) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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

VulkanImage* VulkanImage::CreateCheckerboardTexture(VulkanDevice* device, ArenaAllocator& arena)
{
	constexpr u32 size = 16;
	const u32 magenta = PackUnorm4x8(Vec4(1, 0, 1, 1)); // Magenta
	const u32 black   = PackUnorm4x8(Vec4(0, 0, 0, 1)); // Black

	Array<u32, size * size> pixels;
	for (u32 y = 0; y < size; ++y)
	{
		for (u32 x = 0; x < size; ++x)
		{
			pixels[y * size + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	TextureInfo texInfo = {
		.extent     = { size, size, 1 },
		.mipLevels  = 1,
		.type       = ImageType::Image2D,
		.format     = TextureFormat::RGBA8_UNORM,
		.dimension  = TextureDimension::TEXTURE_2D,
		.usage      = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED
	};

	// Arena-allocated VulkanImage
	auto* image = arena.Emplace<VulkanImage>(device, texInfo);
	image->UploadTextureToGPU(pixels.data(), texInfo);

	return image;
}

// Samplers
VulkanSampler::VulkanSampler(VulkanDevice* device, const SamplerDesc& desc)
{
	this->device = device;

	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = ToVk(desc.magFilter),
		.minFilter = ToVk(desc.minFilter),
		.mipmapMode = ToVk(desc.mipFilter),
		.addressModeU = ToVk(desc.addressU),
		.addressModeV = ToVk(desc.addressV),
		.addressModeW = ToVk(desc.addressW),
		.mipLodBias = desc.mipLodBias,
		.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE,
		.maxAnisotropy = (f32)desc.maxAnisotropy,
		.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = desc.minLod,
		.maxLod = desc.maxLod,
		.borderColor = ToVk(desc.borderColor),
		.unnormalizedCoordinates = desc.unnormalizedCoords ? VK_TRUE : VK_FALSE
	};

	VK_CHECK(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler));
}

void VulkanSampler::Destroy()
{
	if (device != nullptr) { vkDestroySampler(device->device, sampler, nullptr); }
}
