// The MRP world's ties and shrubs on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp): mrp_world_renderer.cpp's kFs brought to GLSL
// 330 for CafeGLSL: the GS's texture functions, the GS's mip level from Q (TEX1 LCM / L / K, MXL), FGE's fog and the alpha test with
// its AFAIL passes (uA.x: 1 the pixels that pass, 2 those that fail). What differs from the PC's: one texture array (the draw's,
// bound by the CPU per draw: no texture units, no subgroup loop), the GS's mip level only (the PC's "full texture" mode by
// textureQueryLod is not there), no modern lighting (sh_*), no shadow pass. With uB.x (the ties' / shrubs' merged draws,
// mrp_world_gx2.cpp) the triangle's texture is the one of its texture unit (draw_flags' bits 16..19, as the PC's units): the batch's
// units bound to uTex (unit 0: none, the white texture) and uT1..uT15, so that a draw may hold triangles of several textures.
// Its builds (ports/wiiu/game/CMakeLists.txt, RAC1_WIIU_VARIANTS; mrp_world_gx2.cpp picks one per batch - 2026-10-28, the GPU's time):
// RAC1_NO_ALPHA_TEST without the alpha test - no discard in the program, so the depth test and write happen before the shader
// (the batches none of whose draws tests: Batch::ate); RAC1_LEAN the texture level from the vertex shader's vLodP (lod_terms), the
// same numbers as the decoding here, a pixel's work smaller. The default build has neither.
#version 330 core
smooth in vec3 vStq;
noperspective in float vLodQ;
noperspective in vec4 vColor;
flat in vec4 vFlat;
noperspective in float vFog;
flat in uvec2 vTex;
#ifdef RAC1_LEAN
flat in vec4 vLodP;  // (mrp_world.vert's lod_terms: 2^L or 0 with LCM, K, the last level)
#endif
flat in uvec2 vState;  // the GS state as data: x = TME | IIP << 1 | FGE << 2 | the alpha test's mode << 3 | ATST << 4 | AREF << 8 | texture unit << 16, y = FOGCOL
uniform sampler2DArray uTex;
uniform sampler2DArray uT1, uT2, uT3, uT4, uT5, uT6, uT7, uT8, uT9, uT10, uT11, uT12, uT13, uT14, uT15;
layout(std140) uniform WorldPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255 (the batch's)
    ivec4 uA;      // x: the alpha test's pass (1, 2), y: the shade debug, z: FGE (the batch's), w: 1 (testing) the alpha test off
    ivec4 uB;      // x: 1 the texture by the triangle's unit (uTex unit 0, uT1..uT15), 0 uTex
};
// a texture unit's array sampled (u 0..15)
vec4 unit_lod(int u, vec3 c, float l) {
    vec4 t;
    if (u < 8) {
        if (u < 4) {
            if (u < 2) {
                if (u == 0) t = textureLod(uTex, c, l);
                else t = textureLod(uT1, c, l);
            } else {
                if (u == 2) t = textureLod(uT2, c, l);
                else t = textureLod(uT3, c, l);
            }
        } else {
            if (u < 6) {
                if (u == 4) t = textureLod(uT4, c, l);
                else t = textureLod(uT5, c, l);
            } else {
                if (u == 6) t = textureLod(uT6, c, l);
                else t = textureLod(uT7, c, l);
            }
        }
    } else {
        if (u < 12) {
            if (u < 10) {
                if (u == 8) t = textureLod(uT8, c, l);
                else t = textureLod(uT9, c, l);
            } else {
                if (u == 10) t = textureLod(uT10, c, l);
                else t = textureLod(uT11, c, l);
            }
        } else {
            if (u < 14) {
                if (u == 12) t = textureLod(uT12, c, l);
                else t = textureLod(uT13, c, l);
            } else {
                if (u == 14) t = textureLod(uT14, c, l);
                else t = textureLod(uT15, c, l);
            }
        }
    }
    return t;
}
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
#ifdef RAC1_LEAN
            // (lod_terms' x is 0 with LCM: the level K; the level rounded and clamped to 0..min(MXL, mips - 1) as below)
            float lod = log2(1.0 / max(abs(vLodQ), 1e-20)) * vLodP.x + vLodP.y;
            float level = clamp(floor(lod + 0.5), 0.0, vLodP.z);
#else
            int mips = int((w >> 22) & 7u) + 1;
            int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
            float K = float((int(w >> 10) << 20) >> 20) / 16.0;
            float lod = lcm != 0 ? K : log2(1.0 / max(abs(vLodQ), 1e-20)) * float(1 << L) + K;
            int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
            level = min(level, mips - 1);
#endif
            if (uB.x != 0) t = unit_lod(int((fl >> 16) & 15u), vec3(st, uLayer), float(level));
            else t = textureLod(uTex, vec3(st, uLayer), float(level));
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
#ifndef RAC1_NO_ALPHA_TEST
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
#endif
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
