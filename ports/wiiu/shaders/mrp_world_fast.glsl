// The fast world pixel shader's per-draw terms (mrp_world_fast.frag; 2026-10-28, the GPU's time): what mrp_world.frag decodes per pixel
// from a draw's texture word, layer and draw_flags, made once per vertex - flat outputs, the provoking (last) vertex's, as vTex / vState
// were. Put by ports/wiiu/shaders/variant.cmake where a vertex shader built with RAC1_FAST has "#pragma rac1_include mrp_world_fast.glsl"
// (mrp_world.vert, mrp_tfrag.vert, mrp_sky.vert): the outputs are declared here, fast_terms() writes them; the shader writes vStq, vColor,
// vFlat and gl_Position itself.
noperspective out vec2 vQF;  // the GS's Q for the mip level, F / 255
flat out vec4 vLod;          // 2^L (0 with LCM), K + 0.5, min(MXL, the texture's levels - 1) + 0.5, the layer
flat out vec4 vFnA;          // the texture function, RGB: o = t' (A + B c) + C c.a - A, B, C; then tm (t' = t tm + ta)
flat out vec4 vFnB;          // alpha's A, B, C; then ta
#ifndef RAC1_NO_ALPHA_TEST
flat out vec4 vAt;           // the alpha test: y = a 255 - r, pass y m + |y| n >= th - r, m, n, th
#endif
flat out vec4 vSel;          // 1 IIP, 1 the alpha test on, the texture unit (draw_flags' bits 16..19), 0
// orig: (host) `w` the texture word, `layer`, `fl` draw_flags (TME | IIP << 1 | FGE << 2 | the alpha test's mode << 3 | ATST << 4 |
// AREF << 8 | unit << 16), `lodq` the vertex's Q for the level, `fog` its F (0..255): mrp_world.frag's decoding, its results as terms
void fast_terms(uint w, uint layer, uint fl, float lodq, float fog) {
    vQF = vec2(lodq, fog * (1.0 / 255.0));
    // the mip level (mrp_world.frag: lod = LCM ? K : log2(1 / |q|) 2^L + K; level = lod <= 0 ? 0 : min(floor(lod + 0.5), MXL, mips - 1))
    int mips = int((w >> 22) & 7u) + 1;
    int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
    float K = float((int(w >> 10) << 20) >> 20) / 16.0;
    vLod = vec4(lcm != 0 ? 0.0 : float(1 << L), K + 0.5, float(min(mxl, mips - 1)) + 0.5, float(layer));
    // the texture function (mrp_world.frag's tfx / tcc branches; TME off: the colour)
    const float k2 = 255.0 / 128.0;
    uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
    vec3 a = vec3(0.0, 0.0, 1.0);  // (alpha with TCC off: the colour's)
    vec3 rgb;
    float tm = (w & 1u) != 0u ? 0.0 : 1.0, ta = (w & 1u) != 0u ? 128.0 / 255.0 : 0.0;
    if ((fl & 1u) == 0u) {
        rgb = vec3(0.0, 1.0, 0.0);
        tm = 0.0;
        ta = 1.0;
    } else if (tfx == 0u) {  // MODULATE
        rgb = vec3(0.0, k2, 0.0);
        if (tcc != 0u) a = vec3(0.0, k2, 0.0);
    } else if (tfx == 1u) {  // DECAL
        rgb = vec3(1.0, 0.0, 0.0);
        if (tcc != 0u) a = vec3(1.0, 0.0, 0.0);
    } else if (tfx == 2u) {  // HIGHLIGHT
        rgb = vec3(0.0, k2, 1.0);
        if (tcc != 0u) a = vec3(1.0, 0.0, 1.0);
    } else {  // HIGHLIGHT2
        rgb = vec3(0.0, k2, 1.0);
        if (tcc != 0u) a = vec3(1.0, 0.0, 0.0);
    }
    vFnA = vec4(rgb, tm);
    vFnB = vec4(a, ta);
#ifndef RAC1_NO_ALPHA_TEST
    // the alpha test (ATST: 0 NEVER, 1 ALWAYS, 2 LESS, 3 LEQUAL, 4 EQUAL, 5 GEQUAL, 6 GREATER, 7 NOTEQUAL; mrp_world.frag's comparisons
    // of a = o.a 255 with AREF): x = y m + |y| n >= th, y = a - AREF; a strict test's th the next float above its bound (y is 0 or at
    // least ~1e-5 from it: a's steps)
    uint atst = (fl >> 4) & 7u;
    float aref = float((fl >> 8) & 0xffu);
    vec4 at = vec4(0.0);  // (ALWAYS: 0 >= 0)
    if (atst == 0u) at = vec4(0.0, 0.0, 0.0, 1.0);                // NEVER: 0 >= 1, never
    else if (atst == 2u) at = vec4(aref, -1.0, 0.0, 1e-30);       // LESS: -y > 0
    else if (atst == 3u) at = vec4(aref, -1.0, 0.0, 0.0);         // LEQUAL: -y >= 0
    else if (atst == 4u) at = vec4(aref, 0.0, -1.0, -0.49999997); // EQUAL: -|y| > -0.5
    else if (atst == 5u) at = vec4(aref, 1.0, 0.0, 0.0);          // GEQUAL: y >= 0
    else if (atst == 6u) at = vec4(aref, 1.0, 0.0, 1e-30);        // GREATER: y > 0
    else if (atst == 7u) at = vec4(aref, 0.0, 1.0, 0.5);          // NOTEQUAL: |y| >= 0.5
    vAt = at;
#endif
    vSel = vec4(float((fl >> 1) & 1u), float((fl >> 3) & 1u), float((fl >> 16) & 15u), 0.0);
}
