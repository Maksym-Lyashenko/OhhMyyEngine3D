#include "core/VulkanRenderer.h"

#include "core/Logger.h"

int main()
{
    try
    {
        Core::Logger::init();

        Core::VulkanRenderer renderer;
        renderer.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        Core::Logger::log(Core::LogLevel::ERROR, e.what());
        return EXIT_FAILURE;
    }
}
