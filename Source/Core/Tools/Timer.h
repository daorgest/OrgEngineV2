#pragma once
#include <chrono>
#include "Logger.h"

struct Timer
{
	using Clock     = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

#ifdef ENABLE_TIMING
	explicit Timer(const char* functionName)
		: name(functionName), start(Clock::now()) {}
#else
	explicit Timer(const char* /*unused*/) {}
#endif

	~Timer()
	{
#ifdef ENABLE_TIMING
		const auto end = Clock::now();
		const f64 ms =
			std::chrono::duration<f64, std::milli>(end - start).count();
		LOG(Info, "{} took {:.2f} ms", name, ms);
#endif
	}

private:
#ifdef ENABLE_TIMING
	const char* name;
	TimePoint start;
#endif
};

// Macros automatically no-op when disabled
#ifdef ENABLE_TIMING
#   define TIME_FUNCTION() Timer _timer(__func__)
#   define TIME_BLOCK(NAME) Timer timer_##__LINE__(NAME)
#else
#   define TIME_FUNCTION() (void)0
#   define TIME_BLOCK(NAME) (void)0
#endif
