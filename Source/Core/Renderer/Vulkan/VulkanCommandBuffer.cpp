//
// Created by Orgest on 11/4/2025.
//

#include "VulkanCommandBuffer.h"

#include <algorithm>

#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanDescriptors.h"
#include "VulkanSwapchain.h"
#include "VulkanDebugUtils.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "Tools/Logger.h"

using namespace Renderer;

// VulkanCommandBuffer Implementation

void VulkanCommandBuffer::InitInternal(VulkanDevice* dev, VkCommandPool pool, bool secondary)
{
	device = dev;
	parentPool = pool;
	isSecondary = secondary;

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VK_CHECK(vkAllocateCommandBuffers(device->device, &allocInfo, &cmd));
}

void VulkanCommandBuffer::DestroyInternal()
{
	if (cmd != VK_NULL_HANDLE && parentPool != VK_NULL_HANDLE && device != nullptr)
	{
		vkFreeCommandBuffers(device->device, parentPool, 1, &cmd);
		cmd = VK_NULL_HANDLE;
	}
}

void VulkanCommandBuffer::Begin(const CommandBufferBeginInfo* inheritanceInfo)
{
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0
	};

	if (inheritanceInfo)
	{
		if (inheritanceInfo->oneTimeSubmit)
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (inheritanceInfo->isSecondary)
		{
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			// TODO: Set inheritance info if provided
		}
	}
	else
	{
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	// Tracy profiling
	if (tracyCtx && !isSecondary)
	{
		CmdBeginLabel(cmd, "Frame CommandBuffer", 0.0f, 0.7f, 1.0f);
	}
}

void VulkanCommandBuffer::End()
{
	if (tracyCtx && !isSecondary)
	{
		CmdEndLabel(cmd);
	}

	VK_CHECK(vkEndCommandBuffer(cmd));
}

void VulkanCommandBuffer::Reset()
{
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
}

void VulkanCommandBuffer::BeginRendering(const RenderingInfo& info)
{
	Vector<VkRenderingAttachmentInfo> colorAttachments;
	colorAttachments.reserve(info.colorAttachments.size());

	for (const auto& attachment : info.colorAttachments)
	{
		const auto* vkTexture = static_cast<VulkanImage*>(attachment.texture);

		VkClearValue clearValue;
		clearValue.color = {
			attachment.clearColor[0], attachment.clearColor[1],
			attachment.clearColor[2], attachment.clearColor[3]
		};

		VkRenderingAttachmentInfo vkAttachment = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = vkTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp      = ConvertLoadOp(attachment.loadOp),
			.storeOp     = ConvertStoreOp(attachment.storeOp),
			.clearValue  = clearValue
		};

		colorAttachments.push_back(vkAttachment);
	}

	VkRenderingAttachmentInfo depthAttachment = {};
	if (info.depthAttachment)
	{
		auto* vkDepth = static_cast<VulkanImage*>(info.depthAttachment->texture);

		VkClearValue clearValue;
		clearValue.depthStencil = {info.depthAttachment->clearDepth, info.depthAttachment->clearStencil};

		depthAttachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = vkDepth->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = ConvertLoadOp(info.depthAttachment->loadOp),
			.storeOp = ConvertStoreOp(info.depthAttachment->storeOp),
			.clearValue = clearValue
		};
	}

	VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {.offset = {0, 0}, .extent = {info.width, info.height}},
		.layerCount = 1,
		.colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
		.pColorAttachments = colorAttachments.data(),
		.pDepthAttachment = info.depthAttachment ? &depthAttachment : nullptr
	};

	vkCmdBeginRendering(cmd, &renderInfo);
}

void VulkanCommandBuffer::EndRendering()
{
	vkCmdEndRendering(cmd);
}

void VulkanCommandBuffer::BindPipeline(GPUPipeline* pipeline)
{
	auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->vk);
}

void VulkanCommandBuffer::BindDescriptorSet(DescriptorSet* set, u32 setIndex, GPUPipeline* pipeline)
{
	auto* vkSet = set;
	auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);

	VkDescriptorSet vkHandle = vkSet->vk;
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->vkLayout,
	                       setIndex, 1, &vkHandle, 0, nullptr);
}

void VulkanCommandBuffer::PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size, const void* data)
{
	const auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
	VkShaderStageFlags vkStages = ConvertShaderStageFlags(stages);
	vkCmdPushConstants(cmd, vkPipeline->vkLayout, vkStages, offset, size, data);
}

void VulkanCommandBuffer::BindVertexBuffer(GPUBuffer* buffer, u32 binding, u64 offset)
{
	auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	VkDeviceSize vkOffset = offset;
	vkCmdBindVertexBuffers(cmd, binding, 1, &vkBuffer->buffer, &vkOffset);
}

void VulkanCommandBuffer::BindIndexBuffer(GPUBuffer* buffer, u64 offset)
{
	auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdBindIndexBuffer(cmd, vkBuffer->buffer, offset, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandBuffer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance)
{
	vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandBuffer::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance)
{
	vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanCommandBuffer::DrawIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride)
{
	auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdDrawIndirect(cmd, vkBuffer->buffer, offset, drawCount, stride);
}

void VulkanCommandBuffer::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ)
{
	vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}

void VulkanCommandBuffer::DispatchIndirect(GPUBuffer* buffer, u64 offset)
{
	auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdDispatchIndirect(cmd, vkBuffer->buffer, offset);
}

void VulkanCommandBuffer::PipelineBarrier(const BarrierInfo& info)
{
	// TODO: Implement proper barrier conversion
	// For now, simple memory barrier
	VkMemoryBarrier2 memBarrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
	};

	VkDependencyInfo depInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &memBarrier
	};

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void VulkanCommandBuffer::ExecuteCommands(std::span<GPUCommandBuffer*> secondaryBuffers)
{
	if (isSecondary)
	{
		LOG(Error, "Cannot execute commands from a secondary command buffer");
		return;
	}

	Vector<VkCommandBuffer> vkCmds;
	vkCmds.reserve(secondaryBuffers.size());

	for (auto* buffer : secondaryBuffers)
	{
		auto* vkBuffer = static_cast<VulkanCommandBuffer*>(buffer);
		vkCmds.push_back(vkBuffer->GetVkHandle());
	}

	vkCmdExecuteCommands(cmd, static_cast<u32>(vkCmds.size()), vkCmds.data());
}

void VulkanCommandBuffer::SetViewport(const Viewport& viewport)
{
	VkViewport vkViewport = {
		.x = viewport.x,
		.y = viewport.y,
		.width = viewport.width,
		.height = viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth
	};
	vkCmdSetViewport(cmd, 0, 1, &vkViewport);
}

void VulkanCommandBuffer::SetScissor(u32 x, u32 y, u32 width, u32 height)
{
	VkRect2D scissor = {
		.offset = {static_cast<i32>(x), static_cast<i32>(y)},
		.extent = {width, height}
	};
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanCommandBuffer::CopyBuffer(GPUBuffer* src, GPUBuffer* dst, u64 size, u64 srcOffset, u64 dstOffset)
{
	auto* vkSrc = static_cast<VulkanBuffer*>(src);
	auto* vkDst = static_cast<VulkanBuffer*>(dst);

	VkBufferCopy2 copyRegion = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size
	};

	VkCopyBufferInfo2 copyInfo = {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.srcBuffer = vkSrc->buffer,
		.dstBuffer = vkDst->buffer,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	vkCmdCopyBuffer2(cmd, &copyInfo);
}

void VulkanCommandBuffer::CopyBufferToTexture(GPUBuffer* src, GPUTexture* dst)
{
	auto* vkSrc = static_cast<VulkanBuffer*>(src);
	auto* vkDst = static_cast<VulkanImage*>(dst);

	VkBufferImageCopy2 region = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {vkDst->textureInfo.extent.width, vkDst->textureInfo.extent.height, vkDst->textureInfo.extent.depth}
	};

	VkCopyBufferToImageInfo2 copyInfo = {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.srcBuffer = vkSrc->buffer,
		.dstImage = vkDst->image,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1,
		.pRegions = &region
	};

	vkCmdCopyBufferToImage2(cmd, &copyInfo);
}

void VulkanCommandBuffer::BeginDebugLabel(const char* name, f32 r, f32 g, f32 b)
{
#ifdef VULKAN_DEBUG_MODE
	CmdBeginLabel(cmd, name, r, g, b);
#endif
}

void VulkanCommandBuffer::EndDebugLabel()
{
#ifdef VULKAN_DEBUG_MODE
	CmdEndLabel(cmd);
#endif
}

void VulkanCommandBuffer::InsertDebugLabel(const char* name, f32 r, f32 g, f32 b)
{
#ifdef VULKAN_DEBUG_MODE
	CmdInsertLabel(cmd, name, r, g, b);
#endif
}

// VulkanCommandPool Implementation

void VulkanCommandPool::Init(GPUDevice* dev, u32 queueFamilyIndex, bool transient)
{
	device = static_cast<VulkanDevice*>(dev);

	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamilyIndex
	};

	if (transient)
		poolInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &pool));
}

void VulkanCommandPool::Destroy()
{
	// Free all allocated command buffers
	for (auto* buffer : allocatedBuffers)
	{
		buffer->DestroyInternal();
		delete buffer;
	}
	allocatedBuffers.clear();

	if (pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device->device, pool, nullptr);
		pool = VK_NULL_HANDLE;
	}
}

void VulkanCommandPool::Reset()
{
	VK_CHECK(vkResetCommandPool(device->device, pool, 0));
}

GPUCommandBuffer* VulkanCommandPool::AllocateBuffer(bool secondary)
{
	auto* buffer = new VulkanCommandBuffer();
	buffer->InitInternal(device, pool, secondary);
	allocatedBuffers.push_back(buffer);
	return buffer;
}

void VulkanCommandPool::FreeBuffer(GPUCommandBuffer* buffer)
{
	if (!buffer) return;

	auto* vkBuffer = static_cast<VulkanCommandBuffer*>(buffer);

	// Remove from tracking list
	for (size_t i = 0; i < allocatedBuffers.size(); ++i)
	{
		if (allocatedBuffers[i] == vkBuffer)
		{
			allocatedBuffers.erase(i);
			break;
		}
	}

	vkBuffer->DestroyInternal();
	delete vkBuffer;
}

void VulkanCommandPool::FreeBuffers(std::span<GPUCommandBuffer*> buffers)
{
	for (auto* buffer : buffers)
	{
		FreeBuffer(buffer);
	}
}

