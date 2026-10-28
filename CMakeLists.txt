cmake_minimum_required(VERSION 3.20)
# the release's version (semantic): bumped at each release, with its tag vX.Y.Z (the in-game update check compares
# it with the latest release's: docs/settings.md)
project(rac1-native VERSION 0.6.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJC OBJCXX)  # (the Metal renderer: docs/metal.md; OBJC too, else GLFW's .m files build as C++)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# (a Debug build runs many times slower than the game needs)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR "rac1-native builds with GCC or clang (on Windows: clang or GCC for MinGW-w64, see docs/platforms.md); "
                        "this is ${CMAKE_CXX_COMPILER_ID}.")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
# the game's code runs with the host FPU rounding toward zero (runtime/ee/ps2_fpu.h): no folding or
# moving of float operations that assumes round-to-nearest, and no a * b + c fused into one rounding
# (the same float operations from GCC and clang, on Linux and Windows: docs/platforms.md)
add_compile_options(-frounding-math -ffp-contract=off)
# the emulated memory is read and written through several types (Vec4 floats, 32-bit words, bytes): no type-based
# alias analysis, which lets the optimizer reorder or drop those accesses (clang's -O2 did, in ovl_24_25.cpp: the moby
# grid stayed empty, the mobys' state went wrong; GCC's happened not to)
add_compile_options(-fno-strict-aliasing)
# The game is always compiled on the player's own machine (./setup.sh): RAC1_NATIVE_ARCH=ON compiles it for that machine's
# CPU (-march=native: AVX2 / AVX-512 where it has them), which the release build cannot assume. The float rules above stay:
# -ffp-contract=off keeps a * b + c two roundings even where the CPU has FMA (the PS2 has none: a fused multiply-add
# changes the last bit, and the game's state), and nothing like -ffast-math: the game's state stays bit for bit the same
# (checked with RAC1_RAMHASH, the oracle and the reference images: docs/architecture.md). Off by default (a build for
# this CPU runs on no older one); ./setup.sh --native-arch on.
option(RAC1_NATIVE_ARCH "Compile for this machine's CPU (-march=native), the float rules kept" OFF)
if(RAC1_NATIVE_ARCH)
    add_compile_options(-march=native)
    # (Windows with GCC: GCC aligns AVX spills to 32 bytes on a stack Windows aligns to 16 only, GCC bug 54412; the
    # assembler makes its aligned vector moves unaligned ones, binutils 2.38 or newer. clang has no such problem)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wa,-muse-unaligned-vector-move)
    endif()
    message(STATUS "RAC1_NATIVE_ARCH: compiled for this machine's CPU (-march=native)")
endif()

# Windows (MinGW-w64, with clang or GCC): see docs/platforms.md
if(WIN32)
    enable_language(RC)  # (the program's manifest: src/platform/win32/)
    # (the Win32 headers without their min / max macros and rarely used parts)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    # (GCC's assembler needs the large object format for the biggest generated sources; clang picks it itself)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wa,-mbig-obj)
    endif()
endif()

find_package(OpenGL REQUIRED)

# ---- third-party libraries (THIRD_PARTY_NOTICES.md) ----
# kept in third_party/: glad (a generated loader), 989snd (OpenGOAL's, adapted), gamecontrollerdb (data)
add_subdirectory(third_party/glad EXCLUDE_FROM_ALL)
add_subdirectory(third_party/989snd)
# downloaded at configure time: GLFW, GLM, Dear ImGui, TinyGLTF, miniaudio
include(cmake/dependencies.cmake)

# ---- src/platform/ : the operating system's calls, behind one interface (Linux: POSIX; Windows: Win32) ----
if(WIN32)
    add_library(rac1_platform STATIC src/platform/platform_win32.cpp)
    target_link_libraries(rac1_platform PUBLIC winmm psapi)  # (timeBeginPeriod: the system timer's period; GetProcessMemoryInfo)
else()
    # The coroutine's context switch: by hand on x86-64 and AArch64 (each .S assembles to nothing on other
    # architectures), ucontext elsewhere (src/platform/ctx_switch.h). RAC1_UCONTEXT=ON compiles only the ucontext
    # switch (to compare; at run time RAC1_COROUTINE=ucontext picks it on Linux without a rebuild).
    enable_language(ASM)
    add_library(rac1_platform STATIC src/platform/platform_posix.cpp
        src/platform/coroutine_x86_64.S src/platform/coroutine_aarch64.S)
    option(RAC1_UCONTEXT "The coroutine switches with ucontext, not the hand-written switch (to compare)" OFF)
    if(RAC1_UCONTEXT)
        target_compile_definitions(rac1_platform PRIVATE RAC1_CTX_FORCE_UCONTEXT)
    endif()
endif()
target_include_directories(rac1_platform PUBLIC src)

# ---- src/formats/ : the game's file formats (our own decoders) ----
add_library(rac1_io INTERFACE)
target_include_directories(rac1_io INTERFACE src)
target_link_libraries(rac1_io INTERFACE rac1_platform)

add_library(rac1_formats STATIC
    src/formats/scene_wad.cpp
    src/formats/level_textures.cpp
    src/formats/level_mesh.cpp
    src/formats/ratchet_executable.cpp
    src/formats/iso_toc.cpp
    src/formats/level_gameplay.cpp
    src/formats/level_collision.cpp
    src/formats/level_core.cpp
    src/formats/moby_model.cpp
    src/formats/moby_mrp.cpp
    src/formats/world_mrp.cpp
    src/formats/tie_model.cpp
    src/formats/shrub_model.cpp
    src/formats/sky_model.cpp
    src/formats/vag.cpp
    src/formats/sound_bank.cpp
    src/formats/animation.cpp
    src/formats/vif_stream.cpp
    src/formats/tie_packet.cpp
)
target_link_libraries(rac1_formats PUBLIC rac1_io)
target_include_directories(rac1_formats PUBLIC src)
target_link_libraries(rac1_formats PUBLIC tinygltf_headers)

# ---- src/profiler/ : the host's frame profiler (the debug menu's Profiler tab, RAC1_PROFILER_*; openly invented tooling) ----
add_library(rac1_profiler STATIC src/profiler/host_profiler.cpp)
target_include_directories(rac1_profiler PUBLIC src)
target_link_libraries(rac1_profiler PUBLIC rac1_platform glad glfw)

# ---- src/jobs/ : the host's job pool (the renderer's scene building spread over the cores; deterministic; openly invented tooling) ----
add_library(rac1_jobs STATIC src/jobs/job_pool.cpp)
target_include_directories(rac1_jobs PUBLIC src)
target_link_libraries(rac1_jobs PUBLIC rac1_profiler)

# ---- the Vulkan renderer's shaders (docs/vulkan.md): GLSL in tools/gen_vk_shaders.py, compiled to SPIR-V here, at build
# time, by glslc (shaderc, the Vulkan SDK) or glslangValidator, with Python (as the setup); without a compiler they are left out and
# the renderer does not start - the game stays on OpenGL, its default. Nothing of it is kept in the repository.
find_program(RAC1_GLSLC NAMES glslc glslangValidator DOC "the GLSL to SPIR-V compiler for the Vulkan renderer")
find_package(Python3 COMPONENTS Interpreter QUIET)
set(RAC1_VK_SHADERS_DIR ${CMAKE_BINARY_DIR}/generated_vk)
set(RAC1_VK_SHADERS_CPP ${RAC1_VK_SHADERS_DIR}/vk_shaders.cpp)
if(Python3_Interpreter_FOUND)
    set(_rac1_vk_compiler "")
    if(APPLE)  # (the Metal renderer's: runtime/mtl/vk_on_metal.mm, no SPIR-V)
        set(_rac1_vk_compiler metal)
    elseif(RAC1_GLSLC)
        set(_rac1_vk_compiler ${RAC1_GLSLC})
    endif()
    add_custom_command(
        OUTPUT ${RAC1_VK_SHADERS_CPP} ${RAC1_VK_SHADERS_DIR}/runtime/vk/vk_shaders.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_vk_shaders.py ${RAC1_VK_SHADERS_DIR} "${_rac1_vk_compiler}"
        DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_vk_shaders.py ${CMAKE_SOURCE_DIR}/src/game/render/mrp_materials_glsl.inc
        COMMENT "Vulkan shaders (tools/gen_vk_shaders.py)"
        VERBATIM)
    if(NOT RAC1_GLSLC AND NOT APPLE)
        message(STATUS "Vulkan renderer: no glslc / glslangValidator found - built without its shaders (OpenGL only)")
    endif()
    add_custom_target(rac1_vk_shaders DEPENDS ${RAC1_VK_SHADERS_CPP})
else()
    message(FATAL_ERROR "the Vulkan renderer's shaders need Python 3 (tools/gen_vk_shaders.py)")
endif()

add_library(rac1_runtime STATIC
    runtime/kernel/ee_copy.cpp
    runtime/kernel/ee_memory.cpp
    runtime/kernel/disc.cpp
    runtime/kernel/bootstrap.cpp
    runtime/vu/vu_interpreter.cpp
    runtime/ee/ee_interpreter.cpp
    runtime/ee/ee_thread.cpp
    runtime/ee/game_thread.cpp
    runtime/ee/ee_timer.cpp
    runtime/vu/vu_xgkick.cpp
    runtime/vu/vu0.cpp
    runtime/vu/vu1.cpp
    runtime/gs/gs_blend.cpp
    runtime/gs/gs_memory.cpp
    runtime/gs/gs_emu.cpp
    runtime/gs/gs_draw.cpp
    runtime/gs/gs_renderer.cpp
    runtime/gs/static_geom_cache.cpp
    src/game/render/texture_decode.cpp
    runtime/sif/sif_rpc.cpp
    runtime/iop/snd989_iop.cpp
    runtime/iop/snd989_stream.cpp
    runtime/iop/iop_ram.cpp
    runtime/iop/stash_iop.cpp
    runtime/ipu/movie_hw.cpp
    $<$<NOT:$<PLATFORM_ID:Darwin>>:runtime/vk/vk_context.cpp>
    ${RAC1_VK_SHADERS_CPP}
    # file I/O (hardware-adjacent, no game decisions)
    runtime/level/level_loader.cpp
)
target_include_directories(rac1_runtime PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${RAC1_VK_SHADERS_DIR})
target_link_libraries(rac1_runtime PUBLIC rac1_platform rac1_profiler rac1_jobs rac1_formats glad glfw glm_headers OpenGL::GL volk imgui opengoal_989snd)

# ---- the Metal renderer (macOS; docs/metal.md): its shaders are MSL made from the Vulkan renderer's GLSL at build time by
# tools/gen_metal_shaders.py (glslc, then SPIRV-Cross: brew install shaderc spirv-cross), embedded as text and compiled
# when the game starts (no offline Metal compiler needed). Nothing of it is kept in the repository. Vulkan itself is not
# used on macOS.
if(APPLE)
    find_program(RAC1_SPIRV_CROSS NAMES spirv-cross DOC "the SPIR-V to MSL translator for the Metal renderer")
    find_program(RAC1_GLSLC_ONLY NAMES glslc DOC "glslc (shaderc) for the Metal renderer's shaders")
    if(NOT RAC1_GLSLC_ONLY OR NOT RAC1_SPIRV_CROSS)
        message(FATAL_ERROR "the Metal renderer's shaders need glslc and spirv-cross: brew install shaderc spirv-cross")
    endif()
    set(RAC1_MTL_SHADERS_DIR ${CMAKE_BINARY_DIR}/generated_mtl)
    set(RAC1_MTL_SHADERS_INC ${RAC1_MTL_SHADERS_DIR}/mtl_shader_sources.inc)
    add_custom_command(
        OUTPUT ${RAC1_MTL_SHADERS_INC} ${RAC1_MTL_SHADERS_DIR}/mtl_shader_table.inc
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_metal_shaders.py ${RAC1_MTL_SHADERS_DIR} ${RAC1_GLSLC_ONLY} ${RAC1_SPIRV_CROSS}
        DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_metal_shaders.py ${CMAKE_SOURCE_DIR}/tools/gen_vk_shaders.py
                ${CMAKE_SOURCE_DIR}/src/game/render/mrp_materials_glsl.inc
        COMMENT "Metal shaders (tools/gen_metal_shaders.py)"
        VERBATIM)
    # (on macOS the Vulkan renderer's code runs on Metal: runtime/mtl/vk_on_metal.mm in place of runtime/vk/vk_context.cpp)
    target_sources(rac1_runtime PRIVATE runtime/mtl/mtl_context.mm runtime/mtl/vk_on_metal.mm ${RAC1_MTL_SHADERS_INC}
        ${RAC1_MTL_SHADERS_DIR}/mtl_shader_table.inc)
    set_source_files_properties(runtime/mtl/mtl_context.mm runtime/mtl/vk_on_metal.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(rac1_runtime PUBLIC ${RAC1_MTL_SHADERS_DIR})
    target_compile_definitions(rac1_runtime PUBLIC RAC1_HAVE_METAL=1)
    target_link_libraries(rac1_runtime PUBLIC "-framework Metal" "-framework QuartzCore" "-framework Cocoa")
endif()

# The movies' MPEG-2 pictures (runtime/ipu/movie_hw.cpp, the IPU's stand-in): FFmpeg's libavcodec
# when pkg-config finds it (optional). Without it the game runs and its movies are skipped (logged).
option(RAC1_MOVIES "Decode the game's movies with libavcodec when found" ON)
if(RAC1_MOVIES)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(RAC1_AVCODEC QUIET IMPORTED_TARGET libavcodec libavutil)
        # (a Homebrew FFmpeg when the system has no development files for it)
        if(NOT RAC1_AVCODEC_FOUND AND NOT CMAKE_CROSSCOMPILING AND EXISTS "/home/linuxbrew/.linuxbrew/lib/pkgconfig/libavcodec.pc")
            set(ENV{PKG_CONFIG_PATH} "/home/linuxbrew/.linuxbrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
            pkg_check_modules(RAC1_AVCODEC QUIET IMPORTED_TARGET libavcodec libavutil)
        endif()
    endif()
    if(RAC1_AVCODEC_FOUND)
        message(STATUS "Movies: libavcodec ${RAC1_AVCODEC_libavcodec_VERSION}")
        target_link_libraries(rac1_runtime PUBLIC PkgConfig::RAC1_AVCODEC)
        target_compile_definitions(rac1_runtime PRIVATE RAC1_HAVE_AVCODEC=1)
    elseif(APPLE)
        # (macOS without pkg-config: Homebrew's FFmpeg found by its headers and libraries)
        find_path(RAC1_AVCODEC_INCLUDE_DIR libavcodec/avcodec.h HINTS /opt/homebrew/include /usr/local/include)
        find_library(RAC1_AVCODEC_LIBRARY avcodec HINTS /opt/homebrew/lib /usr/local/lib)
        find_library(RAC1_AVUTIL_LIBRARY avutil HINTS /opt/homebrew/lib /usr/local/lib)
        if(RAC1_AVCODEC_INCLUDE_DIR AND RAC1_AVCODEC_LIBRARY AND RAC1_AVUTIL_LIBRARY)
            message(STATUS "Movies: libavcodec (${RAC1_AVCODEC_LIBRARY})")
            target_include_directories(rac1_runtime SYSTEM PRIVATE ${RAC1_AVCODEC_INCLUDE_DIR})
            target_link_libraries(rac1_runtime PUBLIC ${RAC1_AVCODEC_LIBRARY} ${RAC1_AVUTIL_LIBRARY})
            target_compile_definitions(rac1_runtime PRIVATE RAC1_HAVE_AVCODEC=1)
        else()
            message(STATUS "Movies: libavcodec not found (brew install ffmpeg): the game's movies are skipped")
        endif()
    else()
        message(STATUS "Movies: libavcodec not found (pkg-config): the game's movies are skipped")
    endif()
endif()

# Without the generated sources (they are made from the player's disc by ./setup.sh): everything that does
# not need them is compiled, the game's hand-written code included, and nothing is linked. For a CI build
# that has no disc (.github/workflows/build.yml); the game cannot run from such a build.
option(RAC1_NO_GENERATED "Compile without the generated sources (no disc): libraries only, no executable" OFF)

# ---- src/game/ : the game's code, ported (`// orig: 0xADDR`) ----
add_subdirectory(src/game)

# ---- src/_devtools/ : the debug menu (host code, behind --debug-tools) ----
option(RAC1_DEVTOOLS "Compile src/_devtools/ (the debug menu)" ON)
add_subdirectory(src/_devtools)

# ---- tests/oracle/ : the oracle, the ports proved against the original code (developers only: docs/oracle.md) ----
# Off by default: ./setup.sh never builds it. It needs the generated sources (the NTSC-U disc's) and, for most of its
# batches, RAM dumps recorded by the developer (assets/dumps/oracle/, never committed).
option(RAC1_ORACLE "Build the oracle (tests/oracle: build/tests/ovl_oracle, docs/oracle.md)" OFF)
if(RAC1_ORACLE)
    if(RAC1_NO_GENERATED)
        message(FATAL_ERROR "RAC1_ORACLE needs the generated sources (RAC1_NO_GENERATED is ON)")
    endif()
    add_subdirectory(tests/oracle)
endif()

# ---- tests/neon/ : the scalar-equivalence test for the VU fast paths' NEON twins (tests/neon/vu_neon_equiv.cpp) ----
# Off by default: ./setup.sh never builds it. It needs no generated sources and no disc: the one test that proves
# the NEON port of the VU SIMD helpers bit-identical to the scalar rules, on x86 (the SSE path) and on AArch64.
option(RAC1_NEON_TEST "Build the NEON equivalence test (tests/neon: build/vu_neon_equiv)" OFF)
if(RAC1_NEON_TEST)
    enable_testing()
    add_executable(vu_neon_equiv tests/neon/vu_neon_equiv.cpp)
    target_include_directories(vu_neon_equiv PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_compile_options(vu_neon_equiv PRIVATE -ffp-contract=off)
    endif()
    add_test(NAME vu_neon_equiv COMMAND vu_neon_equiv)
endif()

# ---- src/setup/ : setup & asset extraction ----
add_library(rac1_setup STATIC
    src/setup/kick_trace.cpp
    src/setup/world_trace.cpp
    src/setup/mrp_export.cpp
    src/setup/scene_export.cpp
    src/setup/iso_extract.cpp
    src/setup/setup_manager.cpp
)
target_include_directories(rac1_setup PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rac1_setup PUBLIC rac1_formats rac1_game rac1_runtime)

# ---- the game: one executable ----
set(RAC1_HOST_SOURCES
    src/host/orig_main.cpp
    src/host/audio_out.cpp
    src/host/sample_prof.cpp
    src/host/host_input.cpp
    src/host/host_language.cpp
    src/host/sim_script.cpp
    src/host/sim_autopilot.cpp
    src/host/update_check.cpp
    src/host/progress.cpp
)
if(WIN32)
    list(APPEND RAC1_HOST_SOURCES src/host/host_priority_win32.cpp src/host/gpu_preference_win32.cpp
         src/platform/win32/rac1-native.rc)
else()
    list(APPEND RAC1_HOST_SOURCES src/host/host_priority_posix.cpp)
endif()
if(RAC1_NO_GENERATED)
    # (no generated code: the host's files compiled, nothing linked)
    add_library(rac1-native OBJECT ${RAC1_HOST_SOURCES})
else()
    add_executable(rac1-native ${RAC1_HOST_SOURCES})
endif()
target_link_libraries(rac1-native PRIVATE rac1_runtime rac1_game rac1_setup miniaudio_headers)
foreach(t rac1_runtime rac1_game rac1_devtools rac1-native)  # (their sources include the shaders' header, made at build time)
    if(TARGET ${t})
        add_dependencies(${t} rac1_vk_shaders)
    endif()
endforeach()
# the installed version (the release's, and the git commit and branch of a clone), remade at each build:
# rac1_version.h, rewritten only when it changes (cmake/version.cmake). With it, the Python the build was configured
# with: the update check's helper (tools/update_check.py) is run with it when the environment names none (on Windows,
# the setup's own Python is not in PATH when the game is started by hand)
find_package(Git QUIET)
find_package(Python3 COMPONENTS Interpreter QUIET)
set(RAC1_VERSION_H "${CMAKE_BINARY_DIR}/version/rac1_version.h")
add_custom_target(rac1_version
    COMMAND "${CMAKE_COMMAND}" "-DVERSION=${PROJECT_VERSION}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DGIT=${GIT_EXECUTABLE}"
            "-DPYTHON=${Python3_EXECUTABLE}" "-DOUT=${RAC1_VERSION_H}" -P "${CMAKE_SOURCE_DIR}/cmake/version.cmake"
    BYPRODUCTS "${RAC1_VERSION_H}"
    VERBATIM)
add_dependencies(rac1-native rac1_version)
target_include_directories(rac1-native PRIVATE "${CMAKE_BINARY_DIR}/version")
if(RAC1_DEVTOOLS)
    target_link_libraries(rac1-native PRIVATE rac1_devtools)
    target_compile_definitions(rac1-native PRIVATE RAC1_DEVTOOLS=1)
endif()
if(WIN32 AND NOT RAC1_NO_GENERATED)
    # a main thread's stack of 64 MB reserved (Linux gives 8 MB or more, Windows 1 MB): the game's code
    # also runs on it, outside the coroutine
    target_link_options(rac1-native PRIVATE -Wl,--stack,67108864)
    # the C++ runtime linked in (no MinGW DLLs beside the program); with FFmpeg, whose static libraries need
    # more than pkg-config lists, everything stays a DLL, and the DLLs it uses are copied beside the program
    if(NOT RAC1_AVCODEC_FOUND)
        target_link_options(rac1-native PRIVATE -static)
    else()
        get_filename_component(RAC1_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET rac1-native POST_BUILD
            COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:rac1-native>" "-DSEARCH_DIR=${RAC1_COMPILER_BIN}"
                    "-DOBJDUMP=${CMAKE_OBJDUMP}" -P "${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
            VERBATIM)
    endif()
endif()

