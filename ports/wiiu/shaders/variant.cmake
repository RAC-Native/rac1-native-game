# A shader's build with defines (ports/wiiu/game/CMakeLists.txt's RAC1_WIIU_VARIANTS - 2026-10-28, the GPU's time): IN's GLSL with a
# "#define <d> 1" line for each of DEFS (comma-separated) after its #version line, written to OUT. Run by cmake -P. (2026-10-28, the fast
# world shader) A "#pragma rac1_include <file>" line is replaced by that file's text (beside IN), whatever the defines: the glslcompiler
# has no #include - the builds without the define skip the pragma itself (inside its #ifdef).
file(READ ${IN} s)
string(REPLACE "," ";" defs "${DEFS}")
set(d "")
foreach(x ${defs})
    string(APPEND d "#define ${x} 1\n")
endforeach()
string(REGEX REPLACE "(#version[^\n]*\n)" "\\1${d}" s "${s}")
get_filename_component(dir ${IN} DIRECTORY)
string(REGEX MATCHALL "#pragma rac1_include [^\n]*" incs "${s}")
foreach(line ${incs})
    string(REGEX REPLACE "#pragma rac1_include[ ]+" "" name "${line}")
    string(STRIP "${name}" name)
    file(READ ${dir}/${name} inc)
    string(REPLACE "${line}" "${inc}" s "${s}")
endforeach()
file(WRITE ${OUT} "${s}")
