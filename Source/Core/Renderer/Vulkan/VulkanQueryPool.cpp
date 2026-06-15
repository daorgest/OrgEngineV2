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
    timestampPeriod = device->deviceProperties.properties.limits.timestampPeriod;
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
    vkCmdWriteTimestamp2(vkCmd->GetVkHandle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, queryPool, queryIndex);
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

    for (auto& [time, available] : queryResults)
    {
        if (available == 0) continue;
        time = static_cast<u64>(static_cast<f64>(time) * timestampPeriod);
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