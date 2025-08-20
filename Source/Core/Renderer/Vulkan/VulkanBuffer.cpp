//
// Created by Orgest on 6/16/2025.
//

#include "VulkanBuffer.h"
#include <vk_mem_alloc.h>

#include "Logger.h"
#include "VulkanCheck.h"
#include "VulkanDebugUtils.h"
#include "VulkanInit.h"

using namespace Renderer;

void VulkanBuffer::Init(VulkanDevice* device, const GPUBufferInfo& info)
{
	this->device = device;
	this->info = info;
	assert(device && "VulkanDevice must not be null");
	assert(info.size > 0 && "Buffer size must be greater than 0");
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	// Vulkan usage flags
	if (HasFlag(info.usage, GPUBufferFlag::VERTEX))                usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (HasFlag(info.usage, GPUBufferFlag::INDEX))                 usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (HasFlag(info.usage, GPUBufferFlag::STORAGE))               usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (HasFlag(info.usage, GPUBufferFlag::CONSTANT))              usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (HasFlag(info.usage, GPUBufferFlag::SHADER_DEVICE_ADDRESS)) usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if (HasFlag(info.usage, GPUBufferFlag::SHADER_BINDING_TABLE))  usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	if (HasFlag(info.usage, GPUBufferFlag::INDIRECT))				usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	// Create buffer info AFTER usage is finalized
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = info.size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	// Memory behavior based on heap type
	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

	switch (info.heapType)
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
		LOG(Error, "Unknown heap type: {}", static_cast<int>(info.heapType));
		break;
	}

	if (info.commit)
	{
		allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	}

	LOG(Debug, "Creating buffer: size={}, usage=0x{:X}, heapType={}, flags=0x{:X}", info.size, usage, static_cast<int>(info.heapType),
	    allocInfo.flags);
	VK_CHECK(vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo));
	std::string debugName = " Buffer[" + GPUBufferFlagsToString(info.usage) + "][" + GPUHeapTypeToString(info.heapType) + "]";
	NameObject(device->device, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<u64>(buffer), debugName.c_str());
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
		std::memcpy(dst, srcData, size);
	Unmap();
}

void VulkanBuffer::CopyFrom(VkCommandBuffer cmd, VulkanBuffer* src, u64 size, u64 srcOffset, u64 dstOffset) const
{
	VkBufferCopy copyRegion = {
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size
	};
	vkCmdCopyBuffer(cmd, src->buffer, this->buffer, 1, &copyRegion);
}

u64 VulkanBuffer::GetDeviceAddress() const
{
	VkBufferDeviceAddressInfo deviceAddressInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = buffer
	};
	return vkGetBufferDeviceAddress(device->device, &deviceAddressInfo);
}

void VulkanBuffer::Destroy()
{
	if (buffer != VK_NULL_HANDLE)
	{
		LOG(Info, "Destroying Buffer with Handle: {}", static_cast<void*>(buffer));
		vmaDestroyBuffer(device->allocator, buffer, this->allocation);
		buffer = VK_NULL_HANDLE;
		allocation = VK_NULL_HANDLE;
	}
}
