//
// Created by Orgest on 11/4/2025.
//

#include "VulkanCommandBuffer.h"

#include <algorithm>
#include <ranges>

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

void VulkanCommandBuffer::Init(VulkanDevice* dev, const CommandBufferLevel level)
{
	device = dev;

    const VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphicsQueueIndex
    };

    VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &cmdPool));

	const VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmdPool,
		.level = (level == CommandBufferLevel::Secondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VK_CHECK(vkAllocateCommandBuffers(device->device, &allocInfo, &cmd));
}

void VulkanCommandBuffer::Destroy()
{
    if (cmdPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device->device, cmdPool, nullptr);
        cmdPool = VK_NULL_HANDLE;
        cmd = VK_NULL_HANDLE;
    }
}

// Internal
static VkImageLayout ResolveLayout(const VkImageLayout layout, const bool useUnified)
{
    // If Unified is OFF, trust the explicit layout.
    if (!useUnified) return layout;

    if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if (layout == VK_IMAGE_LAYOUT_UNDEFINED)
        return VK_IMAGE_LAYOUT_UNDEFINED;

    return VK_IMAGE_LAYOUT_GENERAL;
}

void VulkanCommandBuffer::CopyTexture(GPUTexture* src, GPUTexture* dst)
{
	const auto* vkSrc = static_cast<VulkanTexture*>(src);
	const auto* vkDst = static_cast<VulkanTexture*>(dst);

	// Resolve the layouts based on the unified layout feature
	const VkImageLayout srcLayout = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, device->useUnifiedLayout);
	const VkImageLayout dstLayout = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device->useUnifiedLayout);

	VkImageCopy2 region = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
		.srcSubresource = { vkSrc->subresourceRange.aspectMask, 0, 0, 1 },
		.dstSubresource = { vkDst->subresourceRange.aspectMask, 0, 0, 1 },
		.extent = {
			std::min(vkSrc->textureInfo.extent.width, vkDst->textureInfo.extent.width),
			std::min(vkSrc->textureInfo.extent.height, vkDst->textureInfo.extent.height),
			1
		}
	};

	VkCopyImageInfo2 copyInfo = {
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
		.srcImage = vkSrc->image,
		.srcImageLayout = srcLayout,
		.dstImage = vkDst->image,
		.dstImageLayout = dstLayout,
		.regionCount = 1,
		.pRegions = &region
	};

	vkCmdCopyImage2(cmd, &copyInfo);
}

void VulkanCommandBuffer::BlitTexture(GPUTexture* src, GPUTexture* dst)
{
	const auto* vkSrc = static_cast<VulkanTexture*>(src);
	const auto* vkDst = static_cast<VulkanTexture*>(dst);

	const VkImageLayout srcLayout = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, device->useUnifiedLayout);
	const VkImageLayout dstLayout = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device->useUnifiedLayout);

	VkImageBlit2 blit = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.srcSubresource = { vkSrc->subresourceRange.aspectMask, 0, 0, 1 },
		.srcOffsets = { {0, 0, 0}, {(i32)vkSrc->textureInfo.extent.width, (i32)vkSrc->textureInfo.extent.height, 1} },
		.dstSubresource = { vkDst->subresourceRange.aspectMask, 0, 0, 1 },
		.dstOffsets = { {0, 0, 0}, {(i32)vkDst->textureInfo.extent.width, (i32)vkDst->textureInfo.extent.height, 1} }
	};

	VkBlitImageInfo2 blitInfo = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = vkSrc->image,
		.srcImageLayout = srcLayout,
		.dstImage = vkDst->image,
		.dstImageLayout = dstLayout,
		.regionCount = 1,
		.pRegions = &blit,
		.filter = VK_FILTER_LINEAR
	};

	vkCmdBlitImage2(cmd, &blitInfo);
}

void VulkanCommandBuffer::CollectTracy() const
{
#ifdef TRACY_ENABLE
    if (tracyCtx)
    {
        TracyVkCollect(tracyCtx, cmd);
    }
#endif
    void(0);
}

void VulkanFence::Init(VulkanDevice* dev, const bool createSignaled)
{
    device = dev;

    const VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u
    };
    VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &fence));
}

void VulkanFence::Destroy()
{
    if (device && fence)
    {
        vkDestroyFence(device->device, fence, nullptr);
        fence = VK_NULL_HANDLE;
    }
};

void VulkanFence::Reset()
{
    VK_CHECK(vkResetFences(device->device, 1, &fence));
}

void VulkanSemaphore::Init(VulkanDevice* dev)
{
    device = dev;

    constexpr VkSemaphoreCreateInfo semInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &semaphore));
}

void VulkanSemaphore::Destroy()
{
    if (device && semaphore)
    {
        vkDestroySemaphore(device->device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }
}

void VulkanCommandBuffer::Begin(const CommandBufferBeginInfo* inheritanceInfo)
{
    VkCommandBufferBeginInfo beginInfo = {
       .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

	if (inheritanceInfo)
	{
		if (inheritanceInfo->oneTimeSubmit)
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	else
	{
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
}

void VulkanCommandBuffer::End()
{
	VK_CHECK(vkEndCommandBuffer(cmd));
}

void VulkanCommandBuffer::Reset()
{
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
}

void VulkanCommandBuffer::BeginRendering(const RenderingInfo& info)
{
    if (device->useUnifiedLayout)
    {
        VkMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_2_SHADER_WRITE_BIT |
                             VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_2_SHADER_READ_BIT
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

    for (const auto& [i, attachment] : std::views::enumerate(info.colorAttachments))
    {
        const auto* vkView = static_cast<VulkanTextureView*>(attachment.view);

        VkClearValue clear{};
        std::ranges::copy(attachment.clearValue.color, clear.color.float32);

        colorAttachments[i] = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext       = nullptr,
            .imageView   = vkView->imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .loadOp      = ToVk(attachment.loadOp),
            .storeOp     = ToVk(attachment.storeOp),
            .clearValue  = clear
        };

        if (attachment.resolveView)
        {
            const auto* vkResolveView = static_cast<VulkanTextureView*>(attachment.resolveView);
            colorAttachments[i].resolveImageView = vkResolveView->imageView;
            colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
            colorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        }
    }

	VkRenderingAttachmentInfo depthAttachment = {};
	if (info.depthAttachment)
	{
	    const auto* vkDepthView = static_cast<VulkanTextureView*>(info.depthAttachment->view);

        VkClearValue clearValue;
        clearValue.depthStencil = {
            info.depthAttachment->clearValue.ds.depth,
            info.depthAttachment->clearValue.ds.stencil
        };

		depthAttachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = vkDepthView->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
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
	const auto* vkPipeline = static_cast<const VulkanPipeline*>(pipeline);
	vkCmdBindPipeline(cmd, vkPipeline->bindPoint, vkPipeline->vkPipeline);
}

void VulkanCommandBuffer::BindDescriptorSet(const GPUDescriptorSet* set, u32 setIndex, GPUPipeline* pipeline)
{
    const auto* vkPipeline = static_cast<const VulkanPipeline*>(pipeline);
    const auto* vkSet = static_cast<const VulkanDescriptorSet*>(set);
    vkCmdBindDescriptorSets(cmd, vkPipeline->bindPoint, vkPipeline->vkLayout, setIndex, 1, &vkSet->vk, 0, nullptr);
}

void VulkanCommandBuffer::PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size, const void* data)
{
    auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
    vkCmdPushConstants(cmd, vkPipeline->vkLayout, static_cast<VkShaderStageFlags>(stages), offset, size, data);
}

void VulkanCommandBuffer::TransitionLayout(GPUTexture* texture, TextureLayout newLayout)
{
    if (!texture) return;
    if (texture->currentLayout == newLayout) return;

    pendingTransitions.push_back({
        .texture = texture,
        .oldLayout = texture->currentLayout,
        .newLayout = newLayout
    });

    texture->currentLayout = newLayout;
}

void VulkanCommandBuffer::FlushBarriers()
{
    if (pendingTransitions.empty()) return;

    TransitionLayouts(pendingTransitions);

    pendingTransitions.clear();
}

void VulkanCommandBuffer::TransitionLayouts(Span<const TextureTransition> transitions)
{
    if (transitions.empty()) return;

    thread_local Vector<VkImageMemoryBarrier2> barriers;
    barriers.clear();

    if (barriers.capacity() < transitions.size()) {
        barriers.reserve(transitions.size());
    }

    for (const auto& trans : transitions)
    {
        const auto* vkTex = static_cast<VulkanTexture*>(trans.texture);
        if (!vkTex || vkTex->image == VK_NULL_HANDLE) continue;

        const SyncState srcSync = GetSyncState(trans.oldLayout);
        const SyncState dstSync = GetSyncState(trans.newLayout);

        const u32 mipCount = (trans.mipLevelCount == REMAINING_MIP_LEVELS)
                                 ? vkTex->textureInfo.mipLevels
                                 : trans.mipLevelCount;

        const u32 layerCount = (trans.layerCount == REMAINING_ARRAY_LAYERS)
                                   ? vkTex->textureInfo.arrayLayers
                                   : trans.layerCount;


        barriers.push_back({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = srcSync.stageMask,
            .srcAccessMask = srcSync.accessMask,
            .dstStageMask = dstSync.stageMask,
            .dstAccessMask = dstSync.accessMask,
            .oldLayout = ToVkImageLayout(trans.oldLayout, device->useUnifiedLayout),
            .newLayout = ToVkImageLayout(trans.newLayout, device->useUnifiedLayout),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vkTex->image,
            .subresourceRange = {
                .aspectMask = vkTex->subresourceRange.aspectMask,
                .baseMipLevel = trans.baseMipLevel,
                .levelCount = mipCount,
                .baseArrayLayer = trans.baseArrayLayer,
                .layerCount = layerCount
            }
        });
    }

    if (barriers.empty()) return;

    const VkDependencyInfo depInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<u32>(barriers.size()),
        .pImageMemoryBarriers = barriers.data()
    };


    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void VulkanCommandBuffer::GenerateMipmaps(GPUTexture* texture)
{
    const auto* vkTexture = static_cast<VulkanTexture*>(texture);
    const auto& info = vkTexture->textureInfo;

    const u32 mipLevels = info.mipLevels;
    const u32 layers = (info.type == ImageType::CubeMap) ? 6u : info.arrayLayers;
    const VkImageAspectFlags aspect = vkTexture->subresourceRange.aspectMask;

	if (mipLevels <= 1)
	{
		LOG(Warning, "No need to generate mipmaps if only 1 level exists, skipping...");
		return;
	}

    i32 mipWidth  = static_cast<i32>(info.extent.width);
    i32 mipHeight = static_cast<i32>(info.extent.height);

    const VkImageLayout unifiedSrc = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, device->useUnifiedLayout);
    const VkImageLayout unifiedDst = ResolveLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device->useUnifiedLayout);

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
            .oldLayout = unifiedDst,
            .newLayout = unifiedSrc,
            .image = vkTexture->image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = mipLevel - 1,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = layers
            }
        };

        // Transition next mip level to TRANSFER_DST_OPTIMAL
        barriers[1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = unifiedDst,
            .image = vkTexture->image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = mipLevel,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = layers
            }
        };

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = barriers
        };
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Blit operation for downscaling
        VkImageBlit2 blit = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {aspect, mipLevel - 1, 0, layers},
            .srcOffsets = {{0, 0, 0}, {mipWidth, mipHeight, 1}},
            .dstSubresource = {aspect, mipLevel, 0, layers},
            .dstOffsets = {{0, 0, 0}, {std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1}}
        };

        VkBlitImageInfo2 blitInfo{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = vkTexture->image,
            .srcImageLayout = unifiedSrc,
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

    const SyncState dstSync = GetSyncState(TextureLayout::ShaderReadOnly);
    const VkImageLayout finalVkLayout = ToVkImageLayout(TextureLayout::ShaderReadOnly, device->useUnifiedLayout);

    Vector<VkImageMemoryBarrier2> finalBarriers;
    finalBarriers.reserve(mipLevels);

    for (u32 m = 0; m < mipLevels; m++)
    {
        VkImageLayout currentVkLayout = (m < mipLevels - 1) ? unifiedSrc : unifiedDst;

        finalBarriers.push_back({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = dstSync.stageMask,
            .dstAccessMask = dstSync.accessMask,
            .oldLayout = currentVkLayout,
            .newLayout = finalVkLayout,
            .image = vkTexture->image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = m,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = layers
            }
        });
    }

    VkDependencyInfo finalDep{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<u32>(finalBarriers.size()),
        .pImageMemoryBarriers = finalBarriers.data()
    };
    vkCmdPipelineBarrier2(cmd, &finalDep);
}

void VulkanCommandBuffer::BindVertexBuffer(GPUBuffer* buffer, const u32 binding, const u64 offset)
{
	const auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdBindVertexBuffers(cmd, binding, 1, &vkBuffer->buffer, &offset);
}

void VulkanCommandBuffer::BindIndexBuffer(GPUBuffer* buffer, u64 offset)
{
	const auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
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
	const auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdDrawIndirect(cmd, vkBuffer->buffer, offset, drawCount, stride);
}

void VulkanCommandBuffer::DrawIndexedIndirect(GPUBuffer* buffer, const u64 offset, const u32 drawCount, const u32 stride)
{
    const auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
    vkCmdDrawIndexedIndirect(cmd, vkBuffer->buffer, offset, drawCount, stride);
}

void VulkanCommandBuffer::Dispatch(const u32 groupCountX, const u32 groupCountY, const u32 groupCountZ)
{
	vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}

void VulkanCommandBuffer::DispatchIndirect(GPUBuffer* buffer, const u64 offset)
{
	const auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
	vkCmdDispatchIndirect(cmd, vkBuffer->buffer, offset);
}

void VulkanCommandBuffer::WaitForFence(GPUFence* fence)
{
    if (fence)
    {
        const VulkanFence* vkFence = static_cast<VulkanFence*>(fence);
        VK_CHECK(vkWaitForFences(device->device, 1, &vkFence->fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device->device, 1, &vkFence->fence));
    }
}

void VulkanCommandBuffer::ExecuteCommands(Span<GPUCommandBuffer*> secondaryBuffers) const
{
	// if (isSecondary)
	// {
	// 	LOG(Error, "Cannot execute commands from a secondary command buffer");
	// 	return;
	// }

	thread_local Vector<VkCommandBuffer> vkCmds(secondaryBuffers.size());
    vkCmds.clear();
	vkCmds.reserve(secondaryBuffers.size());

	for (auto* buffer : secondaryBuffers)
	{
	    vkCmds.push_back(static_cast<VulkanCommandBuffer*>(buffer)->GetVkHandle());
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
	const auto* vkSrc = static_cast<VulkanBuffer*>(src);
	const auto* vkDst = static_cast<VulkanBuffer*>(dst);

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
    const auto* vkSrc = static_cast<VulkanBuffer*>(src);
    const auto* vkDst = static_cast<VulkanTexture*>(dst);

    const auto& info = vkDst->textureInfo;
    const u32 mipLevels = info.mipLevels;
    const u32 layers = (info.type == ImageType::CubeMap) ? 6u : info.arrayLayers;

    const u32 bpt = BytesPerTexel(info.format);
    u32 elementSize = 0;
    bool isCompressed = false;

    if (bpt == 0 && info.format != TextureFormat::UNKNOWN)
    {
        isCompressed = true;
        // 8 bytes for BC1/BC4, 16 for everything else
        const bool is8ByteBlock = (info.format == TextureFormat::BC1_RGB_UNORM_BLOCK ||
                                   info.format == TextureFormat::BC1_RGBA_UNORM_BLOCK ||
                                   info.format == TextureFormat::BC4_UNORM_BLOCK);
        elementSize = is8ByteBlock ? 8 : 16;
    }
    else
    {
        elementSize = bpt;
    }

    thread_local Vector<VkBufferImageCopy2> regions;
    regions.clear();
    regions.reserve(mipLevels * layers);
    VkDeviceSize currentBufferOffset = 0;

    // Build the copy regions for every subresource
    for (u32 layer = 0; layer < layers; ++layer)
    {
        for (u32 mip = 0; mip < mipLevels; ++mip)
        {
            const u32 mipW = std::max(1u, info.extent.width >> mip);
            const u32 mipH = std::max(1u, info.extent.height >> mip);
            const u32 mipD = std::max(1u, info.extent.depth >> mip);

            VkDeviceSize mipSize = 0;

            if (isCompressed)
            {
                // Compressed blocks must round up to the nearest 4x4 boundary
                const u32 blocksW = (mipW + 3) / 4;
                const u32 blocksH = (mipH + 3) / 4;
                mipSize = static_cast<VkDeviceSize>(blocksW) * blocksH * elementSize;
            }
            else
            {
                mipSize = static_cast<VkDeviceSize>(mipW) * mipH * mipD * elementSize;
            }

            VkBufferImageCopy2 region = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .bufferOffset = currentBufferOffset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = vkDst->subresourceRange.aspectMask,
                    .mipLevel = mip,
                    .baseArrayLayer = layer,
                    .layerCount = 1
                },
                .imageExtent = { mipW, mipH, mipD }
            };

            regions.push_back(region);
            currentBufferOffset += mipSize;
        }
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
    if (tracyCtx)
    {
        TracyVkCollect(tracyCtx, cmd);
    }
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