// The MRP world's pixels on the Wii U, the fast build (2026-10-28, the GPU's time: ports/wiiu/PERFORMANCE.md): mrp_world.frag's
// picture with its per-draw decoding moved into the vertex shader. mrp_world.frag decodes, per pixel, the draw's GS state (TME, IIP,
// the alpha test's mode, ATST, AREF), its texture word (the texture function, TCC, the mip level's L / K / LCM / MXL and the texture's
// levels) and chooses by branches - ~66 ALU groups a pixel, ~10 control-flow instructions more for the merged draws' unit tree. Here
// the vertex shader (mrp_world_fast.glsl's fast_terms, in mrp_world.vert / mrp_tfrag.vert / mrp_sky.vert built with RAC1_FAST) gives
// them as flat terms, and the pixel's work is straight arithmetic:
//  - the texel t' = t tm + ta (tm 1, ta 0 a texture; tm 0, ta 1 TME off - the colour alone -, ta 128 / 255 a texture word with no
//    texels, mrp_world.frag's (w & 1));
//  - the GS's texture function as o = t' (A + B c) + C c.a, per channel (A, B, C: vFnA's xyz for RGB, vFnB's for alpha);
//  - the GS's mip level: floor(clamp(2^L (-log2 |q|) + K + 0.5, 0, the last level + 0.5)) (2^L 0 with LCM) - clamped at 0 here, not
//    left to the sampler: a level below 0 might pick the magnification filter where mrp_world.frag's 0 picks the minification one;
//  - the alpha test as one comparison: y = a 255 - r, pass when y m + |y| n >= th (vAt: r, m, n, th by ATST; the strict tests' th the
//    next float above), on when the draw's test is (vSel.y), its pass's failing or passing pixels discarded (uA.x 1 / 2).
// Builds (ports/wiiu/game/CMakeLists.txt, each with its own vertex shader - a pair the CPU binds together): RAC1_UNITS the merged
// draws' units (uB.x: the triangle's unit, vSel.z, sampled from uTex / uT1..uT15 - the ties' / shrubs'), RAC1_NO_ALPHA_TEST without
// the alpha test (no discard: the batches with no alpha-tested draw), (2026-11-01) RAC1_NO_TEX without the texture's sample, for the draws
// whose tm is 0 (TME off: the sky's untextured shells) - t' = ta, the value t tm + ta has then; (2026-11-01) RAC1_KILL_ZERO the pixels
// whose alpha is 0 discarded (blended as (Cs - Cd) As + Cd or Cs As + Cd they leave Cd as it is: the sky's clouds' clear texels - the
// colour buffer's alpha there stays what it was instead of 0). Not here: the shade debug (uA.y) and the test's alpha test off
// (uA.w) - mrp_world.frag's: the CPU draws those with it.
#version 330 core
smooth in vec3 vStq;
noperspective in vec2 vQF;   // the GS's Q for the mip level, F / 255
noperspective in vec4 vColor;
flat in vec4 vFlat;
#ifdef RAC1_UNIFORM_TERMS
// ((2026-11-01, the GPU's pixels) the draw's terms from a block of their own (the sky's shells: one draw a batch, its terms the CPU's
// mrp_world_gx2.cpp fast_terms_cpu, the same values) instead of flat inputs: four inputs a pixel instead of nine)
layout(std140) uniform SkyTerms {
    vec4 vLod;
    vec4 vFnA;
    vec4 vFnB;
    vec4 vAt;
    vec4 vSel;
};
#else
flat in vec4 vLod;           // 2^L (0 with LCM), K + 0.5, the last level + 0.5, the layer
flat in vec4 vFnA;           // RGB's A, B, C, then tm
flat in vec4 vFnB;           // alpha's A, B, C, then ta
#ifndef RAC1_NO_ALPHA_TEST
flat in vec4 vAt;            // the alpha test's r, m, n, th
#endif
flat in vec4 vSel;           // 1 IIP (the colour interpolated), 1 the alpha test on, the texture unit, 0
#endif
#ifndef RAC1_NO_TEX
uniform sampler2DArray uTex;
#endif
#ifdef RAC1_UNITS
uniform sampler2DArray uT1, uT2, uT3, uT4, uT5, uT6, uT7, uT8, uT9, uT10, uT11, uT12, uT13, uT14, uT15;
#endif
layout(std140) uniform WorldPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255 (the batch's)
    ivec4 uA;      // x: the alpha test's pass (1, 2), y: the shade debug (0 here), z: FGE (the batch's), w: 0 here
    ivec4 uB;      // x: 1 the texture by the triangle's unit (uTex unit 0, uT1..uT15), 0 uTex
};
#ifdef RAC1_UNITS
// a texture unit's array sampled (u 0..15, a float)
vec4 unit_lod(float u, vec3 c, float l) {
    vec4 t;
    if (u < 7.5) {
        if (u < 3.5) {
            if (u < 1.5) {
                if (u < 0.5) t = textureLod(uTex, c, l);
                else t = textureLod(uT1, c, l);
            } else {
                if (u < 2.5) t = textureLod(uT2, c, l);
                else t = textureLod(uT3, c, l);
            }
        } else {
            if (u < 5.5) {
                if (u < 4.5) t = textureLod(uT4, c, l);
                else t = textureLod(uT5, c, l);
            } else {
                if (u < 6.5) t = textureLod(uT6, c, l);
                else t = textureLod(uT7, c, l);
            }
        }
    } else {
        if (u < 11.5) {
            if (u < 9.5) {
                if (u < 8.5) t = textureLod(uT8, c, l);
                else t = textureLod(uT9, c, l);
            } else {
                if (u < 10.5) t = textureLod(uT10, c, l);
                else t = textureLod(uT11, c, l);
            }
        } else {
            if (u < 13.5) {
                if (u < 12.5) t = textureLod(uT12, c, l);
                else t = textureLod(uT13, c, l);
            } else {
                if (u < 14.5) t = textureLod(uT14, c, l);
                else t = textureLod(uT15, c, l);
            }
        }
    }
    return t;
}
#endif
out vec4 FragColor;
void main() {
    vec4 c = vSel.x != 0.0 ? vColor : vFlat;
#ifdef RAC1_NO_TEX
    vec4 t = vec4(vFnB.w);
#else
    vec3 tc = vec3(vStq.xy / vStq.z, vLod.w);
    float level = floor(clamp(vLod.x * -log2(max(abs(vQF.x), 1e-20)) + vLod.y, 0.0, vLod.z));
    vec4 t;
#ifdef RAC1_UNITS
    if (uB.x != 0) t = unit_lod(vSel.z, tc, level);
    else t = textureLod(uTex, tc, level);
#else
    t = textureLod(uTex, tc, level);
#endif
    t = t * vFnA.w + vFnB.w;
#endif
    vec4 x = vec4(vFnA.xxx, vFnB.x) + vec4(vFnA.yyy, vFnB.y) * c;
    vec4 o = clamp(t * x + vec4(vFnA.zzz, vFnB.z) * c.a, 0.0, 1.0);
    float f = uA.z != 0 ? vQF.y : 1.0;
    o.rgb = o.rgb * f + uFogCol.rgb * (1.0 - f);
#ifndef RAC1_NO_ALPHA_TEST
    float y = o.a * 255.0 - vAt.x;
    bool pass = y * vAt.y + abs(y) * vAt.z >= vAt.w;
    if (vSel.y != 0.0 && pass == (uA.x == 2)) discard;
#endif
#ifdef RAC1_KILL_ZERO
    if (o.a == 0.0) discard;
#endif
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
