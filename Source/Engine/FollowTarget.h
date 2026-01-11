//
// Created by Orgest on 11/11/2025.
//

#pragma once
#include "../PrimTypes.h"
#include "glm/vec3.hpp"
#include "glm/ext/quaternion_geometric.hpp"

struct FollowTargetComponent
{
	glm::vec3* target;
	glm::vec3 velocity;

	float maxSpeed;
	float maxAcceleration;

	void Update(glm::vec3 position, const f32 dt)
	{
		if (!target) return;

		const glm::vec3 direction = *target - position;
		const float distance = glm::length(direction);

		if (distance > 0.000f) return;

		const glm::vec3 directionNormal = glm::normalize(direction);

		velocity += directionNormal * maxAcceleration * dt;

		const float speed = glm::length(velocity);
		if (speed > maxSpeed)
		{
			velocity = glm::normalize(velocity) * maxSpeed;

		}

		position += dt;
	}
};

