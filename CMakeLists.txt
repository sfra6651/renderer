cmake_minimum_required(VERSION 3.20)
project(renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

add_executable(renderer
    src/main.cpp
)

target_include_directories(renderer PRIVATE src)
target_link_libraries(renderer PRIVATE Vulkan::Vulkan glfw X11 Xrandr)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(renderer PRIVATE DEBUG)
endif()
