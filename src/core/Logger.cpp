#include "core/Logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

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
    std::atomic<bool> Logger::colorsEnabled{true};
    std::atomic<int> Logger::minLevel{static_cast<int>(LogLevel::INFO)};

    namespace
    {
        std::mutex g_logMutex;

        inline std::string makeTimestamp()
        {
            using namespace std::chrono;
            auto now = system_clock::now();
            std::time_t t = system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif

            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return oss.str();
        }

        inline std::string makeFileSuffix()
        {
            using namespace std::chrono;
            auto now = system_clock::now();
            std::time_t t = system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
            return oss.str();
        }

    } // namespace

    void Logger::initConsoleVT() noexcept
    {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE)
            return;
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode))
            return;
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
#endif
    }

    void Logger::init(const std::string &baseFilename) noexcept
    {
        std::lock_guard<std::mutex> lock(g_logMutex);

        initConsoleVT();

        // Ensure directory exists
        std::filesystem::path basePath(baseFilename);
        if (basePath.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(basePath.parent_path(), ec);
        }

        // Compose final path: logs/engine_YYYY-MM-DD_HH-MM-SS.log
        const std::string suff = makeFileSuffix();
        std::filesystem::path finalPath =
            basePath.parent_path() /
            (basePath.stem().string() + "_" + suff + basePath.extension().string());

        logFile.close();
        logFile.clear();
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

    void Logger::shutdown() noexcept
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (logFile.is_open())
            logFile.close();
    }

    void Logger::setLevel(LogLevel level) noexcept
    {
        minLevel.store(static_cast<int>(level), std::memory_order_relaxed);
    }

    void Logger::enableColors(bool enabled) noexcept
    {
        colorsEnabled.store(enabled, std::memory_order_relaxed);
    }

    void Logger::log(LogLevel level, const std::string &message) noexcept
    {
        if (static_cast<int>(level) < minLevel.load(std::memory_order_relaxed))
        {
            return; // filtered out
        }

        const std::string ts = makeTimestamp();
        const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

        const char *lvlStr = levelToString(level);
        const char *col = colorsEnabled.load(std::memory_order_relaxed) ? levelToColor(level) : "";
        const char *reset = colorsEnabled.load(std::memory_order_relaxed) ? "\033[0m" : "";

        std::lock_guard<std::mutex> lock(g_logMutex);

        // Console
        std::cout << col << '[' << ts << "][" << lvlStr << "][t:" << tid << "] "
                  << message << reset << std::endl;

        // File (no colors)
        if (logFile.is_open())
        {
            logFile << '[' << ts << "][" << lvlStr << "][t:" << tid << "] "
                    << message << std::endl;
            // Optionally flush to keep file consistent on crash:
            // logFile.flush();
        }
    }

    const char *Logger::levelToString(LogLevel level) noexcept
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

    const char *Logger::levelToColor(LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::INFO:
            return "\033[36m"; // cyan
        case LogLevel::WARNING:
            return "\033[33m"; // yellow
        case LogLevel::ERROR:
            return "\033[31m"; // red
        case LogLevel::DEBUG:
            return "\033[35m"; // magenta
        }
        return "\033[0m";
    }

} // namespace Core
