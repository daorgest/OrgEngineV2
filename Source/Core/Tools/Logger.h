#pragma once

#include <cstdio>
#include <ctime>
#include <fmt/format.h>

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
		FILE* file = fopen("engine_log.txt", "w");
		if (file)
		{
			fclose(file);
		}
	}

	template<typename... Args>
	void Write(LogType type, fmt::format_string<Args...> format, Args&&... args)
	{
		// if (type != LogType::Info)
		// 	return;

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
#if defined(_MSC_VER)
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
		if (type == LogType::Error)
		{
			Platform::ShowMessageBox(message, logTypeString, Platform::MessageBoxType::Error);
		}
#endif
	}
}


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


#define RETURN_LOG(TYPE, FORMAT, ...)                          \
    do {                                                       \
        LOG(TYPE, FORMAT, ##__VA_ARGS__);                      \
        return std::unexpected<std::string>(                   \
            fmt::format(FORMAT, ##__VA_ARGS__));               \
    } while (0)
