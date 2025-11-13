//
// Created by Orgest on 11/4/2025.
//

#pragma once
#include "RenderInterface.h"
#include <volk.h>
#include <tracy/TracyVulkan.hpp>

namespace Renderer
{
	struct VulkanDevice;
	struct VulkanPipeline;
	struct VulkanBuffer;
	struct VulkanImage;

	struct VulkanSwapchain;

	/// Vulkan implementation of GPUCommandBuffer
	struct VulkanCommandBuffer final : GPUCommandBuffer
	{
		// Lifecycle
		void Begin(const CommandBufferBeginInfo* inheritanceInfo = nullptr) override;
		void End() override;
		void Reset() override;

		// Rendering
		void BeginRendering(const RenderingInfo& info) override;
		void EndRendering() override;

		// Pipeline & Descriptors
		void BindPipeline(GPUPipeline* pipeline) override;
		void BindDescriptorSet(DescriptorSet* set, u32 setIndex, GPUPipeline* pipeline) override;
		void PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size, const void* data) override;

		// Vertex/Index Buffers
		void BindVertexBuffer(GPUBuffer* buffer, u32 binding, u64 offset) override;
		void BindIndexBuffer(GPUBuffer* buffer, u64 offset) override;

		// Drawing
		void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
		void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) override;
		void DrawIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride) override;

		// Compute
		void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;
		void DispatchIndirect(GPUBuffer* buffer, u64 offset) override;

		// Synchronization
		void PipelineBarrier(const BarrierInfo& info) override;
		void ExecuteCommands(std::span<GPUCommandBuffer*> secondaryBuffers) override;

		// Viewport/Scissor
		void SetViewport(const Viewport& viewport) override;
		void SetScissor(u32 x, u32 y, u32 width, u32 height) override;

		// Copy operations
		void CopyBuffer(GPUBuffer* src, GPUBuffer* dst, u64 size, u64 srcOffset, u64 dstOffset) override;
		void CopyBufferToTexture(GPUBuffer* src, GPUTexture* dst) override;

		// Debug markers
		void BeginDebugLabel(const char* name, f32 r, f32 g, f32 b) override;
		void EndDebugLabel() override;
		void InsertDebugLabel(const char* name, f32 r, f32 g, f32 b) override;

		// Query
		bool IsSecondary() const override { return isSecondary; }

		// Internal Vulkan-specific methods
		void InitInternal(VulkanDevice* device, VkCommandPool pool, bool secondary = false);
		void DestroyInternal();
		[[nodiscard]] VkCommandBuffer GetVkHandle() const { return cmd; }

		// Public for VulkanRenderer to set Tracy context
		TracyVkCtx tracyCtx = nullptr;

	private:
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkCommandPool parentPool = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		bool isSecondary = false;
	};

	/// Command pool for allocating command buffers
	struct VulkanCommandPool final : GPUCommandPool
	{
		void Init(GPUDevice* device, u32 queueFamilyIndex, bool transient) override;
		void Destroy() override;
		void Reset() override;

		GPUCommandBuffer* AllocateBuffer(bool secondary) override;
		void FreeBuffer(GPUCommandBuffer* buffer) override;
		void FreeBuffers(std::span<GPUCommandBuffer*> buffers) override;

		[[nodiscard]] VkCommandPool GetVkHandle() const { return pool; }

	private:
		VkCommandPool pool = VK_NULL_HANDLE;
		VulkanDevice* device = nullptr;
		Vector<VulkanCommandBuffer*> allocatedBuffers;
	};

} // namespace Renderer

