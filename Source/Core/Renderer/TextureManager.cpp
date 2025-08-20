//
// Created by Orgest on 7/5/2025.
//

#include "TextureManager.h"

#include <optional>

#define STB_IMAGE_IMPLEMENTATION
#include <fstream>
#include <stb_image.h>

#include "Logger.h"

std::optional<TextureData> TextureManager::LoadTextureFromSTB(const std::string_view& path, bool srgb)
{
	i32 texWidth;
	i32 texHeight;

	// We always doing 4 channels
	stbi_uc* pixels = stbi_load(path.data(), &texWidth, &texHeight, nullptr, STBI_rgb_alpha);
	if (pixels == nullptr)
	{
		LOG(Warning, "Failed to load texture from {}", path.data());
		return std::nullopt;
	}

	// Determine TextureFormat based on channel count & srgb
	TextureFormat format = srgb ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNORM;

	size_t size = texWidth * texHeight * 4;
	TextureData data = {
		.width = texWidth,
		.height = texHeight,
		.depth = 1,
		.channels = 4,
		.format = format,
		.data = Vector<u8>(pixels, pixels + size)
	};

	stbi_image_free(pixels);
	return data;
}
