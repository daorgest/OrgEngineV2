//
// Created by Orgest on 6/16/2025.
//

#pragma once
#include <vk_mem_alloc.h>

#include "RendererTypes.h"
#include "RenderInterface.h"

namespace Renderer
{
	// Forward declarations
	struct GPUDevice;
	struct VulkanDevice;

	/// Vulkan implementation of GPUBuffer
	struct VulkanBuffer final : GPUBuffer
	{
		// RHI interface implementation
		void Init(GPUDevice* device, const GPUBufferInfo& inputInfo) override;
		void Destroy() override;
		[[nodiscard]] void* Map() const override;
		void Unmap() const override;
		void Upload(const void* srcData, u64 size) const override;
		[[nodiscard]] u64 GetSize() const override { return info.size; }
		[[nodiscard]] u64 GetDeviceAddress() const override;
		[[nodiscard]] bool IsValid() const override { return buffer != VK_NULL_HANDLE; }

		// Vulkan-specific initialization (backward compatibility)
		void Init(VulkanDevice* devicePtr, BufferPreset preset, u64 size);

		// Constructors
		VulkanBuffer() = default;
		VulkanBuffer(VulkanDevice* devicePtr, const GPUBufferInfo& bufferInfo);
		VulkanBuffer(VulkanDevice* devicePtr, BufferPreset preset, u64 size);

		// Explicitly delete copy, allow move
		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;
		VulkanBuffer(VulkanBuffer&&) noexcept = default;
		VulkanBuffer& operator=(VulkanBuffer&&) noexcept = default;

		// ~VulkanBuffer() override { Destroy(); }

		// Vulkan-specific helpers
		void CopyFrom(VkCommandBuffer cmd, const VulkanBuffer* src, u64 size, u64 srcOffset = 0, u64 dstOffset = 0) const;
		[[nodiscard]] VkBuffer GetVkBuffer() const noexcept { return buffer; }

		// Public Vulkan handles for compatibility
		VkBuffer buffer = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		VmaAllocation allocation = VK_NULL_HANDLE;
		VmaAllocationInfo allocationInfo = {};
		GPUBufferInfo info = {};
	};

} // namespace Renderer
