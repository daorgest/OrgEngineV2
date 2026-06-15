//
// Created by Orgest on 6/15/2025.
//

#pragma once
#include "../PrimTypes.h"
#include <algorithm>
#include <numbers>

#include "Vec4.h"

template <typename T>
T Lerp(T a, T b, f32 t)
{
    return a + t * (b - a);
}

inline f32 ExpLerp(f32 current, f32 target, f32 speed, f32 dt)
{
    return Lerp(current, target, 1.0f - std::exp(-speed * dt));
}

template <typename T>
T Remap(T value, T inMin, T inMax, T outMin, T outMax)
{
    return (value - inMin) / (inMax - inMin) * (outMax - outMin) + outMin;
}

template <typename T>
T Clamp(T value, T min, T max)
{
    return std::max(min, std::min(max, value));
}

inline Vec4 Clamp(const Vec4& v, f32 min, f32 max)
{
    return {
        std::max(min, std::min(max, v.x)),
        std::max(min, std::min(max, v.y)),
        std::max(min, std::min(max, v.z)),
        std::max(min, std::min(max, v.w))
    };
}

template <typename T>
T Round(const T& v)
{
    T result;
    result.x = std::round(v.x);
    result.y = std::round(v.y);
    result.z = std::round(v.z);
    result.w = std::round(v.w);
    return result;
}

inline u32 PackUnorm4x8(const Vec4& v)
{
    const Vec4 clamped = Clamp(v, 0.0f, 1.0f);
    const Vec4 scaled = Round(clamped * 255.0f);

    return (static_cast<u32>(scaled.x) << 0) |
        (static_cast<u32>(scaled.y) << 8) |
        (static_cast<u32>(scaled.z) << 16) |
        (static_cast<u32>(scaled.w) << 24);
}

template <typename T>
constexpr T Radians(T degrees)
{
    return degrees * (std::numbers::pi_v<T> / T(180));
}