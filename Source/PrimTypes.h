#pragma once
#include <expected>

enum OrgErrCode
{
	None = 0,

	// File / I/O
	FileNotFound,
	FileReadFailed,
	FileWriteFailed,
	InvalidFileFormat,

	// Assets
	AssetNotFound,
	AssetLoadFailed,
	AssetUnsupported,
	ShaderCompileFailed,

	// Graphics / Vulkan
	VulkanInitFailed,
	VulkanDeviceLost,
	VulkanSwapchainOutOfDate,
	VulkanNoMemory,
	PipelineCreationFailed,
	DescriptorAllocationFailed,

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


	// Misc
	NotImplemented,
};


template<typename T>
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
#include <type_traits>

// Unsigned
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
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
static constexpr u32 MAX_FRAME_OVERLAP = 3; // Triple buffering for better GPU utilization
static constexpr u32 SWAPCHAIN_IMAGECOUNT = std::max(MAX_FRAME_OVERLAP + 1, 3u);


// Engine Information
constexpr auto ENGINE_NAME = "OrgEngine";
constexpr auto ENGINE_VERSION = "0.1";

constexpr auto ENGINE_BUILD_DATE = __DATE__;
#ifdef _DEBUG
constexpr auto ENGINE_BUILD = "Debug";
#elifdef NDEBUG
constexpr auto ENGINE_BUILD = "Release";
#endif

// Span Macros
#define SPAN_ONE(x) std::span(&(x), 1)
#define SPAN_PTR(ptr, count) std::span((ptr), (count))

// Controllers
constexpr u32 MAX_GAMEPADS = 4;