// Ported overlay code: the moby pass's draw loop (see moby_draw.h). Hand-written assembly kept in
// its control flow (labels by address): its VU0 macro ops on the shared VU0 register file with
// the VU float rules, the 128-bit GPR halves it parks values in (s5 / s6 / t6 / t7 / sp high)
// as their own variables, its scratchpad DMA transfers done at their kick.
//
// Scratchpad while it runs: 0x2000 the light block (toSPR from 0x17fec0: 0x2000 + 0x40 n the
// directional light sets, 0x2400 the point lights, packed in place), 0x2500 the point lights
// reaching the moby, 0x2600 per-class LOD counters, 0x2800 / 0x2c00 the skinning record's two
// buffers, 0x3000 the deferred mobys (0x10 each), 0x3300 the shadow table (0x1aeb80), 0x3400 the
// sub-mesh records (to 0x1ac200), 0x3800 the visibility masks (0x173d00), 0x3a00 per-group
// nearest depths, 0x3e80 / 0x3f00 / 0x3f40 the chain templates.
#include <game/ovl/moby_draw.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_macro.h>
#include <game/ovl/vu0_regs.h>

namespace rac1 {

// orig: (host) the MRP scene state's observer of the draw loop (mrp_scene.h); null: nothing
MobyDrawObserver g_moby_draw_observer = nullptr;

namespace {

constexpr uint32_t SPR = 0x70000000u;
using vu0m::Vf;
using namespace vu0m;

// orig: (helper) a 128-bit GPR
struct Q128 {
    uint64_t lo = 0, hi = 0;
};
const Q128 kZero;
// orig: (helper) LQ
Q128 lq(uint32_t a) { return Q128{ld(a), ld(a + 8)}; }
// orig: (helper) SQ
void sq(uint32_t a, const Q128& v) {
    sd(a, v.lo);
    sd(a + 8, v.hi);
}
// orig: (helper) a 32-bit op's result as the 64-bit register holds it
uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// orig: (helper) BLTZ / BGEZ on a 64-bit register
bool neg(uint64_t v) { return static_cast<int64_t>(v) < 0; }
int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }  // PMINW's word 0

// orig: (helper) the DMA tag + VIF FLUSHA closing a chain part
void chain_flush(uint32_t& sp) {
    sq(sp, kZero);
    sw(sp, 0x10000000u);
    sw(sp + 0xc, 0x13000000u);
    sp += 0x10;
}
// orig: (helper) the colour fade's GS state: the template 0x3f00 / 0x3f10 and ALPHA (0x3f20) or
// FBA / TEST (0x3f30)
void chain_gs_state(uint32_t& sp) {
    chain_flush(sp);
    sq(sp, lq(SPR + 0x3f00));
    sq(sp + 0x10, lq(SPR + 0x3f10));
}

}  // namespace

// orig: 0x002559c0
uint32_t moby_draw_loop(uint32_t a0_in, uint32_t a1_in, int32_t a2_in, uint32_t a3_in) {
    LocalFlags fl(regs());  // (host: the ops' flags in locals, the VU0's written back at the return: the same values)
    Vf* const v = vf();
    uint32_t s8 = a0_in - 0x100;
    const uint32_t s5 = (static_cast<uint32_t>(a2_in) << 8) + a0_in;  // the list's end
    uint32_t t9 = a1_in;
    int32_t ra = a3_in != 0 ? 0xc00 : 0;
    spr_dma_to(0x2000, OV(0x0017fec0), 0x50);  // the light block
    for (uint32_t p = SPR + 0x2600; p != SPR + 0x2800; p += 0x10) sq(p, kZero);
    for (uint32_t i = 0; i < 0x80; i += 0x10) sq(SPR + 0x3800 + i, lq(OV(0x00173d00) + i));
    for (uint32_t i = 0; i < 0x40; i += 0x10) sq(SPR + 0x3300 + i, lq(OV(0x001aeb80) + i));
    uint32_t sp = lwu(OV(0x001611c0));
    uint32_t t7_hi = SPR + 0x3400;  // the sub-mesh records' end
    uint64_t t7 = 0;
    const Q128 tmpl = lq(OV(0x00160810));
    const uint32_t sp_hi = lwu(OV(0x001611c8));  // the packet buffer's limit
    sq(SPR + 0x3f40, tmpl);
    v[11].v[0] = vu0m::add(v[0].v[0], v[0].v[3]);
    v[11].v[1] = vu0m::sub(v[0].v[1], v[0].v[3]);
    const uint32_t s7 = lwu(OV(0x0015fff0));
    int32_t s4 = 1;
    v[1].v[0] = u2f(0x44800000u);  // 1024 (the other lanes: what the register held)
    v[1].v[1] = 0.0f;
    v[1].v[2] = u2f(static_cast<uint32_t>(tmpl.hi));
    v[1].v[3] = u2f(static_cast<uint32_t>(tmpl.hi >> 32));
    uint32_t s5_hi = SPR + 0x3000;  // the deferred mobys' end
    v[24] = vu0m::lqc2(OV(0x00166dc0));
    v[25] = vu0m::lqc2(OV(0x00166c80));
    v[26] = vu0m::lqc2(OV(0x00166c90));
    v[27] = vu0m::lqc2(OV(0x00166ca0));
    v[28] = vu0m::lqc2(OV(0x00166cc0));
    v[29] = vu0m::lqc2(OV(0x00166cd0));
    v[30] = vu0m::lqc2(OV(0x00166ce0));
    v[31] = vu0m::lqc2(OV(0x00166cf0));
    mulbc(fl, v[24], v[24], v[1].v[0], XYZ);
    mulbc(fl, v[24], v[0], v[1].v[0], W);
    uint32_t t8 = SPR + 0x2c00;
    v[19] = vu0m::lqc2(OV(0x0016cae0));
    v[20] = vu0m::lqc2(OV(0x0016caf0));
    v[21] = vu0m::lqc2(OV(0x0016cc10));
    v[22] = vu0m::lqc2(OV(0x0016cc20));
    v[23] = vu0m::lqc2(OV(0x0016cc30));
    for (uint32_t i = 0; i < 0x40; i += 0x10) sq(SPR + 0x3f00 + i, lq(OV(0x001c2ec0) + i));
    for (uint32_t i = 0; i < 0x70; i += 0x10) sq(SPR + 0x3e80 + i, lq(OV(0x00160870) + i));
    // the chain's head: MSCAL 0, the VU1 set up (ref 0x1c2480), the view matrices twice
    sq(sp, kZero);
    sw(sp, 0x10000000u);
    sh(sp + 0xe, 0x1400);
    sh(sp + 0xc, 0);
    sp += 0x10;
    sq(sp, kZero);
    sw(sp + 4, OV(0x001c2480));
    sw(sp, 0x30000004u);
    sp += 0x10;
    {
        const Q128 t = lq(SPR + 0x3f40);
        sq(sp, t);
        sq(sp + 0x70, t);
        sb(sp, 6);
        sb(sp + 0xe, 6);
        sb(sp + 0x70, 6);
        sb(sp + 0x7e, 6);
        sh(sp + 0xc, 0x123);
        sh(sp + 0x7c, 0x287);
        sp += 0xe0;
        for (uint32_t i = 0; i < 0x60; i += 0x10) {
            const Q128 m = lq(OV(0x0016ca40) + i);
            sq(sp - 0xd0 + i, m);
            sq(sp - 0x60 + i, m);
        }
    }
    // the point lights: the ones set (+0x10 non-zero) packed, w = 1 / (radius / 1024) and the
    // position scaled by 1024
    {
        const Q128 h = lq(OV(0x001608d0));  // what the register's high half held
        v[3] = Vf{{u2f(0x44800000u), 0.0f, u2f(static_cast<uint32_t>(h.hi)), u2f(static_cast<uint32_t>(h.hi >> 32))}};
        v[4] = Vf{{u2f(0x3a800000u), 0.0f, u2f(static_cast<uint32_t>(h.hi)), u2f(static_cast<uint32_t>(h.hi >> 32))}};
    }
    uint32_t s6_hi = 0;  // the point lights' count
    {
        uint32_t at = SPR + 0x2400, v1 = SPR + 0x2400;
        while (at != SPR + 0x2500) {
            const uint64_t a1 = ld(at + 0x10);
            at += 0x20;
            if (a1 == 0) continue;
            v[1] = vu0m::lqc2(at - 0x10);
            v[2] = vu0m::lqc2(at - 0x20);
            q() = vu0m::div(v[4].v[0], v[1].v[3]);
            mulbc(fl, v[1], v[1], v[3].v[0], XYZW);
            vu0m::sqc2(v1 + 0x10, v[1]);
            mulbc(fl, v[2], v[0], q(), W);
            vu0m::sqc2(v1, v[2]);
            ++s6_hi;
            v1 += 0x20;
        }
    }

    // The registers the loop carries (named as the original's).
    uint64_t at = 0, v0 = 0, v1 = 0, a0 = 0, a1 = 0, a2 = 0, a3 = 0, t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0;
    uint64_t t6 = 0, s3 = 0;
    uint32_t t6_hi = 0, s1 = 0, s0 = 0;
    uint32_t chain_begin = 0;  // (host) where the moby's chain parts begin, for the observer
    int32_t s2 = 0;
    Q128 t1q, t2q, t3q, t4q, t5q, t6q;
    Vf t3v;  // t3 as QMFC2 of vf1 left it (the deferred path)

L_255d18:
    v0 = sx(static_cast<uint32_t>(lh(s8 + 0x132)));
    s8 += 0x100;
    v[1] = vu0m::lqc2(s8);
    if (s8 == s5) goto L_2566e0;
    at = sx(static_cast<uint32_t>(static_cast<int8_t>(lbu(s8 + 0x20))));
    v[4].v[0] = 0.0f;  // (PMINW v0, s7) << 42: the draw distance << 10 in y
    v[4].v[1] = u2f(static_cast<uint32_t>(min32(static_cast<int32_t>(v0), static_cast<int32_t>(s7))) << 10);
    sub(fl, v[2], v[1], v[24], XYZ);
    if (neg(at)) goto L_2566d0;
    mulbc(fl, v[3], v[11], v[1].v[3], XY);
    v[19].v[1] = itof(v[4].v[1], 1.0f);
    a2 = lbu(s8 + 0x37);
    rotate(fl, v[2], 25, v[2], XYZ);
    a3 = lbu(s8 + 0x36);
    a2 = lbu(SPR + static_cast<uint32_t>(a2) + 0x3800);
    subbc(fl, v[19], v[19], v[1].v[3], Y);
    t0 = lbu(s8 + 0x31);
    mulbc(fl, v[8], v[22], v[1].v[3], XY);
    a3 &= a2;
    t6 = lhu(s8 + 0x34);
    if (a3 == 0) goto L_2566c0;
    addbc(fl, v[1], v[3], v[2].v[2], XY);
    a3 = lbu(s8 + 0x22);
    mulbc(fl, v[5], v[20], v[2].v[2], XY);
    v0 = t6 & 0x81;
    lanes(v[2], XY, [&](int i) { return u2f(f2u(v[2].v[i]) & 0x7fffffffu); });
    if (v0 != 0) goto L_2566c0;
    mulbc(fl, v[7], v[8], v[22].v[2], XY);
    t1 = a3 << 1;
    sub(fl, v[3], v[19], v[1], XY);
    a3 = (a3 << 2) + SPR;
    mul(fl, v[9], v[5], v[21], XY);
    sub(fl, v[6], v[2], v[8], XY);
    add(fl, v[8], v[2], v[7], XY);
    a1 = 0x80000000ull;
    sub(fl, v[4], v[1], v[23], XY);
    v[1].v[2] = vmax(v[0].v[2], v[2].v[2]);
    s3 = 0xa;
    at = qlo(v[3]);
    a2 = t6 & static_cast<uint64_t>(static_cast<int64_t>(ra));
    sub(fl, v[7], v[5], v[6], XY);
    sub(fl, v[8], v[9], v[8], XY);
    v0 = at << 32;
    a0 = qlo(v[4]);
    if (neg(at)) goto L_2566c0;  // above the draw distance
    {
        const float y = v[1].v[1], z = v[1].v[2];  // VMR32.xy
        v[1].v[0] = y;
        v[1].v[1] = z;
    }
    if (!neg(v0)) goto L_2566c0;  // behind the camera
    v0 = qlo(v[7]);
    if (a2 != 0) goto L_256890;
    v1 = qlo(v[8]);
    a0 ^= a1;
    if (neg(v0)) goto L_2566c0;  // off the screen's sides
    v0 <<= 32;
    ftoi0(v[1], XY);
    if (neg(v0)) goto L_2566c0;
    v1 |= a0;
    ftoi0(v[3], Y);
    if (neg(v1)) goto L_255e38;  // crossing the guard band: the clipping program
    v1 <<= 32;
    if (neg(v1)) goto L_255e38;
    s3 = 0xe;
L_255e38:
    if (t0 == 0) sb(s8 + 0x31, 1);
L_255e48:
    chain_begin = sp;
    at = sx(static_cast<uint32_t>(lw(static_cast<uint32_t>(a3) + 0x3a00)));
    s2 = 2;
    v[18] = vu0m::lqc2(s8 + 0x10);
    a0 = sx(static_cast<uint32_t>(lw(s8 + 0x2c)));
    s3 |= 0x14000000u;
    v0 = qlo(v[1]);
    mulbc(fl, v[18], v[18], v[24].v[3], XYZ);
    t8 ^= 0x400;
    qset_xy(v[10], a0);
    sw(static_cast<uint32_t>(a3) + 0x3a00, static_cast<uint32_t>(min32(static_cast<int32_t>(v0), static_cast<int32_t>(at))));
    t2 = ld(s8 + 0x38);
    at = qlo(v[3]);
    sub(fl, v[18], v[18], v[24], XYZ);
    v1 = lbu(s8 + 0x23);
    at >>= 39;
    at = static_cast<uint64_t>(min32(static_cast<int32_t>(at), 0x80));
    {
        const uint32_t lo = static_cast<uint32_t>(static_cast<int32_t>(at) * static_cast<int32_t>(v1));
        t6 &= 0x208;
        for (uint32_t i = 0x40; i <= 0xa0; i += 0x10) sq(t8 + i, kZero);
        at = static_cast<int64_t>(at) < 0x80 ? 1 : 0;
        t6 |= at << 3;
        // the colour: +0x3c's bytes with the alpha scaled by the fade, as floats 65536 + byte
        const uint32_t c = static_cast<uint32_t>(t2 >> 32) | ((lo >> 7) << 24);
        uint32_t w[4];
        for (uint32_t i = 0; i < 4; ++i) w[i] = ((c >> (8 * i)) & 0xff) | 0x47800000u;
        Q128 cq;
        cq.lo = w[0] | static_cast<uint64_t>(w[1]) << 32;
        cq.hi = w[2] | static_cast<uint64_t>(w[3]) << 32;
        sq(t8 + 0xb0, cq);
    }
    at = sx(sp_hi - sp);
    v[15] = vu0m::lqc2(s8 + 0xc0);
    v[16] = vu0m::lqc2(s8 + 0xd0);
    v[17] = vu0m::lqc2(s8 + 0xe0);
    if (neg(at)) goto L_2566e0;  // the packet buffer full
    // the inverse rotation (negated transpose) for the lights, the model-to-view matrix
    v[12].v[0] = vu0m::sub(v[0].v[0], v[15].v[0]);
    v[13].v[1] = vu0m::sub(v[0].v[1], v[16].v[1]);
    v[14].v[2] = vu0m::sub(v[0].v[2], v[17].v[2]);
    v[13].v[0] = vu0m::sub(v[0].v[0], v[15].v[1]);
    v[14].v[0] = vu0m::sub(v[0].v[0], v[15].v[2]);
    v[12].v[1] = vu0m::sub(v[0].v[1], v[16].v[0]);
    v[14].v[1] = vu0m::sub(v[0].v[1], v[16].v[2]);
    v[12].v[2] = vu0m::sub(v[0].v[2], v[17].v[0]);
    v[13].v[2] = vu0m::sub(v[0].v[2], v[17].v[1]);
    mulbc(fl, v[15], v[15], v[10].v[0], XYZ);
    mulbc(fl, v[16], v[16], v[10].v[0], XYZ);
    mulbc(fl, v[17], v[17], v[10].v[0], XYZ);
    transform(fl, v[15], 28, v[15], v[15].v[3]);
    transform(fl, v[16], 28, v[16], v[16].v[3]);
    transform(fl, v[17], 28, v[17], v[17].v[3]);
    transform(fl, v[18], 28, v[18], v[0].v[3]);
    s1 = lwu(s8 + 0x24);
    t7 = lbu(s8 + 0x73);
    t1 = t1 + SPR + 0x2600;
    at = lbu(s8 + 0x72);
    t0 = qlo(v[1]);
    if (t7 != 0) goto L_256b20;
L_255fd0:
    v1 = sx(static_cast<uint32_t>(lw(s8 + 0x54)));
    {
        int32_t d = min32(static_cast<int32_t>(t0), 0x30000);
        if (d < 0) d = 0;
        t0 = sx(static_cast<uint32_t>(d));  // the depth, 0..0x30000
    }
    a0 = lwu(s8 + 0x68);
    at = sx(static_cast<uint32_t>(at) << 10);
    a1 = lwu(s8 + 0x6c);
    a2 = lwu(s8 + 0x60);
    a3 = lwu(s8 + 0x64);
    at = sx(static_cast<uint32_t>(t0) - static_cast<uint32_t>(at));
    sw(t8 + 4, static_cast<uint32_t>(v1));
    sw(t8 + 8, static_cast<uint32_t>(a2));
    sw(t8 + 0xc, static_cast<uint32_t>(a3));
    t3 = lwu(s8 + 0x50);
    if (static_cast<int64_t>(at) > 0) {  // past +0x72 << 10: the low detail model
        t0 = lbu(static_cast<uint32_t>(t1) + 1) + 1u;
        at = lbu(s1 + 4);
        v0 = lbu(s1 + 5);
        a2 = lbu(s1 + 9);
        sb(static_cast<uint32_t>(t1) + 1, static_cast<uint32_t>(t0));
        t6_hi = 1;
    } else {
        t0 = lbu(static_cast<uint32_t>(t1)) + 1u;
        t6_hi = 0;
        at = 0;
        v0 = lbu(s1 + 4);
        a2 = lbu(s1 + 8);
        sb(static_cast<uint32_t>(t1), static_cast<uint32_t>(t0));
    }
    t4 = (t3 >> 16) & 0xff;
    t0 = lwu(s1 + 0x18);
    t1 = lwu(s1 + 0x14);
    t5 = t3 >> 24;
    s1 = lwu(s1);
    sw(t8, static_cast<uint32_t>(v0));
    at <<= 4;
    sw(t8 + 0x10, static_cast<uint32_t>(a2));
    v0 <<= 4;
    sw(t8 + 0x14, static_cast<uint32_t>(t7));
    s1 += static_cast<uint32_t>(at);
    sw(t8 + 0x18, static_cast<uint32_t>(t0));
    s0 = s1 + static_cast<uint32_t>(v0);
    sw(t8 + 0x1c, static_cast<uint32_t>(t1));
    if (a2 != 0) {  // animated: the two animation records +0x68 / +0x6c
        a2 = t3 & 0xff;
        a3 = t3 >> 8;
        if (t4 == t5) {
            a3 &= 0xff;
            a2 += 1;
            if (a2 == a3) sh(t8 + 0x12, 0x400);
        }
        a2 = lhu(static_cast<uint32_t>(a0) + 6);
        a3 = lwu(static_cast<uint32_t>(a0) + 8);
        t0 = lwu(static_cast<uint32_t>(a0) + 0xc);
        a0 += 0x10;
        sw(t8 + 0x20, static_cast<uint32_t>(a0));
        sw(t8 + 0x24, static_cast<uint32_t>(a2));
        sw(t8 + 0x28, static_cast<uint32_t>(a3));
        sw(t8 + 0x2c, static_cast<uint32_t>(t0));
        a2 = lhu(static_cast<uint32_t>(a1) + 6);
        a3 = lwu(static_cast<uint32_t>(a1) + 8);
        t0 = lwu(static_cast<uint32_t>(a1) + 0xc);
        a1 += 0x10;
        sw(t8 + 0x30, static_cast<uint32_t>(a1));
        sw(t8 + 0x34, static_cast<uint32_t>(a2));
        sw(t8 + 0x38, static_cast<uint32_t>(a3));
        sw(t8 + 0x3c, static_cast<uint32_t>(t0));
    }
    t5 = lhu(s8 + 0x34) & 0x10;
    v1 = (static_cast<uint32_t>(t2) >> 16) & 0xff;
    at = ((t2 & 0xff) << 6) + SPR;
    if (v1 == 0) {  // one directional light set (+0x38)
        const uint32_t l = static_cast<uint32_t>(at);
        v[1] = vu0m::lqc2(l + 0x2010);
        v[2] = vu0m::lqc2(l + 0x2030);
        rotate(fl, v[1], 12, v[1], XYZW);
        v[3] = vu0m::lqc2(l + 0x2000);
        v[4] = vu0m::lqc2(l + 0x2020);
        vu0m::sqc2(t8 + 0x70, v[3]);
        rotate(fl, v[2], 12, v[2], XYZW);
        vu0m::sqc2(t8 + 0x80, v[4]);
        sw(t8 + 0xa0, static_cast<uint32_t>(lw(l + 0x200c)));
        sw(t8 + 0xa4, static_cast<uint32_t>(lw(l + 0x202c)));
        sw(t8 + 0x7c, 0);
        sw(t8 + 0x8c, 0);
        sw(t8 + 0x40, f2u(v[1].v[0]));
        sw(t8 + 0x50, f2u(v[1].v[1]));
        sw(t8 + 0x60, f2u(v[1].v[2]));
        sw(t8 + 0x44, f2u(v[2].v[0]));
        sw(t8 + 0x54, f2u(v[2].v[1]));
        sw(t8 + 0x64, f2u(v[2].v[2]));
    } else {  // two sets blended by +0x3a / 4096, the directions renormalised
        v1 <<= 4;
        v0 = ((t2 & 0xff00) >> 2) + SPR;
        a0 = 0x1000 - v1;
        addbc(fl, v[10], v[0], v[0].v[3], XYZ);
        v[9] = Vf{{u2f(static_cast<uint32_t>(v1)), 0.0f, u2f(static_cast<uint32_t>(a0)), 0.0f}};
        const uint32_t l = static_cast<uint32_t>(at), m = static_cast<uint32_t>(v0);
        v[1] = vu0m::lqc2(l + 0x2010);
        v[2] = vu0m::lqc2(l + 0x2030);
        v[3] = vu0m::lqc2(l + 0x2000);
        v[4] = vu0m::lqc2(l + 0x2020);
        v[9].v[0] = itof(v[9].v[0], 1.0f / 4096.0f);
        v[9].v[2] = itof(v[9].v[2], 1.0f / 4096.0f);
        v[5] = vu0m::lqc2(m + 0x2010);
        v[6] = vu0m::lqc2(m + 0x2030);
        v[7] = vu0m::lqc2(m + 0x2000);
        v[8] = vu0m::lqc2(m + 0x2020);
        mulbc(fl, v[1], v[1], v[9].v[2], XYZ);
        mulbc(fl, v[2], v[2], v[9].v[2], XYZ);
        mulbc(fl, v[3], v[3], v[9].v[2], XYZW);
        mulbc(fl, v[4], v[4], v[9].v[2], XYZW);
        mulbc(fl, v[5], v[5], v[9].v[0], XYZ);
        mulbc(fl, v[6], v[6], v[9].v[0], XYZ);
        mulbc(fl, v[7], v[7], v[9].v[0], XYZW);
        mulbc(fl, v[8], v[8], v[9].v[0], XYZW);
        add(fl, v[1], v[1], v[5], XYZ);
        add(fl, v[2], v[2], v[6], XYZ);
        add(fl, v[3], v[3], v[7], XYZW);
        add(fl, v[4], v[4], v[8], XYZW);
        mul(fl, v[5], v[1], v[1], XYZ);
        mul(fl, v[6], v[2], v[2], XYZ);
        vu0m::sqc2(t8 + 0x70, v[3]);
        vu0m::sqc2(t8 + 0x80, v[4]);
        addabc(fl, v[5], v[5].v[1], X);
        maddbc(fl, v[5], v[10], v[5].v[2], X);
        addabc(fl, v[6], v[6].v[1], X);
        maddbc(fl, v[6], v[10], v[6].v[2], X);
        q() = vrsqrt(v[0].v[3], v[5].v[0]);
        rotate(fl, v[1], 12, v[1], XYZW);
        const uint32_t c0 = lwu(t8 + 0x7c), c1 = lwu(t8 + 0x8c);
        sw(t8 + 0x7c, 0);
        sw(t8 + 0x8c, 0);
        sw(t8 + 0xa0, c0);
        sw(t8 + 0xa4, c1);
        mulbc(fl, v[1], v[1], q(), XYZ);
        q() = vrsqrt(v[0].v[3], v[6].v[0]);
        rotate(fl, v[2], 12, v[2], XYZW);
        sw(t8 + 0x40, f2u(v[1].v[0]));
        sw(t8 + 0x50, f2u(v[1].v[1]));
        sw(t8 + 0x60, f2u(v[1].v[2]));
        mulbc(fl, v[2], v[2], q(), XYZ);
        sw(t8 + 0x44, f2u(v[2].v[0]));
        sw(t8 + 0x54, f2u(v[2].v[1]));
        sw(t8 + 0x64, f2u(v[2].v[2]));
    }
    // L_2562f8: the point lights reaching the moby, summed into a third direction / colour
    v[1] = vu0m::lqc2(s8);
    v1 = s6_hi;
    t8 += 0xc0;
    if (v1 == 0) goto L_2564c0;
    {
        v[2] = vu0m::lqc2(SPR + 0x2410);
        uint32_t lp = SPR + 0x2400, out = SPR + 0x2500;
        uint32_t n = 0;
        sub(fl, v[3], v[2], v[1], XYZ);
        addbc(fl, v[6], v[0], v[0].v[3], XYZ);
        subbc(fl, v[7], v[0], v[0].v[3], XYZ);
        for (;;) {
            mul(fl, v[3], v[3], v[3], XYZ);
            if (v1 == 0) break;
            mul(fl, v[4], v[2], v[2], W);
            lp += 0x20;
            v[2] = vu0m::lqc2(lp + 0x10);
            --v1;
            addabc(fl, v[3], v[3].v[0], Y);
            maddabc(fl, v[6], v[3].v[2], Y);
            maddbc(fl, v[5], v[7], v[4].v[3], Y);
            maddbc(fl, v[8], v[0], v[0].v[0], Y);
            sub(fl, v[3], v[2], v[1], XYZ);
            const uint64_t d = qlo(v[5]);
            q() = vsqrt(v[8].v[1]);
            if (static_cast<int64_t>(d) > 0) continue;  // out of its radius
            v[8] = vu0m::lqc2(lp - 0x10);
            v[9] = vu0m::lqc2(lp - 0x20);
            sub(fl, v[8], v[1], v[8], XYZ);
            mulbc(fl, v[5], v[0], q(), W);
            mulbc(fl, v[9], v[9], q(), W);
            q() = vu0m::div(v[0].v[3], v[5].v[3]);
            sub(fl, v[9], v[0], v[9], W);
            rotate(fl, v[8], 12, v[8], XYZW);
            mulbc(fl, v[9], v[9], v[9].v[3], XYZ);
            mulbc(fl, v[8], v[8], q(), XYZ);
            vu0m::sqc2(out, v[9]);
            ++n;
            vu0m::sqc2(out + 0x10, v[8]);
            out += 0x20;
        }
        if (n == 1) {
            const Q128 c = lq(SPR + 0x2500), d = lq(SPR + 0x2510);
            sq(t8 - 0x30, c);
            sw(t8 - 0x24, 0);
            sw(t8 - 0x78, static_cast<uint32_t>(d.lo));
            sw(t8 - 0x68, static_cast<uint32_t>(d.lo >> 32));
            sw(t8 - 0x58, static_cast<uint32_t>(d.hi));
        } else if (n != 0) {
            uint32_t p = SPR + 0x2500;
            add(fl, v[1], v[0], v[0], XYZ);
            lanes(acc(), XYZ, [&](int i) { return vu0m::mul(v[0].v[i], v[0].v[i]); });
            do {
                v[3] = vu0m::lqc2(p);
                v[4] = vu0m::lqc2(p + 0x10);
                add(fl, v[1], v[1], v[3], XYZ);
                p += 0x20;
                maddabc(fl, v[4], v[3].v[3], XYZ);
                --n;
                maddbc(fl, v[2], v[0], v[0].v[0], XYZ);
            } while (n != 0);
            mul(fl, v[3], v[2], v[2], XYZ);
            addabc(fl, v[3], v[3].v[1], X);
            maddbc(fl, v[3], v[6], v[3].v[2], X);
            q() = vrsqrt(v[0].v[3], v[3].v[0]);
            mulbc(fl, v[2], v[2], q(), XYZ);
            vu0m::sqc2(t8 - 0x30, v[1]);
            sw(t8 - 0x24, 0);
            sw(t8 - 0x78, f2u(v[2].v[0]));
            sw(t8 - 0x68, f2u(v[2].v[1]));
            sw(t8 - 0x58, f2u(v[2].v[2]));
        }
    }
L_2564c0:
    t1q = lq(SPR + 0x3f40);
    if (t5 != 0) goto L_256ab0;
L_2564c8:
    t2q = lq(SPR + 0x3e80);
    t3q = lq(SPR + 0x3e90);
    t4q = lq(SPR + 0x3ec0);
    t5q = lq(SPR + 0x3ed0);
    // the chain, a part per packet of the class's mesh (s1..s0, 0x10 each): the matrices (the
    // first two parts, VU1 0x129 / 0x28d by turns), the vertex data references, the program call
    do {
        s4 = -s4;
        if (s2 > 0) {
            sq(sp, t1q);
            --s2;
            vu0m::sqc2(sp + 0x10, v[15]);
            vu0m::sqc2(sp + 0x20, v[16]);
            vu0m::sqc2(sp + 0x30, v[17]);
            vu0m::sqc2(sp + 0x40, v[18]);
            sh(sp + 0xc, s4 > 0 ? 0x28d : 0x129);
            sp += 0x50;
        }
        {
            const uint32_t n = lwu(s1 + 8), d = lwu(s1 + 0xc);
            uint32_t p = sp + 0x20;
            sq(sp, t2q);
            sq(sp + 0x10, t3q);
            sw(t8, (n << 4) | (d & 0xff));
            sw(t8 + 4, p);
            sb(sp + 0xe, d >> 24);
            sb(sp + 0x1e, d >> 24);
            sh(sp, (d >> 8) & 0xff);
            sh(sp + 0x10, (d >> 16) & 0xff);
            t8 += 8;
            p += ((d >> 16) & 0xff) << 4;
            sw(sp + 4, p);
            p += ((d >> 8) & 0xff) << 4;
            sw(sp + 0x14, p);
            sp = p;
        }
        {
            const uint64_t r = ld(s1);
            s1 += 0x10;
            sq(sp, t4q);
            sp += 0x10;
            sw(sp - 0xc, static_cast<uint32_t>(r));
            sh(sp - 0x10, static_cast<uint32_t>(r >> 32));
        }
        if (s2 > 0 && t6 != 0) {  // the first part of a faded / alpha-tested moby: its GS state
            chain_gs_state(sp);
            Q128 g;
            if ((t6 & 0x200) != 0) {
                g = lq(SPR + 0x3f20);
                g.lo = 0x48;
            } else {
                g = lq(SPR + 0x3f30);
                g.lo = sx(static_cast<uint32_t>(g.lo) + (static_cast<uint32_t>(t6) << 4) - 0x600);
            }
            sq(sp + 0x20, g);
            sp += 0x30;
        }
        sq(sp, t5q);
        sp += 0x10;
        sh(sp - 4, static_cast<uint32_t>(s3));
    } while (s1 != s0);
    if (t6 != 0) {  // the GS state put back
        chain_gs_state(sp);
        const Q128 g = (t6 & 0x200) != 0 ? lq(SPR + 0x3f20) : lq(SPR + 0x3f30);
        sq(sp + 0x20, g);
        sp += 0x30;
    }
    sd(t8, 0);
    if (t7 != 0) goto L_256b50;
    t8 += 0x10;
L_256668: {
    // the skinning record to the ring (fromSPR)
    uint32_t n = t8 & 0x3ff;
    sd(t8 - 8, 0);
    t8 &= 0x2c00;
    n >>= 4;
    spr_dma_from(t9, t8, n);
    if (g_moby_draw_observer != nullptr) g_moby_draw_observer(s8, chain_begin, sp, t9, SPR + t8, n, t6_hi, ra < 0 ? 1u : 0u);  // (host)
    t9 += n << 4;
    t8 += SPR;
}
    if (ra >= 0) goto L_255d18;
    goto L_256840;

L_2566c0:  // not drawn
    if (t0 != 0) sb(s8 + 0x31, 0);
    goto L_255d18;
L_2566d0:
    if (static_cast<int64_t>(at) + 1 != 0) goto L_255d18;
L_2566e0:
    if (ra > 0) goto L_256810;
L_2566e8: {
    // the end: the sub-mesh records to 0x1ac200 (bytes at 0x15fff8), the deferred mobys' shadow
    // words to 0x1ac600 (count at 0x15fffc), the chain closed, the ring's terminator
    const uint32_t e = t7_hi;
    const uint32_t n = e - (SPR + 0x3400);
    sw(OV(0x0015fff8), n);
    if (static_cast<int32_t>(n) > 0) {
        uint32_t d = OV(0x001ac200);
        for (uint32_t p = SPR + 0x3400; p != e; p += 0x10, d += 0x10) sq(d, lq(p));
    }
    uint32_t d = OV(0x001ac600), c = 0;
    for (uint32_t p = SPR + 0x3000; p != s5_hi; p += 0x10) {
        const uint64_t r = ld(p);
        sd(d, r);
        if ((r >> 32) != 0) {
            d += 8;
            ++c;
        }
    }
    sw(OV(0x0015fffc), c);
    chain_flush(sp);
    sw(OV(0x001611c0), sp);
    sq(SPR, kZero);
    spr_dma_from(t9, 0, 1);
    return t9 + 0x10;
}

L_256810:  // the first chain closed (0x160000), the deferred mobys drawn after it
    chain_flush(sp);
    sw(OV(0x00160000), sp);
    sp += 0x10;
    ra = -1;
    sw(SPR + 0x3ef0, SPR + 0x3000);
L_256840: {
    uint32_t r = lwu(SPR + 0x3ef0);
    s8 = lwu(r);
    if (r == s5_hi) goto L_2566e8;
    t1 = sx(static_cast<uint32_t>(lh(r + 8)));
    t3 = sx(static_cast<uint32_t>(lw(r + 0xc)));
    t6 = lhu(s8 + 0x34);
    a3 = (t1 << 1) + SPR;
    at = sx(static_cast<uint32_t>(lh(s8 + 0x32)));
    s3 = sx(static_cast<uint32_t>(lh(r + 0xa)));
    r += 0x10;
    sw(SPR + 0x3ef0, r);
    at = sx((static_cast<uint32_t>(at) << 10) - static_cast<uint32_t>(t3));
    if (s3 == 0) goto L_256840;
    qset_xy(v[1], t3);
    qset_xy(v[3], at << 32);
    goto L_255e48;
}

L_256890:  // a moby with a drop shadow (+0x34 & 0xc00, first pass)
    s3 = 0;
    ftoi0(v[3], Y);
    v1 = qlo(v[8]);
    a0 ^= a1;
    if (neg(v0)) goto L_2568e0;
    v0 <<= 32;
    ftoi0(v[1], XY);
    if (neg(v0)) goto L_2568e0;
    s3 = 0xa;
    v1 |= a0;
    if (t0 == 0) sb(s8 + 0x31, 1);
    if (neg(v1)) goto L_2568f0;
    v1 <<= 32;
    if (neg(v1)) goto L_2568f0;
    s3 = 0xe;
    goto L_2568f0;
L_2568e0:
    if (t0 != 0) sb(s8 + 0x31, 0);
L_2568f0:
    t5 = s5_hi;
    t3v = v[1];
    t3 = qlo(v[1]);
    v0 = lhu(s8 + 0x34);
    if (t5 == SPR + 0x3300) goto L_2566c0;  // the deferred list full
    sw(static_cast<uint32_t>(t5), s8);
    sh(static_cast<uint32_t>(t5) + 8, static_cast<uint32_t>(t1));
    sh(static_cast<uint32_t>(t5) + 0xa, static_cast<uint32_t>(s3));
    v0 &= 0x800;
    sw(static_cast<uint32_t>(t5) + 0xc, static_cast<uint32_t>(t3));
    if (v0 != 0) goto L_256a78;
    a0 = sx(static_cast<uint32_t>(lw(s8 + 0x88)));
    v1 = sx(static_cast<uint32_t>(lw(s8 + 0x84)));
    v0 = lbu(s8 + 0xbd);
    if (static_cast<int64_t>(a0) <= 0) goto L_256a90;
    {
        // the shadow's place: on the ground between +0x84 and +0x88 (x 1024) along the shadow
        // table's direction (0x1aeb80 + 0x10 +0xbd), culled as a moby of +0x7f's distance
        v[4].v[0] = u2f(static_cast<uint32_t>(v1));
        v[4].v[1] = u2f(static_cast<uint32_t>(a0));
        v[4].v[2] = u2f(static_cast<uint32_t>(v1 >> 32));
        v[4].v[3] = u2f(static_cast<uint32_t>(a0 >> 32));
        v0 = (v0 << 4) + SPR;
        at = lbu(s8 + 0x7f);
        mulbc(fl, v[4], v[4], v[24].v[3], XY);
        v[8].v[0] = 0.5f;
        v[8].v[1] = 0.0f;
        v[5] = vu0m::lqc2(static_cast<uint32_t>(v0) + 0x3300);
        v[6] = vu0m::lqc2(s8);
        subbc(fl, v[7], v[4], v[4].v[0], Y);
        mulbc(fl, v[7], v[7], v[8].v[0], Y);
        addbc(fl, v[1], v[6], v[7].v[1], W);
        addbc(fl, v[4], v[4], v[7].v[1], X);
        subbc(fl, v[4], v[4], v[6].v[2], X);
        q() = vu0m::div(v[4].v[0], v[5].v[2]);
        mulabc(fl, v[5], q(), XYZ);
        maddbc(fl, v[1], v[6], v[0].v[3], XYZ);
        at <<= 42;
        sub(fl, v[2], v[1], v[24], XYZ);
        v[4].v[0] = 0.0f;
        v[4].v[1] = u2f(static_cast<uint32_t>(at >> 32));
        mulbc(fl, v[3], v[11], v[1].v[3], XY);
        v[19].v[1] = itof(v[4].v[1], 1.0f);
        rotate(fl, v[2], 25, v[2], XYZ);
        subbc(fl, v[19], v[19], v[1].v[3], Y);
        mulbc(fl, v[8], v[22], v[1].v[3], XY);
        addbc(fl, v[1], v[3], v[2].v[2], XY);
        mulbc(fl, v[5], v[20], v[2].v[2], XY);
        lanes(v[2], XY, [&](int i) { return u2f(f2u(v[2].v[i]) & 0x7fffffffu); });
        mulbc(fl, v[7], v[8], v[22].v[2], XY);
        sub(fl, v[3], v[19], v[1], XY);
        mul(fl, v[9], v[5], v[21], XY);
        sub(fl, v[6], v[2], v[8], XY);
        add(fl, v[8], v[2], v[7], XY);
        a1 = 0x80000000ull;
        sub(fl, v[4], v[1], v[23], XY);
        at = qlo(v[3]);
        v[1].v[0] = v[1].v[1];
        sub(fl, v[7], v[5], v[6], XY);
        sub(fl, v[8], v[9], v[8], XY);
        v0 = at << 32;
        t2 = 1;
        a0 = qlo(v[4]);
        if (neg(at)) goto L_256a90;
        const bool behind = !neg(v0);
        v0 = qlo(v[7]);
        if (behind) goto L_256a90;
        v1 = qlo(v[8]);
        a0 ^= a1;
        if (neg(v0)) goto L_256a90;
        v0 <<= 32;
        v1 |= a0;
        if (neg(v0)) goto L_256a90;
        ftoi0(v[1], X);
        if (!neg(v1)) {
            v1 <<= 32;
            if (!neg(v1)) t2 = 0;  // inside the guard band
        }
        // L_256a40: the shadow's intensity from its distance, 0..0x1000
        const uint32_t s = static_cast<uint32_t>(lbu(s8 + 0x7f)) << 10;
        const uint32_t d = (s - f2u(v[1].v[0])) >> 1;
        sh(static_cast<uint32_t>(t5) + 6, static_cast<uint32_t>(t2));
        sh(static_cast<uint32_t>(t5) + 4, static_cast<uint32_t>(min32(static_cast<int32_t>(d), 0x1000)));
        t5 += 0x10;
        s5_hi = static_cast<uint32_t>(t5);
        goto L_255d18;
    }
L_256a78:
    sh(static_cast<uint32_t>(t5) + 6, 0);
    if (s3 == 0) goto L_255d18;
    sh(static_cast<uint32_t>(t5) + 4, 0);
    t5 += 0x10;
    s5_hi = static_cast<uint32_t>(t5);
    goto L_255d18;
L_256a90:  // its shadow not seen: the moby drawn now
    at = sx(static_cast<uint32_t>(lh(s8 + 0x32)));
    if (s3 == 0) goto L_255d18;
    at = sx((static_cast<uint32_t>(at) << 10) - static_cast<uint32_t>(t3));
    v[1] = t3v;
    qset_xy(v[3], at << 32);
    goto L_255e48;

L_256ab0: {  // +0x34 & 0x10: a sub-mesh record for the class's extra packets
    const uint32_t e = t7_hi;
    const uint32_t cls = lwu(s8 + 0x24), r = lwu(s8 + 0x90);
    if (e == SPR + 0x3800) goto L_2564c8;
    const uint32_t alpha = lbu(t8 - 4);
    const uint32_t k = lbu(cls + t6_hi + 0xa);
    const int32_t n = static_cast<int32_t>(s0 - s1 - (k << 4));
    if (n <= 0) goto L_2564c8;
    sw(e, r);
    sb(e + 3, alpha);
    sw(e + 4, s1 + (k << 4));
    sw(e + 8, t9 + (k << 3) + 0xc0);
    sw(e + 0xc, static_cast<uint32_t>(n) >> 4);
    t7_hi = e + 0x10;
    goto L_2564c8;
}

L_256b20: {  // +0x73: the shadow mesh's fade with the depth (0..0x80)
    const int32_t d = static_cast<int32_t>((static_cast<uint32_t>(t7) << 10) - static_cast<uint32_t>(t0));
    t7 = 0;
    if (d <= 0) goto L_255fd0;
    t7 = sx(static_cast<uint32_t>(min32(static_cast<int32_t>(static_cast<uint32_t>(d) >> 7), 0x80)));
    goto L_255fd0;
}

L_256b50: {  // the shadow mesh (class +6 / +7 packets): flattened along the light's direction
    v[9] = vu0m::lqc2(s8);
    const uint32_t base = t8 - (t8 & 0x3ff);
    const uint32_t cls = lwu(s8 + 0x24);
    const uint32_t first = lbu(cls + 7), count = lbu(cls + 6);
    t8 += 0x10;
    s1 = lwu(cls) + (first << 4);
    sub(fl, v[9], v[9], v[24], XYZ);
    sh(base + 0x16, (count + 0xa) >> 1);
    s0 = s1 + (count << 4);
    t8 = (t8 >> 4) << 4;
    t2q = lq(SPR + 0x3e80);
    t3q = lq(SPR + 0x3ea0);
    mul(fl, v[5], v[9], v[9], XYZ);
    addbc(fl, v[6], v[0], v[0].v[3], XYZ);
    t4q = lq(SPR + 0x3eb0);
    t5q = lq(SPR + 0x3ec0);
    t6q = lq(SPR + 0x3ed0);
    addabc(fl, v[5], v[5].v[1], X);
    maddbc(fl, v[5], v[6], v[5].v[2], X);
    v[1] = vu0m::lqc2(s8 + 0xc0);
    v[2] = vu0m::lqc2(s8 + 0xd0);
    v[3] = vu0m::lqc2(s8 + 0xe0);
    q() = vrsqrt(v[0].v[3], v[5].v[0]);
    rotate(fl, v[1], 25, v[1], XYZW);
    rotate(fl, v[2], 25, v[2], XYZW);
    rotate(fl, v[3], 25, v[3], XYZW);
    v[4] = v[0];
    add(fl, v[5], v[0], v[0], XYZ);
    add(fl, v[6], v[0], v[0], XYZ);
    add(fl, v[7], v[0], v[0], XYZ);
    mulbc(fl, v[9], v[9], q(), XYZ);
    rotate(fl, v[9], 25, v[9], XYZW);
    addbc(fl, v[5], v[0], v[0].v[3], X);
    addbc(fl, v[6], v[0], v[9].v[2], Y);
    subbc(fl, v[7], v[0], v[9].v[1], Y);
    addbc(fl, v[6], v[0], v[9].v[1], Z);
    addbc(fl, v[7], v[0], v[9].v[2], Z);
    rotate(fl, v[1], 5, v[1], XYZW);
    rotate(fl, v[2], 5, v[2], XYZW);
    rotate(fl, v[3], 5, v[3], XYZW);
    addbc(fl, v[5], v[0], v[9].v[2], X);
    add(fl, v[6], v[0], v[0], XYZ);
    subbc(fl, v[7], v[0], v[9].v[0], X);
    addbc(fl, v[5], v[0], v[9].v[0], Z);
    addbc(fl, v[6], v[0], v[0].v[3], Y);
    addbc(fl, v[7], v[0], v[0].v[1], Y);
    addbc(fl, v[7], v[0], v[9].v[2], Z);
    rotate(fl, v[1], 5, v[1], XYZW);
    rotate(fl, v[2], 5, v[2], XYZW);
    rotate(fl, v[3], 5, v[3], XYZW);
    vu0m::sqc2(t8, v[1]);
    vu0m::sqc2(t8 + 0x10, v[2]);
    vu0m::sqc2(t8 + 0x20, v[3]);
    vu0m::sqc2(t8 + 0x30, v[4]);
    t8 += 0x40;
    v[1] = vu0m::lqc2(SPR + 0x3ee0);
    t1q = lq(SPR + 0x3f40);
    add(fl, v[18], v[18], v[1], XYZW);
    s2 = 2;
    s3 = 0x1400000a;
    do {
        s4 = -s4;
        if (s2 > 0) {
            sq(sp, t1q);
            --s2;
            vu0m::sqc2(sp + 0x10, v[15]);
            vu0m::sqc2(sp + 0x20, v[16]);
            vu0m::sqc2(sp + 0x30, v[17]);
            vu0m::sqc2(sp + 0x40, v[18]);
            sh(sp + 0xc, s4 > 0 ? 0x28d : 0x129);
            sp += 0x50;
        }
        {
            const uint32_t n = lwu(s1 + 8), d = lwu(s1 + 0xc);
            uint32_t p = sp + 0x30;
            sq(sp, t2q);
            sq(sp + 0x10, t3q);
            sq(sp + 0x20, t4q);
            sw(t8, (n << 4) | (d & 0xff));
            sw(t8 + 4, p);
            sb(sp + 0xe, d >> 24);
            sb(sp + 0x1e, d >> 24);
            sb(sp + 0x2e, d >> 24);
            sh(sp, (d >> 8) & 0xff);
            sh(sp + 0x10, (d >> 16) & 0xff);
            sh(sp + 0x20, (d >> 16) & 0xff);
            t8 += 8;
            p += ((d >> 16) & 0xff) << 4;
            sw(sp + 4, p);
            p += ((d >> 8) & 0xff) << 4;
            sw(sp + 0x14, p);
            p += ((d >> 16) & 0xff) << 4;
            sw(sp + 0x24, p);
            sp = p;
        }
        const uint64_t r = ld(s1);
        s1 += 0x10;
        sq(sp, t5q);
        sp += 0x10;
        sw(sp - 0xc, static_cast<uint32_t>(r));
        sh(sp - 0x10, static_cast<uint32_t>(r >> 32));
        sq(sp, t6q);
        sp += 0x10;
        sh(sp - 4, static_cast<uint32_t>(s3));
    } while (s1 != s0);
    sd(t8, 0);
    t8 += 0x10;
    goto L_256668;
}
}

}  // namespace rac1
