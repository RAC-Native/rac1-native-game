# Acknowledgements

rac1-native is built on the public work of others. Thank you.

## Projects

**[OpenGOAL](https://opengoal.dev/)** ([jak-project on GitHub](https://github.com/open-goal/jak-project)),
the native PC port of the Jak and Daxter games.

- Its reimplementation of the **989snd** sound driver, by Ziemas and the OpenGOAL team, runs this
  port's sound: `third_party/989snd`, taken from jak-project's `game/sound/989snd` and `game/sound/common`
  at commit `efb21c3e`, ISC licence, with the changes listed in `third_party/989snd/README.md`.
- The GS local-memory layout tables in `runtime/gs/gs_emu.cpp` are as OpenGOAL's
  `common/texture/texture_conversion.h` has them (ISC licence).
- The approach itself: porting a PS2 game's code to readable native code, and drawing its 3D with a
  native renderer fed by the game's own decisions instead of emulating the GS. This project follows
  the path OpenGOAL opened.

**[Wrench](https://github.com/chaoticgd/wrench)** by chaoticgd, the asset tool and level editor for the
PS2 Ratchet & Clank games. Its readers were a source of knowledge about the games' file formats (the
disc's table of contents, the overlays' executable container, moby models), and `src/formats/` cites
them where they helped. No Wrench code is included in this repository.

**[PCSX2](https://pcsx2.net/)** ([GitHub](https://github.com/PCSX2/pcsx2)), the PS2 emulator and
debugger. It was the behavioural reference throughout: its debugger for tracing the game, its memory
snapshots as the inputs of the oracle that proves each port, and its source for how the EE and VUs
round floating-point results.

**[Ghidra](https://ghidra-sre.org/)** and the
**[Emotion Engine plugin, ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded)**
by chaoticgd, the disassembler and decompiler used to read the game's MIPS and VU code.

**[Redump](http://redump.org/)**, whose disc records identify a good dump of the game; the setup
checks the image's size against it.

## Libraries

See [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) for versions and licences.

- [GLFW](https://github.com/glfw/glfw): the window, the OpenGL context, keyboard, mouse and gamepads.
- [GLM](https://github.com/g-truc/glm): vector and matrix math.
- [Dear ImGui](https://github.com/ocornut/imgui): the debug tools' interface.
- [TinyGLTF](https://github.com/syoyo/tinygltf), with [nlohmann/json](https://github.com/nlohmann/json)
  and [stb_image](https://github.com/nothings/stb): glTF loading for the older model renderer.
- [miniaudio](https://github.com/mackron/miniaudio): audio output.
- [glad](https://github.com/Dav1dde/glad): the generated OpenGL loader.
- [volk](https://github.com/zeux/volk) and [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers):
  the experimental Vulkan renderer's function loader and the API's headers.
- [shaderc](https://github.com/google/shaderc)'s `glslc`, or [glslang](https://github.com/KhronosGroup/glslang)'s
  `glslangValidator`, optional, from the system: they compile the Vulkan renderer's shaders when the game is built.
- [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB): the community's database of
  controller mappings.
- [FFmpeg](https://ffmpeg.org/) (`libavcodec`, `libavutil`), optional, from the system: the movies'
  MPEG-2 video.

## The game

*Ratchet & Clank* was made by Insomniac Games and published by Sony Computer Entertainment in 2002.
This project is not affiliated with or endorsed by either. It contains none of the game's code or
data; you need your own copy of the game.
