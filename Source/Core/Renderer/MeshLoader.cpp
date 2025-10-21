//
// Created by Orgest on 7/6/2025.
//

#include "MeshLoader.h"

#include "MeshData.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <expected>
#include <meshoptimizer.h>
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <glm/glm.hpp>
#include <utility>

#include "MikkWrapper.h"
#include "Tools/Logger.h"
#define UFBX_REAL_IS_FLOAT
#include <ufbx.h>
using namespace Assets;

// Ufbx bs
static std::string GetBaseDir(const std::string& filepath)
{
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

Result<LoadedModel> MeshLoader::LoadFBX(const char* path)
{
//     ufbx_load_opts opts = {};
//     opts.target_axes        = ufbx_axes_right_handed_y_up;
//     opts.target_unit_meters = 1.0;
//
//     ufbx_error error{};
//     const ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
//     if (!scene) {
//         LOG(Error, "[ufbx] Failed to load: {}", error.description.data);
//         return std::unexpected(AssetNotFound);
//     }
//
//     auto toVec3 = [](const ufbx_vec3& v) -> glm::vec3 { return { (float)v.x, (float)v.y, (float)v.z }; };
//     auto toVec2 = [](const ufbx_vec2& v) -> glm::vec2 { return { (float)v.x, (float)v.y }; };
//
//     LoadedModel model{};
// #if defined(MeshSourceType_FBX) || 1
//     // If you have MeshSourceType::FBX, set it:
//     model.sourceType = MeshSourceType::FBX;
// #endif
//
//     // Walk all nodes with meshes
//     for (const ufbx_node* node : scene->nodes)
//     {
//         if (!node || !node->mesh || node->is_root) continue;
//
//         const ufbx_mesh*  umesh   = node->mesh;
//         const ufbx_matrix toWorld = node->geometry_to_world;
//
//         Mesh outMesh{};
//         outMesh.name = umesh->name.data ? umesh->name.data : "Mesh";
//
//         const bool hasNormals = umesh->vertex_normal.indices.count > 0;
//         const bool hasUVs     = umesh->vertex_uv.indices.count     > 0;
//
//         // One MeshPart per material/group
//         for (const ufbx_mesh_part& part : umesh->parts)
//         {
//             // --- Precompute triangle count (don’t trust num_triangles unless already triangulated)
//             size_t totalTris = 0;
//             for (uint32_t faceIndex : part.face_indices) {
//                 const ufbx_face f = umesh->faces[faceIndex];
//                 const size_t n = f.num_indices;           // polygon size
//                 if (n >= 3) totalTris += (n - 2);         // fan triangulation
//             }
//
//             MeshPart outPart{};
//             const size_t cornerCount = totalTris * 3;
//             outPart.vertices.reserve(cornerCount);
//             outPart.indices.resize(cornerCount); // filled by ufbx_generate_indices
//
//             // Scratch for triangulating a single face
//             Vector<uint32_t> triCornerIdx;
//             triCornerIdx.resize((size_t)umesh->max_face_triangles * 3);
//
//             // --- Build a flat per-corner vertex list
//             for (uint32_t faceIndex : part.face_indices)
//             {
//                 const ufbx_face face = umesh->faces[faceIndex];
//
//                 const uint32_t triCount = ufbx_triangulate_face(
//                     triCornerIdx.data(), triCornerIdx.size(), umesh, face);
//
//                 for (uint32_t i = 0; i < triCount * 3; ++i)
//                 {
//                     const uint32_t corner = triCornerIdx[i];
//
//                     Vertex v{}; // zero-init for deterministic padding
//
//                     // Position → world
//                     ufbx_vec3 p = umesh->vertex_position[corner];
//                     p = ufbx_transform_position(&toWorld, p);
//                     v.position = toVec3(p);
//
//                     // Normal → world (if present)
//                     if (hasNormals) {
//                         ufbx_vec3 n = umesh->vertex_normal[corner];
//                         n = ufbx_transform_direction(&toWorld, n);
//                         v.normal = glm::normalize(toVec3(n));
//                     } else {
//                         v.normal = { 0.f, 1.f, 0.f };
//                     }
//
//                     // UV0 (if present). Flip V here if your pipeline expects it.
//                     if (hasUVs) {
//                         ufbx_vec2 uv0 = umesh->vertex_uv[corner];
//                         // v.uv = { (float)uv0.x, 1.0f - (float)uv0.y }; // uncomment if needed
//                         v.uv = toVec2(uv0);
//                     } else {
//                         v.uv = { 0.f, 0.f };
//                     }
//
//                     outPart.vertices.push_back(v);
//                 }
//             }
//
//             // If this part ended up empty, skip safely
//             if (outPart.vertices.empty()) {
//                 continue;
//             }
//
//             // --- Deduplicate & build index buffer (padding-safe multi-stream)
//             ufbx_vertex_stream streams[3] = {
//                 // position
//                 { (const char*)&outPart.vertices[0].position, outPart.vertices.size(), sizeof(Vertex) },
//                 // normal
//                 { (const char*)&outPart.vertices[0].normal,   outPart.vertices.size(), sizeof(Vertex) },
//                 // uv
//                 { (const char*)&outPart.vertices[0].uv,       outPart.vertices.size(), sizeof(Vertex) },
//             };
//
//             const size_t uniqueCount = ufbx_generate_indices(
//                 streams, 3,
//                 outPart.indices.data(), outPart.indices.size(),
//                 nullptr, nullptr);
//
//             outPart.vertices.resize(uniqueCount);
//
//             // --- AABB (zero-copy)
//             std::span<const Vertex> vspan{ outPart.vertices.data(), outPart.vertices.size() };
//             outPart.aabb = AABB(vspan);
//
//             outMesh.parts.push_back(std::move(outPart));
//         }
//
//         model.meshes.push_back(std::move(outMesh));
//     }
//
//     ufbx_free_scene(scene);
	LoadedModel model;
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

static void BuildMeshPartsFromShape(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, const std::vector<tinyobj::material_t>& materials, Vector<MeshPart>& outParts)
{
    struct Tri { tinyobj::index_t v[3]; int matId; };

    std::vector<Tri> allTris;
    allTris.reserve(shape.mesh.indices.size() / 3);

    size_t idxOffset = 0;
    const auto& fv   = shape.mesh.num_face_vertices;
    const auto& mids = shape.mesh.material_ids;

    for (size_t f = 0; f < fv.size(); ++f)
    {
        const int fverts = fv[f];
        const int matId  = (f < mids.size()) ? mids[f] : -1;
        if (fverts == 3)
        {
            Tri t{};
            t.v[0] = shape.mesh.indices[idxOffset + 0];
            t.v[1] = shape.mesh.indices[idxOffset + 1];
            t.v[2] = shape.mesh.indices[idxOffset + 2];
            t.matId = matId;
            allTris.push_back(t);
        }
        idxOffset += static_cast<size_t>(fverts);
    }

    const bool hasObjNormals  = !attrib.normals.empty();
    const bool hasObjTexcoord = !attrib.texcoords.empty();

    Vector<Vertex> rawVertices;
    Vector<u32> rawIndices;
    rawVertices.reserve(allTris.size() * 3);
    rawIndices.reserve(allTris.size() * 3);

    auto emitVert = [&](const tinyobj::index_t& id) -> void
	{
        Vertex v{};
        if (id.vertex_index >= 0)
        {
            v.position = {
                attrib.vertices[3 * id.vertex_index + 0],
                attrib.vertices[3 * id.vertex_index + 1],
                attrib.vertices[3 * id.vertex_index + 2]
            };
        }
        if (hasObjNormals && id.normal_index >= 0)
        {
            v.normal = {
                attrib.normals[3 * id.normal_index + 0],
                attrib.normals[3 * id.normal_index + 1],
                attrib.normals[3 * id.normal_index + 2]
            };
        }
        if (hasObjTexcoord && id.texcoord_index >= 0)
        {
            v.uv = {
                attrib.texcoords[2 * id.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * id.texcoord_index + 1]
            };
        }
        v.color   = {1.0f, 1.0f, 1.0f};
        v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        rawIndices.push_back(static_cast<u32>(rawVertices.size()));
        rawVertices.push_back(v);
    };

    for (const auto& [v, matId] : allTris)
    {
        emitVert(v[0]); emitVert(v[1]); emitVert(v[2]);
    }

    Vector<u32> remap(rawIndices.size());
    size_t uniqueCount = meshopt_generateVertexRemap(remap.data(), rawIndices.data(), rawIndices.size(), rawVertices.data(), rawVertices.size(), sizeof(Vertex));

    Vector<Vertex> unifiedVerts(uniqueCount);
    Vector<u32> unifiedIdx(rawIndices.size());

    meshopt_remapVertexBuffer(unifiedVerts.data(), rawVertices.data(), rawVertices.size(), sizeof(Vertex), remap.data());
    meshopt_remapIndexBuffer(unifiedIdx.data(), rawIndices.data(), rawIndices.size(), remap.data());

    meshopt_optimizeVertexCache(unifiedIdx.data(), unifiedIdx.data(), unifiedIdx.size(), unifiedVerts.size());

    std::vector<Vertex> fetched(unifiedVerts.size());
    meshopt_optimizeVertexFetch(fetched.data(), unifiedIdx.data(), unifiedIdx.size(), unifiedVerts.data(), unifiedVerts.size(), sizeof(Vertex));
    unifiedVerts.assign(fetched.begin(), fetched.end());

    bool needNormals = !hasObjNormals;
    if (!needNormals)
    {
        for (const auto& v : unifiedVerts)
        {
            float sum = fabsf(v.normal.x) + fabsf(v.normal.y) + fabsf(v.normal.z);
            if (sum < 1e-8f) { needNormals = true; break; }
        }
    }
    if (needNormals && !unifiedIdx.empty()) GenerateNormals(unifiedVerts, unifiedIdx);

    bool haveAnyUV = false;
    for (const auto& v : unifiedVerts)
    {
        if (v.uv.x != 0.f || v.uv.y != 0.f) { haveAnyUV = true; break; }
    }
    if (haveAnyUV && !unifiedVerts.empty() && !unifiedIdx.empty()) GenerateMikkTangents(unifiedVerts.data(), (u32)unifiedVerts.size(), unifiedIdx.data(), (u32)unifiedIdx.size());

    std::unordered_map<int, Vector<u32>> indicesByMat;
    indicesByMat.reserve(8);

    for (size_t tri = 0; tri < allTris.size(); ++tri)
    {
        int matId = allTris[tri].matId;
        indicesByMat[matId].push_back(unifiedIdx[3 * tri + 0]);
        indicesByMat[matId].push_back(unifiedIdx[3 * tri + 1]);
        indicesByMat[matId].push_back(unifiedIdx[3 * tri + 2]);
    }

    outParts.clear();
    outParts.reserve(indicesByMat.size());

    for (auto& [matId, idxs] : indicesByMat)
    {
        std::unordered_map<u32, u32> localRemap;
        localRemap.reserve(idxs.size());

        Vector<Vertex> partVerts;
        Vector<u32> partIdxs;
        partIdxs.reserve(idxs.size());

        for (u32 gi : idxs)
        {
            auto it = localRemap.find(gi);
            if (it == localRemap.end())
            {
                u32 li = (u32)partVerts.size();
                localRemap.emplace(gi, li);
                partVerts.push_back(unifiedVerts[gi]);
                partIdxs.push_back(li);
            }
            else
            {
                partIdxs.push_back(it->second);
            }
        }

        meshopt_optimizeVertexCache(partIdxs.data(), partIdxs.data(), partIdxs.size(), partVerts.size());
        Vector<Vertex> partFetched(partVerts.size());
        meshopt_optimizeVertexFetch(partFetched.data(), partIdxs.data(), partIdxs.size(), partVerts.data(), partVerts.size(), sizeof(Vertex));
        partVerts.assign(partFetched.begin(), partFetched.end());

        MeshPart part{};
        part.materialIndex = (matId >= 0 && std::cmp_less(matId ,materials.size())) ? static_cast<u32>(matId) : 0u;
        part.vertices = std::move(partVerts);
        part.indices = std::move(partIdxs);
        outParts.push_back(std::move(part));
    }
}



Result<LoadedModel> MeshLoader::LoadOBJ(const char* filePath)
{
	using namespace tinyobj;

	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string warn;
	std::string err;


	std::string baseDir = GetBaseDir(filePath);
	if (baseDir.empty())
	{
		baseDir = ".";
	}
#ifdef _WIN32
	baseDir += "\\";
#else
	baseDir += "/";
#endif

	if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath, baseDir.c_str(), true))
	{
		return std::unexpected(AssetLoadFailed);
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
			info.albedoPath = JoinPath(baseDir, mat.diffuse_texname);

		if (!mat.normal_texname.empty())
			info.normalPath = JoinPath(baseDir, mat.normal_texname);
		else if (!mat.bump_texname.empty())
			info.normalPath = JoinPath(baseDir, mat.bump_texname);
		else if (!mat.displacement_texname.empty())
			info.normalPath = JoinPath(baseDir, mat.displacement_texname);

		if (!mat.specular_texname.empty())
			info.specularPath = JoinPath(baseDir, mat.specular_texname);

		if (!mat.emissive_texname.empty())
			info.emissivePath = JoinPath(baseDir, mat.emissive_texname);

		loaded.materials.push_back(std::move(info));
	}
	if (loaded.materials.empty()) {
		Material def{}; def.name = "Default";
		loaded.materials.push_back(std::move(def));
	}

	for (const auto& shape : shapes)
	{
		Mesh mesh;
		mesh.name = shape.name;

		Vector<MeshPart> parts;
		BuildMeshPartsFromShape(attrib, shape, materials, parts);

		for (auto& part : parts)
		{
			// Compute AABB from this part's vertices
			part.aabb = AABB(std::span<const Vertex>(part.vertices.data(), part.vertices.size()));

			mesh.parts.push_back(std::move(part));
		}

		loaded.meshes.push_back(std::move(mesh));
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
		return std::unexpected(NotImplemented);

	default:
		return std::unexpected(NotImplemented);
	}
}
