//
// Created by Orgest on 6/16/2025.
//

#include "VulkanBuffer.h"

#include <vk_mem_alloc.h>
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"
#include "VulkanDevice.h"

using namespace Renderer;

// constructors
VulkanBuffer::VulkanBuffer(GPUDevice* devicePtr, const BufferInfo& bufferInfo)
{
    Init(devicePtr, bufferInfo);
}

void VulkanBuffer::Init(GPUDevice* inputDevice, const BufferInfo& inputInfo)
{
    this->device = static_cast<VulkanDevice*>(inputDevice);
    this->info = inputInfo;
    assert(device && "VulkanDevice must not be null");
    assert(info.size > 0 && "Buffer size must be greater than 0");

    const VkBufferUsageFlags usage = ToVk(inputInfo.usage);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = inputInfo.size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    allocInfo.minAlignment = device->deviceDesc.heapProperties.resourceHeapAlignment;

    switch (inputInfo.heapType)
    {
    case GPUHeapType::Default:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case GPUHeapType::Upload:
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;

    case GPUHeapType::Readback:
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;

    default:
        LOG(Error, "Unknown heap type: {}", static_cast<i32>(inputInfo.heapType));
        break;
    }
    if (usage == 0) {
        assert(false);
    }


    VK_CHECK(vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo));
    const std::string flagsStr = GPUBufferFlagsToString(inputInfo.usage);
    const std::string_view heapStr  = GPUHeapTypeToString(inputInfo.heapType);

    const std::string debugName = fmt::format("{}[{}]", flagsStr, heapStr);

    vmaSetAllocationName(device->allocator, allocation, debugName.c_str());
    NameObject(device->device, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), debugName.c_str());
}

void VulkanBuffer::Destroy()
{
    if (device && buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(device->allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        allocationInfo = {};
    }
}

void* VulkanBuffer::Map() const
{
    if (allocationInfo.pMappedData != nullptr)
    {
        return allocationInfo.pMappedData;
    }

    // Fallback for buffers that weren't created with the mapped bit
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(device->allocator, allocation, &data));
    return data;
}

void VulkanBuffer::Unmap() const
{
    if (allocationInfo.pMappedData == nullptr)
    {
        vmaUnmapMemory(device->allocator, allocation);
    }
}

void VulkanBuffer::Upload(const void* srcData, const u64 size) const
{
    if (void* dst = allocationInfo.pMappedData; dst && srcData && size > 0)
    {
        std::memcpy(dst, srcData, size);

        VkMemoryPropertyFlags memFlags;
        vmaGetMemoryTypeProperties(device->allocator, allocationInfo.memoryType, &memFlags);

        if (!(memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            vmaFlushAllocation(device->allocator, allocation, 0, size);
        }
    }
}
u64 VulkanBuffer::GetDeviceAddress() const
{
    VkBufferDeviceAddressInfo deviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(device->device, &deviceAddressInfo);
}