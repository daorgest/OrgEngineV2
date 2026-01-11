//
// Created by Orgest on 6/15/2025.
//

#pragma once

#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct VulkanDevice;

	/// Vulkan implementation of GPUShader
	struct VulkanShader final : GPUShader
	{
		// RHI interface implementation
		Result<void> Init(GPUDevice* device, std::span<const u32> code, ShaderFormat format = ShaderFormat::SPIRV) override;
        void Destroy();

        // Vulkan-specific constructor (backward compatibility)
		VulkanShader() = default;
		VulkanShader(VulkanDevice* device, std::span<const u32> code, ShaderFormat format = ShaderFormat::SPIRV);

		// Static utility
		static Result<Vector<u32>> ReadShaderFile(const char* filePath);

		// Public Vulkan handles for compatibility
		VkShaderModule shader = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		ShaderFormat format = ShaderFormat::SPIRV;
	};

} // namespace Renderer

