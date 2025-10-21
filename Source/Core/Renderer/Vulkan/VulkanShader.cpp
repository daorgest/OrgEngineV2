//
// Created by Orgest on 6/15/2025.
//

#include "VulkanShader.h"

#include <span>
#include <volk.h>

#include "VulkanInit.h"
#include "Tools/Logger.h"
#include "Tools/Vector.h"

using namespace Renderer;

Vector<u32> VulkanShader::ReadShaderFile(const char* filePath)
{
	FILE* file = std::fopen(filePath, "rb");
	if (file == nullptr)
	{
		LOG(Error, "Failed to open file: {}", filePath);
		return {};
	}

	// Seek to end to get file size
	std::fseek(file, 0, SEEK_END);

	i32 fileSize = std::ftell(file);
	if (fileSize < 0)
	{
		LOG(Error, "Failed to get file size: {}", filePath);
		std::fclose(file);
		return {};
	}

	// Seek back to beginning instead of rewind()
	if (std::fseek(file, 0, SEEK_SET) != 0)
	{
		LOG(Error, "Failed to rewind file: {}", filePath);
		std::fclose(file);
		return {};
	}

	if (fileSize <= 0)
	{
		LOG(Error, "File size is invalid: {}", filePath);
		std::fclose(file);
		return {};
	}

	if (fileSize % sizeof(u32) != 0)
	{
		LOG(Error, "File size is not aligned to u32 / not a binary file: {}", filePath);
		std::fclose(file);
		return {};
	}

	Vector<u32> buffer(fileSize / sizeof(u32));
	const size_t readSize = std::fread(buffer.data(), sizeof(u32), buffer.size(), file);
	std::fclose(file);

	if (readSize != buffer.size())
	{
		LOG(Error, "Failed to read entire file: {}", filePath);
		return {};
	}

	return buffer;
}

VulkanShader::VulkanShader(VulkanDevice* device, std::span<const u32> code, const ShaderFormat format)
{
	this->device = device;
	this->format = format;

	const VkShaderModuleCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size_bytes(),
		.pCode = code.data(),
	};

	if (vkCreateShaderModule(device->device, &createInfo, nullptr, &shader) != VK_SUCCESS)
	{
		LOG(Error, "Failed to create shader module.");
		shader = VK_NULL_HANDLE;
	}
}

void VulkanShader::Destroy() const
{
	if (shader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(device->device, shader, nullptr);
	}
}
