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
VulkanTexture::VulkanTexture(VulkanDevice* device, const TextureInfo& info)
{
	Init(device, info);
}

void VulkanTexture::Init(VulkanDevice* inDevice, const TextureInfo& info)
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
        .usage = ToVk(info.usage),
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo createInfo = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .priority = 1.0f
    };

	FillSubresourceInfo();
    VK_CHECK(vmaCreateImage(inDevice->allocator, &imageInfo, &createInfo, &image, &allocation, &allocInfo));
    CreateImageView(info.format);
}

void VulkanTexture::InitExternal(VulkanDevice* inDevice, const VkImage inImage, const TextureInfo& info)
{
    this->device = inDevice;
    this->image = inImage;
    this->textureInfo = info;
    this->owns = false; // Important: Swapchain images are not owned by us

    FillSubresourceInfo();
    CreateImageView(info.format);
}

void VulkanTexture::Destroy()
{
    views.clear();

    if (device && device->device != VK_NULL_HANDLE)
    {
        if (owns && allocation != VK_NULL_HANDLE)
        {
            if (image == VK_NULL_HANDLE)
            {
                LOG(Error, "VulkanTexture::Destroy(): Leaking memory! Allocation exists but Image handle is NULL.");
            }
            else
            {
                vmaDestroyImage(device->allocator, image, allocation);
            }

            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            allocInfo = {};
        }
    }
    owns = false;
}

void VulkanTexture::UploadData(const void* data)
{
	UploadTextureToGPU(data, textureInfo);
}

GPUTextureView* VulkanTexture::GetView(u32 layer)
{
    if (layer < views.size() && views[layer].imageView)
    {
        return &views[layer];
    }

    LOG(Error, "Requested invalid texture view layer: {}", layer);
    return nullptr;
}

VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();

        // Transfer simple data
        image = other.image;
        allocation = other.allocation;
        device = other.device;
        allocInfo = other.allocInfo;
        subresourceRange = other.subresourceRange;
        textureInfo = other.textureInfo;
        owns = other.owns;

        views = std::move(other.views);

        // Update the 'texture' pointer inside each view to point to 'this'
        for (auto& view : views)
        {
            if (view.imageView)
            {
                view.texture = this;
            }
        }

        // Clean up source
        other.image = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.owns = false;
        other.views.clear();
    }
    return *this;
}

void VulkanTexture::UploadTextureToGPU(const void* data, const TextureInfo& texInfo)
{
    if (!allocation)
        LOG(Error, "VulkanTexture::UploadTextureToGPU(): allocation is null");

    const u32 bytesPerTexel = BytesPerTexel(texInfo.format);
    const bool isCompressed = (bytesPerTexel == 0 && texInfo.format != TextureFormat::UNKNOWN);
    const u16 layers = std::max<u16>(1, texInfo.arrayLayers);
    VkDeviceSize actualSize = 0;

    if (isCompressed) // For DDS
    {
        const bool is8ByteBlock = (texInfo.format == TextureFormat::BC1_RGB_UNORM_BLOCK ||
            texInfo.format == TextureFormat::BC1_RGBA_UNORM_BLOCK ||
            texInfo.format == TextureFormat::BC4_UNORM_BLOCK);
        const u32 blockSize = is8ByteBlock ? 8 : 16;

        for (u32 layer = 0; layer < layers; layer++)
        {
            for (u32 mip = 0; mip < texInfo.mipLevels; mip++)
            {
                // From https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide
                const u32 mipW = std::max(1u, texInfo.extent.width >> mip);
                const u32 mipH = std::max(1u, texInfo.extent.height >> mip);
                const u32 blocksW = (mipW + 3) / 4;
                const u32 blocksH = (mipH + 3) / 4;
                actualSize += static_cast<VkDeviceSize>(blocksW) * blocksH * blockSize;
            }
        }
    }
    else // Non DDS
    {
        // Uncompressed textures
        actualSize = static_cast<VkDeviceSize>(texInfo.extent.width) * static_cast<VkDeviceSize>(texInfo.extent.height)
            * static_cast<VkDeviceSize>(texInfo.extent.depth) * bytesPerTexel * layers;
    }

    const BufferInfo stagingInfo = {
        .size = actualSize,
        .heapType = GPUHeapType::Upload,
    };

    VulkanBuffer staging(device, stagingInfo);
    staging.Upload(data, actualSize);

    device->immediateSubmitter.Submit([&](VulkanCommandBuffer* cmd)
    {
        TextureTransition toCopy = {
            this,
            TextureLayout::Unknown,
            TextureLayout::CopyDestination
        };
        cmd->TransitionLayouts(SPAN_ONE(toCopy));
        cmd->CopyBufferToTexture(&staging, this);

        if (isCompressed)
        {
            TextureTransition toReadOnly = {
                this,
                TextureLayout::CopyDestination,
                TextureLayout::ShaderReadOnly
            };
            cmd->TransitionLayouts(SPAN_ONE(toReadOnly));
        }
        else
        {
            if (texInfo.mipLevels > 1)
            {
                cmd->GenerateMipmaps(this);
            }
            else
            {
                TextureTransition toReadOnly = {
                    this,
                    TextureLayout::CopyDestination,
                    TextureLayout::ShaderReadOnly
                };
                cmd->TransitionLayouts(SPAN_ONE(toReadOnly));
            }
        }
    }, "Texture Upload");
}

void VulkanTexture::CreateImageView(TextureFormat format)
{
    if (image == VK_NULL_HANDLE)
        LOG(Error, "CreateImageView() was called with no image in sight, we doing this right?");

    auto dimension = TextureViewDimension::Auto;
    u32 arrayCount = textureInfo.arrayLayers;


    if (textureInfo.type == ImageType::Image3D)
    {
        dimension = TextureViewDimension::Texture3D;
        arrayCount = 1;
    }

    TextureViewInfo info = {
        .format = format,
        .dimension = dimension,
        .baseMip = 0,
        .mipCount = textureInfo.mipLevels,
        .baseLayer = 0,
        .arrayCount = arrayCount
    };

    views.push_back({device, this, info});
}

void VulkanTexture::FillSubresourceInfo()
{
    subresourceRange.aspectMask = ToVkAspectMask(textureInfo.format);
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = textureInfo.mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = (textureInfo.type == ImageType::CubeMap) ? 6u : 1u;
}

void VulkanTexture::SetName(const std::string& name)
{
#if VULKAN_DEBUG_MODE
    if (image != VK_NULL_HANDLE)
    {
        Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(image), name.c_str());
    }

    if (!views.empty() && views[0].imageView && views[0].imageView != VK_NULL_HANDLE)
    {
        const std::string viewName = name + " View";
        Renderer::NameObject(device->device,
                             VK_OBJECT_TYPE_IMAGE_VIEW,
                             reinterpret_cast<u64>(views[0].imageView),
                             viewName.c_str());
    }
#endif
}

void VulkanTextureView::Init(VulkanDevice* dev, VulkanTexture* tex, const TextureViewInfo& info)
{
    this->device = dev;
    this->texture = tex;
    assert(texture && "Cannot create a Texture View for a null texture allocation!");

    const TextureFormat viewFormat = (info.format != TextureFormat::UNKNOWN)
                                         ? info.format
                                         : texture->textureInfo.format;


    VkImageViewType viewType;
    if (info.dimension == TextureViewDimension::Auto)
    {
        viewType = ToVkImageViewType(texture->textureInfo.dimension);
    }
    else
    {
        viewType = ToVk(info.dimension);
    }


    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = texture->image,
        .viewType = viewType,
        .format = ToVkFormat(viewFormat),
        .components = {
            .r = ToVk(info.swizzle.r),
            .g = ToVk(info.swizzle.g),
            .b = ToVk(info.swizzle.b),
            .a = ToVk(info.swizzle.a)
        },
        .subresourceRange = {
            .aspectMask = texture->subresourceRange.aspectMask,
            .baseMipLevel = info.baseMip,
            .levelCount = info.mipCount,
            .baseArrayLayer = info.baseLayer,
            .layerCount = info.arrayCount
        }
    };

    VK_CHECK(vkCreateImageView(device->device, &viewCreateInfo, nullptr, &imageView));
}

VulkanTextureView::VulkanTextureView(VulkanDevice* dev, VulkanTexture* tex, const TextureViewInfo& info)
{
    Init(dev, tex, info);
}

VulkanTextureView::~VulkanTextureView()
{
    if (device && imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
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
