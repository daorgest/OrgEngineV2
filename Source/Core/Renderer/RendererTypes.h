//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <cassert>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include "Tools/Vector.h"


#define ENUM_CLASS_BITOPS(Enum) \
[[nodiscard]] constexpr Enum operator|(Enum a, Enum b) noexcept { return static_cast<Enum>(std::to_underlying(a) | std::to_underlying(b)); } \
[[nodiscard]] constexpr Enum operator&(Enum a, Enum b) noexcept { return static_cast<Enum>(std::to_underlying(a) & std::to_underlying(b)); } \
[[nodiscard]] constexpr Enum operator^(Enum a, Enum b) noexcept { return static_cast<Enum>(std::to_underlying(a) ^ std::to_underlying(b)); } \
[[nodiscard]] constexpr Enum operator~(Enum a) noexcept { return static_cast<Enum>(~std::to_underlying(a)); } \
constexpr Enum& operator|=(Enum& a, Enum b) noexcept { return a = (a | b); } \
constexpr Enum& operator&=(Enum& a, Enum b) noexcept { return a = (a & b); } \
constexpr Enum& operator^=(Enum& a, Enum b) noexcept { return a = (a ^ b); } \
[[nodiscard]] constexpr Enum operator<<(Enum a, int shift) noexcept { return static_cast<Enum>(std::to_underlying(a) << shift); } \
[[nodiscard]] constexpr Enum operator>>(Enum a, int shift) noexcept { return static_cast<Enum>(std::to_underlying(a) >> shift); } \
constexpr Enum& operator<<=(Enum& a, int shift) noexcept { return a = static_cast<Enum>(std::to_underlying(a) << shift); } \
constexpr Enum& operator>>=(Enum& a, int shift) noexcept { return a = static_cast<Enum>(std::to_underlying(a) >> shift); } \
[[nodiscard]] constexpr bool operator==(Enum a, std::underlying_type_t<Enum> u) noexcept { return std::to_underlying(a) == u; } \
[[nodiscard]] constexpr bool operator==(std::underlying_type_t<Enum> u, Enum a) noexcept { return u == std::to_underlying(a); } \
[[nodiscard]] constexpr bool operator!=(Enum a, std::underlying_type_t<Enum> u) noexcept { return std::to_underlying(a) != u; } \
[[nodiscard]] constexpr bool operator!=(std::underlying_type_t<Enum> u, Enum a) noexcept { return u != std::to_underlying(a); } \
[[nodiscard]] constexpr bool HasFlag(Enum value, Enum flag) noexcept { return (std::to_underlying(value) & std::to_underlying(flag)) != 0; } \
constexpr void SetFlag(Enum& value, Enum flag) noexcept { value |= flag; } \
constexpr void ClearFlag(Enum& value, Enum flag) noexcept { value &= ~flag; } \
constexpr void ToggleFlag(Enum& value, Enum flag) noexcept { value ^= flag; } \
[[nodiscard]] constexpr auto ToUnderlying(Enum e) noexcept { return std::to_underlying(e); }

template <typename E>
[[nodiscard]] constexpr bool HasAny(E value, E mask)
{
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) != 0;
}

template <typename E>
[[nodiscard]] constexpr bool HasAll(E value, E mask)
{
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) == static_cast<U>(mask);
}

struct Extent2D
{
    u32 width;
    u32 height;

    [[nodiscard]] bool IsZero() const { return width == 0 || height == 0; }
    bool operator==(const Extent2D& rhs) const { return width == rhs.width && height == rhs.height; }
};

struct Extent3D
{
    u32 width = 0;
    u32 height = 0;
    u32 depth = 1;
};

struct Viewport
{
    f32 x{}, y{}, width{}, height{}, minDepth = 0.f, maxDepth = 1.f;
};

// Present Modes

enum class PresentMode
{
    VSyncOn, // Synchronized to vertical blank (tearing-free)
    VSyncOff, // Present as fast as possible (tearing allowed)
    LowLatency, // Lower latency, minimizes tearing if supported
    Adaptive // VSync with more tolerance (optional, rare)
};

struct PresentModeInfo
{
    PresentMode mode;
    const char* label;
};

static constexpr PresentModeInfo kVsyncModes[] = {
    {PresentMode::VSyncOn, "VSync On"},
    {PresentMode::VSyncOff, "VSync Off"},
    {PresentMode::Adaptive, "Adaptive"},
    {PresentMode::LowLatency, "Mailbox"}
};


enum class BufferingMode : u32
{
    Double = 2,
    Triple = 3,
    Quad = 4
};

// Pipelines
enum class PrimitiveTopology : u8
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan
};

enum class PolygonMode : u8 { Fill, Line, Point };

enum class CullMode : u8 { None, Front, Back, FrontAndBack };

enum class FrontFace : u8 { CounterClockwise, Clockwise };

enum class SampleCount : u8 { X1, X2, X4, X8, X16, X32, X64 };

enum class StencilOp : u8
{
    Keep, // Don't change the value in the buffer
    Zero, // Set the value to 0
    Replace, // Put our "Reference" value into the buffer
    Increment, // Add 1 to the value
    Invert // Flip all bits (0 becomes 255)
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

// “Dynamic state” list is API-neutral; backends map to their own dynamic states.
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

// Meshes
enum class MeshSourceType
{
    Unknown,
    OBJ,
    OrgPack,
    FBX,
    Runtime
};

// Images
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
    BC2_UNORM_BLOCK, // BC2 compression supporting RGBA with explicit alpha
    BC3_UNORM_BLOCK, // BC3 compression (similar to BC2)
    BC4_UNORM_BLOCK, // BC4 compression for single-channel textures
    BC5_UNORM_BLOCK, // BC5 compression for two-channel textures (e.g., normal maps)
    BC6H_SFLOAT_BLOCK, // BC6H compression for HDR images
    BC7_UNORM_BLOCK, // BC7 compression for high-quality RGBA textures

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
    DepthWrite, // Stencil
    DepthReadOnly,
    CopySource,
    CopyDestination,
    ResolveSource,
    ResolveDestination,
    Present,
    Unknown
};

enum class DepthFormat
{
    D32_SFLOAT, // 32-bit floating-point Depth format
    D24_UNORM_S8_UINT, // 24-bit Depth with 8-bit Stencil
    S8_UINT, // 8-bit Stencil only (standalone)
    DEPTH_FORMAT_COUNT
};

enum class TextureDimension
{
    Texture1D,
    Texture2D,
    Texture3D,
    CubeMap,
    None
};


enum class TextureViewDimension
{
    Auto,
    Texture2D,
    Texture2DArray,
    Cube,
    CubeFace, // a single layer for a cubemap
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
};

ENUM_CLASS_BITOPS(ImageUsage)
using ImageUsageFlags = ImageUsage;


// Blending
enum class BlendFactor : u8
{
    Zero, // Use 0.0
    One, // Use 1.0
    SrcAlpha, // Use the alpha of the new pixel
    OneMinusSrcAlpha, // Use (1.0 - alpha) of the new pixel
    DstAlpha, // Use the alpha of the pixel already on screen
    OneMinusDstAlpha // Use (1.0 - alpha) of the pixel already on screen
};

struct GpuBlendDesc
{
    bool enabled = false;
    // Common setup: SrcAlpha (new) mixed with OneMinusSrcAlpha (old)
    BlendFactor srcFactor = BlendFactor::SrcAlpha;
    BlendFactor dstFactor = BlendFactor::OneMinusSrcAlpha;
};

struct GpuStencilDesc
{
    bool enabled = false;
    StencilOp passOp = StencilOp::Keep; // What to do if the test passes
    CompareOp compareOp = CompareOp::Always; // How to compare against the buffer
    u8 reference = 0; // The value we are checking/writing
};

struct GpuRasterDesc
{
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cull = CullMode::Back;

    TextureFormat depthFormat = TextureFormat::UNKNOWN;
    bool depthWrite = true; // Should we update the Z-buffer?
    CompareOp depthOp = CompareOp::Less; // Draw if closer to camera


    GpuBlendDesc blend;
    GpuStencilDesc stencil;

    SampleCount sampleCount = SampleCount::X1;
    bool alphaToCoverage = false;

    Vector<TextureFormat> colorFormats;

    // Presets
    static GpuRasterDesc Opaque3D(TextureFormat colorFormat, TextureFormat depthFormat)
    {
        GpuRasterDesc desc;
        desc.topology = PrimitiveTopology::TriangleList;
        desc.cull = CullMode::Back;

        // Depth: Write and Test are both ON
        desc.depthFormat = depthFormat;
        desc.depthWrite = true;
        desc.depthOp = CompareOp::GreaterOrEqual;

        // Blending: OFF (Standard for opaque)
        desc.blend.enabled = false;

        desc.colorFormats.push_back(colorFormat);
        return desc;
    }

    static GpuRasterDesc Transparent(TextureFormat colorFormat, TextureFormat depthFormat)
    {
        GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);

        // Depth: Test is ON, but Write is OFF
        desc.depthWrite = false;

        // Blending: Standard "Lerp" mix (SrcAlpha / OneMinusSrcAlpha)
        desc.blend.enabled = true;
        desc.blend.srcFactor = BlendFactor::SrcAlpha;
        desc.blend.dstFactor = BlendFactor::OneMinusSrcAlpha;

        return desc;
    }

    static GpuRasterDesc StencilWrite(TextureFormat colorFormat, TextureFormat depthFormat)
    {
        GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);

        desc.stencil.enabled = true;
        desc.stencil.compareOp = CompareOp::Always;
        desc.stencil.passOp = StencilOp::Replace;
        desc.stencil.reference = 1;

        return desc;
    }

    static GpuRasterDesc StencilRead(TextureFormat colorFormat, TextureFormat depthFormat)
    {
        GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);

        desc.stencil.enabled = true;
        // Only draw where the stencil IS NOT 1
        desc.stencil.compareOp = CompareOp::NotEqual;
        desc.stencil.reference = 1;

        return desc;
    }
};

// Samplers
enum class SamplerFilter
{
    Nearest,
    Linear
};

enum class SamplerMipFilter
{
    None, // No mipmapping (use base LOD)
    Nearest,
    Linear
};

enum class SamplerAddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

enum class SamplerBorderColor
{
    FloatTransparentBlack,
    FloatOpaqueBlack,
    FloatOpaqueWhite
};


struct SamplerInfo
{
    SamplerFilter minFilter = SamplerFilter::Linear;
    SamplerFilter magFilter = SamplerFilter::Linear;
    SamplerMipFilter mipFilter = SamplerMipFilter::Linear;

    SamplerAddressMode addressU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressW = SamplerAddressMode::Repeat;

    f32 mipLodBias = 0.0f;
    f32 minLod = FLT_MIN;
    f32 maxLod = FLT_MAX;

    u32 maxAnisotropy = 16;
    bool anisotropyEnable = false;
    bool compareEnable = false;
    bool unnormalizedCoords = false;
    SamplerBorderColor borderColor = SamplerBorderColor::FloatOpaqueBlack;
};

// Buffers
enum class GPUHeapType : u8 { Default, Upload, Readback, Unknown };

inline const char* GPUHeapTypeToString(GPUHeapType t)
{
    switch (t)
    {
    case GPUHeapType::Default: return "Default";
    case GPUHeapType::Upload: return "Upload";
    case GPUHeapType::Readback: return "Readback";
    default: return "Unknown";
    }
}

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
};

ENUM_CLASS_BITOPS(GPUBufferFlag)

inline std::string GPUBufferFlagsToString(GPUBufferFlag flags)
{
    std::string out;

    if (HasAny(flags, GPUBufferFlag::Vertex)) out += "VERTEX|";
    if (HasAny(flags, GPUBufferFlag::Index)) out += "INDEX|";
    if (HasAny(flags, GPUBufferFlag::Storage)) out += "STORAGE|";
    if (HasAny(flags, GPUBufferFlag::Constant)) out += "CONSTANT|";
    if (HasAny(flags, GPUBufferFlag::Indirect)) out += "INDIRECT|";
    if (HasAny(flags, GPUBufferFlag::ShaderDeviceAddress)) out += "SHADER_DEVICE_ADDRESS|";
    if (HasAny(flags, GPUBufferFlag::ShaderBindingTable)) out += "SHADER_BINDING_TABLE|";

    if (!out.empty()) out.pop_back(); // remove trailing '|'
    if (out.empty()) out = "NONE";

    return out;
}

enum class BufferPreset
{
    VertexGPU,
    VertexStorageGPU,
    IndexGPU,
    UniformHost,
    StagingUpload,
    StagingDownload,
    StorageGPU,
    StorageHostPersistent
};

struct BufferInfo
{
    u64 size = 0;
    GPUHeapType heapType = GPUHeapType::Unknown;
    GPUBufferFlag usage = GPUBufferFlag::None;
    bool commit = false;
    const char* name = nullptr;

    static BufferInfo FromPreset(const BufferPreset preset, const u64 size)
    {
        switch (preset)
        {
        case BufferPreset::VertexGPU:
            return {size, GPUHeapType::Upload, GPUBufferFlag::Vertex};

        case BufferPreset::VertexStorageGPU:
            return {
                size, GPUHeapType::Upload,
                GPUBufferFlag::Vertex | GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress
            };

        case BufferPreset::IndexGPU:
            return {size, GPUHeapType::Default, GPUBufferFlag::Index};

        case BufferPreset::UniformHost:
            return {size, GPUHeapType::Upload, GPUBufferFlag::Constant, true};

        case BufferPreset::StagingUpload:
            return {size, GPUHeapType::Upload, GPUBufferFlag::None, true};

        case BufferPreset::StagingDownload:
            return {size, GPUHeapType::Readback, GPUBufferFlag::None, true};

        case BufferPreset::StorageGPU:
            return {size, GPUHeapType::Default, GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress};

        case BufferPreset::StorageHostPersistent:
            return {size, GPUHeapType::Upload, GPUBufferFlag::Storage, true};

        default:
            assert(false && "Unknown BufferPreset");
            return {};
        }
    }
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
    Fragment = 1 << 1,
    Compute = 1 << 2,
    RayGen = 1 << 3,
    AnyHit = 1 << 4,
    ClosestHit = 1 << 5,
    Miss = 1 << 6,
    Callable = 1 << 7,

    AllGraphics = Vertex | Fragment,
    All = 0xFFFFFFFF
};

ENUM_CLASS_BITOPS(ShaderStage)
using ShaderStageFlags = ShaderStage;

inline std::string ToString(ShaderStage stage)
{
    if (stage == ShaderStage::None) return "None";
    if (stage == ShaderStage::All) return "All";

    Vector<const char*> parts;
    if (u32(stage) & u32(ShaderStage::Vertex)) parts.push_back("Vertex");
    if (u32(stage) & u32(ShaderStage::Fragment)) parts.push_back("Fragment");
    if (u32(stage) & u32(ShaderStage::Compute)) parts.push_back("Compute");
    if (u32(stage) & u32(ShaderStage::RayGen)) parts.push_back("RayGen");
    if (u32(stage) & u32(ShaderStage::AnyHit)) parts.push_back("AnyHit");
    if (u32(stage) & u32(ShaderStage::ClosestHit)) parts.push_back("ClosestHit");
    if (u32(stage) & u32(ShaderStage::Miss)) parts.push_back("Miss");
    if (u32(stage) & u32(ShaderStage::Callable)) parts.push_back("Callable");

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        result += parts[i];
        if (i < parts.size() - 1) result += " | ";
    }
    return result.empty() ? "Unknown" : result;
}

enum class ShaderFormat
{
    UNKNOWN,
    DXIL,
    SPIRV,
};

// RenderPass
enum class LoadOP
{
    Load,
    Clear,
    DontCare
};

enum class StoreOp
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

struct GPUDeviceDesc
{
    std::string name = "Unknown";
    GPUDeviceType type = GPUDeviceType::Unknown;
    GPUVendor vendor = GPUVendor::UNKNOWN;
    u64 driverVersion = 0;
    u64 dedicatedVideoMemory = 0;
    std::string driverVersionString;
    std::string apiName;
};

// NOTE: ImageType is 2D by default
struct TextureInfo
{
    Extent3D extent = {};
    u16 mipLevels = 1;
    u16 arrayLayers = 1;
    ImageType type = ImageType::Image2D;
    TextureFormat format = TextureFormat::UNKNOWN;
    TextureDimension dimension = TextureDimension::Texture2D;
    ImageUsageFlags usage = ImageUsage::None;
    SampleCount sampleCount = SampleCount::X1;

    void EnableMipmaps()
    {
        const u32 size = std::max(extent.width, extent.height);
        mipLevels = static_cast<u16>(std::floor(std::log2(static_cast<f32>(size)))) + 1;
    }
};

struct TextureSwizzleMask
{
    TextureSwizzle r = TextureSwizzle::Identity;
    TextureSwizzle g = TextureSwizzle::Identity;
    TextureSwizzle b = TextureSwizzle::Identity;
    TextureSwizzle a = TextureSwizzle::Identity;
};

struct TextureViewInfo
{
    TextureFormat format = TextureFormat::UNKNOWN;
    TextureViewDimension dimension = TextureViewDimension::Auto;
    u32 baseMip = 0; // which mip to start at
    u32 mipCount = 1; // how many mips

    u32 baseLayer = 0; // which layer to start at
    u32 arrayCount = 1; // how many layers

    TextureSwizzleMask swizzle = {};
};

struct TextureData
{
    i32 width = 0;
    i32 height = 0;
    i32 depth = 1;
    i32 channels = 4;
    TextureFormat format = TextureFormat::UNKNOWN;
    std::variant<Vector<u8>, Vector<f32>> data; // Raw pixel data for basic textures or float data for HDR
};

// Descriptor Binding
struct Binding
{
    u32 binding = 0;
    DescriptorType type = DescriptorType::UniformBuffer;
    ShaderStageFlags stageFlags = ShaderStage::None;

    size_t size = 0; // UBO's / SSBO's, otherwise 0

    // bindless stuff
    u32 count = 1;
    bool isBindless = false;
};


struct DescriptorSetLayoutDesc
{
    u32 setIndex = 0;
    Vector<Binding> bindings; // resource bindings
};

struct PushConstantDesc
{
    u32 size = 0;
    u32 offset = 0;
    ShaderStage stages = ShaderStage::None;
};

struct PipelineLayoutDesc
{
    Vector<DescriptorSetLayoutDesc> setLayouts;
    Vector<PushConstantDesc> pushConstants;
};


// Vertex input

enum class VertexInputRate
{
    Vertex,
    Instance
};

struct VertexBindingDescription
{
    u32 binding = 0;
    u32 stride = 0;
    VertexInputRate inputRate = VertexInputRate::Vertex;
};

struct VertexAttributeDescription
{
    // Vulkan-centric
    u32 location = 0;

    // Common
    u32 binding = 0;
    TextureFormat format = TextureFormat::UNKNOWN;
    u32 offset = 0;
};

struct VertexInputLayout
{
    Vector<VertexBindingDescription> bindings;
    Vector<VertexAttributeDescription> attributes;
};

enum class DebugView : i32
{
    Material = 0,
    Albedo,
    Normal,
    Depth,          // Reconstructed Linear Depth from Reverse-Z
    Roughness,      // Check for "shiny" vs "matte" bugs
    Metallic,       // Identify dielectric vs conductor issues
    WorldPos,       // Useful for debugging world-space effects/lighting
    UVs,
};

struct DebugViewMode
{
    DebugView value;
    const char* label;
};

inline constexpr DebugViewMode kDebugViews[] = {
    {DebugView::Material,  "Material (Lit)"},
    {DebugView::Albedo,    "Albedo (Unlit)"},
    {DebugView::Normal,    "Normals (World Space)"},
    {DebugView::Depth,     "Depth (Linearized)"},
    {DebugView::Roughness, "Roughness"},
    {DebugView::Metallic,  "Metallic"},
    {DebugView::WorldPos,  "World Position"},
    {DebugView::UVs,       "UV Coordinates"},
};
constexpr i32 debugViewCount = std::size(kDebugViews);

inline const char* DebugViewToString(DebugView v)
{
    for (const auto& [value, label] : kDebugViews) if (value == v) return label;
    return "Unknown";
}
