#pragma once

#include <string>
#include <fstream>
#include <atomic>

namespace Core
{

    enum class LogLevel
    {
        INFO,
        WARNING,
        ERROR,
        DEBUG
    };

    /**
     * @brief Simple thread-safe logger to console + file.
     *
     * Usage:
     *   Core::Logger::init("logs/engine.log"); // creates logs/engine_YYYY-MM-DD_HH-MM-SS.log
     *   Core::Logger::setLevel(Core::LogLevel::INFO); // optional: filter out lower severities
     *   CORE_LOG_INFO("Hello");
     *   Core::Logger::shutdown();
     */
    class Logger
    {
    public:
        /// Initialize logger (creates a timestamped file based on \p filename).
        static void init(const std::string &filename = "engine.log") noexcept;

        /// Close log file.
        static void shutdown() noexcept;

        /// Log a message at a given level.
        static void log(LogLevel level, const std::string &message) noexcept;

        /// Set minimal level to be printed/stored (default: INFO).
        static void setLevel(LogLevel level) noexcept;

        /// Enable/disable ANSI colors in console (default: enabled if possible).
        static void enableColors(bool enabled) noexcept;

    private:
        static std::ofstream logFile;
        static std::atomic<bool> colorsEnabled;
        static std::atomic<int> minLevel;

        static const char *levelToString(LogLevel level) noexcept;
        static const char *levelToColor(LogLevel level) noexcept;

        static void initConsoleVT() noexcept; // Windows: enable ANSI colors
    };

} // namespace Core

// Convenience macros
#define CORE_LOG_INFO(msg) ::Core::Logger::log(::Core::LogLevel::INFO, (msg))
#define CORE_LOG_WARN(msg) ::Core::Logger::log(::Core::LogLevel::WARNING, (msg))
#define CORE_LOG_ERROR(msg) ::Core::Logger::log(::Core::LogLevel::ERROR, (msg))
#define CORE_LOG_DEBUG(msg) ::Core::Logger::log(::Core::LogLevel::DEBUG, (msg))
