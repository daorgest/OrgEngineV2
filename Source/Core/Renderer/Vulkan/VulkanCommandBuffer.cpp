//
// Created by Orgest on 11/4/2025.
//

#include "VulkanCommandBuffer.h"

#include <algorithm>

#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanDescriptors.h"
#include "VulkanDebugUtils.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanDevice.h"
#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

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

void VulkanCommandBuffer::InitFromHandle(VulkanDevice* dev, const VkCommandBuffer handle)
{
	device = dev;
	cmd = handle;
	parentPool = VK_NULL_HANDLE;
	isSecondary = false;
}

void VulkanCommandBuffer::DestroyInternal()
{
	if (cmd != VK_NULL_HANDLE && parentPool != VK_NULL_HANDLE && device != nullptr)
	{
		vkFreeCommandBuffers(device->device, parentPool, 1, &cmd);
		cmd = VK_NULL_HANDLE;
	}
}

void VulkanCommandBuffer::CopyTexture(GPUTexture* src, GPUTexture* dst)
{
	// TODO (Orgest): oop
}

VulkanFence::VulkanFence(VulkanDevice* device)
{
	this->device = device;

	VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &fence));
}

VulkanFence::~VulkanFence()
{
	if (device && fence)
	{
		vkDestroyFence(device->device, fence, nullptr);
	}
}

VulkanSemaphore::VulkanSemaphore(VulkanDevice* device)
{
	this->device = device;

	VkSemaphoreCreateInfo semInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &semaphore));
}

VulkanSemaphore::~VulkanSemaphore()
{
	if (device && semaphore)
	{
		vkDestroySemaphore(device->device, semaphore, nullptr);
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
	TracyVkCollect(tracyCtx, cmd);

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
	constexpr VkImageLayout unifiedLayout = VK_IMAGE_LAYOUT_GENERAL;

	if (device->useUnifiedLayout) // Assuming your device pointer is accessible
	{
		VkMemoryBarrier2 barrier = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
							VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
							VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
							 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
							 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
		 };

		VkDependencyInfo dep = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &barrier
		 };
		vkCmdPipelineBarrier2(cmd, &dep);
	}
	const u32 actualAttachmentCount = static_cast<u32>(info.colorAttachments.size());
	Array<VkRenderingAttachmentInfo, MAX_RENDER_TARGETS> colorAttachments = {};

	for (u32 i = 0; i < actualAttachmentCount; i++)
	{
		const auto& attachment = info.colorAttachments[i];

		// Map clear colors from our RenderAttachment structure
		VkClearValue clear;
		// The compiler will turn this into a single SIMD instruction in Release
		std::memcpy(&clear.color.float32, &attachment.clearValue.color, 16);

		colorAttachments[i] = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext       = nullptr,
			.imageView   = static_cast<VulkanTexture*>(attachment.texture)->imageView,
			.imageLayout = unifiedLayout,
			.loadOp      = ToVk(attachment.loadOp),
			.storeOp     = ToVk(attachment.storeOp),
			.clearValue  = clear
		};
	}

	VkRenderingAttachmentInfo depthAttachment = {};
	if (info.depthAttachment)
	{
		auto* vkDepth = static_cast<VulkanTexture*>(info.depthAttachment->texture);

		VkClearValue clearValue;
		clearValue.depthStencil = {
			info.depthAttachment->clearValue.depthClear,
			info.depthAttachment->clearValue.stencilClear
		};

		depthAttachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = vkDepth->imageView,
			.imageLayout = ToVk(vkDepth->imageLayout),
			.loadOp = ToVk(info.depthAttachment->loadOp),
			.storeOp = ToVk(info.depthAttachment->storeOp),
			.clearValue = clearValue
		};
	}

	VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {.offset = {0, 0}, .extent = {info.extent.width, info.extent.height}},
		.layerCount = 1,
		.colorAttachmentCount = actualAttachmentCount,
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
	const auto* vkSet = set;
	const auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->vkLayout,
	                       setIndex, 1, &vkSet->vk, 0, nullptr);
}

void VulkanCommandBuffer::PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size, const void* data)
{
	const auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
	VkShaderStageFlags vkStages = ToVk(stages);
	vkCmdPushConstants(cmd, vkPipeline->vkLayout, vkStages, offset, size, data);
}

void VulkanCommandBuffer::TransitionLayout(GPUTexture* texture, TextureLayout newLayout)
{
	TransitionLayout(texture, texture->currentLayout, newLayout);
}

void VulkanCommandBuffer::TransitionLayout(GPUTexture* texture, TextureLayout oldLayout, TextureLayout newLayout)
{
	if (oldLayout == newLayout && !device->useUnifiedLayout) return;

	auto* vkTexture = static_cast<VulkanTexture*>(texture);

	VkImageLayout vkOld = ToVk(oldLayout);
	VkImageLayout vkNew = ToVk(newLayout);

	if (device->useUnifiedLayout)
	{
		if (vkNew != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
			vkNew != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			vkNew = VK_IMAGE_LAYOUT_GENERAL;
		}
	}

	const SyncState src = GetSyncState(oldLayout, false);
	const SyncState dst = GetSyncState(newLayout, true);

	VkImageMemoryBarrier2 imageBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = src.stageMask,
		.srcAccessMask = src.accessMask,
		.dstStageMask = dst.stageMask,
		.dstAccessMask = dst.accessMask,
		.oldLayout = vkOld,
		.newLayout = vkNew,
		.image = vkTexture->image,
		.subresourceRange = vkTexture->subresourceRange
	};

	VkDependencyInfo depInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &imageBarrier
	};

	vkCmdPipelineBarrier2(cmd, &depInfo);
	vkTexture->imageLayout = device->useUnifiedLayout
		                         ? TextureLayout::General
		                         : TextureLayout::ShaderReadOnly;
}

void VulkanCommandBuffer::GenerateMipmaps(GPUTexture* texture)
{
	auto* vkTexture = static_cast<VulkanTexture*>(texture);

	const u32 mipLevels = vkTexture->textureInfo.mipLevels;
	i32 mipWidth = static_cast<i32>(vkTexture->textureInfo.extent.width);
	i32 mipHeight = static_cast<i32>(vkTexture->textureInfo.extent.height);

	if (mipLevels <= 1)
	{
		LOG(Warning, "No need to generate mipmaps if only 1 level exists, skipping...");
		return;
	}

	const VkImageLayout unifiedLayout = device->useUnifiedLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	const VkImageLayout unifiedDst    = device->useUnifiedLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;


	// Reuse this for barriers
    VkImageMemoryBarrier2 barriers[2]{};

    for (u32 mipLevel = 1; mipLevel < mipLevels; mipLevel++)
    {
        // 1. Transition previous mip level to SOURCE layout
        barriers[0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            // Force GENERAL if unified, otherwise use specific transfer layout
            .oldLayout = (device->useUnifiedLayout && vkTexture->imageLayout != TextureLayout::Unknown) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = unifiedLayout,
            .image = vkTexture->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipLevel - 1,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        // Transition next mip level to TRANSFER_DST_OPTIMAL
        barriers[1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // Mips start undefined
            .newLayout = unifiedDst,
            .image = vkTexture->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipLevel,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = barriers
        };
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Blit operation for downscaling
        VkImageBlit2 blit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1, 0, 1},
            .srcOffsets = { { 0, 0, 0 }, { mipWidth, mipHeight, 1 } },
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1},
            .dstOffsets = { { 0, 0, 0 }, { std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1 } }
        };

        VkBlitImageInfo2 blitInfo{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = vkTexture->image,
            .srcImageLayout = unifiedLayout,
            .dstImage = vkTexture->image,
            .dstImageLayout = unifiedDst,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR
        };
        vkCmdBlitImage2(cmd, &blitInfo);

        // Update extent for next mip
        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
    }

    // If unifiedLayouts is true, this will keep them in VK_IMAGE_LAYOUT_GENERAL
    // while ensuring memory visibility for Fragment Shaders.
    TransitionLayout(texture, TextureLayout::ShaderReadOnly);
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
	auto* vkDst = static_cast<VulkanTexture*>(dst);

	const u32 layers = std::max<u32>(1, vkDst->textureInfo.arrayLayers);
	Vector<VkBufferImageCopy2> regions;
	regions.reserve(layers);

	const u32 bytesPerTexel = BytesPerTexel(vkDst->textureInfo.format);
	const VkDeviceSize layerSize = static_cast<VkDeviceSize>(vkDst->textureInfo.extent.width) * static_cast<VkDeviceSize>(vkDst->textureInfo.extent.height) * static_cast<VkDeviceSize>(vkDst->textureInfo.extent.depth) * bytesPerTexel;

	for (u32 i = 0; i < layers; ++i)
	{
		VkBufferImageCopy2 region = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.bufferOffset = layerSize * i,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vkDst->subresourceRange.aspectMask,
				.mipLevel = 0,
				.baseArrayLayer = i,
				.layerCount = 1
			},
			.imageOffset = {0, 0, 0},
			.imageExtent = {
				vkDst->textureInfo.extent.width,
				vkDst->textureInfo.extent.height,
				vkDst->textureInfo.extent.depth
			}
		};
		regions.push_back(region);
	}

	const VkImageLayout copyLayout = device->useUnifiedLayout
									 ? VK_IMAGE_LAYOUT_GENERAL
									 : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	VkCopyBufferToImageInfo2 copyInfo = {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.srcBuffer = vkSrc->buffer,
		.dstImage = vkDst->image,
		.dstImageLayout = copyLayout,
		.regionCount = static_cast<u32>(regions.size()),
		.pRegions = regions.data()
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