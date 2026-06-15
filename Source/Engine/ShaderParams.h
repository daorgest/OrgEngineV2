//
// Created by Orgest on 11/12/2025.
//

#pragma once

namespace Engine
{
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
        u32 specularIndex = 0;

        MaterialType type = MaterialType::Opaque;
    };

    struct PushConstants
    {
        glm::mat4 model = {1.0f};
        u64 vertexBufferAddress = 0;
        u32 vertexOffset = 0;
        Renderer::RenderPath renderPath = Renderer::RenderPath::Standard;
        f32 instRoughness = 1.0f;
        f32 instMetallic = 1.0f;
        u32 materialIndex = 0;
    };

    struct GPUInstanceSSBO
    {
        glm::mat4 worldMatrix;
        u32 materialIndex;
        f32 roughness;
        f32 metallic;
    };

    struct BBoxPush
    {
        glm::mat4 model;
        glm::vec3 aabbMin;
        f32 depthBias;
        glm::vec3 aabbMax;
        u32 flags;
        glm::vec4 color;
    };

    static_assert(sizeof(BBoxPush) == 112, "Unexpected padding in BBoxPush!");

    enum class LightType : u32 { Directional = 0, Point = 1, Spot = 2 };

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
        Renderer::DebugView debugMode;
        f32 debugDepthRange = 32.4f;
        u32 disableNormalMap = 0;
        f32 normalStrength = 1.0f;
        u32 disableSpecular = 0;
        f32 specularStrength = 1.0f;

        f32 iblStrength = 1.0f;
        f32 iblRoughnessMipBias = 0.0f;
        f32 ambientStrength = 0.2f;
        f32 aoStrength = 1.0f;

        f32 metallicReflectScale = 1.0f;
        f32 roughnessReflectScale = 1.0f;
        glm::vec3 shadowTint = glm::vec3(0.5f, 0.5f, 0.7f);
    };

    struct LightSceneData
    {
        LightUBO lights[MAX_LIGHTS];
        u32 count;
    };

    struct CameraUBO
    {
        glm::vec3 position;
        f32 nearPlane;
        f32 farPlane;
    };

    struct GPUIndirectCommand
    {
        u32 indexCount;
        u32 instanceCount;
        u32 firstIndex;
        i32 vertexOffset;
        u32 firstInstance;
    };

    struct VisibilityVolumeConstants
    {
        glm::vec3 volumeMin;
        u32 gridResolutionX = 0;
        glm::vec3 volumeMax;
        u32 gridResolutionY = 0;
        u32 gridResolutionZ = 0;
        u32 bindlessTexIndex = 0;
        u32 enabled = 0;
    };
} // namespace Engine
