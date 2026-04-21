//
// Created by Orgest on 7/6/2025.
//

#pragma once
#include <glm/glm.hpp>

#include "FPSCamera.h"
#include "Ray.h"
#include "glm/detail/type_quat.hpp"

enum class CameraMode { FreeFly, FPS };

struct Camera
{
	glm::vec3 forward = {0.0f, 0.0f, 1.0f};
	glm::vec3 up      = {0.0f, 1.0f, 0.0f};
	glm::vec3 right   = {1.0f, 0.0f, 0.0f};

    glm::quat rotation = {0.0f, 0.0f, 0.0f, 1.0f};

	glm::mat4 view       = {1.0f};
	glm::mat4 projection = {1.0f};

	f32 yaw = 0.0f;
	f32 pitch = 0.0f;
    f32 roll = 0.0f;
	f32 fov       = 70.0f;
	f32 nearPlane = 0.01f;
	f32 farPlane  = 10000.0f;

    void UpdateVecAndMat(const glm::vec3& position, f32 aspectRatio);
    [[nodiscard]] glm::mat4 GetViewMatrix(const glm::vec3& position) const;
	[[nodiscard]] glm::mat4 GetProjectionMatrix(f32 aspectRatio) const;
    [[nodiscard]] glm::mat4 GetViewProjectionMatrix(f32 aspectRatio) const;

    Ray CreateCameraRay(const glm::vec2& screenCoord) const;
};

struct CameraComponent
{
    Camera base;
    FPSCamera controller;
    glm::vec3 position;
};