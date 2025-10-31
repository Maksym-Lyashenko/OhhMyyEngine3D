cmake_minimum_required(VERSION 3.10)
project(imgui)

set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    # imgui_demo.cpp  # optional

    # backends
    backends/imgui_impl_glfw.cpp
    backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

# include dirs for core + backends
target_include_directories(imgui
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# --- Vulkan integration fix ---
# Find Vulkan (in case it wasn't propagated from parent)
find_package(Vulkan REQUIRED)

# link to glfw and Vulkan so backends can include vulkan/vulkan.h and GLFW/glfw3.h
target_link_libraries(imgui
    PUBLIC
      glfw
      Vulkan::Vulkan
)

# also make sure Vulkan headers are visible to imgui_impl_vulkan.cpp
target_include_directories(imgui
    PUBLIC
      ${Vulkan_INCLUDE_DIRS}
)

# On Windows, if Vulkan SDK is set via ENV, include it explicitly
if (DEFINED ENV{VULKAN_SDK})
    message(STATUS "imgui: adding Vulkan include from ENV: $ENV{VULKAN_SDK}/Include")
    target_include_directories(imgui PUBLIC $ENV{VULKAN_SDK}/Include)
endif()
