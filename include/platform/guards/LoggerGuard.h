#pragma once

#include <string>

namespace Core
{
    enum class LogLevel; // forward-declare to avoid pulling Logger.h here
}

/**
 * @brief RAII guard for Core::Logger lifecycle.
 *
 * On construction: initializes logger and optionally sets min level and color mode.
 * On destruction:  shuts down logger.
 *
 * Notes:
 *  - Non-copyable and non-movable to keep ownership explicit.
 */
namespace Platform::Guards
{

    class LoggerGuard
    {
    public:
        // Basic ctor: only filename (keeps current defaults for level/colors)
        explicit LoggerGuard(const std::string &filename = "logs/engine.log");

        // Extended ctor with level + colors
        LoggerGuard(const std::string &filename,
                    Core::LogLevel minLevel,
                    bool enableColors);

        ~LoggerGuard() noexcept;

        LoggerGuard(const LoggerGuard &) = delete;
        LoggerGuard &operator=(const LoggerGuard &) = delete;
        LoggerGuard(LoggerGuard &&) = delete;
        LoggerGuard &operator=(LoggerGuard &&) = delete;

    private:
        bool initialized_{false};
    };

} // namespace Platform::Guards
