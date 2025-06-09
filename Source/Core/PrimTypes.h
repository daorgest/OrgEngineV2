#pragma once


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

// Math constants
#define M_PI 3.14159265358979323846

// Size conversions
#define BYTES_TO_KB(b) ((float)(b) / 1024.0f)
#define BYTES_TO_MB(b) ((float)(b) / (1024.0f * 1024.0f))
#define BYTES_TO_GB(b) ((float)(b) / (1024.0f * 1024.0f * 1024.0f))