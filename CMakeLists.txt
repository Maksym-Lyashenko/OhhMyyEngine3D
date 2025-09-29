cmake_minimum_required(VERSION 3.25)
project(OhhMyyEngine3D LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GLFW (offline, from libs/)
add_subdirectory(libs/glfw)

# Sources
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Vulkan SDK
find_package(Vulkan REQUIRED)

if (TARGET Vulkan::Vulkan)
    message(STATUS "Using Vulkan target: Vulkan::Vulkan")
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
else()
    message(STATUS "Using Vulkan legacy variables")
    target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
endif()

# Fallback from ENV (Windows quirk)
if (DEFINED ENV{VULKAN_SDK})
    message(STATUS "Adding Vulkan include from ENV: $ENV{VULKAN_SDK}/Include")
    target_include_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/Include)
    target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/Lib)
endif()

# Link GLFW last
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# --- Copy shaders to runtime dir (Debug/Release) ---
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

# Warnings
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
