//
// Created by Orgest on 5/21/2026.
//

#pragma once
#include "../PrimTypes.h"
#include "../Tools/EnumBitmask.h"
namespace Renderer
{
    enum class PresentMode : u8
    {
        VSyncOn = 0,
        VSyncOff,
        LowLatency,
        Adaptive
    };

    enum class RenderPath : u32
    {
        Standard,
        Instance,
        Indirect
    };

    enum class SampleCount : u8
    {
        X1 = 1,
        X2 = 2,
        X4 = 4,
        X8 = 8,
        X16 = 16,
        X32 = 32,
        X64 = 64
    };

    enum class BufferingMode : u8
    {
        Double = 2,
        Triple = 3,
        Quad = 4
    };

    enum class PipelineType : u8
    {
        Graphics,
        Compute,
        Raytracing
    };


    enum class PrimitiveTopology : u8
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan
    };

    enum class PolygonMode : u8
    {
        Fill,
        Line,
        Point
    };

    enum class CullMode : u8
    {
        None,
        Front,
        Back,
        FrontAndBack
    };

    enum class FrontFace : u8
    {
        CounterClockwise,
        Clockwise
    };

    enum class StencilOp : u8
    {
        Keep,
        Zero,
        Replace,
        Increment,
        Invert
    };

    enum class CompareOp : u8
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class DynamicState : u8
    {
        Viewport,
        Scissor,
        LineWidth,
        DepthBias,
        BlendConstants,
        DepthBounds,
        StencilCompareMask,
        StencilWriteMask,
        StencilReference
    };


    enum class MeshSourceType
    {
        Unknown,
        OBJ,
        OrgPack,
        FBX,
        GLTF,
        Runtime
    };


    enum class TextureFormat
    {
        R8_UNORM, // 8-bit Red channel, normalized [0,1]
        RG8_UNORM, // 8-bit Red and Green channels, normalized
        RGB8_UNORM, // 8-bit Red, Green, Blue channels, normalized
        RGBA8_UNORM, // 8-bit Red, Green, Blue, Alpha channels, normalized
        BGRA8_UNORM, // 8-bit Blue, Green, Red, Alpha channels, normalized

        R8_SRGB, // 8-bit Red channel, sRGB
        RG8_SRGB, // 8-bit Red and Green channels, sRGB
        RGB8_SRGB, // 8-bit Red, Green, Blue channels, sRGB
        RGBA8_SRGB, // 8-bit Red, Green, Blue, Alpha channels, sRGB
        BGRA8_SRGB, // 8-bit Blue, Green, Red, Alpha channels, sRGB

        R8_UINT, // 8-bit unsigned integer Red channel
        RG8_UINT, // 8-bit unsigned integer Red and Green channels
        RGBA8_UINT, // 8-bit unsigned integer Red, Green, Blue, Alpha channels
        R16_UINT, // 16-bit unsigned integer Red channel
        RG16_UINT, // 16-bit unsigned integer Red and Green channels
        RGB16_SFLOAT, // 16-bit unsigned integer Red, Green and Blue channels
        RGBA16_UINT, // 16-bit unsigned integer Red, Green, Blue, Alpha channels
        R32_UINT, // 32-bit unsigned integer Red channel
        RG32_UINT, // 32-bit unsigned integer Red and Green channels
        RGBA32_UINT, // 32-bit unsigned integer Red, Green, Blue, Alpha channels

        R8_SINT, // 8-bit signed integer Red channel
        RG8_SINT, // 8-bit signed integer Red and Green channels
        RGBA8_SINT, // 8-bit signed integer Red, Green, Blue, Alpha channels
        R16_SINT, // 16-bit signed integer Red channel
        RG16_SINT, // 16-bit signed integer Red and Green channels
        RGBA16_SINT, // 16-bit signed integer Red, Green, Blue, Alpha channels
        R32_SINT, // 32-bit signed integer Red channel
        RG32_SINT, // 32-bit signed integer Red and Green channels
        RGBA32_SINT, // 32-bit signed integer Red, Green, Blue, Alpha channels

        R16_SFLOAT, // 16-bit floating-point Red channel
        RG16_SFLOAT, // 16-bit floating-point Red and Green channels
        RGBA16_SFLOAT, // 16-bit floating-point Red, Green, Blue, Alpha channels
        R32_SFLOAT, // 32-bit floating-point Red channel
        RG32_SFLOAT, // 32-bit floating-point Red and Green channels
        RGB32_SFLOAT, // 32-bit floating-point Red, Green, Blue channels
        RGBA32_SFLOAT, // 32-bit floating-point Red, Green, Blue, Alpha channels

        R10G10B10A2_UNORM, // 10-bit color per channel + 2-bit alpha
        R11G11B10_UFLOAT, // HDR color (good for skyboxes, light buffers)
        R9G9B9E5_UFLOAT, // HDR RGB with shared exponent, common for light probes

        // Compressed formats (BC / ETC / ASTC)
        BC1_RGB_UNORM_BLOCK, // BC1 compression for RGB textures
        BC1_RGBA_UNORM_BLOCK, // BC1 compression for RGBA textures
        BC1_RGBA_SRGB_BLOCK, // Standard for many legacy albedo maps
        BC2_UNORM_BLOCK, // BC2 compression supporting RGBA with explicit alpha
        BC3_UNORM_BLOCK, // BC3 compression (similar to BC2)
        BC3_SRGB_BLOCK,      // Standard for albedo maps with transparency
        BC4_UNORM_BLOCK, // BC4 compression for single-channel textures
        BC5_UNORM_BLOCK, // BC5 compression for two-channel textures (e.g., normal maps)
        BC6H_SFLOAT_BLOCK, // BC6H compression for HDR images
        BC7_UNORM_BLOCK, // BC7 compression for high-quality RGBA textures
        BC7_SRGB_BLOCK,

        ETC2_RGB8_UNORM_BLOCK, // ETC2 compression for RGB (mobile)
        ETC2_RGBA8_UNORM_BLOCK, // ETC2 compression for RGBA (mobile)
        ASTC_4x4_UNORM_BLOCK, // ASTC compression, 4x4 block (desktop/mobile)
        ASTC_8x8_UNORM_BLOCK, // ASTC compression, 8x8 block (high compression)

        // Depth / Stencil formats
        D16_UNORM, // 16-bit depth
        D24_UNORM_S8_UINT, // 24-bit depth, 8-bit stencil
        D32_SFLOAT, // 32-bit floating-point depth
        D32_SFLOAT_S8_UINT, // 32-bit floating-point depth, 8-bit stencil

        UNKNOWN,
        IMAGE_FORMAT_COUNT
    };

    enum class TextureLayout
    {
        General,
        ShaderReadOnly,
        ColorWrite,
        DepthWrite,
        DepthReadOnly,
        CopySource,
        CopyDestination,
        ResolveSource,
        ResolveDestination,
        Present,
        Unknown
    };

    [[nodiscard]] constexpr std::string_view ToString(TextureLayout layout) noexcept
    {
        switch (layout)
        {
        case TextureLayout::General:          return "General";
        case TextureLayout::ShaderReadOnly:   return "ShaderReadOnly";
        case TextureLayout::ColorWrite:       return "ColorWrite";
        case TextureLayout::DepthWrite:       return "DepthWrite";
        case TextureLayout::DepthReadOnly:    return "DepthReadOnly";
        case TextureLayout::CopySource:       return "CopySource";
        case TextureLayout::CopyDestination:  return "CopyDestination";
        case TextureLayout::ResolveSource:    return "ResolveSource";
        case TextureLayout::ResolveDestination: return "ResolveDestination";
        case TextureLayout::Present:          return "Present";
        case TextureLayout::Unknown:          return "Unknown";
        default:                              return "Invalid";
        }
    }

    enum class DepthFormat
    {
        D32_SFLOAT,
        D24_UNORM_S8_UINT,
        S8_UINT,
        DEPTH_FORMAT_COUNT
    };

    enum class TextureDimension : u8
    {
        Texture1D,
        Texture2D,
        Texture3D,
        CubeMap,
        None
    };

    enum class TextureViewDimension : u8
    {
        Auto,
        Texture2D,
        Texture2DArray,
        Cube,
        CubeFace,
        Texture3D
    };

    enum class TextureSwizzle : u8
    {
        Identity = 0,
        Zero,
        One,
        R,
        G,
        B,
        A
    };

    enum class ImageType : u32
    {
        Image2D,
        Image3D,
        CubeMap
    };

    enum class ImageUsage : u32
    {
        None = 0,
        TransferSrc = 1 << 0,
        TransferDst = 1 << 1,
        Sampled = 1 << 2,
        ColorAttachment = 1 << 3,
        DepthStencil = 1 << 4,
        Storage = 1 << 5,
        InputAttachment = 1 << 6,
        ResolveDst = 1 << 7,
        ResolveSrc = 1 << 8,
        Transient = 1 << 9,
    };

    enum class ResolveMode : u8
    {
        None,
        Average,
        Min,
        Max,
        SampleZero
    };

    enum class BlendFactor : u8
    {
        Zero,
        One,
        SrcAlpha, // Use the alpha of the new pixel
        OneMinusSrcAlpha, // Use (1.0 - alpha) of the new pixel
        DstAlpha, // Use the alpha of the pixel already on screen
        OneMinusDstAlpha // Use (1.0 - alpha) of the pixel already on screen
    };


    enum class SamplerFilter : u8
    {
        Nearest,
        Linear
    };

    enum class SamplerMipFilter : u8
    {
        None,
        Nearest,
        Linear
    };

    enum class SamplerAddressMode : u8
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class SamplerBorderColor : u8
    {
        FloatTransparentBlack,
        FloatOpaqueBlack,
        FloatOpaqueWhite
    };

    // For Descriptor Heaps
    enum class GPUHeapType : u8
    {
        Default,
        Upload,
        Readback,
        Unknown
    };


    enum class GPUBufferFlag : u32
    {
        None = 0,
        Vertex = 1 << 0,
        Index = 1 << 1,
        Storage = 1 << 2,
        Constant = 1 << 3,
        ShaderDeviceAddress = 1 << 4,
        ShaderBindingTable = 1 << 5,
        Indirect = 1 << 6,
        DescriptorHeap = 1 << 7
    };

    enum class BufferPreset
    {
        VertexGPU,
        VertexStorageGPU,
        IndexGPU,
        UniformHost,
        StagingUpload,
        StagingDownload,
        StorageGPU,
        StorageHostPersistent,
        SamplerHeapGPU,
        ResourceHeapGPU,
        IndirectHost,
        IndirectGPU
    };

    enum class DescriptorType
    {
        Unknown,
        Sampler,
        SampledImage,
        CombinedImageSampler,
        StorageImage,
        UniformBuffer,
        StorageBuffer,
        InputAttachment,
        AccelerationStructure
    };

    enum class ShaderStage : u32
    {
        None = 0,
        Vertex = 1 << 0,
        TessControl = 1 << 1,
        TessEvaluation = 1 << 2,
        Geometry = 1 << 3,
        Fragment = 1 << 4,
        Compute = 1 << 5,
        Task = 1 << 6,
        Mesh = 1 << 7,
        RayGen = 1 << 8,
        AnyHit = 1 << 9,
        ClosestHit = 1 << 10,
        Miss = 1 << 11,
        Intersection = 1 << 12,
        Callable = 1 << 13,
        AllGraphics = Vertex | Fragment,
        All = 0x7FFFFFFF
    };

    enum class ShaderFormat
    {
        UNKNOWN,
        DXIL,
        SPIRV
    };

    enum class LoadOP : u8
    {
        Load,
        Clear,
        DontCare
    };

    enum class StoreOP : u8
    {
        Store,
        DontCare
    };

    enum class GPUVendor
    {
        UNKNOWN = 0x0,
        AMD = 0x1002,
        Nvidia = 0x10DE,
        Intel = 0x8086,
        Apple = 0x106B
    };

    enum class GPUDeviceType
    {
        Unknown,
        Integrated,
        Discrete,
        Virtual,
        CPU
    };

    enum class MappingSourceType
    {
        ConstantOffset,
        PushIndex
    };

    enum class VertexInputRate
    {
        Vertex,
        Instance
    };

    enum class CommandBufferLevel : u8
    {
        Primary,
        Secondary
    };

    enum class DebugView : i32
    {
        Material = 0,
        Albedo,
        Normal,
        Depth,
        Roughness,
        Metallic,
        WorldPos,
        UVs,
        Mip,
        ClayRender,
        Validation,
        InstanceID
    };
    

    using ImageUsageFlags = ImageUsage;
    using ShaderStageFlags = ShaderStage;


    // UI & String Mapping Information (Inline Constexpr for headers)
    struct PresentModeInfo
    {
        PresentMode mode;
        const char* label;
    };

    inline constexpr PresentModeInfo kVsyncModes[] = {
        {PresentMode::VSyncOn, "VSync On"},
        {PresentMode::VSyncOff, "VSync Off"},
        {PresentMode::Adaptive, "Adaptive"},
        {PresentMode::LowLatency, "Mailbox"}
    };

    struct MSAAModeInfo
    {
        SampleCount count;
        const char* label;
    };

    inline constexpr MSAAModeInfo kMSAAModes[] = {
        {SampleCount::X1, "1x"},
        {SampleCount::X2, "2x"},
        {SampleCount::X4, "4x"},
        {SampleCount::X8, "8x"},
        {SampleCount::X16, "16x"},
        {SampleCount::X32, "32x"},
        {SampleCount::X64, "64x"}
    };

    struct DebugViewMode
    {
        DebugView value;
        const char* label;
    };

    inline constexpr DebugViewMode kDebugViews[] = {
        {DebugView::Material, "Material (Lit)"},
        {DebugView::Albedo, "Albedo (Unlit)"},
        {DebugView::Normal, "Normals (World Space)"},
        {DebugView::Depth, "Depth (Linearized)"},
        {DebugView::Roughness, "Roughness"},
        {DebugView::Metallic, "Metallic"},
        {DebugView::WorldPos, "World Position"},
        {DebugView::UVs, "UV Coordinates"},
        {DebugView::Mip, "Mipmap View"},
        {DebugView::ClayRender, "Clay Render (Lighting Only)"},
        {DebugView::Validation, "NaN / Validation Detector"},
        {DebugView::InstanceID, "Instance ID Randomizer"}
    };
    inline constexpr i32 kDebugViewCount = std::size(kDebugViews);

    // =========================================================================
    // Utility String Conversions
    // =========================================================================

    [[nodiscard]] inline std::string GPUBufferFlagsToString(GPUBufferFlag flags)
    {
        std::string out;
        out.reserve(64);

        if (HasAny(flags, GPUBufferFlag::Vertex))    out += "Vertex|";
        if (HasAny(flags, GPUBufferFlag::Index))     out += "Index|";
        if (HasAny(flags, GPUBufferFlag::Storage))   out += "Storage|";
        if (HasAny(flags, GPUBufferFlag::Constant))  out += "Constant|";
        if (HasAny(flags, GPUBufferFlag::Indirect))  out += "Indirect|";
        if (HasAny(flags, GPUBufferFlag::ShaderDeviceAddress)) out += "BDA|";
        if (HasAny(flags, GPUBufferFlag::ShaderBindingTable))  out += "SBT|";
        if (HasAny(flags, GPUBufferFlag::DescriptorHeap))      out += "DescHeap|";

        if (out.empty()) return "None";

        out.pop_back(); // Remove the last '|'
        return out;
    }

    [[nodiscard]] constexpr std::string_view GPUHeapTypeToString(GPUHeapType t)
    {
        switch (t)
        {
        case GPUHeapType::Default: return "Default";
        case GPUHeapType::Upload: return "Upload";
        case GPUHeapType::Readback: return "Readback";
        default: return "Unknown";
        }
    }

    [[nodiscard]] constexpr const char* DebugViewToString(DebugView v)
    {
        for (const auto& [value, label] : kDebugViews)
        {
            if (value == v) return label;
        }
        return "Unknown";
    }
}

template <>
inline constexpr bool EnableBitmask<Renderer::ImageUsage> = true;

template <>
inline constexpr bool EnableBitmask<Renderer::GPUBufferFlag> = true;

template <>
inline constexpr bool EnableBitmask<Renderer::ShaderStage> = true;