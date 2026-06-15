//
// Created by Orgest on 12/20/2025.
//

#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

#include "RenderInterface.h"
#include "VulkanCommandBuffer.h"
#include "VulkanPipeline.h"
#include "Tools/Logger.h"

namespace Renderer
{
    struct VulkanInstance;

    // ImmediateSubmitter: Provides single-use command buffer submission for uploads and transitions
    struct ImmediateSubmitter
    {
        VulkanDevice* device = nullptr;
        VulkanFence immFence;
        VulkanCommandBuffer cmdBuffer;

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
        ~VulkanDevice() override { Destroy(); }
        void WaitIdle() override { vkDeviceWaitIdle(device); }

        [[nodiscard]] const GPUDeviceDesc& GetDeviceDesc() const override { return deviceDesc; }
        [[nodiscard]] SampleCount GetMaxUsableSampleCount() const override;


        // Vulkan-specific initialization (backward compatibility)
        bool Init(VulkanInstance* inst);

        // Immediate command submission (RHI template in GPUDevice)
        void ImmediateSubmit(std::function<void(GPUCommandBuffer*)> func) override;

        template <typename Func>
        void InternalImmediateSubmit(Func&& function, const char* debugLabel = nullptr)
        {
            immediateSubmitter.Submit(std::forward<Func>(function), debugLabel);
        }


        // Factory Methods!!
        [[nodiscard]] std::unique_ptr<GPUTexture> CreateTexture(const TextureInfo& info) override;
        [[nodiscard]] std::unique_ptr<GPUTextureView> CreateTextureView(
            GPUTexture* texture, const TextureViewInfo& info) override;
        [[nodiscard]] std::unique_ptr<GPUSampler> CreateSampler(SamplerInfo& info) override;
        [[nodiscard]] std::unique_ptr<GPUBuffer> CreateBuffer(BufferInfo& info) override;
        [[nodiscard]] std::shared_ptr<GPUShader> CreateShader(Span<const u32> code) override;
        [[nodiscard]] std::shared_ptr<GPUShader> CreateShaderPath(const char* path) override;
        [[nodiscard]] std::unique_ptr<GPUPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        [[nodiscard]] std::unique_ptr<GPUPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override;
        [[nodiscard]] std::unique_ptr<GPUShaderBuffer> CreateShaderBuffer(
            DescriptorAllocatorGrowable* alloc, const DescriptorSetLayoutDesc& desc) override;

        VkDevice device = VK_NULL_HANDLE;
        VulkanInstance* instance = nullptr;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 deviceProperties = {};
        GPUDeviceDesc deviceDesc = {};
        VmaAllocator allocator = nullptr;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        ImmediateSubmitter immediateSubmitter;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        u32 graphicsQueueIndex = 0;
        bool useUnifiedLayout = true;
    };

    template <typename Func>
    void ImmediateSubmitter::Submit(Func&& function, const char* labelName, f32 r, f32 g, f32 b)
    {
        immFence.Reset();

        cmdBuffer.Begin();
        cmdBuffer.BeginDebugLabel(labelName, r, g, b);

        function(&cmdBuffer);

        cmdBuffer.EndDebugLabel();
        cmdBuffer.End();

        VkCommandBufferSubmitInfo cmdInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmdBuffer.GetVkHandle()
        };
        const VkSubmitInfo2 submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdInfo
        };
        vkQueueSubmit2(device->graphicsQueue, 1, &submitInfo, immFence.fence);
        vkWaitForFences(device->device, 1, &immFence.fence, VK_TRUE, UINT64_MAX);
        cmdBuffer.CollectTracy();
    }
}
