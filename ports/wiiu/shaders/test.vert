// The Wii U's first picture (ports/wiiu/game/wiiu_gfx.cpp): a triangle, its corners' colours set by the host each frame
#version 330 core
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec4 a_col;
out vec4 v_col;
void main() {
    v_col = a_col;
    gl_Position = a_pos;
}
