//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include "VulkanTexture.h"

#include "RendererTypes.h"
#include "RenderInterface.h"
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
        GPUTexture* GetCurrentImage() override;

        [[nodiscard]] const Extent2D GetExtent() const override
        {
            return {width, height};
        }

        [[nodiscard]] TextureFormat GetFormat() const override;

        // Vulkan-specific methods
        void CreateImages();
        void DestroyImageViews();
        void CreateDepthImage();
        void DestroyDepthImage();
        void SetVsyncMode(PresentMode mode) override;
        void SetBufferingMode(BufferingMode mode) override;
        bool Recreate();

        // Vulkan-specific accessors
        [[nodiscard]] GPUTexture* GetImage(u32 index) override;
        [[nodiscard]] VkFormat GetVkFormat() const { return surfaceFormat.format; }
        [[nodiscard]] VkSwapchainKHR GetVkSwapchain() const noexcept { return swapchain; }
        [[nodiscard]] VkSurfaceKHR GetVkSurface() const noexcept { return surface; }

        // C++23: Delete copy, allow move
        VulkanSwapchain() = default;
        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
        VulkanSwapchain(VulkanSwapchain&&) noexcept = default;
        VulkanSwapchain& operator=(VulkanSwapchain&&) noexcept = default;

        ~VulkanSwapchain() override { Destroy(); }
        PresentMode GetPresentMode() override { return presentMode; }
        Result<u32> AcquireNextImage(GPUSemaphore* semaphore) override;

        // Public Vulkan handles for compatibility
        VulkanDevice* vkDev = nullptr;
        WindowHandle handle = nullptr;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        PresentMode presentMode = PresentMode::VSyncOn;
        BufferingMode bufferingMode = BufferingMode::Double;
        VkSurfaceFormatKHR surfaceFormat = {
            .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        Vector<VulkanTexture> images;
        VulkanTexture depthTexture;

        u32 imageCount = 0;
        u32 width = 0;
        u32 height = 0;
        u32 currentImageIndex = 0;
        bool needsRecreation = false;
    };
} // namespace Renderer