// The GamePad's debug menu (ports/wiiu/game/debug_menu.cpp): text quads, their corners in the screen's clip space
#version 330 core
layout(location = 0) in vec4 a_pos;  // x, y (clip space), u, v (the font atlas)
layout(location = 1) in vec4 a_col;
out vec2 v_uv;
out vec4 v_col;
void main() {
    v_uv = a_pos.zw;
    v_col = a_col;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
