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

void FPSCamera::Update(f32 dt)
{
	// look
	if (!ImGui::GetIO().WantCaptureMouse)
	{
		yaw -= input.xrel * tune.mouseSens;
		pitch -= input.yrel * tune.mouseSens;
		UpdateDirectionVectors();
	}

	// move
	IntegrateFPS(dt);
}

void FPSCamera::IntegrateFPS(float dt)
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

	// ----- Basis EXACT like raylib sample -----
	// front = { sin(rot), 0,  cos(rot) }
	// right = { cos(rot), 0, -sin(rot) }
	const float rot = yaw;
	const glm::vec3 front = { std::sin(rot), 0.f, std::cos(rot) };
	const glm::vec3 right = { std::cos(rot), 0.f, -std::sin(rot) };



    glm::vec2 in{ (float)side, (float)(fwd) };

#if NORMALIZE_INPUT
    // Slow down diagonal movement (optional)
    if (in.x != 0.f || in.y != 0.f) {
        const float len = std::sqrt(in.x*in.x + in.y*in.y);
        in /= len;
    }
#endif
	glm::vec3 desired = (float)side * right + (float)fwd * front;


    // ----- Intent smoothing EXACT style: lerp with CONTROL*dt (not exponential) -----
    const float k = std::clamp(CONTROL * dt, 0.f, 1.f);
    body_.desireDir = glm::mix(body_.desireDir, desired, k);
    // keep horizontal intent only
    body_.desireDir.y = 0.f;

    // ----- Horizontal velocity with multiplicative damping (raylib uses multipliers) -----
    // NOTE: tune.friction and tune.airDrag are MULTIPLIERS here (e.g., 0.86f, 0.98f)
    glm::vec3 hvel{ body_.vel.x, 0.f, body_.vel.z };
    const float decel = body_.grounded ? tune.friction : tune.airDrag;
    hvel *= decel;

    // tiny cutoff (same idea as sample)
    if (glm::length(hvel) < (tune.maxSpeed * 0.01f)) hvel = glm::vec3(0);

    // ----- Acceleration toward intent (raylib style) -----
    float maxSpeed = crouching ? tune.crouchSpeed : tune.maxSpeed;
    const float speedAlong = glm::dot(hvel, body_.desireDir);
    const float accel      = std::clamp(maxSpeed - speedAlong, 0.f, tune.maxAccel * dt);
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

    // ----- Eye height (keep your existing smoothing) -----
    {
        const float targetEye = crouching ? tune.crouchEye : tune.standEye;
        const float kEye = std::clamp(CONTROL * dt, 0.f, 1.f);
        body_.eyeHeight = std::lerp(body_.eyeHeight, targetEye, kEye);
    }

    // ----- Head-bob & FOV kick (you already have this; leaving as-is) -----
    const bool moving = (fwd || side) && body_.grounded;
    {
        const float bobK = std::clamp(CONTROL * dt, 0.f, 1.f);
        const float fovK = std::clamp(tune.fovLerp * dt, 0.f, 1.f);

        if (moving) {
            body_.headTimer += dt * tune.bobFreq;
            body_.walkLerp   = std::lerp(body_.walkLerp, 1.0f, bobK);
            fov              = std::lerp(fov, body_.fovBase - 5.0f, fovK); // raylib sample pulls FOV from 60 -> 55
        } else {
            body_.walkLerp   = std::lerp(body_.walkLerp, 0.0f, bobK);
            fov              = std::lerp(fov, body_.fovBase, fovK);        // back to base (60)
        }
    }

    // ----- Head bob offset (mirrors sample’s sine/cos use & lean feel) -----
    {
        const float s = std::sin(body_.headTimer * glm::pi<float>());
        const float c = std::cos(body_.headTimer * glm::pi<float>());
        glm::vec3 bob = right * (s * tune.bobHorizAmp * 2.0f); // sample uses ~0.1 side; your default is 0.05 -> *2
        bob.y = std::abs(c * tune.bobVertAmp * 1.5f);          // sample ~0.15 up; your default is 0.1 -> *1.5
        bob  *= body_.walkLerp;

        position = body_.pos + glm::vec3(0, body_.eyeHeight, 0) + bob;
    }
}

