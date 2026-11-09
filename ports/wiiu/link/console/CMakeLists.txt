# rac1-link, the Wii U side (prototype): a small homebrew, no game code or data. devkitPPC + wut:
#
#   /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S ports/wiiu/link/console -B <build dir>
#   cmake --build <build dir> -j 3
#
# Gives rac1-link.rpx and, with wut-tools' wuhbtool, rac1-link.wuhb (no icon yet: the Wii U Menu shows a default).
cmake_minimum_required(VERSION 3.13)
project(rac1_link C)

set(CMAKE_C_STANDARD 11)
add_executable(rac1-link main.c ../core/link_server.c ../core/link_hash.c)
target_compile_options(rac1-link PRIVATE -Wall -O2 -ffunction-sections -fdata-sections)
# (Aroma's RPX loader, to start the game: wums' librpxloader)
target_include_directories(rac1-link PRIVATE ${DEVKITPRO}/wums/include)
target_link_directories(rac1-link PRIVATE ${DEVKITPRO}/wums/lib)
target_link_libraries(rac1-link PRIVATE rpxloader)
wut_create_rpx(rac1-link)
if(WUT_WUHBTOOL_EXE)
    wut_create_wuhb(rac1-link NAME "rac1-link" SHORTNAME "rac1-link" AUTHOR "rac1-native")
endif()
