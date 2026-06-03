#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace trackcamhub
{

enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error,
};

class Logger
{
public:
    static void debug(const std::string& message) { write(LogLevel::Debug, message); }
    static void info(const std::string& message) { write(LogLevel::Info, message); }
    static void warn(const std::string& message) { write(LogLevel::Warn, message); }
    static void error(const std::string& message) { write(LogLevel::Error, message); }

private:
    static void write(LogLevel level, const std::string& message)
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        std::ostringstream line;
        line << std::put_time(&tm, "%F %T") << " [" << toString(level) << "] " << message;

        auto& out = level == LogLevel::Error ? std::cerr : std::cout;
        out << line.str() << std::endl;
    }

    static const char* toString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
        }
        return "UNKNOWN";
    }
};

} // namespace trackcamhub

