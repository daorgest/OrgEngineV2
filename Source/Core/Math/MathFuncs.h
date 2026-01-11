//
// Created by Orgest on 6/15/2025.
//

#pragma once
#include "../PrimTypes.h"
#include <algorithm>
#include <numbers>

#include "Vec4.h"

template <typename T>
T Remap(T value, T inMin, T inMax, T outMin, T outMax)
{
    return ((value - inMin) / (inMax - inMin) * (outMax - outMin)) + outMin;
}

template <typename T>
T Clamp(const T& v, f32 min, f32 max)
{
    T result;
    result.x = std::max(min, std::min(max, v.x));
    result.y = std::max(min, std::min(max, v.y));
    result.z = std::max(min, std::min(max, v.z));
    result.w = std::max(min, std::min(max, v.w));
    return result;
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
    union
    {
        u8 in[4];
        u32 out;
    } u{};

    const Vec4 clamped = Clamp(v, 0.0f, 1.0f);
    const Vec4 scaled = Round(clamped * 255.0f);

    u.in[0] = static_cast<u8>(scaled.x);
    u.in[1] = static_cast<u8>(scaled.y);
    u.in[2] = static_cast<u8>(scaled.z);
    u.in[3] = static_cast<u8>(scaled.w);

    return u.out;
}

template <typename T>
constexpr T Radians(T degrees)
{
    return degrees * (std::numbers::pi_v<T> / T(180));
}