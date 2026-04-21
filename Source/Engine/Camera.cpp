//
// Created by Orgest on 7/6/2025.
//

#include "Camera.h"

#include <cmath>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "MathFuncs.h"
#include "glm/gtc/quaternion.hpp"

void Camera::UpdateVecAndMat(const glm::vec3& position, const f32 aspectRatio)
{
    rotation = glm::quat(glm::vec3(
        Radians(pitch), // pitch (X) - looking up/down
        Radians(yaw), // yaw (Y) - looking left/right
        Radians(roll) // roll (Z) - tilting the camera
    ));

    forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    up      = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    right   = rotation * glm::vec3(1.0f, 0.0f, 0.0f);

    view = glm::lookAt(position, position + forward, up);

    projection = GetProjectionMatrix(aspectRatio);
}

glm::mat4 Camera::GetViewMatrix(const glm::vec3& position) const
{
    // This turns our "Rotation" into a "View" (Inverse) Rotation
    glm::mat4 viewRot = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 viewTrans = glm::translate(glm::mat4(1.0f), -position);
    return viewRot * viewTrans;
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

Ray Camera::CreateCameraRay(const glm::vec2& screenCoord) const
{
    // Convert to clip space
    const glm::vec4 clipCoords(screenCoord.x, screenCoord.y, -1.0f, 1.0f);

    // Convert to view space
    const glm::mat4 invProjection = glm::inverse(projection);
    glm::vec4 viewCoords = invProjection * clipCoords;
    viewCoords.z = -1.0f;  // Point towards negative Z in view space
    viewCoords.w = 0.0f;   // Convert to direction vector

    // Convert to world space
    glm::mat4 invView = glm::inverse(view);
    const glm::vec4 worldCoords = invView * viewCoords;

    // Create ray
    Ray ray;
    ray.origin = glm::vec3(invView[3]);  // Camera position in world space
    ray.direction = glm::normalize(glm::vec3(worldCoords));

    return ray;
}
