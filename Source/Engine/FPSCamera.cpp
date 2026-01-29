//
// Created by Orgest on 9/19/2025.
//

#include "FPSCamera.h"
#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include "Application.h"
#include "Camera.h"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"

glm::vec3 FPSCamera::ProjectXZ(const glm::vec3& v)
{
	const glm::vec3 p = {v.x, 0.0f, v.z};
	const f32 len2 = glm::dot(p, p);
	return (len2 > 1e-12f) ? p / std::sqrt(len2) : glm::vec3(0, 0, 1);
}

void FPSCamera::Update(CameraComponent& comp, f32 deltaTime)
{
    Camera& cam = comp.base;
    glm::vec3& footPos = comp.position;

    // 1. Mouse Look: Standard Euler rotation
    cam.yaw   -= (f32)input.xrel * tune.mouseSens;
    cam.pitch -= (f32)input.yrel * tune.mouseSens;
    cam.pitch = std::clamp(cam.pitch, -89.9f, 89.9f);

    // 2. Vertical Physics (Gravity/Jumping)
    if (!grounded) velocity.y -= tune.gravity * deltaTime;
    if (grounded && input.IsActionDown(Action::Jump)) {
        velocity.y = tune.jumpForce;
        grounded = false;
    }

    // 3. Movement State
    const i32 side = i32(input.IsActionHeld(Action::MoveRight)) - i32(input.IsActionHeld(Action::MoveLeft));
    const i32 fwd  = i32(input.IsActionHeld(Action::MoveForward)) - i32(input.IsActionHeld(Action::MoveBackward));
    const bool moving = (fwd != 0 || side != 0);
    const bool crouching = input.keyboard[Keyboard::Ctrl].held;
    const bool sprinting = input.keyboard[Keyboard::Shift].held && !crouching;

    const glm::vec3 front_xz = ProjectXZ(cam.forward);
    const glm::vec3 right_xz = ProjectXZ(cam.right);
    const glm::vec3 desired  = static_cast<f32>(side) * right_xz + static_cast<f32>(fwd) * front_xz;

    // 4. Smoothing & Horizontal Velocity
    const f32 k = std::clamp(CONTROL * deltaTime, 0.f, 1.f);
    desireDir = glm::mix(desireDir, desired, k);

    glm::vec3 hvel = { velocity.x, 0.f, velocity.z };
    const f32 damping = 1.0f - ((1.0f - (grounded ? tune.friction : tune.airDrag)) * (deltaTime * 60.0f));
    hvel *= std::max(0.0f, damping);

    const f32 maxSpeed = crouching ? tune.crouchSpeed : (sprinting ? tune.maxSpeed * tune.sprintSpeed : tune.maxSpeed);
    const f32 accel = std::clamp(maxSpeed - glm::dot(hvel, desireDir), 0.f, tune.maxAccel * deltaTime);

    hvel += desireDir * accel;
    velocity.x = hvel.x;
    velocity.z = hvel.z;

    // 5. Apply Movement & Simple Collision
    footPos += velocity * deltaTime;
    if (footPos.y <= 0.0f) {
        footPos.y = 0.0f;
        velocity.y = 0.0f;
        grounded = true;
    }

    // 6. Dynamic Eye Height & Bobbing Timer
    const f32 targetEye = crouching ? tune.crouchEye : tune.standEye;
    eyeHeight = std::lerp(eyeHeight, targetEye, k);

    if (moving && grounded) {
        f32 bobSpeed = tune.bobFreq * (sprinting ? 1.4f : 1.0f);
        headTimer = std::fmod(headTimer + deltaTime * bobSpeed, 1.0f);
    } else {
        // Slowly reset timer to 0 when standing still to avoid "frozen" head tilt
        headTimer = std::lerp(headTimer, 0.0f, k);
    }
}