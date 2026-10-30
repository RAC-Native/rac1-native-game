// The drop shadows' darkening on the Wii U (mrp_shadow_dark.vert): 13 taps of the mask (point-sampled: the counter's two channels -
// red the faces toward the camera that passed, green the others; a pixel "inside" when red is above green) - the centre twice, four at
// half the radius, eight at the radius -, their share the shadow's; the colour written is 1 - darkening x share (the blend multiplies
// the frame's colour by it, its alpha kept). (testing: a negative darkening - RAC1_WIIU_SHADOWS_GPU_DEBUG=4 - the mask's raw counts at
// the pixel, written without the blend: red the front faces / 8, green the others / 8, blue the net / 4)
#version 330 core
uniform sampler2D u_mask;
in vec4 v_par;
in vec2 v_uv;
out vec4 o_col;
float tap(vec2 o) {
    vec4 t = texture(u_mask, v_uv + o * v_par.xy);
    return clamp((t.r - t.g) * v_par.w, 0.0, 1.0);
}
void main() {
    if (v_par.z < 0.0) {
        vec4 t = texture(u_mask, v_uv);
        float k = 255.0 / 8.0;
        o_col = vec4(min(t.r * k, 1.0), min(t.g * k, 1.0), clamp((t.r - t.g) * 255.0 * 0.25, 0.0, 1.0), 1.0);
        return;
    }
    float m = 2.0 * tap(vec2(0.0, 0.0));
    m += tap(vec2(0.5, 0.0)) + tap(vec2(-0.5, 0.0)) + tap(vec2(0.0, 0.5)) + tap(vec2(0.0, -0.5));
    m += tap(vec2(1.0, 0.0)) + tap(vec2(-1.0, 0.0)) + tap(vec2(0.0, 1.0)) + tap(vec2(0.0, -1.0));
    m += tap(vec2(0.707, 0.707)) + tap(vec2(-0.707, 0.707)) + tap(vec2(0.707, -0.707)) + tap(vec2(-0.707, -0.707));
    o_col = vec4(vec3(1.0 - v_par.z * (m * (1.0 / 14.0))), 1.0);
}
