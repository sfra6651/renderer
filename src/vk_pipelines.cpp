#include <vk_pipelines.h>
#include <fstream>
#include <vk_types.h>
#include <vk_initializers.h>


VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
  // make viewport state from our stored viewport and scissor.
  // at the moment we wont support multiple viewports or scissors
  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;

  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  // setup dummy color blending. We arent using transparent objects yet
  // the blending is just "no blend", but we do write to the color attachment
  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &this->colorBlendAttachment;

  // completely clear VertexInputStateCreateInfo, as we have no need for it
  VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  // build the actual pipeline
  // we now use all of the info structs we have been writing into into this one
  // to create the pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  // connect the renderInfo to the pNext extension mechanism
  pipelineInfo.pNext = &this->renderInfo;

  pipelineInfo.stageCount = (uint32_t)this->shaderStages.size();
  pipelineInfo.pStages = this->shaderStages.data();
  pipelineInfo.pVertexInputState = &_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &this->inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &this->rasterizer;
  pipelineInfo.pMultisampleState = &this->multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &this->depthStencil;
  pipelineInfo.layout = this->pipelineLayout;

  // dynamic state
  VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dynamicInfo.pDynamicStates = &state[0];
  dynamicInfo.dynamicStateCount = 2;

  pipelineInfo.pDynamicState = &dynamicInfo;

  // its easy to error out on create graphics pipeline, so we handle it a bit
  // better than the common VK_CHECK case
  VkPipeline newPipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
      logErr("failed to create pipeline");
      return VK_NULL_HANDLE; // failed to create graphics pipeline
  } else {
      return newPipeline;
  }
}

void PipelineBuilder::clear()
{
  // clear all of the structs we need back to 0 with their correct stype

  this->inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

  this->rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

  this->colorBlendAttachment = {};

  this->multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

  this->pipelineLayout = {};

  this->depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

  this->renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

  this->shaderStages.clear();
}


bool vkutil::load_shader_module(
  const char* filePath,
  VkDevice device,
  VkShaderModule* outShaderModule
) {
  // open the file. With cursor at the end
   std::ifstream file(filePath, std::ios::ate | std::ios::binary);

   if (!file.is_open()) {
       return false;
   }

   // find what the size of the file is by looking up the location of the cursor
   // because the cursor is at the end, it gives the size directly in bytes
   size_t fileSize = (size_t)file.tellg();

   // spirv expects the buffer to be on uint32, so make sure to reserve a int
   // vector big enough for the entire file
   std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

   // put file cursor at beginning
   file.seekg(0);

   // load the entire file into the buffer
   file.read((char*)buffer.data(), fileSize);

   // now that the file is loaded into the buffer, we can close it
   file.close();

   // create a new shader module, using the buffer we loaded
   VkShaderModuleCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.pNext = nullptr;

   // codeSize has to be in bytes, so multply the ints in the buffer by size of
   // int to know the real size of the buffer
   createInfo.codeSize = buffer.size() * sizeof(uint32_t);
   createInfo.pCode = buffer.data();

   // check that the creation goes well.
   VkShaderModule shaderModule;
   if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
       return false;
   }
   *outShaderModule = shaderModule;
   return true;
}

