//
// Created by Orgest on 9/19/2025.
//

#pragma once
#include <span>
#include "Tools/Array.h"
#include "glm/glm.hpp"
struct Vertex;

struct AABB
{
	glm::vec3 center{0.0f};
	glm::vec3 extents{0.0f}; // half-size on each axis


	AABB() = default;

	template <typename T>
	explicit AABB(std::span<T> verts)
	{
		if (verts.empty()) return;

		using std::numeric_limits;
		glm::vec3 mn(numeric_limits<f32>::infinity());
		glm::vec3 mx(-numeric_limits<f32>::infinity());

		if constexpr (std::is_same_v<T, glm::vec3>)
		{
			// Direct vec3 case
			for (const auto& v : verts)
			{
				mn = glm::min(mn, v);
				mx = glm::max(mx, v);
			}
		}
		else
		{
			for (const auto& v : verts)
			{
				if constexpr (requires { v.position; })
				{
					mn = glm::min(mn, v.position);
					mx = glm::max(mx, v.position);
				}
				else if constexpr (requires { v.pos; })
				{
					mn = glm::min(mn, v.pos);
					mx = glm::max(mx, v.pos);
				}
				else
				{
					static_assert(false, "Unsupported vertex type for AABB: must have .pos or .position");
				}
			}
		}

		center = (mn + mx) * 0.5f;
		extents = (mx - mn) * 0.5f;
	}

	template<typename V>
	requires (requires (V v) { v.position; } || requires (V v) { v.pos; })
	explicit AABB(std::span<const u32> indices, std::span<const V> allVertices)
    {
        if (indices.empty() || allVertices.empty()) return;

        using std::numeric_limits;
        glm::vec3 mn(numeric_limits<f32>::infinity());
        glm::vec3 mx(-numeric_limits<f32>::infinity());

        for (const u32 index : indices)
        {
            if (index < allVertices.size())
            {
				if constexpr (requires (V v) { v.position; })
				{
                const glm::vec3& pos = allVertices[index].position;
                mn = glm::min(mn, pos);
                mx = glm::max(mx, pos);
				}
				else
				{
					const glm::vec3& pos = allVertices[index].pos;
					mn = glm::min(mn, pos);
					mx = glm::max(mx, pos);
				}
            }
        }

        center = (mn + mx) * 0.5f;
        extents = (mx - mn) * 0.5f;
    }

	void MergeAABB(const AABB& other)
	{
	    if (extents == glm::vec3(0.0f))
	    {
	        center = other.center;
	        extents = other.extents;
	        return;
	    }

		const glm::vec3 mn = glm::min(Min(), other.Min());
		const glm::vec3 mx = glm::max(Max(), other.Max());

		center = (mn + mx) * 0.5f;
		extents = (mx - mn) * 0.5f;
	}


	[[nodiscard]] glm::vec3 Min() const { return center - extents; }
	[[nodiscard]] glm::vec3 Max() const { return center + extents; }
};

struct Frustum
{
	Array<glm::vec4, 6> planes; // Left, Right, Bottom, Top, Near, Far

	void Update(const glm::mat4& m)
	{
		// Gribb-Hartmann Method using row-major access
		// In GLM, m[column][row]. We extract rows to build plane equations.

		// Left Plane: Row 4 + Row 1
		planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
		// Right Plane: Row 4 - Row 1
		planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
		// Bottom Plane: Row 4 + Row 2
		planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
		// Top Plane: Row 4 - Row 2
		planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
		// Near Plane: Row 4 - Row 3
	    planes[4] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);
	    // Far Plane is just Row 3
	    planes[5] = glm::vec4(m[0][2], m[1][2], m[2][2], m[3][2]);
		// Normalize planes
		for (auto& plane : planes)
		{
			plane /= glm::length(glm::vec3(plane));
		}
	}

	bool IsBoxInFrustum(const AABB& aabb, const glm::mat4& transform) const
	{
		// Transform AABB center to World Space
		const auto globalCenter = glm::vec3(transform * glm::vec4(aabb.center, 1.0f));

		// Transform AABB extents to World Space (Radius-on-Plane method)
		const glm::vec3 right = glm::abs(glm::vec3(transform[0])) * aabb.extents.x;
		const glm::vec3 up    = glm::abs(glm::vec3(transform[1])) * aabb.extents.y;
		const glm::vec3 fwd   = glm::abs(glm::vec3(transform[2])) * aabb.extents.z;
		const glm::vec3 globalExtents = right + up + fwd;

		for (const auto& plane : planes)
		{
			// Signed distance from center to plane
			const f32 dist = glm::dot(glm::vec3(plane), globalCenter) + plane.w;

			// Maximum radius of the box projected onto the plane normal
			const f32 radius = glm::dot(glm::abs(glm::vec3(plane)), globalExtents);

			// If the box is behind the plane by more than its radius, it is outside
			if (dist < -radius) return false;
		}
		return true;
	}
};
