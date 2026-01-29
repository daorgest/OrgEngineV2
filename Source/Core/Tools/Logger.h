#pragma once

#include <ctime>
#include <thread>
#include <mutex>
#include <stacktrace>
#include <condition_variable>
#include <deque>
#include <source_location>
#include <fmt/format.h>

#include "Platform.h"

enum class LogType { Error, Warning, Info, Debug };

class Logger
{
public:
    struct LogEntry
    {
        LogType type;
        std::tm timeStamp;
        std::string message;
    };

    static void Init();
    static void Shutdown();

    template <typename... Args>
        static void Write(LogType type, const std::source_location& loc, fmt::format_string<Args...> format, Args&&... args)
    {
        WriteInternal(type, fmt::format(format, std::forward<Args>(args)...), loc);
    }

    static void LogResultError(std::string_view expr, i32 err, const std::source_location& loc = std::source_location::current());

private:
    static void WriteInternal(LogType type, std::string&& message, const std::source_location& loc);
    static void LoggerThreadWork();
    static const char* ToString(LogType type);
    static const char* GetColor(LogType type);

    static std::deque<LogEntry> gLogEntries;
    static std::mutex gLogMutex;
    static std::condition_variable gLogCondition;
    static std::thread logThread;
    static bool gRunning;
};

// Macros
#define LOG(TYPE, FORMAT, ...) \
Logger::Write(LogType::TYPE, std::source_location::current(), FORMAT, ##__VA_ARGS__)

#define CHECK_RESULT(expr)                                                        \
    do {                                                                          \
        auto _result = (expr);                                                    \
        if (!_result) {                                                           \
            Logger::LogResultError(#expr, static_cast<i32>(_result.error()),      \
                                   std::source_location::current());              \
            assert(false && "Result Failure");                                    \
        }                                                                         \
    } while (0)