//
// Created by Orgest on 12/20/2025.
//

#include "Logger.h"
#include "FileManager.h"
#include "Platform.h"

std::deque<Logger::LogEntry> Logger::gLogEntries;
std::mutex Logger::gLogMutex;
std::condition_variable Logger::gLogCondition;
std::thread Logger::logThread;
bool Logger::gRunning = false;

const char* Logger::ToString(const LogType type)
{
    switch (type)
    {
    case LogType::Error: return "ERROR";
    case LogType::Warning: return "WARN";
    case LogType::Info: return "INFO";
    case LogType::Debug: return "DEBUG";
    default: return "LOG";
    }
}

const char* Logger::GetColor(const LogType type)
{
    switch (type)
    {
    case LogType::Error: return "\033[91m";
    case LogType::Warning: return "\033[93m";
    case LogType::Info: return "\033[92m";
    case LogType::Debug: return "\033[96m";
    default: return "\033[0m";
    }
}

static std::string FormatCleanStacktrace(const std::stacktrace& trace)
{
    std::string cleanTrace;
    int displayIndex = 0;

    for (const auto& entry : trace)
    {
        std::string desc = entry.description();
        std::string file = entry.source_file();

        // Filtering
        if (desc.find("VkLayer_") != std::string::npos ||
            desc.find("KERNEL32") != std::string::npos ||
            desc.find("ntdll") != std::string::npos ||
            desc.find("__scrt_common_main_seh") != std::string::npos)
        {
            continue;
        }
        if (!file.empty())
        {
            cleanTrace += fmt::format("{}> {}({}): {}\n",
                                      displayIndex++, file, entry.source_line(), desc);
        }
        else
        {
            cleanTrace += fmt::format("{}> {}\n", displayIndex++, desc);
        }
    }

    return cleanTrace;
}

void Logger::LoggerThreadWork()
{
    FileManager::Handle logFile;
    const bool fileOpen = logFile.open("engine_log.txt", "a");

    while (gRunning || !gLogEntries.empty())
    {
        LogEntry entry;
        {
            std::unique_lock lock(gLogMutex);
            gLogCondition.wait(lock, [] { return !gLogEntries.empty() || !gRunning; });
            if (gLogEntries.empty() && !gRunning) break;
            entry = std::move(gLogEntries.front());
            gLogEntries.pop_front();
        }

        const char* typeStr = ToString(entry.type);
        FILE* output = (entry.type == LogType::Error || entry.type == LogType::Warning) ? stderr : stdout;

        fmt::print(output, "{}[{:02}:{:02}:{:02}] [{}]\033[37m {}\033[0m\n",
                   GetColor(entry.type), entry.timeStamp.tm_hour, entry.timeStamp.tm_min,
                   entry.timeStamp.tm_sec, typeStr, entry.message);
        std::fflush(output);

        if (fileOpen)
        {
            std::string fileLine = fmt::format("[{:02}:{:02}:{:02}] [{}] {}\n",
                                               entry.timeStamp.tm_hour, entry.timeStamp.tm_min,
                                               entry.timeStamp.tm_sec, ToString(entry.type), entry.message);

            logFile.write_string(fileLine); // <-- Add whatever your write method is
        }
    }
    if (fileOpen) logFile.close();
}

void Logger::Init()
{
    if (gRunning) return;
    gRunning = true;
    {
        FileManager::Handle f("engine_log.txt", "w");
    }
    logThread = std::thread(LoggerThreadWork);
}

void Logger::Shutdown()
{
    {
        std::lock_guard lock(gLogMutex);
        gRunning = false;
    }
    gLogCondition.notify_all();
    if (logThread.joinable()) logThread.join();
}

void Logger::Flush()
{
    while (true)
    {
        std::lock_guard lock(gLogMutex);
        if (gLogEntries.empty()) break;
    }
}

void Logger::WriteInternal(LogType type, std::string&& message, const std::source_location& loc)
{
#ifndef ENGINE_ENABLE_LOGGING
    return;
#endif

    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    std::string finalMessage;

    const char* fileName = loc.file_name();
    const bool hasLocation = fileName && fileName[0] != '\0';
    if (type == LogType::Error)
    {
        const auto trace = std::stacktrace::current(1);

        if (hasLocation) {
            finalMessage = fmt::format("[{}:{}] {}\n~[Stacktrace]~\n{}",
                                       fileName, loc.line(), message, FormatCleanStacktrace(trace));
        } else {
            finalMessage = fmt::format("{}\n~[Stacktrace]~\n{}",
                                       message, FormatCleanStacktrace(trace));
        }
    }
    else if (type == LogType::Warning)
    {
        if (hasLocation) {
            finalMessage = fmt::format("[{}:{}] {}", fileName, loc.line(), message);
        } else {
            finalMessage = std::move(message);
        }
    }
    else
    {
        finalMessage = std::move(message);
    }

    {
        std::lock_guard lock(gLogMutex);
        LogEntry entry;
        entry.type = type;
        entry.timeStamp = tm;
        entry.message = std::move(finalMessage);

        gLogEntries.push_back(std::move(entry));
    }
    gLogCondition.notify_one();

    if (type == LogType::Error)
    {
        Platform::ShowMessageBox(message, "Critical Error", Platform::MessageBoxType::Error);
    }
}
void Logger::LogResultError(std::string_view expr, i32 err, const std::source_location& loc)
{
    // Reuse the internal logic
    Logger::Write(LogType::Error, loc, "[Result Error] Expr: {} | Error Code: {}", expr, err);
}