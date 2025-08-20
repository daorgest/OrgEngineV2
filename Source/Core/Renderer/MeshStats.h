//
// Created by Orgest on 7/12/2025.
//

#pragma once

#include "PrimTypes.h"

struct MeshStats
{
	u32 triCount;
	u32 vertexCount;
	f32 cpuDrawTime;
	f32 gpuDrawTime;
};

struct SceneStats
{
	u32 drawCallCount = 0;
	f32 cpuDrawTime   = 0.0f;
	f32 gpuDrawTime   = 0.0f;
};
