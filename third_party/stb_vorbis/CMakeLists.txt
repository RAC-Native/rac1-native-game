cmake_minimum_required(VERSION 3.14)
project(StbVorbis C)

# stb_vorbis (README.md): the Ogg Vorbis decoder the level composer uses (src/setup/level_music.cpp), from memory only
add_library(stb_vorbis STATIC stb_vorbis.c)
target_include_directories(stb_vorbis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(stb_vorbis PUBLIC STB_VORBIS_NO_PUSHDATA_API STB_VORBIS_NO_STDIO)
if(MSVC)
    target_compile_options(stb_vorbis PRIVATE /w)
else()
    target_compile_options(stb_vorbis PRIVATE -w)
endif()
