//
// Created by Orgest on 11/12/2025.
//

#pragma once

struct SceneUBO
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct PushConstants
{
	glm::mat4 worldMatrix{};
	glm::mat3 normalMatrix{};  // Pre-computed transpose(inverse(worldMatrix3x3))
	u32 vertexOffset = 0;    // For dynamic vertex indexing
	u64 deviceAddress = 0;
	float roughness = 0.5f;  // Surface roughness [0=smooth, 1=rough]
	float metallic = 0.0f;   // Metallic property [0=dielectric, 1=metal]
	glm::vec3 baseColorTint = glm::vec3(1.0f); // Material base color tint (from MTL Kd)
	float _padding = 0.0f;   // Padding for alignment
};

enum class LightType : u32
{
	Directional = 0,
	Point = 1,
	Spot = 2
};
ENUM_CLASS_BITOPS(LightType)

struct LightUBO
{
	glm::vec3 position;
	float range;
	glm::vec3 direction;
	float innerCone;
	glm::vec3 color;
	float intensity;
	u32 type;
	float outerCone;
};

struct DebugUBO
{
	DebugView debugMode;
	f32 debugDepthRange;
	u32 disableNormalMap = 1;  // 1 = don't sample/use normalTexture
	u32 disableSpecular = 0;    // 1 = skip specular BRDF term

	// PBR/IBL Runtime Tuning Parameters
	f32 iblStrength = 1.5f;
	f32 iblRoughnessMipBias = 0.0f;
	f32 ambientStrength = 0.1f;
	f32 aoStrength = 0.3f;
	f32 metallicReflectScale = 1.2f;
	f32 roughnessReflectScale = 1.0f;
};

struct LightUBOCount
{
	u32 count;
};

struct CameraUBO
{
	glm::vec3 position{};
	f32  nearPlane{};
	f32  farPlane{};
};