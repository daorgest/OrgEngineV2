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

void Logger::LogResultError(std::string_view expr, const OrgErrCode err, const std::source_location& loc)
{
    std::string detail = fmt::format(
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
    Logger::Write(LogType::Error, loc, "{}", detail);
}

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
            logFile.write(fileLine.c_str(), 1, fileLine.size());
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
    logThread = std::thread(Logger::LoggerThreadWork);
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