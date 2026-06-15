//
// Created by Orgest on 7/6/2025.
//

#pragma once
#include <expected>
#include <filesystem>

#include "MeshData.h"

namespace fastgltf
{
    class Asset;
}

namespace Renderer
{
    struct BindlessManager;
}

template <typename T>
class AssetPool;

namespace Assets
{
    struct MeshLoader
    {
        static Result<LoadedModel> LoadModelFromSource(Renderer::MeshSourceType type, const std::filesystem::path& path,
                                                       AssetPool<Renderer::TextureData>* texturePool = nullptr);

    private:
        static Result<LoadedModel> LoadGLTF(const std::filesystem::path& path,
                                            AssetPool<Renderer::TextureData>* texturePool);
        static void ParseGLTFMaterials(fastgltf::Asset& gltf, const std::filesystem::path& gltfPath,
                                       AssetPool<Renderer::TextureData>* texturePool, Vector<
                                           Material>& outMaterials);
        static void ParseGLTFMeshes(fastgltf::Asset& gltf, Vector<Mesh>& outMeshes);

        static Result<LoadedModel> LoadOBJ(const std::filesystem::path& path, AssetPool<Renderer::TextureData>* texturePool);

	    // Utils
		static void PostProcessMesh(Mesh& mesh, bool hasNormals, bool hasUVs);
		static void GenerateNormals(Vector<Vertex>& verts, const Vector<u32>& indices);
	};
}
