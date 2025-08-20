//
// Created by Orgest on 7/6/2025.
//

#pragma once
#include <expected>
#include "MeshData.h"

namespace Assets
{
	struct MeshLoader
	{
		static std::expected<LoadedModel, std::string> LoadOBJ(const char* filePath);
		static std::expected<LoadedModel, std::string> LoadModelFromSource(MeshSourceType type, const void *data);
	};
}
