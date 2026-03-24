//
// Created by Orgest on 3/7/2026.
//

#pragma once
#include "glm/vec4.hpp"

enum class MaterialType : u32;
struct TextureData;
using TextureHandle = ResourceHandle<TextureData>;
struct NewMaterial
{
    std::string name;
    TextureHandle albedoMap    = {};
    TextureHandle normalMap    = {};
    TextureHandle metallicMap  = {};
    TextureHandle roughnessMap = {};
    TextureHandle occlusionMap = {};
    TextureHandle emissiveMap  = {};

    MaterialType type = MaterialType::Opaque;

    // PBR factors
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic  = 0.0f;
};

using MaterialHandle = ResourceHandle<NewMaterial>;