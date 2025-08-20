//
// Created by Orgest on 7/6/2025.
//

#include "MeshLoader.h"

#include "MeshData.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <expected>
#include <tiny_obj_loader.h>
#include <unordered_map>

#include "Logger.h"

using namespace Assets;

static std::string GetBaseDir(const std::string& filepath)
{
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

std::expected<LoadedModel, std::string> MeshLoader::LoadOBJ(const char* filePath)
{
	using namespace tinyobj;

	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string warn;
	std::string err;


	std::string baseDir = GetBaseDir(filePath);
	if (baseDir.empty()) {
		baseDir = ".";
	}
#ifdef _WIN32
	baseDir += "\\";
#else
	base_dir += "/";
#endif

	if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath, baseDir.c_str(), true)) {
		return std::unexpected(fmt::format("TinyObj failed: {} {}", warn, err));
	}

	LoadedModel loaded;
	loaded.sourceType = MeshSourceType::OBJ;

	// Convert materials
	// Load .mtl materials
	for (const auto& mat : materials)
	{
		Material info;
		info.name = mat.name;

		if (!mat.diffuse_texname.empty())
			info.albedoPath = baseDir + "/" + mat.diffuse_texname;

		if (!mat.normal_texname.empty())
			info.normalPath = baseDir + "/" + mat.normal_texname;

		if (!mat.specular_texname.empty())
			info.specularPath = baseDir + "/" + mat.specular_texname;

		if (!mat.emissive_texname.empty())
			info.emissivePath = baseDir + "/" + mat.emissive_texname;

		loaded.materials.push_back(std::move(info));
	}

	// Convert shapes to meshes
	for (const auto& shape : shapes)
	{
		Mesh mesh;
		mesh.name = shape.name;

		MeshPart part;
		// Assign material index from .obj
		int matId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
		part.materialIndex = (matId >= 0 && matId < static_cast<int>(materials.size())) ? static_cast<u32>(matId) : 0;


		std::unordered_map<Vertex, u32> uniqueVertices;

		for (const auto& [vertex_index, normal_index, texcoord_index] : shape.mesh.indices)
		{
			Vertex v{};

			if (vertex_index >= 0) {
				v.position = {
					attrib.vertices[3 * vertex_index + 0],
					attrib.vertices[3 * vertex_index + 1],
					attrib.vertices[3 * vertex_index + 2]
				};
			}
			if (normal_index >= 0) {
				v.normal = {
					attrib.normals[3 * normal_index + 0],
					attrib.normals[3 * normal_index + 1],
					attrib.normals[3 * normal_index + 2]
				};
			}
			if (texcoord_index >= 0) {
				v.uv = {
					attrib.texcoords[2 * texcoord_index + 0],
					1.0f - attrib.texcoords[2 * texcoord_index + 1]
				};
			}

			v.color = { 1.0f, 1.0f, 1.0f };

			auto it = uniqueVertices.find(v);
			if (it == uniqueVertices.end()) {
				u32 newIndex = static_cast<u32>(part.vertices.size());
				uniqueVertices[v] = newIndex;
				part.vertices.push_back(v);
				part.indices.push_back(newIndex);
			} else {
				part.indices.push_back(it->second);
			}
		}

		mesh.parts.push_back(std::move(part));
		loaded.meshes.push_back(std::move(mesh));
	}

	return loaded;
}

std::expected<LoadedModel, std::string> MeshLoader::LoadModelFromSource(MeshSourceType type, const void* data)
{
	switch (type)
	{
	case MeshSourceType::OBJ:
		return LoadOBJ(static_cast<const char*>(data));

	case MeshSourceType::OrgPack:
		return std::unexpected("OrgPack loading not implemented yet");

	case MeshSourceType::FBX:
		return std::unexpected("FBX loading not implemented yet");

	case MeshSourceType::Runtime:
		return std::unexpected("Runtime mesh loading not implemented");

	default:
		return std::unexpected("Unsupported source type");
	}
}
