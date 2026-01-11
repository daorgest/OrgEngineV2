//
// Created by Orgest on 7/6/2025.
//

#include "MeshLoader.h"

#include "MeshData.h"
#include "Math/MikkWrapper.h"
#include "Tools/Logger.h"

#include <algorithm>
#include <expected>
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <print>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <ufbx.h>

#include "tracy/Tracy.hpp"

static std::string GetBaseDir(const std::string& filepath)
{
	const size_t pos = filepath.find_last_of("/\\");
	return (pos != std::string::npos) ? filepath.substr(0, pos) : "";
}

static std::string JoinPath(const std::string& dir, const std::string& filename)
{
	if (dir.empty()) return filename;
	const char lastChar = dir.back();
	if (lastChar != '/' && lastChar != '\\')
		return dir + "/" + filename;
	return dir + filename;
}

static void OptimizeMesh(Vector<Vertex>& vertices, Vector<u32>& indices, Vector<MeshPart>& parts)
{
    if (vertices.empty() || indices.empty()) return;

    const size_t indexCount = indices.size();
    const size_t vertexCount = vertices.size();

    // 1. Generate Remap Table
    Vector<u32> remap(indexCount);
	size_t uniqueVertexCount = meshopt_generateVertexRemap(
		remap.data(), indices.data(), indexCount, vertices.data(), vertexCount, sizeof(Vertex)
	);
    // 2. Apply Remap to create initial optimized buffers
	Vector<u32> optimizedIndices(indexCount);
	Vector<Vertex> optimizedVertices(uniqueVertexCount);

	meshopt_remapIndexBuffer(optimizedIndices.data(), indices.data(), indexCount, remap.data());
	meshopt_remapVertexBuffer(optimizedVertices.data(), vertices.data(), vertexCount, sizeof(Vertex), remap.data());

	// 2. Per-Part Optimization: Reorder indices ONLY within their material ranges
	// This prevents triangles from "bleeding" into other material draw calls.
	for (auto& part : parts)
	{
		u32* partIndices = &optimizedIndices[part.firstIndex];

		meshopt_optimizeVertexCache(partIndices, partIndices, part.indexCount, uniqueVertexCount);

		meshopt_optimizeOverdraw(
			partIndices, partIndices, part.indexCount,
			&optimizedVertices[0].position.x, uniqueVertexCount, sizeof(Vertex), 1.05f
		);
	}

	// 3. Global Vertex Fetch: Reorder the vertex buffer to match the new index order
	// This is safe to do globally because it updates all indices in 'optimizedIndices'.
	meshopt_optimizeVertexFetch(
		optimizedVertices.data(), optimizedIndices.data(), indexCount,
		optimizedVertices.data(), uniqueVertexCount, sizeof(Vertex)
	);

	vertices = std::move(optimizedVertices);
	indices = std::move(optimizedIndices);

	// 4. Final AABB Calculation: Must happen AFTER vertices have moved in the buffer
	for (auto& part : parts)
	{
		part.aabb = AABB(
			std::span<const u32>(&indices[part.firstIndex], part.indexCount),
			std::span<const Vertex>(vertices.data(), vertices.size())
		);
	}
}

using namespace Assets;

Result<LoadedModel> MeshLoader::LoadFBX(const char* path)
{
    ufbx_load_opts opts{};
	// opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;

	// Robustness
	opts.strict = true;
	opts.ignore_missing_external_files = true;
	opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_CLAMP;

	// Quality
	opts.generate_missing_normals = true;
	opts.normalize_normals = true;
	opts.use_blender_pbr_material = true;

	// Baking
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;

	// Speed
	opts.ignore_animation = true;

    ufbx_error error{};
    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
    if (!scene)
    {
        LOG(Error, "[ufbx] Failed to load '{}' : {}", path, error.description.data);
        return std::unexpected(AssetLoadFailed);
    }

	for (ufbx_node *node : scene->nodes) {
		printf("%s\n", node->name.data);
	}

    LoadedModel model{};
    model.sourceType = MeshSourceType::FBX;

    // 1. Process Materials (PBR Conversion)
    for (ufbx_material* umat : scene->materials)
    {
        Material out{};
        out.name = umat->name.data;
        out.baseColor = { umat->pbr.base_color.value_vec3.x, umat->pbr.base_color.value_vec3.y, umat->pbr.base_color.value_vec3.z };
        out.roughness = static_cast<float>(umat->pbr.roughness.value_real);
        out.metallic = static_cast<float>(umat->pbr.metalness.value_real);

        // Map texture paths using your existing logic
        if (umat->pbr.base_color.texture) out.albedoPath = umat->pbr.base_color.texture->filename.data;
        if (umat->pbr.normal_map.texture) out.normalPath = umat->pbr.normal_map.texture->filename.data;

        model.materials.push_back(std::move(out));
    }

    // 2. Process Meshes
    for (const ufbx_mesh* umesh : scene->meshes)
    {
        Mesh mesh{};
        mesh.name = umesh->name.data;

        // We accumulate all unique vertices/indices for this mesh
        Vector<Vertex> rawVertices;
        Vector<u32> rawIndices;

    	const bool hasNormals = umesh->vertex_normal.indices.count > 0;
    	const bool hasUVs = umesh->vertex_uv.indices.count > 0;

        for (const ufbx_mesh_part& part : umesh->material_parts)
        {
            if (part.num_triangles == 0) continue;

            MeshPart mp{};
            mp.materialIndex = part.index; // Standard ufbx part-to-material mapping
            mp.firstIndex = static_cast<u32>(rawIndices.size());
            mp.vertexOffset = 0;

            // Extract vertices per triangle
            for (uint32_t face_i : part.face_indices)
            {
                ufbx_face face = umesh->faces[face_i];
                uint32_t num_tris = face.num_indices - 2;

                for (uint32_t tri_i = 0; tri_i < num_tris; tri_i++)
                {
                    // Basic triangulation (fan)
                	uint32_t indices[3] = { 0, tri_i + 1, tri_i + 2 };
                	for (uint32_t i : indices)
                	{
                		uint32_t corner = face.index_begin + i;
                		Vertex v{};

                		// Position is always required
                		ufbx_vec3 p = ufbx_get_vertex_vec3(&umesh->vertex_position, corner);
                		v.position = { p.x, p.y, p.z };

                		// Safe Normal Fetch
                		if (hasNormals) {
                			ufbx_vec3 n = ufbx_get_vertex_vec3(&umesh->vertex_normal, corner);
                			v.normal = { n.x, n.y, n.z };
                		}

                		// Safe UV Fetch - THIS PREVENTS THE CRASH
                		if (hasUVs) {
                			ufbx_vec2 uv = ufbx_get_vertex_vec2(&umesh->vertex_uv, corner);
                			v.uv = { uv.x, 1.0f - uv.y };
                		} else {
                			v.uv = { 0.0f, 0.0f };
                		}

                		rawIndices.push_back(static_cast<u32>(rawVertices.size()));
                		rawVertices.push_back(v);
                	}
                }
            }
            mp.indexCount = static_cast<u32>(rawIndices.size()) - mp.firstIndex;
            mesh.parts.push_back(mp);
        }

        // 3. Optimize Mesh (Unified with OBJ Loader)
        mesh.unifiedVertices = std::move(rawVertices);
        mesh.unifiedIndices = std::move(rawIndices);

    	if (!mesh.unifiedIndices.empty())
    	{
    		OptimizeMesh(mesh.unifiedVertices, mesh.unifiedIndices, mesh.parts);
    	}

        model.meshes.push_back(std::move(mesh));
    }

    ufbx_free_scene(scene);
    return model;
}

static void GenerateNormals(Vector<Vertex>& verts, const Vector<u32>& indices)
{
	if (verts.empty() || indices.size() < 3) return;

	// zero out
	for (auto& v : verts) v.normal = {0, 0, 0};

	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const u32 i0 = indices[i + 0];
		const u32 i1 = indices[i + 1];
		const u32 i2 = indices[i + 2];
		const glm::vec3& p0 = verts[i0].position;
		const glm::vec3& p1 = verts[i1].position;
		const glm::vec3& p2 = verts[i2].position;

		const glm::vec3 fn = glm::cross(p1 - p0, p2 - p0); // area-weighted
		verts[i0].normal += fn;
		verts[i1].normal += fn;
		verts[i2].normal += fn;
	}

	for (auto& v : verts)
		v.normal = glm::normalize(v.normal);
}

static void BuildMeshPartsFromShape(
    const tinyobj::attrib_t& attrib,
    const tinyobj::shape_t& shape,
    const std::vector<tinyobj::material_t>& materials,
    Mesh& outMesh)
{
    std::unordered_map<int, Vector<u32>> materialGroups;

    struct VertHash {
        size_t operator()(const tinyobj::index_t& i) const {
            size_t h = 0;
            auto hash_combine = [](size_t& seed, int v) {
                seed ^= std::hash<int>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };
            hash_combine(h, i.vertex_index);
            hash_combine(h, i.normal_index);
            hash_combine(h, i.texcoord_index);
            return h;
        }
    };

    struct VertEq {
        bool operator()(const tinyobj::index_t& a, const tinyobj::index_t& b) const {
            return a.vertex_index == b.vertex_index &&
                   a.normal_index == b.normal_index &&
                   a.texcoord_index == b.texcoord_index;
        }
    };

    std::unordered_map<tinyobj::index_t, u32, VertHash, VertEq> uniqueVertices;

    size_t indexOffset = 0;
    const bool hasNormals = !attrib.normals.empty();
    const bool hasUVs = !attrib.texcoords.empty();

    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
    {
        int fv = shape.mesh.num_face_vertices[f];
        int matId = shape.mesh.material_ids[f];
        auto& groupIndices = materialGroups[matId];

        for (size_t v = 0; v < static_cast<size_t>(fv); ++v)
        {
            tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
            auto it = uniqueVertices.find(idx);

            if (it == uniqueVertices.end())
            {
                Vertex vert{};
                vert.position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };

                if (hasNormals && idx.normal_index >= 0) {
                    vert.normal = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    };
                }

                if (hasUVs && idx.texcoord_index >= 0) {
                    vert.uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                }

                u32 newIdx = static_cast<u32>(outMesh.unifiedVertices.size());
                uniqueVertices[idx] = newIdx;
                outMesh.unifiedVertices.push_back(vert);
                groupIndices.push_back(newIdx);
            }
            else {
                groupIndices.push_back(it->second);
            }
        }
        indexOffset += fv;
    }

	for (auto& [matId, indices] : materialGroups)
	{
		// 1. Capture global start position
		u32 firstIndex = static_cast<u32>(outMesh.unifiedIndices.size());

		// 2. Build AABB using the material-specific indices and current vertex buffer
		AABB partAABB(std::span<const u32>(indices), std::span<const Vertex>(outMesh.unifiedVertices));

		// 3. Move indices to global buffer via push_back
		for (u32 idx : indices) {
			outMesh.unifiedIndices.push_back(idx);
		}

		// 4. Create MeshPart
		outMesh.parts.emplace_back(MeshPart{
		   .aabb = partAABB,
		   .materialIndex = (matId >= 0) ? static_cast<u32>(matId) : 0u,
		   .indexCount = static_cast<u32>(indices.size()),
		   .firstIndex = firstIndex,
		   .vertexOffset = 0,
		   .localTransform = glm::mat4(1.0f)
		});
	}
}


Result<LoadedModel> MeshLoader::LoadOBJ(const char* filePath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	std::string baseDir = GetBaseDir(filePath);
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath, baseDir.c_str(), true))
	{
		LOG(Error, "[tinyobj] Failed to load '{}': {}", filePath, err);
		return std::unexpected(AssetLoadFailed);
	}

	LoadedModel loaded{ .sourceType = MeshSourceType::OBJ };

	// Material Map
	for (const auto& mat : materials)
	{
		Material out = {
			.name          = mat.name,

			// Texture Mapping using the provided tinyobj struct members
			.albedoPath    = !mat.diffuse_texname.empty()   ? JoinPath(baseDir, mat.diffuse_texname)   : "",
			.normalPath    = !mat.normal_texname.empty()    ? JoinPath(baseDir, mat.normal_texname):
							(!mat.bump_texname.empty()     ? JoinPath(baseDir, mat.bump_texname):
							(!mat.displacement_texname.empty() ? JoinPath(baseDir, mat.displacement_texname) : "")),
			.emissivePath  = !mat.emissive_texname.empty()  ? JoinPath(baseDir, mat.emissive_texname)  : "",
			.roughnessPath = !mat.roughness_texname.empty() ? JoinPath(baseDir, mat.roughness_texname) : "",
			.metallicPath  = !mat.metallic_texname.empty()  ? JoinPath(baseDir, mat.metallic_texname)  : "",
			.aoPath        = !mat.ambient_texname.empty()   ? JoinPath(baseDir, mat.ambient_texname)   : "",

			// // Logic for identifying the Render Path
			// .materialType  = (mat.dissolve < 1.0f || !mat.alpha_texname.empty())
			// 				  ? MaterialType::Transparent
			// 				  : MaterialType::Opaque,

			// PBR Properties and Fallbacks
			.baseColor     = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]),
			.roughness     = mat.roughness, // From tinyobj PBR extension (Pr)
			.metallic      = mat.metallic,  // From tinyobj PBR extension (Pm)
			.ior           = mat.ior,       // Index of Refraction (Ni)
			.opacity       = mat.dissolve,  // Dissolve (d)
			.emissive      = glm::vec3(mat.emission[0], mat.emission[1], mat.emission[2])
		};

		loaded.materials.push_back(std::move(out));
	}


	// Mesh Extraction
	for (const auto& shape : shapes)
	{
		loaded.meshes.emplace_back(Mesh{ .name = shape.name });
		Mesh& mesh = loaded.meshes.back();
		BuildMeshPartsFromShape(attrib, shape, materials, mesh);

		if (!mesh.unifiedIndices.empty())
		{
			OptimizeMesh(mesh.unifiedVertices, mesh.unifiedIndices, mesh.parts);

			// Generate Normals/Tangents after optimization
			if (attrib.normals.empty()) GenerateNormals(mesh.unifiedVertices, mesh.unifiedIndices);
			if (!attrib.texcoords.empty()) GenerateMikkTangents(mesh.unifiedVertices, mesh.unifiedIndices);
		}
	}

	return loaded;
}

Result<LoadedModel> MeshLoader::LoadModelFromSource(MeshSourceType type, const void* data)
{
	switch (type)
	{
	case MeshSourceType::OBJ:
		return LoadOBJ(static_cast<const char*>(data));
	case MeshSourceType::OrgPack:
		return std::unexpected(NotImplemented);
	case MeshSourceType::FBX:
		return LoadFBX(static_cast<const char*>(data));
	case MeshSourceType::Runtime:
	default:
		return std::unexpected(NotImplemented);
	}
}
