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

static VkDevice GetVkDevice(const GPUDevice* device)
{
    return static_cast<const VulkanDevice*>(device)->device;
}

void DescriptorLayout::Destroy(const GPUDevice* device) const
{
    if (vk) vkDestroyDescriptorSetLayout(GetVkDevice(device), vk, nullptr);
}


static VkImageLayout DetermineDescriptorLayout(const VulkanTexture* img, VkDescriptorType type)
{
    // If the device uses a unified layout (like General), stick to it
    if (img->device->useUnifiedLayout) return VK_IMAGE_LAYOUT_GENERAL;

    // Storage images almost always require GENERAL
    if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) return VK_IMAGE_LAYOUT_GENERAL;

    return IsDepthFormat(img->imageFormat)
               ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

DescriptorLayout DescriptorLayoutBuilder::Build(const GPUDevice* device, void* pNext,
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
            newFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

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
    vkCreateDescriptorSetLayout(GetVkDevice(device), &info, nullptr, &result.vk);
    return result;
}

DescriptorLayout DescriptorLayoutBuilder::BuildFromDesc(const GPUDevice* device, const DescriptorSetLayoutDesc& desc)
{
    this->Clear();
    this->metadata = desc.bindings;
    SortBindings(this->metadata);
    return Build(device);
}

// Combined Image + Sampler
DescriptorWriter& DescriptorWriter::WriteCombinedImage(u32 binding, const GPUTexture* image, const GPUSampler* sampler,
                                                       const u32 arrayElement)
{
    if (!sampler || !image) return *this;

    const auto* img = static_cast<const VulkanTexture*>(image);
    const auto* smp = static_cast<const VulkanSampler*>(sampler);

    imageInfos.push_back({
        .sampler = smp->sampler,
        .imageView = img->imageView,
        .imageLayout = DetermineDescriptorLayout(img, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    });

    writes.push_back({
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
                                               const DescriptorType type)
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


DescriptorWriter& DescriptorWriter::WriteBuffer(const u32 binding, const GPUBuffer* buffer, DescriptorType type)
{
    if (!buffer)
        return *this;

    const auto* vkBuf = static_cast<const VulkanBuffer*>(buffer);

    bufferInfos.push_back({
        .buffer = vkBuf->buffer,
        .offset = 0,
        .range = vkBuf->GetSize()
    });

    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = ToVk(type),
        .pBufferInfo = &bufferInfos.back()
    });
    return *this;
}

// Above but with more options
DescriptorWriter& DescriptorWriter::WriteBuffer(u32 binding, const GPUBuffer* buffer, size_t size, size_t offset,
                                                DescriptorType type)
{
    if (!buffer) return *this;
    const auto* vkBuf = static_cast<const VulkanBuffer*>(buffer);

    bufferInfos.push_back({
        .buffer = vkBuf->buffer,
        .offset = offset,
        .range = size
    });

    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = ToVk(type),
        .pBufferInfo = &bufferInfos.back()
    });

    return *this;
}

void DescriptorWriter::Clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::UpdateSet(const GPUDevice* device, const DescriptorSet set)
{
    for (auto& write : writes)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(GetVkDevice(device), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

void DescriptorAllocatorGrowable::Init(GPUDevice* inDevice, const u32 inSetsPerPool,
                                       const std::span<PoolSizes> poolRatios)
{
    this->device = inDevice;
    this->setsPerPool = inSetsPerPool;
    ratios.assign(poolRatios.begin(), poolRatios.end());
}

void DescriptorAllocatorGrowable::ResetPools()
{
    VkDevice vkDev = GetVkDevice(device);
    for (const auto& pool : readyPools) vkResetDescriptorPool(vkDev, pool, 0);
    for (auto& pool : fullPools)
    {
        vkResetDescriptorPool(vkDev, pool, 0);
        readyPools.push_back(pool);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::DestroyPools()
{
    VkDevice vkDev = GetVkDevice(device);
    for (const auto& pool : readyPools) vkDestroyDescriptorPool(vkDev, pool, nullptr);
    readyPools.clear();
    for (const auto& pool : fullPools) vkDestroyDescriptorPool(vkDev, pool, nullptr);
    fullPools.clear();
}

DescriptorSet DescriptorAllocatorGrowable::Allocate(DescriptorLayout layout, bool isBindless, u32 bindlessCount)
{
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &bindlessCount
    };

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        VkDescriptorPool pool = GetPool();
        if (pool == VK_NULL_HANDLE) return {VK_NULL_HANDLE};

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = (isBindless && bindlessCount > 0) ? &variableInfo : nullptr,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout.vk
        };

        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(GetVkDevice(device), &allocInfo, &set);

        if (result == VK_SUCCESS)
        {
            readyPools.push_back(pool);
            return {set};
        }

        fullPools.push_back(pool);
        if (attempt == 1)
            LOG(Error, "Descriptor allocation failed on fresh pool.");
    }

    return {VK_NULL_HANDLE};
}

VkDescriptorPool DescriptorAllocatorGrowable::GetPool()
{
    if (!readyPools.empty())
    {
        const VkDescriptorPool pool = readyPools.back();
        readyPools.pop_back();
        return pool;
    }

    VkDescriptorPool pool = CreatePool(setsPerPool);
    if (pool == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    setsPerPool = std::min<u32>(static_cast<u32>(setsPerPool * 1.5f), 4092);
    return pool;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(u32 setCount)
{
    Vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& [type, ratio] : ratios)
    {
        poolSizes.push_back({ToVk(type), static_cast<u32>(std::ceil(ratio * setCount))});
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = setCount,
        .poolSizeCount = static_cast<u32>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool newPool;
    if (vkCreateDescriptorPool(GetVkDevice(device), &poolInfo, nullptr, &newPool) == VK_SUCCESS)
    {
        LOG(Info, "Created new descriptor pool with capacity: {}", setCount);
        return newPool;
    }

    LOG(Error, "Failed to create descriptor pool.");
    return VK_NULL_HANDLE;
}
