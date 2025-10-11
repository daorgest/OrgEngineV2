//
// Created by Orgest on 8/6/2025.
//

#pragma once
#include <chrono>
#include "Logger.h"

struct Timer
{
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	explicit Timer(const char* functionName)
		: name(functionName), start(Clock::now()) {}

	~Timer()
	{
		const auto end = Clock::now();
		const double ms = std::chrono::duration<double, std::milli>(end - start).count();
		LOG(Info, "{} took {:.2f} ms", name, ms);
	}
private:
	const char* name;
	TimePoint start;
};

#define TIME_FUNCTION() Timer _timer(__func__)
#define TIME_BLOCK(NAME) Timer timer_##__LINE__(NAME)