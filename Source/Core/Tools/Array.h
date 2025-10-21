//
// Created by Orgest on 7/30/2024.
//

// my attempt on making a std::array

#pragma once
#include <cassert>
#include <initializer_list>

template <typename T, std::size_t N> requires (N > 0)
class Array
{
protected:
	T a[N];

public:
	// types (minimal)
	using value_type = T;
	using size_type = std::size_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;

	// ctor: zero-initialize
	constexpr Array() noexcept : a{}
	{
	}

	// from C-array
	constexpr explicit Array(const T (&values)[N]) noexcept
	{
		for (size_type i = 0; i < N; ++i) a[i] = values[i];
	}

	// from init-list (fills remainder with default)
	constexpr Array(std::initializer_list<T> init) noexcept
	{
		assert(init.size() <= N && "Initializer list size exceeds array size.");
		size_type i = 0;
		for (const T& v : init) a[i++] = v;
		for (; i < N; ++i) a[i] = T();
	}

	// capacity
	[[nodiscard]] static constexpr size_type size() noexcept { return N; }

	// data access
	[[nodiscard]] constexpr pointer data() noexcept { return a; }
	[[nodiscard]] constexpr const_pointer data() const noexcept { return a; }

	// iterators (raw pointers)
	[[nodiscard]] constexpr pointer begin() noexcept { return a; }
	[[nodiscard]] constexpr const_pointer begin() const noexcept { return a; }
	[[nodiscard]] constexpr pointer end() noexcept { return a + N; }
	[[nodiscard]] constexpr const_pointer end() const noexcept { return a + N; }

	// element access
	[[nodiscard]] constexpr reference front() noexcept { return a[0]; }
	[[nodiscard]] constexpr const_reference front() const noexcept { return a[0]; }
	[[nodiscard]] constexpr reference back() noexcept { return a[N - 1]; }
	[[nodiscard]] constexpr const_reference back() const noexcept { return a[N - 1]; }

	[[nodiscard]] constexpr reference operator[](size_type i) noexcept
	{
		assert(i < N && "Index out of bounds");
		return a[i];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept
	{
		assert(i < N && "Index out of bounds");
		return a[i];
	}

	// bounds-checked access without exceptions (assert only)
	[[nodiscard]] constexpr reference at(size_type i) noexcept
	{
		assert(i < N && "Array::at out of range");
		return a[i];
	}

	[[nodiscard]] constexpr const_reference at(size_type i) const noexcept
	{
		assert(i < N && "Array::at out of range");
		return a[i];
	}

	// modifiers
	constexpr void fill(const T& v) noexcept
	{
		for (size_type i = 0; i < N; ++i) a[i] = v;
	}

	constexpr void reset() noexcept
	{
		for (size_type i = 0; i < N; ++i) a[i] = T();
	}

	constexpr void swap(Array& other) noexcept
	{
		for (size_type i = 0; i < N; ++i)
		{
			T tmp = a[i];
			a[i] = other.a[i];
			other.a[i] = tmp;
		}
	}

	// utilities
	[[nodiscard]] constexpr bool contains(const T& v) const noexcept
	{
		for (size_type i = 0; i < N; ++i)
			if (a[i] == v) return true;
		return false;
	}

	// comparisons (optional but handy)
	friend constexpr bool operator==(const Array& x, const Array& y) noexcept
	{
		for (size_type i = 0; i < N; ++i)
			if (!(x.a[i] == y.a[i])) return false;
		return true;
	}

	friend constexpr bool operator!=(const Array& x, const Array& y) noexcept
	{
		return !(x == y);
	}
};

// Deduction guide so you don't have to manually specify the type or number of elements.
// Just write: Array arr = {1, 2, 3}; and the compiler deduces Array<int, 3>.
template <typename T, typename... Ts>
Array(T, Ts...) -> Array<T, 1 + sizeof...(Ts)>;
