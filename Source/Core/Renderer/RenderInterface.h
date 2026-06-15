//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <memory>

#include "Platform.h"
#include "RendererTypes.h"
#include "RenderInterface.h"
#include "Tools/Vector.h"

#include "../../../Engine/MeshData.h"
#include "fmt/format.h"

namespace Renderer
{
    struct GPUCommandBuffer;
    struct GPUQueryPool;
    struct ShaderCompiler;
    struct GPUDevice;
    struct GPUPipeline;

    struct NoCopyMove
    {
        NoCopyMove() = default;
        virtual ~NoCopyMove() = default;

        NoCopyMove(const NoCopyMove&) = delete;
        NoCopyMove& operator=(const NoCopyMove&) = delete;
        NoCopyMove(NoCopyMove&&) = delete;
        NoCopyMove& operator=(NoCopyMove&&) = delete;
    };

    struct NoCopy
    {
        NoCopy() = default;
        virtual ~NoCopy() = default;

        NoCopy(const NoCopy&) = delete;
        NoCopy& operator=(const NoCopy&) = delete;
    };

    struct GPUBuffer : NoCopy
    {
        GPUBuffer() = default;
        ~GPUBuffer() override = default;

        virtual void Init(GPUDevice* device, const BufferInfo& info) = 0;
        virtual void Destroy() = 0;

        [[nodiscard]] virtual void* Map() const = 0;
        virtual void Unmap() const = 0;
        virtual void Upload(const void* data, u64 size) const = 0;

        [[nodiscard]] virtual u64 GetSize() const = 0;
        [[nodiscard]] virtual u64 GetDeviceAddress() const = 0; // For bindless/ray tracing
        [[nodiscard]] virtual bool IsValid() const = 0;

        template <typename T>
        void Upload(Span<const T> span) const
        {
            Upload(span.data(), span.size_bytes());
        }

        template <typename T>
        void UploadObject(const T& object) const
        {
            Upload(&object, sizeof(T));
        }
    };

    struct GPUShaderBuffer : NoCopyMove
    {
        GPUShaderBuffer() = default;
        ~GPUShaderBuffer() override = default;

        virtual void UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) = 0;
        virtual void Bind(GPUCommandBuffer* cmd, GPUPipeline* pipeline, u32 frameIndex) = 0;
        virtual void Destroy() = 0;
    };

    struct GPUSampler : NoCopy
    {
        ~GPUSampler() override = default;
    };

    /// Abstract GPU texture view
    struct GPUTextureView : NoCopy
    {
        ~GPUTextureView() override = default;
        void SetName(const char* str);
    };

    struct GPUTexture : NoCopy
    {
        GPUTexture() = default;
        ~GPUTexture() override = default;

        virtual void Destroy() = 0;
        virtual void SetName(const std::string& name) = 0;
        virtual void UploadData(const void* data) = 0;

        virtual GPUTextureView* GetView(u32 layer = 0) = 0;

        TextureLayout currentLayout = TextureLayout::Unknown;
    };

    enum class DescriptorHeapType : u32
    {
        Resource,
        Sampler
    };

    struct DescriptorHeapDesc
    {
        DescriptorHeapType type;
        u32 maxDescriptors;
        const char* name = nullptr;
    };

    /// Abstract class for Descriptor Heaps
    struct GPUDescriptorHeap : NoCopyMove
    {
        GPUDescriptorHeap() = default;
        ~GPUDescriptorHeap() override = default;

        virtual void WriteBuffer(u32 index, const GPUBuffer* buffer, u64 offset, u64 range) = 0;
        virtual void WriteImage(u32 index, const GPUTextureView* texture, TextureLayout layout) = 0;
        virtual void WriteSampler(u32 index, const SamplerInfo& info) = 0;

        [[nodiscard]] virtual u64 GetDeviceAddress() const = 0;
        [[nodiscard]] virtual u32 GetDescriptorSize() const = 0;
        [[nodiscard]] virtual DescriptorHeapType GetType() const = 0;
    };

    /// Abstract descriptor set layout
    struct GPUDescriptorLayout
    {
        GPUDescriptorLayout() = default;
        virtual ~GPUDescriptorLayout() = default;

        virtual void Destroy() = 0;
    };

    /// Abstract descriptor set
    struct GPUDescriptorSet
    {
        GPUDescriptorSet() = default;
        virtual ~GPUDescriptorSet() = default;

        virtual void WriteBuffer(u32 binding, const GPUBuffer* buffer, DescriptorType type) = 0;
        virtual void WriteTexture(u32 binding, GPUTextureView* texture, GPUSampler* sampler, DescriptorType type,
                                  u32 arrayElement = 0) = 0;
        virtual void WriteTextureArray(u32 binding, Span<GPUTextureView*> textures, DescriptorType type) = 0;
        virtual void Update(GPUDevice* device) = 0;
    };

    struct GPUModel
    {
        AABB aabb;
        u64 vertexBufferAddress = 0;

        Vector<MeshPart> parts;
        Vector<Material> materials;
        std::unique_ptr<GPUBuffer> vertexBuffer;
        std::unique_ptr<GPUBuffer> indexBuffer;
        std::unique_ptr<GPUShaderBuffer> materialBuffer;

        void Destroy()
        {
            vertexBuffer.reset();
            indexBuffer.reset();
            materialBuffer.reset();

            parts.clear();
            materials.clear();
            vertexBufferAddress = 0;
            aabb = AABB{};
        }
    };

    /// Abstract shader module (SPIR-V, DXIL, etc.)
    struct GPUShader : NoCopyMove
    {
        GPUShader() = default;
        ~GPUShader() override = default;
    };

    /// Abstract graphics pipeline
    struct GraphicsPipelineDesc
    {
        std::shared_ptr<GPUShader> vertexShader;
        std::shared_ptr<GPUShader> fragmentShader;

        VertexInputLayout vertexLayout;
        GpuRasterDesc raster;
        PipelineLayoutDesc layout;

        std::string slangSourcePath;
    };

    /// Abstract compute pipeline
    struct ComputePipelineDesc
    {
        std::shared_ptr<GPUShader> computeShader;
        PipelineLayoutDesc layout;

        std::string slangSourcePath;
    };

    struct GPUPipeline : NoCopyMove
    {
        GPUPipeline() = default;
        ~GPUPipeline() override = default;

        virtual void Destroy() = 0;
        virtual void Rebuild() = 0;
        virtual void SetSampleCountAndRebuild(SampleCount samples) = 0;

        virtual constexpr explicit operator bool() const noexcept = 0; // IsValid() but....ya
        [[nodiscard]] virtual const PipelineLayoutDesc& GetLayoutDesc() const = 0;
    };


    struct GPUShaderManager : NoCopyMove
    {
        GPUShaderManager() = default;
        ~GPUShaderManager() override = default;

        virtual void Init(GPUDevice* device, ShaderCompiler* compiler) = 0;
        virtual void Destroy() = 0;
        virtual void CheckForReloads() = 0;
        virtual void RegisterPipeline(GPUPipeline* pipeline) = 0;
        virtual void UnregisterPipeline(GPUPipeline* pipeline) = 0;
    };


    struct GPUFence : NoCopyMove
    {
        GPUFence() = default;
        ~GPUFence() override = default;

        virtual void Reset() = 0;
    };

    struct GPUSemaphore : NoCopyMove
    {
        GPUSemaphore() = default;
        ~GPUSemaphore() override = default;
    };

    // --- Render Targets ---
    struct RenderAttachment
    {
        GPUTextureView* view = nullptr;
        GPUTextureView* resolveView = nullptr;

        LoadOP loadOp = LoadOP::Clear;
        StoreOP storeOp = StoreOP::Store;
        ResolveMode resolveMode = ResolveMode::Average;

        union {
            f32 color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            struct
            {
                f32 depth;
                u32 stencil;
            } ds;
        } clearValue;

        [[nodiscard]] static RenderAttachment Color(GPUTextureView* view, GPUTextureView* resolve = nullptr,
                                                    LoadOP load = LoadOP::Clear, const glm::vec4& col = {0, 0, 0, 1})
        {
            RenderAttachment ra = {
                .view = view,
                .resolveView = resolve,
                .loadOp = load,
                .storeOp = resolve ? StoreOP::DontCare : StoreOP::Store,
                .resolveMode = resolve ? ResolveMode::Average : ResolveMode::None
            };
            std::memcpy(ra.clearValue.color, &col[0], sizeof(f32) * 4);
            return ra;
        }

        [[nodiscard]] static RenderAttachment Depth(GPUTextureView* view, LoadOP load = LoadOP::Clear,
                                                    StoreOP store = StoreOP::DontCare, f32 depth = 0.0f, u32 stencil = 0)
        {
            RenderAttachment ra = {
                .view = view,
                .loadOp = load,
                .storeOp = store,
            };
            ra.clearValue.ds.depth = depth;
            ra.clearValue.ds.stencil = stencil;
            return ra;
        }
    };

    struct RenderingInfo
    {
        Extent2D extent = {};
        Span<RenderAttachment> colorAttachments;
        RenderAttachment* depthAttachment = nullptr;
        RenderAttachment* stencilAttachment = nullptr;
    };

    struct CommandBufferBeginInfo
    {
        bool oneTimeSubmit = true;
        const RenderingInfo* renderPassInfo = nullptr; // For secondary buffers
    };

    struct GPUCommandBuffer : NoCopyMove
    {
        GPUCommandBuffer() = default;
        ~GPUCommandBuffer() override = default;

        // Recording
        virtual void Begin(const CommandBufferBeginInfo* inheritanceInfo = nullptr) = 0;
        virtual void End() = 0;
        virtual void Reset() = 0;

        // Render pass
        virtual void BeginRendering(const RenderingInfo& info) = 0;
        virtual void EndRendering() = 0;

        // Pipeline & Descriptors
        virtual void BindPipeline(GPUPipeline* pipeline) = 0;
        virtual void BindDescriptorSet(const GPUDescriptorSet* set, u32 setIndex, GPUPipeline* pipeline) = 0;
        virtual void PushConstants(GPUPipeline* pipeline, ShaderStageFlags stages, u32 offset, u32 size,
                                   const void* data) = 0;

        // Resource Transitions
        virtual void FlushBarriers() = 0;
        virtual void TransitionLayout(GPUTexture* tex, TextureLayout newLayout) = 0;
        virtual void TransitionLayouts(Span<const TextureTransition> transitions) = 0;
        virtual void GenerateMipmaps(GPUTexture* texture) = 0;

        // Vertex/Index Buffers
        virtual void BindVertexBuffer(GPUBuffer* buffer, u32 binding, u64 offset) = 0;
        virtual void BindIndexBuffer(GPUBuffer* buffer, u64 offset) = 0;

        // Drawing
        virtual void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
                                 u32 firstInstance) = 0;
        virtual void DrawIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride) = 0;
        virtual void DrawIndexedIndirect(GPUBuffer* buffer, u64 offset, u32 drawCount, u32 stride) = 0;

        // Compute
        virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
        virtual void DispatchIndirect(GPUBuffer* buffer, u64 offset) = 0;

		// Copy operations
        virtual void CopyBuffer(GPUBuffer* src, GPUBuffer* dst, u64 size, u64 srcOffset, u64 dstOffset) = 0;
        virtual void CopyTexture(GPUTexture* src, GPUTexture* dst) = 0;
        virtual void CopyBufferToTexture(GPUBuffer* src, GPUTexture* dst) = 0;
        virtual void BlitTexture(GPUTexture* src, GPUTexture* dst) = 0;

        // Synchronization
        virtual void WaitForFence(GPUFence* fence) = 0;

        // Viewport/Scissor
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;

        // Debug markers
        virtual void BeginDebugLabel(const char* name, f32 r, f32 g, f32 b) = 0;
        virtual void EndDebugLabel() = 0;
        virtual void InsertDebugLabel(const char* name, f32 r, f32 g, f32 b) = 0;
    };

    struct FrameIndices
    {
        u32 frameIndex;
        u32 imageIndex;
    };

    /// Per-frame resources (command buffer, sync objects, descriptors)
    struct GPUFrameData : NoCopyMove
    {
        GPUFrameData() = default;
        ~GPUFrameData() override = default;

        virtual bool Init(GPUDevice* device) = 0;
        virtual void Destroy() = 0;

        [[nodiscard]] virtual GPUCommandBuffer* GetCommandBuffer() = 0;
        [[nodiscard]] virtual GPUQueryPool* GetQueryPool() = 0;
    };

    /// Abstract swapchain (platform-specific)
    struct GPUSwapchain : NoCopyMove
    {
        GPUSwapchain() = default;
        ~GPUSwapchain() override = default;

        virtual Result<void> Init(GPUDevice* devicePtr, Platform::WindowHandle windowHandle) = 0;
        virtual void Destroy() = 0;
        virtual bool ResizeIfNeeded() = 0;
        virtual void NeedsReCreation() = 0;

        // Upgraded: Returns the image index inside the Result instead of using an out-param
        [[nodiscard]] virtual Result<u32> AcquireNextImage(GPUSemaphore* semaphore) = 0;

        [[nodiscard]] virtual GPUTexture* GetImage(u32 index) = 0;
        [[nodiscard]] virtual GPUTexture* GetCurrentImage() = 0;
        [[nodiscard]] virtual f32 GetRenderScale() const = 0;
        virtual void SetRenderScale(f32 scale) = 0;

        [[nodiscard]] virtual Extent2D GetExtent() const = 0;
        [[nodiscard]] virtual Extent2D GetRenderExtent() const = 0;
        [[nodiscard]] virtual f32 GetAspectRatio() const = 0;
        [[nodiscard]] virtual void* GetNativeFormat() const { return nullptr; }
        [[nodiscard]] virtual PresentMode GetPresentMode() const = 0;
        [[nodiscard]] virtual Platform::WindowHandle GetWindowHandle() const = 0;

        virtual void SetVsyncMode(PresentMode mode) = 0;
        virtual void SetBufferingMode(BufferingMode mode) = 0;
    };

    /// High-level renderer (manages frames, submission)
    struct GPURenderer : NoCopyMove
    {
        GPURenderer() = default;
        ~GPURenderer() override = default;

        virtual bool Init(GPUDevice* device, GPUSwapchain* swapchain, u32 frameOverlap = MAX_FRAME_OVERLAP) = 0;
        virtual void Destroy() = 0;

        // Upgraded: Returns optional frame indices instead of booleans + out params
        [[nodiscard]] virtual GPUCommandBuffer* BeginFrame() = 0;
        virtual void EndFrame() = 0;

        [[nodiscard]] virtual GPUFrameData* GetCurrentFrameData() = 0;
        [[nodiscard]] virtual u32 GetFrameIndex() const = 0;
    };

    struct GPUInterface : NoCopyMove
    {
        GPUInterface() = default;
        ~GPUInterface() override = default;

        virtual bool Init() = 0;
        virtual void Destroy() = 0;
    };

    /// Abstract logical GPU device
    struct GPUDevice : NoCopyMove
    {
        GPUDevice() = default;
        ~GPUDevice() override = default;

        virtual bool Init(GPUInterface* instance) = 0;
        virtual void Destroy() = 0;
        virtual void WaitIdle() = 0;

        // Immediate submit
        virtual void ImmediateSubmit(std::function<void(GPUCommandBuffer*)> func) = 0;

        // Creations
        [[nodiscard]] virtual std::unique_ptr<GPUTexture> CreateTexture(const TextureInfo& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<GPUTextureView> CreateTextureView(
            GPUTexture* texture, const TextureViewInfo& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<GPUSampler> CreateSampler(SamplerInfo& info) = 0;

        [[nodiscard]] virtual std::shared_ptr<GPUShader> CreateShader(Span<const u32> code) = 0;
        [[nodiscard]] virtual std::shared_ptr<GPUShader> CreateShaderPath(const char* path) = 0;

        [[nodiscard]] virtual std::unique_ptr<GPUBuffer> CreateBuffer(BufferInfo& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<GPUShaderBuffer> CreateShaderBuffer(
            struct DescriptorAllocatorGrowable* alloc, const DescriptorSetLayoutDesc& desc) = 0;

        [[nodiscard]] virtual std::unique_ptr<GPUPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<GPUPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

        // Getters
        [[nodiscard]] virtual const GPUDeviceDesc& GetDeviceDesc() const = 0;
        [[nodiscard]] virtual SampleCount GetMaxUsableSampleCount() const = 0;

        SampleCount currentSamples = SampleCount::X4;
    };

    struct GPUQueryPool : NoCopyMove
    {
        GPUQueryPool() = default;
        ~GPUQueryPool() override = default;

        virtual void Reset(GPUCommandBuffer* cmd) = 0;
        virtual void WriteTimestamp(GPUCommandBuffer* cmd, u32 queryIndex) = 0;
        virtual bool FetchResults() = 0;

        [[nodiscard]] virtual f32 GetElapsedMs(u32 timerIndex) const = 0;
        [[nodiscard]] virtual f32 GetDeltaMs(u32 beginIdx, u32 endIdx) const = 0;
    };

    // Helper Function
    [[nodiscard]] constexpr std::string DecodeDriverVersion(u32 driverVersion, const GPUVendor vendor)
    {
        switch (vendor)
        {
        case GPUVendor::Nvidia:
            return fmt::format("{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 14) & 0xFF);
        case GPUVendor::Intel:
            return fmt::format("{}.{}", driverVersion >> 14, driverVersion & 0x3FFF);
        case GPUVendor::AMD:
            return fmt::format("{}.{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 12) & 0x3FF, driverVersion & 0xFFF);
        case GPUVendor::Apple:
            return fmt::format("{}", driverVersion);
        default:
            return fmt::format("Unknown Driver (0x{:X})", driverVersion);
        }
    }
} // namespace Renderer

