#include "platform/guards/LoggerGuard.h"
#include "core/Logger.h"

namespace Platform::Guards
{

    LoggerGuard::LoggerGuard(const std::string &filename)
    {
        Core::Logger::init(filename);
        initialized_ = true;
    }

    LoggerGuard::LoggerGuard(const std::string &filename,
                             Core::LogLevel minLevel,
                             bool enableColors)
    {
        Core::Logger::init(filename);
        Core::Logger::setLevel(minLevel);
        Core::Logger::enableColors(enableColors);
        initialized_ = true;
    }

    LoggerGuard::~LoggerGuard() noexcept
    {
        if (initialized_)
        {
            Core::Logger::shutdown();
        }
    }

} // namespace Platform::Guards
