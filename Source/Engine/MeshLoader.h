//
// Created by Orgest on 7/6/2025.
//

#pragma once
#include <expected>
#include <filesystem>

#include "MeshData.h"

namespace Assets
{
	class MeshLoader
	{
	public:
		static Result<LoadedModel> LoadModelFromSource(MeshSourceType type, const std::filesystem::path& path);

	private:
		static Result<LoadedModel> LoadOBJ(const std::filesystem::path& path);
		static Result<LoadedModel> LoadFBX(const std::filesystem::path& path);

		static void PostProcessMesh(Mesh& mesh, bool hasNormals, bool hasUVs);
		static void GenerateNormals(Vector<Vertex>& verts, const Vector<u32>& indices);
	};
}
