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
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    f32 fov = 70.0f;
    f32 nearPlane = 0.01f;
    f32 farPlane = 10000.0f;

    [[nodiscard]] glm::vec3 GetForward() const { return rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
    [[nodiscard]] glm::vec3 GetUp() const { return rotation * glm::vec3(0.0f, 1.0f, 0.0f); }
    [[nodiscard]] glm::vec3 GetRight() const { return rotation * glm::vec3(1.0f, 0.0f, 0.0f); }

    void Update(const glm::vec3& position, f32 aspectRatio);
    [[nodiscard]] glm::mat4 GetViewMatrix(const glm::vec3& position) const;
    [[nodiscard]] glm::mat4 GetProjectionMatrix(f32 aspectRatio) const;
    [[nodiscard]] glm::mat4 GetViewProjectionMatrix() const;

    Ray CreateCameraRay(const glm::vec2& screenCoord) const;
};

struct CameraComponent
{
    glm::vec3 position;
    Camera camera;
    FPSCamera controller;
};
