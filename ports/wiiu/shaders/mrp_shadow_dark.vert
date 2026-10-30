// The drop shadows' darkening on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp mrp_world_gx2_draw_shadows, 2026-10-30): a quad over the
// shadows' rectangle of the frame, the mask (mrp_shadow_vol) read around each pixel - its soft edge -, the frame multiplied by the blend
// (the colour times the shader's). The vertices: clip x, y and the mask's u, v; the taps' step, the darkening and the counter's scale.
#version 330 core
layout(location = 0) in vec4 a_pos;  // x, y (clip space), u, v (the mask)
layout(location = 1) in vec4 a_par;  // the taps' step in u and v, the darkening (0..1), 1 / the counter's step
out vec4 v_par;
out vec2 v_uv;
void main() {
    v_par = a_par;
    v_uv = a_pos.zw;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
