# The PS2 float rules and the coroutine on the Wii U (built with devkitPPC's powerpc-eabi-cmake); the PC side is
# main_pc.cpp, built by hand: see ../README.md.
cmake_minimum_required(VERSION 3.13)
project(rac1_wiiu_fputest CXX ASM)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_executable(rac1_fputest main_wiiu.cpp ${ROOT}/src/platform/coroutine_ppc.S)
target_include_directories(rac1_fputest PRIVATE ${ROOT})
target_compile_features(rac1_fputest PRIVATE cxx_std_20)
target_compile_options(rac1_fputest PRIVATE -Wall -O2 -frounding-math -ffp-contract=off)
wut_create_rpx(rac1_fputest)
