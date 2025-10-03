#include "rhi/vk/VulkanRenderer.h"

#include "core/Logger.h"
#include "platform/guards/LoggerGuard.h"

int main()
{
    try
    {
        Platform::Guards::LoggerGuard logGuard;

        Vk::VulkanRenderer renderer;
        renderer.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        Core::Logger::log(Core::LogLevel::ERROR, e.what());
        return EXIT_FAILURE;
    }
}
