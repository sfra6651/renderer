# Vulkan Renderer

Learning project building a Vulkan rendering pipeline and camera system from scratch in C++20, following along with [vkguide.dev](https://vkguide.dev) (Victor Blanco). Targets modern Vulkan 1.3 (dynamic rendering, sync2, descriptor indexing, buffer device address) on macOS and Linux. Goal: deeply understand every stage of the GPU pipeline and the camera's world-to-clip transforms — no magic, no hidden state.

**History:** project originally followed the vulkan.org tutorial. That tree lives on the `vulkan-org-tutorial-old` branch. The port to vkguide is the current `main`.

## Learning Project

When the user asks how to do something, **guide them through the approach first** rather than jumping straight to implementation. Explain concepts, trade-offs, and reasoning so they can make informed decisions and learn. Only write code when explicitly asked to implement.

## Build & Run

```bash
./run        # Debug build and run
./run -r     # Release build and run
```

Requires a system-installed Vulkan SDK (the `run` script fails early if not found). All other dependencies are fetched by CPM into `third_party/` on first configure.

## Stack

- **C++20**, CMake 3.24+
- **[SDL3](https://github.com/libsdl-org/SDL)** — windowing + input
- **[vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap)** — Vulkan instance/device selection
- **[VMA](https://github.com/GPUOpen-LibrariesAndSDK/VulkanMemoryAllocator)** — GPU memory allocator
- **[GLM](https://github.com/g-truc/glm)** — math
- **[stb_image](https://github.com/nothings/stb)** — image loading
- **[dear imgui](https://github.com/ocornut/imgui)** — debug UI
- **[fastgltf](https://github.com/spnda/fastgltf)** — glTF loading
- User's own `lib/utils.h` for logging (`log`, `logErr`) instead of `{fmt}`

Dependency management: [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake), pinned tags, sources cached in `third_party/`.

## Project Structure

```
renderer/
├── CMakeLists.txt            # top-level: project, deps, shader build, platform tweaks
├── run                       # build+run script (mac/linux)
├── cmake/
│   ├── CPM.cmake             # dependency fetcher
│   └── CompileShaders.cmake  # shader → SPIR-V build rule
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp              # entry point
│   ├── vk_engine.{h,cpp}     # main engine class (VulkanEngine)
│   ├── vk_initializers.{h,cpp}
│   ├── vk_images.{h,cpp}
│   ├── vk_types.h            # common Vulkan includes, VK_CHECK macro
│   └── lib/                  # user helpers: utils.h (logging), osScaling.h
├── shaders/                  # GLSL source (.vert, .frag, .comp)
├── assets/
│   ├── models/               # glTF / OBJ
│   └── textures/
├── bin/                      # executable + compiled shaders (gitignored)
├── build/                    # cmake build tree (gitignored)
└── third_party/              # fetched dep sources (gitignored)
```

Shaders in `shaders/*.{vert,frag,comp}` are compiled to SPIR-V at build time and written to `bin/shaders/`. The binary runs from the project root, so it loads shaders via `bin/shaders/foo.spv` and assets via `assets/...`.

## Platform Notes

- **macOS**: Vulkan runs via MoltenVK. We enable `VK_KHR_portability_enumeration` on the instance. We're currently targeting **Vulkan 1.3 core** (dynamic rendering + sync2). If MoltenVK on the user's machine can't satisfy that, fall back to 1.2 + KHR extensions (`VK_KHR_dynamic_rendering`, `VK_KHR_synchronization2`). This is a known potential failure point — investigate here first if device selection fails on mac.
- **Linux**: Vulkan 1.3 core expected on any recent driver. X11 + Xrandr are linked for the OS-scale helper.
- vkguide's tutorial is Windows-oriented. Anywhere it assumes Visual Studio, filesystem paths, or Win32-only APIs, adapt to the CMake/SDL3 setup here.

## Learning Milestones (vkguide chapters)

1. **Initializing Vulkan** — instance, device, swapchain, command buffers, sync
2. **Drawing with compute** — compute shader clears screen
3. **Graphics pipelines** — mesh rendering with dynamic rendering
4. **Descriptor indexing + push constants** — bindless-ish resource access
5. **glTF loading** — fastgltf integration, materials, textures
6. **GPU-driven rendering** — indirect draws, culling

## Conventions

- C++20
- Header includes use path relative to `src/` (e.g. `#include "vk_engine.h"`, `#include "lib/utils.h"`)
- New `.cpp` files must be added to `src/CMakeLists.txt`
- Follow vkguide's `vk_*` file naming (`vk_engine`, `vk_initializers`, `vk_images`, `vk_pipelines`, `vk_descriptors`, `vk_loader`, `vk_types`)
- Use the user's `log()` / `logErr()` helpers from `lib/utils.h`, not `std::cout` or `{fmt}`

## Known Issues

### Linux compositor scaling causes oversized swap chain
On Linux with fractional scaling, the compositor renders at 2x the logical resolution then downscales. Potential fixes:
- Exclusive fullscreen via SDL3's fullscreen-desktop mode
- Manual swap chain extent using display's native resolution
- Render scaling — render to a lower-res offscreen image and blit to the swap chain

Not a priority while hardware handles the extra pixels.
