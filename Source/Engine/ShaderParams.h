//
// Created by Orgest on 11/12/2025.
//

#pragma once

struct SceneUBO
{
	glm::mat4 view;
	glm::mat4 proj;
};

enum class MaterialType : u32 { Opaque, AlphaMask, Transparent };

struct MaterialProperties
{
	glm::vec4 baseColor = glm::vec4(1.0f);
	glm::vec3 emissive = glm::vec3(0.0f);
	f32 roughness = 0.5f;
	f32 metallic = 0.0f;
	f32 ior = 1.5f;

	u32 albedoIndex = 0;
	u32 normalIndex = 0;

	MaterialType type = MaterialType::Opaque;
};

struct PushConstants
{
	glm::mat4 model;
    u64 vertexBufferAddress = 0;
    u32 vertexOffset;
	u32 isInstanced = 0;
	f32 instRoughness = 1.0f;
	f32 instMetallic = 1.0f;
	u32 materialIndex;
};

struct GPUInstanceSSBO
{
	glm::mat4 worldMatrix;
	u32 materialIndex;
	f32 roughness;           // 4 bytes: Unique override for this sphere
	f32 metallic;            // 4 bytes: Unique override for this sphere
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
	f32 range;
	glm::vec3 direction;
	f32 innerCone;
	glm::vec3 color;
	f32 intensity;
	LightType type;
	f32 outerCone;
};

struct DebugUBO
{
	DebugView debugMode;
	f32 debugDepthRange;
	u32 disableNormalMap = 0;
	u32 disableSpecular = 0;

	// PBR/IBL Runtime Tuning Parameters
    f32 iblStrength = 1.0f;
    f32 iblRoughnessMipBias = 0.0f;
    f32 ambientStrength = 0.2f;
    f32 aoStrength = 1.0f;
    f32 metallicReflectScale = 1.0f;
    f32 roughnessReflectScale = 1.0f;

    // --- Toon/Deadlock Tuning Defaults ---
    f32 toonThreshold = 0.24f;
    f32 toonSmoothness = 0.105f;
    f32 rimThreshold = 0.83f;
    f32 rimSmoothness = 0.25f;
    f32 rimIntensity = 0.07f;
    glm::vec3 shadowTint = glm::vec3(0.5f, 0.5f, 0.7f);
};

struct LightSceneData
{
    LightUBO lights[16];
	u32 count;
};

struct CameraUBO
{
	glm::vec3 position;
	f32  nearPlane;
	f32  farPlane;
};

static_assert(sizeof(CameraUBO) == 20);

struct GPUIndirectCommand
{
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    i32 vertexOffset;
    u32 firstInstance;
};