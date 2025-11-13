//
// Created by Orgest on 6/27/2025.
//

#include "VulkanRenderPass.h"

#include "RendererTypes.h"
#include "volk.h"
#include "VulkanConvert.h"

using namespace Renderer;

void VulkanRenderPass::Begin(VkCommandBuffer cmd, const Extent2D& size, const RenderPassDesc& renderInfo, const bool clear)
{
	colorInfos.clear();
	colorInfos.reserve(renderInfo.renderPasses.size());
	depthInfo.reset();

	for (const auto& renderPass : renderInfo.renderPasses)
	{
		assert(renderPass.imageView && "Color Imageview needs to be set");

		colorInfos.emplace_back(VkRenderingAttachmentInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = renderPass.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {0.0f, 0.0f, 0.0f, 1.0f} // black
		});
	}

	if (renderInfo.depthTexture)
	{
		assert(renderInfo.depthTexture->imageView && "Depth ImageView must be set");
		depthInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = renderInfo.depthTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.depthStencil = {1.0f, 0}} // Standard depth: clear to 1.0 (far plane)
		};
	}

	const VkRenderingInfo ri = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {.offset = {0, 0}, .extent = {size.width, size.height}},
		.layerCount = 1,
		.colorAttachmentCount = static_cast<u32>(colorInfos.size()),
		.pColorAttachments = colorInfos.empty() ? nullptr : colorInfos.data(),
		.pDepthAttachment = depthInfo ? &*depthInfo : nullptr,
		.pStencilAttachment = nullptr
	};

	vkCmdBeginRendering(cmd, &ri);
}

void VulkanRenderPass::End(VkCommandBuffer cmd)
{
	vkCmdEndRendering(cmd);
}
