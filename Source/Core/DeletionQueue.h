//
// Created by Orgest on 11/11/2024.
//

#pragma once
#include <functional>
#include <iostream>
#include <ranges>
#include <string>

struct DeletionQueue
{
	using Task = std::pair<std::string, std::function<void()>>;

	std::vector<Task> tasks;

	template <typename F>
	void Push(F&& f, std::string_view name = {})
	{
		tasks.emplace_back(std::string{name}, std::forward<F>(f));
	}

	void FlushLIFO() noexcept {
		for (auto & [fst, snd] : std::ranges::reverse_view(tasks))
			snd();  // call the function
		tasks.clear();
	}

	void FlushFIFO() noexcept {
		for (auto& val : tasks | std::views::values)
			val();
		tasks.clear();
	}

	// sum utils
	[[nodiscard]] bool empty() const noexcept { return tasks.empty(); }
	[[nodiscard]] std::size_t size() const noexcept { return tasks.size(); }
};

inline DeletionQueue gDeletionQueue;