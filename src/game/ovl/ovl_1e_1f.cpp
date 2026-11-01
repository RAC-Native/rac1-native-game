// Ported overlay code 0x001e0000..0x001fffff (excluding the math library, see game/math/vecmath.cpp).
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <platform/per_thread.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {

// orig: 0x001eb1f8 — linear search of the 48-entry table at 0x167150 (stride 0xa0) by the s16 at +0x86.
uint32_t moby_table_find_by_uid(int32_t uid) {
    const uint32_t end = g::MOBY_TABLE_167150 + 0x1e00;
    uint32_t p = g::MOBY_TABLE_167150;
    int32_t v = lh(p + 0x86);
    for (;;) {
        if (v == uid) return p;
        p += 0xa0;
        if (!(static_cast<int32_t>(p) < static_cast<int32_t>(end))) return 0;
        v = lh(p + 0x86);
    }
}

// orig: 0x001eb078 — printf-style stub: spills its varargs to the stack and returns.
void debug_printf_stub() {}

// orig: 0x001f35d8
int32_t sub_001f35d8() {
    const int32_t v = lw(OV(0x00173e5c));
    if (v < 0) return -1;
    const int32_t m = v & 0x1f;
    return m == 0x1f ? -1 : m;
}

// orig: 0x001f3608
int32_t sub_001f3608() {
    const int32_t v = lw(OV(0x00173e5c));
    if (v < 0) return 0;
    const int32_t m = v & 0x60;
    return m == 0x60 ? 0 : (m >> 5);
}

// orig: 0x001f6c20 — empty in this build.
void sub_001f6c20() {}

// orig: 0x001f6c30 — empty in this build (takes a position and a radius).
void sub_001f6c30(uint32_t, float) {}

// orig: 0x001f93e0 — append (a, b) to two parallel 64-entry arrays (count at 0x15f424).
void sub_001f93e0(uint32_t a, uint32_t b) {
    const int32_t n = lw(OV(0x0015f424));
    if (!(n < 0x40)) return;
    sw(OV(0x0016d880) + n * 4, a);
    sw(OV(0x0016d980) + n * 4, b);
    sw(OV(0x0015f424), static_cast<uint32_t>(n + 1));
}

// orig: 0x001eb128 — copy the hero moby (*(gp+0x200), 0xa0 bytes) to 0x168f50 and its
// 0x280-byte block 0x169110 to 0x169610, then point the copy's +0x70 at the latter.
void sub_001eb128() {
    const uint32_t copy = OV(0x00168f50), blk = OV(0x00169610);
    mem_copy128(copy, lwu(OV(0x00166e00)), 0xa0);
    mem_copy128(blk, blk - 0x500, 0x280);
    sw(copy + 0x70, blk);
}

// orig: 0x001ed630 — yaw/pitch/distance from b to a: out = {yaw, pitch, dist}.
// (The original also calls angle_sub(yaw, old out.x) and discards the result.)
int32_t sub_001ed630(uint32_t out, uint32_t a, uint32_t b) {
    Vec4 d;
    vec3_sub(d, V(a), V(b));
    const float len2 = vec2_length(d);
    const float pitch = atan2_f(len2, d.z);
    swc1(out + 4, pitch);
    const float yaw = atan2_f(d.x, d.y);
    (void)angle_sub(yaw, lwc1(out + 0));
    swc1(out + 0, yaw);
    swc1(out + 8, vec3_length(d));
    return 0;
}

// orig: 0x001fa050 — GS register write of the packed colour (r | g<<8 | b<<16 | a<<24, 64-bit
// shifts of the sign-extended arguments) to register 1, then a 0x14-qword DMA tag to 0x13cc90.
void sub_001fa050(int64_t r, int64_t g, int64_t b, int64_t a) {
    const uint64_t d = static_cast<uint64_t>(r) | (static_cast<uint64_t>(g) << 8) | (static_cast<uint64_t>(b) << 16) |
                       (static_cast<uint64_t>(a) << 24);
    sub_002a0ba8(1, d);
    constexpr OvConst<0x001611c0u> CUR{};
    sw(lwu(CUR) + 0x0, 0x30000014u);
    sw(lwu(CUR) + 0x4, 0x0013cc90u);
    sw(lwu(CUR) + 0x8, 0);
    sw(lwu(CUR) + 0xc, 0x50000014u);
    sw(CUR, lwu(CUR) + 0x10);
}


namespace {
// VU0 macro-mode lane arithmetic as the interpreter models it (operands and results clamped)
static float qadd(float a, float b) { return vuc(vuc(a) + vuc(b)); }
static float qsub(float a, float b) { return vuc(vuc(a) - vuc(b)); }
static float qmul(float a, float b) { return vuc(vuc(a) * vuc(b)); }
static float qmadd(float acc, float a, float b) { return vuc(vuc(acc) + vuc(a) * vuc(b)); }
static float qmsub(float acc, float a, float b) { return vuc(vuc(acc) - vuc(a) * vuc(b)); }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float qdiv(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) return u2f(((fbits(s) ^ fbits(t)) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}
static float qrsqrt(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    const uint32_t neg = (fbits(s) ^ fbits(t)) & 0x80000000u;
    if (b == 0.0f) return a != 0.0f ? u2f(neg != 0 ? 0xff7fffffu : 0x7f7fffffu) : u2f(neg);
    return vuc(a / std::sqrt(std::fabs(b)));
}
static int32_t qftoi0(float f) {
    const uint32_t u = fbits(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}
struct L3 {
    float x, y, z;
};
static L3 u16x3(uint32_t a) { return L3{static_cast<float>(lhu(a)), static_cast<float>(lhu(a + 2)), static_cast<float>(lhu(a + 4))}; }
static L3 l3(uint32_t a) { return L3{lwc1(a), lwc1(a + 4), lwc1(a + 8)}; }
static L3 add3(const L3& a, const L3& b) { return L3{qadd(a.x, b.x), qadd(a.y, b.y), qadd(a.z, b.z)}; }
static L3 sub3(const L3& a, const L3& b) { return L3{qsub(a.x, b.x), qsub(a.y, b.y), qsub(a.z, b.z)}; }
static L3 mul3(const L3& a, const L3& b) { return L3{qmul(a.x, b.x), qmul(a.y, b.y), qmul(a.z, b.z)}; }
static L3 muls(const L3& a, float s) { return L3{qmul(a.x, s), qmul(a.y, s), qmul(a.z, s)}; }
static L3 madds(const L3& acc, const L3& a, float s) { return L3{qmadd(acc.x, a.x, s), qmadd(acc.y, a.y, s), qmadd(acc.z, a.z, s)}; }
// vopmula ACC, a, b ; vopmsub d, b, a
static L3 opm(const L3& a, const L3& b) {
    return L3{qmsub(qmul(a.y, b.z), b.y, a.z), qmsub(qmul(a.z, b.x), b.z, a.x), qmsub(qmul(a.x, b.y), b.x, a.y)};
}
static float hsum(const L3& v) { return qmadd(qadd(v.x, v.y), 1.0f, v.z); }      // vadday.x + vmaddz.x
static float hsum_y(const L3& v) { return qmadd(qadd(v.y, v.x), 1.0f, v.z); }    // vaddax.y + vmaddz.y
static bool neg(float f) { return (fbits(f) & 0x80000000u) != 0; }
static void sq3(uint32_t a, const L3& v, uint32_t w) { swc1(a, v.x); swc1(a + 4, v.y); swc1(a + 8, v.z); sw(a + 0xc, w); }
// The same lane arithmetic for operands known to be clamped already (a VU result, a small integer's float): vuc(x) is x for those,
// so only the result is clamped - the same values as the q* forms above.
static float cadd(float a, float b) { return vuc(a + b); }
static float csub(float a, float b) { return vuc(a - b); }
static float cmul(float a, float b) { return vuc(a * b); }
static float cmadd(float acc, float a, float b) { return vuc(acc + a * b); }
static float cmsub(float acc, float a, float b) { return vuc(acc - a * b); }
static L3 cl3(uint32_t a) { return L3{vuc(lwc1(a)), vuc(lwc1(a + 4)), vuc(lwc1(a + 8))}; }
static L3 cadd3(const L3& a, const L3& b) { return L3{cadd(a.x, b.x), cadd(a.y, b.y), cadd(a.z, b.z)}; }
static L3 csub3(const L3& a, const L3& b) { return L3{csub(a.x, b.x), csub(a.y, b.y), csub(a.z, b.z)}; }
static L3 cmul3(const L3& a, const L3& b) { return L3{cmul(a.x, b.x), cmul(a.y, b.y), cmul(a.z, b.z)}; }
static L3 cmuls(const L3& a, float s) { return L3{cmul(a.x, s), cmul(a.y, s), cmul(a.z, s)}; }
static L3 cmadds(const L3& acc, const L3& a, float s) { return L3{cmadd(acc.x, a.x, s), cmadd(acc.y, a.y, s), cmadd(acc.z, a.z, s)}; }
static L3 copm(const L3& a, const L3& b) {
    return L3{cmsub(cmul(a.y, b.z), b.y, a.z), cmsub(cmul(a.z, b.x), b.z, a.x), cmsub(cmul(a.x, b.y), b.x, a.y)};
}
static float chsum(const L3& v) { return cmadd(cadd(v.x, v.y), 1.0f, v.z); }
// vf25..vf29 of VU0: the collision routines keep their nearest contact there and store it from
// there, so a contact that sets only some of them (a primitive in sphere_collide) stores the
// others as the previous contact left them. Kept here across calls, as the registers are.
struct VuContact {
    L3 v25{}, v26{}, v27{}, v28{}, v29{};
};
static VuContact g_vu;
}  // namespace

// The VU0 contact registers above zeroed (the oracle starts each original call with clear VU
// registers).
// orig: (host) test support
void collide_vu_reset() { g_vu = VuContact{}; }
// The contact registers above (vf25..vf29: 15 floats) saved and put back whole: a host stretch that runs the game's code on a
// journal of EE memory (mrp_lod.cpp's shadow runs) leaves them as it found them, as it leaves the memory.
// orig: (host)
void collide_vu_save(float out[15]) {
    static_assert(sizeof(VuContact) == 15 * sizeof(float), "the five registers' x / y / z");
    std::memcpy(out, &g_vu, sizeof(VuContact));
}
// orig: (host)
void collide_vu_restore(const float in[15]) { std::memcpy(&g_vu, in, sizeof(VuContact)); }

// orig: 0x001f3170 — sphere (pos, radius) against the collision mesh *(gp-0x75e8) in 1/64 units
// (hand-written VU0 macro code on the scratchpad): bounding spheres select chunks, their vertices
// get outcodes, and each front-facing triangle's closest point inside the radius shrinks it; the
// nearest hit goes to 0x173e40 (+0x20 point, +0x30 pushed-out pos, +0x40 normal, +0x50.. triangle,
// +0x1c 0x1000 | flags). Returns 1 on a hit. vf25..vf29 w lanes (caller leftovers) are stored as 0.
int32_t sphere_mesh_collide(uint32_t pos, float radius) {
    constexpr uint32_t SPR = 0x70000000u;
    uint32_t t8 = lwu(OV(0x00166c00u - 0x75e8));
    if (t8 == 0) return 0;
    int32_t t9 = lw(t8);
    t8 += 0x10;
    const float w21 = u2f(0x3f7fdf3bu);
    const float r = qmul(radius, 64.0f);
    const L3 p = muls(l3(pos), 64.0f);
    const L3 lo3 = L3{qsub(p.x, r), qsub(p.y, r), qsub(p.z, r)};
    const L3 hi3 = L3{qadd(p.x, r), qadd(p.y, r), qadd(p.z, r)};
    float r2 = qmul(r, r);
    // bounding spheres: collect the chunks whose sphere overlaps
    uint32_t list = SPR;
    for (; t9 > 0; --t9, t8 += 0x10) {
        const L3 c = u16x3(t8);
        float s = qadd(r, static_cast<float>(lhu(t8 + 6)));
        s = qmul(s, s);
        const L3 d = mul3(sub3(c, p), sub3(c, p));
        const float y = qmadd(qmadd(qsub(d.y, s), 1.0f, d.x), 1.0f, d.z);
        const int64_t v = static_cast<int64_t>((static_cast<uint64_t>(fbits(y)) << 32) | fbits(d.x));
        if (v > 0) continue;
        sw(list, t8);
        list += 4;
    }
    const uint16_t lo[3] = {static_cast<uint16_t>(qftoi0(lo3.x)), static_cast<uint16_t>(qftoi0(lo3.y)), static_cast<uint16_t>(qftoi0(lo3.z))};
    const uint16_t hi[3] = {static_cast<uint16_t>(qftoi0(hi3.x)), static_cast<uint16_t>(qftoi0(hi3.y)), static_cast<uint16_t>(qftoi0(hi3.z))};
    const uint32_t end = SPR + (list & 0xfff);
    uint32_t hit = 0, w2 = 0;  // w2: vf2.w, stored with the pushed-out position
    L3 h_pt{}, h_n{}, h_a{}, h_b{}, h_c{};
    for (uint32_t e = SPR; e != end; e += 4) {
        const uint32_t ent = lwu(e);
        // vertices (pairs) to the scratchpad as floats + outcode (bit 0 below, bit 1 above, per axis)
        uint32_t vs = lwu(ent + 0xc);
        uint32_t dst = SPR + 0x2000;
        int32_t nv = lhu(ent + 0xa);
        do {
            for (uint32_t k = 0; k < 2; ++k, vs += 8, dst += 0x10) {
                uint32_t oc = 0;
                for (uint32_t a = 0; a < 3; ++a) {
                    const uint16_t c = lhu(vs + a * 2);
                    if ((static_cast<uint16_t>(c - lo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                    if ((static_cast<uint16_t>(hi[a] - c) & 0x8000u) != 0) oc |= 2u << (a * 8);
                }
                sq3(dst, u16x3(vs), oc);
                if (k == 1) w2 = lhu(vs + 6);  // vf2 keeps the raw w of the pair's second vertex
            }
            nv -= 2;
        } while (nv > 0);
        // triangles
        uint32_t tri = lwu(ent + 0xc) + static_cast<uint32_t>(lhu(ent + 0xa)) * 8;
        const uint32_t tend = tri + static_cast<uint32_t>(lbu(ent + 8)) * 4;
        for (; tri != tend; tri += 4) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t flags = lbu(tri + 3);
            if ((lwu(a0 + 0xc) & lwu(a1 + 0xc) & lwu(a2 + 0xc)) != 0) continue;
            const L3 v1 = l3(a0);
            w2 = lwu(a1 + 0xc);
            const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
            L3 rel = sub3(p, v1);
            const L3 n = opm(e3, e2);
            const float dn = hsum(mul3(rel, n));
            const float q = qdiv(dn, hsum(mul3(n, n)));
            if (neg(dn)) continue;
            const L3 nn = sub3(L3{0.0f, 0.0f, 0.0f}, n);
            const L3 acc = add3(L3{0.0f, 0.0f, 0.0f}, rel);
            L3 t7 = muls(n, q);
            rel = madds(acc, nn, q);
            t7 = mul3(t7, t7);
            L3 t9v = sub3(e3, e2);
            const L3 t10 = sub3(rel, e2);
            float d2 = hsum(t7);
            t7 = opm(rel, e2);
            L3 t8v = opm(e3, rel);
            if (neg(qsub(r2, d2))) continue;
            t9v = opm(t10, t9v);
            const float s7 = hsum_y(mul3(t7, n)), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
            if (neg(s7) || neg(s8) || neg(s9)) {
                L3 a{}, b{};
                if (neg(s7)) { b = add3(e2, L3{}); }
                else if (neg(s8)) { a = add3(e3, L3{}); }
                else { a = add3(e2, L3{}); b = add3(e3, L3{}); }
                const L3 d = sub3(rel, a);
                b = sub3(b, a);
                float t = qadd(0.0f, qdiv(hsum(mul3(d, b)), hsum(mul3(b, b))));
                const uint32_t tb = fbits(t);
                if (static_cast<int32_t>(tb) < 0) t = 0.0f;  // vmax with +0
                else if (static_cast<int32_t>(tb) > static_cast<int32_t>(0x3f800000u)) t = 1.0f;
                rel = madds(add3(L3{}, a), b, t);
                const L3 off = sub3(rel, sub3(p, v1));
                d2 = hsum(mul3(off, off));
                if (neg(qsub(r2, d2))) continue;
            }
            hit = flags + 0x1000;
            r2 = qmul(d2, w21);
            h_n = add3(n, L3{});
            h_pt = add3(rel, v1);
            h_a = add3(L3{}, v1);
            h_b = add3(e2, v1);
            h_c = add3(e3, v1);
        }
    }
    if (hit == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, h_pt);
    const float k = u2f(0x3c800000u);
    h_pt = muls(h_pt, k);
    h_a = muls(h_a, k);
    h_b = muls(h_b, k);
    h_c = muls(h_c, k);
    constexpr OvConst<0x00173e40u> OUTB{};
    sq3(OUTB + 0x20, h_pt, 0);
    sq3(OUTB + 0x40, h_n, 0);
    sq3(OUTB + 0x50, h_a, 0);
    sq3(OUTB + 0x60, h_b, 0);
    sq3(OUTB + 0x70, h_c, 0);
    sw(OUTB + 0x18, 0);
    sw(OUTB + 0x1c, hit);
    push = add3(muls(muls(push, q), k), h_pt);
    sq3(OUTB + 0x30, push, w2);
    g_vu = VuContact{h_pt, h_n, h_a, h_b, h_c};
    return 1;
}

namespace {
static uint32_t fp_maxb(uint32_t a, uint32_t b) {  // VMAX on raw lanes
    return (static_cast<int32_t>(a) < 0 && static_cast<int32_t>(b) < 0) ? static_cast<uint32_t>(std::min(static_cast<int32_t>(a), static_cast<int32_t>(b)))
                                                                          : static_cast<uint32_t>(std::max(static_cast<int32_t>(a), static_cast<int32_t>(b)));
}
static uint32_t fp_minb(uint32_t a, uint32_t b) {  // VMINI on raw lanes
    return (static_cast<int32_t>(a) < 0 && static_cast<int32_t>(b) < 0) ? static_cast<uint32_t>(std::max(static_cast<int32_t>(a), static_cast<int32_t>(b)))
                                                                          : static_cast<uint32_t>(std::min(static_cast<int32_t>(a), static_cast<int32_t>(b)));
}
static float fmaxv(float a, float b) { return u2f(fp_maxb(fbits(a), fbits(b))); }
static float fminv(float a, float b) { return u2f(fp_minb(fbits(a), fbits(b))); }
static float clamp01(float t) { return fminv(fmaxv(t, 0.0f), 1.0f); }
static float qsqrt(float x) { return vuc(std::sqrt(std::fabs(vuc(x)))); }
static void sq4f(uint32_t a, float x, float y, float z, float w) { swc1(a, x); swc1(a + 4, y); swc1(a + 8, z); swc1(a + 0xc, w); }

// the primitive records of a moby's collision data (0x20 bytes from col + 0x10, enabled unless
// the bit in moby +0x98 is set, selected by the query's set bit, the last one with +2 < 0):
// does the sphere (p, r; r2 = r * r) touch one? All in 1024ths, the moby at mp with rows R*, scale sc.
static bool moby_prims_hit(uint32_t m, uint32_t col, uint32_t flags, uint32_t s0, const L3& p, float r, float r2, const L3& mp,
                           const L3& R0, const L3& R1, const L3& R2, float sc) {
    if (lwu(col + 4) == 0) return false;
    auto xf = [&](const L3& v) { return madds(madds(madds(muls(R0, v.x), R1, v.y), R2, v.z), mp, 1.0f); };
    const int32_t want = (flags & 4) == 0 ? 2 : 1;
    uint32_t mask = lwu(m + 0x98);
    int32_t more = 0;
    for (uint32_t rec = col + 0x10; more >= 0; rec += 0x20) {
        const uint32_t off = mask & 1;
        mask >>= 1;
        more = lh(rec + 2);
        const int32_t type = static_cast<int8_t>(lbu(rec));
        if (off != 0 || (want & more) == 0) continue;
        L3 c;
        float w;
        if (type == 1) {  // sphere
            c = xf(muls(l3(rec + 0x10), sc));
            w = qmul(lwc1(rec + 0x1c), sc);
        } else if (type == 2) {  // sphere on a joint
            const uint32_t j = s0 + (lwu(rec + 4) << 4);
            c = xf(muls(add3(l3(j), l3(rec + 0x10)), sc));
            w = qmul(lwc1(rec + 0xc), sc);
        } else if (type == 3) {  // vertical capsule: z clamped to [base, base + height]
            c = xf(muls(l3(rec + 0x10), sc));
            w = qmul(lwc1(rec + 0x1c), sc);
            const float top = qadd(c.z, qmul(lwc1(rec + 4), sc));
            c.z = fminv(top, fmaxv(c.z, p.z));
        } else {  // capsule between two joints
            const L3 a = xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
            L3 ab = sub3(xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4)), sc)), a);
            const L3 ap = sub3(p, a);
            const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(ap, ab)), hsum(mul3(ab, ab)))));
            ab = muls(ab, t);
            c = add3(a, ab);
            w = qmul(lwc1(rec + 0xc), sc);
        }
        // sphere (c, w) against the query sphere
        float sr = qadd(r, w);
        const L3 dv = sub3(c, p);
        const float d2 = hsum(mul3(dv, dv));
        sr = qmul(sr, sr);
        const float q = qsqrt(d2);
        if (neg(qsub(sr, d2))) continue;
        float dist = qadd(0.0f, q);
        const float inv = qadd(0.0f, qdiv(1.0f, dist));
        dist = qmul(dist, 1.0f);
        const float reach = fminv(dist, w);
        c = add3(c, muls(muls(sub3(p, c), inv), reach));
        const L3 e = sub3(c, p);
        if (neg(qsub(r2, hsum(mul3(e, e))))) continue;
        return true;
    }
    return false;
}
}  // namespace

// Mobys touched by a sphere (pos, radius), hand-written VU0 macro code: the
// grid cells (0x19b7e0) under the sphere give moby ids; a moby counts once per query (+0x9c =
// query counter 0x173e50) when its bounding sphere (+0x00, w = radius) overlaps and it has
// collision data (+0x94): first its triangles (vertices posed by the moby's rows, outcodes,
// front faces unless flags & 0x10, types filtered by flags >> 8 when flags & 0x20, skipped with
// tri flag & 0x80 when flags & 0x80), then its primitive records (spheres, joint spheres,
// vertical capsules, joint segments; joint positions from moby_pose_matrices, cached in the
// 8-entry ring at *(0x173e48)). Hits are listed at 0x177f00 (0-terminated); with `rec`, mobys
// flagged 0x4000 also get a 64-byte contact record in the ring 0x178100. Returns the count.
// `ignore` is skipped. Coordinates must lie in [r, 1024 - r).
// orig: 0x001f2868
int32_t mobys_in_sphere_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore, const uint32_t* rec) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0), HITS = OV(0x00177f00);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    sw(OUTB + 0x10, query);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    const float r0 = radius;
    const L3 lo = L3{qsub(p0.x, r0), qsub(p0.y, r0), qsub(p0.z, r0)};
    const L3 hi = L3{qadd(p0.x, r0), qadd(p0.y, r0), qadd(p0.z, r0)};
    // MAC sign flags of lo - 0 and of hi - 1024
    if (neg(qsub(lo.x, 0.0f)) || neg(qsub(lo.y, 0.0f)) || neg(qsub(lo.z, 0.0f))) return 0;
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi.x, 1024.0f)) && neg(qsub(hi.y, 1024.0f)) && neg(qsub(hi.z, 1024.0f)))) return 0;
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f);
    const float r2 = qmul(r, r);
    // the cells: [start, end) of u16 moby ids each, in a zero-terminated list on the scratchpad
    uint32_t cl = SPR + 0x3800;
    const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 4, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 4;
    const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 4, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 4;
    for (uint32_t y = y0;; ++y) {
        for (uint32_t x = x0;; ++x) {
            const uint32_t c = GRID + (x << 2) + (y << 8);
            const uint32_t start = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
            sw(cl, start);
            sw(cl + 4, start + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2));
            cl += 8;
            if (x == x1) break;
        }
        if (y == y1) break;
    }
    sd(cl, 0);
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    uint32_t hits = HITS;
    uint32_t s0 = SPR;             // joint positions: scratchpad or a cached buffer (kept across mobys)
    uint32_t a3 = GRID;            // $a3, stored in the cache ring (stale from earlier work)
    const L3 zero{0.0f, 0.0f, 0.0f};
    for (uint32_t cell = SPR + 0x3800;; cell += 8) {
        uint32_t id = lwu(cell);
        if (id == 0) break;
        const uint32_t cend = lwu(cell + 4);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            // bounding spheres
            const L3 c = l3(m);
            const float sr = qadd(r, lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            const L3 d = mul3(sub3(c, p), sub3(c, p));
            const float ov = qmadd(qmadd(qsub(d.x, qmul(sr, sr)), 1.0f, d.y), 1.0f, d.z);
            if (lwu(m + 0x9c) == query) continue;
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t key = lwu(m + 0xa8);
            sw(m + 0x9c, query);
            if (nj != 0) {
                const uint32_t ring = lwu(OUTB + 8);
                uint32_t buf = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = buf + i * 0x800;
                        break;
                    }
                }
                if (!cached) {
                    const uint32_t slot = lwu(OUTB + 0xc);
                    sw(OUTB + 0xc, (slot + 1) & 7);
                    sw(ring + slot * 8, key);
                    sw(ring + slot * 8 + 4, a3);
                    a3 = moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, cell);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            bool hit = false;
            const uint32_t vbytes = lwu(col + 0xc);
            if (vbytes != 0) {
                // vertices, posed (scaled rows, relative to the moby), with outcodes
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                const L3 rel = sub3(p, mp);
                const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                const L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                for (int32_t left = static_cast<int32_t>(vbytes), k = 0; left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
                    for (uint32_t h = 0; h < 2; ++h) {
                        const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + h * 8;
                        const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                        const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        const L3 below = sub3(w, vlo), above = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(below.x)) oc |= 1u;
                        if (neg(above.x)) oc |= 2u;
                        if (neg(below.y)) oc |= 1u << 8;
                        if (neg(above.y)) oc |= 2u << 8;
                        if (neg(below.z)) oc |= 1u << 16;
                        if (neg(above.z)) oc |= 2u << 16;
                        sq3(dst + h * 0x10, w, oc);
                        if (h == 0) a3 = neg(above.x) ? 2u : 0u;
                    }
                }
                // triangles: 4-byte vertex index triples + flags
                uint32_t tri = vbase + vbytes;
                const uint32_t tend = tri + lwu(col + 8);
                for (; tri != tend && !hit;) {
                    const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
                    const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
                    const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
                    const uint32_t tf = lbu(tri + 3);
                    tri += 4;
                    a3 = lwu(a1 + 0xc);
                    if ((lwu(a0 + 0xc) & a3 & lwu(a2 + 0xc)) != 0) continue;
                    if ((flags & 0x80 & tf) != 0) continue;
                    const L3 v1 = l3(a0);
                    const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
                    L3 rp = sub3(sub3(p, mp), v1);
                    a3 = ((flags >> 8) ^ tf) & 0x1f;
                    if ((flags & 0x20) != 0 && a3 == 0) continue;
                    const L3 n = opm(e3, e2);
                    const float dn = hsum(mul3(rp, n));
                    const float q = qdiv(dn, hsum(mul3(n, n)));
                    a3 = 0;
                    if ((flags & 0x10) == 0 && neg(dn)) continue;
                    const L3 acc = add3(zero, rp);
                    L3 t7 = muls(n, q);
                    rp = madds(acc, sub3(zero, n), q);
                    t7 = mul3(t7, t7);
                    L3 t9v = sub3(e3, e2);
                    const L3 t10 = sub3(rp, e2);
                    float d2 = hsum(t7);
                    t7 = opm(rp, e2);
                    L3 t8v = opm(e3, rp);
                    if (neg(qsub(r2, d2))) continue;
                    t9v = opm(t10, t9v);
                    const L3 m7 = mul3(t7, n);
                    a3 = fbits(m7.x);
                    const float s7 = hsum_y(m7), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
                    if (neg(s7) || neg(s8) || neg(s9)) {
                        L3 ea{}, eb{};
                        if (neg(s7)) { eb = add3(e2, zero); }
                        else if (neg(s8)) { ea = add3(e3, zero); }
                        else { ea = add3(e2, zero); eb = add3(e3, zero); }
                        const L3 dd = sub3(rp, ea);
                        eb = sub3(eb, ea);
                        const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                        rp = madds(add3(zero, ea), eb, t);
                        const L3 off = sub3(rp, sub3(sub3(p, mp), v1));
                        if (neg(qsub(r2, hsum(mul3(off, off))))) continue;
                    }
                    hit = true;
                }
            }
            if (!hit) hit = moby_prims_hit(m, col, flags, s0, p, r, r2, mp, R0, R1, R2, sc);
            if (hit) {
                sw(hits, m);
                hits += 4;
            }
        }
    }
    sw(hits, 0);
    sq4f(OUTB + 0x20, 0.0f, 0.0f, 0.0f, 1.0f);
    sw(OUTB + 0x1c, 0x3f);
    sw(OUTB + 0x18, lwu(HITS));
    if (rec != nullptr && hits != HITS) {
        constexpr OvConst<0x00178100u> CONTACTS{};
        uint32_t next = lwu(OUTB + 0x14);
        for (uint32_t h = HITS; h != hits; h += 4) {
            const uint32_t m = lwu(h);
            if ((lhu(m + 0x34) & 0x4000) == 0) continue;
            const int32_t old = static_cast<int8_t>(lbu(m + 0xa4));
            if (old >= 0) {
                const uint32_t o = CONTACTS + static_cast<uint32_t>(old) * 64;
                if (lwu(o + 0x34) == m && static_cast<int32_t>(lwu(o + 0x2c) - rec[7]) > 0) continue;
            }
            const uint32_t c = CONTACTS + next * 64;
            sq4f(c, 0.0f, 0.0f, 0.0f, 1.0f);
            sw(c + 0x38, 0);
            for (uint32_t w = 0; w < 9; ++w) sw(c + 0x10 + w * 4, rec[w]);
            sw(c + 0x34, m);
            sb(m + 0xa4, next);
            next = (next + 1) & 0x3f;
        }
        sw(OUTB + 0x14, next);
    }
    return static_cast<int32_t>((hits - HITS) >> 2);
}
// orig: 0x001f2868 (EE-address form)
int32_t mobys_in_sphere(uint32_t pos, float radius, uint32_t flags, uint32_t ignore, uint32_t rec) {
    return mobys_in_sphere_v(V(pos), radius, flags, ignore, rec != 0 ? static_cast<const uint32_t*>(ee_ram_ptr(rec)) : nullptr);
}

namespace {
// 0x1efbd0 (custom register ABI: tree t8, z v1, y v0, x at -> at): the level collision tree, three
// levels of (u16 first, u16 count, entries); 0 outside or for an empty cell.
static uint32_t coll_tree_cell(uint32_t tree, int32_t x, int32_t y, int32_t z) {
    int32_t i = z - static_cast<int32_t>(lhu(tree));
    if (i < 0 || static_cast<int32_t>(lhu(tree + 2)) - i <= 0) return 0;
    const uint32_t zl = static_cast<uint32_t>(lhu(tree + 4 + static_cast<uint32_t>(i) * 2)) << 2;
    if (zl == 0) return 0;
    const uint32_t yn = zl + tree;
    i = y - static_cast<int32_t>(lhu(yn));
    if (i < 0 || static_cast<int32_t>(lhu(yn + 2)) - i <= 0) return 0;
    const uint32_t yl = lwu(yn + 4 + static_cast<uint32_t>(i) * 4);
    if (yl == 0) return 0;
    const uint32_t xn = yl + tree;
    i = x - static_cast<int32_t>(lhu(xn));
    if (i < 0 || static_cast<int32_t>(lhu(xn + 2)) - i <= 0) return 0;
    return lwu(xn + 4 + static_cast<uint32_t>(i) * 4);
}
static int16_t h16(int32_t v) { return static_cast<int16_t>(static_cast<uint16_t>(v)); }
}  // namespace

// Nearest contact of a sphere (pos, radius) with the level collision and the mobys, hand-written
// VU0 macro code. Level (unless flags & 1): the 4-unit cells of the tree *(0x173e40) near the sphere
// are fetched to the scratchpad by DMA (double-buffered); packed vertices (10/10/12 bits) get
// outcodes, then the triangles (and each chunk's quads as second triangles) are tested as in
// mobys_in_sphere, each hit shrinking the radius (x 0.999). Mobys: as mobys_in_sphere, without
// their primitives when flags & 2. Writes the nearest contact to 0x173e40 (+0x18 moby or 0, +0x1c
// 0x1000 | triangle flags or minus the primitive record, +0x20 point, +0x30 pushed-out position,
// +0x40 normal / push, +0x50.. triangle: the previous contact's after a primitive) and returns 1, or
// 0 without a contact. The w lanes of vf25..vf29 (never written) are stored as 0; the w of +0x30
// is the last vf2.w this call loaded (0 if none: the register's older content is not modelled).
// orig: 0x001f0d60
int32_t sphere_collide_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    const L3 lo0 = L3{qsub(p0.x, radius), qsub(p0.y, radius), qsub(p0.z, radius)};
    const L3 hi0 = L3{qadd(p0.x, radius), qadd(p0.y, radius), qadd(p0.z, radius)};
    sw(OUTB + 0x10, query);
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f);
    float r2 = qmul(r, r);
    if (neg(qsub(lo0.x, 0.0f)) || neg(qsub(lo0.y, 0.0f)) || neg(qsub(lo0.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi0.x, 1024.0f)) && neg(qsub(hi0.y, 1024.0f)) && neg(qsub(hi0.z, 1024.0f)))) return 0;
    const int32_t ilo[3] = {qftoi0(lo0.x), qftoi0(lo0.y), qftoi0(lo0.z)};
    const int32_t ihi[3] = {qftoi0(hi0.x), qftoi0(hi0.y), qftoi0(hi0.z)};
    const L3 lo = muls(lo0, 1024.0f), hi = muls(hi0, 1024.0f);
    const float nr2 = qsub(0.0f, r2);
    const float k999 = qmul(1.0f, u2f(0x3f7fdf3bu));
    r2 = qmul(r2, k999);
    const L3 zero{0.0f, 0.0f, 0.0f};
    // the nearest contact so far
    uint32_t s5 = 0, s6 = 0, w2 = 0;
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27, v28 = g_vu.v28, v29 = g_vu.v29;
    uint32_t s0 = SPR, a3 = 0;
    // triangles [tri, tend) over vertices at SPR + 0x2000 (relative to `org`, query `rel`), tri flags in byte 3
    auto tri_scan = [&](uint32_t tri, uint32_t tend, const L3& rel, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t tf = lbu(tri + 3);
            tri += 4;
            a3 = lwu(a1 + 0xc);
            if ((lwu(a0 + 0xc) & a3 & lwu(a2 + 0xc)) != 0) continue;
            w2 = a3;
            if ((flags & 0x80 & tf) != 0) continue;
            const L3 v1 = l3(a0);
            const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
            L3 rp = sub3(rel, v1);
            a3 = ((flags >> 8) ^ tf) & 0x1f;
            if ((flags & 0x20) != 0 && a3 == 0) continue;
            const L3 n = opm(e3, e2);
            const float dn = hsum(mul3(rp, n));
            const float q = qdiv(dn, hsum(mul3(n, n)));
            a3 = 0;
            if ((flags & 0x10) == 0 && neg(dn)) continue;
            const L3 acc = add3(zero, rp);
            L3 t7 = muls(n, q);
            rp = madds(acc, sub3(zero, n), q);
            t7 = mul3(t7, t7);
            L3 t9v = sub3(e3, e2);
            const L3 t10 = sub3(rp, e2);
            float d2 = hsum(t7);
            t7 = opm(rp, e2);
            L3 t8v = opm(e3, rp);
            if (neg(qsub(r2, d2))) continue;
            t9v = opm(t10, t9v);
            const L3 m7 = mul3(t7, n);
            a3 = fbits(m7.x);
            const float s7 = hsum_y(m7), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
            if (neg(s7) || neg(s8) || neg(s9)) {
                L3 ea{}, eb{};
                if (neg(s7)) { eb = add3(e2, zero); }
                else if (neg(s8)) { ea = add3(e3, zero); }
                else { ea = add3(e2, zero); eb = add3(e3, zero); }
                const L3 dd = sub3(rp, ea);
                eb = sub3(eb, ea);
                const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                rp = madds(add3(zero, ea), eb, t);
                const L3 off = sub3(rp, sub3(rel, v1));
                d2 = hsum(mul3(off, off));
                if (neg(qsub(r2, d2))) continue;
            }
            s5 = obj;
            const L3 wv1 = add3(v1, org);
            s6 = tf + 0x1000;
            r2 = qmul(d2, k999);
            v26 = add3(n, zero);
            v25 = add3(rp, wv1);
            v27 = add3(zero, wv1);
            v28 = add3(e2, wv1);
            v29 = add3(e3, wv1);
        }
    };
    if ((flags & 1) == 0) {
        // the level: cells of the collision tree whose box reaches the sphere, as a list of
        // (centre, chunk) on the scratchpad; the first chunk is fetched right away
        sd(SPR, 0);
        uint32_t list = SPR + 0x3800;
        bool fetching = false;
        const int32_t c0[3] = {static_cast<int32_t>(static_cast<uint32_t>(ilo[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[2]) >> 2)};
        const int32_t c1[3] = {static_cast<int32_t>(static_cast<uint32_t>(ihi[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[2]) >> 2)};
        const L3 base = L3{static_cast<float>(c0[0] << 12), static_cast<float>(c0[1] << 12), static_cast<float>(c0[2] << 12)};
        L3 corner = base;
        for (int32_t cz = c0[2];; ++cz) {
            corner.y = base.y;
            for (int32_t cy = c0[1];; ++cy) {
                corner.x = base.x;
                for (int32_t cx = c0[0];; ++cx) {
                    const L3 far = L3{qadd(corner.x, 4096.0f), qadd(corner.y, 4096.0f), qadd(corner.z, 4096.0f)};
                    const L3 cl = L3{fminv(fmaxv(corner.x, p.x), far.x), fminv(fmaxv(corner.y, p.y), far.y), fminv(fmaxv(corner.z, p.z), far.z)};
                    const L3 ctr = L3{qadd(corner.x, 2048.0f), qadd(corner.y, 2048.0f), qadd(corner.z, 2048.0f)};
                    const L3 d = sub3(cl, p);
                    const L3 dq = mul3(d, d);
                    const float gap = qmadd(qmadd(qadd(nr2, dq.x), 1.0f, dq.y), 1.0f, dq.z);
                    if (neg(gap)) {
                        const uint32_t chunk = coll_tree_cell(tree, cx, cy, cz);
                        sq3(list, ctr, 0);
                        sw(list + 0xc, chunk);
                        if (chunk != 0) {
                            list += 0x10;
                            if (!fetching) {
                                fetching = true;
                                spr_dma_to(0, (chunk >> 8) + tree, chunk & 0xff);
                            }
                        }
                    }
                    corner.x = qadd(corner.x, 4096.0f);
                    if (cx == c1[0]) break;
                }
                corner.y = qadd(corner.y, 4096.0f);
                if (cy == c1[1]) break;
            }
            corner.z = qadd(corner.z, 4096.0f);
            if (cz == c1[2]) break;
        }
        sw(list + 0xc, 0);
        // the chunks: two buffers (SPR + 0 / + 0x1000), the next one fetched while this one is tested
        uint32_t buf = SPR + 0x1000;
        for (uint32_t e = SPR + 0x3800;; e += 0x10) {
            const uint32_t chunk = lwu(e + 0xc), next = lwu(e + 0x1c);
            const L3 org = l3(e);
            if (chunk == 0) break;
            buf ^= 0x1000;
            if (next != 0) {
                const uint32_t other = buf ^ 0x1000;
                sd(other, 0);
                spr_dma_to(other - SPR, (next >> 8) + tree, next & 0xff);
            }
            const uint32_t nv = lbu(buf + 2);
            const L3 rel = sub3(p, org);
            if (nv == 0) continue;
            const L3 rlo = sub3(lo, org), rhi = sub3(hi, org);
            const int16_t blo[3] = {h16(qftoi0(rlo.x)), h16(qftoi0(rlo.y)), h16(qftoi0(rlo.z))};
            const int16_t bhi[3] = {h16(qftoi0(rhi.x)), h16(qftoi0(rhi.y)), h16(qftoi0(rhi.z))};
            uint32_t src = buf + 4, dst = SPR + 0x1fe0;
            for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
                dst += 0x20;
                for (uint32_t h = 0; h < 2; ++h, src += 4) {
                    const uint32_t pv = lwu(src);
                    const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
                    uint32_t oc = 0;
                    for (uint32_t a = 0; a < 3; ++a) {
                        if ((static_cast<uint16_t>(c[a] - blo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                        if ((static_cast<uint16_t>(bhi[a] - c[a]) & 0x8000u) != 0) oc |= 2u << (a * 8);
                    }
                    const L3 vf = L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])};
                    sq3(dst + h * 0x10, vf, oc);
                }
                w2 = 0;
            }
            const uint32_t t0 = buf + 4 + nv * 4;
            const uint32_t tend = t0 + static_cast<uint32_t>(lbu(buf)) * 4;
            tri_scan(t0, tend, rel, org, tree);
            // quads: the first lbu(+3) triangles again as (a, c, extra vertex)
            const uint32_t nq = lbu(buf + 3);
            if (nq == 0) continue;
            for (uint32_t i = 0; i < nq; ++i) {
                const uint32_t t = t0 + i * 4;
                const uint32_t c = lbu(t + 2), x = lbu(tend + i);
                sb(t + 1, c);
                sb(t + 2, x);
            }
            tri_scan(t0, t0 + nq * 4, rel, org, tree);
        }
    }
    // the mobys
    s5 = 0;
    {
        const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 14, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 14;
        const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 14, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 14;
        uint32_t cl = SPR + 0x3800;
        for (uint32_t y = y0;; ++y) {
            for (uint32_t x = x0;; ++x) {
                const uint32_t c = GRID + (x << 2) + (y << 8);
                const uint32_t start = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
                sw(cl, start);
                sw(cl + 4, start + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2));
                cl += 8;
                if (x == x1) break;
            }
            if (y == y1) break;
        }
        sd(cl, 0);
    }
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    a3 = GRID;
    for (uint32_t cell = SPR + 0x3800;; cell += 8) {
        uint32_t id = lwu(cell);
        if (id == 0) break;
        const uint32_t cend = lwu(cell + 4);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            const L3 c = l3(m);
            const float sr = qadd(r, lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            const L3 d = mul3(sub3(c, p), sub3(c, p));
            const float ov = qmadd(qmadd(qsub(d.x, qmul(sr, sr)), 1.0f, d.y), 1.0f, d.z);
            if (lwu(m + 0x9c) == query) continue;
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t vbytes = lwu(col + 0xc);
            sw(m + 0x9c, query);
            if (nj != 0) {
                if (((flags & 2) == 0 ? 1u : 0u) == vbytes) continue;
                const uint32_t key = lwu(m + 0xa8);
                const uint32_t ring = lwu(OUTB + 8);
                const uint32_t bufs = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = bufs + i * 0x800;
                        break;
                    }
                }
                if (!cached) {
                    const uint32_t slot = lwu(OUTB + 0xc);
                    sw(OUTB + 0xc, (slot + 1) & 7);
                    sw(ring + slot * 8, key);
                    sw(ring + slot * 8 + 4, a3);
                    a3 = moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, cell);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            if (vbytes != 0) {
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                const L3 rel = sub3(p, mp);
                const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                const L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                int32_t k = 0;
                for (int32_t left = static_cast<int32_t>(vbytes); left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
                    for (uint32_t h = 0; h < 2; ++h) {
                        const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + h * 8;
                        const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                        const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        const L3 below = sub3(w, vlo), above = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(below.x)) oc |= 1u;
                        if (neg(above.x)) oc |= 2u;
                        if (neg(below.y)) oc |= 1u << 8;
                        if (neg(above.y)) oc |= 2u << 8;
                        if (neg(below.z)) oc |= 1u << 16;
                        if (neg(above.z)) oc |= 2u << 16;
                        sq3(dst + h * 0x10, w, oc);
                        if (h == 0) a3 = neg(above.x) ? 2u : 0u;
                    }
                }
                w2 = static_cast<uint32_t>(static_cast<int32_t>(lh(vbase + static_cast<uint32_t>(k) * 0x10 + 0xe)));  // prefetched vertex
                const uint32_t t0 = vbase + vbytes;
                tri_scan(t0, t0 + lwu(col + 8), rel, mp, m);
            }
            // primitives, nearest kept
            if ((flags & 2) != 0 || lwu(col + 4) == 0) continue;
            auto xf = [&](const L3& v) { return madds(madds(madds(muls(R0, v.x), R1, v.y), R2, v.z), mp, 1.0f); };
            const int32_t want = (flags & 4) == 0 ? 2 : 1;
            uint32_t mask = lwu(m + 0x98);
            int32_t more = 0;
            for (uint32_t rec = col + 0x10; more >= 0; rec += 0x20) {
                const uint32_t off = mask & 1;
                mask >>= 1;
                more = lh(rec + 2);
                const int32_t type = static_cast<int8_t>(lbu(rec));
                if (off != 0 || (want & more) == 0) continue;
                L3 pc;
                float pw;
                if (type == 1) {
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                } else if (type == 2) {
                    w2 = lwu(rec + 0xc);
                    const uint32_t j = s0 + (lwu(rec + 4) << 4);
                    pc = xf(muls(add3(l3(j), l3(rec + 0x10)), sc));
                    pw = qmul(lwc1(rec + 0xc), sc);
                } else if (type == 3) {
                    w2 = lwu(rec + 0xc);
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                    const float top = qadd(pc.z, qmul(lwc1(rec + 4), sc));
                    pc.z = fminv(top, fmaxv(pc.z, p.z));
                } else {
                    const uint32_t jb = s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4);
                    w2 = lwu(jb + 0xc);
                    const L3 a = xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
                    L3 ab = sub3(xf(muls(l3(jb), sc)), a);
                    const L3 ap = sub3(p, a);
                    const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(ap, ab)), hsum(mul3(ab, ab)))));
                    ab = muls(ab, t);
                    pc = add3(a, ab);
                    pw = qmul(lwc1(rec + 0xc), sc);
                }
                float psr = qadd(r, pw);
                const L3 dv = sub3(pc, p);
                const float d2 = hsum(mul3(dv, dv));
                psr = qmul(psr, psr);
                const float q = qsqrt(d2);
                if (neg(qsub(psr, d2))) continue;
                float dist = qadd(0.0f, q);
                const float inv = qadd(0.0f, qdiv(1.0f, dist));
                dist = qmul(dist, 1.0f);
                const float reach = fminv(dist, pw);
                const L3 push = muls(muls(sub3(p, pc), inv), reach);
                pc = add3(pc, push);
                const L3 e = sub3(pc, p);
                const float d2b = hsum(mul3(e, e));
                if (neg(qsub(r2, d2b))) continue;
                s5 = m;
                s6 = 0u - rec;
                r2 = qmul(d2b, 1.0f);
                v25 = add3(zero, pc);
                v26 = add3(zero, push);
            }
        }
    }
    if (s6 == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, v25);
    const float k = u2f(0x3a800000u);
    v25 = muls(v25, k);
    v27 = muls(v27, k);
    v28 = muls(v28, k);
    v29 = muls(v29, k);
    sq3(OUTB + 0x20, v25, 0);
    sq3(OUTB + 0x40, v26, 0);
    sq3(OUTB + 0x50, v27, 0);
    sq3(OUTB + 0x60, v28, 0);
    sq3(OUTB + 0x70, v29, 0);
    sw(OUTB + 0x18, s5);
    sw(OUTB + 0x1c, s6);
    push = add3(muls(muls(push, q), k), v25);
    sq3(OUTB + 0x30, push, w2);
    g_vu = VuContact{v25, v26, v27, v28, v29};
    return 1;
}
// orig: 0x001f0d60 (EE-address form)
int32_t sphere_collide(uint32_t pos, float radius, uint32_t flags, uint32_t ignore) { return sphere_collide_v(V(pos), radius, flags, ignore); }

// Nearest contact of an upright capsule (pos at the bottom, radius, height) with the level
// collision and the mobys: sphere_collide with the triangle test done against the capsule's axis
// segment (the nearest axis point to the triangle's plane, or the plane crossing), the moby
// bounding test against the capsule's z range, the primitives against the nearest axis point (a
// joint segment projected in xy). Output as sphere_collide; the pushed-out position keeps the
// contact's height along the axis.
// orig: 0x001f19a0
int32_t capsule_collide_v(const Vec4& pos, float radius, float height, uint32_t flags, uint32_t ignore) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    L3 hi0 = L3{qadd(p0.x, radius), qadd(p0.y, radius), qadd(p0.z, radius)};
    const L3 lo0 = L3{qsub(p0.x, radius), qsub(p0.y, radius), qsub(p0.z, radius)};
    hi0.z = qadd(hi0.z, height);
    sw(OUTB + 0x10, query);
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f), h = qmul(height, 1024.0f);
    float r2 = qmul(r, r);
    if (neg(qsub(lo0.x, 0.0f)) || neg(qsub(lo0.y, 0.0f)) || neg(qsub(lo0.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi0.x, 1024.0f)) && neg(qsub(hi0.y, 1024.0f)) && neg(qsub(hi0.z, 1024.0f)))) return 0;
    if (static_cast<int32_t>(fbits(height)) <= 0) return 0;
    const int32_t ilo[3] = {qftoi0(lo0.x), qftoi0(lo0.y), qftoi0(lo0.z)};
    const int32_t ihi[3] = {qftoi0(hi0.x), qftoi0(hi0.y), qftoi0(hi0.z)};
    const L3 lo = muls(lo0, 1024.0f), hi = muls(hi0, 1024.0f);
    const float nr2 = qsub(0.0f, r2);
    const float k999 = qmul(1.0f, u2f(0x3f7fdf3bu));
    r2 = qmul(r2, k999);
    const L3 zero{0.0f, 0.0f, 0.0f};
    uint32_t s5 = 0, s6 = 0, w2 = 0;
    float v5y = 0.0f;  // vf5.y: read by the moby bounding test when a height difference is exactly +0
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27, v28 = g_vu.v28, v29 = g_vu.v29;
    uint32_t s0 = SPR, a3 = 0;
    auto tri_scan = [&](uint32_t tri, uint32_t tend, const L3& rel, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t tf = lbu(tri + 3);
            tri += 4;
            a3 = lwu(a1 + 0xc);
            if ((lwu(a0 + 0xc) & a3 & lwu(a2 + 0xc)) != 0) continue;
            w2 = a3;
            if ((flags & 0x80 & tf) != 0) continue;
            const L3 v1 = l3(a0);
            const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
            const L3 bot = sub3(rel, v1);   // vf4
            L3 top = bot;                   // vf5
            v5y = top.y;
            a3 = ((flags >> 8) ^ tf) & 0x1f;
            top.z = qadd(top.z, h);
            if ((flags & 0x20) != 0 && a3 == 0) continue;
            const L3 n = opm(e3, e2);
            const float db = hsum(mul3(bot, n)), dt = hsum(mul3(top, n));
            const float hn = qmul(h, n.z);
            const uint32_t anz = fbits(n.z) & 0x7fffffffu;   // vabs.z
            const float z2 = qsub(bot.z, e2.z), z3 = qsub(bot.z, e3.z);
            if ((flags & 0x10) == 0) {
                a3 = fbits(dt);
                if (neg(u2f(fbits(db) & fbits(dt)))) continue;
            }
            const float tz2 = qsub(top.z, e2.z), tz3 = qsub(top.z, e3.z);
            L3 a13;         // vf13: the axis point
            float q;        // n scale to the plane
            if (static_cast<int32_t>(anz - 0x3f800000u) < 0) {
                // |n.z| < 1: the axis point at v1's height, clamped to the axis
                float z = qsub(0.0f, bot.z);
                z = fmaxv(fminv(z, h), 0.0f);
                a13 = L3{bot.x, bot.y, qadd(z, bot.z)};
                q = qdiv(hsum(mul3(a13, n)), hsum(mul3(n, n)));
            } else {
                const float qc = qdiv(db, hn);
                if (!neg(u2f(fbits(bot.z) | fbits(z2) | fbits(z3)))) {          // bottom above the triangle
                    q = qdiv(db, hsum(mul3(n, n)));
                    a13 = bot;
                } else if (neg(u2f(fbits(top.z) & fbits(tz2) & fbits(tz3)))) {  // top below it
                    q = qdiv(dt, hsum(mul3(n, n)));
                    a13 = top;
                } else {
                    // the axis crosses the plane at x: the nearest triangle point to it, its height on the axis
                    const float cz = qmul(h, qc);
                    L3 x = L3{bot.x, bot.y, qsub(bot.z, cz)};
                    top = x;
                    v5y = top.y;
                    const L3 t9 = sub3(e3, e2), t10 = sub3(x, e2);
                    const float s7 = hsum_y(mul3(opm(x, e2), n)), s8 = hsum_y(mul3(opm(e3, x), n)), s9 = hsum_y(mul3(opm(t10, t9), n));
                    float pz;
                    if (!neg(s7) && !neg(s8) && !neg(s9)) {
                        pz = x.z;
                    } else {
                        L3 ea{}, eb{};
                        if (neg(s7)) { eb = add3(e2, zero); }
                        else if (neg(s8)) { ea = add3(e3, zero); }
                        else { ea = add3(e2, zero); eb = add3(e3, zero); }
                        const L3 dd = sub3(x, ea);
                        eb = sub3(eb, ea);
                        const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                        pz = madds(add3(zero, ea), eb, t).z;
                    }
                    float z = qsub(pz, bot.z);
                    z = fmaxv(fminv(z, h), 0.0f);
                    a13 = L3{bot.x, bot.y, qadd(z, bot.z)};
                    q = qdiv(hsum(mul3(a13, n)), hsum(mul3(n, n)));
                }
            }
            const L3 off = muls(n, q);
            L3 rp = sub3(a13, off);
            float d2 = hsum(mul3(off, off));
            a3 = 0;  // qmfc2 + dsll32 of r2 - d2
            const L3 t9v0 = sub3(e3, e2), t11 = sub3(rp, e2);
            const L3 t7 = opm(rp, e2), t8v = opm(e3, rp), t9v = opm(t11, t9v0);
            if (neg(qsub(r2, d2))) continue;
            const L3 m7 = mul3(t7, n);
            a3 = fbits(m7.x);
            const float s7 = hsum_y(m7), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
            if (neg(s7) || neg(s8) || neg(s9)) {
                L3 ea{}, eb{};
                if (neg(s7)) { eb = add3(e2, zero); }
                else if (neg(s8)) { ea = add3(e3, zero); }
                else { ea = add3(e2, zero); eb = add3(e3, zero); }
                const L3 dd = sub3(rp, ea);
                eb = sub3(eb, ea);
                const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                rp = madds(add3(zero, ea), eb, t);
                const L3 e = sub3(rp, a13);
                d2 = hsum(mul3(e, e));
                if (neg(qsub(r2, d2))) continue;
            }
            s5 = obj;
            const L3 wv1 = add3(v1, org);
            s6 = tf + 0x1000;
            r2 = qmul(d2, k999);
            v26 = add3(n, zero);
            v25 = add3(rp, wv1);
            v27 = add3(zero, wv1);
            v28 = add3(e2, wv1);
            v29 = add3(e3, wv1);
        }
    };
    if ((flags & 1) == 0) {
        // the level: cells of the collision tree whose box reaches the sphere, as a list of
        // (centre, chunk) on the scratchpad; the first chunk is fetched right away
        sd(SPR, 0);
        uint32_t list = SPR + 0x3800;
        bool fetching = false;
        const int32_t c0[3] = {static_cast<int32_t>(static_cast<uint32_t>(ilo[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[2]) >> 2)};
        const int32_t c1[3] = {static_cast<int32_t>(static_cast<uint32_t>(ihi[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[2]) >> 2)};
        const L3 base = L3{static_cast<float>(c0[0] << 12), static_cast<float>(c0[1] << 12), static_cast<float>(c0[2] << 12)};
        L3 corner = base;
        for (int32_t cz = c0[2];; ++cz) {
            corner.y = base.y;
            for (int32_t cy = c0[1];; ++cy) {
                corner.x = base.x;
                for (int32_t cx = c0[0];; ++cx) {
                    const L3 far = L3{qadd(corner.x, 4096.0f), qadd(corner.y, 4096.0f), qadd(corner.z, 4096.0f)};
                    const L3 cl = L3{fminv(fmaxv(corner.x, p.x), far.x), fminv(fmaxv(corner.y, p.y), far.y), fminv(fmaxv(corner.z, p.z), far.z)};
                    const L3 ctr = L3{qadd(corner.x, 2048.0f), qadd(corner.y, 2048.0f), qadd(corner.z, 2048.0f)};
                    const L3 d = sub3(cl, p);
                    const L3 dq = mul3(d, d);
                    const float gap = qmadd(qmadd(qadd(nr2, dq.x), 1.0f, dq.y), 1.0f, dq.z);
                    if (neg(gap)) {
                        const uint32_t chunk = coll_tree_cell(tree, cx, cy, cz);
                        sq3(list, ctr, 0);
                        sw(list + 0xc, chunk);
                        if (chunk != 0) {
                            list += 0x10;
                            if (!fetching) {
                                fetching = true;
                                spr_dma_to(0, (chunk >> 8) + tree, chunk & 0xff);
                            }
                        }
                    }
                    corner.x = qadd(corner.x, 4096.0f);
                    if (cx == c1[0]) break;
                }
                corner.y = qadd(corner.y, 4096.0f);
                if (cy == c1[1]) break;
            }
            corner.z = qadd(corner.z, 4096.0f);
            if (cz == c1[2]) break;
        }
        sw(list + 0xc, 0);
        // the chunks: two buffers (SPR + 0 / + 0x1000), the next one fetched while this one is tested
        uint32_t buf = SPR + 0x1000;
        for (uint32_t e = SPR + 0x3800;; e += 0x10) {
            const uint32_t chunk = lwu(e + 0xc), next = lwu(e + 0x1c);
            const L3 org = l3(e);
            if (chunk == 0) break;
            buf ^= 0x1000;
            if (next != 0) {
                const uint32_t other = buf ^ 0x1000;
                sd(other, 0);
                spr_dma_to(other - SPR, (next >> 8) + tree, next & 0xff);
            }
            const uint32_t nv = lbu(buf + 2);
            const L3 rel = sub3(p, org);
            if (nv == 0) continue;
            const L3 rlo = sub3(lo, org), rhi = sub3(hi, org);
            const int16_t blo[3] = {h16(qftoi0(rlo.x)), h16(qftoi0(rlo.y)), h16(qftoi0(rlo.z))};
            const int16_t bhi[3] = {h16(qftoi0(rhi.x)), h16(qftoi0(rhi.y)), h16(qftoi0(rhi.z))};
            uint32_t src = buf + 4, dst = SPR + 0x1fe0;
            for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
                dst += 0x20;
                for (uint32_t hh = 0; hh < 2; ++hh, src += 4) {
                    const uint32_t pv = lwu(src);
                    const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
                    uint32_t oc = 0;
                    for (uint32_t a = 0; a < 3; ++a) {
                        if ((static_cast<uint16_t>(c[a] - blo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                        if ((static_cast<uint16_t>(bhi[a] - c[a]) & 0x8000u) != 0) oc |= 2u << (a * 8);
                    }
                    const L3 vf = L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])};
                    sq3(dst + hh * 0x10, vf, oc);
                }
                w2 = 0;
            }
            const uint32_t t0 = buf + 4 + nv * 4;
            const uint32_t tend = t0 + static_cast<uint32_t>(lbu(buf)) * 4;
            tri_scan(t0, tend, rel, org, tree);
            // quads: the first lbu(+3) triangles again as (a, c, extra vertex)
            const uint32_t nq = lbu(buf + 3);
            if (nq == 0) continue;
            for (uint32_t i = 0; i < nq; ++i) {
                const uint32_t t = t0 + i * 4;
                const uint32_t c = lbu(t + 2), x = lbu(tend + i);
                sb(t + 1, c);
                sb(t + 2, x);
            }
            tri_scan(t0, t0 + nq * 4, rel, org, tree);
        }
    }
    // the mobys
    s5 = 0;
    {
        const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 14, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 14;
        const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 14, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 14;
        uint32_t cl = SPR + 0x3800;
        for (uint32_t y = y0;; ++y) {
            for (uint32_t x = x0;; ++x) {
                const uint32_t c = GRID + (x << 2) + (y << 8);
                const uint32_t start = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
                sw(cl, start);
                sw(cl + 4, start + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2));
                cl += 8;
                if (x == x1) break;
            }
            if (y == y1) break;
        }
        sd(cl, 0);
    }
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    a3 = GRID;
    for (uint32_t cell = SPR + 0x3800;; cell += 8) {
        uint32_t id = lwu(cell);
        if (id == 0) break;
        const uint32_t cend = lwu(cell + 4);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            // bounding sphere against the capsule: xy distance, then the z range
            const L3 c = l3(m);
            const float sr = qadd(r, lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            const float below = qsub(qsub(qsub(c.z, sr), p.z), h);   // vf4.z: sphere bottom - capsule top
            const float above = qsub(qadd(c.z, sr), p.z);             // vf5.z: sphere top - capsule bottom
            const float dx = qsub(c.x, p.x), dy = qsub(c.y, p.y);
            const float ov = qmadd(qsub(qmul(dx, dx), qmul(sr, sr)), 1.0f, qmul(dy, dy));
            if (lwu(m + 0x9c) == query) continue;
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const int64_t up = static_cast<int64_t>((static_cast<uint64_t>(fbits(above)) << 32) | fbits(v5y));
            if (up <= 0) continue;
            if (!neg(below)) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t vbytes = lwu(col + 0xc);
            sw(m + 0x9c, query);
            if (nj != 0) {
                if (((flags & 2) == 0 ? 1u : 0u) == vbytes) continue;
                const uint32_t key = lwu(m + 0xa8);
                const uint32_t ring = lwu(OUTB + 8);
                const uint32_t bufs = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = bufs + i * 0x800;
                        break;
                    }
                }
                if (!cached) {
                    const uint32_t slot = lwu(OUTB + 0xc);
                    sw(OUTB + 0xc, (slot + 1) & 7);
                    sw(ring + slot * 8, key);
                    sw(ring + slot * 8 + 4, a3);
                    a3 = moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, cell);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            if (vbytes != 0) {
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                v5y = S2.y;
                const L3 rel = sub3(p, mp);
                L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                vhi.z = qadd(vhi.z, h);
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                int32_t k = 0;
                for (int32_t left = static_cast<int32_t>(vbytes); left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
                    for (uint32_t hh = 0; hh < 2; ++hh) {
                        const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + hh * 8;
                        const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                        const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        const L3 bl = sub3(w, vlo), ab = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(bl.x)) oc |= 1u;
                        if (neg(ab.x)) oc |= 2u;
                        if (neg(bl.y)) oc |= 1u << 8;
                        if (neg(ab.y)) oc |= 2u << 8;
                        if (neg(bl.z)) oc |= 1u << 16;
                        if (neg(ab.z)) oc |= 2u << 16;
                        sq3(dst + hh * 0x10, w, oc);
                        if (hh == 0) a3 = neg(ab.x) ? 2u : 0u;
                    }
                }
                w2 = static_cast<uint32_t>(static_cast<int32_t>(lh(vbase + static_cast<uint32_t>(k) * 0x10 + 0xe)));
                const uint32_t t0 = vbase + vbytes;
                tri_scan(t0, t0 + lwu(col + 8), rel, mp, m);
            }
            // primitives, nearest kept: against the nearest point of the capsule axis
            if ((flags & 2) != 0 || lwu(col + 4) == 0) continue;
            auto xf = [&](const L3& v) { return madds(madds(madds(muls(R0, v.x), R1, v.y), R2, v.z), mp, 1.0f); };
            const int32_t want = (flags & 4) == 0 ? 2 : 1;
            uint32_t mask = lwu(m + 0x98);
            int32_t more = 0;
            for (uint32_t rec = col + 0x10; more >= 0; rec += 0x20) {
                const uint32_t off = mask & 1;
                mask >>= 1;
                more = lh(rec + 2);
                const int32_t type = static_cast<int8_t>(lbu(rec));
                if (off != 0 || (want & more) == 0) continue;
                L3 pc;
                float pw;
                if (type == 1) {
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                } else if (type == 2) {
                    w2 = lwu(rec + 0xc);
                    const uint32_t j = s0 + (lwu(rec + 4) << 4);
                    pc = xf(muls(add3(l3(j), l3(rec + 0x10)), sc));
                    pw = qmul(lwc1(rec + 0xc), sc);
                } else if (type == 3) {
                    w2 = lwu(rec + 0xc);
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                    const float top = qadd(pc.z, qmul(lwc1(rec + 4), sc));
                    pc.z = fminv(top, fmaxv(pc.z, p.z));
                } else {
                    // joint segment: nearest point to the axis in xy (a short one: its z range clamped)
                    const uint32_t jb = s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4);
                    w2 = lwu(jb + 0xc);
                    const L3 a = xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
                    const L3 ab = sub3(xf(muls(l3(jb), sc)), a);
                    const L3 ap = sub3(p, a);
                    const float num = qadd(qmul(ap.x, ab.x), qmul(ap.y, ab.y));
                    const float den = qadd(qmul(ab.x, ab.x), qmul(ab.y, ab.y));
                    const float q = qdiv(num, den);
                    a3 = 0;  // qmfc2 + dsll32 of 1 - |den|
                    if (!neg(qsub(1.0f, u2f(fbits(den) & 0x7fffffffu)))) {
                        const L3 b = add3(ab, a);
                        pc = a;
                        pc.z = fminv(b.z, fmaxv(a.z, p.z));
                    } else {
                        const float t = clamp01(qadd(0.0f, q));
                        pc = madds(add3(zero, a), ab, t);
                    }
                    pw = qmul(lwc1(rec + 0xc), sc);
                }
                // sphere (pc, pw) against the axis point at its height
                float az = qsub(pc.z, p.z);
                az = fmaxv(fminv(az, h), 0.0f);
                const L3 axis = L3{p.x, p.y, qadd(p.z, az)};
                float psr = qadd(r, pw);
                const L3 dv = sub3(pc, axis);
                const float d2 = hsum(mul3(dv, dv));
                psr = qmul(psr, psr);
                const float q = qsqrt(d2);
                if (neg(qsub(psr, d2))) continue;
                float dist = qadd(0.0f, q);
                const float inv = qadd(0.0f, qdiv(1.0f, dist));
                dist = qmul(dist, 1.0f);
                const float reach = fminv(dist, pw);
                L3 push = sub3(axis, pc);
                v5y = push.y;
                push = muls(push, inv);
                v5y = push.y;
                push = muls(push, reach);
                v5y = push.y;
                pc = add3(pc, push);
                const L3 e = sub3(pc, axis);
                const float d2b = hsum(mul3(e, e));
                if (neg(qsub(r2, d2b))) continue;
                s5 = m;
                s6 = 0u - rec;
                r2 = qmul(d2b, 1.0f);
                v25 = add3(zero, pc);
                v26 = add3(zero, push);
            }
        }
    }
    if (s6 == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, v25);
    L3 back = sub3(v25, p);
    const float k = u2f(0x3a800000u);
    v25 = muls(v25, k);
    v27 = muls(v27, k);
    v28 = muls(v28, k);
    v29 = muls(v29, k);
    sq3(OUTB + 0x20, v25, 0);
    sq3(OUTB + 0x40, v26, 0);
    sq3(OUTB + 0x50, v27, 0);
    sq3(OUTB + 0x60, v28, 0);
    sq3(OUTB + 0x70, v29, 0);
    sw(OUTB + 0x18, s5);
    sw(OUTB + 0x1c, s6);
    back.z = fmaxv(fminv(back.z, h), 0.0f);
    push.z = qadd(push.z, back.z);
    push = muls(push, q);
    push.z = qsub(push.z, back.z);
    push = add3(muls(push, k), v25);
    sq3(OUTB + 0x30, push, w2);
    g_vu = VuContact{v25, v26, v27, v28, v29};
    return 1;
}
// orig: 0x001f19a0 (EE-address form)
int32_t capsule_collide(uint32_t pos, float radius, float height, uint32_t flags, uint32_t ignore) {
    return capsule_collide_v(V(pos), radius, height, flags, ignore);
}

namespace {
// ((2026-11-01, the Wii U's game step) RAC1_COLL_MESH_CACHE, the Wii U's default 1, the PC's 0) a moby's collision mesh turned into
// the world by segment_collide_v (each vertex: the rows scaled, three VU0 multiply-adds) is a function of the rows', the scale's
// bits and the vertex words alone: the result kept per moby with those bits and a copy of the words, and used again while all of
// them are the same (most mobys do not move; floor probes and sound lines test the same ones every frame). The outcodes, which
// depend on the segment, are still made per call.
struct MeshCacheEntry {
    uint32_t m = 0, vbase = 0, nb = 0;
    uint32_t key[10] = {};
    std::vector<uint32_t> raw;
    std::vector<L3> w;
};
struct MeshCache {
    static constexpr uint32_t kN = 128;
    MeshCacheEntry e[kN];
};
bool mesh_cache_on() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_COLL_MESH_CACHE");
#if defined(__WIIU__) && !defined(RAC1_VU0_EXACT)
        return e == nullptr || std::atoi(e) != 0;
#else
        return e != nullptr && std::atoi(e) != 0;
#endif
    }();
    return on;
}
MeshCache& mesh_cache() {
#if defined(__WIIU__)
    static platform::PerThread<MeshCache, 4> c;
    return c.get();
#else
    static thread_local MeshCache c{};
    return c;
#endif
}
}  // namespace

// Nearest hit of the segment start -> end with the level collision and the mobys (hand-written VU0
// macro code). The 4-unit cells the segment passes are listed in order (per-axis crossings at
// SPR + 0x3000/0x3200/0x3400 merged into (t, packed cell) at SPR + 0x3800, ending with t = 1);
// unless flags & 1 their chunks are fetched (double-buffered) and their triangles hit where the
// segment crosses them (front faces unless flags & 0x10), nearest t kept in vf24. Then the mobys
// of the grid cells along the segment (only those flagged 0x4000 when flags & 9): bounding sphere
// near the segment, triangles as above, primitives (spheres, joint spheres, vertical cylinders,
// joint capsules: the entry point). Writes the hit to 0x173e40 as sphere_collide (no pushed-out
// position) and returns 1; a moby hit flagged 0x4000 also gets a contact record (with `rec`).
// orig: 0x001efc70
int32_t segment_collide_v(const Vec4& start, const Vec4& end, uint32_t flags, uint32_t ignore, const uint32_t* rec) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    const L3 s0f = L3{start.x, start.y, start.z}, e0f = L3{end.x, end.y, end.z};
    const L3 mn = L3{fminv(s0f.x, e0f.x), fminv(s0f.y, e0f.y), fminv(s0f.z, e0f.z)};
    const L3 mx = L3{fmaxv(s0f.x, e0f.x), fmaxv(s0f.y, e0f.y), fmaxv(s0f.z, e0f.z)};
    const L3 ps = muls(s0f, 1024.0f), pe = muls(e0f, 1024.0f);   // vf30 / vf31
    // vf28 / vf29 are the contact registers (g_vu) the other routines store from: this one loads the end points into them
    // (lqc2 at 0x1efc74 / 0x1efc7c) and converts them in place (vftoi0 at 0x1efd08 / 0x1efd20), before its first exits, so
    // every return leaves them so (a later contact that sets only vf25 / vf26 stores these, not an older contact's)
    const int32_t is[3] = {qftoi0(ps.x), qftoi0(ps.y), qftoi0(ps.z)};
    const int32_t ie[3] = {qftoi0(pe.x), qftoi0(pe.y), qftoi0(pe.z)};
    const L3 is_f = L3{u2f(static_cast<uint32_t>(is[0])), u2f(static_cast<uint32_t>(is[1])), u2f(static_cast<uint32_t>(is[2]))};
    const L3 ie_f = L3{u2f(static_cast<uint32_t>(ie[0])), u2f(static_cast<uint32_t>(ie[1])), u2f(static_cast<uint32_t>(ie[2]))};
    g_vu.v28 = s0f;
    g_vu.v29 = e0f;
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    sw(OUTB + 0x10, query);
    g_vu.v28 = is_f;
    if (neg(qsub(mn.x, 0.0f)) || neg(qsub(mn.y, 0.0f)) || neg(qsub(mn.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    g_vu.v29 = ie_f;
    if (!(neg(qsub(mx.x, 1024.0f)) && neg(qsub(mx.y, 1024.0f)) && neg(qsub(mx.z, 1024.0f)))) return 0;
    const L3 seg = sub3(pe, ps);   // vf13
    const L3 zero{0.0f, 0.0f, 0.0f};
    uint32_t s5 = 0, s6 = 0;
    // vf25..vf27 from the previous contact; vf28 / vf29 still hold the end points as integers
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27;
    L3 v28 = is_f;
    L3 v29 = ie_f;
    float best = 1.0f;   // vf24.x
    const uint32_t c0[3] = {static_cast<uint32_t>(is[0]) >> 12, static_cast<uint32_t>(is[1]) >> 12, static_cast<uint32_t>(is[2]) >> 12};
    const uint32_t c1[3] = {static_cast<uint32_t>(ie[0]) >> 12, static_cast<uint32_t>(ie[1]) >> 12, static_cast<uint32_t>(ie[2]) >> 12};
    uint32_t cellw = (c0[0] & 0xff) | (c0[1] & 0xff) << 8 | (c0[2] & 0xff) << 16;
    sw(SPR + 0x3800, 0);
    sw(SPR + 0x3804, cellw);
    if ((flags & 1) == 0) {
        const uint32_t chunk = coll_tree_cell(tree, static_cast<int32_t>(c0[0] & 0xffff), static_cast<int32_t>(c0[1]), static_cast<int32_t>(c0[2]));
        sd(SPR, 0);
        if (chunk != 0) spr_dma_to(0, (chunk >> 8) + tree, chunk & 0xff);
    }
    uint32_t list = SPR + 0x3800;
    if (c0[0] != c1[0] || c0[1] != c1[1] || c0[2] != c1[2]) {
        // crossings per axis: the t of each cell boundary passed, clamped to [0, 1]
        int32_t total = 0;
        const float sv[3] = {seg.x, seg.y, seg.z}, st[3] = {ps.x, ps.y, ps.z};
        for (uint32_t ax = 0; ax < 3; ++ax) {
            const float q = qdiv(1.0f, sv[ax]);
            int32_t a = static_cast<int32_t>(c0[ax]), b = static_cast<int32_t>(c1[ax]), d = 1;
            if (b - a < 0) {
                ++a;
                ++b;
                d = -1;
            }
            uint32_t out = SPR + 0x3000 + ax * 0x200;
            if (a != b) {
                for (a += d;; a += d) {
                    float t = qsub(static_cast<float>(a << 12), st[ax]);
                    t = qmul(t, q);
                    ++total;
                    int32_t tb = static_cast<int32_t>(fbits(t));
                    tb = std::max(std::min(tb, 0x3f800000), 0);
                    sw(out, static_cast<uint32_t>(tb));
                    out += 4;
                    if (a == b) break;
                }
            }
            sw(out, 0x50000000u);
            sb(SPR + 0x3ffc + ax, static_cast<uint32_t>(d));
        }
        // merged, nearest first: each step moves the packed cell along its axis
        const uint32_t dx = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffc))));
        const uint32_t dy = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffd)))) << 8;
        const uint32_t dz = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffe)))) << 16;
        uint32_t px = SPR + 0x3000, py = SPR + 0x3200, pz = SPR + 0x3400;
        for (; total > 0; --total) {
            const int32_t tx = lw(px), ty = lw(py), tz = lw(pz);
            list += 8;
            if (ty - tx < 0) {
                if (tz - ty < 0) {
                    sw(list, static_cast<uint32_t>(tz));
                    cellw += dz;
                    pz += 4;
                } else {
                    sw(list, static_cast<uint32_t>(ty));
                    cellw += dy;
                    py += 4;
                }
            } else if (tz - tx < 0) {
                sw(list, static_cast<uint32_t>(tz));
                cellw += dz;
                pz += 4;
            } else {
                sw(list, static_cast<uint32_t>(tx));
                cellw += dx;
                px += 4;
            }
            sw(list + 4, cellw);
        }
    } else {
        // one cell: nothing to do when the end points coincide
        const int32_t d0 = is[0] - ie[0], d1 = is[1] - ie[1], d2 = is[2] - ie[2];
        if ((static_cast<uint64_t>(static_cast<uint32_t>(d0 | d1)) | (static_cast<uint64_t>(static_cast<uint32_t>(d1 | d2)) << 32)) == 0) return 0;
    }
    sd(list + 8, 0x3f800000u);
    // triangles [tri, tend) over SPR + 0x2000 (relative to org; the segment relative: a, b)
    auto tri_scan = [&](uint32_t tri, uint32_t tend, const L3& a, const L3& b, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t tf = lbu(tri + 3);
            tri += 4;
            if ((lwu(a0 + 0xc) & lwu(a1 + 0xc) & lwu(a2 + 0xc)) != 0) continue;
            if ((flags & 0x80 & tf) != 0) continue;
            // (the vertices clamped once as they are read; seg, a, b, org, best are VU results: every operand below clean)
            const L3 v1 = cl3(a0);
            const L3 e2 = csub3(cl3(a1), v1), e3 = csub3(cl3(a2), v1);
            if ((flags & 0x20) != 0 && (((flags >> 8) ^ tf) & 0x1f) == 0) continue;
            const L3 n = copm(e3, e2);
            const float sn = chsum(cmul3(seg, n));
            const float ds = chsum(cmul3(csub3(v1, a), n)), de = chsum(cmul3(csub3(v1, b), n));
            const float q = qdiv(ds, sn);
            if ((flags & 0x10) == 0 && (!neg(ds) || static_cast<int32_t>(fbits(de)) <= 0)) continue;
            if (!neg(u2f(fbits(ds) ^ fbits(de)))) continue;
            const L3 hp = cmadds(cmuls(csub3(a, v1), 1.0f), seg, q);
            const L3 wv1 = cadd3(v1, org);
            const float s7 = chsum(cmul3(copm(hp, e2), n)), s8 = chsum(cmul3(copm(e3, hp), n));
            const float s9 = chsum(cmul3(copm(csub3(hp, e2), csub3(e3, e2)), n));
            const float gain = csub(best, q);
            if (neg(s7) || neg(s8) || neg(s9) || static_cast<int32_t>(fbits(gain)) <= 0) continue;
            s6 = tf + 0x1000;
            s5 = obj;
            best = qadd(0.0f, q);
            v25 = add3(hp, wv1);
            v26 = add3(n, zero);
            v27 = add3(zero, wv1);
            v28 = add3(e2, wv1);
            v29 = add3(e3, wv1);
        }
    };
    if ((flags & 1) == 0) {
        uint32_t buf = SPR + 0x1000;
        for (uint32_t e = SPR + 0x3800;; e += 8) {
            const uint32_t t = lwu(e), tnext = lwu(e + 8), cw = lwu(e + 4);
            if (t == 0x3f800000u) break;
            if (s6 != 0 && !(static_cast<int64_t>(static_cast<uint64_t>(fbits(best)) << 32) - static_cast<int64_t>(static_cast<uint64_t>(t) << 32) > 0)) break;
            const uint32_t ncw = lwu(e + 0xc);
            buf ^= 0x1000;
            if (tnext != 0x3f800000u) {
                const uint32_t chunk = coll_tree_cell(tree, static_cast<int32_t>(ncw & 0xff), static_cast<int32_t>((ncw >> 8) & 0xff), static_cast<int32_t>(ncw >> 16));
                const uint32_t other = buf ^ 0x1000;
                sd(other, 0);
                if (chunk != 0) spr_dma_to(other - SPR, (chunk >> 8) + tree, chunk & 0xff);
            }
            const uint32_t nv = lbu(buf + 2);
            const L3 pin = madds(muls(seg, u2f(t)), ps, 1.0f), pout = madds(muls(seg, u2f(tnext)), ps, 1.0f);
            if (nv == 0) continue;
            const int32_t ci[3] = {static_cast<int32_t>(((cw & 0xff) << 12) + 0x800), static_cast<int32_t>((((cw >> 8) & 0xff) << 12) + 0x800), static_cast<int32_t>((((cw >> 16) & 0xff) << 12) + 0x800)};
            const L3 org = L3{static_cast<float>(ci[0]), static_cast<float>(ci[1]), static_cast<float>(ci[2])};
            const int32_t ri[3] = {qftoi0(pin.x) - ci[0], qftoi0(pin.y) - ci[1], qftoi0(pin.z) - ci[2]};
            const int32_t ro[3] = {qftoi0(pout.x) - ci[0], qftoi0(pout.y) - ci[1], qftoi0(pout.z) - ci[2]};
            int16_t blo[3], bhi[3];
            for (uint32_t k = 0; k < 3; ++k) {
                blo[k] = h16(std::min(ri[k], ro[k]) - 1);
                bhi[k] = h16(std::max(ri[k], ro[k]) + 1);
            }
            const L3 a = sub3(ps, org), b = sub3(pe, org);
            uint32_t src = buf + 4, dst = SPR + 0x1fe0;
            for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
                dst += 0x20;
                for (uint32_t hh = 0; hh < 2; ++hh, src += 4) {
                    const uint32_t pv = lwu(src);
                    const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
                    uint32_t oc = 0;
                    for (uint32_t k = 0; k < 3; ++k) {
                        if ((static_cast<uint16_t>(c[k] - blo[k]) & 0x8000u) != 0) oc |= 1u << (k * 8);
                        if ((static_cast<uint16_t>(bhi[k] - c[k]) & 0x8000u) != 0) oc |= 2u << (k * 8);
                    }
                    const L3 vf = L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])};
                    sq3(dst + hh * 0x10, vf, oc);
                }
            }
            const uint32_t t0 = buf + 4 + nv * 4;
            const uint32_t tend = t0 + static_cast<uint32_t>(lbu(buf)) * 4;
            tri_scan(t0, tend, a, b, org, tree);
            const uint32_t nq = lbu(buf + 3);
            if (nq == 0) continue;
            for (uint32_t i = 0; i < nq; ++i) {
                const uint32_t tt = t0 + i * 4;
                const uint32_t c = lbu(tt + 2), x = lbu(tend + i);
                sb(tt + 1, c);
                sb(tt + 2, x);
            }
            tri_scan(t0, t0 + nq * 4, a, b, org, tree);
        }
    }
    // the mobys: grid cells (16 units) along the segment, from the cells list
    s5 = 0;
    const float len2 = hsum(mul3(seg, seg));
    const float inv2 = qdiv(1.0f, len2);
    {
        auto gcell = [](uint32_t cw) {
            const uint32_t x = (cw & 0xff) >> 2, y = ((cw >> 8) & 0xff) >> 2;
            return x | y << 16;
        };
        uint32_t cur = gcell(lhu(SPR + 0x3804));
        sw(SPR + 0x3800, cur);
        uint32_t out = SPR + 0x3804;
        for (uint32_t e = SPR + 0x3808; lwu(e) != 0x3f800000u; e += 8) {
            const uint32_t g = gcell(lhu(e + 4));
            if (g == cur) continue;
            sw(out, g);
            cur = g;
            out += 4;
        }
        sw(out, 0xffffffffu);
    }
    const L3 sdir = muls(seg, inv2);   // vf21
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    uint32_t s0 = SPR;
    for (uint32_t ce = SPR + 0x3800;; ce += 4) {
        const uint32_t gv = lwu(ce);
        if (static_cast<int32_t>(gv) < 0) break;
        const uint32_t c = GRID + (gv >> 8) + ((gv & 0xffff) << 2);
        uint32_t id = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
        const uint32_t cend = id + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2);
        sw(ce, cend);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            const L3 cm = l3(m);
            const float rr = qmul(lwc1(m + 0xc), lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            if ((flags & 9) != 0 && (lhu(m + 0x34) & 0x4000) == 0) continue;
            // nearest point of the segment (up to the best hit) to the bounding sphere
            float t = hsum(mul3(sub3(cm, ps), sdir));
            t = fminv(fmaxv(t, 0.0f), best);
            const L3 np = sub3(madds(add3(zero, ps), seg, t), cm);
            const L3 nq = mul3(np, np);
            const float ov = qmadd(qmadd(qsub(nq.x, rr), 1.0f, nq.y), 1.0f, nq.z);
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const uint32_t prev = lwu(m + 0x9c);
            sw(m + 0x9c, query);
            if (prev == query) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t vbytes = lwu(col + 0xc);
            if (nj != 0) {
                if (((flags & 2) == 0 ? 1u : 0u) == vbytes) continue;
                const uint32_t key = lwu(m + 0xa8);
                const uint32_t ring = lwu(OUTB + 8);
                const uint32_t bufs = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = bufs + i * 0x800;
                        break;
                    }
                }
                if (!cached) {
                    const uint32_t slot = lwu(OUTB + 0xc);
                    sw(OUTB + 0xc, (slot + 1) & 7);
                    sw(ring + slot * 8, key);
                    sw(ring + slot * 8 + 4, query);
                    moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, ce);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            if (vbytes != 0) {
                const L3 a = sub3(ps, mp), b = sub3(pe, mp);
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                const L3 vlo = L3{fminv(a.x, b.x), fminv(a.y, b.y), fminv(a.z, b.z)};
                const L3 vhi = L3{fmaxv(a.x, b.x), fmaxv(a.y, b.y), fmaxv(a.z, b.z)};
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                int32_t k = 0;
                const uint32_t nb = (vbytes + 0xf) & ~0xfu;   // the bytes the loop below reads (vbytes as a count > 0)
                const MeshCacheEntry* ce = nullptr;
                if (mesh_cache_on() && static_cast<int32_t>(vbytes) > 0 && nb <= 0x1000 && (vbase & 3) == 0) {
                    const uint32_t key[10] = {fbits(R0.x), fbits(R0.y), fbits(R0.z), fbits(R1.x), fbits(R1.y), fbits(R1.z),
                                              fbits(R2.x), fbits(R2.y), fbits(R2.z), fbits(sc)};
                    const void* rp = ee_ram_ptr_read(vbase);
                    MeshCacheEntry& e = mesh_cache().e[(m >> 8) & (MeshCache::kN - 1)];
                    static const bool stats = std::getenv("RAC1_COLL_MESH_STATS") != nullptr;
                    static uint32_t n_call = 0, n_miss = 0;
                    if (stats && ++n_call % 20000 == 0) std::fprintf(stderr, "[meshcache] %u uses, %u made\n", n_call, n_miss);
                    if (!(e.m == m && e.vbase == vbase && e.nb == nb && std::memcmp(e.key, key, sizeof key) == 0 &&
                          std::memcmp(e.raw.data(), rp, nb) == 0)) {
                        ++n_miss;
                        e.m = m;
                        e.vbase = vbase;
                        e.nb = nb;
                        std::memcpy(e.key, key, sizeof key);
                        e.raw.resize(nb / 4);
                        std::memcpy(e.raw.data(), rp, nb);
                        e.w.resize(nb / 8);
                        for (uint32_t i = 0; i < nb / 8; ++i) {
                            const uint32_t va = vbase + i * 8;
                            const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                            e.w[i] = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        }
                    }
                    ce = &e;
                }
                if (ce != nullptr) {
                    for (uint32_t i = 0; i < nb / 8; ++i) {
                        if ((i & 1) == 0) dst += 0x20;
                        const L3& w = ce->w[i];
                        const L3 bl = sub3(w, vlo), ab = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(bl.x)) oc |= 1u;
                        if (neg(ab.x)) oc |= 2u;
                        if (neg(bl.y)) oc |= 1u << 8;
                        if (neg(ab.y)) oc |= 2u << 8;
                        if (neg(bl.z)) oc |= 1u << 16;
                        if (neg(ab.z)) oc |= 2u << 16;
                        sq3(dst + (i & 1) * 0x10, w, oc);
                    }
                } else
                for (int32_t left = static_cast<int32_t>(vbytes); left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
                    for (uint32_t hh = 0; hh < 2; ++hh) {
                        const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + hh * 8;
                        const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                        const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        const L3 bl = sub3(w, vlo), ab = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(bl.x)) oc |= 1u;
                        if (neg(ab.x)) oc |= 2u;
                        if (neg(bl.y)) oc |= 1u << 8;
                        if (neg(ab.y)) oc |= 2u << 8;
                        if (neg(bl.z)) oc |= 1u << 16;
                        if (neg(ab.z)) oc |= 2u << 16;
                        sq3(dst + hh * 0x10, w, oc);
                    }
                }
                const uint32_t t0 = vbase + vbytes;
                tri_scan(t0, t0 + lwu(col + 8), a, b, mp, m);
            }
            if ((flags & 2) != 0 || lwu(col + 4) == 0) continue;
            auto rot = [&](const L3& v) { return madds(madds(muls(R0, v.x), R1, v.y), R2, v.z); };
            const int32_t want = (flags & 4) == 0 ? 2 : 1;
            uint32_t mask = lwu(m + 0x98);
            int32_t more = 0;
            for (uint32_t pr = col + 0x10; more >= 0; pr += 0x20) {
                const uint32_t off = mask & 1;
                mask >>= 1;
                more = lh(pr + 2);
                const int32_t type = static_cast<int8_t>(lbu(pr));
                if (off != 0 || (want & more) == 0) continue;
                L3 pc;
                float pw;
                if (type == 1) {
                    pc = add3(rot(muls(l3(pr + 0x10), sc)), mp);
                    pw = qmul(lwc1(pr + 0x1c), sc);
                } else if (type == 2) {
                    const uint32_t j = s0 + (lwu(pr + 4) << 4);
                    pc = add3(rot(muls(add3(l3(j), l3(pr + 0x10)), sc)), mp);
                    pw = qmul(lwc1(pr + 0xc), sc);
                } else if (type == 3) {
                    // vertical cylinder: the segment's entry (in xy) clamps the centre's height
                    pc = add3(rot(muls(l3(pr + 0x10), sc)), mp);
                    pw = qmul(lwc1(pr + 0x1c), sc);
                    const L3 d = seg, cs = sub3(pc, ps);
                    float a2 = qadd(qmul(d.x, d.x), qmul(d.y, d.y));
                    float b2 = qadd(qmul(cs.x, d.x), qmul(cs.y, d.y));
                    float c2 = qadd(qmul(cs.x, cs.x), qmul(cs.y, cs.y));
                    a2 = fmaxv(a2, u2f(0x3c800000u));
                    const float rr2 = qmul(pw, pw);
                    float tt = clamp01(qadd(0.0f, qdiv(b2, a2)));
                    const float nx = qsub(qmadd(qadd(0.0f, ps.x), d.x, tt), pc.x), ny = qsub(qmadd(qadd(0.0f, ps.y), d.y, tt), pc.y);
                    const float ovc = qmadd(qsub(qmul(nx, nx), rr2), 1.0f, qmul(ny, ny));
                    if (static_cast<int32_t>(fbits(ovc)) > 0) continue;
                    b2 = qadd(b2, b2);
                    c2 = qsub(c2, rr2);
                    float disc = qsub(qmul(b2, b2), qmul(qmul(4.0f, a2), c2));
                    disc = fmaxv(disc, 0.0f);
                    const float sq = qsqrt(disc);
                    b2 = qsub(0.0f, b2);
                    a2 = qadd(a2, a2);
                    const float h = qmul(lwc1(pr + 4), sc);
                    b2 = qadd(b2, sq);
                    tt = clamp01(qsub(0.0f, qdiv(b2, a2)));
                    const L3 ent = madds(add3(zero, ps), d, tt);
                    const float top = qadd(pc.z, h);
                    pc.z = fminv(fmaxv(pc.z, ent.z), top);
                } else {
                    // capsule between two joints: its axis point nearest to the segment's entry
                    const L3 a = rot(muls(l3(s0 + (static_cast<uint32_t>(lh(pr + 4)) << 4)), sc));
                    L3 ax = sub3(rot(muls(l3(s0 + (static_cast<uint32_t>(lh(pr + 6)) << 4)), sc)), a);
                    pw = qmul(lwc1(pr + 0xc), sc);
                    const float rr2 = qmul(qadd(0.0f, pw), qadd(0.0f, pw));
                    const L3 wa = add3(a, mp);
                    const L3 cr = opm(ax, seg), crn = opm(seg, ax);
                    const L3 c2 = mul3(cr, cr);
                    const L3 m7 = opm(seg, cr), m8 = opm(ax, crn);
                    L3 ent = ps;
                    const L3 par = sub3(c2, L3{1.0f, 1.0f, 1.0f});
                    if (!(neg(par.x) && neg(par.y) && neg(par.z))) {
                        const float cc = hsum(c2);
                        const float k7 = hsum(mul3(m7, sub3(ps, wa))), k8 = hsum(mul3(m8, sub3(wa, ps)));
                        const L3 on_ax = madds(add3(zero, wa), ax, qdiv(k7, cc));
                        const L3 on_seg = madds(add3(zero, ps), seg, qdiv(k8, cc));
                        const float al = hsum(mul3(ax, ax)), sl = hsum(mul3(seg, seg));
                        const L3 dd = sub3(on_seg, on_ax);
                        const L3 dq = mul3(dd, dd);
                        const float gap = qmadd(qmadd(qadd(qsub(0.0f, rr2), dq.x), 1.0f, dq.y), 1.0f, dq.z);
                        if (!neg(gap)) continue;
                        const L3 ah = muls(ax, qrsqrt(1.0f, al));
                        L3 sh = muls(seg, qrsqrt(1.0f, sl));
                        float cs = hsum(mul3(ah, sh));
                        cs = qmul(cs, cs);
                        cs = qsub(1.0f, cs);
                        float back = qadd(0.0f, qdiv(qsub(0.0f, gap), cs));
                        back = fmaxv(back, 0.0f);
                        sh = muls(sh, qsqrt(back));
                        ent = sub3(on_seg, sh);
                        if (neg(hsum(mul3(sub3(ent, ps), seg)))) ent = ps;
                    }
                    const L3 rel = sub3(ent, wa);
                    const float tt = clamp01(qadd(0.0f, qdiv(hsum(mul3(rel, ax)), hsum(mul3(ax, ax)))));
                    ax = muls(ax, tt);
                    pc = add3(wa, ax);
                }
                // sphere (pc, pw) against the segment: the entry t
                const L3 d = seg, cs = sub3(pc, ps);
                float a2 = hsum(mul3(d, d)), b2 = hsum(mul3(cs, d)), c2 = hsum(mul3(cs, cs));
                const float rr2 = qmul(pw, pw);
                float tt = clamp01(qadd(0.0f, qdiv(b2, a2)));
                const L3 nn = sub3(madds(add3(zero, ps), d, tt), pc);
                const L3 nq2 = mul3(nn, nn);
                const float ovs = qmadd(qmadd(qsub(nq2.x, rr2), 1.0f, nq2.y), 1.0f, nq2.z);
                if (static_cast<int32_t>(fbits(ovs)) > 0) continue;
                b2 = qadd(b2, b2);
                c2 = qsub(c2, rr2);
                float disc = qsub(qmul(b2, b2), qmul(qmul(4.0f, a2), c2));
                disc = fmaxv(disc, 0.0f);
                const float sq = qsqrt(disc);
                b2 = qsub(0.0f, b2);
                a2 = qadd(a2, a2);
                b2 = qadd(b2, sq);
                tt = clamp01(qsub(0.0f, qdiv(b2, a2)));
                const L3 hit = madds(add3(zero, ps), d, tt);
                const L3 nrm = sub3(hit, pc);
                if (static_cast<int32_t>(fbits(qsub(best, tt))) <= 0) continue;
                s5 = m;
                s6 = 0u - pr;
                best = qadd(0.0f, tt);
                v25 = add3(zero, hit);
                v26 = add3(zero, nrm);
            }
        }
    }
    if (s6 == 0) {
        g_vu = VuContact{v25, v26, v27, v28, v29};  // (no contact: vf25..vf29 as the scan left them)
        return 0;
    }
    const float k = u2f(0x3a800000u);
    v25 = muls(v25, k);
    v27 = muls(v27, k);
    v28 = muls(v28, k);
    v29 = muls(v29, k);
    sq3(OUTB + 0x20, v25, 0);
    sq3(OUTB + 0x40, v26, 0);
    sq3(OUTB + 0x50, v27, 0);
    sq3(OUTB + 0x60, v28, 0);
    sq3(OUTB + 0x70, v29, 0);
    sw(OUTB + 0x18, s5);
    sw(OUTB + 0x1c, s6);
    g_vu = VuContact{v25, v26, v27, v28, v29};
    if (s5 == 0 || rec == nullptr || (lhu(s5 + 0x34) & 0x4000) == 0) return 1;
    const uint32_t next = lwu(OUTB + 0x14);
    const int32_t old = static_cast<int8_t>(lbu(s5 + 0xa4));
    if (old >= 0) {
        const uint32_t o = OV(0x00178100) + static_cast<uint32_t>(old) * 64;
        if (lwu(o + 0x34) == s5 && static_cast<int32_t>(lwu(o + 0x2c) - rec[7]) > 0) return 1;
    }
    sw(OUTB + 0x14, (next + 1) & 0x3f);
    sb(s5 + 0xa4, next);
    const uint32_t c = OV(0x00178100) + next * 64;
    uint32_t rw[9];
    for (uint32_t w = 0; w < 9; ++w) rw[w] = rec[w];
    sq3(c, v25, 0);
    for (uint32_t w = 0; w < 9; ++w) sw(c + 0x10 + w * 4, rw[w]);
    sw(c + 0x34, s5);
    const uint32_t col = lwu(s5 + 0x94);
    const int32_t at = static_cast<int32_t>(s6) >= 0 ? -0x20 : static_cast<int32_t>((0u - s6) - col);
    sw(c + 0x38, static_cast<uint32_t>(at >> 5));
    return 1;
}
// orig: 0x001efc70 (EE-address form)
int32_t segment_collide(uint32_t start, uint32_t end, uint32_t flags, uint32_t ignore, uint32_t rec) {
    return segment_collide_v(V(start), V(end), flags, ignore, rec != 0 ? static_cast<const uint32_t*>(ee_ram_ptr(rec)) : nullptr);
}

// Adds a sparkle to the 16-slot list 0x16ea40 (0x20 each; free when the timer +0x10 is not
// positive): at `pos`, 24 frames, kind `kind` (+0x14), size `size`, a random angle. Returns its
// slot, -1 when full.
// orig: 0x001fe990
int32_t sparkle_add(const Vec4& pos, uint32_t kind, float size) {
    for (int32_t i = 0; i < 0x10; ++i) {
        const uint32_t e = OV(0x0016ea40) + static_cast<uint32_t>(i) * 0x20;
        if (0 < lh(e + 0x10)) continue;
        V(e) = pos;
        swc1(e + 0x1c, size);
        sh(e + 0x10, 0x18);
        sh(e + 0x14, kind);
        swc1(e + 0x18, rand_angle());
        return i;
    }
    return -1;
}

// Queues a camera effect (at most 3 a frame, 0x169040, 0x30 each; none in mode 2): kind `kind`,
// `a`, `b` (+0x18), the values f0 / f1, the point `at` (when given, flag +0x24); its curve
// `curve` with `f2` (-1: by kind, 0xd and 0x1f..0x27 a quarter turn, curve 2).
// orig: 0x001edf60
void cam_effect_add(uint32_t kind, uint32_t a, const Vec4* at, uint32_t b, int32_t curve, float f0, float f1, float f2) {
    if (lw(OV(0x0015f5c4)) == 2) return;
    const int32_t n = lw(OV(0x00169100));
    if (!(n < 3)) return;
    const uint32_t e = OV(0x00169040) + static_cast<uint32_t>(n) * 0x30;
    sw(e + 0x20, kind);
    sw(e + 0x14, a);
    swc1(e + 0x10, f0);
    swc1(e + 0x1c, f1);
    sw(e + 0x18, b);
    if (at != nullptr) {
        V(e) = *at;
        sh(e + 0x24, 1);
    } else {
        sh(e + 0x24, 0);
    }
    if (curve == -1) {
        const int32_t k = static_cast<int32_t>(b);
        if (k == 0xd || (!(k < 0xd) && k < 0x28 && !(k < 0x1f))) {
            sw(e + 0x2c, 2);
            swc1(e + 0x28, u2f(0x3fc90fdbu));
            sh(e + 0x26, 0);
        }
    } else {
        sw(e + 0x2c, static_cast<uint32_t>(curve));
        sh(e + 0x26, 4);
        swc1(e + 0x28, f2);
    }
    sw(OV(0x00169100), static_cast<uint32_t>(lw(OV(0x00169100)) + 1));
}

}  // namespace rac1
