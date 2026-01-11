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

    static std::deque<LogEntry> gLogEntries;
    static std::mutex gLogMutex;
    static std::condition_variable gLogCondition;
    static std::thread logThread;
    static bool gRunning;

    // Static interface for global access
    static void Init();
    static void Shutdown();

    template <typename... Args>
    static void Write(LogType type, std::source_location loc, fmt::format_string<Args...> format, Args&&... args)
    {
#ifndef NDEBUG
        bool shouldLog = true;
#else
        const bool shouldLog = (type == LogType::Error || type == LogType::Warning);
#endif
        if (!shouldLog) return;

        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &now);
        std::string message = fmt::format(format, std::forward<Args>(args)...);

        {
            std::lock_guard lock(gLogMutex);
            gLogEntries.push_back({type, tm, std::move(message)});
        }
        gLogCondition.notify_one();
#ifdef NDEBUG
        if (type == LogType::Error)
        {
            auto trace = std::stacktrace::current();
            std::string filteredTrace = "";
            int frameCount = 0;

            for (const auto& frame : trace)
            {
                std::string file = frame.source_file();
                std::string func = frame.description();

                // Skip driver/logger noise
                if (file.empty() ||
                    file.find("vulkan") != std::string::npos ||
                    file.find("khronos") != std::string::npos ||
                    func.find("Logger::Write") != std::string::npos ||
                    func.find("DebugCallback") != std::string::npos)
                {
                    continue;
                }

                // Record up to 8 engine frames
                if (frameCount < 5) {
                    filteredTrace += fmt::format("  [{}] {} \n       -> {}:{}\n",
                        frameCount++, func, file, frame.source_line());
                }
            }

            std::string boxMessage = fmt::format(
                "Engine Error Context:\n\n"
                "Message: {}\n\n"
                "Deep Engine Callstack:\n{}",
                message,
                filteredTrace.empty() ? "  (No engine frames identified)" : filteredTrace
            );

            Platform::ShowMessageBox(boxMessage, "Critical Error", Platform::MessageBoxType::Error);
        }
#endif
    }

    static void LogResultError(std::string_view expr, OrgErrCode err,
                               const std::source_location& loc = std::source_location::current());

private:
    static void LoggerThreadWork();
    static const char* ToString(LogType type);
    static const char* GetColor(LogType type);
};

// Macros
#ifndef NDEBUG
#define LOG(TYPE, FORMAT, ...) Logger::Write(LogType::TYPE, std::source_location::current(), FORMAT, ##__VA_ARGS__)
#else
#define LOG(TYPE, FORMAT, ...) \
        do { if (LogType::TYPE == LogType::Error || LogType::TYPE == LogType::Warning) \
            Logger::Write(LogType::TYPE, std::source_location::current(), FORMAT, ##__VA_ARGS__); } while(0)
#endif

#define CHECK_RESULT(expr)                                                        \
    if (auto _r = (expr); !_r)                                                    \
        Logger::LogResultError(#expr, _r.error(), std::source_location::current()); (0)