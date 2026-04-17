cmake_minimum_required(VERSION 3.24)
project(renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Output: bin/ for the executable, bin/shaders/ for SPIR-V
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Cache CPM downloads inside third_party so deps live in-repo and survive clean builds
set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/third_party CACHE PATH "")
include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/CompileShaders.cmake)

# --- Vulkan SDK (system-installed) ----------------------------------------
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

# --- Third-party libraries via CPM ----------------------------------------
CPMAddPackage(
  NAME vk-bootstrap
  GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
  GIT_TAG v1.4.307
)

CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.1.0
)

CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.2.0
  OPTIONS
    "SDL_TEST_LIBRARY OFF"
    "SDL_SHARED ON"
    "SDL_STATIC OFF"
)

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.1
)

CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG f0569113c93ad095470c54bf34a17b36646bbbb5
  DOWNLOAD_ONLY YES
)
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.5-docking
  DOWNLOAD_ONLY YES
)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL3::SDL3)

CPMAddPackage(
  NAME fastgltf
  GITHUB_REPOSITORY spnda/fastgltf
  GIT_TAG v0.8.0
)

# VMA is header-only; one TU in src/ defines the impl.
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${VulkanMemoryAllocator_SOURCE_DIR}/include)
target_link_libraries(vma INTERFACE Vulkan::Vulkan)

# --- Source ---------------------------------------------------------------
add_subdirectory(src)

# --- Shaders --------------------------------------------------------------
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/shaders/*.vert
  ${CMAKE_SOURCE_DIR}/shaders/*.frag
  ${CMAKE_SOURCE_DIR}/shaders/*.comp
)
compile_shaders(renderer "${SHADER_SOURCES}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

# --- Platform tweaks ------------------------------------------------------
if(APPLE)
  set_target_properties(renderer PROPERTIES
    BUILD_RPATH "@executable_path;@loader_path"
    INSTALL_RPATH "@executable_path;@loader_path"
  )
endif()
