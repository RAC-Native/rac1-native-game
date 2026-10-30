// The MRP's mobys' pixels on the Wii U, the fast build (2026-11-01, the GPU's pixels: ports/wiiu/PERFORMANCE.md): mrp_moby.frag's
// picture with the draw's decoding moved into the vertex shader (mrp_moby.vert built RAC1_FAST: flat terms made once per vertex from the
// draw's texture word and its batch's flags, MobyDraw's pCmd.w), as mrp_world_fast.frag does for the world. mrp_moby.frag decodes the
// texture word per pixel and chooses by branches (55-73 ALU groups, 28-33 control-flow instructions, a textureSize per pixel); here:
//  - the texel t' = t tm + ta (tm 1, ta 0 a texture; tm 0, ta 128 / 255 a texture word with no texels; tm 0, ta 1 TME off);
//  - the texture function o = (t' (A + B c)) S + C c.a per channel (A, B, C: vFnA's xyz for RGB, vFnB's for alpha; S vScl's x / y):
//    mrp_moby.frag's products in its order (t c, then x 255 / 128), the same values;
//  - the level as mrp_moby.frag's: mip mode 0 (uB.y) the GS's, floor(clamp(A log2(1 / |q|) + K + 0.5, 0, min(MXL, the levels - 1) + 0.5))
//    (A 2^L, 0 with LCM), mode 1 every mip by the derivatives - the texture's size from the batch's block (uC.y / z, float bits: the
//    bound array's) instead of textureSize;
//  - the alpha test as one comparison (mrp_world_fast.frag's: y = a 255 - AREF, pass when y m + |y| n >= th; vAt by ATST), its pixels
//    discarded when they fail and the pass's fail mode says so (uC.w: worked out by the CPU from uC.x and AFAIL as mrp_moby.frag does).
// Builds (ports/wiiu/game/CMakeLists.txt): mrp_moby_fast, mrp_moby_fast_noat (RAC1_NO_ALPHA_TEST: no discard), and the same two built
// RAC1_UNIFORM_TERMS (mrp_moby_fast_ut, mrp_moby_fast_noat_ut: the terms from a block, RAC1_WIIU_MOBY_UTERMS). Not here: the shade
// debug (uB.z) - mrp_moby.frag's: the CPU draws those with it.
#version 330 core
noperspective in vec4 vColor;
noperspective in vec3 vStq;
noperspective in float vFog;
#ifdef RAC1_UNIFORM_TERMS
// ((2026-11-01, the GPU's pixels) the draw's terms from a block of their own (mrp_moby_gx2.cpp moby_terms_cpu: the same values, one block
// a draw) instead of five flat inputs - three inputs a pixel instead of eight: on Latte the pixels' rate follows their inputs' count (the
// sky's shells: PERFORMANCE.md))
layout(std140) uniform MobyTerms {
    vec4 vLod;
    vec4 vFnA;
    vec4 vFnB;
    vec4 vScl;
    vec4 vAt;
};
#else
flat in vec4 vLod;  // A (2^L, 0 with LCM), K, min(MXL, the levels - 1) + 0.5, the layer
flat in vec4 vFnA;  // RGB's A, B, C, then tm
flat in vec4 vFnB;  // alpha's A, B, C, then ta
flat in vec4 vScl;  // RGB's S, alpha's S, the levels - 1, 0
#ifndef RAC1_NO_ALPHA_TEST
flat in vec4 vAt;  // the alpha test's r, m, n, th
#endif
#endif
uniform sampler2DArray uTex;
layout(std140) uniform MobyPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255
    ivec4 uA;      // TME, ATE, ATST, AREF (here: ATE)
    ivec4 uB;      // AFAIL, the mip mode, the shade debug (0 here), FGE
    ivec4 uC;      // x: the pass; y, z: the bound array's width, height (float bits); w: 1 the failing pixels discarded
};
out vec4 FragColor;
void main() {
    vec4 c = vColor;
    vec2 st = vStq.xy / vStq.z;
    // (the derivatives in uniform control flow, as mrp_moby.frag's)
    vec2 tsz = intBitsToFloat(uC.yz);
    vec2 ddx = dFdx(st * tsz), ddy = dFdy(st * tsz);
    float auto_lod = 0.5 * log2(max(max(dot(ddx, ddx), dot(ddy, ddy)), 1e-20));
    float lod;
    if (uB.y == 0) {
        float l0 = log2(1.0 / max(abs(vStq.z), 1e-20)) * vLod.x + vLod.y;
        lod = floor(clamp(l0 + 0.5, 0.0, vLod.z));
    } else {
        lod = clamp(auto_lod, 0.0, vScl.z);
    }
    vec4 t = textureLod(uTex, vec3(st, vLod.w), lod);
    t = t * vec4(vFnA.w) + vec4(vFnB.w);
    vec4 p = t * (vec4(vFnA.xxx, vFnB.x) + vec4(vFnA.yyy, vFnB.y) * c);
    vec4 o = p * vec4(vScl.xxx, vScl.y) + vec4(vFnA.zzz, vFnB.z) * c.a;
    o = clamp(o, 0.0, 1.0);
    float f = uB.w != 0 ? vFog / 255.0 : 1.0;  // the GS's fogging (PRIM FGE): Cv F + FOGCOL (1 - F)
    o.rgb = o.rgb * f + uFogCol.rgb * (1.0 - f);
#ifndef RAC1_NO_ALPHA_TEST
    float y = o.a * 255.0 - vAt.x;
    bool pass = y * vAt.y + abs(y) * vAt.z >= vAt.w;
    if (uA.y != 0 && !pass && uC.w != 0) discard;
#endif
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
