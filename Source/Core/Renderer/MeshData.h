//
// Created by Orgest on 6/28/2025.
//
#pragma once
#include "RendererTypes.h"
#include "Tools/Vector.h"

#include <string>

#include <type_traits>  // std::underlying_type_t
#include <utility>      // std::to_underlying (C++23)

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "AABB.h"

#define ENUM_CLASS_BITOPS(Enum)                                                        \
[[nodiscard]] constexpr Enum operator|(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) | std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator&(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) & std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator^(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) ^ std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator~(Enum a) noexcept {                            \
return static_cast<Enum>(~std::to_underlying(a)); }                                 \
constexpr Enum& operator|=(Enum& a, Enum b) noexcept { return a = (a | b); }         \
constexpr Enum& operator&=(Enum& a, Enum b) noexcept { return a = (a & b); }         \
constexpr Enum& operator^=(Enum& a, Enum b) noexcept { return a = (a ^ b); }         \
/* == / != against underlying integer type */                                        \
[[nodiscard]] constexpr bool operator==(Enum a, std::underlying_type_t<Enum> u) noexcept { \
return std::to_underlying(a) == u; }                                               \
[[nodiscard]] constexpr bool operator==(std::underlying_type_t<Enum> u, Enum a) noexcept { \
return u == std::to_underlying(a); }                                               \
[[nodiscard]] constexpr bool operator!=(Enum a, std::underlying_type_t<Enum> u) noexcept { \
return std::to_underlying(a) != u; }                                               \
[[nodiscard]] constexpr bool operator!=(std::underlying_type_t<Enum> u, Enum a) noexcept { \
return u != std::to_underlying(a); }

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 tangent;
	glm::vec3 color;
	glm::vec2 uv;
};

struct Particle2D
{
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;
};

struct BBoxPush
{
	glm::mat4 model;       // object->world for the AABB's owner
	glm::vec3 aabbMin;     float depthBias;  // e.g. 0.0001f or 0.0f
	glm::vec3 aabbMax;     uint32_t flags;   // optional
	glm::vec4 color;       // e.g. (1,1,0,1)
};


struct MeshPart
{
	Vector<Vertex> vertices;
	Vector<u32> indices;
	AABB aabb;
	u32 materialIndex = 0;
};

struct Mesh
{
	Vector<MeshPart> parts;
	std::string name;
};

struct Material
{
	std::string name;
	std::string albedoPath;
	std::string normalPath;
	std::string specularPath;
	std::string emissivePath;
};

struct LoadedModel
{
	Vector<Mesh> meshes;
	Vector<Material> materials;
	MeshSourceType sourceType = MeshSourceType::Unknown;
};

struct SceneUBO
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct PushConstants
{
	glm::mat4 worldMatrix{};
	u64 deviceAddress = 0;
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
};

struct LightMeta
{
	u32 count;
};

struct CameraUBO
{
	glm::vec3 position{};
	f32  nearPlane{};
	f32  farPlane{};
};

struct NormalMatrixUBO
{
	glm::mat3 normalMatrix;
};

struct MaterialPropertiesUBO
{
	u32 hasNormalMap;
	f32 normalScale;
};