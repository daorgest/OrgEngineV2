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

void VulkanQueryPool::WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits2 stage, uint32_t queryIndex) const {
	assert(queryIndex < queryCount);
	vkCmdWriteTimestamp2(cmd, stage, queryPool, queryIndex);
}

void VulkanQueryPool::FetchResults()
{
	queryResults.resize(queryCount);

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

	if (res != VK_SUCCESS && res != VK_NOT_READY)
	{
		fprintf(stderr, "vkGetQueryPoolResults failed with error code 0x%X.\n", res);
		return;
	}

	// Conversion sourced from Godot Engine's Vulkan rendering driver.
	auto mult64to128 = [](u64 u, u64 v, u64 &h, u64 &l) {
		u64 u1 = (u & 0xffffffff);
		u64 v1 = (v & 0xffffffff);
		u64 t = (u1 * v1);
		u64 w3 = (t & 0xffffffff);
		u64 k = (t >> 32);

		u >>= 32;
		t = (u * v1) + k;
		k = (t & 0xffffffff);
		u64 w1 = (t >> 32);

		v >>= 32;
		t = (u1 * v) + k;
		k = (t >> 32);

		h = (u * v) + w1 + k;
		l = (t << 32) + w3;
	};

	constexpr u64 shift_bits = 16;
	f64 timestampPeriod = f64(device->deviceProperties.limits.timestampPeriod);
	u64 scale = u64(timestampPeriod * f64(1 << shift_bits));

	// Convert all timestamps using the trick
	for (auto& [time, available] : queryResults)
	{
		u64 hi = 0;
		u64 lo = 0;
		mult64to128(time, scale, hi, lo);
		time = (lo >> shift_bits) | (hi << (64 - shift_bits));
	}

}
