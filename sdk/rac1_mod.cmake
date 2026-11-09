# rac1-native's mod SDK: a mod's plugin built with the game's rules (docs/custom-levels.md, "Code").
#
# A mod's plugin/CMakeLists.txt:
#
#     cmake_minimum_required(VERSION 3.16)
#     project(my_mod C)
#     include(${RAC1_SDK}/rac1_mod.cmake)
#     rac1_mod_plugin(my_mod my_mod.c)
#
# The setup (rac1-native --setup --level-mods, or --mod-plugins) configures it with RAC1_SDK (this folder) and
# RAC1_PLUGIN_OUT (where the library goes: assets/generated/mods/<folder>/plugin/), with the compilers the game was built
# with. The library is `rac1_mod` + the platform's suffix (.so, .dll, .dylib), whatever the target's name.

if(NOT RAC1_SDK)
    get_filename_component(RAC1_SDK "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
endif()

function(rac1_mod_plugin target)
    add_library(${target} MODULE ${ARGN})
    target_include_directories(${target} PRIVATE "${RAC1_SDK}")
    # the game's float rules: its code runs with the FPU rounding toward zero (as the PS2's), and a * b + c stays two
    # roundings (no FMA contraction), no fast math; no type-based alias analysis (the game's memory is read through several
    # types)
    target_compile_options(${target} PRIVATE -frounding-math -ffp-contract=off -fno-strict-aliasing -fvisibility=hidden)
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "rac1_mod" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden
                                               POSITION_INDEPENDENT_CODE ON)
    if(RAC1_PLUGIN_OUT)
        foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
            set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY${cfg} "${RAC1_PLUGIN_OUT}"
                                                       RUNTIME_OUTPUT_DIRECTORY${cfg} "${RAC1_PLUGIN_OUT}")
        endforeach()
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE m)
    endif()
    if(MINGW)
        target_link_options(${target} PRIVATE -static-libgcc)
    endif()
endfunction()
