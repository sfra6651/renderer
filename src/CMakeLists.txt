add_executable(renderer
  main.cpp
  vk_engine.cpp
  vk_initializers.cpp
  vk_images.cpp
  vk_descriptors.cpp
)

target_include_directories(renderer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(renderer PRIVATE
  Vulkan::Vulkan
  SDL3::SDL3
  vk-bootstrap::vk-bootstrap
  vma
  glm::glm
  stb_image
  imgui
  fastgltf::fastgltf
)

if(LINUX)
  target_link_libraries(renderer PRIVATE X11 Xrandr)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(renderer PRIVATE DEBUG)
endif()
