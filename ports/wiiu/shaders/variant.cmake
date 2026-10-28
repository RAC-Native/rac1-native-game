# A shader's build with defines (ports/wiiu/game/CMakeLists.txt's RAC1_WIIU_VARIANTS - 2026-10-28, the GPU's time): IN's GLSL with a
# "#define <d> 1" line for each of DEFS (comma-separated) after its #version line, written to OUT. Run by cmake -P.
file(READ ${IN} s)
string(REPLACE "," ";" defs "${DEFS}")
set(d "")
foreach(x ${defs})
    string(APPEND d "#define ${x} 1\n")
endforeach()
string(REGEX REPLACE "(#version[^\n]*\n)" "\\1${d}" s "${s}")
file(WRITE ${OUT} "${s}")
