// The 3D's upscale on the Wii U (ports/wiiu/game/wiiu_gfx.cpp, RAC1_WIIU_3D_SCALE - 2026-10-28): the 3D drawn into a smaller colour buffer,
// its frame's rectangle drawn over the TV's frame by a quad (a triangle strip of four corners, written by the CPU per frame)
#version 330 core
layout(location = 0) in vec4 a_pos;  // x, y (clip space), u, v (the smaller buffer's texture)
out vec2 v_uv;
void main() {
    v_uv = a_pos.zw;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
