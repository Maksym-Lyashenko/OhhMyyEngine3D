#pragma once

#include "core/Logger.h"

namespace Platform::Guards
{

    class LoggerGuard
    {
    public:
        explicit LoggerGuard(const std::string &filename = "logs/engine.log")
        {
            Core::Logger::init(filename);
        }

        ~LoggerGuard()
        {
            Core::Logger::shutdown();
        }

        LoggerGuard(const LoggerGuard &) = delete;
        LoggerGuard &operator=(const LoggerGuard &) = delete;
    };
} // namespace Platform::Guards
