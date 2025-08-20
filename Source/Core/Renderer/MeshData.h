//
// Created by Orgest on 6/28/2025.
//
#pragma once
#include <span>

#include "Mat4x4.h"
#include "RendererTypes.h"
#include "Vec2.h"
#include "Vec3.h"
#include "Vector.h"

#include <string>
#include <unordered_map>
#include <variant>

#include "Vec4.h"

#include <type_traits>  // std::underlying_type_t
#include <utility>      // std::to_underlying (C++23)

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

struct Vertex {
	Vec3 position;
	Vec3 normal;
	Vec3 color;
	Vec2 uv;

	bool operator==(const Vertex& other) const {
		return AlmostEqual(position, other.position) &&
			   AlmostEqual(normal, other.normal) &&
			   AlmostEqual(uv, other.uv) &&
			   AlmostEqual(color, other.color);
	}

private:
	// Templated approximate equality for Vec2/Vec3
	template<typename Vec>
	static bool AlmostEqual(const Vec& a, const Vec& b, float epsilon = 1e-5f) {
		for (int i = 0; i < a.Length(); ++i) {
			if (std::abs(a[i] - b[i]) > epsilon)
				return false;
		}
		return true;
	}
};
template <>
struct std::hash<Vertex> {
	size_t operator()(const Vertex& v) const noexcept
	{
		const size_t h1 = hash<float>()(v.position.x) ^ hash<float>()(v.position.y) << 1 ^ hash<float>()(v.position.z) << 2;
		const size_t h2 = hash<float>()(v.normal.x) << 1 ^ hash<float>()(v.normal.y) << 2 ^ hash<float>()(v.normal.z) << 3;
		const size_t h3 = hash<float>()(v.uv.x) << 1 ^ hash<float>()(v.uv.y) << 2;
		const size_t h4 = hash<float>()(v.color.x) << 1 ^ hash<float>()(v.color.y) << 2 ^ hash<float>()(v.color.z) << 3;
		return (((h1 ^ h2) ^ h3) ^ h4);
	}
};

struct Particle2D
{
	Vec2 position;
	Vec2 velocity;
	Vec4 color;
};

struct MeshPart {
	Vector<Vertex> vertices;
	Vector<u32> indices;
	u32 materialIndex = 0;
};

struct Mesh {
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
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
	MeshSourceType sourceType = MeshSourceType::Unknown;
};

struct UBO
{
	Mat4x4 model;
	Mat4x4 view;
	Mat4x4 proj;
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
	Vec3 position;
	float range;
	Vec3 direction;
	float innerCone;
	Vec3 color;
	float intensity;
	u32 type;
	float outerCone;
};

struct DebugUBO
{
	DebugView debugMode;
};

struct LightMeta
{
	u32 count;
};

struct CameraUBO
{
	Vec3 position{};
	f32  nearPlane{};
	f32  farPlane{};
};


struct RenderDrawPushConstants
{
	Mat4x4 worldMatrix;
	u64 deviceAddress = 0;
};