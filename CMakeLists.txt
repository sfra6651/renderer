cmake_minimum_required(VERSION 3.20)
project(renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.313
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw glm)
FetchContent_Populate(VulkanHeaders)

add_executable(renderer
    src/main.cpp
        src/myVulkanHelpers.h
)

target_include_directories(renderer BEFORE PRIVATE ${vulkanheaders_SOURCE_DIR}/include)
target_include_directories(renderer PRIVATE src)
target_compile_definitions(renderer PRIVATE
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
)
target_link_libraries(renderer PRIVATE Vulkan::Vulkan glfw glm::glm)

# Slang shader compilation
find_program(SLANGC_EXECUTABLE slangc REQUIRED)

set(SHADER_SLANG_SOURCES ${CMAKE_SOURCE_DIR}/src/shaders/shader.slang)

set(SHADERS_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
set(SLANG_SPV ${SHADERS_DIR}/slang.spv)

add_custom_command(
    OUTPUT ${SLANG_SPV}
    COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SLANG_SOURCES}
        -target spirv -profile spirv_1_4 -emit-spirv-directly
        -fvk-use-entrypoint-name
        -entry vertMain -entry fragMain
        -o ${SLANG_SPV}
    DEPENDS ${SHADER_SLANG_SOURCES}
    COMMENT "Compiling Slang shaders"
    VERBATIM
)

add_custom_target(shaders DEPENDS ${SLANG_SPV})
add_dependencies(renderer shaders)

if(LINUX)
    target_link_libraries(renderer PRIVATE X11 Xrandr)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(renderer PRIVATE DEBUG)
endif()
