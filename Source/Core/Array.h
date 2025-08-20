//
// Created by Orgest on 7/30/2024.
//

// my attempt on making a std::array

#pragma once
#include <cassert>
#include <initializer_list> // initializer list lol

template <typename T, size_t N>  requires (N > 0)
class Array
{
protected:
	T a[N];
public:
	constexpr Array() : a {} {}

	constexpr explicit Array(const T (&values)[N])
	{
		for (size_t i = 0; i < N; ++i)
		{
			a[i] = values[i];
		}
	}

	constexpr Array(std::initializer_list<T> init)
	{
		assert(init.size() <= N && "Initializer list size exceeds array size.");
		size_t i = 0;
		for (const T& val : init)
		{
			a[i++] = val;
		}
		// Fill remaining elements with default values (if init.size() < N)
		for (; i < N; ++i)
		{
			a[i] = T();
		}
	}

	constexpr bool contains(const T& val) const
	{
		for (size_t i = 0; i < N; i++)
		{
			if (a[i] == val)
			{
				return true;
			}
		}
		return false;
	}

	/// Returns the number of elements in the array.
	[[nodiscard]] constexpr size_t size() const noexcept { return N; }
	constexpr size_t size() noexcept { return N; }

	constexpr T* begin() noexcept { return a; }
	constexpr const T* begin() const noexcept { return a; }

	constexpr T* data() noexcept { return a; }
	constexpr const T* data() const noexcept { return a; }

	constexpr T& front() noexcept { return a[0]; }
	constexpr const T& front() const noexcept { return a[0]; }

	constexpr T* end() noexcept { return a + N; }
	constexpr const T* end() const noexcept { return a + N; }

	/// Conversion operator to void*.
	explicit operator void*() { return static_cast<void*>(a); }

	/// Conversion operator to const void*.
	explicit operator const void*() const { return static_cast<const void*>(a); }

	constexpr void fill(const T& val) noexcept
	{
		for (size_t i = 0; i < N; i++)
		{
			a[i] = val;
		}
	}

	constexpr T& operator[] (size_t i)
	{
		assert(i < N && "Index out of bounds");
		return a[i];
	}

	const T& operator[](size_t i) const
	{
		assert(i < N && "Index out of bounds");
		return a[i];
	}

	constexpr void reset() noexcept
	{
		for (size_t i = 0; i < N; i++)
		{
			a[i] = T();
		}
	}

};

// Deduction guide so you don't have to manually specify the type or number of elements.
// Just write: Array arr = {1, 2, 3}; and the compiler deduces Array<int, 3>.
template <typename T, typename... Ts>
Array(T, Ts...) -> Array<T, 1 + sizeof...(Ts)>;