// The 3D's upscale on the Wii U (wiiu_blit.vert): the smaller colour buffer sampled bilinearly (the CPU's sampler), its alpha as it is
#version 330 core
uniform sampler2D u_tex;
in vec2 v_uv;
out vec4 o_col;
void main() {
    o_col = texture(u_tex, v_uv);
}
