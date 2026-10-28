// The MRP world's tfrags on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp): mrp_world_renderer.cpp's kVs - the program the PC's tfrag
// batches draw with, over the compute program's vertices - brought to GLSL 330 for CafeGLSL, its world projection (uMode 0) only: the
// vertex's world point through the world renderers' view-projection. The vertices are the compute program's (mrpwb::GpuV, 72 bytes),
// made here by the CPU (mrp_world_renderer.cpp tfrag_frame_cpu) and written by the PPC: big-endian 32-bit words, which the fetch shader
// swaps (GX2_ENDIAN_SWAP_8_IN_32), the RGBA bytes in memory order. What differs from the PC's: the matrix in a uniform block (written
// with each 32-bit word swapped: GX2 reads the blocks as little-endian words), no GS-frame mode (the game's VU1 projection: aVu is not
// read), no modern lighting (sh_clip, vShWorld). The pixel side is mrp_world.frag's (the same outputs: one texture array, bound by the
// CPU per run of one texture unit).
#version 330 core
layout(location = 0) in vec4 aWorld;
layout(location = 2) in vec3 aStq;
layout(location = 3) in float aLodQ;
layout(location = 4) in uvec4 aRgba;
layout(location = 5) in float aFog;
layout(location = 6) in uvec2 aTex;    // the texture word, the array layer
layout(location = 7) in uvec2 aState;  // the GS state: draw_flags, FOGCOL
layout(std140) uniform TfragFrame {
    vec4 uVP[4];  // the world renderers' view-projection (mrp_world_view_proj), its columns
};
smooth out vec3 vStq;
#ifdef RAC1_FAST
// (2026-10-28) the fast pixel shader's terms (mrp_world_fast.frag): its outputs and fast_terms()
#pragma rac1_include mrp_world_fast.glsl
noperspective out vec4 vColor;
flat out vec4 vFlat;
#else
noperspective out float vLodQ;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFog;
flat out uvec2 vTex;
flat out uvec2 vState;
#endif
void main() {
#ifdef RAC1_FAST
    fast_terms(aTex.x, aTex.y, aState.x, aLodQ, aFog);
#else
    vTex = aTex;
    vState = aState;
#endif
    gl_Position = mat4(uVP[0], uVP[1], uVP[2], uVP[3]) * aWorld;
    vStq = aStq;
#ifndef RAC1_FAST
    vLodQ = aLodQ;
#endif
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
#ifndef RAC1_FAST
    vFog = aFog;
#endif
}
