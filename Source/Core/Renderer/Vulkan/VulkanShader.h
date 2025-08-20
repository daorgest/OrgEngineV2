//
// Created by Orgest on 6/15/2025.
//

#pragma once

#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "Vector.h"

namespace Renderer
{
	struct VulkanDevice;

	struct VulkanShader : GPUShader
	{
		VkShaderModule shader = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		ShaderFormat format = ShaderFormat::SPIRV;

		VulkanShader() = default;

		static Vector<u32> ReadShaderFile(const char* filePath);
		VulkanShader(VulkanDevice* device, std::span<const u32> code, ShaderFormat format = ShaderFormat::SPIRV);
		void Destroy() const;
	};
}
