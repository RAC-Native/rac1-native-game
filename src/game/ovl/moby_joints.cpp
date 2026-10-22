// Ported overlay code: the moby joints (see moby_joints.h). Hand-written assembly kept in its
// control flow: VU0 macro ops on the shared VU0 register file (vu0_regs.h), the scratchpad DMA
// transfers done at their kick (so the original's waits on a sentinel word the transfer
// overwrites are already over), MMI unpacking of the s16 keys.
//
// Scratchpad while the pose is built: per joint 0x40 bytes at 0 (+0 the rotation quaternion, +0x10
// the scale (+0x1c set: scaled), +0x20 the translation (+0x2c the parent's record or 0), +0x30
// the second key's translation, +0x3c a list link), the keyframes from 0x2000, the bind matrices
// at 0x1c00. The RAM buffer `work`: +0 the lists' heads, +0x10 per joint 0x20 bytes (the scale
// keys); +0x1fc0 the transfer's DMA chain.
#include <game/ovl/moby_joints.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_regs.h>

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;
using namespace vu0m;

// orig: (helper) a 32-bit op's result as the 64-bit register holds it
uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// orig: (helper) BLTZ / BGEZ on a 64-bit register
bool neg(uint64_t v) { return static_cast<int64_t>(v) < 0; }
// orig: (helper) QMTC2 of four words
void set_words(Vf& d, const uint32_t w[4]) {
    for (int i = 0; i < 4; ++i) d.v[i] = u2f(w[i]);
}
// orig: (helper) PEXTLH with zero then PSRLW 13: the four halfwords << 3, unsigned
void unpack_u3(Vf& d, uint64_t x) {
    uint32_t w[4];
    for (int i = 0; i < 4; ++i) w[i] = (static_cast<uint32_t>((x >> (16 * i)) & 0xffff) << 16) >> 13;
    set_words(d, w);
}
// orig: (helper) PEXTLH / PEXTUH with zero then PSRAW 16: the four halfwords sign-extended
void unpack_s16(Vf& d, uint64_t x) {
    uint32_t w[4];
    for (int i = 0; i < 4; ++i) w[i] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>((x >> (16 * i)) & 0xffff)));
    set_words(d, w);
}
// orig: (helper) VITOFn on the masked lanes
void itof_lanes(Vf& d, const Vf& s, int m, float scale) {
    lanes(d, m, [&](int i) { return itof(s.v[i], scale); });
}
// orig: (helper) VMR32.xyzw
void mr32(Vf& d, const Vf& s) {
    const Vf t = s;
    d.v[0] = t.v[1];
    d.v[1] = t.v[2];
    d.v[2] = t.v[3];
    d.v[3] = t.v[0];
}
// orig: (helper) the quaternion dot product into y (VADDAx.y / VMADDAz.y / VMADDw.y with vf4 = 1)
void dot_y(LocalFlags& fl, Vf& d) {
    Vf* const v = vf();
    addabc(fl, d, d.v[0], Y);
    maddabc(fl, v[4], d.v[2], Y);
    maddbc(fl, d, v[4], d.v[3], Y);
}
// orig: (helper) the four lanes summed into x (VADDAy.x / VMADDAz.x / VMADDw.x with vf4 = 1)
void sum_x(LocalFlags& fl, Vf& d) {
    Vf* const v = vf();
    addabc(fl, d, d.v[1], X);
    maddabc(fl, v[4], d.v[2], X);
    maddbc(fl, d, v[4], d.v[3], X);
}
// orig: (helper) two weighted quaternions summed (or, of opposite sense, subtracted) and
// normalised: vf16 the sum, vf17.x its squared length, vf18.y the dot
void blend_quats(LocalFlags& fl, int a, int b) {
    Vf* const v = vf();
    add(fl, v[16], v[a], v[b], XYZW);
    mul(fl, v[18], v[a], v[b], XYZW);
}
// orig: (helper) (the second half: the length, the shorter arc, Q = 1 / length)
void blend_quats_norm(LocalFlags& fl, int a, int b) {
    Vf* const v = vf();
    mul(fl, v[17], v[16], v[16], XYZW);
    dot_y(fl, v[18]);
    sum_x(fl, v[17]);
    if (neg(qlo(v[18]))) {
        sub(fl, v[16], v[a], v[b], XYZW);
        mul(fl, v[17], v[16], v[16], XYZW);
        sum_x(fl, v[17]);
    }
    q() = vrsqrt(v[0].v[3], v[17].v[0]);
}

}  // namespace

// orig: 0x00250fe0
void moby_joints_pose(uint32_t a0, uint32_t a1) {
    LocalFlags fl(regs());  // (host: the ops' flags in locals, the VU0's written back at the return: the same values)
    Vf* const v = vf();
    qset_xy(v[28], sx(a1));  // (the caller's s0..s2 parked in vf29..vf31: no such registers here)
    const int32_t n = lh(a0 + 0x10);
    if (n == 0) {  // no joints: the identity
        mulbc(fl, v[1], v[0], v[0].v[0], XYZW);
        mulbc(fl, v[2], v[0], v[0].v[0], XYZW);
        mr32(v[3], v[0]);
        addbc(fl, v[1], v[1], v[0].v[3], X);
        addbc(fl, v[2], v[2], v[0].v[3], Y);
        sqc2(SPR, v[1]);
        sqc2(SPR + 0x10, v[2]);
        sqc2(SPR + 0x20, v[3]);
        sqc2(SPR + 0x30, v[0]);
        return;
    }
    const uint32_t t7 = a0;
    const uint32_t un = static_cast<uint32_t>(n);
    // the transfer's chain at work + 0x1fc0: the joints' rest data, the first keyframe, the second
    // (the chain ended after the first when there is none)
    const uint32_t chain = a1 + 0x1fc0;
    sw(chain, 0x30000000u | un);
    sw(chain + 4, lwu(t7 + 0x18));
    const uint32_t key2 = lwu(t7 + 4);
    sw(chain + 0x10, (0x30000000u | lwu(t7 + 0x24)) & (key2 == 0 ? 0xffffu : 0xffffffffu));
    sw(chain + 0x14, lwu(t7 + 0x20));
    const int32_t flags = lh(t7 + 0x12);
    mulbc(fl, v[1], v[0], v[0].v[0], XYZW);
    mulbc(fl, v[2], v[0], v[0].v[0], XYZW);
    mr32(v[3], v[0]);
    addbc(fl, v[4], v[0], v[0].v[3], XYZ);
    sub(fl, v[4], v[0], v[0], W);
    sw(chain + 0x20, lwu(t7 + 0x34));
    sw(chain + 0x24, lwu(t7 + 0x30));
    addbc(fl, v[1], v[1], v[0].v[3], X);
    addbc(fl, v[2], v[2], v[0].v[3], Y);
    v[7] = lqc2(t7);
    uint32_t at = (un << 4) + SPR + 0x2000;  // the first key: quaternions, then its lists
    uint32_t k2 = (lwu(t7 + 0x24) << 4) + at;  // the second key
    uint32_t v0 = lhu(t7 + 0x28) + at, v1 = lhu(t7 + 0x2c) + at;
    uint32_t l1 = lhu(t7 + 0x38) + k2, l2 = lhu(t7 + 0x3c) + k2;
    subbc(fl, v[6], v[4], v[7].v[1], X);  // 1 - t
    mr32(v[7], v[7]);                 // t
    sw(at - 4, 0xffffffffu);
    sw(k2 - 4, 0xffffffffu);
    const uint32_t s1 = (lwu(t7 + 0x34) << 4) + k2;
    sw(s1 - 4, 0xffffffffu);
    const uint32_t t5 = static_cast<uint32_t>(qlo(v[28])) + 0x10;
    subbc(fl, v[5], v[0], v[0].v[3], X);
    sd(t5 - 0x10, 0);
    sd(t5 - 8, 0);
    spr_dma_to_chain(0x2000, chain, 0x104);
    uint32_t t4 = 0;
    if (key2 != 0) {
        {
            // the scales 1 (joints and records)
            int32_t c = n;
            uint32_t p = SPR - 0x40, r = t5 - 0x20;
            do {
                p += 0x40;
                sqc2(p, v[4]);
                r += 0x20;
                sqc2(p + 0x10, v[4]);
                --c;
                sqc2(r, v[4]);
                sqc2(r + 0x10, v[4]);
            } while (c > 0);
        }
        {
            // the rest translations, both keys' slots
            int32_t c = n;
            uint32_t src = SPR + 0x2000, dst = SPR - 0x100;
            do {
                v[16] = lqc2(src);
                dst += 0x100;
                v[17] = lqc2(src + 0x10);
                c -= 4;
                v[18] = lqc2(src + 0x20);
                v[19] = lqc2(src + 0x30);
                sqc2(dst + 0x20, v[16]);
                sqc2(dst + 0x60, v[17]);
                sqc2(dst + 0xa0, v[18]);
                sqc2(dst + 0xe0, v[19]);
                sqc2(dst + 0x30, v[16]);
                sqc2(dst + 0x70, v[17]);
                sqc2(dst + 0xb0, v[18]);
                sqc2(dst + 0xf0, v[19]);
                sw(dst + 0x3c, 0);
                sw(dst + 0x7c, 0);
                src += 0x40;
                sw(dst + 0xbc, 0);
                sw(dst + 0xfc, 0);
            } while (c > 0);
        }
        // the first key's scales (u3.15): the joint's (negative) or a record's, listed
        uint32_t s0 = t5 - 0x10, t3 = t5 - 0x1c;
        for (;;) {
            const uint64_t d = ld(v0);
            if (v0 == v1) break;
            uint32_t j = lbu(v0 + 6);
            v0 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                sw(t3 + 0xc, jp);
                t3 = jp;
                sqc2(jp, v[8]);
            } else {
                j += t5;
                sw(s0 + 0xc, j);
                s0 = j;
                sqc2(j, v[8]);
            }
        }
        // the first key's translations (s16), listed
        int32_t c2 = lh(t7 + 0x2e);
        t4 = t5 - 0x48;
        for (;;) {
            const uint64_t d = ld(v1);
            if (c2 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(v1 + 6));
            --c2;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            v1 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t keep = lwu(jp + 0x2c);
            sw(t4 + 0x3c, jp);
            t4 = jp;
            sw(jp + 0x3c, 0);
            sqc2(jp + 0x20, v[8]);
            sw(jp + 0x2c, keep);
        }
        // the second key's scales: into the +0x10 slots, listed when not yet
        for (;;) {
            const uint64_t d = ld(l1);
            if (l1 == l2) break;
            uint32_t j = lbu(l1 + 6);
            l1 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                const uint32_t linked = lwu(jp + 0xc);
                sqc2(jp + 0x10, v[8]);
                if (linked == 0) {
                    sw(t3 + 0xc, jp);
                    t3 = jp;
                }
            } else {
                j += t5;
                const uint32_t linked = lwu(j + 0x1c);
                sqc2(j + 0x10, v[8]);
                if (linked == 0) {
                    sw(s0 + 0xc, j);
                    s0 = j;
                }
            }
        }
        sw(t3 + 0xc, 0);
        sw(s0 + 0xc, 0);
        // the second key's translations: into +0x30, listed when not yet
        int32_t c3 = lh(t7 + 0x3e);
        uint32_t a2 = l2;
        for (;;) {
            const uint64_t d = ld(a2);
            if (c3 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(a2 + 6));
            --c3;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            a2 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t linked = lwu(jp + 0x3c);
            sqc2(jp + 0x30, v[8]);
            sw(jp + 0x3c, linked);
            if (linked == 0) {
                sw(t4 + 0x3c, jp);
                t4 = jp;
                sw(jp + 0x3c, 0);
            }
        }
        // the listed scales and translations lerped by t
        v[10].v[3] = v[0].v[3];
        for (uint32_t p = lwu(t5 - 0x10); p != 0;) {
            v[8] = lqc2(p);
            v[9] = lqc2(p + 0x10);
            mulabc(fl, v[8], v[6].v[0], XYZ);
            maddbc(fl, v[10], v[9], v[7].v[0], XYZ);
            const uint32_t nx = lwu(p + 0xc);
            sqc2(p + 0x10, v[10]);
            p = nx;
        }
        for (uint32_t p = lwu(t5 - 0xc); p != 0;) {
            v[8] = lqc2(p + 0x20);
            v[9] = lqc2(p + 0x30);
            mulabc(fl, v[8], v[6].v[0], XYZ);
            maddbc(fl, v[10], v[9], v[7].v[0], XYZ);
            const uint32_t keep = lwu(p + 0x2c);
            sqc2(p + 0x20, v[10]);
            sw(p + 0x2c, keep);
            p = lwu(p + 0x3c);
        }
        // the rotations, two joints at a time: lerped along the shorter arc and normalised
        // (flag 0x400: lerped only)
        const bool plain = (flags & 0x400) != 0;
        uint32_t p = SPR - 0x80;
        for (int32_t c = n; c > 0; c -= 2) {
            const uint64_t qa_lo = ld(at), qa_hi = ld(at + 8), qb_lo = ld(k2), qb_hi = ld(k2 + 8);
            unpack_s16(v[8], qa_lo);
            unpack_s16(v[9], qb_lo);
            unpack_s16(v[10], qa_hi);
            unpack_s16(v[11], qb_hi);
            itof_lanes(v[8], v[8], XYZW, 1.0f / 32768.0f);
            itof_lanes(v[9], v[9], XYZW, 1.0f / 32768.0f);
            itof_lanes(v[10], v[10], XYZW, 1.0f / 32768.0f);
            itof_lanes(v[11], v[11], XYZW, 1.0f / 32768.0f);
            p += 0x80;
            at += 0x10;
            k2 += 0x10;
            if (plain) {
                mulabc(fl, v[8], v[6].v[0], XYZW);
                maddbc(fl, v[8], v[9], v[7].v[0], XYZW);
                mulabc(fl, v[10], v[6].v[0], XYZW);
                maddbc(fl, v[10], v[11], v[7].v[0], XYZW);
            } else {
                mul(fl, v[12], v[8], v[9], XYZW);
                mul(fl, v[13], v[10], v[11], XYZW);
                mulbc(fl, v[14], v[9], v[5].v[0], XYZW);
                mulbc(fl, v[15], v[11], v[5].v[0], XYZW);
                mulabc(fl, v[8], v[6].v[0], XYZW);
                maddbc(fl, v[8], v[9], v[7].v[0], XYZW);
                maddbc(fl, v[9], v[14], v[7].v[0], XYZW);
                dot_y(fl, v[12]);
                dot_y(fl, v[13]);
                mulabc(fl, v[10], v[6].v[0], XYZW);
                const uint64_t da = qlo(v[12]);
                maddbc(fl, v[10], v[11], v[7].v[0], XYZW);
                maddbc(fl, v[11], v[15], v[7].v[0], XYZW);
                const uint64_t db = qlo(v[13]);
                if (!neg(da)) {
                    mul(fl, v[9], v[8], v[8], XYZW);
                } else {
                    v[8] = v[9];
                    mul(fl, v[9], v[9], v[9], XYZW);
                }
                if (!neg(db)) {
                    mul(fl, v[11], v[10], v[10], XYZW);
                } else {
                    v[10] = v[11];
                    mul(fl, v[11], v[11], v[11], XYZW);
                }
                sum_x(fl, v[9]);
                sum_x(fl, v[11]);
                q() = vrsqrt(v[0].v[3], v[9].v[0]);
                mulbc(fl, v[8], v[8], q(), XYZW);
                q() = vrsqrt(v[0].v[3], v[11].v[0]);
                mulbc(fl, v[10], v[10], q(), XYZW);
            }
            sqc2(p, v[8]);
            sqc2(p + 0x40, v[10]);
        }
    } else {  // one keyframe
        {
            int32_t c = n;
            uint32_t p = SPR - 0x40, r = t5 - 0x20;
            do {
                p += 0x40;
                r += 0x20;
                sqc2(p + 0x10, v[4]);
                --c;
                sqc2(r, v[4]);
            } while (c > 0);
        }
        {
            int32_t c = n;
            uint32_t src = SPR + 0x2000, dst = SPR - 0x100;
            do {
                v[16] = lqc2(src);
                dst += 0x100;
                v[17] = lqc2(src + 0x10);
                c -= 4;
                v[18] = lqc2(src + 0x20);
                v[19] = lqc2(src + 0x30);
                sqc2(dst + 0x20, v[16]);
                sqc2(dst + 0x60, v[17]);
                sqc2(dst + 0xa0, v[18]);
                src += 0x40;
                sqc2(dst + 0xe0, v[19]);
            } while (c > 0);
        }
        uint32_t s0 = t5 - 0x10;
        for (;;) {
            const uint64_t d = ld(v0);
            if (v0 == v1) break;
            uint32_t j = lbu(v0 + 6);
            v0 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                sqc2(jp + 0x10, v[8]);
            } else {
                j += t5;
                sw(s0 + 0xc, j);
                s0 = j;
                sqc2(j, v[8]);
            }
        }
        sw(s0 + 0xc, 0);
        int32_t c2 = lh(t7 + 0x2e);
        for (;;) {
            const uint64_t d = ld(v1);
            if (c2 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(v1 + 6));
            --c2;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            v1 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t keep = lwu(jp + 0x2c);
            sqc2(jp + 0x20, v[8]);
            sw(jp + 0x2c, keep);
        }
        int32_t c = n;
        uint32_t p = SPR - 0x40;
        unpack_s16(v[8], ld(at));
        at += 8;
        do {
            const uint64_t d = ld(at);
            itof_lanes(v[9], v[8], XYZW, 1.0f / 32768.0f);
            --c;
            at += 8;
            unpack_s16(v[8], d);
            p += 0x40;
            sqc2(p, v[9]);
        } while (c > 0);
    }
    // the blend channels: per channel (+8 its weight, +0x10 its keys, 3 quadwords per joint,
    // +0x2c the joint's record) blended into the joints by weight
    for (uint32_t t9 = lwu(t7 + 8); t9 != 0;) {
        const uint32_t qwc = static_cast<uint32_t>(lhu(t9 + 2)) * 3;
        spr_dma_to(0x2000, lwu(t9 + 0x10), qwc);
        const uint32_t end = (qwc << 4) + SPR + 0x2000;
        qset_xy(v[21], sx(lwu(t9 + 8)));
        t9 = lwu(t9 + 0x1c);
        sub(fl, v[20], v[4], v[21], X);
        uint32_t r = SPR + 0x2000;
        do {
            const uint32_t j = lwu(r + 0x2c);
            r += 0x30;
            v[13] = lqc2(r - 0x30);
            v[14] = lqc2(r - 0x20);
            v[15] = lqc2(r - 0x10);
            v[10] = lqc2(j);
            v[11] = lqc2(j + 0x10);
            v[12] = lqc2(j + 0x20);
            mulbc(fl, v[10], v[10], v[20].v[0], XYZW);
            mulbc(fl, v[13], v[13], v[21].v[0], XYZW);
            mulbc(fl, v[11], v[11], v[20].v[0], XYZ);
            mulbc(fl, v[14], v[14], v[21].v[0], XYZ);
            mulbc(fl, v[12], v[12], v[20].v[0], XYZ);
            mulbc(fl, v[15], v[15], v[21].v[0], XYZ);
            blend_quats(fl, 10, 13);
            add(fl, v[11], v[11], v[14], XYZ);
            add(fl, v[12], v[12], v[15], XYZ);
            blend_quats_norm(fl, 10, 13);
            sqc2(j + 0x10, v[11]);
            sqc2(j + 0x20, v[12]);
            if (lwu(j + 0x1c) == 0) sw(j + 0x1c, lwu(r - 0x14));
            mulbc(fl, v[10], v[16], q(), XYZW);
            sqc2(j, v[10]);
        } while (r != end);
    }
    // the additive layers (+3: 0 composed on the pose, else blended by +0xc)
    v[19] = v[0];
    for (uint32_t t8 = lwu(t7 + 0xc); t8 != 0;) {
        const uint32_t r = t8;
        const uint32_t j = lwu(r + 4);
        const uint32_t mode = lbu(r + 3);
        v[8] = lqc2(j);
        v[10] = lqc2(j + 0x10);
        v[12] = lqc2(j + 0x20);
        v[9] = lqc2(r + 0x10);
        v[11] = lqc2(r + 0x20);
        v[13] = lqc2(r + 0x30);
        t8 = lwu(r + 8);
        if (mode == 0) {
            mul(fl, v[14], v[9], v[8], W);
            mul(fl, v[15], v[9], v[8], XYZ);
            mulbc(fl, v[16], v[9], v[8].v[3], XYZ);
            mulbc(fl, v[17], v[8], v[9].v[3], XYZ);
            opmula(fl, v[8], v[9]);  // the cross product
            opmsub(fl, v[18], v[9], v[8]);
            addabc(fl, v[15], v[15].v[1], X);
            maddbc(fl, v[15], v[4], v[15].v[2], X);
            add(fl, v[8], v[16], v[17], XYZ);
            add(fl, v[8], v[8], v[18], XYZ);
            subbc(fl, v[8], v[14], v[15].v[0], W);
            mul(fl, v[19], v[10], v[11], XYZ);
            add(fl, v[12], v[12], v[13], XYZ);
        } else {
            qset_xy(v[20], sx(lwu(r + 0xc)));
            subbc(fl, v[20], v[0], v[20].v[0], W);
            mulbc(fl, v[8], v[8], v[20].v[3], XYZW);
            mulbc(fl, v[9], v[9], v[20].v[0], XYZW);
            mulbc(fl, v[10], v[10], v[20].v[3], XYZ);
            mulbc(fl, v[11], v[11], v[20].v[0], XYZ);
            mulbc(fl, v[12], v[12], v[20].v[3], XYZ);
            mulbc(fl, v[13], v[13], v[20].v[0], XYZ);
            blend_quats(fl, 8, 9);
            add(fl, v[19], v[10], v[11], XYZ);
            add(fl, v[12], v[12], v[13], XYZ);
            blend_quats_norm(fl, 8, 9);
            mulbc(fl, v[8], v[16], q(), XYZW);
        }
        sqc2(j, v[8]);
        sqc2(j + 0x10, v[19]);
        sqc2(j + 0x20, v[12]);
    }
    // the bind matrices (+0x1c) to the scratchpad's 0x1c00; the joints' matrices from their
    // quaternion, scale and translation under their parent's
    sw((un << 6) + SPR + 0x1bfc, 0xffffffffu);  // (the transfer's last word: the original waits on it)
    spr_dma_to(0x1c00, lwu(t7 + 0x1c), un << 2);
    const uint32_t last = (un << 6) + SPR;
    for (uint32_t a3 = SPR;;) {
        v[10] = lqc2(a3);
        v[16] = v[1];
        add(fl, v[11], v[10], v[10], XYZW);
        v[17] = v[2];
        v[18] = v[3];
        v[19] = v[0];
        mulbc(fl, v[12], v[11], v[10].v[3], XYZ);
        mulbc(fl, v[13], v[11], v[10].v[0], XYZ);
        mulbc(fl, v[14], v[11], v[10].v[1], YZ);
        mulbc(fl, v[15], v[11], v[10].v[2], Z);
        addbc(fl, v[17], v[0], v[12].v[2], X);
        subbc(fl, v[18], v[0], v[12].v[1], X);
        addbc(fl, v[18], v[0], v[12].v[0], Y);
        subbc(fl, v[16], v[16], v[14].v[1], X);
        subbc(fl, v[17], v[17], v[13].v[0], Y);
        subbc(fl, v[18], v[18], v[13].v[0], Z);
        subbc(fl, v[16], v[13], v[12].v[2], Y);
        addbc(fl, v[16], v[13], v[12].v[1], Z);
        subbc(fl, v[17], v[14], v[12].v[0], Z);
        addbc(fl, v[17], v[17], v[13].v[1], X);
        addbc(fl, v[18], v[18], v[13].v[2], X);
        addbc(fl, v[18], v[18], v[14].v[2], Y);
        a3 += 0x40;
        subbc(fl, v[16], v[16], v[15].v[2], X);
        const uint32_t scaled = lwu(a3 - 0x24);
        subbc(fl, v[17], v[17], v[15].v[2], Y);
        const uint32_t parent = lwu(a3 - 0x14);
        subbc(fl, v[18], v[18], v[14].v[1], Z);
        v[8] = lqc2(a3 - 0x30);
        v[9] = lqc2(a3 - 0x20);
        if (scaled != 0) {
            mulbc(fl, v[16], v[16], v[8].v[0], XYZ);
            mulbc(fl, v[17], v[17], v[8].v[1], XYZ);
            mulbc(fl, v[18], v[18], v[8].v[2], XYZ);
        }
        v[12] = v[1];
        if (parent == 0) {
            v[13] = v[2];
            v[14] = v[3];
            v[15] = v[0];
        } else {
            v[12] = lqc2(parent);
            v[13] = lqc2(parent + 0x10);
            v[14] = lqc2(parent + 0x20);
            v[15] = lqc2(parent + 0x30);
        }
        add(fl, v[19], v[19], v[9], XYZ);
        rotate(fl, v[20], 12, v[16], XYZW);
        rotate(fl, v[21], 12, v[17], XYZW);
        rotate(fl, v[22], 12, v[18], XYZW);
        transform(fl, v[23], 12, v[19], v[0].v[3]);
        sqc2(a3 - 0x40, v[20]);
        sqc2(a3 - 0x30, v[21]);
        sqc2(a3 - 0x20, v[22]);
        sqc2(a3 - 0x10, v[23]);
        if (a3 == last) break;
    }
    // the record scales (lerped by t with a second key) on the joints' rows
    for (uint32_t r = lwu(t5 - 4); r != 0;) {
        const uint32_t m = ((r - t5) << 1) + SPR;
        v[1] = lqc2(r);
        if (key2 != 0) {
            v[8] = lqc2(r + 0x10);
            mulabc(fl, v[1], v[6].v[0], XYZ);
            maddbc(fl, v[1], v[8], v[7].v[0], XYZ);
        }
        v[2] = lqc2(m);
        v[3] = lqc2(m + 0x10);
        v[4] = lqc2(m + 0x20);
        mulbc(fl, v[2], v[2], v[1].v[0], XYZ);
        r = lwu(r + 0xc);
        mulbc(fl, v[3], v[3], v[1].v[1], XYZ);
        mulbc(fl, v[4], v[4], v[1].v[2], XYZ);
        sqc2(m, v[2]);
        sqc2(m + 0x10, v[3]);
        sqc2(m + 0x20, v[4]);
    }
    // each joint's matrix times its bind matrix
    uint32_t b = SPR + 0x1c00, m = SPR;
    do {
        v[15] = lqc2(b + 0x30);
        v[14] = lqc2(b + 0x20);
        v[13] = lqc2(b + 0x10);
        v[12] = lqc2(b);
        v[20] = lqc2(m);
        v[21] = lqc2(m + 0x10);
        v[22] = lqc2(m + 0x20);
        v[23] = lqc2(m + 0x30);
        transform(fl, v[19], 20, v[15], v[0].v[3]);
        rotate(fl, v[16], 20, v[12], XYZW);
        rotate(fl, v[17], 20, v[13], XYZW);
        rotate(fl, v[18], 20, v[14], XYZW);
        sqc2(m, v[16]);
        sqc2(m + 0x10, v[17]);
        sqc2(m + 0x20, v[18]);
        b += 0x40;
        sqc2(m + 0x30, v[19]);
        m += 0x40;
    } while (m != last);
}

}  // namespace rac1
