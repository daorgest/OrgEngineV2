//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <span>

#include "Platform.h"
#include "RendererTypes.h"
#include "fmt/format.h"
#include "glm/vec4.hpp"

namespace Renderer
{
    struct GPUCommandBuffer;
    struct GraphicsPipelineDesc;
    struct DescriptorSet;
    struct GPUSampler;
    struct GPUBuffer;
    struct GPUDevice;

    /// Abstract GPU API instance (VkInstance, ID3D12Device factory, etc.)
	struct GPUInterface
	{
		virtual ~GPUInterface() = default;
		virtual bool Init() = 0;
		virtual void Destroy() = 0;
	};


	// Resources
	/// Abstract GPU buffer (vertex, index, uniform, storage)
	struct GPUBuffer
	{
		virtual ~GPUBuffer() = default;
        virtual void Init(GPUDevice* device, const BufferInfo& info) = 0;
		virtual void Destroy() = 0;

		virtual void* Map() const = 0;
		virtual void Unmap() const = 0;
		virtual void Upload(const void* data, u64 size) const = 0;
		[[nodiscard]] virtual u64 GetSize() const = 0;
		[[nodiscard]] virtual u64 GetDeviceAddress() const = 0; // For bindless/ray tracing
		[[nodiscard]] virtual bool IsValid() const = 0;

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

        virtual void SetName(const std::string& name) = 0;
		virtual void UploadData(const void* data) = 0;
		TextureLayout currentLayout = TextureLayout::Unknown;
	};


	/// Abstract GPU sampler
    struct GPUSampler
    {
        GPUSampler() = default;
        virtual ~GPUSampler() = default;

        GPUSampler(const GPUSampler&) = delete;
        GPUSampler& operator=(const GPUSampler&) = delete;

        // Allow moving at the base level if needed
        GPUSampler(GPUSampler&&) noexcept = default;
        GPUSampler& operator=(GPUSampler&&) noexcept = default;
	};

	// Shaders & Pipelines
	/// Abstract shader module (SPIR-V, DXIL, etc.)
	struct GPUShader
	{
		virtual ~GPUShader() = default;
		virtual Result<void> Init(GPUDevice* device, std::span<const u32> code, ShaderFormat format) = 0;
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

    /// Abstract logical GPU device
    struct GPUDevice
    {
        virtual ~GPUDevice() = default;
        virtual bool Init(GPUInterface* instance) = 0;
        virtual void Destroy() = 0;
        virtual void WaitIdle() = 0;

        // creations
        virtual std::unique_ptr<GPUTexture> CreateTexture(TextureInfo& info) = 0;
        virtual std::unique_ptr<GPUSampler> CreateSampler(SamplerInfo& info) = 0;
        virtual std::unique_ptr<GPUBuffer> CreateBuffer(BufferInfo& info) = 0;

        [[nodiscard]] virtual const GPUDeviceDesc& GetDeviceDesc() const = 0;
    };


    class GPUQueryPool
    {
    public:
        virtual ~GPUQueryPool() = default;

        // Uses the abstract CommandBuffer type instead of void*
        virtual void Reset(GPUCommandBuffer* cmd) = 0;
        virtual void WriteTimestamp(GPUCommandBuffer* cmd, u32 queryIndex) = 0;

        virtual bool FetchResults() = 0;
        [[nodiscard]] virtual f32 GetDeltaMs(u32 beginIdx, u32 endIdx) const = 0;
    };

    // Command Recording
	// Helper structs for render target info
	struct RenderAttachment
	{
		GPUTexture* texture = nullptr;
        LoadOP loadOp = LoadOP::Clear;
        StoreOp storeOp = StoreOp::Store;

        union
        {
            f32 color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            struct
            {
                f32 depthClear;
                u32 stencilClear;
            };
        } clearValue;


        static RenderAttachment Color(GPUTexture* tex, LoadOP load = LoadOP::Clear, const glm::vec4& col = {0, 0, 0, 1})
        {
            RenderAttachment ra =
            {
                .texture = tex,
                .loadOp = load
            };
            std::memcpy(ra.clearValue.color, &col[0], sizeof(f32) * 4);
            return ra;
        }

        static RenderAttachment Depth(GPUTexture* tex, LoadOP load = LoadOP::Clear, f32 depth = 0.0f,
                                      u32 stencil = 0)
        {
            RenderAttachment ra = {
                .texture = tex,
                .loadOp = load
            };
            ra.clearValue.depthClear = depth;
			ra.clearValue.stencilClear = stencil;
			return ra;
		}
	};

	struct RenderingInfo
	{
		Extent2D extent;
		std::span<RenderAttachment> colorAttachments;
		RenderAttachment* depthAttachment = nullptr;
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

	struct GPUFence
	{
		virtual ~GPUFence() = default;
	};

	struct GPUSemaphore
	{
		virtual ~GPUSemaphore() = default;
	};


	/// Abstract command buffer for GPU work
	struct GPUCommandBuffer
	{
		virtual ~GPUCommandBuffer() = default;

		// Recording
		virtual void Begin(const CommandBufferBeginInfo* inheritanceInfo = nullptr) = 0;
        virtual void End() = 0;
        virtual void Reset() = 0;

        // Render pass
        virtual void BeginRendering(const RenderingInfo& info) = 0;
        virtual void EndRendering() = 0;

        // Pipeline & Descriptors
        virtual void BindPipeline(GPUPipeline* pipeline) = 0;
        virtual void BindDescriptorSet(DescriptorSet* set, u32 setIndex, GPUPipeline* pipeline) = 0;
        virtual void PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size,
                                   const void* data) = 0;

        // Resource Transitions
        virtual void TransitionLayout(GPUTexture* texture, TextureLayout newLayout) = 0;
        virtual void TransitionLayout(GPUTexture* texture, TextureLayout oldLayout, TextureLayout newLayout) = 0;
        virtual void GenerateMipmaps(GPUTexture* texture) = 0;

		// Vertex/Index Buffers
		virtual void BindVertexBuffer(GPUBuffer* buffer, u32 binding, u64 offset) = 0;
		virtual void BindIndexBuffer(GPUBuffer* buffer, u64 offset) = 0;

		// Drawing
		virtual void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
                                 u32 firstInstance) = 0;
        virtual void DrawIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride) = 0;

		// Compute
		virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
		virtual void DispatchIndirect(GPUBuffer* buffer, u64 offset) = 0;

		// Copy operations
		virtual void CopyBuffer(GPUBuffer* src, GPUBuffer* dst, u64 size, u64 srcOffset, u64 dstOffset) = 0;
		virtual void CopyTexture(GPUTexture* src, GPUTexture* dst) = 0;
        virtual void CopyBufferToTexture(GPUBuffer* src, GPUTexture* dst) = 0;

		// Synchronization
		virtual void PipelineBarrier(const BarrierInfo& info) = 0;

		// Viewport/Scissor
		virtual void SetViewport(const Viewport& viewport) = 0;
		virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;

		// Debug markers
		virtual void BeginDebugLabel(const char* name, f32 r, f32 g, f32 b) = 0;
		virtual void EndDebugLabel() = 0;
		virtual void InsertDebugLabel(const char* name, f32 r, f32 g, f32 b) = 0;
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
        virtual Result<void> Init(GPUDevice* devicePtr, Platform::WindowHandle windowHandle_) = 0;
        virtual void Destroy() = 0;
		virtual bool ResizeIfNeeded() = 0;

		virtual Result<u32> AcquireNextImage(GPUSemaphore* semaphore) = 0;

		[[nodiscard]] virtual PresentMode GetPresentMode() = 0;
		[[nodiscard]] virtual GPUTexture* GetImage(u32 index) = 0;
		[[nodiscard]] virtual GPUTexture* GetCurrentImage() = 0;

		[[nodiscard]] virtual const Extent2D GetExtent() const = 0;
		[[nodiscard]] virtual TextureFormat GetFormat() const = 0;

		virtual void SetVsyncMode(PresentMode mode) = 0;
		virtual void SetBufferingMode(BufferingMode mode) = 0;
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


        virtual GPUFrameData* GetCurrentFrameData() = 0;
        virtual u32 GetFrameNumber() const = 0;
        virtual u32 GetFrameIndex() const = 0;
    };

    constexpr std::string_view DecodeDriverVersion(u32 driverVersion, const GPUVendor vendor)
    {
        switch (vendor)
        {
        case GPUVendor::Nvidia:
            return fmt::format("{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 14) & 0xFF);

		case GPUVendor::Intel:
			return fmt::format("{}.{}", driverVersion >> 14, driverVersion & 0x3FFF);

		case GPUVendor::AMD:
			return fmt::format("{}.{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 12) & 0x3FF,
							   driverVersion & 0xFFF);

		case GPUVendor::Apple:
			// Apple tends to report plain numeric driver version
			return fmt::format("{}", driverVersion);

		default:
			return fmt::format("Unknown Driver (0x{:X})", driverVersion);
		}
	}

} // namespace Renderer

