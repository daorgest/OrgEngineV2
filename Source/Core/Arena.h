//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <utility>

#include "Vector.h"

struct ORGAPI ArenaAllocator
{
	u8* base = nullptr;
	size_t size = 0;
	size_t capacity = 0;

	explicit ArenaAllocator(size_t size);
	~ArenaAllocator() { Destroy(); }

	[[nodiscard]] void* Alloc(size_t allocSize, size_t alignment = 8);

	void Destroy() const;
	void Reset();

	template<typename T>
	[[nodiscard]]
	T* PushStruct()
	{
		return static_cast<T*>(Alloc(sizeof(T), alignof(T)));
	}

	template <typename T>
	[[nodiscard]]
	T* PushArray(size_t count)
	{
		return static_cast<T*>(Alloc(sizeof(T) * count, alignof(T)));
	}

	template <typename T, typename... Args>
	[[nodiscard]]
	T* Emplace(Args&&... args)
	{
		void* memory = Alloc(sizeof(T), alignof(T));
		return ::new(memory) T(std::forward<Args>(args)...);
	}
};
