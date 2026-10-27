// The GS's texture functions, fog and alpha test (ports/wiiu/game/gs_draw_gx2.cpp): mirrors gs_draw.cpp's FS for the
// screen's 2D and the render targets (colours as bytes / 255, the GS's alpha 0x80 = 1.0). Not here (yet): DATE, the frame
// sampled as a texture, the per-pixel mip level, the AFAIL split's Z pass. No dual-source blend: a render target's batch that
// blends with the source alpha is drawn twice instead (gs_draw_gx2.cpp), the colour with the blend's alpha (uC.x 0) and then
// the alpha alone with the GS's raw alpha (uC.x 1).
#version 330 core
in vec4 vColor;
in vec3 vTex;
in float vFog;
// a glyph's sampling kept inside its own cell (Viewport::sprite_clamp): s, t clamped to xy..zw (x > z: none); the same
// on the primitive's vertices (gs_draw.cpp's is flat)
in vec4 vClamp;
uniform sampler2D uTexture;
layout(std140) uniform GsDraw {
    vec4 uFogCol;  // rgb: FOGCOL / 255
    ivec4 uA;      // TME, TFX, TCC, ATE
    ivec4 uB;      // ATST, AREF, AFAIL, FGE
    ivec4 uC;      // x: the alpha written the GS's raw one (0x80 = 128 / 255: a render target's) instead of 0x80 = 1.0
};
out vec4 FragColor;
void main() {
    vec4 c = vColor;
    vec4 o = c;
    if (uA.x != 0) {
        vec2 st = vTex.xy / vTex.z;
        if (vClamp.x <= vClamp.z) st = clamp(st, vClamp.xy, vClamp.zw);
        vec4 t = texture(uTexture, st);
        int tfx = uA.y;
        bool tcc = uA.z != 0;
        if (tfx == 0) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1) { o.rgb = t.rgb; o.a = tcc ? t.a : c.a; }
        else if (tfx == 2) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc ? t.a : c.a; }
    }
    o = clamp(o, 0.0, 1.0);
    if (uB.w != 0) {  // Cv F + FOGCOL (1 - F), F the byte / 255
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol.rgb * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uA.w != 0) {
        int atst = uB.x;
        float aref = float(uB.y);
        bool pass = true;
        if (atst == 0) pass = false;
        else if (atst == 2) pass = a < aref;
        else if (atst == 3) pass = a <= aref;
        else if (atst == 4) pass = abs(a - aref) < 0.5;
        else if (atst == 5) pass = a >= aref;
        else if (atst == 6) pass = a > aref;
        else if (atst == 7) pass = abs(a - aref) >= 0.5;
        // a failed pixel: AFAIL 0 KEEP / 2 ZB_ONLY write no colour; 1 FB_ONLY / 3 RGB_ONLY do (no depth here: no Z pass)
        if (!pass && (uB.z == 0 || uB.z == 2)) discard;
    }
    // (gs_draw.cpp: a render target keeps the GS's raw alpha, as the textures read from its memory; its blend reads the other)
    FragColor = uC.x != 0 ? o : vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
