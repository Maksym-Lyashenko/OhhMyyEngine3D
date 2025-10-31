cmake_minimum_required(VERSION 3.25)
project(OhhMyyEngine3D LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# GLFW (offline, from libs/)
add_subdirectory(libs/glfw)
add_subdirectory(libs/glm)
add_subdirectory(libs/meshoptimizer)
add_subdirectory(libs/imgui)


# cgltf from libs header-only
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libs/cgltf)

# stb_image from libs header-only
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libs/stb)

# vma from libs header-only
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libs/vma/include)


# Sources
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp libs/libs.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE include)

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
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm cgltf stb_image meshoptimizer imgui vma)
target_compile_definitions(${PROJECT_NAME} PRIVATE OME3D_USE_STB=1)

# --- Copy shaders to runtime dir (Debug/Release) ---
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

# --- Copy assets to runtime dir (Debug/Release) ---
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Warnings
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
