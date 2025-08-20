//
// Created by Orgest on 6/27/2025.
//

#pragma once
#include <optional>
#include <vulkan/vulkan_core.h>

#include "Vector.h"
#include "VulkanTexture.h"

struct Extent2D;

namespace Renderer
{
	struct VulkanRenderPass
	{
		Vector<VkImageView> colorAttachments;
		Vector<VkRenderingAttachmentInfo> colorAttachmentInfos;

		std::optional<VkImageView> depthStencilAttachment;
		std::optional<VkRenderingAttachmentInfo> depthAttachmentInfo;

		Vector<VkClearValue> clearValues;
		VkRect2D renderArea;

		void Begin(VkCommandBuffer commandBuffer, VkAttachmentLoadOp loadOp);
		void Begin(VkCommandBuffer cmd, const Extent2D& size, VkImageView targetView, bool clear = true);
		void End(VkCommandBuffer commandBuffer);
	};
}
