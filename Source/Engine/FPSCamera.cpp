//
// Created by Orgest on 9/19/2025.
//

#include "FPSCamera.h"
#include <algorithm>
#include <cmath>

#include "imgui.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/ext/scalar_constants.hpp"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"

FPSCamera::FPSCamera()
{
	body_.fovBase = fov;
	SyncBodyFromCameraStanding();
}

void FPSCamera::SyncBodyFromCameraStanding()
{
	body_.pos = position;
	body_.pos.y = std::max(0.0f, body_.pos.y - tune.standEye);
	body_.vel = glm::vec3(0);
	body_.grounded = (body_.pos.y <= 0.0f + 1e-4f);
	body_.eyeHeight = tune.standEye;
	body_.fovBase = fov;
}

glm::vec3 FPSCamera::ProjectXZ(glm::vec3 v)
{
	const glm::vec3 p{v.x, 0.0f, v.z};
	f32 len2 = glm::dot(p, p);
	return (len2 > 1e-12f) ? p / std::sqrt(len2) : glm::vec3(0, 0, 1);
}

void FPSCamera::Update(f32 dt, bool allowMouseLook)
{
	// Look - only process mouse input if allowed (Application handles ImGui/Alt checks)
	if (allowMouseLook)
	{
		yaw -= input.xrel * tune.mouseSens;
		pitch -= input.yrel * tune.mouseSens;
		pitch = std::clamp(pitch, -89.9f, 89.9f);  // Prevent gimbal lock flip
		UpdateDirectionVectors();
	}

	// move
	IntegrateFPS(dt);
}

void FPSCamera::IntegrateFPS(f32 dt)
{
    // ----- Gravity (air only) -----
    if (!body_.grounded) body_.vel.y -= tune.gravity * dt;

    // ----- Jump -----
    if (body_.grounded && input.keyboard[Keyboard::Space].pressed) {
        body_.vel.y    = tune.jumpForce;
        body_.grounded = false;
    }

    // ----- Input (A/D, W/S) -----
	const int side = int(input.keyboard[Keyboard::D].held) - int(input.keyboard[Keyboard::A].held);
	const int fwd  = int(input.keyboard[Keyboard::W].held) - int(input.keyboard[Keyboard::S].held);
	const bool crouching = input.keyboard[Keyboard::Ctrl].held;
	const bool sprinting = input.keyboard[Keyboard::Shift].held && !crouching;

	// ----- Use camera's actual direction vectors (projected to XZ plane) -----
	// Project forward and right onto XZ plane (ignore pitch for movement)
	const glm::vec3 front = ProjectXZ(forward);  // Camera's forward, flattened
	const glm::vec3 right_vec = ProjectXZ(right);   // Camera's right, flattened

#if NORMALIZE_INPUT
    // Slow down diagonal movement (optional)

	glm::vec2 in{ (f32)side, (f32)(fwd) };
    if (in.x != 0.f || in.y != 0.f) {
        const f32 len = std::sqrt(in.x*in.x + in.y*in.y);
        in /= len;
    }
#endif
	glm::vec3 desired = (f32)side * right_vec + (f32)fwd * front;

    // ----- Intent smoothing EXACT style: lerp with CONTROL*dt (not exponential) -----
    const f32 k = std::clamp(CONTROL * dt, 0.f, 1.f);
    body_.desireDir = glm::mix(body_.desireDir, desired, k);
    // keep horizontal intent only
    body_.desireDir.y = 0.f;

    // ----- Horizontal velocity with frame-rate independent damping -----
    glm::vec3 hvel{ body_.vel.x, 0.f, body_.vel.z };
    const f32 decel = body_.grounded ? tune.friction : tune.airDrag;
    const float dampingFactor = 1.0f - ((1.0f - decel) * (dt * 60.0f));
    hvel *= std::max(0.0f, dampingFactor);

    if (glm::length(hvel) < (tune.maxSpeed * 0.01f)) hvel = glm::vec3(0);

    // ----- Acceleration toward intent (raylib style) -----
	float maxSpeed = crouching ? tune.crouchSpeed
				 : sprinting ? tune.maxSpeed * tune.sprintSpeed
				 : tune.maxSpeed;


	float accelScale = sprinting ? 1.5f : 1.0f;
	const float speedAlong = glm::dot(hvel, body_.desireDir);
	const float accel = std::clamp(maxSpeed - speedAlong, 0.f, tune.maxAccel * accelScale * dt);

    hvel += body_.desireDir * accel;

    // Commit horizontal
    body_.vel.x = hvel.x;
    body_.vel.z = hvel.z;

    // ----- Integrate position -----
    body_.pos += body_.vel * dt;

    // ----- Ground plane -----
    if (body_.pos.y <= 0.0f) {
        body_.pos.y    = 0.0f;
        body_.vel.y    = 0.0f;
        body_.grounded = true;
    }

    // ----- Eye height -----
    {
        const float targetEye = crouching ? tune.crouchEye : tune.standEye;
        const float kEye = std::clamp(CONTROL * dt, 0.f, 1.f);
        body_.eyeHeight = std::lerp(body_.eyeHeight, targetEye, kEye);
    }


	// ----- Head-bob & FOV kick -----
	{
    	const bool moving = (fwd || side) && body_.grounded;
    	const float fovK = std::clamp(tune.fovLerp * dt, 0.f, 1.f);
    	const float bobK = std::clamp(CONTROL * dt, 0.f, 1.f);

    	// --- FOV logic ---
    	float targetFov = body_.fovBase;
    	if (sprinting)
    		targetFov = body_.fovBase * tune.sprintFOV;     // widen FOV while sprinting
    	else if (moving)
    		targetFov = body_.fovBase - tune.fovKick;       // slight inward FOV when walking
    	fov = std::lerp(fov, targetFov, fovK);

    	// --- Head-bob timing & amplitude ---
    	if (moving)
    	{
    		// accelerate bob when sprinting
    		float bobSpeed = tune.bobFreq * (sprinting ? 1.6f : 1.0f);
    		body_.headTimer += dt * bobSpeed;
    		if (body_.headTimer > 1.0f)
    			body_.headTimer -= 1.0f; // wrap nicely

    		body_.walkLerp = std::lerp(body_.walkLerp, 1.0f, bobK);
    	}
    	else
    	{
    		body_.walkLerp = std::lerp(body_.walkLerp, 0.0f, bobK);
    	}

    	// --- Compute bob offset ---
    	const float s = std::sin(body_.headTimer * glm::pi<float>() * 2.0f);
    	const float c = std::cos(body_.headTimer * glm::pi<float>() * 2.0f);

    	float horizAmp = tune.bobHorizAmp * (sprinting ? 1.5f : 1.0f);
    	float vertAmp  = tune.bobVertAmp  * (sprinting ? 1.5f : 1.0f);

    	glm::vec3 bob = ProjectXZ(right) * (s * horizAmp * 2.0f);
    	bob.y = std::abs(c * vertAmp * 1.5f);
    	bob *= body_.walkLerp;

    	position = body_.pos + glm::vec3(0, body_.eyeHeight, 0) + bob;
	}
}

