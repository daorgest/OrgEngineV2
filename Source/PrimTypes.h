#pragma once
#include <expected>

enum OrgErrCode
{
	None = 0,

	InitFailed,
	NotInitialized,
	AlreadyInitialized,
	MissingDependency,

	// File / I/O
	FileNotFound,
	FileAccessDenied,
	FileReadFailed,
	FileWriteFailed,
	FileOpenFailed,
	InvalidFileFormat,
	UnsupportedFileFormat,
	DirectoryNotFound,
	PathTooLong,
	IOError,


	// Assets
	AssetNotFound,
	AssetLoadFailed,
	AssetUnsupported,
	AssetCorrupted,
	AssetVersionMismatch,
	ShaderCompileFailed,
	MaterialLoadFailed,
	TextureLoadFailed,
	MeshLoadFailed,
	StaleHandle,

	// Graphics / Vulkan
	VulkanInitFailed,
	VulkanDeviceLost,
	VulkanSwapchainOutOfDate,
	VulkanNoMemory,
	VulkanInvalidState,
	VulkanTimeout,
	VulkanPipelineCreationFailed,
	VulkanDescriptorAllocationFailed,
	VulkanShaderModuleFailed,
	VulkanImageCreationFailed,
	VulkanBufferCreationFailed,
	VulkanCommandBufferFailed,

	// Swapchain
	OutOfDate,
	Suboptimal,
	SurfaceLost,
	DeviceLost,

	// Audio
	AudioInitFailed,
	AudioDeviceLost,
	AudioBufferFailed,

	// Scripting / Logic
	ScriptCompileFailed,
	ScriptRuntimeError,

	// Platform / System
	WindowInitFailed,
	InputDeviceLost,
	ThreadCreateFailed,
	OutOfMemory,
	PlatformError,


	// Misc
	NotImplemented,
};


template <typename T>
using Result = std::expected<T, OrgErrCode>;

#ifdef ORGAPI_DLL_EXPORT
#  define ORGAPI __declspec(dllexport)
#elifdef ORGAPI_DLL_IMPORT
#  define ORGAPI __declspec(dllimport)
#else
#  define ORGAPI
#endif

#include <cstdint>
#include <string_view>

// Unsigned
using u8    = uint8_t;
using u16   = uint16_t;
using u32   = uint32_t;
using u64   = uint64_t;
using uChar = unsigned char;

// Signed
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// Floating Point
using f32 = float;
using f64 = double;


constexpr u32 INVALID_ID = 0xFFFFFFFF;
constexpr u64 INVALID_ID_64 = 0xFFFFFFFFFFFFFFFF;

// Size Constants
constexpr u64 Kilobyte = 1024;
constexpr u64 Megabyte = 1024 * Kilobyte;
constexpr u64 Gigabyte = 1024 * Megabyte;
constexpr u64 Terabyte = 1024 * Gigabyte;

// Size Conversions
constexpr float BytesToKB(u64 bytes) { return (float)bytes / (float)Kilobyte; }
constexpr float BytesToMB(u64 bytes) { return (float)bytes / (float)Megabyte; }
constexpr float BytesToGB(u64 bytes) { return (float)bytes / (float)Gigabyte; }

// Frame overlap
static constexpr u32 MAX_FRAME_OVERLAP    = 2;
static constexpr u32 SWAPCHAIN_IMAGECOUNT = std::max(MAX_FRAME_OVERLAP + 1, 3u);


// Engine Information
constexpr auto ENGINE_NAME    = "OrgEngine";
constexpr auto ENGINE_VERSION = "0.1";
constexpr auto ENGINE_BUILD_DATE = __DATE__;
#ifdef _DEBUG
constexpr auto ENGINE_BUILD = "Debug";
#elifdef NDEBUG
constexpr auto ENGINE_BUILD = "Release";
#endif

// FPS Options
constexpr i32 BACKGROUND_FPS = 15;
constexpr auto BACKGROUND_FRAME_TIME = 1000 / BACKGROUND_FPS;

// Span Macros
#define SPAN_ONE(x) std::span(&(x), 1)
#define SPAN_PTR(ptr, count) std::span((ptr), (count))

// Controllers
constexpr u32 MAX_GAMEPADS = 4;

// Max Render Images/Targets
constexpr u32 MAX_RENDER_TARGETS = 1;

// Cameras
constexpr u32 MAX_SCENE_CAMERAS = 2;

// Lights
constexpr u32 MAX_LIGHTS = 8;


template <typename T>
struct ResourceHandle
{
    u64 id = INVALID_ID_64;

    constexpr ResourceHandle() = default;
    constexpr ResourceHandle(u32 index, u32 gen)
        : id((static_cast<u64>(gen) << 32) | index) {}

    constexpr u32 index(this auto&& self) noexcept { return static_cast<u32>(self.id & 0xFFFFFFFF); }
    constexpr u32 gen(this auto&& self)   noexcept { return static_cast<u32>(self.id >> 32); }

    auto operator<=>(const ResourceHandle&) const = default;
    [[nodiscard]] constexpr bool IsValid() const noexcept { return id != INVALID_ID_64; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return IsValid(); }
};

using TextureHandle = ResourceHandle<u32>;