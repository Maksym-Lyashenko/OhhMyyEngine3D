#include "core/Logger.h"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef ERROR
#endif

namespace Core
{
    std::ofstream Logger::logFile;

    void Logger::init(const std::string &baseFilename)
    {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE)
        {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode))
            {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
#endif

        // Ensure directory exists
        std::filesystem::path basePath(baseFilename);
        if (basePath.has_parent_path())
        {
            std::filesystem::create_directories(basePath.parent_path());
        }

        // Generate timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now_time);
#else
        localtime_r(&now_time, &tm);
#endif

        std::ostringstream timestamp;
        timestamp << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

        // Final filename: logs/engine_YYYY-MM-DD_HH-MM-SS.log
        std::filesystem::path finalPath = basePath.parent_path() /
                                          (basePath.stem().string() + "_" + timestamp.str() + basePath.extension().string());

        // Open log file
        logFile.open(finalPath, std::ios::out | std::ios::trunc);
        if (!logFile.is_open())
        {
            std::cerr << "Failed to open log file: " << finalPath << std::endl;
        }
        else
        {
            std::cout << "Logging to file: " << finalPath << std::endl;
        }
    }

    void Logger::shutdown()
    {
        if (logFile.is_open())
            logFile.close();
    }

    void Logger::log(LogLevel level, const std::string &message)
    {
        std::string prefix = "[" + levelToString(level) + "] ";

        // Console output with colors
        std::cout << levelToColor(level) << prefix << message << "\033[0m" << std::endl;

        // File output (no colors)
        if (logFile.is_open())
        {
            logFile << prefix << message << std::endl;
        }
    }

    std::string Logger::levelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::DEBUG:
            return "DEBUG";
        }
        return "UNKNOWN";
    }

    std::string Logger::levelToColor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::INFO:
            return "\033[36m"; // Cyan
        case LogLevel::WARNING:
            return "\033[33m"; // Yellow
        case LogLevel::ERROR:
            return "\033[31m"; // Red
        case LogLevel::DEBUG:
            return "\033[35m"; // Magenta
        }
        return "\033[0m";
    }
}
