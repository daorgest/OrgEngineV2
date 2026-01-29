//
// Created by Orgest on 8/1/2025.
//

#pragma once
#include <volk.h>

#include "RenderInterface.h"
#include "Tools/Vector.h"

namespace Renderer
{
    struct VulkanDevice;

    struct VulkanQueryPool final : GPUQueryPool
    {
        struct TimestampResult
        {
            u64 time;
            u64 available;
        };

        bool Init(VulkanDevice* inDevice, u32 inQueryCount);
        ~VulkanQueryPool() override { Destroy(); }
        void Destroy();

        void Reset(GPUCommandBuffer* cmd) override;
        void WriteTimestamp(GPUCommandBuffer* cmd, u32 queryIndex) override;

        bool FetchResults() override;
        f32 GetElapsedMs(u32 timerIndex) const;
        [[nodiscard]] f32 GetDeltaMs(u32 beginIdx, u32 endIdx) const override;

    private:
        VulkanDevice* device = nullptr;
        VkQueryPool queryPool = VK_NULL_HANDLE;

        f32 timestampPeriod = 0.0f;
        u32 queryCount = 0;
        Vector<TimestampResult> queryResults;
    };
}
