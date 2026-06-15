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
        Result<void> Init(GPUDevice* device, WindowHandle windowHandle) override;
        void Destroy() override;
        bool ResizeIfNeeded() override;
        void NeedsReCreation() override;

        [[nodiscard]] Result<u32> AcquireNextImage(GPUSemaphore* semaphore) override;

        [[nodiscard]] GPUTexture* GetCurrentImage() override;
        [[nodiscard]] GPUTexture* GetImage(u32 index) override;

        [[nodiscard]] Extent2D GetExtent() const override { return {width, height}; }
        [[nodiscard]] Extent2D GetRenderExtent() const override { return {renderWidth, renderHeight}; }
        [[nodiscard]] f32 GetAspectRatio() const override { return static_cast<f32>(width) / static_cast<f32>(height); }
        [[nodiscard]] f32 GetRenderScale() const override { return renderScale; }
        [[nodiscard]] Platform::WindowHandle GetWindowHandle() const override { return handle; }
        [[nodiscard]] PresentMode GetPresentMode() const override { return presentMode; };

        void SetRenderScale(f32 scale) override;
        void SetVsyncMode(PresentMode mode) override;
        void SetBufferingMode(BufferingMode mode) override;

        VulkanSwapchain() = default;
        ~VulkanSwapchain() override { Destroy(); }

        void CreateImages();
        void DestroySwapchainTextures();
        bool Recreate();

        VkSurfaceFormatKHR PickSurfaceFormat(const Vector<VkSurfaceFormatKHR>& availableFormats) const;

        VulkanDevice* vkDev = nullptr;
        Platform::WindowHandle handle = nullptr;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

        u32 width = 0, height = 0;
        u32 renderWidth = 0, renderHeight = 0;
        f32 renderScale = 1.0f;
        u32 currentImageIndex = 0;
        u32 imageCount = 0;
        bool preferHDR = false;

        PresentMode presentMode = PresentMode::VSyncOn;
        BufferingMode bufferingMode = BufferingMode::Double;
        bool needsRecreation = false;
        bool justRecreated = false;

        Vector<VulkanTexture> images;
    };
} // namespace Renderer