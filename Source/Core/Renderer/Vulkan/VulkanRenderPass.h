//
// Created by Orgest on 6/27/2025.
//

#pragma once
#include <optional>
#include <span>

#include "VulkanTexture.h"
#include "Tools/Vector.h"

struct Extent2D;

namespace Renderer
{
	struct RenderPassDesc
	{
		std::span<const VulkanTexture> renderPasses;
		const VulkanTexture* depthTexture = nullptr;
	};

	struct VulkanRenderPass
	{
		Vector<VkRenderingAttachmentInfo> colorAttachments;

		void Begin(VkCommandBuffer cmd, const Extent2D& size, const RenderPassDesc& renderInfo, bool clear = true);
		void End(VkCommandBuffer commandBuffer);
	};
}
