//
// Created by Orgest on 6/16/2025.
//

#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

#include "RendererTypes.h"
#include "RenderInterface.h"

namespace Renderer
{
    struct VulkanDevice;

    struct VulkanBuffer final : GPUBuffer
    {
        VulkanBuffer(GPUDevice* devicePtr, const BufferInfo& bufferInfo);
        ~VulkanBuffer() override { Destroy(); }
        void Destroy() override;

        void Init(GPUDevice* device, const BufferInfo& inputInfo) override;
        [[nodiscard]] void* Map() const override;
        void Unmap() const override;
        void Upload(const void* srcData, u64 size) const override;
        [[nodiscard]] u64 GetSize() const override { return info.size; }
        [[nodiscard]] u64 GetDeviceAddress() const override;
        [[nodiscard]] bool IsValid() const override { return buffer != VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer GetVkBuffer() const noexcept { return buffer; }

        VulkanBuffer(VulkanBuffer&& other) noexcept { *this = std::move(other); }
        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept
        {
            if (this != &other)
            {
                Destroy();
                buffer = other.buffer;
                device = other.device;
                allocation = other.allocation;
                allocationInfo = other.allocationInfo;
                info = other.info;

                other.buffer = VK_NULL_HANDLE;
                other.allocation = VK_NULL_HANDLE;
            }
            return *this;
        }

        VkBuffer buffer = VK_NULL_HANDLE;
        VulkanDevice* device = nullptr;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};
        BufferInfo info = {};
    };
} // namespace Renderer
