#pragma once

#include "vk_types.h"

namespace vkutil {

  bool load_shader_module(
    const char* filepath,
    VkDevice device,
    VkShaderModule* outShaderModule
  ); 

}

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineLayout pipelineLayout;
  VkPipelineDepthStencilStateCreateInfo depthStencil;
  VkPipelineRenderingCreateInfo renderInfo;
  VkFormat colorAttachmentFormat;

  PipelineBuilder() { clear(); }

  void clear();

  VkPipeline build_pipeline(VkDevice device);
};
