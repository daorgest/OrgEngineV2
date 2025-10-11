//
// Created by Orgest on 6/9/2025.
//

#include "Arena.h"

#include "Logger.h"

ArenaAllocator::ArenaAllocator(size_t capacity)
	: base(static_cast<u8*>(Platform::Allocate(capacity))), capacity(capacity)
{
}

void* ArenaAllocator::Alloc(size_t allocSize, size_t alignment)
{
	const auto currentPtr = reinterpret_cast<uintptr_t>(base + size);
	const uintptr_t alignedPtr = (currentPtr + alignment - 1) & ~(alignment - 1);
	const size_t newSize = alignedPtr + allocSize - reinterpret_cast<uintptr_t>(base);

	if (newSize > capacity)
	{
		LOG(Error, "ArenaAllocator out of memory! Requested: {}, Alignment: {}, Used: {}, Total: {}",
		    allocSize, alignment, size, capacity);
		return nullptr;
	}

	auto result = reinterpret_cast<void*>(alignedPtr);
	size = newSize;

	LOG(Debug, "ArenaAllocator alloc: {} bytes (aligned to {}) -> Pointer: {}", allocSize, alignment, result);
	return result;
}

void ArenaAllocator::Reset()
{
	size = 0;
}

void ArenaAllocator::Destroy() const
{
	Platform::Free(base);
}
