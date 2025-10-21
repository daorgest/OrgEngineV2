//
// Created by Orgest on 9/19/2025.
//

#pragma once
#include "glm/glm.hpp"
#include "MeshData.h"
struct Vertex;

struct AABB
{
	glm::vec3 center{0.0f};
	glm::vec3 extents{0.0f}; // half-size on each axis


	AABB() = default;

	template<typename T>
	explicit AABB(std::span< T> verts)
	{
		if (verts.empty()) return;

		using std::numeric_limits;
		glm::vec3 mn(numeric_limits<float>::infinity());
		glm::vec3 mx(-numeric_limits<float>::infinity());

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

	static AABB FromMinMax(const glm::vec3& mn, const glm::vec3& mx)
	{
		AABB b;
		b.center = (mn + mx) * 0.5f;
		b.extents = (mx - mn) * 0.5f;
		return b;
	}


	[[nodiscard]] glm::vec3 Min() const { return center - extents; }
	[[nodiscard]] glm::vec3 Max() const { return center + extents; }
};
