# Metal (macOS)

On macOS the game draws with Metal. OpenGL stops at 4.1 there (no compute shaders), and the
[Vulkan](vulkan.md) renderer was never tried through MoltenVK, so Metal is the macOS renderer and
the only one: Options > Graphics > Renderer shows "Metal", and `RAC1_RENDERER` is ignored. If Metal
cannot start, the game says so and exits (there is no OpenGL to fall back to).

The OpenGL and Vulkan renderers are unchanged: Linux and Windows build and run as before.

## How it works

The Metal renderer is not a third copy of the renderer. It runs the Vulkan renderer's code as it is,
on a small layer that implements Vulkan's calls with Metal:

- `runtime/mtl/mtl_context.{h,mm}`: the Metal device, the window's layer, the frame's command buffer,
  the render targets, pipelines, buffers, textures, samplers, the frame's ring buffer, ImGui's Metal
  backend.
- `runtime/mtl/vk_on_metal.mm`: the `rac1::vk` API (`runtime/vk/vk_context.h`, unchanged) on top of
  `rac1::mtl`, and volk's function pointers (`vkCmdDraw`, `vkCreatePipeline`, `vkUpdateDescriptorSets`
  ...) pointed at Metal versions. On macOS it replaces `runtime/vk/vk_context.cpp`.
- The shaders: `tools/gen_vk_shaders.py` (its `metal` mode) gives the GLSL, `tools/gen_metal_shaders.py`
  turns it into MSL with SPIRV-Cross into `runtime/mtl/shaders/`, with a table of where each Vulkan
  (set, binding) lands in Metal (`runtime/mtl/mtl_shader_table.inc`). CMake embeds them
  (`cmake/embed_metal_shaders.cmake`) and Metal compiles them at start-up.

After changing a shader in `tools/gen_vk_shaders.py`, run `python3 tools/gen_metal_shaders.py`.

## Differences from Vulkan, handled in the layer

- **The provoking vertex.** The Vulkan renderer takes a triangle's flat inputs from its last vertex
  (`VK_EXT_provoking_vertex`, as OpenGL), the way the game's data writes them; Metal takes them from
  the first, with no switch. The fragment shaders with flat inputs carry the marker
  `// rac1: flat inputs`; their triangles are drawn with the indices turned (a, b, c) -> (c, a, b),
  the same triangle and winding with the last vertex first. Indices and indirect commands the CPU
  wrote are turned into the frame's ring; the tfrag compute shader writes its triangles already
  turned. (MSL 4's `vertex_value` would avoid this but gave wrong results on an M3.)
- **ImGui (the debug menu).** `ImGui_ImplMetal`, with its font texture made at start-up rather than
  in the first frame.
- **The tfrag compute shader's doubles** (the EE's rounding): Metal has no `double`, so the generator
  does them with 64-bit integers, the same round-toward-zero adds and multiplies bit for bit
  (`tests/metal/tfrag_double_check.mm` checks them against the CPU's).

## Building

```
python3 tools/setup.py            # macOS 13 or later, Apple silicon, Xcode's command line tools
./build/rac1-native               # from the repository's root
```

FFmpeg is optional, as on the other systems: `brew install ffmpeg` for the movies (pkg-config is not needed on macOS).
