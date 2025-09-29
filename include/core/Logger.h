#pragma once

#include <string>
#include <iostream>
#include <fstream>

namespace Core
{
    enum class LogLevel
    {
        INFO,
        WARNING,
        ERROR,
        DEBUG
    };

    class Logger
    {
    public:
        static void init(const std::string &filename = "engine.log"); // init with file
        static void shutdown();                                       // close log file

        static void log(LogLevel level, const std::string &message);

    private:
        static std::ofstream logFile;

        static std::string levelToString(LogLevel level);
        static std::string levelToColor(LogLevel level);
    };
}

// Convenience macros
#define CORE_LOG_INFO(msg) Core::Logger::log(Core::LogLevel::INFO, msg)
#define CORE_LOG_WARN(msg) Core::Logger::log(Core::LogLevel::WARNING, msg)
#define CORE_LOG_ERROR(msg) Core::Logger::log(Core::LogLevel::ERROR, msg)
#define CORE_LOG_DEBUG(msg) Core::Logger::log(Core::LogLevel::DEBUG, msg)
