//
// Created by Orgest on 7/6/2025.
//

#ifndef CAMERA_H
#define CAMERA_H
#include <glm/glm.hpp>

enum class CameraMode { FreeFly, FPS };

struct Camera
{
	glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
	glm::vec3 position = {0.0f, 0.0f, 0.0f};

	glm::vec3 forward = {0.0f, 0.0f, 1.0f};
	glm::vec3 up      = {0.0f, 1.0f, 0.0f};
	glm::vec3 right   = {1.0f, 0.0f, 0.0f};

	glm::mat4 view       = {1.0f};
	glm::mat4 projection = {1.0f};

	f32 yaw = 0.0f;
	f32 pitch = 0.0f;

	f32 fov       = 70.0f;
	f32 nearPlane = 0.01f;
	f32 farPlane  = 10000.0f;

	void Update(f32 deltaTime);
	void UpdateDirectionVectors();
	[[nodiscard]] glm::mat4 GetViewMatrix() const;
	[[nodiscard]] glm::mat4 GetProjectionMatrix(f32 aspectRatio) const;
};



#endif //CAMERA_H
