#include <chrono>
#include "Logger.h"

struct Timer
{
	using Clock     = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

#if defined(ENABLE_TIMING)
	explicit Timer(const char* functionName)
		: name(functionName), start(Clock::now()) {}
#else
	explicit Timer(const char*) {}
#endif

	~Timer()
	{
#if defined(ENABLE_TIMING)
		const auto end = Clock::now();
		const double ms =
			std::chrono::duration<double, std::milli>(end - start).count();
		LOG(Info, "{} took {:.2f} ms", name, ms);
#endif
	}

private:
#if defined(ENABLE_TIMING)
	const char* name;
	TimePoint start;
#endif
};

// Macros automatically no-op when disabled
#if defined(ENABLE_TIMING)
#   define TIME_FUNCTION() Timer _timer(__func__)
#   define TIME_BLOCK(NAME) Timer timer_##__LINE__(NAME)
#else
#   define TIME_FUNCTION() (void)0
#   define TIME_BLOCK(NAME) (void)0
#endif
