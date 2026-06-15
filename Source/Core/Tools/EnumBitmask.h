//
// Created by Orgest on 6/2/2026.
//

#pragma once
#include <type_traits>
#include <utility>

// Enum bitmask
template <typename T>
constexpr bool EnableBitmask = false;

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
[[nodiscard]] constexpr T operator|(T a, T b) noexcept
{
    return static_cast<T>(std::to_underlying(a) | std::to_underlying(b));
}

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
[[nodiscard]] constexpr T operator&(T a, T b) noexcept
{
    return static_cast<T>(std::to_underlying(a) & std::to_underlying(b));
}

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
[[nodiscard]] constexpr T operator^(T a, T b) noexcept
{
    return static_cast<T>(std::to_underlying(a) ^ std::to_underlying(b));
}

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
[[nodiscard]] constexpr T operator~(T a) noexcept { return static_cast<T>(~std::to_underlying(a)); }

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
constexpr T& operator|=(T& a, T b) noexcept { return a = (a | b); }

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
constexpr T& operator&=(T& a, T b) noexcept { return a = (a & b); }

template <typename T>
    requires std::is_enum_v<T> && EnableBitmask<T>
constexpr T& operator^=(T& a, T b) noexcept { return a = (a ^ b); }

// Helpers
template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] constexpr bool HasAny(E value, E mask) noexcept
{
    return (std::to_underlying(value) & std::to_underlying(mask)) != 0;
}

template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] constexpr bool HasAll(E value, E mask) noexcept
{
    return (std::to_underlying(value) & std::to_underlying(mask)) == std::to_underlying(mask);
}
