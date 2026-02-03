//
// Created by Orgest on 6/15/2025.
//

#include "VulkanShader.h"

#include <span>
#include <volk.h>

#include "VulkanDevice.h"
#include "Tools/Logger.h"
#include "Tools/FileManager.h"

using namespace Renderer;

VulkanShader::VulkanShader(VulkanDevice* dev, std::span<const u32> code, ShaderFormat fmt)
{
    device = dev;
    format = fmt;

    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size_bytes(),
        .pCode = code.data(),
    };

    VK_CHECK(vkCreateShaderModule(device->device, &createInfo, nullptr, &shader));
}

void VulkanShader::Destroy() const
{
	if (shader)
	{
		vkDestroyShaderModule(device->device, shader, nullptr);
	}
}