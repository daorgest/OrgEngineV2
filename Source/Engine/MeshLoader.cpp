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


#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <glm/gtc/type_ptr.hpp>
#include "BindlessManager.h"
#include "TextureLoader.h"
#include "Tools/AssetPool.h"

using namespace Assets;

Result<LoadedModel> MeshLoader::LoadGLTF(const std::filesystem::path& path,
                                         AssetPool<Renderer::TextureData>* texturePool)
{
    if (!texturePool) return std::unexpected{PointerLoadFailed};

    if (!std::filesystem::exists(path))
    {
        return std::unexpected{AssetNotFound};
    }

    LOG(Info, "[FastGLTF] Loading: {}", path.string());

    constexpr auto supportedExtensions =
        fastgltf::Extensions::EXT_meshopt_compression |
        fastgltf::Extensions::KHR_materials_emissive_strength |
        fastgltf::Extensions::KHR_materials_unlit |
        fastgltf::Extensions::EXT_texture_webp |
        fastgltf::Extensions::MSFT_texture_dds |
        fastgltf::Extensions::KHR_materials_transmission |
        fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness |
        fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::MSFT_packing_normalRoughnessMetallic |
        fastgltf::Extensions::MSFT_packing_occlusionRoughnessMetallic;

    fastgltf::Parser parser(supportedExtensions);

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::GenerateMeshIndices |
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::DecomposeNodeMatrices;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);

    if (gltfFile.error() != fastgltf::Error::None)
    {
        return std::unexpected{AssetNotFound};
    }

    auto asset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    if (asset.error() != fastgltf::Error::None)
    {
        LOG(Warning, "[FastGLTF] Failed to parse: {}", fastgltf::getErrorName(asset.error()));
        return std::unexpected{AssetLoadFailed};
    }

    fastgltf::Asset& gltf = asset.get();
    LoadedModel model{.sourceType = Renderer::MeshSourceType::GLTF};


    ParseGLTFMaterials(gltf, path, texturePool, model.materials);
    ParseGLTFMeshes(gltf, model.meshes);
    // fastgltf::validate(asset.get());
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

Result<LoadedModel> MeshLoader::LoadModelFromSource(const Renderer::MeshSourceType type,
                                                    const std::filesystem::path& path,
                                                    AssetPool<Renderer::TextureData>* texturePool)
{
    switch (type)
    {
    case Renderer::MeshSourceType::OBJ: return LoadOBJ(path, texturePool);
    case Renderer::MeshSourceType::GLTF: return LoadGLTF(path, texturePool);
    default: return std::unexpected(NotImplemented);
    }
}


void MeshLoader::ParseGLTFMaterials(fastgltf::Asset& gltf, const std::filesystem::path& gltfPath,
                                    AssetPool<Renderer::TextureData>* texturePool,
                                    Vector<Material>& outMaterials)
{
    if (gltfPath.parent_path().empty())
        return;

    outMaterials.reserve(gltf.materials.size());


    auto loadTexture = [&](const auto& texInfoOpt, const char* nameSuffix, bool isSrgb,
                           std::string& outPath) -> ResourceHandle<Renderer::TextureData>
    {
        if (!texInfoOpt.has_value() || !texturePool) return {};

        auto& texture = gltf.textures[texInfoOpt.value().textureIndex];
        if (!texture.imageIndex.has_value()) return {};

        auto& image = gltf.images[texture.imageIndex.value()];

        if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data))
        {
            const std::filesystem::path fullPath = gltfPath.parent_path() / uri->uri.fspath();
            std::filesystem::path ddsPath = fullPath;
            ddsPath.replace_extension(".dds");

            // Direct disk check, no map caching needed
            outPath = std::filesystem::exists(ddsPath) ? ddsPath.string() : fullPath.string();

            auto res = texturePool->Load(outPath, [isSrgb](const std::string& p) -> Result<Renderer::TextureData>
            {
                auto ddsRes = TextureLoader::LoadTextureFromDDS(p.c_str());
                if (ddsRes) return ddsRes;

                return TextureLoader::LoadTextureFromSTB(p.c_str(), isSrgb);
            });

            return res ? *res : ResourceHandle<Renderer::TextureData>{};
        }

        Span<const u8> imageBytes;
        if (auto* array = std::get_if<fastgltf::sources::Array>(&image.data))
        {
            imageBytes = {reinterpret_cast<const u8*>(array->bytes.data()), array->bytes.size()};
        }
        else if (auto* bvSource = std::get_if<fastgltf::sources::BufferView>(&image.data))
        {
            auto& bufferView = gltf.bufferViews[bvSource->bufferViewIndex];
            auto& buffer = gltf.buffers[bufferView.bufferIndex];
            if (auto* arr = std::get_if<fastgltf::sources::Array>(&buffer.data))
            {
                imageBytes = {
                    reinterpret_cast<const u8*>(arr->bytes.data()) + bufferView.byteOffset, bufferView.byteLength
                };
            }
        }

        if (!imageBytes.empty())
        {
            outPath = fmt::format("{}_emb_{}_{}", gltfPath.string(), nameSuffix, texture.imageIndex.value());
            auto res = texturePool->Load(outPath, [imageBytes, isSrgb](const std::string&)
            {
                return TextureLoader::LoadTextureFromMemory(imageBytes, isSrgb);
            });
            return res ? *res : ResourceHandle<Renderer::TextureData>{};
        }

        return {};
    };

    for (auto& mat : gltf.materials)
    {
        Material outMat{};
        outMat.name = mat.name.empty() ? "GLTF_Material" : std::string(mat.name);

        // PBR Base Factors
        outMat.baseColor = {
            std::clamp(mat.pbrData.baseColorFactor[0], 0.0f, 1.0f),
            std::clamp(mat.pbrData.baseColorFactor[1], 0.0f, 1.0f),
            std::clamp(mat.pbrData.baseColorFactor[2], 0.0f, 1.0f)
        };

        outMat.opacity = std::clamp(static_cast<f32>(mat.pbrData.baseColorFactor[3]), 0.0f, 1.0f);
        outMat.roughness = mat.pbrData.roughnessFactor;

        f32 metalFallback = mat.pbrData.metallicFactor;
        if (metalFallback == 1.0f && !mat.pbrData.metallicRoughnessTexture.has_value())
        {
            metalFallback = 0.0f; // Force non-metal if no map and no explicit factor was saved
        }
        outMat.metallic = std::clamp(metalFallback, 0.0f, 1.0f);

        // Specular/Glossiness Interceptor
        if (mat.specularGlossiness)
        {
            outMat.baseColor = {
                std::clamp(mat.specularGlossiness->diffuseFactor[0], 0.0f, 1.0f),
                std::clamp(mat.specularGlossiness->diffuseFactor[1], 0.0f, 1.0f),
                std::clamp(mat.specularGlossiness->diffuseFactor[2], 0.0f, 1.0f)
            };

            outMat.roughness = 1.0f - std::clamp(static_cast<f32>(mat.specularGlossiness->glossinessFactor), 0.0f,
                                                 1.0f);

            f32 maxSpec = std::max({
                mat.specularGlossiness->specularFactor[0],
                mat.specularGlossiness->specularFactor[1],
                mat.specularGlossiness->specularFactor[2]
            });

            outMat.opacity = std::clamp(static_cast<f32>(mat.specularGlossiness->diffuseFactor[3]), 0.0f, 1.0f);
            outMat.metallic = (maxSpec > 0.5f) ? 1.0f : 0.0f;
        }

        outMat.emissive = {
            std::clamp(mat.emissiveFactor[0], 0.0f, 1.0f),
            std::clamp(mat.emissiveFactor[1], 0.0f, 1.0f),
            std::clamp(mat.emissiveFactor[2], 0.0f, 1.0f)
        };

        // --- Texture Resolution ---

        outMat.albedoHandle = loadTexture(mat.pbrData.baseColorTexture, "albedo", true, outMat.albedoPath);
        if (!outMat.albedoHandle && mat.specularGlossiness)
        {
            outMat.albedoHandle = loadTexture(mat.specularGlossiness->diffuseTexture, "albedo", true,
                                              outMat.albedoPath);
        }

        outMat.normalHandle = loadTexture(mat.normalTexture, "normal", false, outMat.normalPath);

        outMat.specularHandle = loadTexture(mat.pbrData.metallicRoughnessTexture, "metalRough", false,
                                            outMat.specularPath);

        if (!outMat.specularHandle && mat.specularGlossiness)
        {
            outMat.specularHandle = loadTexture(mat.specularGlossiness->specularGlossinessTexture, "specGloss", false,
                                                outMat.specularPath);
        }
        else if (!outMat.specularHandle && mat.packedOcclusionRoughnessMetallicTextures)
        {
            outMat.specularHandle = loadTexture(
                mat.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture, "metalRough", false,
                outMat.specularPath);
        }

        // Alpha Mode
        if (mat.alphaMode == fastgltf::AlphaMode::Mask) outMat.materialType = Engine::MaterialType::AlphaMask;
        else if (mat.alphaMode == fastgltf::AlphaMode::Blend) outMat.materialType = Engine::MaterialType::Transparent;
        else outMat.materialType = Engine::MaterialType::Opaque;

        if (mat.transmission)
        {
            // Force physical glass into the transparent pipeline so we can at least see through it
            outMat.materialType = Engine::MaterialType::Transparent;

            // A transmissionFactor of 1.0 means fully transparent. Invert it to lower our opacity.
            outMat.opacity *= (1.0f - mat.transmission->transmissionFactor);
        }
        outMaterials.push_back(std::move(outMat));
    }
}

void MeshLoader::ParseGLTFMeshes(fastgltf::Asset& gltf, Vector<Mesh>& outMeshes)
{
    outMeshes.reserve(gltf.meshes.size());

    Vector<Vector<glm::mat4>> meshTransforms(gltf.meshes.size());

    if (gltf.defaultScene.has_value())
    {
        fastgltf::iterateSceneNodes(gltf, gltf.defaultScene.value(), fastgltf::math::fmat4x4(),
                                    [&](fastgltf::Node& node, fastgltf::math::fmat4x4 matrix)
                                    {
                                        if (node.meshIndex.has_value())
                                        {
                                            glm::mat4 worldMatrix;
                                            std::memcpy(glm::value_ptr(worldMatrix), matrix.data(), sizeof(glm::mat4));

                                            meshTransforms[node.meshIndex.value()].push_back(worldMatrix);
                                        }
                                    });
    }

    for (size_t meshIdx = 0; meshIdx < gltf.meshes.size(); ++meshIdx)
    {
        auto& gltfMesh = gltf.meshes[meshIdx];
        auto& transforms = meshTransforms[meshIdx];

        // If this mesh is never instanced in the scene graph, skip uploading its data entirely
        if (transforms.empty()) continue;

        Mesh mesh{};
        mesh.name = gltfMesh.name.empty() ? "GLTF_Mesh" : std::string(gltfMesh.name);

        bool hasNormals = false;
        bool hasUVs = false;

        for (auto& prim : gltfMesh.primitives)
        {
            u32 initialVertex = static_cast<u32>(mesh.unifiedVertices.size());
            u32 initialIndex = static_cast<u32>(mesh.unifiedIndices.size());

            // --- Indices ---
            if (prim.indicesAccessor.has_value())
            {
                auto& indexAccessor = gltf.accessors[prim.indicesAccessor.value()];
                mesh.unifiedIndices.reserve(mesh.unifiedIndices.size() + indexAccessor.count);

                fastgltf::iterateAccessorWithIndex<u32>(gltf, indexAccessor, [&](u32 index, size_t)
                {
                    mesh.unifiedIndices.push_back(index + initialVertex);
                });
            }

            // --- Positions ---
            const auto posIt = prim.findAttribute("POSITION");
            if (posIt == prim.attributes.end()) continue;

            auto& posAccessor = gltf.accessors[posIt->accessorIndex];
            mesh.unifiedVertices.resize(initialVertex + posAccessor.count);

            fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 pos, size_t idx)
            {
                mesh.unifiedVertices[initialVertex + idx].position = pos;
                mesh.unifiedVertices[initialVertex + idx].color = glm::vec3(1.0f);
            });

            // --- Normals ---
            if (auto normIt = prim.findAttribute("NORMAL"); normIt != prim.attributes.end())
            {
                hasNormals = true;
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normIt->accessorIndex],
                                                              [&](glm::vec3 normal, size_t idx)
                                                              {
                                                                  mesh.unifiedVertices[initialVertex + idx].normal =
                                                                      normal;
                                                              });
            }

            // --- UVs ---
            if (auto uvIt = prim.findAttribute("TEXCOORD_0"); uvIt != prim.attributes.end())
            {
                hasUVs = true;
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uvIt->accessorIndex],
                                                              [&](glm::vec2 uv, size_t idx)
                                                              {
                                                                  mesh.unifiedVertices[initialVertex + idx].uv = uv;
                                                              });
            }

            // --- Tangents ---
            if (auto tangIt = prim.findAttribute("TANGENT"); tangIt != prim.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[tangIt->accessorIndex],
                                                              [&](glm::vec4 tangent, size_t idx)
                                                              {
                                                                  mesh.unifiedVertices[initialVertex + idx].tangent =
                                                                      tangent;
                                                              });
            }

            const u32 indexCount = static_cast<u32>(mesh.unifiedIndices.size()) - initialIndex;
            const u32 matIndex = static_cast<u32>(prim.materialIndex.value_or(0));

            for (const auto& transform : transforms)
            {
                MeshPart part{};
                part.firstIndex = initialIndex;
                part.indexCount = indexCount;
                part.materialIndex = matIndex;
                part.localTransform = transform;

                mesh.parts.push_back(part);
            }
        }

        PostProcessMesh(mesh, hasNormals, hasUVs);
        outMeshes.push_back(std::move(mesh));
    }
}

Result<LoadedModel> MeshLoader::LoadOBJ(const std::filesystem::path& path,
                                        AssetPool<Renderer::TextureData>* texturePool)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::filesystem::path baseDir = path.parent_path();
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(), baseDir.string().c_str(),
                          true))
    {
        LOG(Error, "[tinyobj] Failed to load '{}': {}", path.string(), err);
        return std::unexpected(AssetLoadFailed);
    }

    LoadedModel loaded{.sourceType = Renderer::MeshSourceType::OBJ};
    loaded.materials.reserve(materials.size());
    loaded.meshes.reserve(shapes.size());

    // Material Mapping
    for (const auto& mat : materials)
    {
        std::string normalPath = "";
        if (!mat.normal_texname.empty())
        {
            normalPath = (baseDir / mat.normal_texname).string();
        }
        else if (!mat.bump_texname.empty())
        {
            normalPath = (baseDir / mat.bump_texname).string();
        }
        else if (!mat.displacement_texname.empty())
        {
            normalPath = (baseDir / mat.displacement_texname).string();
        }

        Engine::MaterialType type = Engine::MaterialType::Opaque;
        if (mat.dissolve < 1.0f)
        {
            type = Engine::MaterialType::Transparent;
        }
        else if (!mat.alpha_texname.empty())
        {
            type = Engine::MaterialType::AlphaMask;
        }

        Material out = {
            .name = mat.name,
            .albedoPath = !mat.diffuse_texname.empty() ? (baseDir / mat.diffuse_texname).string() : "",
            .normalPath = normalPath,
            .materialType = type,

            // PBR Properties and Fallbacks
            .baseColor = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]},
            .roughness = mat.roughness,
            .metallic = mat.metallic,
            .ior = mat.ior, // Index of Refraction (Ni)
            .opacity = mat.dissolve, // Dissolve (d)
            .emissive = glm::vec3(mat.emission[0], mat.emission[1], mat.emission[2])
        };

        if (texturePool)
        {
            if (!out.albedoPath.empty())
            {
                auto res = texturePool->Load(out.albedoPath, [](const std::string& p)
                {
                    return TextureLoader::LoadTextureFromSTB(p, true);
                });
                if (res) out.albedoHandle = *res;
            }

            if (!out.normalPath.empty())
            {
                auto res = texturePool->Load(out.normalPath, [](const std::string& p)
                {
                    return TextureLoader::LoadTextureFromSTB(p, false);
                });
                if (res) out.normalHandle = *res;
            }
        }

        loaded.materials.push_back(std::move(out));
    }


    // Mesh Extraction
    for (const auto& shape : shapes)
    {
        Mesh mesh{.name = shape.name};
        BuildMeshPartsFromShape(attrib, shape, mesh);

        PostProcessMesh(mesh, !attrib.normals.empty(), !attrib.texcoords.empty());
        loaded.meshes.push_back(std::move(mesh));
    }

    return loaded;
}

void MeshLoader::PostProcessMesh(Mesh& mesh, const bool hasNormals, const bool hasUVs)
{
    if (mesh.unifiedIndices.empty()) return;

    // Meshoptimizer: Reindex and Optimize
    const size_t indexCount = mesh.unifiedIndices.size();
    const size_t vertexCount = mesh.unifiedVertices.size();

    Vector<u32> remap(indexCount);
    const size_t uniqueVerts = meshopt_generateVertexRemap(remap.data(), mesh.unifiedIndices.data(), indexCount,
                                                           mesh.unifiedVertices.data(), vertexCount, sizeof(Vertex));

    Vector<u32> optIndices(indexCount);
    Vector<Vertex> optVertices(uniqueVerts);

    meshopt_remapIndexBuffer(optIndices.data(), mesh.unifiedIndices.data(), indexCount, remap.data());
    meshopt_remapVertexBuffer(optVertices.data(), mesh.unifiedVertices.data(), vertexCount, sizeof(Vertex),
                              remap.data());

    for (const auto& part : mesh.parts)
    {
        meshopt_optimizeVertexCache(&optIndices[part.firstIndex], &optIndices[part.firstIndex], part.indexCount,
                                    uniqueVerts);
        meshopt_optimizeOverdraw(&optIndices[part.firstIndex], &optIndices[part.firstIndex], part.indexCount,
                                 &optVertices[0].position.x, uniqueVerts, sizeof(Vertex), 1.05f);
    }

    meshopt_optimizeVertexFetch(optVertices.data(), optIndices.data(), indexCount, optVertices.data(), uniqueVerts,
                                sizeof(Vertex));

    mesh.unifiedVertices = std::move(optVertices);
    mesh.unifiedIndices = std::move(optIndices);

    if (!hasNormals) GenerateNormals(mesh.unifiedVertices, mesh.unifiedIndices);
    if (hasUVs) GenerateMikkTangents(mesh.unifiedVertices, mesh.unifiedIndices);


    // Setting up aabb's!
    for (auto& part : mesh.parts)
    {
        const auto partIndices = Span<const u32>(mesh.unifiedIndices.data(), mesh.unifiedIndices.size())
            .subspan(part.firstIndex, part.indexCount);

        const Span<const Vertex> allVertices(mesh.unifiedVertices.data(), mesh.unifiedVertices.size());

        part.aabb = AABB(partIndices, allVertices);
    }
}

void MeshLoader::GenerateNormals(Vector<Vertex>& verts, const Vector<u32>& indices)
{
    for (auto& v : verts) v.normal = {0, 0, 0};
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const glm::vec3 fn = glm::cross(verts[indices[i + 1]].position - verts[indices[i]].position,
                                        verts[indices[i + 2]].position - verts[indices[i]].position);
        verts[indices[i]].normal += fn;
        verts[indices[i + 1]].normal += fn;
        verts[indices[i + 2]].normal += fn;
    }
    for (auto& v : verts) v.normal = glm::normalize(v.normal);
}
