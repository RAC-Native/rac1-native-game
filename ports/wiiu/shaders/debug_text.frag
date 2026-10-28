// The GamePad's debug menu (ports/wiiu/game/debug_menu.cpp): the font atlas' coverage times the quad's colour (u < 0: a plain box)
#version 330 core
uniform sampler2D u_font;
in vec2 v_uv;
in vec4 v_col;
out vec4 o_col;
void main() {
    float a = v_uv.x < 0.0 ? 1.0 : texture(u_font, v_uv).r;
    o_col = vec4(v_col.rgb, v_col.a * a);
}
