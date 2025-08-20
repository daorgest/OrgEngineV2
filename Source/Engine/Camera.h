//
// Created by Orgest on 7/6/2025.
//

#ifndef CAMERA_H
#define CAMERA_H
#include "Mat4x4.h"


struct Camera
{
	Vec3 velocity = {0.0f, 0.0f, 0.0f};
	Vec3 position = {0.0f, 0.0f, 0.0f};

	Vec3 forward = {0.0f, 0.0f, 1.0f};
	Vec3 up      = {0.0f, 1.0f, 0.0f};
	Vec3 right   = {1.0f, 0.0f, 0.0f};

	Mat4x4 view       = {1.0f};
	Mat4x4 projection = {1.0f};

	f32 yaw = 0.0f;
	f32 pitch = 0.0f;

	f32 fov       = 70.0f;
	f32 nearPlane = 0.01f;
	f32 farPlane  = 10000.0f;

	void Update(f32 deltaTime);
	void UpdateDirectionVectors();
	[[nodiscard]] Mat4x4 GetViewMatrix() const;
	[[nodiscard]] Mat4x4 GetProjectionMatrix(f32 aspectRatio) const;
};



#endif //CAMERA_H
