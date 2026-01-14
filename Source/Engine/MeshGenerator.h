//
// Created by Orgest on 11/2/2025.
//
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "MeshData.h"
#include "MikkWrapper.h"

#include "../../Source/PrimTypes.h"
namespace MeshGenerator
{
	// Generate a cube with proper normals, tangents, and UVs
	inline Mesh GenerateCube(f32 size = 1.0f)
	{
		Mesh mesh;
		mesh.name = "Cube";

		const f32 halfSize = size * 0.5f;

		Vector<Vertex>& vertices = mesh.unifiedVertices;
		Vector<u32>& indices = mesh.unifiedIndices;

		// 24 vertices (4 per face, each face has unique normals)
		vertices.reserve(24);

		// Front face (+Z)
		vertices.push_back({{-halfSize, -halfSize, halfSize}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{halfSize, -halfSize, halfSize}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{halfSize, halfSize, halfSize}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{-halfSize, halfSize, halfSize}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1, 1}, {0, 1}});

		// Back face (-Z)
		vertices.push_back({{halfSize, -halfSize, -halfSize}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{-halfSize, -halfSize, -halfSize}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{-halfSize, halfSize, -halfSize}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{halfSize, halfSize, -halfSize}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1, 1}, {0, 1}});

		// Right face (+X)
		vertices.push_back({{halfSize, -halfSize, halfSize}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{halfSize, -halfSize, -halfSize}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{halfSize, halfSize, -halfSize}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{halfSize, halfSize, halfSize}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1, 1}, {0, 1}});

		// Left face (-X)
		vertices.push_back({{-halfSize, -halfSize, -halfSize}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{-halfSize, -halfSize, halfSize}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{-halfSize, halfSize, halfSize}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{-halfSize, halfSize, -halfSize}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1, 1}, {0, 1}});

		// Top face (+Y)
		vertices.push_back({{-halfSize, halfSize, halfSize}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{halfSize, halfSize, halfSize}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{halfSize, halfSize, -halfSize}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{-halfSize, halfSize, -halfSize}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {0, 1}});

		// Bottom face (-Y)
		vertices.push_back({{-halfSize, -halfSize, -halfSize}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {0, 0}});
		vertices.push_back({{halfSize, -halfSize, -halfSize}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {1, 0}});
		vertices.push_back({{halfSize, -halfSize, halfSize}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {1, 1}});
		vertices.push_back({{-halfSize, -halfSize, halfSize}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1, 1}, {0, 1}});

		// 36 indices (6 faces * 2 triangles * 3 vertices)
		indices = {
			// Front
			0, 1, 2, 2, 3, 0,
			// Back
			4, 5, 6, 6, 7, 4,
			// Right
			8, 9, 10, 10, 11, 8,
			// Left
			12, 13, 14, 14, 15, 12,
			// Top
			16, 17, 18, 18, 19, 16,
			// Bottom
			20, 21, 22, 22, 23, 20
		};

		// Create single mesh part
		MeshPart part{};
		part.indexCount = static_cast<u32>(indices.size());
		part.aabb = AABB(std::span<const Vertex>(vertices));

		mesh.parts.push_back(part);

		return mesh;
	}

	// Generate a UV sphere with given subdivisions
	// segments = horizontal (longitude), rings = vertical (latitude)
	inline auto GenerateSphere(f32 radius = 1.0f, u32 segments = 64, u32 rings = 32) -> Mesh
	{
		Mesh mesh;
		mesh.name = "Sphere";

		Vector<Vertex>& vertices = mesh.unifiedVertices;
		Vector<u32>& indices = mesh.unifiedIndices;

		// Generate vertices
		for (u32 ring = 0; ring <= rings; ++ring)
		{
			const f32 phi = glm::pi<f32>() * static_cast<f32>(ring) / static_cast<f32>(rings);

			for (u32 seg = 0; seg <= segments; ++seg)
			{
				const f32 theta = 2.0f * glm::pi<f32>() * static_cast<f32>(seg) / static_cast<f32>(segments);

				Vertex vert{};
				vert.position = glm::vec3(
					radius * std::sin(phi) * std::cos(theta),
					radius * std::cos(phi),
					radius * std::sin(phi) * std::sin(theta)
				);
				vert.normal = glm::normalize(vert.position);
				vert.uv = glm::vec2(static_cast<f32>(seg) / (f32)segments, 1.0f - ((f32)ring / (f32)rings));
				vert.color = glm::vec3(1.0f);
				vert.tangent = glm::vec4(0.0f);

				vertices.push_back(vert);
			}
		}

		// Generate indices (counter-clockwise winding for outward-facing triangles)
		for (u32 ring = 0; ring < rings; ++ring)
		{
			for (u32 seg = 0; seg < segments; ++seg)
			{
				const u32 current = ring * (segments + 1) + seg;
				const u32 next = current + segments + 1;

				// Two triangles per quad (reversed winding order)
				indices.push_back(current);
				indices.push_back(current + 1);
				indices.push_back(next);

				indices.push_back(current + 1);
				indices.push_back(next + 1);
				indices.push_back(next);
			}
		}

		// Create single mesh part
		MeshPart part{};
		part.indexCount = static_cast<u32>(indices.size());
		part.aabb = AABB(std::span<const Vertex>(vertices));

		mesh.parts.push_back(part);
		GenerateMikkTangents(vertices, indices);
		return mesh;
	}

	// Generate a torus (donut shape)
	// majorRadius = distance from center to tube center
	// minorRadius = tube radius
	// majorSegments = segments around the major circle
	// minorSegments = segments around the tube
	inline auto GenerateTorus(f32 majorRadius = 1.0f, f32 minorRadius = 0.3f, u32 majorSegments = 32, u32 minorSegments = 16) -> Mesh
	{
		Mesh mesh;
		mesh.name = "Torus";

		Vector<Vertex>& vertices = mesh.unifiedVertices;
		Vector<u32>& indices = mesh.unifiedIndices;

		// Generate vertices
		for (u32 i = 0; i <= majorSegments; ++i)
		{
			const f32 u = 2.0f * glm::pi<f32>() * static_cast<f32>(i) / static_cast<f32>(majorSegments);
			const f32 cosU = std::cos(u);
			const f32 sinU = std::sin(u);

			for (u32 j = 0; j <= minorSegments; ++j)
			{
				const f32 v = 2.0f * glm::pi<f32>() * static_cast<f32>(j) / static_cast<f32>(minorSegments);
				const f32 cosV = std::cos(v);
				const f32 sinV = std::sin(v);

				Vertex vert{};

				// Position
				vert.position.x = (majorRadius + minorRadius * cosV) * cosU;
				vert.position.y = minorRadius * sinV;
				vert.position.z = (majorRadius + minorRadius * cosV) * sinU;

				// Normal (points outward from the tube surface)
				vert.normal = glm::normalize(glm::vec3(cosV * cosU, sinV, cosV * sinU));

				// Tangent (derivative with respect to u - along the major circle)
				// dP/du perpendicular to the tube, wrapping around the torus
				glm::vec3 tangent = glm::vec3(
					-(majorRadius + minorRadius * cosV) * sinU,
					0.0f,
					(majorRadius + minorRadius * cosV) * cosU
				);
				tangent = glm::normalize(tangent);
				vert.tangent = glm::vec4(tangent, 1.0f);

				// Default white color
				vert.color = glm::vec3(1.0f, 1.0f, 1.0f);

				// UV coordinates
				vert.uv.x = static_cast<f32>(i) / static_cast<f32>(majorSegments);
				vert.uv.y = static_cast<f32>(j) / static_cast<f32>(minorSegments);

				vertices.push_back(vert);
			}
		}

		// Generate indices
		for (u32 i = 0; i < majorSegments; ++i)
		{
			for (u32 j = 0; j < minorSegments; ++j)
			{
				const u32 current = i * (minorSegments + 1) + j;
				const u32 next = (i + 1) * (minorSegments + 1) + j;

				// Two triangles per quad
				indices.push_back(current);
				indices.push_back(next);
				indices.push_back(current + 1);

				indices.push_back(next);
				indices.push_back(next + 1);
				indices.push_back(current + 1);
			}
		}

		// Create single mesh part
		MeshPart part{};
		part.indexCount = static_cast<u32>(indices.size());
		part.aabb = AABB(std::span<const Vertex>(vertices));

		mesh.parts.push_back(part);

		return mesh;
	}
} // namespace MeshGenerator

