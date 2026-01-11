//
// Created by Orgest on 11/11/2024.
//

#pragma once
#include <functional>
#include <ranges>

#include "Logger.h"
#include "Vector.h"


// Will prob use this, was from the last attempt on making a engine...but ehhh....
struct DeletionQueue
{
	using Task = std::pair<std::string, std::function<void()>>;
	Vector<Task> tasks;

	template <typename F>
	void Push(F&& f, const std::string_view name = {})
	{
		tasks.emplace_back(std::string{name}, std::forward<F>(f));
	}

	void FlushLIFO() noexcept
	{
		LOG(Debug, "DeletionQueue: Flushing {} tasks (LIFO)", tasks.size());

		for (auto& [name, fn] : std::ranges::reverse_view(tasks))
		{
			LOG(Debug, "   - Destroying {}", name);
			fn();
		}

		tasks.clear();
	}

	void FlushFIFO() noexcept
	{
		LOG(Debug, "DeletionQueue: Flushing {} tasks (FIFO)", tasks.size());

		for (auto& [name, fn] : tasks)
		{
			LOG(Debug, "   - Destroying {}", name);
			fn();
		}

		tasks.clear();
	}
};
