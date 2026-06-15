//
// Created by Orgest on 7/6/2025.
//
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "MathFuncs.h"
#include "glm/gtc/quaternion.hpp"

void Camera::Update(const glm::vec3& position, const f32 aspectRatio)
{
    view       = GetViewMatrix(position);
    projection = GetProjectionMatrix(aspectRatio);
}

glm::mat4 Camera::GetViewMatrix(const glm::vec3& position) const
{
    // This turns the "Rotation" into a "View" (Inverse) Rotation
    const glm::mat4 viewRot   = glm::mat4_cast(glm::conjugate(rotation));
    const glm::mat4 viewTrans = glm::translate(glm::mat4(1.0f), -position);
    return viewRot * viewTrans;
}

glm::mat4 Camera::GetProjectionMatrix(const f32 aspectRatio) const
{
	glm::mat4 proj = glm::perspective(Radians(fov), aspectRatio, farPlane, nearPlane);
	proj[1][1] *= -1.0f; // Y-flip for Vulkan
	return proj;
}

glm::mat4 Camera::GetViewProjectionMatrix() const
{
    return projection * view;
}

Ray Camera::CreateCameraRay(const glm::vec2& screenCoord) const
{
    const glm::vec4 clipCoords(screenCoord.x, screenCoord.y, -1.0f, 1.0f);

    glm::vec4 viewCoords = glm::inverse(projection) * clipCoords;
    viewCoords.z = -1.0f;
    viewCoords.w = 0.0f;

    const glm::vec4 worldCoords = glm::inverse(view) * viewCoords;

    Ray ray;
    ray.origin    = glm::vec3(glm::inverse(view)[3]);
    ray.direction = glm::normalize(glm::vec3(worldCoords));
    return ray;
}
