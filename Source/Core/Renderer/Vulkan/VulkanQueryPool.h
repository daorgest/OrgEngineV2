//
// Created by Orgest on 8/1/2025.
//

#pragma once
#include <volk.h>
#include "Vector.h"

namespace Renderer
{
	struct VulkanDevice;
	struct VulkanQueryPool
	{

		struct TimestampResult {
			u64 time;
			u64 available;
		};

		bool Init(VulkanDevice* device, u32 queryCount);
		void Destroy();
		void Reset(VkCommandBuffer cmd) const;
		void WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits2 stage, uint32_t queryIndex) const;
		void FetchResults();

		VulkanDevice* device = nullptr;
		VkQueryPool queryPool = VK_NULL_HANDLE;

		u32 queryCount = 0;
		Vector<TimestampResult> queryResults;
	};

}