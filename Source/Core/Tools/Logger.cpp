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

void Logger::WriteInternal(LogType type, std::string&& message, const std::source_location& loc)
{
#ifdef NDEBUG
    if (type == LogType::Debug || type == LogType::Info) return;
#endif

    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    std::string finalMessage;
    if (type == LogType::Error || type == LogType::Warning)
    {
        finalMessage = fmt::format("[{}:{}] {}", loc.file_name(), loc.line(), message);
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