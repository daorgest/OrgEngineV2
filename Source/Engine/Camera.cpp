//
// Created by Orgest on 7/6/2025.
//

#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "MathFuncs.h"

void Camera::Update(f32 deltaTime)
{
	position += velocity * deltaTime;

	view = GetViewMatrix();
}

void Camera::UpdateDirectionVectors()
{
	const f32 yawRad   = Radians(yaw);
	const f32 pitchRad = Radians(pitch);

	forward = glm::normalize(glm::vec3(
		std::cos(pitchRad) * std::sin(yawRad), // +X = turn right
		std::sin(pitchRad),                    // +Y = look up
		std::cos(pitchRad) * std::cos(yawRad)  // +Z = look forward
	));

	right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	up    = glm::normalize(glm::cross(right, forward));
}

glm::mat4 Camera::GetViewMatrix() const
{
	return glm::lookAt(position, position + forward, up);
}

glm::mat4 Camera::GetProjectionMatrix(f32 aspectRatio) const
{
	glm::mat4 proj = glm::perspective(Radians(fov), aspectRatio, nearPlane, farPlane);
	proj[1][1] *= -1.0f; // Y-flip for Vulkan
	return proj;
}
