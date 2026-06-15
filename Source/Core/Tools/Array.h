//
// Created by Orgest on 7/30/2024.
//

// my attempt on making a std::array
#pragma once

template<typename T> struct Span;

template <typename T, size_t N> requires (N > 0)
struct Array
{

    T a[N];

    // Array() noexcept = default;

    // constexpr Array(std::initializer_list<T> init) noexcept
    // {
    //     assert(init.size() <= N && "Initializer list size exceeds array size.");
    //     size_t i = 0;
    //     for (const T& v : init) a[i++] = v;
    //     for (; i < N; ++i) a[i] = T();
    // }


    [[nodiscard]] static constexpr size_t size() noexcept { return N; }
    [[nodiscard]] constexpr size_t size_bytes() const noexcept { return N * sizeof(T); }

    // Data Access
    [[nodiscard]] constexpr T* data() noexcept { return a; }
    [[nodiscard]] constexpr const T* data() const noexcept { return a; }


    [[nodiscard]] constexpr T* begin() noexcept { return a; }
    [[nodiscard]] constexpr const T* begin() const noexcept { return a; }
    [[nodiscard]] constexpr T* end() noexcept { return a + N; }
    [[nodiscard]] constexpr const T* end() const noexcept { return a + N; }

    [[nodiscard]] constexpr T& operator[](size_t index) noexcept
    {
        // ASSERT(index < N);
        return a[index];
    }

    [[nodiscard]] constexpr const T& operator[](size_t index) const noexcept
    {
        // ASSERT(index < N);
        return a[index];
    }

    // Implicitly convert Array to a Span
    [[nodiscard]] constexpr operator Span<T>() noexcept
    {
        return Span<T>(a, N);
    }

    // Implicitly convert a const Array to a const Span
    [[nodiscard]] constexpr operator Span<const T>() const noexcept
    {
        return Span<const T>(a, N);
    }

    [[nodiscard]] constexpr T& front() noexcept { return a[0]; }
    [[nodiscard]] constexpr T& back() noexcept { return a[N - 1]; }

    // Utilities
    constexpr void fill(const T& value) noexcept
    {
        for (size_t i = 0; i < N; ++i)
        {
            a[i] = value;
        }
    }

    [[nodiscard]] constexpr bool contains(const T& value) const noexcept
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (a[i] == value) return true;
        }
        return false;
    }
};

// Deduction Guide for CTAD
template <typename T, typename... Ts>
Array(T, Ts...) -> Array<T, 1 + sizeof...(Ts)>;
