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

#define UFBX_CPP11 1
#define UFBX_REAL_IS_FLOAT
#include <ufbx.h>

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

// Unified mesh optimization using meshoptimizer
static void OptimizeMesh(Vector<Vertex>& vertices, Vector<u32>& indices)
{
	if (vertices.empty() || indices.empty())
	{
		return;
	}

	// Generate vertex remap to deduplicate vertices
	Vector<u32> remap(indices.size());
	const size_t uniqueCount = meshopt_generateVertexRemap(
		remap.data(),
		indices.data(),
		indices.size(),
		vertices.data(),
		vertices.size(),
		sizeof(Vertex)
	);

	// Create optimized buffers
	Vector<Vertex> optimizedVertices(uniqueCount);
	Vector<u32> optimizedIndices(indices.size());

	// Remap vertices and indices
	meshopt_remapVertexBuffer(
		optimizedVertices.data(),
		vertices.data(),
		vertices.size(),
		sizeof(Vertex),
		remap.data()
	);

	meshopt_remapIndexBuffer(
		optimizedIndices.data(),
		indices.data(),
		indices.size(),
		remap.data()
	);

	// Optimize for vertex cache (post-transform cache optimization)
	meshopt_optimizeVertexCache(
		optimizedIndices.data(),
		optimizedIndices.data(),
		optimizedIndices.size(),
		optimizedVertices.size()
	);

	// Optimize vertex fetch (reorder vertices for better cache locality)
	Vector<Vertex> fetchOptimized(optimizedVertices.size());
	meshopt_optimizeVertexFetch(
		fetchOptimized.data(),
		optimizedIndices.data(),
		optimizedIndices.size(),
		optimizedVertices.data(),
		optimizedVertices.size(),
		sizeof(Vertex)
	);

	// Update original buffers with optimized data
	vertices = std::move(fetchOptimized);
	indices = std::move(optimizedIndices);
}

using namespace Assets;

Result<LoadedModel> MeshLoader::LoadFBX(const char* path)
{
	ufbx_scene *scene = ufbx_load_file(path, nullptr, nullptr);

	for (size_t i = 0; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		fmt::println("{}", node->name.data);
	}


 //    // ---------------------------------------------------------
 //    // 1. Load scene
 //    // ---------------------------------------------------------
 //    ufbx_load_opts opts{};
 //    opts.target_axes                   = ufbx_axes_right_handed_y_up;
 //    opts.target_unit_meters            = 1.0;
 //    opts.load_external_files           = true;
 //    opts.ignore_missing_external_files = true;
 //
 //    ufbx_error        error{};
 //    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
 //    if (!scene)
 //    {
	//     LOG(Error, "[ufbx] Failed to load '{}' : {}", path, error.description.data);
	//     return std::unexpected(AssetNotFound);
 //    }
 //
 //    auto toVec2   = [](const ufbx_vec2& v) -> glm::vec2 { return {static_cast<float>(v.x), static_cast<float>(v.y)}; };
 //    auto toVec3   = [](const ufbx_vec3& v) -> glm::vec3 { return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)}; };
 //    auto toString = [](ufbx_string s) -> std::string
 //    {
	//     if (!s.data || s.length == 0) return {};
	//     return std::string(s.data, s.length);
 //    };
 //
 //    auto getTexturePath = [](const ufbx_material_map& map) -> std::string
 //    {
 //        if (!map.texture || !map.texture_enabled) return {};
 //
 //        const ufbx_texture* tex = map.texture;
 //
 //        // Prefer relative path, then absolute, then plain filename
 //        if (tex->relative_filename.length > 0)
	//         return {tex->relative_filename.data, tex->relative_filename.length};
 //
 //        if (tex->filename.length > 0)
 //            return std::string(tex->filename.data, tex->filename.length);
 //
 //        if (tex->absolute_filename.length > 0)
 //            return std::string(tex->absolute_filename.data, tex->absolute_filename.length);
 //
 //        return {};
 //    };
 //
 //    LoadedModel model{};
 //    model.sourceType = MeshSourceType::FBX;
 //
 //    // ---------------------------------------------------------
 //    // 2. Materials (standard PBR-ish)
 //    // ---------------------------------------------------------
 //    model.materials.reserve(scene->materials.count);
 //
	// for (const ufbx_material *umat : scene->materials)
	// {
	// 	Material out{};
	// 	out.name = toString(umat->name);
 //
	// 	// --- Base color ---
	// 	if (umat->pbr.base_color.has_value)
	// 	{
	// 		out.baseColor = toVec3(umat->pbr.base_color.value_vec3);
	// 	}
	// 	else if (umat->fbx.diffuse_color.has_value)
	// 	{
	// 		out.baseColor = toVec3(umat->fbx.diffuse_color.value_vec3);
	// 	}
	// 	else
	// 	{
	// 		out.baseColor = glm::vec3(1.0f);
	// 	}
 //
	// 	// --- Roughness ---
	// 	if (umat->pbr.roughness.has_value)
	// 		out.roughness = static_cast<float>(umat->pbr.roughness.value_real);
 //
	// 	// --- Metallic ---
	// 	if (umat->pbr.metalness.has_value)
	// 		out.metallic = static_cast<float>(umat->pbr.metalness.value_real);
	// 	else
	// 		out.metallic = 0.0f;
 //
	// 	// --- IOR ---
	// 	if (umat->pbr.specular_ior.has_value)
	// 		out.ior = static_cast<float>(umat->pbr.specular_ior.value_real);
 //
	// 	// --- Opacity ---
	// 	if (umat->pbr.opacity.has_value)
	// 		out.opacity = static_cast<float>(umat->pbr.opacity.value_real);
 //
	// 	// --- Emissive ---
	// 	if (umat->pbr.emission_color.has_value)
	// 		out.emissive = toVec3(umat->pbr.emission_color.value_vec3);
	// 	else if (umat->fbx.emission_color.has_value)
	// 		out.emissive = toVec3(umat->fbx.emission_color.value_vec3);
 //
	// 	// --- Texture paths ---
	// 	out.albedoPath     = getTexturePath(umat->pbr.base_color);
	// 	out.normalPath     = getTexturePath(umat->pbr.normal_map);
	// 	out.roughnessPath  = getTexturePath(umat->pbr.roughness);
	// 	out.metallicPath   = getTexturePath(umat->pbr.metalness);
	// 	out.aoPath         = getTexturePath(umat->pbr.ambient_occlusion);
	// 	out.emissivePath   = getTexturePath(umat->pbr.emission_color);
	// 	out.specularPath   = getTexturePath(umat->pbr.specular_color);
 //
	// 	model.materials.push_back(std::move(out));
	// }
 //
 //
 //    // Helper: map a node/mesh material pointer to global model.materials index
 //    auto findMaterialIndex = [&](const ufbx_material* matPtr) -> u32
 //    {
	//     if (!matPtr) return 0;
	//     for (size_t i = 0; i < scene->materials.count; ++i) { if (scene->materials.data[i] == matPtr) { return static_cast<u32>(i); } }
	//     return 0;
 //    };
 //
 //    // ---------------------------------------------------------
 //    // 3. Meshes: unify vertices/indices per Mesh
 //    // ---------------------------------------------------------
 //    for (const ufbx_node *node : scene->nodes)
 //    {
	//     if (!node || !node->mesh || node->is_root)
	// 	    continue;
 //
	//     const ufbx_mesh*  umesh   = node->mesh;
	//     const ufbx_matrix toWorld = node->geometry_to_world;
 //
	//     Mesh mesh{};
	//     mesh.name = toString(umesh->name);
	//     if (mesh.name.empty())
	// 	    mesh.name = "Mesh";
 //
	//     Vector<Vertex>& outVerts = mesh.unifiedVertices;
	//     Vector<u32>&    outIdx   = mesh.unifiedIndices;
 //
 //        u32 baseVertex  = 0;
 //        u32 indexOffset = 0;
 //
 //        const bool hasNormals = umesh->vertex_normal.exists;
 //        const bool hasUVs     = umesh->vertex_uv.exists;
 //
	//     // Use material_parts so each part corresponds to one material slot
	//     for (size_t pi = 0; pi < umesh->material_parts.count; ++pi)
	//     {
	// 	    const ufbx_mesh_part& part = umesh->material_parts.data[pi];
 //
	// 	    // -------------------------------------------------
	// 	    // 3.1 Count triangles in this part
	// 	    // -------------------------------------------------
	// 	    size_t triCount = 0;
	// 	    for (size_t fi_i = 0; fi_i < part.face_indices.count; ++fi_i)
	// 	    {
	// 		    uint32_t        faceIndex = part.face_indices.data[fi_i];
	// 		    const ufbx_face f         = umesh->faces.data[faceIndex];
	// 		    if (f.num_indices >= 3)
	// 			    triCount += (f.num_indices - 2);
	// 	    }
 //
	// 	    if (triCount == 0)
	// 		    continue;
 //
	// 	    const size_t triCornerCount = triCount * 3;
 //
	// 	    Vector<Vertex> tempCorners;
	// 	    tempCorners.reserve(triCornerCount);
 //
	// 	    Vector<uint32_t> triCornerIdx;
	// 	    triCornerIdx.resize((size_t)umesh->max_face_triangles * 3);
 //
	// 	    // Choose material for this part from node->materials or mesh->materials
	// 	    const ufbx_material* partMat = nullptr;
	// 	    if (node->materials.count > 0 && part.index < node->materials.count)
	// 		    partMat = node->materials.data[part.index];
	// 	    else if (umesh->materials.count > 0 && part.index < umesh->materials.count)
	// 		    partMat = umesh->materials.data[part.index];
 //
	// 	    MeshPart mp{};
	// 	    mp.materialIndex = findMaterialIndex(partMat);
 //
	// 	    // -------------------------------------------------
	// 	    // 3.2 Build flat per-corner vertex list (triangulated)
	// 	    // -------------------------------------------------
	// 	    for (uint32_t faceIndex : part.face_indices)
	// 	    {
	// 		    const ufbx_face face = umesh->faces[faceIndex];
 //
	// 		    // Triangulate into corner indices
	// 		    uint32_t nTris = ufbx_triangulate_face(
	// 			    triCornerIdx.data(), triCornerIdx.size(),
	// 			    umesh, face
	// 		    );
 //
	// 		    // Iterate corners
	// 		    for (uint32_t corner : std::span(triCornerIdx.data(), nTris * 3))
	// 		    {
	// 			    Vertex v{};
 //
	// 			    // ----- Position -----
	// 			    ufbx_vec3 p = ufbx_get_vertex_vec3(&umesh->vertex_position, corner);
	// 			    p           = ufbx_transform_position(&toWorld, p);
	// 			    v.position  = toVec3(p);
 //
	// 			    // ----- Normal -----
	// 			    if (hasNormals)
	// 			    {
	// 				    ufbx_vec3 n = ufbx_get_vertex_vec3(&umesh->vertex_normal, corner);
	// 				    n           = ufbx_transform_direction(&toWorld, n);
	// 				    v.normal    = glm::normalize(toVec3(n));
	// 			    }
	// 			    else { v.normal = {0.f, 1.f, 0.f}; }
 //
	// 			    // ----- UV -----
	// 			    if (hasUVs)
	// 			    {
	// 				    ufbx_vec2 uv = ufbx_get_vertex_vec2(&umesh->vertex_uv, corner);
	// 				    v.uv         = toVec2(uv);
	// 				    // Optional: flip Y
	// 				    // v.uv.y = 1.f - v.uv.y;
	// 			    }
	// 			    else { v.uv = {0.f, 0.f}; }
 //
	// 			    tempCorners.push_back(v);
	// 		    }
	// 	    }
 //
 //
 //            if (tempCorners.empty())
 //                continue;
 //
 //            // -------------------------------------------------
 //            // 3.3 Deduplicate & build index buffer using ufbx_generate_indices()
 //            // -------------------------------------------------
 //            const size_t cornerCount = tempCorners.size();
 //
 //            Vector<u32> partIndices;
 //            partIndices.resize(cornerCount);
 //
 //            ufbx_vertex_stream streams[3] = {
	//             {static_cast<void*>(&tempCorners[0].position), cornerCount, sizeof(Vertex)},
	//             {static_cast<void*>(&tempCorners[0].normal), cornerCount, sizeof(Vertex)},
	//             {static_cast<void*>(&tempCorners[0].uv), cornerCount, sizeof(Vertex)},
 //            };
 //
 //            ufbx_error genErr{};
 //            size_t uniqueCount = ufbx_generate_indices(
 //                streams, 3,
 //                partIndices.data(), partIndices.size(),
 //                nullptr, &genErr
 //            );
 //
 //            // Clamp uniqueCount just in case
 //            if (uniqueCount > cornerCount) uniqueCount = cornerCount;
 //            tempCorners.resize(uniqueCount);
 //
 //            // -------------------------------------------------
 //            // 3.4 Append to unified mesh buffers
 //            // -------------------------------------------------
 //            mp.vertexOffset = baseVertex;
 //            mp.firstIndex   = indexOffset;
 //            mp.indexCount   = (u32)partIndices.size(); // number of *indices* for this part
 //
 //            // Copy unique vertices
 //            outVerts.reserve(outVerts.size() + uniqueCount);
 //            for (size_t i = 0; i < uniqueCount; ++i)
 //            {
 //                outVerts.push_back(tempCorners[i]);
 //            }
 //
 //            // Copy indices, offset by baseVertex
 //            outIdx.reserve(outIdx.size() + partIndices.size());
 //            for (unsigned int partIndice : partIndices)
 //            {
 //                outIdx.push_back(partIndice + baseVertex);
 //            }
 //
 //            // Advance offsets
 //            baseVertex  += (u32)uniqueCount;
 //            indexOffset += mp.indexCount;
 //
 //            // Compute AABB over this part's unique vertices
 //            std::span<const Vertex> vspan{
 //                &mesh.unifiedVertices[mp.vertexOffset],
 //                uniqueCount
 //            };
 //            mp.aabb = AABB(vspan);
 //
 //            mesh.parts.push_back(mp);
 //        }
 //
 //        model.meshes.push_back(std::move(mesh));
 //    }

    ufbx_free_scene(scene);
    return {};
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
    struct Tri { tinyobj::index_t v[3]; int matId; };

    std::vector<Tri> allTris;
    allTris.reserve(shape.mesh.indices.size() / 3);

    size_t idxOffset = 0;
    const auto& fv = shape.mesh.num_face_vertices;
    const auto& mids = shape.mesh.material_ids;

    // Load and filter triangles (skip non-triangulated faces)
    for (size_t f = 0; f < fv.size(); ++f)
    {
        const int fverts = fv[f];
        const int matId = (f < mids.size()) ? mids[f] : -1;
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

    if (allTris.empty()) return;

    // Sort by material ID for efficient batching
    std::ranges::sort(allTris, [](const Tri& a, const Tri& b) {
        return a.matId < b.matId;
    });

    const bool hasObjNormals = !attrib.normals.empty();
    const bool hasObjTexcoord = !attrib.texcoords.empty();

    Vector<Vertex> rawVertices;
    Vector<u32> rawIndices;
    rawVertices.reserve(allTris.size() * 3);
    rawIndices.reserve(allTris.size() * 3);

    // Emit vertices from sorted triangles
    auto emitVert = [&](const tinyobj::index_t& id)
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
        v.color = {1.0f, 1.0f, 1.0f};
        v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        rawIndices.push_back(static_cast<u32>(rawVertices.size()));
        rawVertices.push_back(v);
    };

    for (const auto& [v, matId] : allTris)
    {
        emitVert(v[0]); emitVert(v[1]); emitVert(v[2]);
    }

    // Move raw data to output mesh and optimize with unified function
    outMesh.unifiedVertices = std::move(rawVertices);
    outMesh.unifiedIndices = std::move(rawIndices);
    OptimizeMesh(outMesh.unifiedVertices, outMesh.unifiedIndices);

    // Generate normals if missing or invalid
    bool needNormals = !hasObjNormals;
    if (!needNormals)
    {
        for (const auto& v : outMesh.unifiedVertices)
        {
            if (fabsf(v.normal.x) + fabsf(v.normal.y) + fabsf(v.normal.z) < 1e-8f)
            {
                needNormals = true;
                break;
            }
        }
    }
    if (needNormals && !outMesh.unifiedIndices.empty())
        GenerateNormals(outMesh.unifiedVertices, outMesh.unifiedIndices);

    // Generate tangents if we have UVs
    bool haveUVs = false;
    for (const auto& v : outMesh.unifiedVertices)
    {
        if (v.uv.x != 0.f || v.uv.y != 0.f)
        {
            haveUVs = true;
            break;
        }
    }
    if (haveUVs && !outMesh.unifiedVertices.empty() && !outMesh.unifiedIndices.empty())
        GenerateMikkTangents(outMesh.unifiedVertices.data(), (u32)outMesh.unifiedVertices.size(),
                            outMesh.unifiedIndices.data(), (u32)outMesh.unifiedIndices.size());

    // Create mesh parts by material ranges
    outMesh.parts.clear();
    if (allTris.empty()) return;

    int currentMatId = allTris[0].matId;
    u32 firstIndex = 0;

    for (size_t tri = 0; tri < allTris.size(); ++tri)
    {
        if (allTris[tri].matId != currentMatId)
        {
            MeshPart part{};
            part.materialIndex = (currentMatId >= 0 && std::cmp_less(currentMatId, materials.size()))
                                 ? static_cast<u32>(currentMatId) : 0u;
            part.firstIndex = firstIndex;
            part.indexCount = static_cast<u32>(tri * 3) - firstIndex;
            part.vertexOffset = 0;
            part.aabb = AABB(
                std::span<const u32>(outMesh.unifiedIndices.data() + firstIndex, part.indexCount),
                std::span<const Vertex>(outMesh.unifiedVertices.data(), outMesh.unifiedVertices.size())
            );

            outMesh.parts.push_back(part);

            currentMatId = allTris[tri].matId;
            firstIndex = static_cast<u32>(tri * 3);
        }
    }

    // Add final part
    MeshPart lastPart{};
    lastPart.materialIndex = (currentMatId >= 0 && std::cmp_less(currentMatId, materials.size()))
                             ? static_cast<u32>(currentMatId) : 0u;
    lastPart.firstIndex = firstIndex;
    lastPart.indexCount = static_cast<u32>(allTris.size() * 3) - firstIndex;
    lastPart.vertexOffset = 0;
    lastPart.aabb = AABB(
        std::span<const u32>(outMesh.unifiedIndices.data() + firstIndex, lastPart.indexCount),
        std::span<const Vertex>(outMesh.unifiedVertices.data(), outMesh.unifiedVertices.size())
    );
    outMesh.parts.push_back(lastPart);
}



Result<LoadedModel> MeshLoader::LoadOBJ(const char* filePath)
{
	using namespace tinyobj;

	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string warn, err;

	std::string baseDir = GetBaseDir(filePath);
	if (baseDir.empty()) baseDir = ".";
	baseDir += (baseDir.back() == '/' || baseDir.back() == '\\') ? "" :
#ifdef _WIN32
	           "\\";
#else
	           "/";
#endif

	if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath, baseDir.c_str(), true))
		return std::unexpected(AssetLoadFailed);

	LoadedModel loaded;
	loaded.sourceType = MeshSourceType::OBJ;

	// Load materials from .mtl file with full PBR support
	for (const auto& mat : materials)
	{
		Material info;
		info.name = mat.name;

		// --- Texture Maps ---
		if (!mat.diffuse_texname.empty())
			info.albedoPath = ::JoinPath(baseDir, mat.diffuse_texname);

		// Try normal, bump, then displacement maps
		if (!mat.normal_texname.empty())
			info.normalPath = ::JoinPath(baseDir, mat.normal_texname);
		else if (!mat.bump_texname.empty())
			info.normalPath = ::JoinPath(baseDir, mat.bump_texname);
		else if (!mat.displacement_texname.empty())
			info.normalPath = ::JoinPath(baseDir, mat.displacement_texname);

		if (!mat.specular_texname.empty())
			info.specularPath = ::JoinPath(baseDir, mat.specular_texname);

		if (!mat.emissive_texname.empty())
			info.emissivePath = ::JoinPath(baseDir, mat.emissive_texname);

		// PBR texture maps (if available)
		if (!mat.roughness_texname.empty())
			info.roughnessPath = ::JoinPath(baseDir, mat.roughness_texname);

		if (!mat.metallic_texname.empty())
			info.metallicPath = ::JoinPath(baseDir, mat.metallic_texname);

		if (!mat.sheen_texname.empty())
		{
			// Use sheen as AO fallback
			info.aoPath = ::JoinPath(baseDir, mat.sheen_texname);
		}

		// --- PBR Material Properties (scalar values) ---

		// Base color from diffuse (Kd in MTL)
		info.baseColor = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);

		// Roughness (Pr in MTL, 0.0-1.0)
		info.roughness = mat.roughness;

		// Metallic (Pm in MTL, 0.0-1.0)
		info.metallic = mat.metallic;

		// Index of Refraction (Ni in MTL)
		info.ior = mat.ior;

		// Opacity/Dissolve (d in MTL, 1.0 = opaque, 0.0 = transparent)
		info.opacity = mat.dissolve;

		// Emissive color (Ke in MTL)
		info.emissive = glm::vec3(mat.emission[0], mat.emission[1], mat.emission[2]);

		// Debug: Log PBR properties
		LOG(Debug, "Material '{}': R={:.2f} M={:.2f} IOR={:.2f} Op={:.2f}",
		    info.name, info.roughness, info.metallic, info.ior, info.opacity);

		loaded.materials.push_back(std::move(info));
	}

	if (loaded.materials.empty())
		loaded.materials.push_back(Material{.name = "Default"});

	// Build meshes
	for (const auto& shape : shapes)
	{
		Mesh mesh;
		mesh.name = shape.name;
		BuildMeshPartsFromShape(attrib, shape, materials, mesh);

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
	default:
		return std::unexpected(NotImplemented);
	}
}
