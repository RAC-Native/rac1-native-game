// The MRP's 2D on the Wii U (ports/wiiu/game/mrp_2d_gx2.cpp): mrp_2d.cpp's FS in a uniform block - the GS's texture functions (colours
// as bytes / 255, 0x80 = 1.0), the per-pixel mip level, the alpha test with its fail modes, fogging, the frame sampled (a copy of
// the screen around the primitive) as its texture and for the destination alpha test. No dual-source blend: a render target's run
// that blends with the source alpha is drawn twice (mrp_2d_gx2.cpp), the colour with the blend's alpha (uC.x 0) and then the alpha
// alone with the GS's raw alpha (uC.x 1), as gs_draw_gx2's.
#version 330 core
in vec4 vColor;
in vec3 vTex;
in float vFog;
// a glyph's sampling kept inside its own cell (Viewport::sprite_clamp): s, t clamped to xy..zw (x > z: none); the same on the
// primitive's vertices (mrp_2d.cpp's is flat)
in vec4 vClamp;
in vec2 vWin;
uniform sampler2D uTexture;
// the screen's copy (rows from the colour buffer's top), the mip levels after the first
uniform sampler2D uScreen;
uniform sampler2D uMip1;
uniform sampler2D uMip2;
uniform sampler2D uMip3;
layout(std140) uniform Mrp2dPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255
    ivec4 uA;      // TME, TFX, TCC, ATE
    ivec4 uB;      // ATST, AREF, AFAIL, FGE
    ivec4 uC;      // x: the alpha written the GS's raw one (a render target's); y: the AFAIL split's pass (1 the Z); z: the frame sampled; w: DATE
    ivec4 uD;      // DATM, mip levels after the first (uMip1..3), the first level's number, TEX1 LCM
    ivec4 uE;      // TEX1 MXL, the window's height (the screen's copy: GL's rows from the bottom -> its rows), 0, 0
    vec4 uMipKL;   // TEX1 K, 2^L
    vec4 uScrMap;  // the window position the texture coordinates name: x = uScrMap.x + uScrMap.y s, y from the bottom = uScrMap.z + uScrMap.w t
};
out vec4 FragColor;
void main() {
    if (uC.w != 0) {  // DATE: the destination's alpha bit 7 (the screen's alpha is the GS's x 255 / 128)
        bool b7 = texelFetch(uScreen, ivec2(vWin), 0).a > 254.0 / 255.0;
        if ((uD.x == 0) == b7) discard;
    }
    vec4 c = vColor;
    vec4 o = c;
    if (uA.x != 0) {
        vec2 st = vTex.xy / vTex.z;
        vec4 t;
        if (uC.z != 0) {
            int sx = int(uScrMap.x + uScrMap.y * st.x), sy = int(uScrMap.z + uScrMap.w * st.y);
            t = texelFetch(uScreen, ivec2(sx, uE.y - 1 - sy), 0);
            t.a = t.a * (128.0 / 255.0);
        } else {
            int idx = 0;
            if (uD.y > 0) {
                float lod = uD.w != 0 ? uMipKL.x : log2(1.0 / max(abs(vTex.z), 1e-20)) * uMipKL.y + uMipKL.x;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), uE.x);
                idx = clamp(level - uD.z, 0, uD.y);
            }
            if (vClamp.x <= vClamp.z) st = clamp(st, vClamp.xy, vClamp.zw);
            if (idx == 0) t = texture(uTexture, st);
            else if (idx == 1) t = texture(uMip1, st);
            else if (idx == 2) t = texture(uMip2, st);
            else t = texture(uMip3, st);
        }
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
        if (!pass && uC.y == 0 && (uB.z == 0 || uB.z == 2)) discard;
        if (!pass && uC.y == 1 && uB.z != 2) discard;
    }
    FragColor = uC.x != 0 ? o : vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
