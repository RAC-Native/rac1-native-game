// The drop shadows' counter on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp mrp_world_gx2_draw_shadows, 2026-10-30): the prisms of the
// game's shadow shapes (src/game/render/mrp_shadows_gpu.inc) drawn into a mask of their own against the world's depth - the faces toward
// the camera counted in its red channel, the others in its green (the blend adds), where they are not farther than the surface - and the
// mask's clears (a quad, the identity matrix in the block). The vertices: the world point (big-endian words, swapped by the fetch shader) and
// the value written in w; the matrix in a uniform block written with each 32-bit word swapped (GX2 reads the blocks as little-endian words).
#version 330 core
layout(location = 0) in vec4 a_pos;  // x, y, z (world, or clip space with the identity), the value
layout(std140) uniform ShadowFrame {
    vec4 uVP[4];  // the world renderers' view-projection (its columns), or the identity
};
out vec4 v_val;
void main() {
    v_val = vec4(a_pos.w);
    gl_Position = mat4(uVP[0], uVP[1], uVP[2], uVP[3]) * vec4(a_pos.xyz, 1.0);
}
