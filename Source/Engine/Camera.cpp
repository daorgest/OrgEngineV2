//
// Created by Orgest on 7/6/2025.
//

#include "Camera.h"

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

	forward = Vec3{
		std::cos(pitchRad) * std::sin(yawRad), // X
		std::sin(pitchRad),                    // Y
		std::cos(pitchRad) * std::cos(yawRad)  // Z
	}.Normalized();

	right = forward.Cross(Vec3{0, 1, 0}).Normalized();
	up    = right.Cross(forward).Normalized();
}

Mat4x4 Camera::GetViewMatrix() const
{
	return Mat4x4::LookAt(position, position + forward, up);
}

Mat4x4 Camera::GetProjectionMatrix(f32 aspectRatio) const
{
	Mat4x4 proj = Mat4x4::Perspective(Radians(fov), aspectRatio, nearPlane, farPlane);
	proj.m[5] *= -1.0f; // Y-flip for Vulkan
	return proj;
}
