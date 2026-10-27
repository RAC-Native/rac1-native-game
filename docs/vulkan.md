# The Vulkan renderer (experimental)

An opt-in second backend for the same renderer: Options > Graphics > **Renderer** (OpenGL, the
default, or Vulkan), or `RAC1_RENDERER=vulkan` for one run. It draws exactly what the OpenGL path
draws - the [MRP](rendering.md) world, sky, mobys, particles, effects, VU1 program 7's water and
lava surfaces and drop shadows, the [modern lighting](lighting.md) (sun cascades, point lights and
their shadow cubes, ambient occlusion, sun rays, emissive surfaces, [materials](materials.md)), the
[ambient particles](particles.md) and the GS replica's 2D - from the same scene the game's own render code
decides; only the API calls differ. Nothing of the game's state depends on it.

The renderer is read once, when the window is made: a change on the Graphics page takes effect at
the next start. When Vulkan cannot start (no Vulkan 1.3 driver, a device without the features
below, a build without the shaders), the game says so in its log and opens with OpenGL; the saved
choice is kept.

## How it is built

- `runtime/vk/vk_context.{h,cpp}`: the device, the swapchain, the frames in flight (2), a 32 MB
  ring per frame for the per-frame data (vertices, uniforms, uploads), transient descriptor pools
  reset each frame, a sampler cache, render targets with dynamic rendering, and helpers that mirror
  the GL calls the renderer used (viewport, scissor, a stack of targets, blits, copies, uploads,
  readbacks). The loader is [volk](https://github.com/zeux/volk) (no Vulkan SDK needed to build);
  the headers are Vulkan-Headers. Both come with the build (`cmake/dependencies.cmake`).
- The scene is drawn into an RGBA8 colour image with a depth-and-stencil image (D32S8, else D24S8)
  in OpenGL's orientation (row 0 the bottom), blitted onto the swapchain at the present. The
  depth uses OpenGL's -1..1 range (`VK_EXT_depth_clip_control`) and the last vertex provokes
  (`VK_EXT_provoking_vertex`), so the shaders' arithmetic is OpenGL's.
- Each rendering module keeps its OpenGL code and gains a Vulkan branch (`vk::is_active()`):
  `runtime/gs/gs_draw.cpp` (the GS replica: `draw_vk`, the same primitive loop as `draw`, its batches
  drawn with Vulkan pipelines keyed by their blend, depth and colour-mask state),
  `mrp_world_renderer.cpp`, `mrp_tfrag_gpu.cpp` (the tfrag compute shader), `mrp_moby_renderer.cpp`,
  `mrp_fx.cpp`, `mrp_shadows.cpp`, `mrp_lighting.cpp`; `orig_main.cpp`'s frame (the clears, the
  render scale's buffer, the frozen frames of the menus and fades, the movies, the screenshots).
  The last frame of the game shown is kept in the 3D's colour image (cleared only at the next frame's start) and copied for the
  pause menu, a fade or a store only when one of them reads it (OpenGL draws into the window's own framebuffer, gone at the swap:
  it copies every frame). The tfrag compute shader's resident colours are a buffer per frame in flight, each brought up to date
  when its frame comes (the changes of the uploads it missed), so the frame before's compute never reads colours being written.
- The shaders are GLSL in `tools/gen_vk_shaders.py` (the OpenGL ones, made Vulkan's: push
  constants and descriptor sets instead of uniforms), compiled to SPIR-V **at build time** by `glslc`
  (shaderc, part of the Vulkan SDK; the `glslc` package on Debian / Ubuntu / Fedora, `shaderc` on
  Arch, `mingw-w64-clang-x86_64-shaderc` in MSYS2) or `glslangValidator`, into the build directory
  (`build/generated_vk/`). Nothing generated is kept in the repository. Without either compiler the
  build still succeeds, with the renderer left out (its shaders empty: it never starts). The setup
  lists `glslc` as optional; the private MSYS2 that `setup.cmd` and the launcher download on Windows
  does not include it (see [Setup](setup.md#on-windows) and [The launcher](launcher.md#what-it-runs)).
- The debug menu (ImGui's Vulkan backend) and the Profiler work with it: the GPU's time per stage
  from Vulkan timestamps (a query pool, read back frames later, never waited for), the GPU's memory
  from `VK_EXT_memory_budget`, and "Count Vulkan allocations" (the renderer's images and buffers by
  use) in place of the GL object count. The debug crosshair's world point (the Lighting tab) reads
  the depth two frames late, without a wait.

Requirements: Vulkan 1.3 (dynamic rendering, synchronization2), with `samplerAnisotropy`,
`fillModeNonSolid`, `imageCubeArray`, `multiDrawIndirect`, `drawIndirectFirstInstance`,
`depthClamp`, `depthBiasClamp`, `shaderClipDistance`, `dualSrcBlend`, `shaderFloat64` (the tfrag
compute shader's EE rounding), `shaderInt64` and scalar block layout; `hostQueryReset` for the
profiler's GPU times.

## Performance

Measured headless and unpaced (each frame as fast as it goes) on the development machine (Ryzen AI 9
HX 470, Radeon 890M integrated, Mesa 26: radeonsi for OpenGL, RADV for Vulkan), with Lighting High,
Dynamic Shadows, Ambient Occlusion SSAO, Level of Detail High, Fill Screen; frames 1801 to 2400 of
`RAC1_GOTO=<level>@60 RAC1_SIM_INPUT=forward`, `RAC1_PROFILER_DUMP=600`. Frame time in ms (fps),
the main thread's own work, the GPU's busy time from the timestamps (the kernel's account in
brackets). Run to run, the frame rates vary by about 5 % (Veldin 1920x540 again: OpenGL 134 fps,
Vulkan 162):

| Scene | OpenGL frame | Vulkan frame | OpenGL main / GPU | Vulkan main / GPU |
|---|---|---|---|---|
| Veldin 1920x540 | 7.92 (126) | 5.87 (170) | 6.67 / 6.73 (5.53) | 4.55 / 4.10 (4.20) |
| Aridia 1920x540 | 11.78 (85) | 10.41 (96) | 8.72 / 7.75 (7.11) | 7.97 / 6.51 (7.09) |
| Kerwan 1920x540 | 10.68 (94) | 9.66 (104) | 8.46 / 7.40 (6.90) | 7.67 / 6.25 (6.54) |
| Rilgar 1920x540 | 11.16 (90) | 10.73 (93) | 9.58 / 7.56 (6.72) | 8.51 / 6.45 (6.46) |
| Veldin 3840x1080 | 11.35 (88) | 9.30 (107) | 9.49 / 10.60 (8.91) | 7.46 / 7.83 (8.60) |
| Aridia 3840x1080 | 13.87 (72) | 14.20 (70) | 10.68 / 11.04 (10.98) | 10.84 / 10.00 (10.04) |
| Kerwan 3840x1080 | 13.85 (72) | 13.40 (75) | 10.91 / 11.38 (9.85) | 10.36 / 9.55 (9.90) |
| Rilgar 3840x1080 | 12.84 (78) | 12.56 (80) | 11.03 / 10.14 (9.40) | 9.98 / 8.77 (9.24) |

Both are bound by the main thread (the game's own code, the same in both, is most of it). Vulkan's
GPU time is lower, mostly in the sun's cascades and the mobys (the casters' and the mobys' draws
cost less to submit); the occlusion and the world cost the same. Its CPU work in the draw itself is
a little higher where the frame's data is copied into the frame's ring (a "Draw (other)" stage of
1.4 to 4.9 ms against OpenGL's 1.1 to 2.3, where the driver does that copy on its own thread); in
all, the main thread works the same or less. The process's resident memory is about 70 MB lower;
its VRAM about 80 to 160 MB higher as measured (128 MB of it the two frame rings, which a frame used about 6 MB of: since made 32 MB each)
and its GTT lower.

## What it does not do

- The legacy host renderers (the old world, sky and moby drawers, `RAC1_TITLE_HOST`,
  `RAC1_MOBY_GPU`): not drawn. With the MRP world off (`RAC1_MRP=0`, `RAC1_WORLD_MRP=0`) the world
  comes from the frame's VU1 chain through the GS replica (as `RAC1_WORLD_FAITHFUL=1`), so
  `RAC1_MRP=0` is not the OpenGL path's picture.
- The testing comparisons that draw both renderers offscreen (`RAC1_MRP_DIFF`, `RAC1_MOBY_DIFF`,
  the world check), the debug menu's Wireframe and Shading views, `RAC1_FRAME_PROF=2`'s GPU columns
  (the Profiler's own timings work), `RAC1_LAMP_SCAN` and `RAC1_SUN_COVER`'s probe.
- Switching the renderer while the game runs.

## Checking it

Headless runs work as with OpenGL (an invisible window, no swapchain; screenshots read back from
the frame's image):

```sh
RAC1_RENDERER=vulkan RAC1_HEADLESS=1 RAC1_FRAMES=600 RAC1_SHOTS=600 RAC1_GOTO=0@60 RAC1_SIM_INPUT=forward ./build/rac1-native
```

The reference images are OpenGL's and stay so; a Vulkan picture is compared with OpenGL's of the
same run: on a tour of the 19 levels (two pictures each, Daino's settings) the mean difference is 0.01
to 0.3 of a level in 255, under 0.5 % of the pixels off by more than 8 - single pixels at polygon
edges and alpha-tested leaves, where the two drivers rasterise a hair differently. The texture level
of the "Full" Texture Detail is computed in the shaders (isotropic, as OpenGL's `textureQueryLod`
gives it): Vulkan drivers may fold the sampler's anisotropy into `textureQueryLod`, which picked
sharper levels than OpenGL (the Gadgetron signs on Aridia and Hoven). `RAC1_VK_VALIDATION=1` turns the
Khronos validation layer on when it is installed. `RAC1_PROFILER_DUMP=300` prints both renderers'
stage times for a comparison.
