//
// Created by Orgest on 6/24/2025.
//

#include "VulkanDescriptors.h"

#include <ranges>

#include "VulkanBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Tools/Logger.h"

using namespace Renderer;

// TODO Orgest: Trash this whole dang thing and use VK_EXT_descriptor_heap if wider support

static VkDevice GetVkDevice(const GPUDevice* device)
{
    return static_cast<const VulkanDevice*>(device)->device;
}

void DescriptorLayout::Destroy(const GPUDevice* device) const
{
    if (vk) vkDestroyDescriptorSetLayout(GetVkDevice(device), vk, nullptr);
}

void VulkanDescriptorSet::WriteBuffer(u32 binding, const GPUBuffer* buffer, DescriptorType type)
{
    writer.WriteBuffer(binding, buffer, type);
}

void VulkanDescriptorSet::WriteTexture(u32 binding, GPUTextureView* texture, GPUSampler* sampler, DescriptorType type, u32 arrayElement)
{
    writer.WriteImage(binding, texture, sampler, type, arrayElement, 1);
}

void VulkanDescriptorSet::WriteTextureArray(const u32 binding, Span<GPUTextureView*> textures, const DescriptorType type)
{
    for (const auto& [i, tex] : std::views::enumerate(textures))
    {
        writer.WriteImage(binding, tex, nullptr, type, static_cast<u32>(i), 1);
    }
}

void VulkanDescriptorSet::Update(GPUDevice* device)
{
    if (writer.writes.empty()) return;

    writer.UpdateSet(device, vk);
    writer.Clear();
}

static VkImageLayout DetermineDescriptorLayout(const VulkanTexture* img, VkDescriptorType type)
{
    // If the device uses a unified layout (like General), stick to it
    if (img->device->useUnifiedLayout) return VK_IMAGE_LAYOUT_GENERAL;

    // Storage images almost always require GENERAL
    if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) return VK_IMAGE_LAYOUT_GENERAL;

    return IsDepthFormat(ToVkFormat(img->textureInfo.format))
               ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

// Layout Builder
DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(const Binding& binding)
{
    metadata.push_back(binding);
    return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBindings(const Span<const Binding> bindings)
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
    this->metadata.assign(desc.bindings);
    SortBindings(this->metadata);
    return Build(device);
}

// Combined Image + Sampler
DescriptorWriter& DescriptorWriter::WriteImage(u32 binding, const GPUTextureView* view, const GPUSampler* sampler,
    DescriptorType type, u32 arrayElement, u32 count)
{
    const VkDescriptorType vkType = ToVk(type);
    const auto* vkView = view ? static_cast<const VulkanTextureView*>(view) : nullptr;
    const auto* vkSmp = sampler ? static_cast<const VulkanSampler*>(sampler) : nullptr;


    imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = vkSmp ? vkSmp->sampler : VK_NULL_HANDLE,
        .imageView = vkView ? vkView->imageView : VK_NULL_HANDLE,
        .imageLayout = vkView ? DetermineDescriptorLayout(vkView->texture, vkType) : VK_IMAGE_LAYOUT_UNDEFINED
    });

    writes.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = VK_NULL_HANDLE,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
        .descriptorCount = count,
        .descriptorType = vkType,
        .pImageInfo = &imageInfos.back()
    });

    return *this;
}

DescriptorWriter& DescriptorWriter::WriteBuffer(const u32 binding, const GPUBuffer* buffer, DescriptorType type, u32 arrayElement, size_t size, size_t offset)
{
    if (!buffer) return *this;

    const auto* vkBuf = static_cast<const VulkanBuffer*>(buffer);

    bufferInfos.push_back({
        .buffer = vkBuf->buffer,
        .offset = offset,
        .range = (size == 0) ? vkBuf->GetSize() : size
    });

    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
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

void DescriptorWriter::UpdateSet(const GPUDevice* device, VkDescriptorSet set)
{
    for (auto& write : writes)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(GetVkDevice(device), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}
void DescriptorAllocatorGrowable::Init(GPUDevice* inDevice, const u32 inSetsPerPool,
                                       Span<const PoolSizes> poolRatios)
{
    this->device = inDevice;
    this->setsPerPool = inSetsPerPool;
    ratios.assign(poolRatios);
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

VulkanDescriptorSet DescriptorAllocatorGrowable::Allocate(DescriptorLayout layout, bool isBindless, u32 bindlessCount)
{
    // 1. Setup the variable count info
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &bindlessCount
    };

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        VkDescriptorPool pool = GetPool();
        if (pool == VK_NULL_HANDLE) return {};

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = (isBindless && bindlessCount > 0) ? &variableInfo : nullptr,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout.vk
        };

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult result = vkAllocateDescriptorSets(GetVkDevice(device), &allocInfo, &set);

        // Success Path
        if (result == VK_SUCCESS)
        {
            readyPools.push_back(pool);
            return VulkanDescriptorSet(set);
        }

        if (result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY)
        {
            fullPools.push_back(pool);
        }


        else
        {
            // Put the perfectly good pool back into rotation so we don't leak it
            readyPools.push_back(pool);
            LOG(Error, "Vulkan descriptor allocation failed with unexpected error code: {}", static_cast<int>(result));
            return {};
        }
    }

    // If we reach here, even the brand-new pool failed.
    LOG(Error, "Descriptor allocation failed on fresh pool.");
    return {};
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
