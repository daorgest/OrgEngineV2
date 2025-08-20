//
// Created by Orgest on 6/27/2025.
//

#include "VulkanRenderPass.h"

#include "RendererTypes.h"
#include "volk.h"

using namespace Renderer;

void VulkanRenderPass::Begin(VkCommandBuffer cmd, VkAttachmentLoadOp loadOp)
{
	colorAttachmentInfos.clear();
	colorAttachmentInfos.reserve(colorAttachments.size());

	for (size_t i = 0; i < colorAttachments.size(); ++i) {
		VkClearValue clear{};
		if (i < clearValues.size()) clear = clearValues[i];
		else                        clear.color = {{0.f, 0.f, 0.f, 1.f}};

		VkRenderingAttachmentInfo colorAtt{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = colorAttachments[i],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp      = loadOp,
			.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue  = clear
		};
		colorAttachmentInfos.emplace_back(colorAtt);
	}

	if (depthStencilAttachment) {
		VkClearValue depthClear{};
		if (clearValues.size() > colorAttachments.size())
			depthClear = clearValues[colorAttachments.size()];
		else
			depthClear.depthStencil = {0.0f, 0}; // reverse-Z

		depthAttachmentInfo = VkRenderingAttachmentInfo{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = *depthStencilAttachment,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp      = loadOp,
			.storeOp     = VK_ATTACHMENT_STORE_OP_STORE, // keep it
			.clearValue  = depthClear
		};
	} else {
		depthAttachmentInfo.reset();
	}

	VkRenderingInfo ri{
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea           = renderArea,
		.layerCount           = 1,
		.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size()),
		.pColorAttachments    = colorAttachmentInfos.data(),
		.pDepthAttachment     = depthAttachmentInfo ? &*depthAttachmentInfo : nullptr
	};

	vkCmdBeginRendering(cmd, &ri);
}

void VulkanRenderPass::Begin(VkCommandBuffer cmd, const Extent2D& size, VkImageView targetView, bool clear)
{
	renderArea = { {0, 0}, {size.width, size.height} };
	colorAttachments.clear();
	colorAttachments.emplace_back(targetView);
	if (clear) {
		clearValues.clear();
		clearValues.push_back(VkClearValue{ .color = {{0.f, 0.f, 0.f, 1.f}} }); // default color
		if (depthStencilAttachment) {
			clearValues.push_back(VkClearValue{ .depthStencil = {0.0f, 0} });   // reverse-Z depth
		}
	}

	const VkAttachmentLoadOp op = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
										: VK_ATTACHMENT_LOAD_OP_LOAD;

	assert(colorAttachments.size() <= 8 && "Too many color attachments!");
	Begin(cmd, op);
}

void VulkanRenderPass::End(VkCommandBuffer commandBuffer)
{
	vkCmdEndRendering(commandBuffer);
	depthStencilAttachment.reset();
}
