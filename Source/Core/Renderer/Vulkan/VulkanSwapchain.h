//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include "RendererTypes.h"
#include "RenderInterface.h"
#include "VulkanTexture.h"
#include "Tools/Vector.h"

constexpr u32 UNDEFINED_EXTENT = UINT32_MAX;
constexpr u32 DOUBLE_BUFFERING = 2;
constexpr u32 TRIPLE_BUFFERING = 3;

namespace Renderer
{
    using WindowHandle = void*;

    struct VulkanDevice;

    /// Vulkan implementation of GPUSwapchain
    struct VulkanSwapchain final : GPUSwapchain
    {
        // RHI interface implementation
        Result<void> Init(GPUDevice* devicePtr, WindowHandle windowHandle_) override;
        void Destroy() override;
        bool ResizeIfNeeded() override;
        [[nodiscard]] VulkanTexture* GetCurrentImage() override;

        [[nodiscard]] Extent2D GetExtent() const override
        {
            return {width, height};
        }
        // Vulkan-specific methods
        void CreateImages();
        void DestroyImageViews();
        void CreateDepthImage();
        void CreateShadowMap();
        void CreateMsaaColorImage();
        void DestroyDepthImage();
        void SetVsyncMode(PresentMode mode) override;
        void SetBufferingMode(BufferingMode mode) override;
        void SetMSAASamples(SampleCount samples) override;
        bool Recreate();

        [[nodiscard]] VulkanTexture* GetImage(u32 index) override;

        // C++23: Delete copy, allow move
        VulkanSwapchain() = default;
        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
        VulkanSwapchain(VulkanSwapchain&&) noexcept = default;
        VulkanSwapchain& operator=(VulkanSwapchain&&) noexcept = default;

        ~VulkanSwapchain() override { Destroy(); }
        PresentMode GetPresentMode() override { return presentMode; }
        Result<u32> AcquireNextImage(GPUSemaphore* semaphore, u32& imageIndex) override;
        Platform::WindowHandle GetWindowHandle() const override { return handle; }
        [[nodiscard]] f32 GetAspectRatio() const override;
        [[nodiscard]] SampleCount GetMSAASamples() override;

        // Public Vulkan handles for compatibility
        VulkanDevice* vkDev = nullptr;
        WindowHandle handle = nullptr;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR surfaceFormat = {
            .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };

        u32 imageCount = 0;
        u32 width = 0;
        u32 height = 0;
        u32 currentImageIndex = 0;
        PresentMode presentMode = PresentMode::VSyncOn;
        BufferingMode bufferingMode = BufferingMode::Double;
        SampleCount currentSamples = SampleCount::X4;
        bool needsRecreation = false;

        Vector<VulkanTexture> images;
        VulkanTexture depthTexture;
        VulkanTexture msaaColorImage;
        VulkanTexture shadowTexture;
    };
} // namespace Renderer