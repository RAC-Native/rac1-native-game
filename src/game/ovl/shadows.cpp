// Ported overlay code: the level's shadow meshes (see shadows.h). 0x1fe6d8 and 0x1fe08c are
// hand-written assembly: their VU0 macro ops are computed with the VU float rules (operands and
// results: denormals signed zero, Inf / NaN the signed largest float), their scratchpad DMA done
// at its kick.
//
// The record (32 bytes): +0 the position (xyz), +0xc a radius taken off the culling radius,
// +0x10 the depth past which it fades (over 16 units), +0x14 written: low half the alpha
// (0 = not drawn), bit 31 "needs clipping" (partly off screen); +0x18 written: its TEX0. Its
// mesh (table 0x15f440): +0 (strip batches << 16) | alpha scale, +4 the texture (+0 log2 width,
// +4 log2 height, +0x10 the PSMT8 texels; the palette is the shared 0x16e480), from +8 the
// batches, 16-byte aligned: the vertex count, the positions (3 halfwords each: 1/1024 units
// about the record's position less 32), aligned, the ST (2 halfwords each: 1/1024 less 32).
// The lights (SPR+0x2400, from 0x1802c0, 8 x 32 bytes): +0 xyz the colour, +0x10 the position,
// +0x1c the radius; one with a zero +0x10 doubleword is off.
#include <game/ovl/shadows.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/render/mrp_fx.h>

#include <cstdio>

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;
constexpr OvConst<0x001611c0u> CUR{};  // the packet's cursor (gp - 0x5a40)

// orig: (helper) a VU0 register's four lanes (raw bits)
struct Vr {
    uint32_t u[4];
};
// orig: (helper) LQC2 / SQC2
Vr ldq(uint32_t a) {
    Vr r;
    for (uint32_t i = 0; i < 4; ++i) r.u[i] = lwu(a + 4 * i);
    return r;
}
// orig: (helper)
void stq(uint32_t a, const Vr& r) {
    for (uint32_t i = 0; i < 4; ++i) sw(a + 4 * i, r.u[i]);
}
// orig: (helper) a lane as the VU reads it, a result as it writes it
float fl(uint32_t u) { return vuc(u2f(u)); }
uint32_t res(float f) { return f2u(vuc(f)); }
// orig: (helper) VMAX / VMINI: the lanes compared as signed integers (both negative: reversed)
uint32_t vmax(uint32_t a, uint32_t b) {
    const int32_t x = static_cast<int32_t>(a), y = static_cast<int32_t>(b);
    return static_cast<uint32_t>((x < 0 && y < 0) ? std::min(x, y) : std::max(x, y));
}
uint32_t vmini(uint32_t a, uint32_t b) {
    const int32_t x = static_cast<int32_t>(a), y = static_cast<int32_t>(b);
    return static_cast<uint32_t>((x < 0 && y < 0) ? std::max(x, y) : std::min(x, y));
}
// orig: (helper) VDIV Q: by zero the signed largest float
uint32_t vdiv(uint32_t s, uint32_t t) {
    const float a = fl(s), b = fl(t);
    if (b == 0.0f) return ((s ^ t) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu;
    return res(a / b);
}
// orig: (helper) VFTOI0 (truncated, saturating)
uint32_t ftoi0(uint32_t u) {
    const float f = u2f(u);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu;
    return static_cast<uint32_t>(static_cast<int32_t>(f));
}
// orig: (helper) VITOF12
uint32_t itof12(uint32_t u) { return f2u(static_cast<float>(static_cast<int32_t>(u)) * (1.0f / 4096.0f)); }
// orig: (helper) CVT.W.S (truncated, saturating)
int32_t cvt_w(float s) {
    if (s >= 2147483648.0f) return INT32_MAX;
    if (s <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(s);
}
// orig: (helper) MULT rd: the low word of the signed product
int32_t mult(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(a) * b)); }
// orig: (helper) a doubleword at any address (LDR / LDL)
uint64_t ld_unaligned(uint32_t a) {
#if defined(RAC1_EE_WORDS_BE)
    uint64_t v = 0;  // (EE memory as host words: byte by byte, each at its place)
    for (uint32_t i = 0; i < 8; ++i) v |= static_cast<uint64_t>(lbu(a + i)) << (8 * i);
    return v;
#else
    uint64_t v;
    std::memcpy(&v, ee_ram_ptr(a), 8);
    return v;
#endif
}
// orig: (helper) the pad to the next quadword: a zero word at the cursor, then one per word to it
uint32_t pad_zero(uint32_t at) {
    for (;;) {
        const bool aligned = (at & 0xf) == 0;
        sw(at, 0);
        if (aligned) return at;
        at += 4;
    }
}
// orig: (helper) 4 halfwords zero-extended to words, times 4 (PEXTLH zero / PSLLW 2)
void halves4(uint32_t w[4], uint64_t d) {
    for (int i = 0; i < 4; ++i) w[i] = static_cast<uint32_t>((d >> (16 * i)) & 0xffff) << 2;
}

}  // namespace

// orig: 0x002a0eb8
void packet_ref_13cf10() {
    const uint32_t p = lwu(CUR);
    sw(p, 0x3000000bu);
    sw(p + 4, 0x0013cf10u);
    sw(p + 8, 0);
    sw(p + 0xc, 0x5000000bu);
    sw(CUR, p + 0x10);
}

// orig: 0x001ffa18 — a matrix of diagonal (s, s, s) and row 3 vf0 (VU0)
void mat4_scale_diag(Mat4& d, float s) {
    const float z = vuc(0.0f * 0.0f);
    d.r[0] = Vec4{vuc(z + vuc(s)), z, z, vuc(1.0f * 0.0f)};
    d.r[1] = Vec4{z, vuc(z + vuc(s)), z, vuc(1.0f * 0.0f)};
    d.r[2] = Vec4{z, z, vuc(z + vuc(s)), vuc(1.0f * 0.0f)};
    d.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
}

// orig: 0x001fe6d8
void shadows_prepare(uint32_t s6) {
    spr_dma_to(0x2400, OV(0x001802c0), 0x10);  // toSPR: the lights (the wait is 0x1fe08c's)
    // vf30.xyz = 1 (VADDW vf30, vf0, vf0w): not read by this pass
    const uint32_t t2 = OV(0x0016e480);  // the shadows' palette
    uint32_t v = lwu(0x0015ee74);
    const uint32_t t4 = v >> 8;  // the palette's GS block
    sw(0x0015ee74, v + 0x400);
    int32_t n = lw(OV(0x0015f438));
    uint32_t rec = lwu(OV(0x0015f43c)), tab = lwu(OV(0x0015f440));
    const uint32_t cam = OV(0x00166c80);  // the view rows; +0x140 the camera
    const float f3 = 16.0f, f7 = 255.0f;
    const float f6 = fpu_div(f7, f3);
    do {
        const uint32_t t1 = lwu(tab);
        const float f1 = lwc1(rec + 0x10);
        const float f2 = fpu_add(f1, f3);
        const float f8 = lwc1(rec + 0xc);
        const float f12 = fpu_sub(f2, f8);
        const int64_t on = point_on_screen(rec, f12);
        sw(rec + 0x14, 0);
        if (on >= 0) {
            // the view depth: (rows 0x166c80..0x166ca0) . (position - camera), w 1
            const Vr p = ldq(rec), c = ldq(cam + 0x140);
            float d[3];
            for (int k = 0; k < 3; ++k) d[k] = vuc(fl(p.u[k]) - fl(c.u[k]));
            const Vr r0 = ldq(cam), r1 = ldq(cam + 0x10), r2 = ldq(cam + 0x20);
            float acc = vuc(fl(r0.u[2]) * d[0]);
            acc = vuc(acc + fl(r1.u[2]) * d[1]);
            acc = vuc(acc + fl(r2.u[2]) * d[2]);
            const float f4 = vuc(acc + 0.0f * 1.0f);
            uint32_t a1;
            if (f4 <= f1) {
                a1 = s6 | 0xff;
            } else {
                const float f5 = fpu_sub(f7, fpu_mul(fpu_sub(f4, f1), f6));
                a1 = static_cast<uint32_t>(cvt_w(f5));
            }
            if (on == 0) a1 |= 0x80000000u;
            sw(rec + 0x14, a1);
            // the texture's GS address (the pointer 0x15ee74 past its texels), its TEX0
            v = lwu(0x0015ee74);
            const uint32_t t3 = v >> 8;
            const uint32_t t7 = lwu(t1 + 4);
            const int32_t tw = lw(t7), th = lw(t7 + 4);
            v += 1u << ((static_cast<uint32_t>(tw + th)) & 31);
            sw(0x0015ee74, v);
            // TBW: 1 << max(tw - 6, 0), but the clamp's MOVE sits in the BGEZ's delay slot: 1 << 0
            const uint64_t tbw = 1;
            const uint64_t tex0 = static_cast<uint64_t>(t3) + (tbw << 14) + (0x13ull << 20) + (static_cast<uint64_t>(static_cast<int64_t>(tw)) << 26) +
                                  (static_cast<uint64_t>(static_cast<int64_t>(th)) << 30) + (1ull << 34) + (static_cast<uint64_t>(t4) << 37) +
                                  (0x1000ull << 51);
            sd(rec + 0x18, tex0);
            // the upload record (palette, texels) at 0x16cd80 + 16 x *(0x15f418)
            const int32_t cnt = lw(OV(0x0015f418));
            if (cnt + 0x40 >= 0) {
                const uint32_t e = OV(0x0016cd80) + (static_cast<uint32_t>(cnt) << 4);
                sw(e, t2);
                sh(e + 4, 0);
                sh(e + 6, t4);
                sw(e + 8, t7 + 0x10);
                sb(e + 0xc, static_cast<uint32_t>(lw(t7)));
                sb(e + 0xd, static_cast<uint32_t>(lw(t7 + 4)));
                sh(e + 0xe, t3);
                sw(OV(0x0015f418), static_cast<uint32_t>(cnt + 1));
            }
        }
        rec += 0x20;
        --n;
        tab += 4;
    } while (n != 0);
}

// orig: 0x001fc740
void shadows_vu1_setup() {
    Mat4 m;
    mat4_scale_diag(m, 1024.0f);
    vec3_scale(m.r[3], V(OV(0x00166dc0)), -1024.0f);
    m.r[3].w = 1.0f;
    if (lw(OV(0x0015f5e0)) != 7) {
        packet_ref(0x0010e810, lhu(0x0010e800));  // the VU1 program
        sw(OV(0x0015f5e0), 7);
    }
    sw(lwu(CUR), 0x10000000u);
    sw(lwu(CUR) + 4, 0);
    sw(lwu(CUR) + 8, 0x11000000u);     // FLUSH
    sw(lwu(CUR) + 0xc, 0x01000404u);   // STCYCL 4, 4
    const uint32_t s0 = lwu(CUR);
    sw(s0 + 0x10, 0);
    sw(s0 + 0x14, 0);
    sw(s0 + 0x18, 0);
    sw(s0 + 0x1c, 0x6c0c43a4u);        // UNPACK V4-32, 12 quadwords to 0x3a4
    mat4_mul(M(s0 + 0x20), M(OV(0x00166cc0)), m);
    swc1(s0 + 0x20 + 0x38, fpu_add(lwc1(s0 + 0x20 + 0x38), lwc1(OV(0x0015f308))));
    mat4_mul(M(s0 + 0x60), M(OV(0x00166d40)), m);
    swc1(s0 + 0x60 + 0x38, fpu_add(lwc1(s0 + 0x60 + 0x38), lwc1(OV(0x0015f308))));
    sw(s0 + 0xa0, 0x8000);             // the clipped fans' GIF tag (vf31), w the screen scale
    sw(s0 + 0xa4, 0x303ec000u);
    sw(s0 + 0xa8, 0x412);
    swc1(s0 + 0xac, lwc1(OV(0x0016cc50)));
    lq_sq(s0 + 0xb0, OV(0x0016cbd0));      // vf29: the screen scale
    lq_sq(s0 + 0xc0, OV(0x0016cbe0));      // vf30: the screen offset
    swc1(s0 + 0xd0, lwc1(OV(0x0016cc6c)));  // vf7: the fog's clamp
    swc1(s0 + 0xd4, lwc1(OV(0x0016cc68)));
    sw(s0 + 0xe4, 0x020001d2u);        // OFFSET 0x1d2
    sw(s0 + 0xe8, 0x15000000u);        // MSCALF 0: the start-up
    sw(s0 + 0xe0, 0x03000000u);        // BASE 0
    sw(s0 + 0xd8, 0);
    sw(s0 + 0xdc, 0);
    sw(s0 + 0xec, 0);
    const uint32_t s1 = s0 + 0xf0;
    const uint32_t a0 = lwu(CUR);
    sw(a0, lwu(a0) | static_cast<uint32_t>((static_cast<int32_t>(s1 - a0) >> 4) - 1));
    sw(CUR, s1);
    mrp_fx_note_vu7(s0 + 0x20);  // (host: the prim lists' projection, mrp_fx.h)
    packet_ref_13cf10();
}

// orig: 0x001fe08c
void shadows_packets() {
    // 0x1fe0c4: the wait for the toSPR DMA, but on t2 as 0x1fe6d8 left it (the palette 0x16e480,
    // not the channel 0x1000d400): a set bit 8 there would spin the original forever
    if ((lwu(OV(0x0016e480)) & 0x100u) != 0) {
        static bool logged = false;
        if (!logged) std::fprintf(stderr, "[TODO_PORT] 0x001fe0c4: the original spins forever here (bit 8 of 0x16e480 set): not spun\n");
        logged = true;
    }
    // the lights on: their +0 w made 1 / radius, packed at SPR+0x2400
    uint32_t at = 0x2400, v1 = 0x2400;
    int32_t nl = 0;
    for (;;) {
        if (at == 0x2500) break;
        const uint64_t a1 = ld(SPR + at + 0x10);
        at += 0x20;
        if (a1 == 0) continue;
        const Vr l1 = ldq(SPR + at - 0x10);
        Vr l0 = ldq(SPR + at - 0x20);
        const uint32_t q = vdiv(0x3f800000u, l1.u[3]);
        stq(SPR + v1 + 0x10, l1);
        l0.u[3] = res(1.0f * fl(q));
        stq(SPR + v1, l0);
        ++nl;
        v1 += 0x20;
    }
    const uint32_t vf5x = lwu(OV(0x00166d50));  // vf5.x as 0x1ffdd8 left it (row 1 of 0x166d40): read by BGTZ
    uint32_t s1 = lwu(OV(0x0015f43c));
    int32_t s2 = lw(OV(0x0015f438));
    uint32_t s4 = lwu(OV(0x0015f440));
    do {
        const uint32_t s3 = lwu(s4);
        const Vr p = ldq(s1);
        uint32_t vf15x = 0;  // vf15 = vf0 + vf0: x 0
        if (nl != 0) {
            uint32_t v0 = 0x2400;
            int32_t n = nl;
            Vr vf2 = ldq(SPR + 0x2410);
            uint32_t d[3];
            for (int k = 0; k < 3; ++k) d[k] = res(fl(vf2.u[k]) - fl(p.u[k]));
            for (;;) {
                for (int k = 0; k < 3; ++k) d[k] = res(fl(d[k]) * fl(d[k]));  // (BEQ's delay slot)
                if (n == 0) break;
                const uint32_t r2 = res(fl(vf2.u[3]) * fl(vf2.u[3]));
                v0 += 0x20;
                vf2 = ldq(SPR + v0 + 0x10);
                --n;
                float acc = vuc(fl(d[1]) + fl(d[0]));
                acc = vuc(acc + 1.0f * fl(d[2]));
                const uint32_t vf5y = res(acc + -1.0f * fl(r2));
                const uint32_t dist2 = res(acc + 0.0f * 0.0f);
                for (int k = 0; k < 3; ++k) d[k] = res(fl(vf2.u[k]) - fl(p.u[k]));
                const uint32_t q = res(std::sqrt(std::fabs(fl(dist2))));
                const int64_t a1 = static_cast<int64_t>((static_cast<uint64_t>(vf5y) << 32) | vf5x);
                if (a1 > 0) continue;  // outside the light's radius
                Vr l0 = ldq(SPR + v0 - 0x20);  // (vf8 = position - light: not read)
                l0.u[3] = res(fl(l0.u[3]) * fl(q));
                l0.u[3] = res(1.0f - fl(l0.u[3]));
                l0.u[0] = vmax(l0.u[0], l0.u[1]);
                l0.u[0] = vmax(l0.u[0], l0.u[2]);
                l0.u[0] = res(fl(l0.u[0]) * 0.5f);
                l0.u[0] = vmini(l0.u[0], 0x3f800000u);
                l0.u[0] = res(fl(l0.u[0]) * fl(l0.u[3]));
                vf15x = res(fl(vf15x) + fl(l0.u[0]));
            }
        }
        vf15x = vmini(vf15x, 0x3f800000u);
        vf15x = res(1.0f - fl(vf15x));
        vf15x = res(fl(vf15x) * 256.0f);
        const int32_t light = static_cast<int32_t>(ftoi0(vf15x));
        const uint32_t v0 = lwu(s1 + 0x14);
        int32_t a2 = static_cast<int32_t>(v0 & 0xffff);
        if (a2 != 0) {
            const uint32_t t0 = ((v0 >> 31) & 1) * 2 + 0xc;  // MSCALF 0xc, or 0xe: clipped
            sw(OV(0x001c40a0), lwu(s1 + 0x18));  // the TEX0 into the mesh's GS packet
            sw(OV(0x001c40a4), lwu(s1 + 0x1c));
            uint32_t t8 = lwu(CUR);
            for (uint32_t k = 0; k < 3; ++k) lq_sq(t8 + 0x10 * k, OV(0x001c4080) + 0x10 * k);
            sw(CUR, t8 + 0x30);
            uint32_t a1 = s3;
            const uint32_t w0 = lwu(a1);
            a2 = mult(a2, static_cast<int32_t>(w0 & 0xffff));
            a2 = mult(a2, light);
            a2 = static_cast<int32_t>(static_cast<uint32_t>(a2) >> 16);
            uint32_t batches = w0 >> 16;
            a1 += 8;
            do {
                a1 = (a1 + 0xf) & ~0xfu;
                const uint32_t cnt = lwu(a1);
                a1 += 4;
                const uint32_t xyz = a1;
                a1 += static_cast<uint32_t>(mult(static_cast<int32_t>(cnt) >> 1, 0xc));
                a1 = (a1 + 0xf) & ~0xfu;
                const uint32_t st = a1;
                a1 += cnt * 4;
                const uint32_t t2 = lwu(CUR);
                uint32_t o = t2;
                const uint32_t t7 = cnt << 16;
                for (uint32_t k = 0; k < 5; ++k) lq_sq(o + 0x10 * k, OV(0x001c4020) + 0x10 * k);
                o += 0x50;
                sw(o - 0x40, lwu(o - 0x40) | cnt);  // the GIF tag's NLOOP
                lq_sq(o, OV(0x00160ec0));              // UNPACK of the ST
                o += 0x10;
                sw(o - 4, lwu(o - 4) | t7);
                uint32_t t3 = st;
                uint32_t left = cnt;
                do {
                    uint32_t w[4];
                    halves4(w, ld(t3));
                    left -= 2;
                    for (uint32_t k = 0; k < 4; ++k) sw(o + 4 * k, res(fl(itof12(w[k])) - 32.0f));
                    t3 += 8;
                    o += 0x10;
                } while (left != 0);
                o = pad_zero(o);
                lq_sq(o, OV(0x00160ee0));  // UNPACK of the colours
                o += 0x10;
                sw(o - 4, lwu(o - 4) | t7);
                const int64_t lo = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(a2) << 24)) | 0xffffff;
                const uint64_t rgba = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(lo) << 32);
                left = cnt;
                do {
                    left -= 2;
                    sd(o, rgba);
                    o += 8;
                } while (left != 0);
                o = pad_zero(o);
                lq_sq(o, OV(0x00160f00));  // UNPACK of the positions
                o += 0x10;
                sw(o - 4, lwu(o - 4) | t7);
                Vr base = p;  // vf4: the record's position less 32
                for (int k = 0; k < 4; ++k) base.u[k] = res(fl(base.u[k]) - 32.0f);
                t3 = xyz;
                left = cnt;
                do {
                    uint32_t a[4], b[4];
                    halves4(a, ld_unaligned(t3));
                    halves4(b, ld_unaligned(t3 + 4) >> 16);
                    t3 += 12;
                    left -= 2;
                    for (uint32_t k = 0; k < 3; ++k) sw(o + 4 * k, res(fl(itof12(a[k])) + fl(base.u[k])));
                    for (uint32_t k = 0; k < 3; ++k) sw(o + 0xc + 4 * k, res(fl(itof12(b[k])) + fl(base.u[k])));
                    o += 0x18;
                } while (left != 0);
                o = pad_zero(o);
                sw(o, 0x15000000u | t0);  // MSCALF
                sw(o + 4, 0x11000000u);   // FLUSH
                sd(o + 8, 0);
                o += 0x10;
                sw(t2, lwu(t2) | (((o - t2) >> 4) - 1));
                sw(CUR, o);
                --batches;
            } while (batches != 0);
        }
        s1 += 0x20;
        --s2;
        s4 += 4;
    } while (s2 != 0);
}

// orig: 0x001fca48
void shadows_draw(uint32_t s6) {
    if (lw(OV(0x0015f438)) == 0) return;
    sub_002a0ba8(0x08, 5);        // CLAMP_1
    sub_002a0ba8(0x14, 0x61);     // TEX1_1
    sub_002a0ba8(0x47, 0x513f1);  // TEST_1
    sub_002a0ba8(0x4a, 1);        // FBA_1
    shadows_prepare(s6);
    sw(OV(0x0015f308), 0xbd23d70au);  // the Z bias -0.04
    shadows_vu1_setup();
    shadows_packets();
    sw(OV(0x0015f308), 0);
    sub_002a0ba8(0x4a, 0);
}

}  // namespace rac1
