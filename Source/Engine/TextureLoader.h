//
// Created by Orgest on 7/5/2025.
//

#pragma once
#include <string_view>
#include "../Core/Renderer/RendererTypes.h"

struct TextureLoader
{
	static Result<TextureData> LoadTextureFromSTB(std::string_view path, bool srgb);
	static Result<TextureData> LoadHDRTextureFromSTB(std::string_view path);
};