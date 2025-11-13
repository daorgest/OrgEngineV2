#pragma once

#include <cstdio>
#include <ctime>
#include <source_location>
#include <fmt/format.h>

#include "FileManager.h"
#include "Platform.h"

enum class LogType
{
	Error,
	Warning,
	Info,
	Debug
};

namespace Log
{
	inline const char* ToString(LogType type)
	{
		switch (type)
		{
		case LogType::Error:   return "ERROR";
		case LogType::Warning: return "WARN";
		case LogType::Info:    return "INFO";
		case LogType::Debug:   return "DEBUG";
		default:               return "LOG";
		}
	}

	inline void InitLogFile()
	{
		auto fileResult = FileManager::Open("engine_log.txt", "w");
	}

	template<typename... Args>
	void Write(LogType type, fmt::format_string<Args...> format, Args&&... args)
	{
#ifndef NDEBUG
		// Log everything
		bool shouldLog = true;
#else
		// --- RELEASE BUILD ---
		// Only log errors (and optionally warnings)
		const bool shouldLog = (type == LogType::Error);
#endif
		if (!shouldLog)
			return;

		// Timestamp
		const std::time_t now = std::time(nullptr);
		std::tm tm{};
		localtime_s(&tm, &now);

		// Prefix
		char header[64];
		const char* logTypeString = ToString(type);
		std::snprintf(header, sizeof(header), "[%02d:%02d:%02d] [%s] ",
			tm.tm_hour, tm.tm_min, tm.tm_sec, logTypeString);

		// Format the message with fmt
		std::string message = fmt::format(format, std::forward<Args>(args)...);

		// Output to stderr or stdout
		FILE* output = (type == LogType::Error || type == LogType::Warning) ? stderr : stdout;
		std::fprintf(output, "%s%s\n", header, message.c_str());

		// Append to log file
		FILE* file = nullptr;
#ifdef _MSC_VER
		if (fopen_s(&file, "engine_log.txt", "a") == 0 && file)
		{
			std::fprintf(file, "%s%s\n", header, message.c_str());
			std::fclose(file);
		}
#else
		file = std::fopen("engine_log.txt", "a");
		if (file)
		{
			std::fprintf(file, "%s%s\n", header, message.c_str());
			std::fclose(file);
		}
#endif

#ifdef NDEBUG
		Platform::ShowMessageBox(message, logTypeString, Platform::MessageBoxType::Error);
#endif
	}
}  // namespace Log


// Usage: LOG(Info, "Window size: {} x {}", width, height);
#ifdef NDEBUG
    #define LOG(TYPE, FORMAT, ...) \
        Log::Write(LogType::TYPE, FORMAT, ##__VA_ARGS__)
#else
    #define LOG(TYPE, FORMAT, ...) \
        do { \
            if constexpr (LogType::TYPE == LogType::Error || LogType::TYPE == LogType::Warning) \
                Log::Write(LogType::TYPE, FORMAT, ##__VA_ARGS__); \
        } while(0)
#endif

// to reduce boilerplate for std::expected result errors
inline void LogResultError(std::string_view expr, const OrgErrCode err, const std::source_location& loc = std::source_location::current())
{
	LOG(Error,
	    "[Result Error]\n"
	    "  Expr:   {}\n"
	    "  Func:   {}\n"
	    "  File:   {}\n"
	    "  Line:   {}\n"
	    "  Error:  {}",
	    expr,
	    loc.function_name(),
	    loc.file_name(),
	    loc.line(),
	    static_cast<int>(err)
	);
}

#define CHECK_RESULT(expr)                                                        \
    if (auto _r = (expr); !_r)                                                    \
        LogResultError(#expr, _r.error(), std::source_location::current());


#define RETURN_LOG(TYPE, FORMAT, ...)                          \
    do {                                                       \
        LOG(TYPE, FORMAT, ##__VA_ARGS__);                      \
        return std::unexpected<std::string>(                   \
            fmt::format(FORMAT, ##__VA_ARGS__));               \
    } while (0)
