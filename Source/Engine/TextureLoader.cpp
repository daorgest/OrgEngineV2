//
// Created by Orgest on 7/5/2025.
//

#include "../../Engine/TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <ostream>
#include <stb_image.h>

#include "Tools/DDSHeader.h"
#include "Tools/FileManager.h"
#include "Tools/Logger.h"

using namespace Renderer;


Result<TextureData> TextureLoader::LoadTextureFromDDS(const char* path)
{
    auto fileRes = FileManager::Open(path, "rb");
    if (!fileRes)
    {
        return std::unexpected(fileRes.error());
    }

    const FileManager::Handle& file = fileRes.value();

    // Magic check
    u32 magic = 0;
    if (auto res = file.read<u32>(SPAN_ONE(magic)); !res) return std::unexpected(res.error());

    if (magic != DDS_MAGIC)
    {
        return std::unexpected(InvalidFileFormat);
    }

    // Header
    DDS_HEADER header = {};
    if (auto res = file.read<DDS_HEADER>(SPAN_ONE(header)); !res) return std::unexpected(res.error());
    if (header.dwSize != 124)
    {
        return std::unexpected(InvalidFileFormat);
    }

    TextureFormat parsedFormat = TextureFormat::UNKNOWN;
    u32 parsedArrayLayers = 1;

    if (header.ddspf.dwFourCC == 0x30315844) // "DX10"
    {
        DDS_HEADER_DXT10 dx10Header = {};
        if (auto res = file.read<DDS_HEADER_DXT10>({&dx10Header, 1}); !res) return std::unexpected(res.error());

        parsedArrayLayers = std::max(1u, dx10Header.arraySize);

        switch (dx10Header.dxgiFormat)
        {
        case 98: parsedFormat = TextureFormat::BC7_UNORM_BLOCK; break;
        case 99: parsedFormat = TextureFormat::BC7_SRGB_BLOCK; break;
        case 95: parsedFormat = TextureFormat::BC6H_SFLOAT_BLOCK; break;
        case 71: parsedFormat = TextureFormat::BC1_RGB_UNORM_BLOCK; break;
        case 77: parsedFormat = TextureFormat::BC3_UNORM_BLOCK; break;
        default:
            LOG(Error, "Unsupported DXGI Format: {}", dx10Header.dxgiFormat);
            return std::unexpected(InvalidFileFormat);
        }
    }
    else // Legacy DXT1/DXT5
    {
        switch (header.ddspf.dwFourCC)
        {
        case 0x31545844: parsedFormat = TextureFormat::BC1_RGB_UNORM_BLOCK; break; // DXT1
        case 0x35545844: parsedFormat = TextureFormat::BC3_UNORM_BLOCK; break;     // DXT5
        default: return std::unexpected(InvalidFileFormat);
        }
    }


    const i32 remainingBytes = file.size() - file.tell();
    Vector<u8> buffer(remainingBytes);
    if (auto res = file.read<u8>(buffer); !res) return std::unexpected(res.error());

    return TextureData {
        .width       = static_cast<i32>(header.dwWidth),
        .height      = static_cast<i32>(header.dwHeight),
        .depth       = static_cast<i32>(std::max(1u, header.dwDepth)),
        .mipLevels   = static_cast<u16>(std::max(1u, header.dwMipMapCount)),
        .arrayLayers = static_cast<u16>(parsedArrayLayers),
        .format      = parsedFormat,
        .data        = std::move(buffer)
    };
}

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
		.data = Vector<u8>({pixels, size}),
	};

	stbi_image_free(pixels);
	return data;
}

Result<TextureData> TextureLoader::LoadTextureFromMemory(Span<const u8> buffer, bool srgb)
{
    i32 texWidth;
    i32 texHeight;
    i32 channels;

    stbi_uc* pixels = stbi_load_from_memory(buffer.data(), static_cast<i32>(buffer.size()), &texWidth, &texHeight, &channels, STBI_rgb_alpha);

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
        .data = Vector<u8>({pixels, size}),
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
        .data = Vector<f32>({pixels, size}),
     };

    stbi_image_free(pixels);
    return data;
}