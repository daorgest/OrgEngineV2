//
// Created by Orgest on 6/15/2025.
//

#pragma once

#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "RenderInterface.h"

namespace Renderer
{
	struct VulkanDevice;

	/// Vulkan implementation of GPUShader
	struct VulkanShader final : GPUShader
	{
	    VulkanShader(VulkanDevice* device, std::span<const u32> code, ShaderFormat format = ShaderFormat::SPIRV);
	    ~VulkanShader() override { Destroy(); };
        void Destroy() const;

		// Public Vulkan handles for compatibility
		VkShaderModule shader = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		ShaderFormat format = ShaderFormat::SPIRV;
	};

} // namespace Renderer

