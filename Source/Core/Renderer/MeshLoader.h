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
		static Result<LoadedModel> LoadFBX(const char* path);
		static Result<LoadedModel> LoadOBJ(const char* filePath);
		static Result<LoadedModel> LoadModelFromSource(MeshSourceType type, const void *data);
	};
}
