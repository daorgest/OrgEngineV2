//
// Created by Orgest on 9/19/2025.
//

#pragma once
#include "Camera.h"

struct Input;

#define GRAVITY          28.0f      // feels snappier, not floaty
#define MAX_SPEED        12.0f
#define CROUCH_SPEED      5.0f
#define JUMP_FORCE       10.0f
#define MAX_ACCEL        90.0f
#define FRICTION          0.85f     // retain some sliding
#define AIR_DRAG          0.992f    // minor slow-down in air (~30 % per second)
#define CONTROL           12.0f
#define CROUCH_HEIGHT     0.5f
#define STAND_HEIGHT      1.0f
#define BOTTOM_HEIGHT     0.5f

#define NORMALIZE_INPUT   0


struct FPSCameraTuning
{
	f32 mouseSens   = 0.1f;
	f32 maxSpeed    = MAX_SPEED;       // 20.0f
	f32 crouchSpeed = CROUCH_SPEED;    // 5.0f
	f32 maxAccel    = MAX_ACCEL;       // 150.0f
	f32 friction    = FRICTION;        // 0.86f
	f32 airDrag     = AIR_DRAG;        // 0.98f
	f32 gravity     = GRAVITY;         // 32.0f
	f32 jumpForce   = JUMP_FORCE;      // 12.0f

	f32 sprintSpeed = 1.75f;
	f32 sprintFOV   = 1.1f;

	f32 bobFreq     = 1.5f;
	f32 bobHorizAmp = 0.05f;
	f32 bobVertAmp  = 0.1f;

	f32 crouchEye   = BOTTOM_HEIGHT + CROUCH_HEIGHT;
	f32 standEye    = BOTTOM_HEIGHT + STAND_HEIGHT;

	f32 fovKick     = 5.0f;
	f32 fovLerp     = 5.0f;
};

struct FPSCamera final : Camera
{
	FPSCameraTuning tune{};

	FPSCamera();
	void SyncBodyFromCameraStanding();

	void Update(f32 dt, bool allowMouseLook); // allowMouseLook = can process mouse input

private:
	struct Body
	{
		glm::vec3 pos{0, 0, 0};
		glm::vec3 vel{0, 0, 0};
		glm::vec3 desireDir{0, 0, 1};
		bool grounded{true};
		f32 headTimer{0.0f};
		f32 eyeHeight{1.6f};
		f32 walkLerp{0.0f};
		f32 fovBase{70.0f};
	} body_;

	static glm::vec3 ProjectXZ(glm::vec3 v);
	void IntegrateFPS(f32 dt);
};
