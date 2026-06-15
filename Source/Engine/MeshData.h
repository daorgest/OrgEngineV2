//
// Created by Orgest on 6/28/2025.
//
#pragma once
#include "../Core/Renderer/RendererTypes.h"
#include "Tools/Vector.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "AABB.h"
#include "ShaderParams.h"

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 tangent;
	glm::vec3 color;
	glm::vec2 uv;
};

struct SkyVertex
{
	glm::vec3 position;
	glm::vec2 uv;
};

struct Particle2D
{
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;
};

struct MeshPart
{
	AABB aabb;
	u32 materialIndex = 0;
	u32 indexCount = 0;
	u32 firstIndex = 0;     // Offset into the Mesh's global index buffer
	u32 vertexOffset = 0;   // Offset to add to each index (base vertex)

	glm::mat4 localTransform = glm::mat4(1.0f);
};

struct Mesh
{
	std::string name;
	Vector<MeshPart> parts;
	Vector<Vertex> unifiedVertices;
	Vector<u32> unifiedIndices;
};

struct Material
{
	std::string name;

	// Texture paths
	std::string albedoPath;
	std::string normalPath;
    std::string specularPath;

    // orr....
    ResourceHandle<Renderer::TextureData> albedoHandle;
    ResourceHandle<Renderer::TextureData> normalHandle;
    ResourceHandle<Renderer::TextureData> specularHandle;

    Engine::MaterialType materialType = Engine::MaterialType::Opaque;

    // PBR material properties (fallback values if no textures)
	glm::vec3 baseColor = glm::vec3(1.0f);  // Base albedo color (Kd in MTL)
	f32 roughness = 0.5f;      // Surface roughness [0=smooth, 1=rough] (Pr in MTL)
	f32 metallic = 0.0f;       // Metallic factor [0=dielectric, 1=metal] (Pm in MTL)
	f32 ior = 1.5f;            // Index of refraction (Ni in MTL)
	f32 opacity = 1.0f;        // Opacity/transparency (d in MTL)
	glm::vec3 emissive = glm::vec3(0.0f); // Emissive color (Ke in MTL)
};

struct LoadedModel
{
	Vector<Mesh> meshes;
	Vector<Material> materials;
    Renderer::MeshSourceType sourceType = Renderer::MeshSourceType::Unknown;
};
