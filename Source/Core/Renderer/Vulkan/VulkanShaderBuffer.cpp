//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <algorithm>

#include <tracy/Tracy.hpp>
#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "Tools/Logger.h"

using namespace Renderer;

VulkanShaderBuffer::VulkanShaderBuffer(GPUDevice* device, DescriptorAllocatorGrowable* alloc,
                                       const DescriptorSetLayoutDesc& desc)
{
    this->device = static_cast<VulkanDevice*>(device);
    this->allocator = alloc;
    this->desc = desc;

    Initialize();
}

void VulkanShaderBuffer::UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size)
{
    ZoneScopedN("VulkanShaderBuffer::UpdateBinding");
#ifdef _DEBUG
    // Validate frame index
    if (frameIndex >= MAX_FRAME_OVERLAP)
    {
        LOG(Error, "ShaderBuffer: frameIndex {} out of range (max: {})", frameIndex, MAX_FRAME_OVERLAP);
        return;
    }

    // Validate binding -> slot mapping
    if (binding >= bindingToSlot.size())
    {
        LOG(Error, "ShaderBuffer: binding {} out of range (table size: {})", binding, bindingToSlot.size());
        return;
    }

    const u32 slot = bindingToSlot[binding];
    if (slot == UINT32_MAX)
    {
        LOG(Error, "ShaderBuffer: binding {} is not present in this UniformBufferDesc", binding);
        return;
    }

    if (slot >= slotCount)
    {
        LOG(Error, "ShaderBuffer: slot {} out of range (slotCount: {}) for binding={}", slot, slotCount, binding);
        return;
    }
#endif
    const u32 bufferIndex = index(frameIndex, binding);
#ifdef _DEBUG
    if (bufferIndex >= buffers.size())
    {
        LOG(Error, "ShaderBuffer: calculated buffer index {} out of range (size: {})", bufferIndex, buffers.size());
        return;
    }
#endif
    const VulkanBuffer& buf = buffers[bufferIndex];
#ifdef _DEBUG
    if (!buf.IsValid())
    {
        LOG(Error, "ShaderBuffer at frame={} binding={} (slot={}) is not valid!", frameIndex, binding, slot);
        return;
    }

    // Validate data
    if (data == nullptr)
    {
        LOG(Error, "ShaderBuffer: null data pointer for frame={} binding={}", frameIndex, binding);
        return;
    }

    // Validate size
    if (size > buf.info.size)
    {
        LOG(Error, "[CRITICAL] Buffer Overflow! Set={} | Frame={} | Binding={} | Requested={} bytes | Actual Capacity={} bytes",
            desc.setIndex, frameIndex, binding, size, buf.info.size);
        return;
    }
#endif
    buf.Upload(data, size);
}

void VulkanShaderBuffer::Update(u32 frameIndex, const void* data, size_t size)
{
    UpdateBinding(frameIndex, 0, data, size);
}

void VulkanShaderBuffer::Bind(GPUCommandBuffer* cmd, GPUPipeline* pipeline, u32 frameIndex)
{
    ZoneScopedN("VulkanShaderBuffer::Bind");
    assert(frameIndex < MAX_FRAME_OVERLAP && "frameIndex out of range");

    cmd->BindDescriptorSet(&descriptorSets[frameIndex], desc.setIndex, pipeline);
}


void VulkanShaderBuffer::Destroy()
{
    for(auto& buf : buffers)
    {
        buf.Destroy();
    }
    buffers.clear();

    layout.Destroy(device);
    slotCount = 0;
}

void VulkanShaderBuffer::Initialize()
{
    ZoneScopedN("VulkanShaderBuffer::Initialize");

    // --- PASS 1: Metadata & Mapping ---
    SortBindings(desc.bindings);

    u32 maxBinding = 0;
    slotCount = 0;

    // Determine bounds and count in one pass
    for (const auto& b : desc.bindings)
    {
        maxBinding = std::max(maxBinding, b.binding);
        if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
        {
            slotCount++;
        }
    }

    // Prepare table for O(1) binding->slot lookup
    bindingToSlot.assign(maxBinding + 1, UINT32_MAX);

    u32 currentSlot = 0;
    DescriptorLayoutBuilder layoutBuilder;

    for (const auto& b : desc.bindings)
    {
        layoutBuilder.AddBinding(b);

        if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
        {
            bindingToSlot[b.binding] = currentSlot++;
        }
    }
    layout = layoutBuilder.Build(device);

    // --- PASS 2: Resource Construction ---
    buffers.clear();
    buffers.reserve(MAX_FRAME_OVERLAP * slotCount);

    // Layout: [Frame 0: Slot 0, 1, 2...][Frame 1: Slot 0, 1, 2...]
    for (u32 f = 0; f < MAX_FRAME_OVERLAP; ++f)
    {
        for (const auto& b : desc.bindings)
        {
            if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
            {
                BufferInfo info = BufferInfo::FromPreset(ToPreset(b.type), b.size);
                buffers.emplace_back(device, info);
            }
        }
    }

    const bool isBindless = std::ranges::any_of(desc.bindings, [](const auto& b) { return b.isBindless; });
    u32 maxCount = 1;
    for (const auto& b : desc.bindings) if (b.isBindless) maxCount = std::max(maxCount, b.count);

    AllocateDescriptorSets(isBindless, maxCount);
}
void VulkanShaderBuffer::AllocateDescriptorSets(const bool isBindless, const u32 bindlessCount)
{
    ZoneScoped;

    for (u32 frame = 0; frame < MAX_FRAME_OVERLAP; ++frame)
    {
        descriptorSets[frame] = allocator->Allocate(layout, isBindless, bindlessCount);

        auto& writer = descriptorSets[frame].writer;

        for (const Binding& b : desc.bindings)
        {
            if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
            {
                const u32 bufferIdx = index(frame, b.binding);
                writer.WriteBuffer(b.binding, &buffers[bufferIdx], b.type);
            }
        }
        writer.UpdateSet(device, descriptorSets[frame].vk);
        writer.Clear();
    }

}

u32 VulkanShaderBuffer::index(const u32 frame, const u32 binding) const noexcept
{
    return (frame * slotCount) + bindingToSlot[binding];
}
