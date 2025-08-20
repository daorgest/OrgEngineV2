#pragma once
#if defined(ORGAPI_DLL_EXPORT)
#  define ORGAPI __declspec(dllexport)
#elif defined(ORGAPI_DLL_IMPORT)
#  define ORGAPI __declspec(dllimport)
#else
#  define ORGAPI
#endif

#include <cstdint>

// Unsigned
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// Signed
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// Floating Point
using f32 = float;
using f64 = double;

using uChar = unsigned char;

constexpr u32 INVALID_ID = 0xFFFFFFFF;

// Size Constants
constexpr u64 Kilobyte = 1024;
constexpr u64 Megabyte = 1024 * Kilobyte;
constexpr u64 Gigabyte = 1024 * Megabyte;

// Size Conversions
constexpr float BytesToKB(u64 bytes) { return (float)bytes / (float)Kilobyte; }
constexpr float BytesToMB(u64 bytes) { return (float)bytes / (float)Megabyte; }
constexpr float BytesToGB(u64 bytes) { return (float)bytes / (float)Gigabyte; }