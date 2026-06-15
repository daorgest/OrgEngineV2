//
// Created by Orgest on 7/22/2025.
//

#include <new>
#include <tracy/Tracy.hpp>

void* operator new(std::size_t count)
{
	void* ptr = std::malloc(count);
	if (ptr == nullptr) throw std::bad_alloc{};
	TracyAlloc(ptr, count);
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	TracyFree(ptr);
	std::free(ptr);
}

void* operator new[](std::size_t count)
{
	void* ptr = std::malloc(count);
	if (ptr == nullptr) throw std::bad_alloc{};
	TracyAlloc(ptr, count);
	return ptr;
}

void operator delete[](void* ptr) noexcept
{
	TracyFree(ptr);
	std::free(ptr);
}