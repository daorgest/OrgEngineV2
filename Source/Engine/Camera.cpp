//
// Created by Orgest on 7/6/2025.
//

#include "Camera.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "MathFuncs.h"

void Camera::UpdateVecAndMat(const glm::vec3& position, const f32 aspectRatio)
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

    view = glm::lookAt(position, position + forward, up);

    projection = GetProjectionMatrix(aspectRatio);
}

glm::mat4 Camera::GetViewMatrix(const glm::vec3& position) const
{
    return glm::lookAt(position, position + forward, up);
}

glm::mat4 Camera::GetProjectionMatrix(const f32 aspectRatio) const
{
	glm::mat4 proj = glm::perspectiveRH_ZO(Radians(fov), aspectRatio, farPlane, nearPlane);
	proj[1][1] *= -1.0f; // Y-flip for Vulkan
	return proj;
}

glm::mat4 Camera::GetViewProjectionMatrix(f32 aspectRatio) const
{
    return GetProjectionMatrix(aspectRatio) * view;
}