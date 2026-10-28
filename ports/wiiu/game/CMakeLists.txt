# The game on the Wii U, without graphics (RAC1_NO_GRAPHICS): the same sources as the PC build (their lists read from
# the project's own CMakeLists), devkitPPC + wut, a .rpx. Configure with devkitPro's wrapper:
#
#   /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S ports/wiiu/game -B build-wiiu
#   cmake --build build-wiiu -j 3
#
# It needs a PC build first (RAC1_PC_BUILD, default build/): the generated sources (assets/generated/src, made by
# ./setup.sh from the player's disc), the dependencies CMake downloaded there, the Vulkan shaders' and the version's
# generated files. Nothing of the game is in the .rpx's sources: the generated code is compiled in, as on the PC.
cmake_minimum_required(VERSION 3.20)
project(rac1_wiiu C CXX ASM)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
get_filename_component(ROOT ${ROOT} ABSOLUTE)
set(RAC1_PC_BUILD ${ROOT}/build CACHE PATH "The PC build (its downloaded dependencies and generated files)")
set(RAC1_GENERATED_SRC ${ROOT}/assets/generated/src CACHE PATH "The generated sources (./setup.sh)")
set(DEPS ${RAC1_PC_BUILD}/_deps)

# A target's sources as a project CMakeLists lists them (add_library(<name> STATIC ...))
function(rac1_sources_of file target out)
    file(READ ${file} text)
    string(REGEX REPLACE "#[^\n]*" "" text "${text}")
    string(REGEX MATCH "add_library\\(${target} STATIC([^)]*)\\)" m "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" ";" list "${CMAKE_MATCH_1}")
    set(r "")
    get_filename_component(dir ${file} DIRECTORY)
    foreach(s IN LISTS list)
        if(s STREQUAL "" OR s MATCHES "^\\$")
            continue()
        endif()
        list(APPEND r ${dir}/${s})
    endforeach()
    set(${out} ${r} PARENT_SCOPE)
endfunction()

rac1_sources_of(${ROOT}/CMakeLists.txt rac1_runtime RUNTIME_SRC)
rac1_sources_of(${ROOT}/CMakeLists.txt rac1_formats FORMATS_SRC)
rac1_sources_of(${ROOT}/CMakeLists.txt rac1_setup SETUP_SRC)
rac1_sources_of(${ROOT}/src/game/CMakeLists.txt rac1_game GAME_SRC)
include(${RAC1_GENERATED_SRC}/generated_sources.cmake)  # RAC1_GENERATED_SOURCES
set(GEN_SRC "")
foreach(f IN LISTS RAC1_GENERATED_SOURCES)
    list(APPEND GEN_SRC ${RAC1_GENERATED_SRC}/${f})
endforeach()
# The VU1's translated programs (the world's tfrag / tie / shrub, the mobys'): VU1 draws, and without graphics its
# programs never run (they run from the frame's display chain, never from the game's update). Left out, they also keep
# the code under the 32 MB a PowerPC bl reaches (see the link options below); wiiu_stubs.cpp registers none.
list(FILTER GEN_SRC EXCLUDE REGEX "/world_vu1_(tfrag|tie|shrub)_[ab](_fast)?\\.cpp$|/moby_vu1(_fast)?\\.cpp$")
file(GLOB SND989_SRC ${ROOT}/third_party/989snd/game/sound/989snd/*.cpp ${ROOT}/third_party/989snd/game/sound/common/*.cpp)

set(HOST_SRC
    ${ROOT}/src/host/orig_main.cpp
    ${ROOT}/src/host/sample_prof.cpp
    ${ROOT}/src/host/host_input.cpp
    ${ROOT}/src/host/host_language.cpp
    ${ROOT}/src/host/sim_script.cpp
    ${ROOT}/src/host/sim_autopilot.cpp
    ${ROOT}/src/host/update_check.cpp
    ${ROOT}/src/host/progress.cpp
    ${ROOT}/src/profiler/host_profiler.cpp
    ${ROOT}/src/jobs/job_pool.cpp
    ${ROOT}/src/platform/coroutine_ppc.S
    ${ROOT}/third_party/glad/src/glad.c
    ${DEPS}/imgui-src/imgui.cpp
    ${DEPS}/imgui-src/imgui_draw.cpp
    ${DEPS}/imgui-src/imgui_tables.cpp
    ${DEPS}/imgui-src/imgui_widgets.cpp
    ${DEPS}/imgui-src/misc/cpp/imgui_stdlib.cpp
    ${RAC1_PC_BUILD}/generated_vk/vk_shaders.cpp
    platform_wiiu.cpp
    wiiu_stubs.cpp
    wiiu_main.cpp
    wiiu_sampler.cpp
    wiiu_input.cpp
    wiiu_audio.cpp
    wiiu_gfx.cpp
    gs_draw_gx2.cpp
    mrp_2d_gx2.cpp
    mrp_moby_gx2.cpp
    mrp_world_gx2.cpp
    debug_menu.cpp
)

add_executable(rac1_wiiu ${HOST_SRC} ${RUNTIME_SRC} ${FORMATS_SRC} ${SETUP_SRC} ${GAME_SRC} ${GEN_SRC} ${SND989_SRC})
list(FILTER RUNTIME_SRC EXCLUDE REGEX "^$")
target_include_directories(rac1_wiiu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${ROOT} ${ROOT}/src ${ROOT}/third_party/989snd ${ROOT}/third_party/glad/include
    ${RAC1_PC_BUILD}/generated_vk ${RAC1_PC_BUILD}/version
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${DEPS}/glfw-src/include ${DEPS}/glm-src ${DEPS}/imgui-src ${DEPS}/imgui-src/backends ${DEPS}/tinygltf-src
    ${DEPS}/miniaudio-src ${DEPS}/volk-src ${DEPS}/vulkan_headers-src/include)
target_compile_definitions(rac1_wiiu PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_OPENGL_LOADER_GLAD=1 IMGUI_IMPL_VULKAN_USE_VOLK=1
    GLFW_INCLUDE_NONE)
target_compile_features(rac1_wiiu PRIVATE cxx_std_20)
# the float rules of the PC build (docs/decisions.md 11): no a * b + c fused (GCC fuses fmadds on PowerPC by default)
target_compile_options(rac1_wiiu PRIVATE -O2 -frounding-math -ffp-contract=off -fno-strict-aliasing -w)
# elf2rpl takes no thread-local storage (its TPREL relocations): the few thread_local variables become ordinary ones.
# Right while the game's work stays on one thread, as without graphics (the renderers' workers, the users of most of
# them, never start); a build with workers needs them made per thread another way. One inside a function must be written
# `static thread_local` (the same in C++): a bare one would become a local of each call (989snd's Rand once did).
target_compile_definitions(rac1_wiiu PRIVATE thread_local=)
set_source_files_properties(${ROOT}/src/host/orig_main.cpp PROPERTIES COMPILE_DEFINITIONS "main=rac1_orig_main")
set_source_files_properties(${GEN_SRC} PROPERTIES COMPILE_OPTIONS "-g0")
target_link_libraries(rac1_wiiu PRIVATE stdc++fs)
target_link_options(rac1_wiiu PRIVATE -Wl,--wrap=abort -Wl,--wrap=__cxa_throw)  # (wiiu_main.cpp: the stack on the log first)
# The code must stay under 32 MB: a PowerPC bl reaches +-32 MB, and past it the linker puts a branch island right
# after the call in the .init section, which __do_global_ctors_aux then returns into and runs again (the static
# constructors forever). Unused functions dropped (-ffunction-sections is wut's default).
target_link_options(rac1_wiiu PRIVATE -Wl,--gc-sections)
# (experiment) sqrtf on integers, correctly rounded to nearest, in place of newlib's: exact_sqrt.cpp
option(RAC1_WIIU_EXACT_SQRT "sqrtf on integers (round to nearest only: an emulator experiment)" OFF)
if(RAC1_WIIU_EXACT_SQRT)
    target_sources(rac1_wiiu PRIVATE exact_sqrt.cpp)
    target_link_options(rac1_wiiu PRIVATE -Wl,--wrap=sqrtf)
endif()
wut_create_rpx(rac1_wiiu)

# The picture's shaders (ports/wiiu/shaders): GLSL compiled on the PC into .gsh files with CafeGLSL's compiler
# (-DRAC1_WIIU_GLSLC=<its bin/glslcompiler>), written to shaders/ in the build (copied beside the game's data on the SD;
# generated: never in the repository). Each <name>.vert / <name>.frag pair is one file; a pair whose pixel shader is another's names it
# in RAC1_WIIU_FRAG_<name> (mrp_tfrag, mrp_sky: mrp_world.frag).
set(RAC1_WIIU_GLSLC "" CACHE FILEPATH "CafeGLSL's glslcompiler, for ports/wiiu/shaders")
set(RAC1_WIIU_SHADERS test gs_draw mrp_2d mrp_moby mrp_world mrp_tfrag mrp_sky debug_text wiiu_blit)
set(RAC1_WIIU_FRAG_mrp_tfrag mrp_world)
set(RAC1_WIIU_FRAG_mrp_sky mrp_world)
if(RAC1_WIIU_GLSLC)
    set(_gsh)
    foreach(sh ${RAC1_WIIU_SHADERS})
        set(_src ${CMAKE_CURRENT_SOURCE_DIR}/../shaders)
        set(_frag ${sh})
        if(DEFINED RAC1_WIIU_FRAG_${sh})
            set(_frag ${RAC1_WIIU_FRAG_${sh}})
        endif()
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${sh}.gsh
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${RAC1_WIIU_GLSLC} -vs ${_src}/${sh}.vert -ps ${_src}/${_frag}.frag -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/${sh}.gsh
            DEPENDS ${_src}/${sh}.vert ${_src}/${_frag}.frag
            COMMENT "CafeGLSL: ${sh}.gsh")
        list(APPEND _gsh ${CMAKE_CURRENT_BINARY_DIR}/shaders/${sh}.gsh)
    endforeach()
    # the builds with defines (2026-10-28, the GPU's time: the backends pick one per batch, RAC1_WIIU_NOKILL / RAC1_WIIU_LEAN): each
    # RAC1_WIIU_VARIANT_<name> is its vertex shader, its pixel shader and the defines (comma-separated) - both shaders' sources with
    # them written into the build (ports/wiiu/shaders/variant.cmake: a vertex shader that does not read them compiles as its own build;
    # its "#pragma rac1_include" files put in), then compiled together as the others into shaders/<name>.gsh. (2026-10-28) The fast
    # world pixel shader's pairs (RAC1_FAST: mrp_world_fast.frag, the vertex shaders' fast_terms - their own outputs: bound as a pair,
    # mrp_world_gx2.cpp gx2_pair_variant; RAC1_UNITS the merged draws' unit tree, the ties' / shrubs')
    set(RAC1_WIIU_VARIANTS mrp_world_noat mrp_tfrag_noat mrp_moby_noat mrp_sky_noat
        mrp_world_fast mrp_world_fast_noat mrp_tfrag_fast mrp_tfrag_fast_noat mrp_sky_fast mrp_sky_fast_noat
        mrp_sky_host mrp_sky_host_noat mrp_sky_host_fast mrp_sky_host_fast_noat)  # (the lean ones - their vertex shaders output vLodP - left out: the world came out untextured on the console, 2026-10-28; lean_vert.patch in rac1-scratch/gpuperf)
    set(RAC1_WIIU_VARIANT_mrp_world_noat mrp_world mrp_world RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_world_lean mrp_world mrp_world RAC1_LEAN)
    set(RAC1_WIIU_VARIANT_mrp_world_lean_noat mrp_world mrp_world RAC1_LEAN,RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_tfrag_noat mrp_tfrag mrp_world RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_tfrag_lean mrp_tfrag mrp_world RAC1_LEAN)
    set(RAC1_WIIU_VARIANT_mrp_tfrag_lean_noat mrp_tfrag mrp_world RAC1_LEAN,RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_moby_noat mrp_moby mrp_moby RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_sky_noat mrp_sky mrp_world RAC1_NO_ALPHA_TEST)  # (2026-10-28: the sky's batches without an alpha test)
    set(RAC1_WIIU_VARIANT_mrp_world_fast mrp_world mrp_world_fast RAC1_FAST,RAC1_UNITS)
    set(RAC1_WIIU_VARIANT_mrp_world_fast_noat mrp_world mrp_world_fast RAC1_FAST,RAC1_UNITS,RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_tfrag_fast mrp_tfrag mrp_world_fast RAC1_FAST)
    set(RAC1_WIIU_VARIANT_mrp_tfrag_fast_noat mrp_tfrag mrp_world_fast RAC1_FAST,RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_sky_fast mrp_sky mrp_world_fast RAC1_FAST)
    set(RAC1_WIIU_VARIANT_mrp_sky_fast_noat mrp_sky mrp_world_fast RAC1_FAST,RAC1_NO_ALPHA_TEST)
    # (2026-10-28: the host sky's shells - mrp_sky.vert's STQ x the clip w in the shader, mrp_sky_host.inc; each its own pair)
    set(RAC1_WIIU_VARIANT_mrp_sky_host mrp_sky mrp_world RAC1_SKY_HOST)
    set(RAC1_WIIU_VARIANT_mrp_sky_host_noat mrp_sky mrp_world RAC1_SKY_HOST,RAC1_NO_ALPHA_TEST)
    set(RAC1_WIIU_VARIANT_mrp_sky_host_fast mrp_sky mrp_world_fast RAC1_FAST,RAC1_SKY_HOST)
    set(RAC1_WIIU_VARIANT_mrp_sky_host_fast_noat mrp_sky mrp_world_fast RAC1_FAST,RAC1_SKY_HOST,RAC1_NO_ALPHA_TEST)
    foreach(v ${RAC1_WIIU_VARIANTS})
        set(_src ${CMAKE_CURRENT_SOURCE_DIR}/../shaders)
        list(GET RAC1_WIIU_VARIANT_${v} 0 _vert)
        list(GET RAC1_WIIU_VARIANT_${v} 1 _frag)
        list(GET RAC1_WIIU_VARIANT_${v} 2 _defs)
        set(_gen ${CMAKE_CURRENT_BINARY_DIR}/shader_src/${v}.frag)
        set(_genv ${CMAKE_CURRENT_BINARY_DIR}/shader_src/${v}.vert)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${v}.gsh
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shader_src
            COMMAND ${CMAKE_COMMAND} -DIN=${_src}/${_frag}.frag -DOUT=${_gen} -DDEFS=${_defs} -P ${_src}/variant.cmake
            COMMAND ${CMAKE_COMMAND} -DIN=${_src}/${_vert}.vert -DOUT=${_genv} -DDEFS=${_defs} -P ${_src}/variant.cmake
            COMMAND ${RAC1_WIIU_GLSLC} -vs ${_genv} -ps ${_gen} -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/${v}.gsh
            DEPENDS ${_src}/${_vert}.vert ${_src}/${_frag}.frag ${_src}/variant.cmake ${_src}/mrp_world_fast.glsl
            COMMENT "CafeGLSL: ${v}.gsh (${_defs})")
        list(APPEND _gsh ${CMAKE_CURRENT_BINARY_DIR}/shaders/${v}.gsh)
    endforeach()
    add_custom_target(rac1_wiiu_shaders ALL DEPENDS ${_gsh})
else()
    message(STATUS "RAC1_WIIU_GLSLC not set: the picture's shaders are not built")
endif()

# The GamePad debug menu's font atlas (debug_menu.cpp): made at build time from a monospaced font on this machine
# (ports/wiiu/tools/font_atlas.py; an empty atlas when there is no font or no PIL), into the build folder - never committed
# (the system's python3, whose PIL renders the font: not necessarily the interpreter CMake's FindPython picks)
find_program(RAC1_FONT_PYTHON python3 PATHS /usr/bin /usr/local/bin NO_DEFAULT_PATH)
if(NOT RAC1_FONT_PYTHON)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(RAC1_FONT_PYTHON ${Python3_EXECUTABLE})
endif()
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/font_atlas.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND ${RAC1_FONT_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/font_atlas.py ${CMAKE_CURRENT_BINARY_DIR}/generated/font_atlas.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/font_atlas.py
    COMMENT "the debug menu's font atlas")
add_custom_target(rac1_wiiu_font DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/font_atlas.h)
add_dependencies(rac1_wiiu rac1_wiiu_font)
