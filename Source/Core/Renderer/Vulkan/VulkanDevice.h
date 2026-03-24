//
// Created by Orgest on 12/20/2025.
//

#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

#include "RenderInterface.h"
#include "VulkanCheck.h"
#include "VulkanDebugUtils.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"
#include "Tools/Logger.h"

namespace Renderer
{
    struct VulkanInstance;

    // ImmediateSubmitter: Provides single-use command buffer submission for uploads and transitions
    struct ImmediateSubmitter
    {
        VulkanDevice* device = nullptr;
        VkFence immFence = VK_NULL_HANDLE;
        VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
        VkCommandPool immCommandPool = VK_NULL_HANDLE;

        ImmediateSubmitter() = default;
        void Init(VulkanDevice* inDevice);
        void Destroy();

        template <typename Func>
        void Submit(Func&& function, const char* labelName = "Immediate Submit", f32 r = 0.5f, f32 g = 0.5f,
                    f32 b = 0.5f);
    };

    struct VulkanDevice final : GPUDevice
    {
        // RHI interface implementation
        bool Init(GPUInterface* instance) override;
        void Destroy() override;
        void WaitIdle() override { vkDeviceWaitIdle(device); }
        [[nodiscard]] const GPUDeviceDesc& GetDeviceDesc() const override { return deviceDesc; }

        // Vulkan-specific initialization (backward compatibility)
        bool Init(VulkanInstance* inst);

        // Immediate command submission (RHI template in GPUDevice)
        void ImmediateSubmit(std::function<void(GPUCommandBuffer*)> func) override;

        template <typename Func>
        void InternalImmediateSubmit(Func&& function, const char* debugLabel = nullptr)
        {
            immediateSubmitter.Submit(std::forward<Func>(function), debugLabel);
        }


        // Vulkan-specific accessors
        [[nodiscard]] VkDevice GetVkDevice() const noexcept { return device; }
        [[nodiscard]] VkPhysicalDevice GetVkPhysicalDevice() const noexcept { return physicalDevice; }
        [[nodiscard]] VmaAllocator GetVmaAllocator() const noexcept { return allocator; }
        [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept { return graphicsQueue; }
        std::unique_ptr<GPUTexture> CreateTexture(TextureInfo& info) override;
        std::unique_ptr<GPUSampler> CreateSampler(SamplerInfo& info) override;
        std::unique_ptr<GPUBuffer> CreateBuffer(BufferInfo& info) override;
        std::unique_ptr<GPUShader> CreateShader(std::span<const u32> code) override;
        std::unique_ptr<GPUShader> CreateShaderPath(const char* path) override;
        std::unique_ptr<GPUPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        std::unique_ptr<GPUPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override;

        // Public Vulkan handles for compatibility
        VkDevice device = VK_NULL_HANDLE;
        VulkanInstance* instance = nullptr;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties deviceProperties = {};
        GPUDeviceDesc deviceDesc = {};
        VmaAllocator allocator = nullptr;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        u32 graphicsQueueIndex = 0;
        bool useUnifiedLayout = true;
        ImmediateSubmitter immediateSubmitter;
    };

    template <typename Func>
    void ImmediateSubmitter::Submit(Func&& function, const char* labelName, f32 r, f32 g, f32 b)
    {
        vkResetFences(device->device, 1, &immFence);
        vkResetCommandBuffer(immCommandBuffer, 0);

        VkCommandBuffer cmd = immCommandBuffer;

        constexpr VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
#if VULKAN_DEBUG_MODE
        CmdBeginLabel(cmd, labelName, r, g, b);
#endif

        function(cmd);

#if VULKAN_DEBUG_MODE
        CmdEndLabel(cmd);
#endif

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkCommandBufferSubmitInfo cmdInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd,
            .deviceMask = 0
        };
        const VkSubmitInfo2 submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdInfo
        };
        vkQueueSubmit2(device->graphicsQueue, 1, &submitInfo, immFence);
        vkWaitForFences(device->device, 1, &immFence, VK_TRUE, UINT64_MAX);
    }
}
