//
// Created by Orgest on 6/16/2025.
//

#pragma once
#include <vk_mem_alloc.h>

#include "RendererTypes.h"

namespace Renderer
{
	struct VulkanDevice;
	struct VulkanBuffer
	{
		// static std::atomic<u32> bufferAllocationCount;
		VkBuffer buffer = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		VmaAllocation allocation = VK_NULL_HANDLE;
		VmaAllocationInfo allocationInfo = {}; // contains mapped data, offsets and such lol
		GPUBufferInfo info;
		int allocCount = 0;

		VulkanBuffer() = default;
		void Init(VulkanDevice* device, const GPUBufferInfo& info);
		void Init(VulkanDevice* device, const BufferPreset preset, u64 size) { Init(device, GPUBufferInfo::FromPreset(preset, size)); };

		// Constructor Versions
		VulkanBuffer(VulkanDevice* device, const GPUBufferInfo& info) { Init(device, info); };
		VulkanBuffer(VulkanDevice* device, BufferPreset preset, u64 size)
			: VulkanBuffer(device, GPUBufferInfo::FromPreset(preset, size))
		{
		}

		// NO NO NO WE KEEPING THIS, WHY ARE WE MOVING/COPYING BUFFERS
		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;
		VulkanBuffer(VulkanBuffer&& other) noexcept = default;
		VulkanBuffer& operator=(VulkanBuffer&& other) noexcept = default;

		// Helpers
		[[nodiscard]] void* Map() const;
		void Unmap() const;
		void Upload(const void* srcData, u64 size) const;
		void CopyFrom(VkCommandBuffer cmd, VulkanBuffer* src, u64 size, u64 srcOffset = 0, u64 dstOffset = 0) const;
		[[nodiscard]] u64 GetDeviceAddress() const;
		[[nodiscard]] bool IsValid() const noexcept { return buffer != VK_NULL_HANDLE; }
		void Destroy();
	};
}
