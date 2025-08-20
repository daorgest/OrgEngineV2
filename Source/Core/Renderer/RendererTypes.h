//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <cassert>
#include <cmath>
#include <string>
#include <type_traits>

#include "PrimTypes.h"
#include "Vector.h"

template <typename E>
constexpr bool HasFlag(E value, E flag)
{
	using UT = std::underlying_type_t<E>;
	return (static_cast<UT>(value) & static_cast<UT>(flag)) != 0;
}

// Present Modes

enum class PresentMode
{
	VSyncOn,         // Synchronized to vertical blank (tearing-free)
	VSyncOff,        // Present as fast as possible (tearing allowed)
	LowLatency,      // Lower latency, minimizes tearing if supported
	RelaxedVSync     // VSync with more tolerance (optional, rare)
};

enum class BufferingMode : u32
{
	Double = 2,
	Triple = 3,
	Quad   = 4
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

namespace ImageUsage
{
	enum Enum : u32
	{
		NONE = 0,
		TRANSFER_SRC = 1 << 0, // Source for copy operations
		TRANSFER_DST = 1 << 1, // Destination for copy operations
		SAMPLED = 1 << 2, // Sampled texture
		COLOR_ATTACHMENT = 1 << 3, // Render target
		DEPTH_STENCIL_ATTACHMENT = 1 << 4, // Depth/stencil buffer
		STORAGE = 1 << 5, // Storage image
		INPUT_ATTACHMENT = 1 << 6, // Subpass input
		RESOLVE_DST = 1 << 7, // Resolve operation destination
		RESOLVE_SRC = 1 << 8, // Resolve operation source
	};
};

typedef u32 ImageUsageFlags;

enum class DepthFormat
{
	D32_SFLOAT, // 32-bit floating-point Depth format
	D24_UNORM_S8_UINT, // 24-bit Depth with 8-bit Stencil
	S8_UINT, // 8-bit Stencil only (standalone)
	DEPTH_FORMAT_COUNT
};

enum class TextureDimension
{
	TEXTURE_1D,
	TEXTURE_2D,
	TEXTURE_3D,
	NONE
};

enum class ImageType : u32
{
	Image2D,
	CubeMap
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

// Buffer

enum class GPUBufferFlag : u32
{
	NONE = 0,
	VERTEX = 1 << 0,
	INDEX = 1 << 1,
	STORAGE = 1 << 2,
	CONSTANT = 1 << 3,
	SHADER_DEVICE_ADDRESS = 1 << 4,
	SHADER_BINDING_TABLE = 1 << 5,
	INDIRECT = 1 << 6,
};

inline GPUBufferFlag operator|(GPUBufferFlag a, GPUBufferFlag b)
{
	return static_cast<GPUBufferFlag>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline std::string GPUBufferFlagsToString(GPUBufferFlag flags)
{
	std::string out;

	if (HasFlag(flags, GPUBufferFlag::VERTEX)) out += "VERTEX|";
	if (HasFlag(flags, GPUBufferFlag::INDEX)) out += "INDEX|";
	if (HasFlag(flags, GPUBufferFlag::STORAGE)) out += "STORAGE|";
	if (HasFlag(flags, GPUBufferFlag::CONSTANT)) out += "CONSTANT|";
	if (HasFlag(flags, GPUBufferFlag::INDIRECT)) out += "INDIRECT|";
	if (HasFlag(flags, GPUBufferFlag::SHADER_DEVICE_ADDRESS)) out += "SHADER_DEVICE_ADDRESS|";
	if (HasFlag(flags, GPUBufferFlag::SHADER_BINDING_TABLE)) out += "SHADER_BINDING_TABLE|";

	if (!out.empty()) out.pop_back(); // remove trailing '|'
	if (out.empty()) out = "NONE";

	return out;
}


enum class GPUHeapType : u8
{
	Default,
	Upload,
	Readback,
	Unknown
};

inline const char* GPUHeapTypeToString(GPUHeapType type)
{
	switch (type)
	{
	case GPUHeapType::Default: return "Default";
	case GPUHeapType::Upload: return "Upload";
	case GPUHeapType::Readback: return "Readback";
	default: return "Unknown";
	}
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
	GPUBufferFlag usage = GPUBufferFlag::NONE;
	bool commit = false;
	const char* name = nullptr;

	static GPUBufferInfo FromPreset(const BufferPreset preset, const u64 size)
	{
		switch (preset)
		{
		case BufferPreset::VertexGPU:
			return {size, GPUHeapType::Upload, GPUBufferFlag::VERTEX};

		case BufferPreset::VertexStorageGPU:
			return {size, GPUHeapType::Upload, GPUBufferFlag::VERTEX | GPUBufferFlag::STORAGE | GPUBufferFlag::SHADER_DEVICE_ADDRESS};

		case BufferPreset::IndexGPU:
			return {size, GPUHeapType::Default, GPUBufferFlag::INDEX};

		case BufferPreset::UniformHost:
			return {size, GPUHeapType::Upload, GPUBufferFlag::CONSTANT, true};

		case BufferPreset::StagingUpload:
			return {size, GPUHeapType::Upload, GPUBufferFlag::NONE, true};

		case BufferPreset::StagingDownload:
			return {size, GPUHeapType::Readback, GPUBufferFlag::NONE, true};

		case BufferPreset::StorageGPU:
			return {size, GPUHeapType::Default, GPUBufferFlag::STORAGE | GPUBufferFlag::SHADER_DEVICE_ADDRESS};

		case BufferPreset::StorageHostPersistent:
			return {size, GPUHeapType::Upload, GPUBufferFlag::STORAGE, true};

		default:
			assert(false && "Unknown BufferPreset");
			break;
		}
	}
};

enum class DescriptorType
{
	Sampler,
	SampledImage,
	CombinedImageSampler,
	StorageImage,
	UniformBuffer,
	StorageBuffer,
	InputAttachment,
	AccelerationStructure
};

// Vendors/Shader Format

namespace ShaderStage
{
	enum Enum : u32
	{
		NONE           = 0,
		VERTEX         = 1 << 0,
		FRAGMENT       = 1 << 1,
		COMPUTE        = 1 << 2,
		RAYGEN         = 1 << 3,
		ANY_HIT        = 1 << 4,
		CLOSEST_HIT    = 1 << 5,
		MISS           = 1 << 6,
		CALLABLE       = 1 << 7,

		ALL_GRAPHICS   = VERTEX | FRAGMENT,
		ALL            = 0xFFFFFFFF
	};
}
typedef u32 ShaderStageFlags;


enum class ShaderFormat
{
	UNKNOWN,
	DXIL,
	SPIRV,
};

enum class GPUVendor
{
	UNKNOWN = 0x0,
	AMD = 0x1002,
	Nvidia = 0x10DE,
	INTEL = 0x8086
};

struct Extent2D
{
	u32 width;
	u32 height;

	[[nodiscard]] bool IsZero() const { return width == 0 || height == 0; }
	bool operator==(const Extent2D& rhs) const { return width == rhs.width && height == rhs.height; }
};

struct Extent3D
{
	u32 width;
	u32 height;
	u32 depth;
};

struct Viewport
{
	f32 x{}, y{}, width{}, height{}, minDepth = 0.f, maxDepth = 1.f;
};

// NOTE: ImageType is 2D by default
struct TextureInfo
{
	Extent3D extent;
	u16 mipLevels = 1;
	ImageType type = ImageType::Image2D;
	TextureFormat format = TextureFormat::IMAGE_FORMAT_UNKNOWN;
	TextureDimension dimension = TextureDimension::TEXTURE_2D;
	ImageUsageFlags usage = ImageUsage::NONE;

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
	u32 binding;
	DescriptorType type = DescriptorType::UniformBuffer;
	size_t size = 0;
};

struct UniformBufferDesc
{
	u32 framesInFlight = 1; // per-frame copies (for FIF)
	ShaderStageFlags stageFlags = 0; // shader stages
	std::string_view debugName; // optional
	Vector<Binding> bindings; // resource bindings
};

enum class DebugView
{
	None,        // Default lit
	Albedo,      // Texture color
	Normals,     // World-space normal
	Depth,       // Linearized depth
	Material
};
