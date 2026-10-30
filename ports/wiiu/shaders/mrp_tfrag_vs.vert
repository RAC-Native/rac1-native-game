// (2026-10-30) The MRP world's tfrags on the Wii U with their programs in this vertex shader (ports/wiiu/game/mrp_world_gx2.cpp,
// RAC1_WIIU_TFRAG_VS; src/game/render/mrp_tfrag_gpu_vs.inc): what the CPU route (mrp_world_renderer.cpp tfrag_draw_cpu_one over
// tfrag_exec_cpu_fast) makes per sent vertex, made here per triangle corner from the level's tables and the frame's block - no vertex
// written by the CPU. tfrag_vs_eval_cpu (mrp_tfrag_gpu_vs.inc) is this shader's C++ twin, operation for operation: change both.
//  - aCorner: draw << 16 | the triangle's closing vertex << 2 | the corner (0, 1, 2: the vertices closing - 2 .. closing);
//  - tD (the frame's block, little-endian words): per draw 3 texels (image's first texel, plan's first texel, colours' first word or ~0,
//    its first run's texel; its view set's texel, stoff, q0, flags; col_addr, col_count), per view set 17 (the VU view's R0, R1, R2, T;
//    the clip matrix's columns; the constants 0x290, 0x291, 0x298, 0x295, 0x296, 0x29a, 0x29b, 0x29c, 0x29d), per run 1 (texture word,
//    layer, draw_flags, FOGCOL);
//  - uImg: the level's tfrag images (a texel a qword, RGBA32UI, 4096 a row), uPlan: the plans' tables (RGBA32UI, 4096 a row), uTCol: the
//    resident lit colours (R32UI, 1024 a row: RGBA8, R in the low byte).
// Built RAC1_FAST only (its pixel shader mrp_world_fast.frag: shaders/mrp_tfrag_vs_fast.gsh, _noat without the alpha test - each pair
// compiled together: the varyings the same set as mrp_tfrag.vert's fast build, vAt under the same #ifndef as the pixel shader's).
#version 330 core
layout(location = 0) in uint aCorner;
layout(std140) uniform TfragVs {
    uvec4 tD[4096];
};
uniform usampler2D uImg;
uniform usampler2D uPlan;
uniform usampler2D uTCol;
smooth out vec3 vStq;
#pragma rac1_include mrp_world_fast.glsl
noperspective out vec4 vColor;
flat out vec4 vFlat;

uint gImg;
uint gPlan;
uint gCol;
uint gSet;
vec4 gV0;
vec4 gV1;
vec4 gV2;
vec4 gV3;

uvec4 img(uint i) { return texelFetch(uImg, ivec2(int(i & 4095u), int(i >> 12u)), 0); }
uvec4 pln(uint i) { return texelFetch(uPlan, ivec2(int(i & 4095u), int(i >> 12u)), 0); }
uint tcol(uint i) { return texelFetch(uTCol, ivec2(int(i & 1023u), int(i >> 10u)), 0).x; }
vec4 fv(uint i) { return uintBitsToFloat(tD[i]); }
uint vtx(uint j) {
    uvec4 t = pln(gPlan + 1u + (j >> 2u));
    uint l = j & 3u;
    return l == 0u ? t.x : l == 1u ? t.y : l == 2u ? t.z : t.w;
}
vec4 xform(vec4 p) {
    vec4 a = gV3;
    a = a + gV0 * p.x;
    a = a + gV1 * p.y;
    return a + gV2 * p.z;
}
vec4 itof(uvec4 u) { return vec4(ivec4(u)); }
// a record's colour (its qword q + 1): the lit colour's bytes (lit k + 1; none: 0), or the image's words
vec4 rcol(uint q, uint lit) {
    if (lit != 0u) {
        uint w = gCol != 0xffffffffu ? tcol(gCol + lit - 1u) : 0u;
        return vec4(uvec4(w) >> uvec4(0u, 8u, 16u, 24u) & 0xffu);
    }
    return itof(img(gImg + q + 1u));
}
// a record without morph: X (the raw record through the view) or raw (its words as floats)
void rec_x(uvec4 d, out vec4 p, out vec4 c) {
    uint q = d.x & 0xffffu;
    if ((d.x & 0x80000u) != 0u) p = uintBitsToFloat(img(gImg + q));
    else p = xform(itof(img(gImg + q)));
    c = rcol(q, d.w);
}
// M: X(q) morphed toward its parents by the band's factors, f = clamp(c + k p.w), p' = (P1 + P2) f.x + p f.y
void morph(uvec4 d, vec4 P1, vec4 C1, vec4 P2, vec4 C2, out vec4 p, out vec4 c) {
    uint q = d.x & 0xffffu;
    vec4 p0 = xform(itof(img(gImg + q)));
    vec4 c0 = rcol(q, d.w);
    uint band = (d.x >> 18u) & 1u;
    vec4 K = fv(gSet + 13u + band);
    vec4 C = fv(gSet + 15u + band);
    vec4 M = fv(gSet + 9u);
    float fx = C.x * 1.0 + K.x * p0.w;
    float fy = C.y * 1.0 + K.y * p0.w;
    fx = max(min(fx, M.x), 0.0);
    fy = max(min(fy, M.y), 0.0);
    p = (P1 + P2) * fx + p0 * fy;
    c = (C1 + C2) * fx + c0 * fy;
}
// a record of depth 0 or 1 (its parents X / raw), `i` its texel
void rec1(uint i, out vec4 p, out vec4 c) {
    uvec4 d = pln(i);
    if ((d.x & 0x100000u) == 0u) {
        rec_x(d, p, c);
        return;
    }
    vec4 P1, C1, P2, C2;
    rec_x(pln(gPlan + d.y), P1, C1);
    rec_x(pln(gPlan + d.z), P2, C2);
    morph(d, P1, C1, P2, C2, p, c);
}
// any record (depth 2: its parents of depth 1 at most)
void rec2(uint i, out vec4 p, out vec4 c) {
    uvec4 d = pln(i);
    if (((d.x >> 16u) & 3u) < 2u) {
        rec1(i, p, c);
        return;
    }
    vec4 P1, C1, P2, C2;
    rec1(gPlan + d.y, P1, C1);
    rec1(gPlan + d.z, P2, C2);
    morph(d, P1, C1, P2, C2, p, c);
}
// both records' w past the band's threshold (MS / F)
bool cnd(uint r1, uint r2, uint band) {
    float thr = fv(gSet + 13u + band).w;
    vec4 p, c;
    rec2(gPlan + r1, p, c);
    bool a = (p.w - thr) < 0.0;
    rec2(gPlan + r2, p, c);
    return a && (p.w - thr) < 0.0;
}
// the vertex info a slot ends on (its leaf's texel): B's selection, or with F (B's lane-2 record and A's lane-3 record past the
// threshold) ? A : B
uint leaf(uint slot) {
    uvec4 s0 = pln(gPlan + slot);
    uvec4 s1 = pln(gPlan + slot + 1u);
    uint f = s0.x;
    uint lb = s0.z >> 16u;
    if ((f & 16u) != 0u && cnd(s1.y & 0xffffu, s1.y >> 16u, (f >> 5u) & 1u)) lb = s0.z & 0xffffu;
    if ((f & 1u) == 0u) return lb;
    uint la = s0.y >> 16u;
    if ((f & 4u) != 0u && cnd(s1.x & 0xffffu, s1.x >> 16u, (f >> 3u) & 1u)) la = s0.y & 0xffffu;
    uvec4 LB = pln(gPlan + lb);
    uvec4 LA = pln(gPlan + la);
    if (LB.z != 0xffffu && cnd(LB.z, LA.y, (f >> 1u) & 1u)) return la;
    return lb;
}
// a sent vertex's position (forms 3 / 4: the raw record through the view)
vec4 raw_pos(uint vq) {
    uint rq = img(gImg + vq).w & 0xffffu;
    return xform(itof(img(gImg + rq)));
}
// the guard band: P x C6 outside its w (CLIPW)
bool outside(vec4 P, vec4 c6) {
    vec4 g = P * c6;
    float w = abs(g.w);
    return g.x > w || g.x < -w || g.y > w || g.y < -w || g.z > w || g.z < -w;
}
int ftoi(float v) { return int(clamp(v, -2147483648.0, 2147483520.0)); }

void main() {
    uint d = aCorner >> 16u;
    uint jc = (aCorner >> 2u) & 0x3fffu;
    uint j = jc - 2u + (aCorner & 3u);
    uvec4 t0 = tD[3u * d];
    uvec4 t1 = tD[3u * d + 1u];
    uvec4 t2 = tD[3u * d + 2u];
    gImg = t0.x;
    gPlan = t0.y;
    gCol = t0.z;
    gSet = t1.x;
    float stoff = uintBitsToFloat(t1.y);
    float q0 = uintBitsToFloat(t1.z);
    gV0 = fv(gSet);
    gV1 = fv(gSet + 1u);
    gV2 = fv(gSet + 2u);
    gV3 = fv(gSet + 3u);
    uint form = pln(gPlan).x;
    vec4 c0 = fv(gSet + 8u);
    vec4 c4 = fv(gSet + 10u);
    vec4 c5 = fv(gSet + 11u);
    vec4 c6 = fv(gSet + 12u);
    // ADC (form 3, not a caster): a vertex of the triangle - one whose flags count - outside the guard band drops it
    bool adc = false;
    if (form == 3u && (t1.w & 1u) == 0u) {
        for (uint k = 0u; k < 3u; ++k) {
            uint vk = vtx(jc - 2u + k);
            if ((vk & 0x10000u) != 0u && outside(raw_pos(vk & 0xffffu), c6)) adc = true;
        }
    }
    uint vt = vtx(j);
    vec4 P;
    uvec4 rgba;
    float s;
    float t;
    if (form == 0u) {
        uint lf = leaf(vt & 0xffffu);
        uvec4 LF = pln(gPlan + lf);
        uvec4 iq = img(gImg + LF.x);
        s = uintBitsToFloat(iq.x);
        t = uintBitsToFloat(iq.y);
        vec4 C;
        rec2(gPlan + LF.y, P, C);
        vec4 x = C * c4;
        rgba = uvec4(ftoi(x.x), ftoi(x.y), ftoi(x.z), ftoi(x.w)) & 0xffu;
    } else {
        uvec4 iq = img(gImg + (vt & 0xffffu));
        s = uintBitsToFloat(iq.x);
        t = uintBitsToFloat(iq.y);
        uint rq = iq.w & 0xffffu;
        P = xform(itof(img(gImg + rq)));
        // the record's colour words: the lit colour's bytes when its qword is one of them, else the image's
        uint cq = rq + 1u;
        if (cq >= t2.x && ((cq - t2.x) & 1u) == 0u && (cq - t2.x) / 2u < t2.y) {
            uint w = gCol != 0xffffffffu ? tcol(gCol + (cq - t2.x) / 2u) : 0u;
            rgba = uvec4(w) >> uvec4(0u, 8u, 16u, 24u) & 0xffu;
        } else {
            rgba = img(gImg + cq) & 0xffu;
        }
    }
    float s0 = s - stoff;
    float t0f = t - stoff;
    float q = c0.x / P.w;
    float qq = q0 * q;
    float ss = s0 * q;
    float tt = t0f * q;
    float fw = P.w + c5.w;
    if (form >= 3u) {
        fw = min(fw, c0.z);
        fw = max(fw, c0.y);
    } else {
        fw = max(fw, c0.y);
        fw = min(fw, c0.z);
    }
    float kk = qq * P.w;
    float sgn = kk < 0.0 ? -1.0 : 1.0;
    float Wg = P.w * sgn;
    float ws = Wg != 0.0 ? Wg : 1.0;
    float fog = float((uint(ftoi(fw * 16.0)) >> 4u) & 0xffu);
    uvec4 run = tD[t0.w + (vt >> 17u)];
    fast_terms(run.x, run.y, run.z, qq, fog);
    vStq = vec3(ss, tt, qq) * ws;
    vColor = vec4(rgba) / 255.0;
    vFlat = vColor;
    gl_Position = mat4(fv(gSet + 4u), fv(gSet + 5u), fv(gSet + 6u), fv(gSet + 7u)) * P;
    if (adc) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // (the triangle's three corners the same point outside the view: not drawn)
}
