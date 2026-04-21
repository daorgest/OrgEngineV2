//
// Created by Orgest on 7/6/2025.
//

#include "MeshLoader.h"

#include "MeshData.h"
#include "Math/MikkWrapper.h"
#include "Tools/Logger.h"

#include <meshoptimizer.h>
#include <glm/glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <ufbx.h>

using namespace Assets;

Result<LoadedModel> MeshLoader::LoadFBX(const std::filesystem::path& path)
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
	ufbx_scene* scene = ufbx_load_file(path.string().c_str(), &opts, &error);

	if (!scene) return std::unexpected(AssetLoadFailed);

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
        out.roughness = static_cast<f32>(umat->pbr.roughness.value_real);
        out.metallic = static_cast<f32>(umat->pbr.metalness.value_real);

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
            mp.materialIndex = part.index;
            mp.firstIndex = static_cast<u32>(mesh.unifiedIndices.size());

            for (u32 face_i : part.face_indices)
            {
                ufbx_face face = umesh->faces[face_i];
                for (u32 i = 0; i < face.num_indices - 2; i++)
                {
                    u32 corners[3] = { face.index_begin, face.index_begin + i + 1, face.index_begin + i + 2 };
                    for (u32 corner : corners)
                    {
                        Vertex v{};
                        ufbx_vec3 p = ufbx_get_vertex_vec3(&umesh->vertex_position, corner);
                        v.position = { p.x, p.y, p.z };

                        if (hasNormals) {
                            ufbx_vec3 n = ufbx_get_vertex_vec3(&umesh->vertex_normal, corner);
                            v.normal = { n.x, n.y, n.z };
                        }
                        if (hasUVs) {
                            ufbx_vec2 uv = ufbx_get_vertex_vec2(&umesh->vertex_uv, corner);
                            v.uv = { uv.x, 1.0f - uv.y };
                        }

                        mesh.unifiedIndices.push_back(static_cast<u32>(mesh.unifiedVertices.size()));
                        mesh.unifiedVertices.push_back(v);
                    }
                }
            }
            mp.indexCount = static_cast<u32>(mesh.unifiedIndices.size()) - mp.firstIndex;
            mesh.parts.push_back(mp);
        }

        PostProcessMesh(mesh, hasNormals, hasUVs);
        model.meshes.push_back(std::move(mesh));
    }

    ufbx_free_scene(scene);
    return model;
}

static void BuildMeshPartsFromShape(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, Mesh& outMesh)
{
    // Let meshoptimizer handle de-duplication.
    size_t indexOffset = 0;
    const bool hasNormals = !attrib.normals.empty();
    const bool hasUVs = !attrib.texcoords.empty();

    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
    {
        const u32 fv = shape.mesh.num_face_vertices[f];
        i32 matId = shape.mesh.material_ids[f];

        // Start a new part if material changes, or it's the first part
        if (outMesh.parts.empty() || outMesh.parts.back().materialIndex != static_cast<u32>(matId))
        {
            outMesh.parts.emplace_back(MeshPart{
                .materialIndex = (matId >= 0) ? static_cast<u32>(matId) : 0u,
                .firstIndex = static_cast<u32>(outMesh.unifiedIndices.size()),
                .localTransform = glm::mat4(1.0f)
            });
        }

        for (size_t v = 0; v < fv; ++v)
        {
            tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
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

            outMesh.unifiedIndices.push_back(static_cast<u32>(outMesh.unifiedVertices.size()));
            outMesh.unifiedVertices.push_back(vert);
        }

        outMesh.parts.back().indexCount += fv;
        indexOffset += fv;
    }
}

Result<LoadedModel> MeshLoader::LoadOBJ(const std::filesystem::path& path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	std::filesystem::path baseDir = path.parent_path();
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(), baseDir.string().c_str(), true))
	{
		LOG(Error, "[tinyobj] Failed to load '{}': {}", path.string(), err);
		return std::unexpected(AssetLoadFailed);
	}

	LoadedModel loaded{ .sourceType = MeshSourceType::OBJ };

	// Material Mapping
	for (const auto& mat : materials)
	{

	    std::string normalPath = "";
	    if (!mat.normal_texname.empty()) {
	        normalPath = (baseDir / mat.normal_texname).string();
	    }
	    else if (!mat.bump_texname.empty()) {
	        normalPath = (baseDir / mat.bump_texname).string();
	    }
	    else if (!mat.displacement_texname.empty()) {
	        normalPath = (baseDir / mat.displacement_texname).string();
	    }

	    MaterialType type = MaterialType::Opaque;
	    if (mat.dissolve < 1.0f)
	    {
	        type = MaterialType::Transparent;
	    }
	    else if (!mat.alpha_texname.empty())
	    {
	        type = MaterialType::AlphaMask;
	    }

		Material out = {
			.name       = mat.name,
			.albedoPath = !mat.diffuse_texname.empty() ? (baseDir / mat.diffuse_texname).string() : "",
			.normalPath = normalPath,
	        .materialType  = type,

			// PBR Properties and Fallbacks
			.baseColor = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]},
			.roughness = mat.roughness,
			.metallic  = mat.metallic,
			.ior       = mat.ior,      // Index of Refraction (Ni)
			.opacity   = mat.dissolve, // Dissolve (d)
			.emissive  = glm::vec3(mat.emission[0], mat.emission[1], mat.emission[2])
		};

		loaded.materials.push_back(std::move(out));
	}


	// Mesh Extraction
	for (const auto& shape : shapes)
	{
		Mesh mesh{ .name = shape.name };
		BuildMeshPartsFromShape(attrib, shape, mesh);

		PostProcessMesh(mesh, !attrib.normals.empty(), !attrib.texcoords.empty());
		loaded.meshes.push_back(std::move(mesh));
	}

	return loaded;
}

Result<LoadedModel> MeshLoader::LoadModelFromSource(const MeshSourceType type, const std::filesystem::path& path)
{
	switch (type)
	{
		case MeshSourceType::OBJ: return LoadOBJ(path);
		case MeshSourceType::FBX: return LoadFBX(path);
		default: return std::unexpected(NotImplemented);
	}
}

void MeshLoader::PostProcessMesh(Mesh& mesh, bool hasNormals, bool hasUVs)
{
	if (mesh.unifiedIndices.empty()) return;

	// Meshoptimizer: Reindex and Optimize
	const size_t indexCount = mesh.unifiedIndices.size();
	const size_t vertexCount = mesh.unifiedVertices.size();

	Vector<u32> remap(indexCount);
	const size_t uniqueVerts = meshopt_generateVertexRemap(remap.data(), mesh.unifiedIndices.data(), indexCount, mesh.unifiedVertices.data(), vertexCount, sizeof(Vertex));

	Vector<u32> optIndices(indexCount);
	Vector<Vertex> optVertices(uniqueVerts);

	meshopt_remapIndexBuffer(optIndices.data(), mesh.unifiedIndices.data(), indexCount, remap.data());
	meshopt_remapVertexBuffer(optVertices.data(), mesh.unifiedVertices.data(), vertexCount, sizeof(Vertex), remap.data());

	for (auto& part : mesh.parts)
	{
		u32* pIndices = &optIndices[part.firstIndex];
		meshopt_optimizeVertexCache(pIndices, pIndices, part.indexCount, uniqueVerts);
		meshopt_optimizeOverdraw(pIndices, pIndices, part.indexCount, &optVertices[0].position.x, uniqueVerts, sizeof(Vertex), 1.05f);
	}

	meshopt_optimizeVertexFetch(optVertices.data(), optIndices.data(), indexCount, optVertices.data(), uniqueVerts, sizeof(Vertex));

	mesh.unifiedVertices = std::move(optVertices);
	mesh.unifiedIndices = std::move(optIndices);


	if (!hasNormals) GenerateNormals(mesh.unifiedVertices, mesh.unifiedIndices);
	if (hasUVs) GenerateMikkTangents(mesh.unifiedVertices, mesh.unifiedIndices);


    // Setting up aabb's!
	for (auto& part : mesh.parts)
	{
		part.aabb = AABB(
			std::span<const u32>(&mesh.unifiedIndices[part.firstIndex], part.indexCount),
			std::span<const Vertex>(mesh.unifiedVertices)
		);
	}
}

void MeshLoader::GenerateNormals(Vector<Vertex>& verts, const Vector<u32>& indices)
{
	for (auto& v : verts) v.normal = {0, 0, 0};
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const glm::vec3 fn = glm::cross(verts[indices[i + 1]].position - verts[indices[i]].position, verts[indices[i + 2]].position - verts[indices[i]].position);
		verts[indices[i]].normal += fn;
		verts[indices[i + 1]].normal += fn;
		verts[indices[i + 2]].normal += fn;
	}
	for (auto& v : verts) v.normal = glm::normalize(v.normal);
}
