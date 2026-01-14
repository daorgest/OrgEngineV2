//
// Created by Orgest on 6/16/2025.
//

#include "VulkanBuffer.h"
#include <vk_mem_alloc.h>

#include "VulkanCheck.h"
#include "VulkanDebugUtils.h"
#include "VulkanDevice.h"

using namespace Renderer;

// constructors
VulkanBuffer::VulkanBuffer(VulkanDevice* devicePtr, const BufferInfo& bufferInfo)
{
    VulkanBuffer::Init(devicePtr, bufferInfo);
}

VulkanBuffer::VulkanBuffer(VulkanDevice* devicePtr, BufferPreset preset, u64 size)
{
    Init(devicePtr, preset, size);
}

// Vulkan-specific Init overload for presets
void VulkanBuffer::Init(VulkanDevice* devicePtr, BufferPreset preset, u64 size)
{
    Init(devicePtr, BufferInfo::FromPreset(preset, size));
}

void VulkanBuffer::Init(GPUDevice* inputDevice, const BufferInfo& inputInfo)
{
    this->device = static_cast<VulkanDevice*>(inputDevice);
    this->info = inputInfo;
    assert(device && "VulkanDevice must not be null");
    assert(info.size > 0 && "Buffer size must be greater than 0");

    // Base usage always includes transfer capabilities for uploads/downloads
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // Vulkan usage flags
    if (HasAny(inputInfo.usage, GPUBufferFlag::Vertex)) usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (HasAny(inputInfo.usage, GPUBufferFlag::Index)) usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (HasAny(inputInfo.usage, GPUBufferFlag::Storage)) usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (HasAny(inputInfo.usage, GPUBufferFlag::Constant)) usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (HasAny(inputInfo.usage, GPUBufferFlag::ShaderDeviceAddress)) usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (HasAny(inputInfo.usage, GPUBufferFlag::ShaderBindingTable)) usage |=
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    if (HasAny(inputInfo.usage, GPUBufferFlag::Indirect)) usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    // Create buffer info AFTER usage is finalized
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = inputInfo.size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // Memory behavior based on heap type
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT; // for Nsight

    switch (inputInfo.heapType)
    {
    case GPUHeapType::Default:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case GPUHeapType::Upload:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;

    case GPUHeapType::Readback:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;

    default:
        LOG(Error, "Unknown heap type: {}", static_cast<i32>(inputInfo.heapType));
        break;
    }

    if (inputInfo.commit)
    {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VK_CHECK(vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo));
    const std::string debugName = " Buffer[" + GPUBufferFlagsToString(inputInfo.usage) + "][" + GPUHeapTypeToString(
        inputInfo.heapType) + "]";
    NameObject(device->device, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<u64>(buffer), debugName.c_str());
}

void VulkanBuffer::Destroy()
{
    if (buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(device->allocator, buffer, this->allocation);
    }
}

void* VulkanBuffer::Map() const
{
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(device->allocator, allocation, &data));
    return data;
}

void VulkanBuffer::Unmap() const
{
    vmaUnmapMemory(device->allocator, allocation);
}

void VulkanBuffer::Upload(const void* srcData, u64 size) const
{
    assert(srcData && "Buffer data is null");
    assert(size <= info.size && "Upload size exceeds buffer size, where did that come from?");
    void* dst = Map();
    if ((dst != nullptr) && (srcData != nullptr) && size > 0)
    {
        std::memcpy(dst, srcData, size);
    }
    Unmap();
}

u64 VulkanBuffer::GetDeviceAddress() const
{
    VkBufferDeviceAddressInfo deviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(device->device, &deviceAddressInfo);
}

// could put this in my abstracted command buffer.....
void VulkanBuffer::CopyFrom(const VkCommandBuffer cmd, const VulkanBuffer* src, u64 size, u64 srcOffset,
                            u64 dstOffset) const
{
    VkBufferCopy copyRegion = {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size
    };
    vkCmdCopyBuffer(cmd, src->buffer, this->buffer, 1, &copyRegion);
}
