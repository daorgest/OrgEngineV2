//
// Created by Orgest on 6/11/2026.
//

#pragma once

template<typename T>
struct Span
{
    T* content = nullptr;
    size_t currentSize = 0;

    Span() = default;

    operator Span<const T>() const noexcept
    {
        return Span<const T>(content, currentSize);
    }

    constexpr Span(T* data, const size_t size) : content(data), currentSize(size) {}
    constexpr Span(T* start, T* end) : content(start), currentSize(end - start) {}

    template <size_t N>
    constexpr Span(T(&arr)[N]) : content(arr), currentSize(N) {}

    [[nodiscard]] constexpr T& operator[](size_t index)
    {
        // assert(index < size);
        return content[index];
    }

    [[nodiscard]] constexpr const T& operator[](size_t index) const
    {
        // assert(index < size);
        return content[index];
    }

    [[nodiscard]] constexpr T* data() { return content; }
    [[nodiscard]] constexpr const T* data() const { return content; }

    [[nodiscard]] constexpr size_t size() const { return currentSize; }
    [[nodiscard]] constexpr size_t size_bytes() const noexcept { return currentSize * sizeof(T); }
    [[nodiscard]] constexpr bool empty() const noexcept { return currentSize == 0; }

    [[nodiscard]] constexpr T* begin() const noexcept { return content; }
    [[nodiscard]] constexpr T* end() const noexcept { return content + currentSize; }

    [[nodiscard]] constexpr Span subspan(size_t offset, size_t sub_count) const
    {
        // assert(offset <= size);
        // assert(offset + sub_count <= size);
        return Span(content + offset, sub_count);
    }

    [[nodiscard]] constexpr Span subspan(size_t offset) const
    {
        // assert(offset <= size);
        return Span(content + offset, currentSize - offset);
    }

    [[nodiscard]] constexpr Span first(size_t count) const
    {
        // assert(count <= currentSize);
        return Span(content, count);
    }
};
