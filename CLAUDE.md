# Vulkan Renderer

Learning project building a Vulkan rendering pipeline and camera system from scratch in C++20. The goal is to deeply understand every stage of the GPU rendering pipeline and how the camera transforms world space to screen space — no magic, no hidden state.

## Learning Project

The user is learning Vulkan and GPU rendering from the ground up. When they ask how to do something, **guide them through the approach first** rather than jumping straight to implementation. Explain the concepts, trade-offs, and reasoning so they can make informed decisions and learn. Only write code when explicitly asked to implement.

## Build & Run

```bash
./run        # Debug build and run
./run -r     # Release (optimized) build and run
```

Dependencies are fetched automatically via CMake FetchContent where possible.

## Project Structure

```
src/
└── main.cpp    # Entry point
```

## Learning Milestones

1. **Clear the screen** — instance, device, swap chain, render pass, command buffers, synchronization
2. **Draw a triangle** — vertex buffers, SPIR-V shaders, graphics pipeline
3. **3D shapes** — cube, sphere/circle; index buffers, depth buffering, 3D vertex data
4. **Camera system** — view/projection matrices, world-to-clip space transforms, camera controls
5. **Lighting** — normals, diffuse/specular shading, fragment shader math
6. **Textured floor + model** — image loading, samplers, descriptor sets, model loading

## Known Issues

### Linux compositor scaling causes oversized swap chain
On Linux with fractional scaling, the compositor renders at 2x the logical resolution then downscales. A full-screen borderless window results in a ~5120x2806 swap chain instead of the native 3840x~2160. Potential fixes:
- **Exclusive fullscreen** — pass monitor to `glfwCreateWindow` to bypass compositor
- **Manual swap chain extent** — use `glfwGetVideoMode` native resolution instead of `capabilities.currentExtent`
- **Render scaling** — render to a lower-res offscreen image and blit to the swap chain

Not a priority while hardware can handle the extra pixels.

## Conventions

- C++20, compiled with CMake 3.20+
- Header includes use path relative to `src/` (e.g. `#include "renderer/renderer.h"`)
- Private member variables use trailing underscore (`swapChainExtent_`)
- New `.cpp` files must be added to the `add_executable` list in `CMakeLists.txt`
