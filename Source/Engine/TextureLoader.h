//
// Created by Orgest on 7/5/2025.
//

#pragma once
#include <string_view>
#include "../Core/Renderer/RendererTypes.h"

struct TextureLoader
{
    static Result<Renderer::TextureData> LoadTextureFromDDS(const char* path);
	static Result<Renderer::TextureData> LoadTextureFromSTB(std::string_view path, bool srgb);
    static Result<Renderer::TextureData> LoadTextureFromMemory(Span<const u8> buffer, bool srgb);
    static Result<Renderer::TextureData> LoadHDRTextureFromSTB(std::string_view path);
};