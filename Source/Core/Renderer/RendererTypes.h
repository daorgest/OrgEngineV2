//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <cassert>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <type_traits>

#include "RendererTypes.h"
#include "Tools/Vector.h"

#define ENUM_CLASS_BITOPS(Enum)                                                        \
[[nodiscard]] constexpr Enum operator|(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) | std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator&(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) & std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator^(Enum a, Enum b) noexcept {                    \
return static_cast<Enum>(std::to_underlying(a) ^ std::to_underlying(b)); }         \
[[nodiscard]] constexpr Enum operator~(Enum a) noexcept {                            \
return static_cast<Enum>(~std::to_underlying(a)); }                                 \
constexpr Enum& operator|=(Enum& a, Enum b) noexcept { return a = (a | b); }         \
constexpr Enum& operator&=(Enum& a, Enum b) noexcept { return a = (a & b); }         \
constexpr Enum& operator^=(Enum& a, Enum b) noexcept { return a = (a ^ b); }         \
/* == / != against underlying integer type */                                        \
[[nodiscard]] constexpr bool operator==(Enum a, std::underlying_type_t<Enum> u) noexcept { \
return std::to_underlying(a) == u; }                                               \
[[nodiscard]] constexpr bool operator==(std::underlying_type_t<Enum> u, Enum a) noexcept { \
return u == std::to_underlying(a); }                                               \
[[nodiscard]] constexpr bool operator!=(Enum a, std::underlying_type_t<Enum> u) noexcept { \
return std::to_underlying(a) != u; }                                               \
[[nodiscard]] constexpr bool operator!=(std::underlying_type_t<Enum> u, Enum a) noexcept { \
return u != std::to_underlying(a); }

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
	RelaxedVSync // VSync with more tolerance (optional, rare)
};

static constexpr std::pair<PresentMode, const char*> kVsyncModes[] = {
	{ PresentMode::VSyncOn,  "VSync On"  },
	{ PresentMode::VSyncOff, "VSync Off" },
	{ PresentMode::RelaxedVSync,  "Adaptive"  },
	{ PresentMode::LowLatency,  "Mailbox"  }
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

	R16_SFLOAT, // 16-bit floating-point Red channel A
	RG16_SFLOAT, // 16-bit floating-point Red and Green channels
	RGBA16_SFLOAT, // 16-bit floating-point Red, Green, Blue, Alpha channels
	R32_SFLOAT, // 32-bit floating-point Red channel
	RG32_SFLOAT, // 32-bit floating-point Red and Green channels
	RGB32_SFLOAT, // 32-bit floating-point Red, Green, Blue channels
	RGBA32_SFLOAT, // 32-bit floating-point Red, Green, Blue, Alpha channels

	R10G10B10A2_UNORM, // 10-bit color per channel + 2-bit alpha
	R11G11B10_UFLOAT, // HDR color (good for skyboxes, light buffers)

	BC1_RGB_UNORM_BLOCK, // BC1 compression for RGB textures
	BC1_RGBA_UNORM_BLOCK, // BC1 compression for RGBA textures
	BC2_UNORM_BLOCK, // BC2 compression supporting RGBA with explicit alpha (more control)
	BC3_UNORM_BLOCK, // BC3 compression (similar to BC2)
	BC4_UNORM_BLOCK, // BC4 compression for single-channel textures
	BC5_UNORM_BLOCK, // BC5 compression for two-channel textures (e.g., normal maps)
	BC6H_SFLOAT_BLOCK, // BC6H compression for high dynamic range (HDR) images
	BC7_UNORM_BLOCK, // BC7 compression for high-quality RGBA textures

	D16_UNORM, // 16-bit depth
	D32_SFLOAT, // 32-bit floating-point depth
	D24_UNORM_S8_UINT, // 24-bit depth, 8-bit stencil
	D32_SFLOAT_S8_UINT, // 32-bit floating-point depth, 8-bit stencil

	IMAGE_FORMAT_UNKNOWN,
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

enum class ImageType : u32
{
	Image2D,
	CubeMap
};


namespace ImageUsage
{
	enum Flags : u32
	{
		None            = 0,
		TransferSrc     = 1 << 0,
		TransferDst     = 1 << 1,
		Sampled         = 1 << 2,
		ColorAttachment = 1 << 3,
		DepthStencil    = 1 << 4,
		Storage         = 1 << 5,
		InputAttachment = 1 << 6,
		ResolveDst      = 1 << 7,
		ResolveSrc      = 1 << 8,
	};
}

using ImageUsageFlags = u32;


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


struct SamplerDesc
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

	bool anisotropyEnable = false;
	u32 maxAnisotropy = 16;
	bool compareEnable = false;
	SamplerBorderColor borderColor = SamplerBorderColor::FloatOpaqueBlack;
	bool unnormalizedCoords = false;
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

struct GPUBufferInfo
{
	u64 size = 0;
	GPUHeapType heapType = GPUHeapType::Unknown;
	GPUBufferFlag usage = GPUBufferFlag::None;
	bool commit = false;
	const char* name = nullptr;

	static GPUBufferInfo FromPreset(const BufferPreset preset, const u64 size)
	{
		switch (preset)
		{
		case BufferPreset::VertexGPU:
			return {size, GPUHeapType::Upload, GPUBufferFlag::Vertex};

		case BufferPreset::VertexStorageGPU:
			return {size, GPUHeapType::Upload, GPUBufferFlag::Vertex | GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress};

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
};

// NOTE: ImageType is 2D by default
struct TextureInfo
{
	Extent3D extent = {};
	u16 mipLevels = 1;
	u16 arrayLayers = 1;
	ImageType type = ImageType::Image2D;
	TextureFormat format = TextureFormat::IMAGE_FORMAT_UNKNOWN;
	TextureDimension dimension = TextureDimension::Texture2D;
	ImageUsageFlags usage = ImageUsage::None;

	void EnableMipmaps()
	{
		const u32 size = std::max(extent.width, extent.height);
		mipLevels = static_cast<u16>(std::floor(std::log2(static_cast<f32>(size)))) + 1;
	}
};

struct TextureData
{
	i32 width = 0;
	i32 height = 0;
	i32 depth = 1;
	i32 channels = 4;
	TextureFormat format = TextureFormat::IMAGE_FORMAT_UNKNOWN;
	Vector<u8> data;
};

// Scene BS

// Descriptor Binding
struct Binding
{
	u32 binding = 0;
	DescriptorType type = DescriptorType::UniformBuffer;
	size_t size = 0;
};

struct UniformBufferDesc
{
	ShaderStageFlags stageFlags; // shader stages
	Vector<Binding> bindings; // resource bindings
};

enum class DebugView : i32
{
	Material = 0,
	Albedo   = 1,
	Normal   = 2,
	DepthRaw = 3,   // hardware depth buffer
	DepthLin = 4,   // linearized depth (world units)
	DepthLog = 5,   // log-mapped depth for visualization
	UVs      = 6,
};

struct DebugViewItem
{
	DebugView value;
	const char* label;
};

inline constexpr DebugViewItem kDebugViews[] = {
	{DebugView::Material, "Material"},
	{DebugView::Albedo, "Albedo"},
	{DebugView::Normal, "Normals (mapped)"},
	{DebugView::DepthRaw, "Depth (raw)"},
	{DebugView::DepthLin, "Depth (linear)"},
	{DebugView::DepthLog, "Depth (log)"},
	{DebugView::UVs, "UVs"},
};

inline const char* DebugViewToString(DebugView v)
{
	for (const auto& [value, label] : kDebugViews) if (value == v) return label;
	return "Unknown";
}
