//
// Created by Orgest on 6/27/2025.
//

#include "VulkanRenderPass.h"

#include "RendererTypes.h"
#include "volk.h"
#include "VulkanConvert.h"
#include "VulkanDevice.h"

using namespace Renderer;

void VulkanRenderPass::Begin(VkCommandBuffer cmd, const Extent2D& size, const RenderPassDesc& renderInfo,
                             const bool clear)
{
	colorAttachments.clear();
	colorAttachments.reserve(renderInfo.renderPasses.size());

	bool useUnified = false;
	if (!renderInfo.renderPasses.empty())
	{
		useUnified = renderInfo.renderPasses[0].device->useUnifiedLayout;
	}
	else if (renderInfo.depthTexture)
	{
		useUnified = renderInfo.depthTexture->device->useUnifiedLayout;
	}

	const VkImageLayout colorLayout = useUnified ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	const VkImageLayout depthLayout = useUnified ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	if (useUnified)
	{
		VkMemoryBarrier2 barrier = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		};

		VkDependencyInfo dep = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &barrier
		};
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	for (const auto& renderPass : renderInfo.renderPasses)
	{
		assert(renderPass.imageView && "Color Imageview needs to be set");

		colorAttachments.emplace_back(VkRenderingAttachmentInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = renderPass.imageView,
			.imageLayout = colorLayout,
			.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {0.0f, 0.0f, 0.0f, 1.0f}
		});
	}

	VkRenderingAttachmentInfo depthInfo = {};
	if (renderInfo.depthTexture)
	{
		assert(renderInfo.depthTexture->imageView && "Depth ImageView must be set");
		depthInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = renderInfo.depthTexture->imageView,
			.imageLayout = depthLayout,
			.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.depthStencil = {1.0f, 0}}
		};
	}

	VkRenderingInfo ri = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {.offset = {0, 0}, .extent = {size.width, size.height}},
		.layerCount = 1,
		.colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
		.pColorAttachments = colorAttachments.empty() ? nullptr : colorAttachments.data(),
		.pDepthAttachment = renderInfo.depthTexture ? &depthInfo : nullptr
	};

	vkCmdBeginRendering(cmd, &ri);
}

void VulkanRenderPass::End(VkCommandBuffer cmd)
{
	vkCmdEndRendering(cmd);
}
