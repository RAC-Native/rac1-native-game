// Ported overlay code: the particles' draw (see particles_draw.h). Hand-written assembly: its VU0
// macro ops are computed with the VU float rules (operands and results: denormals signed zero,
// Inf / NaN the signed largest float), its scratchpad DMA transfers done at their kick.
//
// The particle record (0x40 bytes, pool *(0x16016c), highest slot *(0x160174)): +0 the type,
// +1 the flags (negative: free; bits 0-1 the shape: 0 a sprite turned by the angle +8, 1 a
// sprite of size +0xc turned in the FPU, 2 a line +0x10 to +0x20, 3 a streak +0x10 to +0x20 of
// width +0x1c; bit 0x20 drawn last frame; bit 0 not drawn closer than its size), +2 the
// texture (TEX0 table 0x1b1a80), +3 written to the packet, +4 the colour (and +0xc the end's,
// shapes 2 / 3), +7 / +0xf the alpha, +8 the angle (256 steps, sine table 0x166880), +9 the
// depth range (high nibble the far end, low the near end), +0xc the size, +0x10 the position.
#include <game/ovl/particles_draw.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/render/mrp_particles.h>

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;

// orig: (helper) a VU float register (the four lanes as the VU sees them)
struct Vf {
    float v[4];
};
// orig: (helper) LQC2
Vf ldv(uint32_t a) {
    Vf r;
    for (int i = 0; i < 4; ++i) r.v[i] = lwc1(a + 4 * static_cast<uint32_t>(i));
    return r;
}
// orig: (helper) SQC2
void stv(uint32_t a, const Vf& r) {
    for (int i = 0; i < 4; ++i) swc1(a + 4 * static_cast<uint32_t>(i), r.v[i]);
}
// orig: (helper) the VU0 float ops (operands and results by the VU rules)
float vmul(float a, float b) { return vuc(vuc(a) * vuc(b)); }
float vadd(float a, float b) { return vuc(vuc(a) + vuc(b)); }
float vsub(float a, float b) { return vuc(vuc(a) - vuc(b)); }
float vmadd(float acc, float a, float b) { return vuc(vuc(acc) + vuc(a) * vuc(b)); }
float vmsub(float acc, float a, float b) { return vuc(vuc(acc) - vuc(a) * vuc(b)); }
// orig: (helper) VFTOI12: x 4096 truncated, saturating
int32_t ftoi12(float f) {
    f *= 4096.0f;
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}
// orig: (helper) VCLIPW.xyz: the six outside bits of `p` against |w| shifted into the clip flags
void clipw(uint32_t& clip, const Vf& p, float w) {
    const uint32_t wu = f2u(w);
    const int32_t lim = (wu & 0x7f800000u) != 0 ? static_cast<int32_t>(wu & 0x7fffffffu) : 0x007fffff;
    clip <<= 6;
    for (int i = 0; i < 3; ++i) {
        const uint32_t u = f2u(p.v[i]);
        if (static_cast<int32_t>(u) > lim) clip |= 1u << (2 * i);
        if (static_cast<int32_t>(u ^ 0x80000000u) > lim) clip |= 2u << (2 * i);
    }
    clip &= 0xffffff;
}
// orig: (helper) VDIV / VRSQRT Q (by zero: the signed largest float, 0 / 0 for the root a signed 0)
float vdiv(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) return u2f(((f2u(s) ^ f2u(t)) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}
// orig: (helper) VRSQRT Q = s / sqrt(|t|)
float vrsqrt(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) {
        const bool neg = ((f2u(s) ^ f2u(t)) & 0x80000000u) != 0;
        if (a != 0.0f) return u2f(neg ? 0xff7fffffu : 0x7f7fffffu);
        return u2f(neg ? 0x80000000u : 0u);
    }
    return vuc(a / std::sqrt(std::fabs(b)));
}
// orig: (helper) the view rotation rows vf29..vf31 applied to xyz (VMULAx / VMADDAy / VMADDz)
Vf rotate(const Vf r[3], const Vf& p, float w) {
    Vf o;
    for (int i = 0; i < 3; ++i) o.v[i] = vmadd(vmadd(vmul(r[0].v[i], p.v[0]), r[1].v[i], p.v[1]), r[2].v[i], p.v[2]);
    o.v[3] = w;
    return o;
}
// orig: (helper) the projection rows vf20..vf23: xyzw = row0 x + row1 y + row2 z + row3
Vf project(const Vf r[4], const Vf& p) {
    Vf o;
    for (int i = 0; i < 4; ++i) o.v[i] = vmadd(vmadd(vmadd(vmul(r[0].v[i], p.v[0]), r[1].v[i], p.v[1]), r[2].v[i], p.v[2]), r[3].v[i], 1.0f);
    return o;
}
// orig: (helper) PEXTLB + PEXTLH with zero: the colour's four bytes as four words
void colour_words(uint32_t a, uint32_t rgba) {
    for (uint32_t i = 0; i < 4; ++i) sw(a + 4 * i, (rgba >> (8 * i)) & 0xff);
}
int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }  // PMINW on sign-extended words

// The scratchpad's TEX0 entry of an unresolved texture (bit 63 clear) given its place in the
// GS memory: an upload record at 0x16cd80 (+0 the image >> 4, +4 its low nibble, +6 / +0xe the
// GS blocks of the palette and the image, +8 the high word >> 4, +0xc 0x505), the GS memory
// pointer 0x15ee74 (s0) advanced past palette and image, the TEX0 built on the template vf20.
// orig: 0x0026888c
void particle_texture_alloc(uint64_t& t0, uint64_t& t0_hi, uint32_t t1, uint32_t& s0, uint32_t& s1, uint32_t& s2, uint64_t vf20_lo,
                            uint64_t vf20_hi) {
    uint32_t a2 = static_cast<uint32_t>(t0) >> 4;
    uint32_t a3 = static_cast<uint32_t>(t0) & 0xf;
    uint64_t a1 = t0 >> 36;
    sw(s2, a2);
    t0 = vf20_lo;
    t0_hi = vf20_hi;
    sw(s2 + 8, static_cast<uint32_t>(a1));
    sh(s2 + 0xc, 0x505);
    sh(s2 + 4, a3);
    a3 <<= 8;
    const uint32_t step = 0x400 - a3;
    a2 = s0 >> 8;
    sh(s2 + 6, a2);
    const uint64_t cpsm = static_cast<uint64_t>(a3) << 43;
    s0 += step;
    t0 |= static_cast<uint64_t>(a2) << 37;
    a2 = s0 >> 8;
    t0 |= a2;
    sh(s2 + 0xe, a2);
    s2 += 0x10;
    s1 += 1;
    s0 += 0x400;
    t0 |= cpsm;
    sd(t1 + 0x3000, t0);
}

}  // namespace

// orig: 0x00267c98
void particles_draw() {
    mrp_particles_note_begin();
    uint32_t t8 = lwu(OV(0x0016016c));
    int32_t t9 = lw(OV(0x00160174));
    const int32_t s3 = lw(OV(0x0016017c));
    const Vf cam = ldv(OV(0x00166dc0));
    const Vf rot[3] = {ldv(OV(0x00166c80)), ldv(OV(0x00166c90)), ldv(OV(0x00166ca0))};
    Vf proj[4] = {ldv(OV(0x00166d40)), ldv(OV(0x00166d50)), ldv(OV(0x00166d60)), ldv(OV(0x00166d70))};
    Vf scr = ldv(OV(0x0016cc00));         // vf24
    const Vf vf26 = ldv(OV(0x0016caf0));  // vf26
    const Vf vf27 = ldv(OV(0x0016cc20));  // vf27
    const Vf vf25 = ldv(OV(0x001607e0));  // vf25
    for (int r = 0; r < 3; ++r)
        for (float& f : proj[r].v) f = vmul(f, 1024.0f);
    scr.v[0] = vmul(scr.v[0], 0.9375f);
    scr.v[1] = vmul(scr.v[1], 0.9375f);
    t9 += 1;
    if (t9 == 0) return;
    uint32_t t2 = SPR + 0x3800, t3 = SPR + 0x1000;
    const uint32_t t4 = SPR + 0x3000;
    uint32_t t5 = SPR + 0x0800;
    uint32_t clip = 0;
    // phase 1: the first 32 records in, the depth buckets cleared
    spr_dma_to(0, t8, 0x80);
    t8 += 0x800;
    for (uint32_t a = t4; a < t4 + 0x800; a += 0x10) {
        sd(a, 0);
        sd(a + 8, 0);
    }
    while (t9 != 0) {
        const uint32_t v0 = t5 & ~0x7ffu;
        int32_t t6 = t9 - 0x20;
        if (t6 < 0) t6 = 0;
        t5 = v0 ^ 0x800;
        if (t6 != 0) {  // the next 32 into the other half while this one is culled
            spr_dma_to(v0, t8, 0x80);
            t8 += 0x800;
        }
        t5 -= 0x40;
        while (t9 != t6) {
            const Vf pos = ldv(t5 + 0x50);
            const uint32_t size = lwu(t5 + 0x4c);
            t5 += 0x40;
            const int32_t t1 = static_cast<int8_t>(lbu(t5 + 1));
            t9 -= 1;
            if (t1 < 0) continue;
            Vf vf2 = {{vsub(pos.v[0], cam.v[0]), vsub(pos.v[1], cam.v[1]), vsub(pos.v[2], cam.v[2]), 0.0f}};
            const int32_t t0 = t1 & 0xdf;
            bool drawn = false;
            int32_t z12 = 0;
            if ((t1 & 2) == 0) {
                // a sprite or a line: the centre, inside the view widened by the size
                const float sx = vmul(vf25.v[0], u2f(size));
                vf2 = rotate(rot, vf2, vf2.v[3]);
                const uint32_t a2 = lbu(t5 + 9);
                const float w4x = vmul(vf27.v[0], sx), w4y = vmul(vf27.v[1], sx);
                const int32_t far = min32(static_cast<int32_t>((a2 & 0xf0) << 13), s3);
                const int32_t r12 = ftoi12(sx);
                const int32_t near = static_cast<int32_t>((a2 & 0xf) << 10);
                z12 = ftoi12(vf2.v[2]);
                float ax = u2f(f2u(vf2.v[0]) & 0x7fffffffu), ay = u2f(f2u(vf2.v[1]) & 0x7fffffffu);
                const float e5x = vmul(vf26.v[0], vf2.v[2]), e5y = vmul(vf26.v[1], vf2.v[2]);
                ax = vsub(ax, w4x);
                ay = vsub(ay, w4y);
                ax = vsub(e5x, ax);
                ay = vsub(e5y, ay);
                const int64_t z = z12;
                if (z - near > 0 && z - far < 0 && !((z - r12 < 0x100) && (t1 & 1) != 0) &&
                    (f2u(ay) & 0x80000000u) == 0 && (f2u(ax) & 0x80000000u) == 0)
                    drawn = true;
            } else {
                // a streak: both ends inside the frustum, the middle's depth
                Vf vf4 = ldv(t5 + 0x20);
                const uint32_t a0b = lbu(t5 + 9);
                for (int i = 0; i < 3; ++i) vf4.v[i] = vsub(vf4.v[i], cam.v[i]);
                const Vf vf1 = project(proj, vf2);
                const Vf vf3 = project(proj, vf4);
                Vf mid;
                for (int i = 0; i < 4; ++i) mid.v[i] = vmadd(vmul(vf2.v[i], vf25.v[2]), vf4.v[i], vf25.v[2]);
                clipw(clip, vf1, vf1.v[3]);
                clipw(clip, vf3, vf3.v[3]);
                Vf s1 = vf1, s3v = vf3;
                for (int i = 0; i < 2; ++i) {
                    s1.v[i] = vmul(vf1.v[i], scr.v[i]);
                    s3v.v[i] = vmul(vf3.v[i], scr.v[i]);
                }
                mid = rotate(rot, mid, mid.v[3]);
                const uint32_t first = clip;
                clipw(clip, s1, s1.v[3]);
                clipw(clip, s3v, s3v.v[3]);
                if ((first & 0xfff) == 0) {
                    z12 = ftoi12(mid.v[2]);
                    const int32_t far = min32(static_cast<int32_t>((a0b & 0xf0) << 13), s3);
                    const int32_t near = static_cast<int32_t>((a0b & 0xf) << 10);
                    const uint32_t both = (clip & 0x3f) & (clip >> 6);
                    if (both == 0 && z12 - near > 0 && z12 - far < 0) drawn = true;
                }
            }
            if (drawn) {  // into the bucket of its depth (z >> 10, at most 0x3ff)
                const uint32_t b = t4 + (static_cast<uint32_t>(min32(z12 >> 10, 0x3ff)) << 1);
                const uint32_t head = lhu(b);
                sh(b, t3);
                sh(t3, static_cast<uint32_t>(t9));
                sh(t3 + 2, head);
                t3 += 4;
            } else if (t0 != t1 && (t2 & 0x4000) == 0) {  // not drawn now: its "drawn" flag cleared
                sh(t2, static_cast<uint32_t>(t9));
                sh(t2 + 2, static_cast<uint32_t>(t0));
                t2 += 4;
            }
        }
    }
    // phase 2: the flags cleared, the buckets far to near into a list of indices
    const uint32_t pool = lwu(OV(0x0016016c));
    const int32_t last = lw(OV(0x00160174));
    for (uint32_t k = SPR + 0x3800; k != t2; k += 4)
        sb(static_cast<uint32_t>((last - lhu(k)) << 6) + pool + 1, lhu(k + 2));
    uint32_t list = SPR;
    for (uint32_t b = SPR + 0x37fe;;) {  // (bucket 0 is read but not drawn)
        const uint32_t head = lhu(b);
        b -= 2;
        if (b == SPR + 0x2ffe) break;
        if (head == 0) continue;
        for (uint32_t n = head | SPR; n != SPR;) {
            const uint32_t idx = lhu(n);
            n = lhu(n + 2) + SPR;
            sh(list, static_cast<uint32_t>(last - static_cast<int32_t>(idx)));
            list += 2;
        }
    }
    sh(list, 0);
    const uint32_t end = list;
    uint32_t t6 = SPR;
    t8 = SPR + 0x1000;
    if (t6 == end) return;
    // phase 3: the packets, 0x800-byte chunks through the scratchpad to the packet buffer
    uint32_t ring = lwu(OV(0x001611c0));
    const uint32_t base = lwu(OV(0x0016016c));
    uint32_t s1 = lwu(OV(0x0015f418)), s0 = lwu(0x0015ee74);
    uint32_t s2 = OV(0x0016cd80) + (s1 << 4);
    const uint32_t vf17 = OV(0x001607a0), vf18 = OV(0x001607b0), vf19 = OV(0x001607c0);
    const uint64_t vf20_lo = ld(OV(0x001607d0)), vf20_hi = ld(OV(0x001607d8));
    const float f4 = vf25.v[0];
    spr_dma_to(0x3000, OV(0x001b1a80), 0x40);  // the TEX0 of the particle textures
    spr_dma_to(0x3400, OV(0x00166880), 0x40);  // the sine table
    // the chunk of the VU1 program (REF to it), its constants (the view, the screen) and its tags
    sd(t8, 0);
    sd(t8 + 8, 0);
    sw(t8 + 4, 0x00101080);
    sw(t8, 0x30000000);
    ring += 0x10;
    sh(t8, lwu(0x00101070));
    t8 += 0x10;
    lq_sq(t8, OV(0x00160780));
    lq_sq(t8 + 0x70, OV(0x00160790));
    lq_sq(t8 + 0x50, OV(0x0016cc50));
    lq_sq(t8 + 0x60, OV(0x0016cbe0));
    for (uint32_t r = 0; r < 4; ++r) {
        Vf m = ldv(OV(0x00166cc0) + 0x10 * r);
        if (r < 3)
            for (float& f : m.v) f = vmul(f, 1024.0f);
        stv(t8 + 0x10 + 0x10 * r, m);
    }
    t8 += 0x80;
    ring += 0x80;
    auto flush = [&ring, &t8] {  // the chunk out (fromSPR), the other half next
        const uint32_t used = t8 & 0x7ff;
        t8 -= used;
        spr_dma_from(ring - used, t8, used >> 4);
        t8 ^= 0x800;
    };
    flush();
    t8 += 0x10;
    ring += 0x10;
    uint32_t verts = 0;
    uint32_t t5n = (static_cast<uint32_t>(lhu(t6)) << 6) + base;
    for (;;) {
        const uint32_t v0 = lbu(t5n + 1);
        if (t6 == end) break;
        const uint32_t p = t5n;
        Vf vf1 = ldv(p + 0x10);
        t6 += 2;
        const uint32_t shape = v0 & 3;
        t5n = (static_cast<uint32_t>(lhu(t6)) << 6) + base;
        for (int i = 0; i < 3; ++i) vf1.v[i] = vsub(vf1.v[i], cam.v[i]);
        auto mark = [p, v0] {
            if (v0 != (v0 | 0x20)) sb(p + 1, v0 | 0x20);
        };
        // the fade by the depth range: min((far - z) >> 4, z - near, 0x1000)
        auto fade = [s3](uint32_t a9, int32_t z) {
            const int32_t near = static_cast<int32_t>((a9 & 0xf) << 10);
            const int32_t far = min32(static_cast<int32_t>((a9 & 0xf0) << 13), s3);
            int32_t f = static_cast<int32_t>(static_cast<uint32_t>(far - z) >> 4);
            f = min32(f, z - near);
            return min32(f, 0x1000);
        };
        auto texture = [&](uint32_t tex, uint64_t& hi) {
            const uint32_t t1 = (tex << 3) + SPR;
            uint64_t t0 = ld(t1 + 0x3000);
            hi = 0;
            if (static_cast<int64_t>(t0) >= 0) particle_texture_alloc(t0, hi, t1, s0, s1, s2, vf20_lo, vf20_hi);
            mrp_particles_note_tex(tex, t0);  // (host: the texture's GS address this frame, for the ambient particles; no state)
            return t0;
        };
        uint32_t step = 0x40, nverts = 4;
        if (shape == 0) {
            const uint32_t tex = lbu(p + 2), ang = lbu(p + 8) << 2;
            const uint32_t rgba = lwu(p + 4);
            Vf vf3 = ldv(p);
            const uint32_t a9 = lbu(p + 9);
            Vf vf2 = rotate(rot, vf1, 0.0f);
            vf3.v[3] = vmul(vf3.v[3], vf25.v[3]);
            const int32_t z = ftoi12(vf2.v[2]);
            vf2.v[2] = vadd(vf2.v[2], vf25.v[2]);
            const float cs = lwc1(SPR + 0x3400 + ((ang + 0x300) & 0x3ff)), sn = lwc1(SPR + 0x3400 + ang);
            const float q = vdiv(vf3.v[3], vf2.v[2]);
            mark();
            stv(t8, vf1);
            colour_words(t8 + 0x10, rgba);
            sw(t8 + 0xc, 0);
            const int32_t f = fade(a9, z);
            const int64_t prod = static_cast<int64_t>(f) * static_cast<int64_t>(lbu(p + 7));
            uint64_t hi = 0;
            const uint64_t t0 = texture(tex, hi);
            sd(t8 + 0x30, t0);
            sd(t8 + 0x38, hi);
            swc1(t8 + 0x20, vmul(sn, q));
            swc1(t8 + 0x24, vmul(cs, q));
            sd(t8 + 0x28, lbu(p + 3));
            sw(t8 + 0x1c, static_cast<uint32_t>(prod) >> 12);
        } else if (shape == 1) {
            const uint32_t tex = lbu(p + 2), ang = lbu(p + 8) << 2;
            const uint32_t rgba = lwu(p + 4);
            const float size = fpu_mul(lwc1(p + 0xc), f4);
            Vf vf2 = rotate(rot, vf1, 0.0f);
            const uint32_t a9 = lbu(p + 9);
            const int32_t z = ftoi12(vf2.v[2]);
            const float sn = lwc1(SPR + 0x3400 + ang), cs = lwc1(SPR + 0x3400 + ((ang + 0x300) & 0x3ff));
            stv(t8, vf1);
            colour_words(t8 + 0x10, rgba);
            mark();
            swc1(t8 + 0x20, fpu_mul(sn, size));
            const int32_t f = fade(a9, z);
            swc1(t8 + 0x24, fpu_mul(cs, size));
            sw(t8 + 0xc, 1);
            const int64_t prod = static_cast<int64_t>(f) * static_cast<int64_t>(lbu(p + 7));
            uint64_t hi = 0;
            const uint64_t t0 = texture(tex, hi);
            sd(t8 + 0x30, t0);
            sd(t8 + 0x38, hi);
            sd(t8 + 0x28, lbu(p + 3));
            sw(t8 + 0x1c, static_cast<uint32_t>(prod) >> 12);
        } else if (shape == 2) {  // a line, untextured: the two ends and their colours
            const uint32_t rgba = lwu(p + 4), rgba2 = lwu(p + 0xc);
            Vf vf2 = ldv(p + 0x20);
            for (int i = 0; i < 3; ++i) vf2.v[i] = vsub(vf2.v[i], cam.v[i]);
            stv(t8, vf1);
            colour_words(t8 + 0x10, rgba);
            colour_words(t8 + 0x30, rgba2);
            stv(t8 + 0x20, vf2);
            sw(t8 + 0xc, 0xffffffffu);
            mark();
            sw(t8 + 0x2c, 0);
        } else {  // a streak: the ends, the width across the view (dir x middle) / |dir x middle|
            const uint32_t rgba = lwu(p + 4), rgba2 = lwu(p + 0xc);
            Vf vf2 = ldv(p + 0x20);
            for (int i = 0; i < 3; ++i) vf2.v[i] = vsub(vf2.v[i], cam.v[i]);
            const uint32_t tex = lbu(p + 2), al = lbu(p + 7), al2 = lbu(p + 0xf), a9 = lbu(p + 9);
            Vf mid, dir, cr;
            for (int i = 0; i < 3; ++i) {
                mid.v[i] = vmadd(vmul(vf1.v[i], vf25.v[2]), vf2.v[i], vf25.v[2]);
                dir.v[i] = vsub(vf2.v[i], vf1.v[i]);
            }
            cr.v[0] = vmsub(vmul(dir.v[1], mid.v[2]), mid.v[1], dir.v[2]);
            cr.v[1] = vmsub(vmul(dir.v[2], mid.v[0]), mid.v[2], dir.v[0]);
            cr.v[2] = vmsub(vmul(dir.v[0], mid.v[1]), mid.v[0], dir.v[1]);
            const Vf vm = rotate(rot, mid, 0.0f);
            float l2 = vadd(vmul(cr.v[0], cr.v[0]), vmul(cr.v[1], cr.v[1]));
            l2 = vmadd(l2, 1.0f, vmul(cr.v[2], cr.v[2]));
            const int32_t z = ftoi12(vm.v[2]);
            colour_words(t8 + 0x30, rgba);
            colour_words(t8 + 0x40, rgba2);
            const float q = vrsqrt(vf1.v[3], l2);
            stv(t8, vf1);
            stv(t8 + 0x10, vf2);
            mark();
            const int32_t f = fade(a9, z);
            sw(t8 + 0xc, 0xfffffffeu);
            sd(t8 + 0x58, lbu(p + 3));
            uint64_t hi = 0;
            const uint64_t t0 = texture(tex, hi);
            sd(t8 + 0x50, t0);
            sw(t8 + 0x20, f2u(vmul(cr.v[0], q)));
            sw(t8 + 0x24, f2u(vmul(cr.v[1], q)));
            sw(t8 + 0x28, f2u(vmul(cr.v[2], q)));
            sw(t8 + 0x3c, static_cast<uint32_t>(f) * al >> 12);
            sw(t8 + 0x4c, static_cast<uint32_t>(f) * al2 >> 12);
            sw(t8 + 0x2c, 0);
            step = 0x60;
            nverts = 6;
        }
        const uint32_t at = t8 & 0x700;
        ring += step;
        t8 += step;
        verts += nverts;
        if (at != 0x700) continue;
        // the chunk full: MSCALF at its end, its CNT tag + UNPACK and the vertex count in front
        lq_sq(t8, vf18);
        t8 += 0x10;
        ring += 0x10;
        const uint32_t used = t8 & 0x7ff;
        const uint32_t head = t8 - used;
        lq_sq(head, vf17);
        sh(head, (used >> 4) - 1);
        sb(head + 0xe, (used >> 4) - 2);
        sw(head + 0x3c, verts);
        verts = 0;
        flush();
        t8 += 0x10;
        ring += 0x10;
    }
    if (verts != 0) {
        const uint32_t used = t8 & 0x7ff;
        lq_sq(t8, vf18);
        const uint32_t head = t8 - used;
        t8 += 0x10;
        ring += 0x10;
        sw(head + 0x3c, verts);
        lq_sq(head, vf17);
        sh(head, used >> 4);
        sb(head + 0xe, (used >> 4) - 1);
        t8 += 0x10;
        ring += 0x10;
    }
    lq_sq(t8 - 0x10, vf19);
    flush();
    sw(0x0015ee74, s0);
    sw(OV(0x0015f418), s1);
    sw(OV(0x001611c0), ring);
}

}  // namespace rac1
