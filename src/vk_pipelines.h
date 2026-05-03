#pragma once

#include "vk_types.h"

namespace vkutil {

  bool load_shader_module(
    const char* filepath,
    VkDevice device,
    VkShaderModule shaderModule
  ); 

}
