// The MRP's 2D on the Wii U (ports/wiiu/game/mrp_2d_gx2.cpp): mrp_2d.cpp's VS with its mapping done on the CPU - the vertices come in
// clip space (the frame -> window mapping with Viewport::xscale, a render target's GS pixels, both with the GS's row 0 at GX2's
// top), so this stage passes them on. aWin: the vertex's window pixel position (x, y from the colour buffer's top left), which the
// pixel shader reads the screen's copy at (DATE: gl_FragCoord's in mrp_2d.cpp).
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aTex;
layout(location = 3) in float aFog;
layout(location = 4) in vec4 aClamp;
layout(location = 5) in vec2 aWin;
out vec4 vColor;
out vec3 vTex;
out float vFog;
out vec4 vClamp;
out vec2 vWin;
void main() {
    vColor = aColor;
    vTex = aTex;
    vFog = aFog;
    vClamp = aClamp;
    vWin = aWin;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
