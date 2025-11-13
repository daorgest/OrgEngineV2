//
// Created by Orgest on 6/15/2025.
//

#pragma once
#include <algorithm>
#include <numbers>
template<typename T>
T Remap(T value, T inMin, T inMax, T outMin, T outMax)
{
	return ((value - inMin) / (inMax - inMin) * (outMax - outMin)) + outMin;
}

template<typename T>
T Clamp(const T& v, f32 min, f32 max)
{
	T result;
	result.x = std::max(min, std::min(max, v.x));
	result.y = std::max(min, std::min(max, v.y));
	result.z = std::max(min, std::min(max, v.z));
	result.w = std::max(min, std::min(max, v.w));
	return result;
}

template<typename T>
T Round(const T& v)
{
	T result;
	result.x = std::round(v.x);
	result.y = std::round(v.y);
	result.z = std::round(v.z);
	result.w = std::round(v.w);
	return result;
}

template <typename T>
constexpr T Radians(T degrees) {
	return degrees * (std::numbers::pi_v<T> / T(180));
}