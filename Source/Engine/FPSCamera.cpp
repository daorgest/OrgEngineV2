//
// Created by Orgest on 9/19/2025.
//

#include "FPSCamera.h"
#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtx/norm.hpp>

#include "Camera.h"
#include "Input/InputSys.h"

glm::vec3 FPSCamera::ProjectXZ(const glm::vec3& v)
{
	const glm::vec3 p = {v.x, 0.0f, v.z};
	const f32 len2 = glm::dot(p, p);
	return (len2 > 1e-12f) ? p / std::sqrt(len2) : glm::vec3(0, 0, 1);
}

void FPSCamera::Update(CameraComponent& comp, f32 deltaTime)
{
    Camera& cam = comp.camera;
    glm::vec3& footPos = comp.position;

    // --- 1. Mouse & Controller Look ---
    f32 deltaYaw = -static_cast<f32>(input.xrel) * tune.mouseSens;
    f32 deltaPitch = -static_cast<f32>(input.yrel) * tune.mouseSens;

    if (input.usingController && input.controllers[0].connected)
    {
        const f32 rightX = input.GetRightStickX();
        const f32 rightY = input.GetRightStickY();
        if (rightX != 0.0f || rightY != 0.0f)
        {
            deltaYaw += rightX * 150.0f * deltaTime;
            deltaPitch -= rightY * 150.0f * deltaTime;
        }
    }

    yaw += deltaYaw;
    pitch += deltaPitch;
    pitch = std::clamp(pitch, -89.9f, 89.9f);

    const glm::quat qYaw = glm::angleAxis(Radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qPitch = glm::angleAxis(Radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    cam.rotation = qYaw * qPitch;

    f32 rightVal = static_cast<f32>(i32(input.IsActionHeld(Action::MoveRight)) - i32(
        input.IsActionHeld(Action::MoveLeft)));
    f32 fwdVal = static_cast<f32>(i32(input.IsActionHeld(Action::MoveForward)) - i32(
        input.IsActionHeld(Action::MoveBackward)));

    if (input.usingController && input.controllers[0].connected)
    {
        rightVal += input.GetLeftStickX();
        fwdVal += input.GetLeftStickY();
    }

    glm::vec2 inputMove(rightVal, fwdVal);
    if (glm::length2(inputMove) > 1.0f) inputMove = glm::normalize(inputMove);
    const bool moving = (inputMove.x != 0.0f || inputMove.y != 0.0f);

    // --- 3. The State Machine (Bitmask System) --
    // Update environmental flags dynamically
    if (!grounded) flags |= FPSFlags::Airborne;
    else flags &= ~FPSFlags::Airborne;

    if (moving) flags |= FPSFlags::Moving;
    else flags &= ~FPSFlags::Moving;

    // Crouch Toggle (1 press)
    if (input.IsActionDown(Action::Crouch))
    {
        if (HasAny(flags, FPSFlags::Crouching))
        {
            flags &= ~FPSFlags::Crouching; // Turn OFF
        }
        else
        {
            flags |= FPSFlags::Crouching; // Turn ON
            flags &= ~FPSFlags::Sprinting; // Forcibly kill sprint if we crouch
        }
    }

    // Sprint Trigger
    if (!HasAny(flags, FPSFlags::Crouching) && input.IsActionDown(Action::Sprint) && moving)
    {
        flags |= FPSFlags::Sprinting;
        sprintTimer = tune.sprintDuration;
    }

    // Sprint Timeout & Stop Logic
    if (HasAny(flags, FPSFlags::Sprinting))
    {
        sprintTimer -= deltaTime;
        if (!moving || sprintTimer <= 0.0f)
        {
            flags &= ~FPSFlags::Sprinting; // Drop out of sprint if stopped or tired
        }
    }

    // --- 4. Vertical Physics (Jumping) ---
    if (!grounded) velocity.y -= tune.gravity * deltaTime;

    if (grounded && input.IsActionDown(Action::Jump)) {
        velocity.y = tune.jumpForce;
        grounded = false;
        flags |= FPSFlags::Airborne;
        flags &= ~FPSFlags::Crouching; // Instantly leave crouch on jump
    }

    // --- 5. Resolve State back to variables for physics ---
    const bool crouching = HasAny(flags, FPSFlags::Crouching);
    const bool sprinting = HasAny(flags, FPSFlags::Sprinting);

    const glm::vec3 front_xz = ProjectXZ(cam.GetForward());
    const glm::vec3 right_xz = ProjectXZ(cam.GetRight());
    const glm::vec3 desired = inputMove.x * right_xz + inputMove.y * front_xz;

    // --- 6. Smoothing & Horizontal Velocity ---
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

    footPos += velocity * deltaTime;
    if (footPos.y <= 0.0f) {
        footPos.y = 0.0f;
        velocity.y = 0.0f;
        grounded = true;
    }

    const f32 targetEye = crouching ? tune.crouchEye : tune.standEye;
    eyeHeight = std::lerp(eyeHeight, targetEye, k);

    if (moving && grounded) {
        f32 bobSpeed = tune.bobFreq * (sprinting ? 1.4f : 1.0f);
        headTimer = std::fmod(headTimer + deltaTime * bobSpeed, 1.0f);
        walkLerp = std::lerp(walkLerp, 1.0f, k * 1.5f);
    } else {
        walkLerp = std::lerp(walkLerp, 0.0f, k * 1.5f);
    }
}