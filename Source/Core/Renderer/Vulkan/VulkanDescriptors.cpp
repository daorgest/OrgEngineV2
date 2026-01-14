//
// Created by Orgest on 6/24/2025.
//

#include "VulkanDescriptors.h"

#include <cmath>

#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Tools/Logger.h"

using namespace Renderer;

void DescriptorLayout::Destroy(const VulkanDevice* device) const
{
    if (vk)
    {
        vkDestroyDescriptorSetLayout(device->device, vk, nullptr);
    }
}

// Layout Builder
DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(const Binding& binding)
{
    metadata.push_back(binding);
    return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBindings(const std::span<const Binding> bindings)
{
    metadata.reserve(metadata.size() + bindings.size());
    for (const auto& b : bindings)
    {
        AddBinding(b);
    }
    return *this;
}

DescriptorLayout DescriptorLayoutBuilder::Build(const VulkanDevice* device, void* pNext,
                                                const VkDescriptorSetLayoutCreateFlags flags)
{
    Vector<VkDescriptorBindingFlags> bindingFlags;
    bool hasBindless = false;

    for (size_t i = 0; i < metadata.size(); i++)
    {
        const auto& b = metadata[i];
        VkDescriptorBindingFlags newFlag = 0;

        if (b.isBindless)
        {
            hasBindless = true;
            newFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

            // Only the LAST binding can be variable count
            if (i == metadata.size() - 1)
            {
                newFlag |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
        }

        vkBindings.push_back({
            .binding = b.binding,
            .descriptorType = ToVk(b.type),
            .descriptorCount = std::max(1u, b.count),
            .stageFlags = ToVk(b.stageFlags)
        });
        bindingFlags.push_back(newFlag);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = pNext,
        .bindingCount = static_cast<u32>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = hasBindless ? &flagsInfo : pNext,
        .flags = flags | (hasBindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0u),
        .bindingCount = static_cast<u32>(vkBindings.size()),
        .pBindings = vkBindings.data()
    };

    DescriptorLayout result;
    vkCreateDescriptorSetLayout(device->device, &info, nullptr, &result.vk);
    return result;
}

// Combined Image + Sampler
DescriptorWriter& DescriptorWriter::WriteCombinedImage(u32 binding, const GPUTexture* image,
                                                       const GPUSampler* sampler, const u32 arrayElement)
{
    if (sampler == nullptr)
    {
        return *this;
    }

    const auto* img = static_cast<const VulkanTexture*>(image);
    const auto* smp = static_cast<const VulkanSampler*>(sampler);

    VkImageLayout layout = img->device->useUnifiedLayout ? VK_IMAGE_LAYOUT_GENERAL :
                           (IsDepthFormat(img->imageFormat) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = smp->sampler, // Accessing Vulkan handle
        .imageView = img->imageView,
        .imageLayout = layout
    });

    writes.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfos.back()
    });

    return *this;
}

// Single Image or Single Sampler
DescriptorWriter& DescriptorWriter::WriteImage(u32 binding, const GPUTexture* image, const GPUSampler* sampler,
    DescriptorType type)
{
    VkDescriptorType vkType = ToVk(type);

    if (vkType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || vkType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
    {
        if (image == nullptr)
            return *this;

        const auto* img = static_cast<const VulkanTexture*>(image);

        VkImageLayout layout;
        if (img->device->useUnifiedLayout)
        {
            layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            layout = (vkType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                         ? VK_IMAGE_LAYOUT_GENERAL
                         : (IsDepthFormat(img->imageFormat)
                                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        imageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = img->imageView,
            .imageLayout = layout,
        });
    }
    else if (vkType == VK_DESCRIPTOR_TYPE_SAMPLER)
    {
        if (sampler == nullptr)
            return *this;

        const auto* smp = static_cast<const VulkanSampler*>(sampler);

        imageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = smp->sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        });
    }

    writes.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = vkType,
        .pImageInfo = &imageInfos.back()
    });

    return *this;
}

// Image Array
DescriptorWriter& DescriptorWriter::WriteImageArray(u32 binding, const std::span<const GPUTexture*> images,
                                                    const DescriptorType type)
{
    if (images.empty()) return *this;

    const u32 start = static_cast<u32>(imageInfos.size());

    for (const GPUTexture* baseImg : images)
    {
        const auto* img = static_cast<const VulkanTexture*>(baseImg);

        VkImageLayout layout;
        if (img->device->useUnifiedLayout)
        {
            layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            layout = ToVk(img->imageLayout);
        }

        imageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = img->imageView,
            .imageLayout = layout,
        });
    }

    writes.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = static_cast<u32>(images.size()),
        .descriptorType = ToVk(type),
        .pImageInfo = &imageInfos.at(start)
    });

    return *this;
}

DescriptorWriter& DescriptorWriter::WriteBuffer(const u32 binding, const VulkanBuffer* buffer, const DescriptorType type)
{
	if (!buffer)
		return *this;

	bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer->buffer,
		.offset = 0,
		.range  = buffer->info.size
	});

	writes.emplace_back(VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding      = binding,
		.descriptorCount = 1,
		.descriptorType  = ToVk(type),
		.pBufferInfo     = &bufferInfos.back()
	});

	return *this;
}

DescriptorWriter& DescriptorWriter::WriteBuffer(u32 binding, const VulkanBuffer* buffer, size_t size, size_t offset,
    DescriptorType type)
{
    if (!buffer)
        return *this;

    bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer->buffer,
        .offset = offset,
        .range  = size
    });

    writes.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = ToVk(type),
        .pBufferInfo     = &bufferInfos.back()
    });

    return *this;
}

DescriptorWriter& DescriptorWriter::WriteBuffers(std::span<const Binding> bindings, const VulkanBuffer* megaBuffer,
                                                 std::span<const u32> offsets)
{
    for (size_t i = 0; i < bindings.size(); ++i)
    {
        WriteBuffer(bindings[i].binding, megaBuffer, bindings[i].size, offsets[i], bindings[i].type);
    }
    return *this;
}

void DescriptorWriter::Clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::UpdateSet(const VulkanDevice* device, const DescriptorSet set)
{
    for (auto& write : writes)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device->device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

// Descriptor Allocator Growable
void DescriptorAllocatorGrowable::Init(VulkanDevice* device, u32 setsPerPool, std::span<PoolSizes> poolRatios)
{
    this->device = device;
    this->setsPerPool = setsPerPool;

    ratios.clear();
    ratios.reserve(poolRatios.size());

    for (const auto& ratio : poolRatios)
    {
        ratios.push_back(ratio);
    }
}

void DescriptorAllocatorGrowable::ResetPools()
{
    for (const auto& pool : readyPools)
    {
        vkResetDescriptorPool(device->device, pool, 0);
    }

    for (auto& pool : fullPools)
    {
        vkResetDescriptorPool(device->device, pool, 0);
        readyPools.push_back(pool);
    }

    fullPools.clear();
}

void DescriptorAllocatorGrowable::DestroyPools()
{
    for (auto& pool : readyPools)
    {
        vkDestroyDescriptorPool(device->device, pool, nullptr);
    }
    readyPools.clear();

    for (auto& pool : fullPools)
    {
        vkDestroyDescriptorPool(device->device, pool, nullptr);
    }
    fullPools.clear();
}

DescriptorSet DescriptorAllocatorGrowable::Allocate(DescriptorLayout layout, const bool isBindless, const u32 bindlessCount)
{
    VkDescriptorSet set = VK_NULL_HANDLE;
    void* pNextChain = nullptr;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo = {};

    // Bindless? maybe.
    if (isBindless && bindlessCount > 0)
    {
        variableInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableInfo.descriptorSetCount = 1;
        variableInfo.pDescriptorCounts = &bindlessCount;
        pNextChain = &variableInfo;
    }

    for (;;)
    {
        // Get or create a pool for allocation
        VkDescriptorPool pool = GetPool();
        if (pool == VK_NULL_HANDLE)
        {
            LOG(Error, "GetPool() returned null pool");
            return {set};
        }

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = pNextChain,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout.vk
        };

        VkResult result = vkAllocateDescriptorSets(device->device, &allocInfo, &set);

        // wtf
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            LOG(Warning, "Descriptor pool ran out of memory or fragmented for some reason. Creating a new pool.");
            // Mark the current pool as full and retry with a new pool
            fullPools.push_back(pool);
            continue; // loop will grab a new pool from GetPool()
        }

        if (result != VK_SUCCESS)
        {
            LOG(Error, "Failed to allocate descriptor set (VkResult: {})", static_cast<i32>(result));
            return {set};;
        }

        // Allocation succeeded — pool is still usable, so push it back for reuse
        readyPools.push_back(pool);
        return {set};;
    }
}


VkDescriptorPool DescriptorAllocatorGrowable::GetPool()
{
    if (!readyPools.empty())
    {
        VkDescriptorPool pool = readyPools.back();
        readyPools.pop_back();
        return pool;
    }

    VkDescriptorPool pool = CreatePool(setsPerPool);
    if (pool == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    setsPerPool = std::min<u32>(setsPerPool, 4092);
    // its 4096 for the hard limit but this is for funny alignment reasons
    return pool;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(u32 setCount)
{
    Vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(ratios.size());

    for (const auto& [type, ratio] : ratios)
    {
        const u32 count = static_cast<u32>(std::ceil(ratio * static_cast<f32>(setCount)));
        poolSizes.push_back({ToVk(type), count});
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = setCount,
        .poolSizeCount = static_cast<u32>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool newPool;
    VkResult res = vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &newPool);

    if (res == VK_SUCCESS)
    {
        LOG(Info, "Created new descriptor pool with capacity: {}", setCount);
        return newPool;
    }

    LOG(Error, "Failed to create descriptor pool.");
    return VK_NULL_HANDLE;
}
