#include "vk_engine.h"

int main(int, char**) {
  VulkanEngine engine;
  engine.init();
  engine.run();
  engine.cleanup();
  return 0;
}
