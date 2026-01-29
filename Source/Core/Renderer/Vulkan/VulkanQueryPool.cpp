//
// Created by Orgest on 8/1/2025.
//
#include "VulkanQueryPool.h"

#include "VulkanCheck.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"

using namespace Renderer;


bool VulkanQueryPool::Init(VulkanDevice* inDevice, const u32 inQueryCount)
{
    device = inDevice;
    queryCount = inQueryCount;


    const VkQueryPoolCreateInfo createInfo = {
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = inQueryCount
    };

    VK_CHECK(vkCreateQueryPool(inDevice->device, &createInfo, nullptr, &queryPool));

    // Capture the nanosecond period for timing calculations
    timestampPeriod = device->deviceProperties.limits.timestampPeriod;
    queryResults.resize(inQueryCount);

    return true;
}

void VulkanQueryPool::Destroy()
{
    if (queryPool) {
        vkDestroyQueryPool(device->device, queryPool, nullptr);
        queryPool = VK_NULL_HANDLE;
    }
}

void VulkanQueryPool::Reset(GPUCommandBuffer* cmd)
{
    const auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
    vkCmdResetQueryPool(vkCmd->GetVkHandle(), queryPool, 0, queryCount);
}

void VulkanQueryPool::WriteTimestamp(GPUCommandBuffer* cmd, const u32 queryIndex)
{
    const auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
    vkCmdWriteTimestamp2(vkCmd->GetVkHandle(), VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, queryPool, queryIndex);
}

bool VulkanQueryPool::FetchResults()
{
    VkResult result = vkGetQueryPoolResults(
        device->device,
        queryPool,
        0,
        queryCount,
        queryResults.size() * sizeof(TimestampResult),
        queryResults.data(),
        sizeof(TimestampResult),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
    );

    if (result != VK_SUCCESS) return false;


    // --- Godot-style Fixed Point Conversion Trick ---
    constexpr uint64_t shift_bits = 16;
    // We treat the period as a fixed-point f64 scaled by 2^16
    const f64 scaledPeriod = static_cast<f64>(timestampPeriod) * static_cast<f64>(1LL << shift_bits);

    for (auto& res : queryResults) {
        if (res.available == 0) continue;

        uint64_t h = 0, l = 0;

        // Lambda for 64x64 to 128-bit multiplication
        auto mult64to128 = [](uint64_t u, uint64_t v, uint64_t &hIn, uint64_t &lIn) {
            const uint64_t u1 = (u & 0xffffffff);
            const uint64_t v1 = (v & 0xffffffff);
            uint64_t t = (u1 * v1);
            const uint64_t w3 = (t & 0xffffffff);
            uint64_t k = (t >> 32);

            u >>= 32;
            t = (u * v1) + k;
            k = (t & 0xffffffff);
            const uint64_t w1 = (t >> 32);

            v >>= 32;
            t = (u1 * v) + k;
            k = (t >> 32);

            hIn = (u * v) + w1 + k;
            lIn = (t << 32) + w3;
        };

        mult64to128(res.time, static_cast<uint64_t>(scaledPeriod), h, l);

        // Reassemble the 128-bit result and shift back down
        uint64_t finalTime = l >> shift_bits;
        finalTime |= (h << (64 - shift_bits));

        res.time = finalTime; // res.time is now in nanoseconds
    }

    return true;
}

f32 VulkanQueryPool::GetElapsedMs(u32 timerIndex) const
{
    const u32 start = timerIndex * 2;
    const u32 end   = start + 1;
    return GetDeltaMs(start, end);
}

f32 VulkanQueryPool::GetDeltaMs(u32 beginIdx, u32 endIdx) const
{
    // Ensure GPU has finished writing both timestamps before calculating
    if (queryResults[beginIdx].available == 0 || queryResults[endIdx].available == 0)
    {
        return 0.0f;
    }

    const u64 start = queryResults[beginIdx].time;
    const u64 end   = queryResults[endIdx].time;

    if (end < start) return 0.0f;

    // Logic: (Delta Ticks * NanoSecPerTick) / 1,000,000.0f
    return static_cast<f32>(end - start) * 1e-6f;
}