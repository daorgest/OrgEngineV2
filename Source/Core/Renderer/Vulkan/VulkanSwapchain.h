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
        bool Init(GPUDevice* device, void* windowHandle) override;
        void Destroy() override;
        bool Resize() override;
        u32 AcquireNextImage(void* semaphore) override;
        void Present(u32 imageIndex, void* waitSemaphore) override;
        GPUTexture* GetCurrentImage() override;

        [[nodiscard]] const Extent2D& GetExtent() const override
        {
            thread_local Extent2D extent;
            extent = {width, height};
            return extent;
        }

        [[nodiscard]] TextureFormat GetFormat() const override;

        // Vulkan-specific initialization (backward compatibility)
        bool Init(VulkanDevice* device, WindowHandle handle);

        // Vulkan-specific methods
        void CreateImages();
        void DestroyImageViews();
        void CreateDepthImage();
        void DestroyDepthImage() const;
        void VsyncEnable(PresentMode mode);
        void SetBufferingMode(BufferingMode mode);
        bool Recreate();

        // Vulkan-specific accessors
        [[nodiscard]] const VulkanImage& GetImage(u32 index) const;
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

        // Public Vulkan handles for compatibility
        VulkanDevice* device = nullptr;
        WindowHandle handle = nullptr;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        PresentMode presentMode = PresentMode::VSyncOn;
        BufferingMode bufferingMode = BufferingMode::Double;
        VkSurfaceFormatKHR surfaceFormat = {
            .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        Vector<VulkanImage> images;
        VulkanImage depthImage;

        u32 imageCount = 0;
        u32 width = 0;
        u32 height = 0;
        u32 currentImageIndex = 0;
        bool needsRecreation = false;
    };
} // namespace Renderer