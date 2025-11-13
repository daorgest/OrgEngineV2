//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <span>
#include "RendererTypes.h"
#include "Tools/Vector.h"

#include "VulkanDescriptors.h"
#include "glm/vec4.hpp"

namespace Renderer
{

	/// Abstract GPU API instance (VkInstance, ID3D12Device factory, etc.)
	struct GPUInterface
	{
		virtual ~GPUInterface() = default;
		virtual bool Init() = 0;
		virtual void Destroy() = 0;
	};

	/// Abstract logical GPU device
	struct GPUDevice
	{
		virtual ~GPUDevice() = default;
		virtual bool Init(GPUInterface* instance) = 0;
		virtual void Destroy() = 0;
		virtual void WaitIdle() = 0;
		[[nodiscard]] virtual const GPUDeviceDesc& GetDeviceDesc() const = 0;

		// Immediate command submission (for uploads, transitions, etc.)
		template<typename Func>
		void ImmediateSubmit(Func&& function, const char* debugLabel = nullptr);
	};

	// Resources
	/// Abstract GPU buffer (vertex, index, uniform, storage)
	struct GPUBuffer
	{
		virtual ~GPUBuffer() = default;
		virtual void Init(GPUDevice* device, const GPUBufferInfo& info) = 0;
		virtual void Destroy() = 0;

		virtual void* Map() const = 0;
		virtual void Unmap() const = 0;
		virtual void Upload(const void* data, u64 size) const = 0;
		[[nodiscard]] virtual u64 GetSize() const = 0;
		[[nodiscard]] virtual u64 GetDeviceAddress() const = 0; // For bindless/ray tracing
		[[nodiscard]] virtual bool IsValid() const = 0;

		GPUBufferInfo info; // Common metadata

		template<typename T>
		void Upload(std::span<const T> span) const
		{
			Upload(std::as_bytes(span).data(), std::as_bytes(span).size());
		}

		template<typename T>
		void UploadObject(const T& object) const
		{
			Upload(&object, sizeof(T));
		}
	};

	/// Abstract GPU texture/image
	struct GPUTexture
	{
		virtual ~GPUTexture() = default;
		virtual void Init(GPUDevice* device, const TextureInfo& info) = 0;
		virtual void Destroy() = 0;

		virtual void UploadData(const void* data) = 0;
		virtual void TransitionLayout(void* cmdBuffer, TextureLayout newLayout) = 0;
		virtual void GenerateMipmaps(void* cmdBuffer) = 0;

		TextureInfo textureInfo;  // Common metadata
		TextureLayout currentLayout = TextureLayout::Unknown;
	};

	/// Abstract GPU sampler
	struct GPUSampler
	{
		virtual ~GPUSampler() = default;
		virtual void Init(GPUDevice* device, const SamplerDesc& desc) = 0;
		virtual void Destroy() = 0;

		SamplerDesc desc; // Common descriptor
	};

	// Shaders & Pipelines
	/// Abstract shader module (SPIR-V, DXIL, etc.)
	struct GPUShader
	{
		virtual ~GPUShader() = default;
		virtual Result<void> Init(GPUDevice* device, std::span<const u32> code, ShaderFormat format) = 0;
		virtual void Destroy() = 0;

		ShaderFormat format = ShaderFormat::UNKNOWN;
	};

	/// Shader hot-reload manager (optional feature)
	struct GPUShaderManager
	{
		virtual ~GPUShaderManager() = default;
		virtual void Init(GPUDevice* device) = 0;
		virtual void Destroy() = 0;
		virtual void CheckForReloads() = 0; // Poll for file changes
		virtual void RegisterPipeline(void* pipeline, const char* sourcePath) = 0;
	};

	/// Abstract graphics/compute pipeline
	struct GPUPipeline
	{
		virtual ~GPUPipeline() = default;
		virtual void Destroy() = 0;
		virtual bool IsValid() const = 0;

		// Hot-reload support
		virtual void Rebuild(GPUDevice* device) = 0;
	};

	// Descriptors (Bindless Resources)
	/// Abstract descriptor set layout (defines resource binding structure)
	struct GPUDescriptorLayout
	{
		virtual ~GPUDescriptorLayout() = default;
		virtual void Destroy() = 0;
	};

	/// Abstract descriptor set (actual resource bindings)
	struct GPUDescriptorSet
	{
		virtual ~GPUDescriptorSet() = default;

		// Resource binding operations
		virtual void WriteBuffer(u32 binding, GPUBuffer* buffer, DescriptorType type) = 0;
		virtual void WriteTexture(u32 binding, GPUTexture* texture, GPUSampler* sampler, DescriptorType type) = 0;
		virtual void WriteTextureArray(u32 binding, std::span<GPUTexture*> textures, DescriptorType type) = 0;
		virtual void Update(GPUDevice* device) = 0;
	};

	// Command Recording
	// Helper structs for render target info
	struct RenderingAttachment
	{
		GPUTexture* texture = nullptr;
		LoadOP loadOp = LoadOP::Clear;
		StoreOp storeOp = StoreOp::Store;
		glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
		f32 clearDepth = 1.0f; // Standard depth: clear to 1.0 (far plane)
		u32 clearStencil = 0;
	};

	struct RenderingInfo
	{
		u32 width = 0;
		u32 height = 0;
		Vector<RenderingAttachment> colorAttachments;
		RenderingAttachment* depthAttachment = nullptr;
	};

	/// Command buffer begin information
	struct CommandBufferBeginInfo
	{
		bool isSecondary = false;
		bool oneTimeSubmit = true;
		const RenderingInfo* renderPassInfo = nullptr; // For secondary buffers
	};

	/// Barrier information for synchronization
	struct BarrierInfo
	{
		// TODO(Orgest): Expand with actual barrier details
		u32 placeholder = 0;
	};

	/// Abstract command buffer for GPU work
	struct GPUCommandBuffer
	{
		virtual ~GPUCommandBuffer() = default;

		// Recording
		virtual void Begin(const CommandBufferBeginInfo* inheritanceInfo) = 0;
		virtual void End() = 0;
		virtual void Reset() = 0;

		// Render pass
		virtual void BeginRendering(const RenderingInfo& info) = 0;
		virtual void EndRendering() = 0;

		// Pipeline & Descriptors
		virtual void BindPipeline(GPUPipeline* pipeline) = 0;
		virtual void BindDescriptorSet(DescriptorSet* set, u32 setIndex, GPUPipeline* pipeline) = 0;
		virtual void PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size, const void* data) = 0;

		// Vertex/Index Buffers
		virtual void BindVertexBuffer(GPUBuffer* buffer, u32 binding = 0, u64 offset = 0) = 0;
		virtual void BindIndexBuffer(GPUBuffer* buffer, u64 offset = 0) = 0;

		// Drawing
		virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0) = 0;
		virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0, i32 vertexOffset = 0, u32 firstInstance = 0) = 0;
		virtual void DrawIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride) = 0;

		// Compute
		virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
		virtual void DispatchIndirect(GPUBuffer* buffer, u64 offset) = 0;

		// Synchronization
		virtual void PipelineBarrier(const BarrierInfo& info) = 0;
		virtual void ExecuteCommands(std::span<GPUCommandBuffer*> secondaryBuffers) = 0;

		// Viewport/Scissor
		virtual void SetViewport(const Viewport& viewport) = 0;
		virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;

		// Copy operations
		virtual void CopyBuffer(GPUBuffer* src, GPUBuffer* dst, u64 size, u64 srcOffset = 0, u64 dstOffset = 0) = 0;
		virtual void CopyBufferToTexture(GPUBuffer* src, GPUTexture* dst) = 0;

		// Debug markers
		virtual void BeginDebugLabel(const char* name, f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f) = 0;
		virtual void EndDebugLabel() = 0;
		virtual void InsertDebugLabel(const char* name, f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f) = 0;

		// Query
		virtual bool IsSecondary() const = 0;
	};

	/// Command pool for allocating command buffers
	struct GPUCommandPool
	{
		virtual ~GPUCommandPool() = default;

		virtual void Init(GPUDevice* device, u32 queueFamilyIndex, bool transient = false) = 0;
		virtual void Destroy() = 0;
		virtual void Reset() = 0;

		virtual GPUCommandBuffer* AllocateBuffer(bool secondary = false) = 0;
		virtual void FreeBuffer(GPUCommandBuffer* buffer) = 0;
		virtual void FreeBuffers(std::span<GPUCommandBuffer*> buffers) = 0;
	};

	// Swapchain & Presentation
	/// Abstract swapchain (platform-specific)
	struct GPUSwapchain
	{
		virtual ~GPUSwapchain() = default;
		virtual bool Init(GPUDevice* device, void* windowHandle) = 0;
		virtual void Destroy() = 0;
		virtual bool Resize() = 0;

		virtual u32 AcquireNextImage(void* semaphore) = 0;
		virtual void Present(u32 imageIndex, void* waitSemaphore) = 0;
		virtual GPUTexture* GetCurrentImage() = 0;
		[[nodiscard]] virtual const Extent2D& GetExtent() const = 0;
		[[nodiscard]] virtual TextureFormat GetFormat() const = 0;
	};

	// Frame Management
	/// Per-frame resources (command buffer, sync objects, descriptors)
	struct GPUFrameData
	{
		virtual ~GPUFrameData() = default;
		virtual bool Init(GPUDevice* device) = 0;
		virtual void Destroy() = 0;
		virtual void Reset() = 0; // Reset per-frame allocators

		virtual GPUCommandBuffer* GetCommandBuffer() = 0;
		virtual void* GetRenderFence() = 0;
		virtual void* GetAcquireSemaphore() = 0;
		virtual void* GetPresentSemaphore() = 0;
	};

	/// High-level renderer (manages frames, submission)
	struct GPURenderer
	{
		virtual ~GPURenderer() = default;
		virtual bool Init(GPUDevice* device, GPUSwapchain* swapchain, u32 frameOverlap = 2) = 0;
		virtual void Destroy() = 0;

		// Frame lifecycle
		virtual bool BeginFrame(u32& frameIndex, u32& imageIndex) = 0;
		virtual void EndFrame(u32 frameIndex, u32 imageIndex) = 0;
		virtual bool ResizeIfNeeded() = 0;

		virtual GPUFrameData* GetCurrentFrameData() = 0;
		virtual u32 GetFrameNumber() const = 0;
	};

} // namespace Renderer

