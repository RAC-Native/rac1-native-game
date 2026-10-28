// The MRP sky on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp, MRP_LITE.md phase 5): mrp_world_renderer.cpp's kVs - the program the PC's
// sky batches (Batch::sky, the CPU path's vertices) draw with - brought to GLSL 330 for CafeGLSL, its world projection (uMode 0) only, as
// mrp_tfrag.vert: the vertex's world point - for the sky a direction, w 0 (sky_dir: the point at infinity projection A sees at the GS's
// X, Y) - through the world renderers' view-projection, the STQ already multiplied by the clip w on the CPU (sky_gpuv). The vertices
// are mrpwb::GpuV (72 bytes) made by build_triangles and written by the PPC: big-endian 32-bit words the fetch shader swaps
// (GX2_ENDIAN_SWAP_8_IN_32), the RGBA bytes in memory order. What differs from mrp_tfrag.vert: the depth at the far plane (z = w (1 - e):
// the PC's depth clamp for the sky - a direction's own z is past the far plane, and clipped there), the block's name. The CPU draws it
// with the depth test and writes off: the depth is never read. The pixel side is mrp_world.frag's (the same outputs).
// (2026-10-28) Built RAC1_SKY_HOST (shaders/mrp_sky_host*.gsh, its own pairs): the host sky's shells (mrp_sky_host.inc) - static corners,
// the shell's point (w 0) through the view-projection x the shell's matrix (the block's), the STQ multiplied by the clip w here (the
// GS's affine S / Q, as the CPU does for the capture's; negative behind the camera: the triangle clipped with the attribute homogeneous).
#version 330 core
layout(location = 0) in vec4 aWorld;
layout(location = 2) in vec3 aStq;
layout(location = 3) in float aLodQ;
layout(location = 4) in uvec4 aRgba;
layout(location = 5) in float aFog;
layout(location = 6) in uvec2 aTex;    // the texture word, the array layer
layout(location = 7) in uvec2 aState;  // the GS state: draw_flags, FOGCOL
layout(std140) uniform SkyFrame {
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
    vec4 p = mat4(uVP[0], uVP[1], uVP[2], uVP[3]) * aWorld;
    p.z = p.w * (1.0 - 1.0 / 1024.0);
    gl_Position = p;
#ifdef RAC1_SKY_HOST
    vStq = aStq * p.w;
#else
    vStq = aStq;
#endif
#ifndef RAC1_FAST
    vLodQ = aLodQ;
#endif
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
#ifndef RAC1_FAST
    vFog = aFog;
#endif
}
