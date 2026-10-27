// The MRP world's ties and shrubs on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp): mrp_world_renderer.cpp's kFs brought to GLSL
// 330 for CafeGLSL: the GS's texture functions, the GS's mip level from Q (TEX1 LCM / L / K, MXL), FGE's fog and the alpha test with
// its AFAIL passes (uA.x: 1 the pixels that pass, 2 those that fail). What differs from the PC's: one texture array (the draw's,
// bound by the CPU per draw: no texture units, no subgroup loop), the GS's mip level only (the PC's "full texture" mode by
// textureQueryLod is not there), no modern lighting (sh_*), no shadow pass.
#version 330 core
smooth in vec3 vStq;
noperspective in float vLodQ;
noperspective in vec4 vColor;
flat in vec4 vFlat;
noperspective in float vFog;
flat in uvec2 vTex;
flat in uvec2 vState;  // the GS state as data: x = TME | IIP << 1 | FGE << 2 | the alpha test's mode << 3 | ATST << 4 | AREF << 8 | texture unit << 16, y = FOGCOL
uniform sampler2DArray uTex;
layout(std140) uniform WorldPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255 (the batch's)
    ivec4 uA;      // x: the alpha test's pass (1, 2), y: the shade debug, z: FGE (the batch's), w: 1 (testing) the alpha test off
};
out vec4 FragColor;
void main() {
    uint fl = vState.x;
    int uTme = int(fl & 1u), uIip = int((fl >> 1) & 1u), uAte = int((fl >> 3) & 1u), uAtst = int((fl >> 4) & 7u), uAref = int((fl >> 8) & 0xffu);
    int uShade = uA.y;
    vec4 c = uIip != 0 ? vColor : vFlat;
    vec4 o = c;
    uint w = vTex.x;
    float uLayer = float(vTex.y);
    if (uTme != 0) {
        vec2 st = vStq.xy / vStq.z;
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
            float K = float((int(w >> 10) << 20) >> 20) / 16.0;
            float lod = lcm != 0 ? K : log2(1.0 / max(abs(vLodQ), 1e-20)) * float(1 << L) + K;
            int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
            level = min(level, mips - 1);
            t = textureLod(uTex, vec3(st, uLayer), float(level));
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uA.z != 0) {
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol.rgb * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0 && uA.w == 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (uA.x == 1 && !pass) discard;  // (uAte: the draw's mode - the test on and its failing pixels treated apart)
        if (uA.x == 2 && pass) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
