//
// Created by Orgest on 7/12/2025.
//

#pragma once
struct MeshStats
{
	u32 triCount;
	u32 vertexCount;
};

struct SceneStats
{
	u32 drawCallCount = 0;
	u32 totalMeshCount = 0;
	u32 totalVerts = 0;
	u32 totalTris = 0;
	f32 cpuDrawTime = 0.0f;
	f32 gpuDrawTime = 0.0f;
	f32 gpuBusy = 0.0f; // %

	void ResetFrame()
	{
		drawCallCount = 0;
		cpuDrawTime = 0.0f;
		gpuDrawTime = 0.0f;
		gpuBusy = 0.0f;
	}
};
