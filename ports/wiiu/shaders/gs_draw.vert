// The GS replica's primitives on the Wii U (ports/wiiu/game/gs_draw_gx2.cpp): the vertices come already in clip space
// (the frame -> window mapping, Viewport::xscale and the rest, done on the CPU), so this stage only passes them on.
// Mirrors gs_draw.cpp's VS without its uniforms (uFb / uScaleX / uRt: render targets are not drawn yet).
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aTex;
layout(location = 3) in float aFog;
layout(location = 4) in vec4 aClamp;
out vec4 vColor;
out vec3 vTex;
out float vFog;
out vec4 vClamp;
void main() {
    vColor = aColor;
    vTex = aTex;
    vFog = aFog;
    vClamp = aClamp;
    gl_Position = vec4(aPos, 0.5, 1.0);
}
