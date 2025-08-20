//
// Created by Orgest on 7/16/2025.
//
#pragma once

#include <cstdint>

// barebones for now, will expand as i start to get my model formats/texture formats packable for now
enum class FileType : uint8_t
{
	Image,
	Audio,
	Mesh,
	Script,
	Binary,
	Unknown
};

enum class CompressionType : uint8_t
{
	LZ4,
	ZSTD,
	None
};

struct File
{
	uint64_t offset{};
	uint64_t uncompressedSize{};
	uint64_t compressedSize{};

	char name[96]{};

	// union
	// {
	// 	std::array<uint8_t, 16> guid; // in editor;
	// 	uint32_t runtimeID; // for runtime stuff
	// };

	FileType type{};
	CompressionType compression = CompressionType::None;
	uint8_t padding[6]{};
};
static_assert(sizeof(File) == 128, "File size mismatch");

struct Header
{
	char magic[8] = "ORGPACK";
	uint32_t version = 1;
	uint32_t fileCount = 0;
	uint64_t indexOffset = 0; // location of dict
	uint8_t padding[8]{};
};
static_assert(sizeof(Header) == 32, "Header size mismatch");
