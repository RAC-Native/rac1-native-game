// Blarg's Nebula G34 (level 6) own draw callbacks, hand-ported: class 1035's bolts (the electric barrier), class 1062's
// first draw and class 1123's (the electric arc), and the strands of quads the two electric ones build on the VU0. Their
// updates queue them (sub_001f93e0); registered in blarg_part2.cpp. Addresses are level 6's (`orig: L6:`); its own data is
// L6_ADDR. The level's own copies of two engine functions they call are the engine's ports: 0x217ab8 (mat4_scale_diag)
// and 0x214428 (shadows_vu1_setup, 0x1fc740 relinked).
#include <game/math/vecmath.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/vu0_regs.h>

namespace rac1 {

namespace {
using eer::Q128;

// a word as a register just loaded holds it (sign-extended; the high half 0)
Q128 gpr(uint32_t w) {
    Q128 q;
    eer::set32(q, w);
    return q;
}
// a 32-bit word sign-extended (a lw), as the 64-bit GS register values are built from them
uint64_t sx(uint32_t w) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w))); }

// the GS ALPHA_1 value of the level's four words at `words` (A, B, C, D: 2 bits each) and the fixed alpha `fix`
uint64_t alpha_mode(uint32_t words, uint64_t fix) {
    return sx(lwu(words)) | (sx(lwu(words + 4)) << 2) | (sx(lwu(words + 8)) << 4) | (sx(lwu(words + 0xc)) << 6) | fix;
}
constexpr uint64_t kClampRepeat = (0xff90ull << 32) | 0x260;  // the CLAMP_1 the draws set

// the GS state of the electric draws: the texture 0xe, the clamp, no test, the alpha blend of the level's words `blend`
void electric_gs_state(uint32_t blend) {
    sub_002a0ba8(6, effect_tex0(0xe));          // (TEX0_1)
    sub_002a0ba8(0x14, kClampRepeat);           // (CLAMP_1)
    sub_002a0ba8(8, 0);                         // (TEST_1)
    sub_002a0ba8(0x42, alpha_mode(blend, 0x8000ull << 24));  // (ALPHA_1)
}

// the packet cursor and the quadword templates the strands' packets start from
constexpr uint32_t kSprPoints = 0x70001000u;  // the strand's points as built (two quadwords a point: its two edges)
constexpr uint32_t kSprUvs = 0x70002000u;     // their texture coordinates (two doublewords a point)

// `n` words of 0 from `at` up to the next quadword; the end
uint32_t pad_to_quad(uint32_t at) {
    while ((at & 0xf) != 0) {
        sw(at, 0);
        at += 4;
    }
    return at;
}
}  // namespace

// Four strands of quads of the electric draws (VU1 program 7's packets, one a strand): the record `rec` (+0 / +4 the
// strength's top and base, +8 the strands' four strengths, +0xc / +0x10 the colours at the base and at the top, +0x14
// the points of a strand, +0x18 the strands' points (all four's, in turn), +0x1c the texture's step in u along a strand,
// +0x20 its start) through the matrix `mat`. Each point gives two vertices 0.1 apart in z, the texture's v 0 and 1; a
// strand's colour is the colours' blend by its strength (above the base: up to the top; below: down from the base). The
// VU0 does it in macro mode as the original (vf14..vf17 the matrix, vf20 / vf21 the edges' offsets, vf4 / vf5 the
// coordinates, vf6 their step; vf1..vf3, vf10, vf11 the colour); the points and coordinates go through the scratchpad.
// orig: L6:0x00216b88
void arc_strands_draw_l6(uint32_t rec, uint32_t mat) {
    using namespace vu0m;
    Vf* r = vf();
    eer::vmacro();
    eer::lqc2_(14, mat);
    eer::lqc2_(15, mat + 0x10);
    eer::lqc2_(16, mat + 0x20);
    eer::lqc2_(17, mat + 0x30);
    eer::qmtc2(1, gpr(0x3d4ccccdu));  // (0.05: half the strand's width)
    r[20] = r[0];
    r[21] = r[0];
    subbc(r[20], r[20], r[1].v[0], Z);
    addbc(r[21], r[21], r[1].v[0], Z);
    for (int32_t strand = 3; strand >= 0; --strand) {
        // the strand's points: each through the matrix as its two edges, and their coordinates (u stepping, v 0 / 1)
        eer::qmtc2(1, gpr(lwu(rec + 0x20)));
        add(r[4], r[0], r[0], Y);
        add(r[4], r[0], r[1], X);
        const int32_t count = lw(rec + 0x14);
        add(r[5], r[0], r[1], X);
        addbc(r[5], r[0], r[0].v[3], Y);
        eer::qmtc2(6, gpr(lwu(rec + 0x1c)));
        uint32_t p = lwu(rec + 0x18) + (static_cast<uint32_t>(strand * count) << 4);
        uint32_t pts = kSprPoints, uvs = kSprUvs;
        int32_t n = count;
        do {  // (as the original: at least one point)
            add(r[4], r[4], r[6], X);
            add(r[5], r[5], r[6], X);
            eer::lqc2_(2, p);
            add(r[3], r[2], r[21], XYZ);
            add(r[2], r[2], r[20], XYZ);
            mulabc(r[17], r[0].v[3], XYZW);
            maddabc(r[16], r[3].v[2], XYZW);
            maddabc(r[15], r[3].v[1], XYZW);
            maddbc(r[3], r[14], r[3].v[0], XYZW);
            mulabc(r[17], r[0].v[3], XYZW);
            maddabc(r[16], r[2].v[2], XYZW);
            maddabc(r[15], r[2].v[1], XYZW);
            maddbc(r[2], r[14], r[2].v[0], XYZW);
            sd(uvs, eer::qmfc2(4).lo);
            eer::sqc2_(pts, 2);
            eer::sqc2_(pts + 0x10, 3);
            sd(uvs + 8, eer::qmfc2(5).lo);
            pts += 0x20;
            p += 0x10;
            uvs += 0x10;
        } while (--n != 0);
        // the strand's colour: x the blend (vf11.x 1 / base, vf10.x 1 / (top - base)), then the colours' lerp packed
        const uint32_t top = lwu(rec), base = lwu(rec + 4);
        eer::qmtc2(11, gpr(base));
        itofv(r[11], r[11], XYZW, 1.0f);
        eer::qmtc2(10, gpr(top - base));
        itofv(r[10], r[10], XYZW, 1.0f);
        q() = div(r[0].v[3], r[11].v[0]);
        addbc(r[11], r[0], q(), X);
        q() = div(r[0].v[3], r[10].v[0]);
        addbc(r[10], r[0], q(), X);
        const uint32_t strength = lwu(lwu(rec + 8) + static_cast<uint32_t>(strand) * 4);
        if (static_cast<int32_t>(strength - base) > 0) {
            eer::qmtc2(1, gpr(strength - base));  // (above the base: (s - base) / (top - base))
            itofv(r[1], r[1], XYZW, 1.0f);
            mul(r[1], r[1], r[10], X);
        } else {
            eer::qmtc2(1, gpr(strength));  // (below: 1 - s / base)
            itofv(r[1], r[1], XYZW, 1.0f);
            mul(r[1], r[1], r[11], X);
            subbc(r[1], r[0], r[1].v[0], W);
            addbc(r[1], r[0], r[1].v[3], X);
        }
        const Q128 none{};
        eer::qmtc2(2, eer::pextlh(none, eer::pextlb(none, gpr(lwu(rec + 0xc)))));   // (the colours' bytes as four integers)
        eer::qmtc2(3, eer::pextlh(none, eer::pextlb(none, gpr(lwu(rec + 0x10)))));
        subbc(r[1], r[0], r[1].v[0], W);
        itofv(r[2], r[2], XYZW, 1.0f);
        itofv(r[3], r[3], XYZW, 1.0f);
        mulabc(r[2], r[1].v[3], XYZW);
        maddbc(r[1], r[3], r[1].v[0], XYZW);
        ftoiv(r[1], r[1], XYZW, 1.0f);
        const uint32_t colour = eer::ppacb(none, eer::ppach(none, eer::qmfc2(1))).w();
        // the packet: the header (0x1c47a0's five quadwords, the vertices' count in its GIF tag), then three UNPACKs (the
        // coordinates 0x160ec0, the colours 0x160ee0, the positions 0x160f00, each with the count), each padded to a
        // quadword, and MSCALF 0xe / FLUSH; the header's DMA tag gets the packet's size
        const uint32_t verts = static_cast<uint32_t>(lw(rec + 0x14)) << 1;
        const uint32_t unpack_n = verts << 16;
        const uint32_t start = lwu(L6_ADDR(0x001611c0));
        uint32_t at = start;
        for (uint32_t k = 0; k < 5; ++k) lq_sq(at + k * 0x10, L6_ADDR(0x001c47a0) + k * 0x10);
        at += 0x50;
        sw(start + 0x10, lwu(start + 0x10) | verts);
        lq_sq(at, L6_ADDR(0x00160ec0));
        at += 0x10;
        sw(at - 4, lwu(at - 4) | unpack_n);
        for (uint32_t k = 0; k < verts; ++k, at += 8) sd(at, ld(kSprUvs + k * 8));
        at = pad_to_quad(at);
        lq_sq(at, L6_ADDR(0x00160ee0));
        at += 0x10;
        sw(at - 4, lwu(at - 4) | unpack_n);
        for (uint32_t k = 0; k < verts; ++k, at += 4) sw(at, colour);
        at = pad_to_quad(at);
        lq_sq(at, L6_ADDR(0x00160f00));
        at += 0x10;
        sw(at - 4, lwu(at - 4) | unpack_n);
        for (uint32_t k = 0; k < verts; ++k, at += 0xc) {
            const uint32_t v = kSprPoints + k * 0x10;
            sw(at, lwu(v));
            sw(at + 4, lwu(v + 4));
            sw(at + 8, lwu(v + 8));
        }
        at = pad_to_quad(at);
        sw(at, 0x15000000u | 0xe);  // (MSCALF 0xe)
        sw(at + 4, 0x11000000u);    // (FLUSH)
        sd(at + 8, 0);
        at += 0x10;
        sw(start, lwu(start) | (((at - start) >> 4) - 1));
        sw(L6_ADDR(0x001611c0), at);
    }
}

// Class 1035's draw, the barrier's nine bolts: the moby's rotation scaled by 0x161e0c, the GS state (the alpha blend of
// 0x161dd0), the VU1 program 7 set up, then each bolt's frame (its rotation 0x1db060 + 0x10 k turned by the moby's, at its
// offset 0x1dafd0 + 0x10 k through the moby's rotation, from the moby) and its strands: the level's points 0x1db0f0 (30
// a strand), the strengths 0x161e40 against 0x161e50 / 0x161e54, the colours 0x161df0 (its alpha by the pvar +0x18) and
// 0x161df4, the texture's step and start 0x161df8 / 0x161dfc.
// orig: L6:0x002f6dd0
void blg_class1035_draw_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x3c0);
    const uint32_t turn = sp, scale = sp + 0x40, bolts = sp + 0x80, offset = sp + 0x2c0, rec = sp + 0x2d0;
    const uint32_t pv = lwu(m + 0x78);
    euler_to_mat4_vu(M(turn), V(m + 0x40));
    mat4_scale_diag(M(scale), lwc1(L6_ADDR(0x00161e0c)));
    mat4_mul(M(turn), M(turn), M(scale));
    electric_gs_state(L6_ADDR(0x00161dd0));
    shadows_vu1_setup();
    for (uint32_t k = 0; k < 9; ++k) {
        const uint32_t frame = bolts + k * 0x40;
        euler_to_mat4_vu(M(frame), V(L6_ADDR(0x001db060) + k * 0x10));
        mat4_mul(M(frame), M(turn), M(frame));
        mat4_transform(V(offset), V(L6_ADDR(0x001dafd0) + k * 0x10), M(turn));
        vec3_add(V(frame + 0x30), V(offset), V(m + 0x10));
        swc1(frame + 0x3c, 1.0f);
    }
    const uint32_t colour = lwu(L6_ADDR(0x00161df0));
    sw(rec + 0xc, rgba_lerp(colour & 0xffffffu, colour, lwc1(pv + 0x18)));
    sw(rec + 0, lwu(L6_ADDR(0x00161e50)));
    sw(rec + 4, lwu(L6_ADDR(0x00161e54)));
    sw(rec + 8, L6_ADDR(0x00161e40));
    sw(rec + 0x10, lwu(L6_ADDR(0x00161df4)));
    sw(rec + 0x14, 0x1e);
    sw(rec + 0x18, L6_ADDR(0x001db0f0));
    swc1(rec + 0x1c, lwc1(L6_ADDR(0x00161df8)));
    swc1(rec + 0x20, lwc1(L6_ADDR(0x00161dfc)));
    for (uint32_t k = 0; k < 9; ++k) arc_strands_draw_l6(rec, bolts + k * 0x40);
    ee_stack_free(0x3c0);
}

// Class 1062's first draw: 50 quads of the level's mesh (corners 0x1f1220: four pairs of a point of 0x1f0900 and texture
// coordinates of 0x1f1540), each drawn twice: with the texture of 0x161f90 and its coordinates moved by the camera's angles
// (0x167558 times 0x161fa0 in u, 0x167554 times 0x161fa4 in v), and with the texture 0x2a as they are; the colours
// 0x161f94 / 0x161f98, the alpha blend of 0x161f7c (its fixed alpha 0x161f8c). (The original also works out, per quad, its
// centre 0x1f2610 scaled by 0x161f9c, the direction to it from the camera and that direction's dot product with its normal
// 0x1f19d0, in stack temporaries it never reads: left out.)
// orig: L6:0x002fd1a0
void blg_class1062_draw_a_l6(uint32_t) {
    const uint32_t sp = ee_stack_alloc(0x250);
    const uint32_t moved = sp, plain = sp + 0x90, ident = sp + 0x120;
    const float du = fpu_mul(lwc1(L6_ADDR(0x00161fa0)), lwc1(L6_ADDR(0x00167558)));
    const float dv = fpu_mul(lwc1(L6_ADDR(0x00161fa4)), lwc1(L6_ADDR(0x00167554)));
    // the two quads' descriptors: four points +0, four colours +0x40, four coordinates +0x50, the GS registers +0x70
    // (TEX1 0, TEX0, CLAMP, ALPHA)
    sd(moved + 0x78, effect_tex0(lw(L6_ADDR(0x00161f90))));
    sd(plain + 0x78, effect_tex0(0x2a));
    const uint64_t blend = alpha_mode(L6_ADDR(0x00161f7c), sx(lwu(L6_ADDR(0x00161f8c))) << 32);
    sd(moved + 0x88, blend);
    sd(plain + 0x88, blend);
    sd(moved + 0x80, kClampRepeat);
    sd(plain + 0x80, kClampRepeat);
    sd(plain + 0x70, 0);
    sd(moved + 0x70, 0);
    mat4_identity(M(ident));
    for (uint32_t k = 0; k < 4; ++k) {
        sw(moved + 0x40 + k * 4, lwu(L6_ADDR(0x00161f94)));
        sw(plain + 0x40 + k * 4, lwu(L6_ADDR(0x00161f98)));
    }
    for (uint32_t q = 0; q < 50; ++q) {
        uint32_t corner = L6_ADDR(0x001f1220) + q * 0x10;
        for (uint32_t c = 0; c < 4; ++c, corner += 4) {
            lq_sq(moved + c * 0x10, L6_ADDR(0x001f0900) + (static_cast<uint32_t>(lh(corner)) << 4));
            lq_sq(plain + c * 0x10, moved + c * 0x10);
            const uint32_t uv = static_cast<uint32_t>(lh(corner + 2)) << 3;
            const float u = lwc1(L6_ADDR(0x001f1540) + uv), v = lwc1(L6_ADDR(0x001f1544) + uv);
            swc1(moved + 0x50 + c * 8, fpu_add(u, du));
            swc1(moved + 0x54 + c * 8, fpu_add(v, dv));
            swc1(plain + 0x50 + c * 8, u);
            swc1(plain + 0x54 + c * 8, v);
        }
        quad_draw(moved, ident);
        quad_draw(plain, ident);
    }
    ee_stack_free(0x250);
}

// Class 1123's draw, the arc's four strands of bolts: the moby's rotation scaled by 0x162208, the GS state (the alpha
// blend of 0x1621c0), the VU1 program 7 set up, then four frames (the rotations 0x1fe890 + 0x10 k turned by the moby's,
// at the moby, each 1.7 higher from 0.6) and the strands: the moby's own points (its pvars, 30 a strand) and strengths
// (pvars +0xf00) against 0x162234 / 0x162238, the colours 0x1621e0 / 0x1621e4, the texture's step and start 0x1621f4 /
// 0x1621f8.
// orig: L6:0x00305170
void blg_class1123_draw_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x2a0);
    const uint32_t turn = sp, scale = sp + 0x40, frames = sp + 0x80, rec = sp + 0x1c0;
    const uint32_t pv = lwu(m + 0x78);
    euler_to_mat4_vu(M(turn), V(m + 0x40));
    mat4_scale_diag(M(scale), lwc1(L6_ADDR(0x00162208)));
    mat4_mul(M(turn), M(turn), M(scale));
    electric_gs_state(L6_ADDR(0x001621c0));
    shadows_vu1_setup();
    for (int32_t k = 0; k < 4; ++k) {
        const uint32_t frame = frames + static_cast<uint32_t>(k) * 0x40, at = frame + 0x30;
        euler_to_mat4_vu(M(frame), V(L6_ADDR(0x001fe890) + static_cast<uint32_t>(k) * 0x10));
        mat4_mul(M(frame), M(turn), M(frame));
        vec3_add(V(at), V(at), V(m + 0x10));
        const float z = lwc1(at + 8);
        swc1(at + 0xc, 1.0f);
        swc1(at + 8, fpu_add(z, fpu_add(fpu_mul(static_cast<float>(k), u2f(0x3fd9999au)), u2f(0x3f19999au))));  // (1.7 k + 0.6)
    }
    sw(rec + 0, lwu(L6_ADDR(0x00162234)));
    sw(rec + 4, lwu(L6_ADDR(0x00162238)));
    sw(rec + 8, pv + 0xf00);
    sw(rec + 0xc, lwu(L6_ADDR(0x001621e0)));
    sw(rec + 0x10, lwu(L6_ADDR(0x001621e4)));
    sw(rec + 0x14, 0x1e);
    sw(rec + 0x18, pv);
    swc1(rec + 0x1c, lwc1(L6_ADDR(0x001621f4)));
    swc1(rec + 0x20, lwc1(L6_ADDR(0x001621f8)));
    for (uint32_t k = 0; k < 4; ++k) arc_strands_draw_l6(rec, frames + k * 0x40);
    ee_stack_free(0x2a0);
}

}  // namespace rac1
