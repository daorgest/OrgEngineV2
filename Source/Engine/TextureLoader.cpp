//
// Created by Orgest on 7/5/2025.
//

#include "../../Engine/TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Result<TextureData> TextureLoader::LoadTextureFromSTB(std::string_view path, bool srgb)
{
	i32 texWidth;
	i32 texHeight;

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
		.data = Vector(pixels, pixels + size),
	};

	stbi_image_free(pixels);
	return data;
}

Result<TextureData> TextureLoader::LoadHDRTextureFromSTB(std::string_view path)
{
    i32 texWidth, texHeight, channelsInFile;
    f32* pixels = stbi_loadf(path.data(), &texWidth, &texHeight, &channelsInFile, 4);
    if (pixels == nullptr)
    {
        return std::unexpected(AssetLoadFailed);
    }

    const size_t size = static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4;

    TextureData data = {
        .width = texWidth, .height = texHeight, .depth = 1, .channels = 4,
        .format = TextureFormat::RGBA32_SFLOAT,
        .data = Vector(pixels, pixels + size),
     };

    stbi_image_free(pixels);
    return data;
}
