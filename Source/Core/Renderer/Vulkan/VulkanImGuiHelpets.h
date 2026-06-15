//
// Created by Orgest on 6/9/2026.
//

#pragma once
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include "RenderEnums.h"
#include "VulkanConvert.h"
#include "VulkanTexture.h"

namespace Renderer::UI
{
    inline ImTextureID AddTexture(GPUTextureView* view, const TextureLayout layout = TextureLayout::ShaderReadOnly)
    {
        if (!view) return 0;
        const auto* vkView = static_cast<VulkanTextureView*>(view);
        VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(vkView->imageView, ToVk(layout));
        return reinterpret_cast<ImTextureID>(descSet);
    }

    inline void RemoveTexture(const ImTextureID textureId)
    {
        if (textureId != ImTextureID_Invalid)
        {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(textureId));
        }
    }
}
