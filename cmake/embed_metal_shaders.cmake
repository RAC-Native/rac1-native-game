# The Metal renderer's shaders (runtime/mtl/shaders/*.metal, made by tools/gen_metal_shaders.py) as C++ raw strings:
# OUT gets one {"name", R"rac1src(...)rac1src"}, line a shader, included by runtime/mtl/mtl_context.mm, which compiles
# them at start (newLibraryWithSource: no offline Metal compiler needed to build).
# cmake -DDIR=<shaders dir> -DOUT=<file.inc> -P embed_metal_shaders.cmake
file(GLOB _files "${DIR}/*.metal")
list(SORT _files)
set(_out "// Made by cmake/embed_metal_shaders.cmake from runtime/mtl/shaders/: do not edit.\n")
foreach(_f ${_files})
    get_filename_component(_name "${_f}" NAME_WE)
    file(READ "${_f}" _src)
    string(APPEND _out "{\"${_name}\", R\"rac1src(${_src})rac1src\"},\n")
endforeach()
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
if(NOT _old STREQUAL _out)
    file(WRITE "${OUT}" "${_out}")
endif()
