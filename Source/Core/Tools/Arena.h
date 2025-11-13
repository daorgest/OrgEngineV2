//
// Created by Orgest on 6/9/2025.
//

#pragma once

struct ArenaAllocator
{
	u8* base = nullptr;
	size_t size = 0;
	size_t capacity = 0;

	ArenaAllocator() = default;
	explicit ArenaAllocator(size_t capacity);

	void* Alloc(size_t allocSize, size_t alignment = 8);

	template<typename T>
	T* Alloc(size_t count = 1)
	{
		return static_cast<T*>(Alloc(sizeof(T) * count, alignof(T)));
	}

	template<typename T, typename... Args>
	T* Emplace(Args&&... args)
	{
		void* memory = Alloc(sizeof(T), alignof(T));
		if (memory)
		{
			return new (memory) T(std::forward<Args>(args)...);
		}
		return nullptr;
	}

	void Reset();
	void Destroy() const;
};

