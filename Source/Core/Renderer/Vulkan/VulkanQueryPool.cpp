//
// Created by Orgest on 8/1/2025.
//

#include "VulkanQueryPool.h"
#include "VulkanInit.h"

#include "VulkanCheck.h"

using namespace Renderer;
bool VulkanQueryPool::Init(VulkanDevice* device, u32 queryCount)
{
	this->device = device;
	this->queryCount = queryCount;

	VkQueryPoolCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		.queryType = VK_QUERY_TYPE_TIMESTAMP,
		.queryCount = queryCount
	};
	VK_CHECK(vkCreateQueryPool(device->device, &createInfo, nullptr, &queryPool));

	// initialize state so first vkGetQueryPoolResults won't trip validation
	vkResetQueryPool(device->device, queryPool, 0, queryCount);
	queryResults.resize(queryCount);
	return true;
}

void VulkanQueryPool::Destroy()
{
	vkDestroyQueryPool(device->device, queryPool, nullptr);
}

void VulkanQueryPool::Reset(VkCommandBuffer cmd) const
{
	vkCmdResetQueryPool(cmd, queryPool, 0, queryCount);
}

void VulkanQueryPool::WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits2 stage, u32 queryIndex) const {
	assert(queryIndex < queryCount);
	vkCmdWriteTimestamp2(cmd, stage, queryPool, queryIndex);
}

bool VulkanQueryPool::FetchResults()
{

	VkResult res = vkGetQueryPoolResults(
		device->device,
		queryPool,
		0,
		queryCount,
		sizeof(TimestampResult) * queryResults.size(),
		queryResults.data(),
		sizeof(TimestampResult),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
	);

	if (res == VK_NOT_READY)
		return false; // GPU not done yet; try next frame (after fence)

	if (res != VK_SUCCESS)
	{
		fprintf(stderr, "vkGetQueryPoolResults failed: 0x%X\n", res);
		return false;
	}
	return true;
}

f32 VulkanQueryPool::DeltaMs(u32 beginIdx, u32 endIdx) const
{
	if (beginIdx >= queryResults.size() || endIdx >= queryResults.size())
		return 0.0f;

	const auto& b = queryResults[beginIdx];
	const auto& e = queryResults[endIdx];

	// Must be available (non-zero per spec when WITH_AVAILABILITY_BIT)
	if (b.available == 0 || e.available == 0 || e.time <= b.time)
		return 0.0f;

	// timestampPeriod is *nanoseconds per tick*
	const f32 periodNs = device->deviceProperties.limits.timestampPeriod;
	const f64 ns = static_cast<f64>(e.time - b.time) * static_cast<f64>(periodNs);

	return static_cast<f32>(ns * 1e-6); // ns -> ms
}

