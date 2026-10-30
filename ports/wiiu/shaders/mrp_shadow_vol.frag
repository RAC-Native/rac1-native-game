// The drop shadows' counter on the Wii U (mrp_shadow_vol.vert): the vertex's value, added by the blend into the channel the draw writes
#version 330 core
in vec4 v_val;
out vec4 o_col;
void main() {
    o_col = v_val;
}
