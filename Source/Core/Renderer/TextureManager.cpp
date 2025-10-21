//
// Created by Orgest on 7/5/2025.
//

#include "TextureManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "../Tools/Logger.h"

Result<TextureData> TextureManager::LoadTextureFromSTB(std::string_view path, bool srgb)
{
	i32 texWidth;
	i32 texHeight;

	// We're always doing 4 channels
	stbi_uc* pixels = stbi_load(path.data(), &texWidth, &texHeight, nullptr, STBI_rgb_alpha);
	if (pixels == nullptr)
	{
		return std::unexpected(AssetLoadFailed);
	}

	const size_t size = static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4;
	TextureData data = {
		.width = texWidth,
		.height = texHeight,
		.depth = 1,
		.channels = 4,
		.format = srgb ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNORM,
		.data = Vector<u8>(pixels, pixels + size),
	};

	stbi_image_free(pixels);
	return data;
}
