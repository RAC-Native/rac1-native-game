// Hand-written overlay code: Novalis's scrolling surfaces (waterfalls / energy strips: classes 676, 678, 761, 1225) and
// its water grid (class 751), drawn through VU1 program 7 and rendered by the Modern Rendering Pipeline (mrp_fx.h). The
// water's code (its grid, set-up, cells, draw and the class's draw callback) is the same engine code on Rilgar, Umbris,
// Pokitaru, Hoven and Gemlik: those ports serve every level (their `orig:` lines; batches 501 / 511), every address the
// engine's (OV) but each level's tile records.
#include <game/ovl/novalis_water.h>
#include <game/ovl/novalis_draw_cbs.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/vu0_local.h>
#include <game/render/mrp_fx.h>

namespace rac1 {

namespace {

constexpr OvConst<0x001611c0u> CUR{};  // the frame's DMA chain packet cursor (every level's, where the overlay map pairs it with Veldin's)

void load_vf(vu0l::Vu& u, int r, uint32_t addr) {
    const uint32_t a = addr & ~15u;
    u.set_words(r, lwu(a), lwu(a + 4), lwu(a + 8), lwu(a + 12));
}

void store_vf(const vu0l::Vu& u, int r, uint32_t addr) {
    const uint32_t a = addr & ~15u;
    for (int i = 0; i < 4; ++i) sw(a + static_cast<uint32_t>(i) * 4u, u.word(r, i));
}

void qmtc2_s32(vu0l::Vu& u, int r, uint32_t v) {
    u.set_words(r, v, (v & 0x80000000u) != 0 ? 0xffffffffu : 0u, 0u, 0u);
}

void qmtc2_z_s32(vu0l::Vu& u, int r, uint32_t v) {
    // Matches set32(at, v); at = pcpyld(at, zero); qmtc2(r, at): places v in lane Z (with sign extension in lane W).
    u.set_words(r, 0u, 0u, v, (v & 0x80000000u) != 0 ? 0xffffffffu : 0u);
}

uint32_t pad_qword(uint32_t at) {
    while ((at & 0xfu) != 0) {
        sw(at, 0);
        at += 4u;
    }
    return at;
}

void wrap_pm1(uint32_t addr) {
    const float v = lwc1(addr);
    if (1.0f < v) {
        swc1(addr, fpu_sub(v, 1.0f));
    } else if (v < -1.0f) {
        swc1(addr, fpu_add(v, 1.0f));
    }
}

void wrap_01(uint32_t addr) {
    const float v = lwc1(addr);
    if (1.0f < v) {
        swc1(addr, fpu_sub(v, 1.0f));
    }
    const float v2 = lwc1(addr);
    if (v2 < 0.0f) {
        swc1(addr, fpu_add(v2, 1.0f));
    }
}

void clip_corners_4(const Vec4 corners[4], uint32_t& inner_codes, uint32_t& outer_codes) {
    const uint32_t cam = OV(0x00166dc0);
    inner_codes = 0;
    outer_codes = 0;
    for (int i = 0; i < 4; ++i) {
        Vec4 rel{};
        Vec4 p_in{};
        Vec4 p_out{};
        vec3_sub(rel, corners[i], V(cam));
        vec3_scale(p_in, rel, 1024.0f);
        vec3_scale(p_out, rel, 1024.0f);
        mat4_transform(p_in, p_in, M(cam - 0x80u));
        mat4_transform(p_out, p_out, M(cam - 0x40u));
        inner_codes = (inner_codes << 8) | static_cast<uint32_t>(clip_outcode(p_in));
        outer_codes = (outer_codes << 8) | static_cast<uint32_t>(clip_outcode(p_out));
    }
}

bool all_corners_outside(uint32_t codes) {
    static constexpr uint32_t kPlanes[] = {0x01010101u, 0x02020202u, 0x04040404u, 0x08080808u, 0x20202020u};
    for (uint32_t m : kPlanes) {
        if ((codes & m) == m) return true;
    }
    return false;
}

}  // namespace

// orig: L1:0x002b96c0
void nov_strips_vu7_setup_l1() { shadows_vu1_setup(); }

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik; every address the engine's.)
// orig: L1:0x002b8f98 L5:0x002cd960 L7:0x002cbed0 L11:0x002c8108 L12:0x002bba98 L13:0x002af8b8
void nov_water_vu7_setup_l1() {
    const uint32_t s1 = OV(0x00166dc0);  // camera position
    Mat4 scale_m{};
    mat4_scale_diag(scale_m, 1024.0f);
    vec3_scale(scale_m.r[3], V(s1), -1024.0f);
    scale_m.r[3].w = 1.0f;

    if (lw(OV(0x0015f5e0)) != 7) {
        packet_ref(0x0010e810, lhu(0x0010e800));
        sw(OV(0x0015f5e0), 7);
    }
    sw(lwu(CUR), 0x10000000u);
    sw(lwu(CUR) + 4, 0);
    sw(lwu(CUR) + 8, 0x11000000u);    // FLUSH
    sw(lwu(CUR) + 0xc, 0x01000404u);  // STCYCL 4, 4
    const uint32_t s0 = lwu(CUR);
    sw(s0 + 0x10, 0);
    sw(s0 + 0x14, 0);
    sw(s0 + 0x18, 0);
    sw(s0 + 0x1c, 0x6c0c43a4u);  // UNPACK V4-32, 12 quadwords to 0x3a4
    mat4_mul(M(s0 + 0x20), M(s1 - 0x100), scale_m);
    mat4_mul(M(s0 + 0x60), M(s1 - 0x80), scale_m);
    const uint32_t v1 = OV(0x0016ca40);
    sw(s0 + 0xa8, 0x412);
    sw(s0 + 0xa0, 0x8000);
    sw(s0 + 0xa4, 0x303ec000u);
    swc1(s0 + 0xac, lwc1(v1 + 0x210));
    lq_sq(s0 + 0xb0, v1 + 0x190);  // vf29: screen scale
    lq_sq(s0 + 0xc0, v1 + 0x1a0);  // vf30: screen offset
    swc1(s0 + 0xd0, lwc1(v1 + 0x22c));
    sw(s0 + 0xe4, 0x020001d2u);  // OFFSET 0x1d2
    swc1(s0 + 0xd4, lwc1(v1 + 0x228));
    sw(s0 + 0xe8, 0x15000000u);  // MSCALF 0
    sw(s0 + 0xe0, 0x03000000u);  // BASE 0
    sw(s0 + 0xd8, 0);
    sw(s0 + 0xdc, 0);
    sw(s0 + 0xec, 0);
    const uint32_t a2 = s0 + 0xf0;
    const uint32_t a0 = lwu(CUR);
    sw(a0, lwu(a0) | static_cast<uint32_t>((static_cast<int32_t>(a2 - a0) >> 4) - 1));
    sw(CUR, a2);
    mrp_fx_note_vu7(s0 + 0x20);
    packet_ref_13cf10();
    sw(lwu(CUR), 0x30000007u);
    sw(lwu(CUR) + 4, OV(0x001ca800));
    sw(lwu(CUR) + 8, 0);
    sw(lwu(CUR) + 0xc, 0x50000007u);
    sw(OV(0x00161234), 0xffffffffu);
    sw(OV(0x00161228), 0xffffffffu);
    sw(OV(0x0016122c), 0xffffffffu);
    sw(CUR, lwu(CUR) + 0x10);
    sw(OV(0x00161230), 0xffffffffu);
}

// The strips' packet (VU1 program 7): `n` vertices' ST (`st`), colours (`rgba`) and positions (`xyz`, 12 bytes each) behind
// the engine's GIF / VIF templates (Veldin's 0x1c4020 x 5, 0x160ec0 / 0x160ee0 / 0x160f00), MSCAL 0xc (+2 with `clip`
// bit 0), then a second packet of the ST pairs `st2` that calls the first's colours and positions again; at the
// packet cursor (0x1611c0), which is advanced.
// (Shared: the same code on Gaspar, Hoven and Oltanis; every address the engine's, relocated by the level's map.)
// orig: L1:0x0021fa98 L9:0x0021d250 L12:0x00220a58 L14:0x00211128
void nov_prim_list2_l1(int32_t n, uint32_t xyz, uint32_t rgba, uint32_t st, uint32_t st2, uint32_t clip) {
    const uint32_t un = static_cast<uint32_t>(n);
    const uint32_t mscal = ((clip & 1u) << 1) + 0xcu;
    const uint32_t n_hi = un << 16;

    const uint32_t cur = OV(0x001611c0);
    uint32_t pkt1 = lwu(cur);
    uint32_t at = pkt1;
    for (uint32_t i = 0; i < 5; ++i, at += 0x10u) {
        lq_sq(at, OV(0x001c4020) + i * 0x10u);
    }
    sw(at - 0x40u, lwu(at - 0x40u) | un);

    lq_sq(at, OV(0x00160ec0));
    at += 0x10u;
    sw(at - 4u, lwu(at - 4u) | n_hi);
    for (uint32_t i = 0; i < un; ++i, at += 8u) {
        sd(at, ld(st + i * 8u));
    }
    at = pad_qword(at);

    const uint32_t rgba_xyz_start = at;
    lq_sq(at, OV(0x00160ee0));
    at += 0x10u;
    sw(at - 4u, lwu(at - 4u) | n_hi);
    for (uint32_t i = 0; i < un; ++i, at += 4u) {
        sw(at, lwu(rgba + i * 4u));
    }
    at = pad_qword(at);

    lq_sq(at, OV(0x00160f00));
    at += 0x10u;
    sw(at - 4u, lwu(at - 4u) | n_hi);
    for (uint32_t i = 0; i < un; ++i, at += 0xcu) {
        const uint32_t src = xyz + i * 0xcu;
        sw(at + 0u, lwu(src + 0u));
        sw(at + 4u, lwu(src + 4u));
        sw(at + 8u, lwu(src + 8u));
    }
    at = pad_qword(at);

    sw(at + 0u, 0x15000000u | mscal);
    sw(at + 4u, 0x11000000u);
    sd(at + 8u, 0ull);
    at += 0x10u;
    const uint32_t rgba_xyz_end = at;
    sw(pkt1, lwu(pkt1) | (((at - pkt1) >> 4) - 1u));

    const uint32_t pkt2 = at;
    for (uint32_t i = 0; i < 5; ++i, at += 0x10u) {
        lq_sq(at, OV(0x001c4020) + i * 0x10u);
    }
    sw(at - 0x40u, lwu(at - 0x40u) | un);
    sw(at - 0x3cu, lwu(at - 0x3cu) | 0x01000000u);
    sw(at - 0x28u, lwu(at - 0x28u) | 0x01000000u);

    lq_sq(at, OV(0x00160ec0));
    at += 0x10u;
    sw(at - 4u, lwu(at - 4u) | n_hi);
    for (uint32_t i = 0; i < un; ++i, at += 8u) {
        sd(at, ld(st2 + i * 8u));
    }
    at = pad_qword(at);
    sw(pkt2, lwu(pkt2) | (((at - pkt2) >> 4) - 1u));

    sw(at + 0u, 0x30000000u | ((rgba_xyz_end - rgba_xyz_start) >> 4));
    sw(at + 4u, rgba_xyz_start);
    sd(at + 8u, 0ull);
    sw(cur, at + 0x10u);
}

// orig: L1:0x00262618
void nov_strip_verts_l1(uint32_t rec) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();

    uint32_t pts = lwu(rec + 0x0u);
    uint32_t uvs = lwu(rec + 0x4u);
    uint32_t count = lwu(rec + 0xcu);
    const uint32_t col = lwu(rec + 0x30u);
    const uint32_t phase = lwu(rec + 0x50u);

    load_vf(u, 10, rec + 0x10u);
    load_vf(u, 12, rec + 0x40u);
    qmtc2_s32(u, 15, 0x40c90fdbu);
    qmtc2_s32(u, 16, 0x40490fdbu);
    qmtc2_s32(u, 17, 0x3f000000u);

    uint32_t out_xyz = 0x70000000u;
    uint32_t out_st1 = 0x70001000u;
    uint32_t out_st2 = 0x70002000u;
    uint32_t out_col = 0x70003000u;
    uint32_t v1_hi0 = 0;
    uint32_t v1_hi1 = 0;

    for (; count != 0; --count) {
        const uint32_t px = lwu(pts + 0x0u);
        const uint32_t py = lwu(pts + 0x4u);
        const uint64_t heights = ld(pts + 0x8u);
        sw(out_xyz + 0x0u, px);
        sw(out_xyz + 0x4u, py);

        const uint32_t ang_idx = ((((px + py) >> 16) & 0xffu) << 6);
        const uint32_t ang_fixed = ((ang_idx + phase) << 3) & 0xfffu;
        u.set_words(20, ang_fixed, 0u, 0u, 0u);
        u.itof(20, 20, vu0l::X, 1.0f / 4096.0f);
        u.mul(20, 20, 15, vu0l::X);
        u.sub(20, 20, 16, vu0l::X);

        u.move(1, 20, vu0l::X);
        u.store();
        vu0::call(0x192);
        if (vu0::running()) vu0::drain();
        u.load();
        u.move(13, 1, vu0l::X);

        u.move(1, 20, vu0l::X);
        u.store();
        vu0::call(0x190);
        if (vu0::running()) vu0::drain();
        u.load();
        u.addbc(13, 0, u.lane(1, 0), vu0l::Y);

        u.set_words(1, static_cast<uint32_t>(heights), static_cast<uint32_t>(heights >> 32), v1_hi0, v1_hi1);
        qmtc2_s32(u, 3, lwu(rec + 0x5cu));
        u.subbc(3, 0, u.lane(3, 0), vu0l::W);
        u.mulbc(1, 1, u.lane(3, 0), vu0l::X);
        u.mulbc(1, 1, u.lane(3, 3), vu0l::Y);
        u.addbc(1, 1, u.lane(1, 1), vu0l::X);
        v1_hi0 = u.word(1, 2);
        v1_hi1 = u.word(1, 3);
        sw(out_xyz + 0x8u, u.word(1, 0));

        u.mulbc(4, 13, u.lane(10, 2), vu0l::XY);
        const uint64_t uv = ld(uvs);
        u.set_words(1, static_cast<uint32_t>(uv), static_cast<uint32_t>(uv >> 32), v1_hi0, v1_hi1);

        u.add(2, 1, 12, vu0l::XY);
        u.add(2, 2, 4, vu0l::XY);
        sd(out_st1, static_cast<uint64_t>(u.word(2, 0)) | (static_cast<uint64_t>(u.word(2, 1)) << 32));

        u.addbc(3, 1, u.lane(12, 2), vu0l::X);
        u.addbc(3, 1, u.lane(12, 3), vu0l::Y);
        u.sub(3, 3, 4, vu0l::XY);
        v1_hi0 = u.word(3, 2);
        v1_hi1 = u.word(3, 3);
        sd(out_st2, static_cast<uint64_t>(u.word(3, 0)) | (static_cast<uint64_t>(u.word(3, 1)) << 32));

        sw(out_col, col);

        pts += 0x10u;
        uvs += 0x8u;
        out_xyz += 0xcu;
        out_st1 += 0x8u;
        out_st2 += 0x8u;
        out_col += 0x4u;
    }

    u.store();
}

// orig: L1:0x002b96e0
void nov_strips_draw_l1(int32_t n, uint32_t recs) {
    if (n <= 0) return;
    for (int32_t i = 0; i < n; ++i, recs += 0x60u) {
        const float s1 = lwc1(recs + 0x10u);
        swc1(recs + 0x40u, fpu_add(lwc1(recs + 0x40u), fpu_mul(lwc1(recs + 0x20u), s1)));
        swc1(recs + 0x44u, fpu_add(lwc1(recs + 0x44u), fpu_mul(lwc1(recs + 0x24u), s1)));
        wrap_pm1(recs + 0x40u);
        wrap_pm1(recs + 0x44u);

        const float s2 = lwc1(recs + 0x14u);
        swc1(recs + 0x48u, fpu_add(lwc1(recs + 0x48u), fpu_mul(lwc1(recs + 0x28u), s2)));
        swc1(recs + 0x4cu, fpu_add(lwc1(recs + 0x4cu), fpu_mul(lwc1(recs + 0x2cu), s2)));
        wrap_pm1(recs + 0x48u);
        wrap_pm1(recs + 0x4cu);

        sw(recs + 0x50u, (lwu(recs + 0x50u) + lwu(recs + 0x1cu)) & 0x00ffffffu);

        const int32_t vis = static_cast<int32_t>(point_on_screen(lwu(recs + 0x8u), 400.0f));
        if (vis != -1) {
            const uint32_t p = lwu(CUR);
            sw(p + 0x00u, 0x30000007u);
            sw(p + 0x04u, L1_ADDR(0x001cac80u));
            sw(p + 0x08u, 0);
            sw(p + 0x0cu, 0x50000007u);

            const uint32_t gs = p + 0x10u;
            sw(gs + 0x00u, 0x10000005u);
            sw(gs + 0x04u, 0);
            sw(gs + 0x08u, 0);
            sw(gs + 0x0cu, 0x50000005u);
            sd(gs + 0x10u, (0x8000ull << 47) | 0x8001ull);
            sd(gs + 0x18u, 0xeeeeull);
            sd(gs + 0x20u, (static_cast<uint64_t>(lbu(recs + 0x3cu)) << 32) | 0x64ull);
            sd(gs + 0x28u, 0x42ull);
            sd(gs + 0x30u, (static_cast<uint64_t>(lbu(recs + 0x3du)) << 32) | 0x64ull);
            sd(gs + 0x38u, 0x43ull);
            sd(gs + 0x40u, effect_tex0(lw(recs + 0x34u)));
            sd(gs + 0x48u, 0x6ull);
            sd(gs + 0x50u, effect_tex0(lw(recs + 0x38u)));
            sd(gs + 0x58u, 0x7ull);
            sw(CUR, gs + 0x60u);

            nov_strip_verts_l1(recs);
            nov_prim_list2_l1(lw(recs + 0xcu), 0x70000000u, 0x70003000u, 0x70001000u, 0x70002000u,
                              static_cast<uint32_t>(1 - vis));
        }
    }
}

// The water grids (Novalis's class 751, the same engine code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik): each tile record
// (0x1190 bytes) holds three height blocks of 0x5c0 from +0x50 (16 x 16 floats, then the edges its neighbours give it:
// rows +0x400 / +0x480 / +0x500, columns +0x440 / +0x4c0 / +0x540, corners +0x580..+0x594), its 8 neighbours' indices at
// +0xc..+0x13 (0xff: none) and its "live" half +0x1e. The engine's words 0x161218 / 0x16121c (the current and the previous
// block), 0x161220 (the block drawn) and 0x161224 (the phase between two steps, by the step 0x1ca8a0 of the water's data
// 0x1ca880) drive them. Every address is the engine's, relocated by each level's overlay map.

// A height block (`tile`, 0x5c0 bytes) spread into the 24-wide grid `grid` (rows 0x60 apart from +0x70, the edges round
// it from the block's edge rows / columns / corners).
// (Shared: the same code on Rilgar, Umbris (twice), Pokitaru, Hoven and Gemlik.)
// orig: L1:0x00261c10 L5:0x00276bc0 L7:0x00275380 L11:0x00272fa8 L12:0x00265de8 L13:0x00258e78
void nov_water_grid_in_l1(uint32_t grid, uint32_t tile) {
    uint32_t src = tile;
    uint32_t dst_row = grid + 0x70u;
    for (int r = 0; r < 16; ++r, dst_row += 0x60u) {
        for (int q = 0; q < 4; ++q, src += 0x10u) {
            lq_sq(dst_row + static_cast<uint32_t>(q) * 0x10u, src);
        }
    }
    for (uint32_t q = 0; q < 4; ++q) {
        lq_sq(grid + 0x10u + q * 0x10u, tile + 0x400u + q * 0x10u);
        lq_sq(grid + 0x670u + q * 0x10u, tile + 0x480u + q * 0x10u);
        lq_sq(grid + 0x6d0u + q * 0x10u, tile + 0x500u + q * 0x10u);
    }
    for (uint32_t r = 0; r < 16; ++r) {
        sw(grid + 0x6cu + r * 0x60u, lwu(tile + 0x440u + r * 4u));
        sw(grid + 0xb0u + r * 0x60u, lwu(tile + 0x4c0u + r * 4u));
        sw(grid + 0xb4u + r * 0x60u, lwu(tile + 0x540u + r * 4u));
    }
    sw(grid + 0x0cu, lwu(tile + 0x580u));
    sw(grid + 0x50u, lwu(tile + 0x584u));
    sw(grid + 0x66cu, lwu(tile + 0x588u));
    sw(grid + 0x6b0u, lwu(tile + 0x58cu));
    sw(grid + 0x6b4u, lwu(tile + 0x590u));
    sw(grid + 0x710u, lwu(tile + 0x594u));
}

// One wave step of tile `tile` of the records `tiles` into its block `buf`, from the grid `grid` (nov_water_grid_in_l1's,
// the tile's current block): each of the 16 x 16 cells becomes its 8 neighbours' sum / 4 less the block's old value there,
// damped by 1/16 unless the result is smaller than the water's threshold (its bits, Veldin's 0x1ca89c, against the
// value's without the sign). Then the block's edges go to the neighbours' blocks `buf` (+0xc..+0x13: 0xff none).
// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x00262038 L5:0x00276fe8 L7:0x002757a8 L11:0x002733d0 L12:0x00266210 L13:0x002592a0
void nov_water_grid_wave_l1(uint32_t grid, int32_t tile, int32_t buf, uint32_t tiles) {
    const float quarter = 0.25f, damp = 0.0625f, one = 1.0f;
    const uint32_t still = lwu(OV(0x001ca89c));
    const uint32_t rec = tiles + static_cast<uint32_t>(tile) * 0x1190u;
    const uint32_t block = static_cast<uint32_t>(buf) * 0x5c0u;
    const uint32_t mine = rec + block + 0x50u;
    uint32_t out = mine;
    uint32_t at = grid + 0x70u;
    for (int r = 0; r < 16; ++r, at += 0x20u) {
        for (int c = 0; c < 16; ++c, at += 4u, out += 4u) {
            // (the original's MADD.S / MSUB.S chain: the neighbours from the row above, the sides, the row below)
            float acc = fpu_mul(lwc1(at - 0x64u), quarter);
            acc = fpu_add(acc, fpu_mul(lwc1(at - 0x60u), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at - 0x5cu), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at - 0x4u), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at + 0x4u), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at + 0x5cu), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at + 0x60u), quarter));
            acc = fpu_add(acc, fpu_mul(lwc1(at + 0x64u), quarter));
            const float v = fpu_sub(acc, fpu_mul(lwc1(out), one));
            const uint32_t bits = f2u(v);
            if (static_cast<int32_t>((bits & 0x7fffffffu) - still) < 0) sw(out, bits);
            else swc1(out, fpu_sub(v, fpu_mul(v, damp)));
        }
    }
    // the edges to the neighbours' blocks `buf`
    const uint32_t blocks = tiles + block + 0x50u;  // (tile 0's block `buf`)
    auto neighbour = [&](uint32_t k) -> uint32_t {
        const uint32_t i = lbu(rec + 0xcu + k);
        return i == 0xffu ? 0u : blocks + i * 0x1190u;
    };
    if (const uint32_t nb = neighbour(0); nb != 0) {
        for (uint32_t q = 0; q < 4; ++q) {
            lq_sq(nb + 0x480u + q * 0x10u, mine + q * 0x10u);
            lq_sq(nb + 0x500u + q * 0x10u, mine + 0x40u + q * 0x10u);
        }
    }
    if (const uint32_t nb = neighbour(1); nb != 0) {
        for (uint32_t k = 0; k < 16; ++k) {
            sw(nb + 0x4c0u + k * 4u, lwu(mine + k * 0x40u));
            sw(nb + 0x540u + k * 4u, lwu(mine + 4u + k * 0x40u));
        }
    }
    if (const uint32_t nb = neighbour(2); nb != 0) {
        for (uint32_t q = 0; q < 4; ++q) lq_sq(nb + 0x400u + q * 0x10u, mine + 0x3c0u + q * 0x10u);
    }
    if (const uint32_t nb = neighbour(3); nb != 0) {
        for (uint32_t k = 0; k < 16; ++k) sw(nb + 0x440u + k * 4u, lwu(mine + 0x3cu + k * 0x40u));
    }
    if (const uint32_t nb = neighbour(4); nb != 0) {
        const uint32_t w0 = lwu(mine), w1 = lwu(mine + 4u), w2 = lwu(mine + 0x40u);
        sw(nb + 0x58cu, w0);
        sw(nb + 0x590u, w1);
        sw(nb + 0x594u, w2);
    }
    if (const uint32_t nb = neighbour(5); nb != 0) sw(nb + 0x588u, lwu(mine + 0x3cu));
    if (const uint32_t nb = neighbour(6); nb != 0) sw(nb + 0x584u, lwu(mine + 0x3c0u));
    if (const uint32_t nb = neighbour(7); nb != 0) sw(nb + 0x580u, lwu(mine + 0x3fcu));
}

// The step of the `n` tiles at `tiles`: each live one (+0x1e) spread from its current block (0x161218) into the scratchpad's
// grid and stepped into its previous block (0x16121c), which then becomes the current one (the two words swapped).
// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b7f30 L5:0x002cc910 L7:0x002cae68 L11:0x002c70b8 L12:0x002baa48 L13:0x002ae850
void nov_water_grid_step_l1(uint32_t tiles, int32_t n) {
    const uint32_t cur = OV(0x00161218), prev = OV(0x0016121c);
    constexpr uint32_t spr = 0x70000000u;
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t rec = tiles + static_cast<uint32_t>(i) * 0x1190u;
        if (lhu(rec + 0x1eu) == 0) continue;
        nov_water_grid_in_l1(spr, rec + lwu(cur) * 0x5c0u + 0x50u);
        nov_water_grid_wave_l1(spr, i, lw(prev), tiles);
    }
    const uint32_t c = lwu(cur), p = lwu(prev);
    sw(prev, c);
    sw(cur, p);
}

// Block `out` = `a` (1 - t) + `b` t, 92 quadwords (a height block), on VU0 as the original (VMULAw / VMADDx).
// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x00261d78 L5:0x00276d28 L7:0x002754e8 L11:0x00273110 L12:0x00265f50 L13:0x00258fe0
void nov_water_grid_blend_l1(uint32_t out, uint32_t a, uint32_t b, float t) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();
    qmtc2_s32(u, 1, f2u(t));
    u.subbc(1, 0, u.lane(1, 0), vu0l::W);  // (vf1.w = 1 - t, vf1.x = t)
    for (uint32_t q = 0; q < 92; q += 4) {
        for (int k = 0; k < 4; ++k) {
            load_vf(u, 2 + k, a + (q + static_cast<uint32_t>(k)) * 0x10u);
            load_vf(u, 6 + k, b + (q + static_cast<uint32_t>(k)) * 0x10u);
        }
        for (int k = 0; k < 4; ++k) {
            u.mulabc(2 + k, u.lane(1, 3), vu0l::XYZW);
            u.maddbc(2 + k, 6 + k, u.lane(1, 0), vu0l::XYZW);
        }
        for (int k = 0; k < 4; ++k) store_vf(u, 2 + k, out + (q + static_cast<uint32_t>(k)) * 0x10u);
    }
    u.store();
}

// The water's frame for the `n` tiles at `tiles`: when the phase (0x161224) has come within a step (the water's +0x20) of 1,
// a new step (nov_water_grid_step_l1, the phase back to the step) and the block drawn (0x161220) the previous one;
// else the block drawn is the third, each live tile's blended between the previous and the current block by the phase.
// The phase then grows by the step.
// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b7fe0 L5:0x002cc9c0 L7:0x002caf18 L11:0x002c7168 L12:0x002baaf8 L13:0x002ae900
void nov_water_grid_update_l1(uint32_t tiles, int32_t n) {
    const uint32_t water = OV(0x001ca880);
    const uint32_t cur = OV(0x00161218), prev = OV(0x0016121c), shown = OV(0x00161220), phase = OV(0x00161224);
    const float step = lwc1(water + 0x20u);
    if (fpu_sub(1.0f, step) <= lwc1(phase)) {
        swc1(phase, step);
        nov_water_grid_step_l1(tiles, n);
        sw(shown, lwu(prev));
    } else {
        sw(shown, 2);
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t rec = tiles + static_cast<uint32_t>(i) * 0x1190u;
            if (lhu(rec + 0x1eu) == 0) continue;
            nov_water_grid_blend_l1(rec + lwu(shown) * 0x5c0u + 0x50u, rec + lwu(prev) * 0x5c0u + 0x50u, rec + lwu(cur) * 0x5c0u + 0x50u,
                                    lwc1(phase));
        }
    }
    swc1(phase, fpu_add(lwc1(phase), lwc1(water + 0x20u)));
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x00261df8 L5:0x00276da8 L7:0x00275568 L11:0x00273190 L12:0x00265fd0 L13:0x00259060
void nov_water_grid_light_l1(uint32_t tile) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();

    qmtc2_s32(u, 14, 0x3f000000u);
    qmtc2_s32(u, 1, 0x47800040u);
    u.addbc(14, 0, u.lane(1, 0), vu0l::Y);
    qmtc2_s32(u, 1, 0x3f12b021u);
    u.addbc(14, 0, u.lane(1, 0), vu0l::Z);
    qmtc2_s32(u, 1, 0x40500000u);
    const uint32_t c_base = OV(0x001ca880);
    load_vf(u, 15, c_base + 0x10u);
    u.addbc(14, 0, u.lane(1, 0), vu0l::W);
    load_vf(u, 16, c_base + 0x00u);
    u.addbc(19, 0, u.lane(0, 3), vu0l::X);
    load_vf(u, 21, c_base + 0x30u);
    load_vf(u, 20, OV(0x00166c80) + 0x140u);

    for (int r = 10; r <= 13; ++r) u.addbc(r, 0, u.lane(0, 0), vu0l::XYZW);
    u.subbc(10, 10, u.lane(15, 1), vu0l::Y);
    u.addbc(11, 11, u.lane(15, 1), vu0l::Y);
    u.addbc(12, 12, u.lane(15, 1), vu0l::Y);
    u.subbc(13, 13, u.lane(15, 1), vu0l::Y);
    u.subbc(11, 12, u.lane(15, 0), vu0l::X);
    u.addbc(13, 13, u.lane(15, 0), vu0l::X);

    load_vf(u, 17, tile);
    u.addbc(18, 17, u.lane(16, 3), vu0l::Y);

    uint32_t row_ptr = 0x70000070u;
    uint32_t uv_out = 0x70000780u;
    uint32_t col_out = 0x70001088u;

    for (int iy = 0; iy < 17; ++iy, row_ptr += 0x60u) {
        u.addbc(18, 17, u.lane(16, 2), vu0l::X);
        uint32_t h_ptr = row_ptr;
        qmtc2_s32(u, 25, lwu(h_ptr));
        qmtc2_z_s32(u, 26, lwu(h_ptr - 0x4u));
        qmtc2_z_s32(u, 27, lwu(h_ptr + 0x4u));
        qmtc2_z_s32(u, 28, lwu(h_ptr - 0x60u));
        qmtc2_z_s32(u, 29, lwu(h_ptr + 0x60u));
        h_ptr += 4u;

        for (int ix = 0; ix < 17; ++ix, h_ptr += 4u, col_out += 4u, uv_out += 8u) {
            u.addbc(18, 0, u.lane(25, 0), vu0l::Z);
            u.subbc(10, 18, u.lane(29, 2), vu0l::Z);
            u.subbc(11, 29, u.lane(27, 2), vu0l::Z);
            u.subbc(12, 18, u.lane(28, 2), vu0l::Z);
            u.subbc(13, 28, u.lane(26, 2), vu0l::Z);

            u.opmula(11, 10);
            u.opmsub(1, 10, 11);
            u.opmula(13, 12);
            u.opmsub(2, 12, 13);
            u.add(1, 1, 2, vu0l::XYZ);
            u.mul(3, 1, 1, vu0l::XYZ);
            u.addabc(3, u.lane(3, 1), vu0l::X);
            u.maddbc(3, 19, u.lane(3, 2), vu0l::X);
            u.rsqrt(u.lane(0, 3), u.lane(3, 0));

            qmtc2_s32(u, 25, lwu(h_ptr));
            u.addbc(18, 18, u.lane(17, 2), vu0l::Z);
            qmtc2_z_s32(u, 26, lwu(h_ptr - 0x4u));
            qmtc2_z_s32(u, 27, lwu(h_ptr + 0x4u));
            qmtc2_z_s32(u, 28, lwu(h_ptr - 0x60u));
            qmtc2_z_s32(u, 29, lwu(h_ptr + 0x60u));

            u.mulbc(1, 1, u.q, vu0l::XYZ);
            u.sub(2, 20, 18, vu0l::XYZ);
            u.sub(1, 0, 1, vu0l::XYZ);
            u.mul(4, 2, 2, vu0l::XYZ);
            u.addabc(4, u.lane(4, 1), vu0l::X);
            u.maddbc(3, 19, u.lane(4, 2), vu0l::X);
            u.rsqrt(u.lane(0, 3), u.lane(3, 0));

            u.mul(4, 21, 1, vu0l::XYZ);
            u.addabc(4, u.lane(4, 1), vu0l::X);
            u.maddbc(3, 19, u.lane(4, 2), vu0l::X);
            u.subbc(3, 3, u.lane(14, 2), vu0l::X);
            u.mulbc(3, 3, u.lane(14, 3), vu0l::X);
            u.addbc(3, 3, u.lane(14, 2), vu0l::X);
            u.max_lane(3, 0, u.lane(3, 0), u.lane(0, 0));
            u.mini_lane(3, 0, u.lane(3, 0), u.lane(19, 0));

            u.mulbc(2, 2, u.q, vu0l::XYZ);
            u.addbc(3, 3, u.lane(14, 1), vu0l::X);
            u.sub(5, 0, 2, vu0l::XYZ);
            const uint32_t gray = (u.word(3, 0) & 0xffu) * 0x010101u;

            u.mul(4, 5, 1, vu0l::XYZ);
            u.addabc(4, u.lane(4, 1), vu0l::X);
            u.maddbc(4, 19, u.lane(4, 2), vu0l::X);
            const bool neg_dot = u.negative(4, 0);
            u.mulbc(1, 1, u.lane(4, 0), vu0l::XYZ);
            u.sub(1, 1, 5, vu0l::XYZ);
            if (!neg_dot) {
                u.add(1, 1, 1, vu0l::XYZ);
                u.add(2, 5, 1, vu0l::XYZ);
            }

            u.mulbc(2, 2, u.lane(15, 2), vu0l::XY);
            u.mulbc(2, 2, u.lane(14, 0), vu0l::XY);
            u.addbc(2, 2, u.lane(14, 0), vu0l::XY);

            sw(col_out, gray);
            sd(uv_out, static_cast<uint64_t>(u.word(2, 0)) | (static_cast<uint64_t>(u.word(2, 1)) << 32));
            u.addbc(18, 18, u.lane(15, 0), vu0l::X);
        }
        u.addbc(18, 18, u.lane(15, 1), vu0l::Y);
    }

    u.store();
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b8c08 L5:0x002cd5d0 L7:0x002cbb40 L11:0x002c7d78 L12:0x002bb708 L13:0x002af528
void nov_water_grid_xyz_l1(uint32_t tile) {
    const uint32_t c_base = OV(0x001ca880);
    uint32_t out = 0x70002000u;
    float y = fpu_add(lwc1(tile + 0x4u), lwc1(c_base + 0xcu));
    for (int32_t row = 0; row < 17; ++row) {
        float x = fpu_add(lwc1(tile + 0x0u), lwc1(c_base + 0x8u));
        uint32_t h_ptr = 0x70000010u + static_cast<uint32_t>(row + 1) * 0x60u;
        for (int col = 0; col < 17; ++col, h_ptr += 4u, out += 0xcu) {
            swc1(out + 0u, x);
            swc1(out + 4u, y);
            swc1(out + 8u, fpu_add(lwc1(h_ptr), lwc1(tile + 0x8u)));
            x = fpu_add(x, lwc1(c_base + 0x10u));
        }
        y = fpu_add(y, lwc1(c_base + 0x14u));
    }
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b80d0 L5:0x002ccab0 L7:0x002cb008 L11:0x002c7258 L12:0x002babe8 L13:0x002ae9f0
void nov_water_uv_scroll_l1(uint32_t tile) {
    swc1(tile + 0x20u, fpu_add(lwc1(tile + 0x20u), lwc1(tile + 0x28u)));
    wrap_01(tile + 0x20u);
    swc1(tile + 0x24u, fpu_add(lwc1(tile + 0x24u), lwc1(tile + 0x2cu)));
    wrap_01(tile + 0x24u);

    swc1(tile + 0x30u, angle_add(lwc1(tile + 0x30u), lwc1(tile + 0x38u)));
    swc1(tile + 0x34u, angle_add(lwc1(tile + 0x34u), lwc1(tile + 0x3cu)));

    const float du0 = fpu_add(lwc1(tile + 0x20u), fpu_mul(cos_vu(lwc1(tile + 0x30u)), lwc1(tile + 0x40u)));
    const float dv0 = fpu_add(lwc1(tile + 0x24u), fpu_mul(sin_vu(lwc1(tile + 0x30u)), lwc1(tile + 0x40u)));
    const float du1 = fpu_add(lwc1(tile + 0x20u), fpu_mul(cos_vu(lwc1(tile + 0x34u)), lwc1(tile + 0x40u)));
    const float dv1 = fpu_add(lwc1(tile + 0x24u), fpu_mul(sin_vu(lwc1(tile + 0x34u)), lwc1(tile + 0x40u)));

    const uint32_t flags = OV(0x001cad30);
    const uint32_t src = OV(0x001cab60);
    const uint32_t dst = OV(0x001ca9e0);
    for (uint32_t i = 0; i < 46; ++i) {
        const float u0 = lwc1(src + i * 8u + 0u);
        const float v0 = lwc1(src + i * 8u + 4u);
        if (lw(flags + i * 4u) == 0) {
            swc1(dst + i * 8u + 0u, fpu_add(u0, du0));
            swc1(dst + i * 8u + 4u, fpu_add(v0, dv0));
        } else {
            swc1(dst + i * 8u + 0u, fpu_add(u0, du1));
            swc1(dst + i * 8u + 4u, fpu_add(v0, dv1));
        }
    }
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x00262388 L5:0x00277338 L7:0x00275af8 L11:0x00273720 L12:0x00266560 L13:0x002595f0
void nov_water_cell_packet_l1(uint32_t mscal, int32_t cell) {
    const uint32_t ucell = static_cast<uint32_t>(cell);
    uint32_t pkt1 = lwu(CUR);
    uint32_t at = pkt1;
    for (uint32_t i = 0; i < 5; ++i, at += 0x10u) {
        lq_sq(at, OV(0x001cace0) + i * 0x10u);
    }
    lq_sq(at, OV(0x00161240));
    at += 0x10u;

    for (uint32_t i = 0; i < 0x17u; ++i, at += 0x10u) {
        const uint32_t s = OV(0x001ca9e0) + i * 0x10u;
        sd(at + 0u, ld(s + 0u));
        sd(at + 8u, ld(s + 8u));
    }

    const uint32_t rgba_xyz_start = at;
    lq_sq(at, OV(0x00161260));
    at += 0x10u;

    const uint32_t col_base = 0x70001088u + (ucell << 2);
    for (uint32_t i = 0; i < 0x2eu; ++i, at += 4u) {
        const uint32_t off = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(OV(0x001ca8c0) + i * 2u))));
        sw(at, lwu(col_base + off));
    }
    sd(at, 0ull);
    at += 8u;

    lq_sq(at, OV(0x00161280));
    at += 0x10u;

    const uint32_t xyz_base = 0x70002000u + ucell * 12u;
    for (uint32_t i = 0; i < 0x17u; ++i, at += 0x18u) {
        const uint32_t idx_pair = OV(0x001ca980) + i * 4u;
        const uint32_t o0 = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(idx_pair + 0u))));
        const uint32_t o1 = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(idx_pair + 2u))));
        const uint32_t p0 = xyz_base + o0;
        const uint32_t p1 = xyz_base + o1;
        sw(at + 0x00u, lwu(p0 + 0u));
        sw(at + 0x04u, lwu(p0 + 4u));
        sw(at + 0x08u, lwu(p0 + 8u));
        sw(at + 0x0cu, lwu(p1 + 0u));
        sw(at + 0x10u, lwu(p1 + 4u));
        sw(at + 0x14u, lwu(p1 + 8u));
    }

    sw(at + 0x00u, 0x14000000u | mscal);
    sw(at + 0x04u, 0);
    sd(at + 0x08u, 0ull);
    sd(at + 0x10u, 0ull);
    sw(at + 0x14u, 0x11000000u);
    at += 0x18u;
    sw(pkt1, lwu(pkt1) | (((at - pkt1) >> 4) - 1u));

    const uint32_t pkt2 = at;
    for (uint32_t i = 0; i < 5; ++i, at += 0x10u) {
        lq_sq(at, OV(0x001cace0) + i * 0x10u);
    }
    sw(at - 0x3cu, lwu(at - 0x3cu) | 0x01000000u);
    sw(at - 0x28u, lwu(at - 0x28u) | 0x01000000u);

    lq_sq(at, OV(0x00161240));
    at += 0x10u;

    const uint32_t uv_base = 0x70000780u + (ucell << 3);
    for (uint32_t i = 0; i < 0x2eu; ++i, at += 8u) {
        const uint32_t off = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(OV(0x001ca920) + i * 2u))));
        sd(at, ld(uv_base + off));
    }
    sw(pkt2, lwu(pkt2) | (((at - pkt2) >> 4) - 1u));

    sw(at + 0u, 0x30000031u);
    sw(at + 4u, rgba_xyz_start);
    sd(at + 8u, 0ull);
    sw(CUR, at + 0x10u);
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b8cb0 L5:0x002cd678 L7:0x002cbbe8 L11:0x002c7e20 L12:0x002bb7b0 L13:0x002af5d0
void nov_water_cell_l1(uint32_t tile, int32_t cell, int32_t clip) {
    int32_t cur_clip = clip;
    if (cur_clip != 0) {
        const uint32_t c_base = OV(0x001ca880);
        const float off_y = lwc1(c_base + 0xcu);
        const float off_x = lwc1(c_base + 0x8u);
        const float step_y = fpu_mul(-off_y, 0.5f);
        const float step_x = fpu_mul(-off_x, 0.5f);
        const float base_x = fpu_add(lwc1(tile + 0x0u), off_x);
        const float base_y = fpu_add(lwc1(tile + 0x4u), off_y);
        const float y0 = fpu_mul(step_y, int_to_float(cell >> 2));
        const float x0 = fpu_mul(step_x, int_to_float(cell & 3));
        const float y1 = fpu_add(y0, step_y);
        const float x1 = fpu_add(x0, step_x);
        const float wx0 = fpu_add(base_x, x0);
        const float wy1 = fpu_add(base_y, y1);
        const float wx1 = fpu_add(base_x, x1);
        const float wy0 = fpu_add(base_y, y0);
        const float wz = lwc1(tile + 0x8u);

        const Vec4 corners[4] = {
            {wx0, wy0, wz, 1.0f},
            {wx1, wy0, wz, 1.0f},
            {wx0, wy1, wz, 1.0f},
            {wx1, wy1, wz, 1.0f},
        };
        uint32_t inner_codes = 0;
        uint32_t outer_codes = 0;
        clip_corners_4(corners, inner_codes, outer_codes);
        if (all_corners_outside(outer_codes)) return;
        if (cur_clip == 1) {
            if (inner_codes == 0) cur_clip = 0;
        } else {
            cur_clip = 0;
        }
    }

    const int32_t grid_cell = (cell >> 2) * 0x44 + ((cell & 3) << 2);
    nov_water_cell_packet_l1(cur_clip != 0 ? 0xeu : 0xcu, grid_cell);
}

// (Shared: the same code on Rilgar, Umbris, Pokitaru, Hoven and Gemlik.)
// orig: L1:0x002b91c8 L5:0x002cdb90 L7:0x002cc100 L11:0x002c8338 L12:0x002bbcc8 L13:0x002afae8
void nov_water_draw_l1(uint32_t tiles, int32_t n) {
    nov_water_vu7_setup_l1();
    const uint32_t c_base = OV(0x001ca880);
    for (int32_t i = 0; i < n; ++i, tiles += 0x1190u) {
        uint32_t cell_mask = lhu(tiles + 0x1eu);
        if (cell_mask == 0) continue;

        const float tx = lwc1(tiles + 0x0u);
        const float ty = lwc1(tiles + 0x4u);
        const float tz = lwc1(tiles + 0x8u);
        const float ox = lwc1(c_base + 0x8u);
        const float oy = lwc1(c_base + 0xcu);
        const float x_hi = fpu_add(tx, ox);
        const float x_lo = fpu_sub(tx, ox);
        const float y_hi = fpu_add(ty, oy);
        const float y_lo = fpu_sub(ty, oy);

        const Vec4 corners[4] = {
            {x_hi, y_hi, tz, 1.0f},
            {x_lo, y_hi, tz, 1.0f},
            {x_hi, y_lo, tz, 1.0f},
            {x_lo, y_lo, tz, 1.0f},
        };
        uint32_t inner_codes = 0;
        uint32_t outer_codes = 0;
        clip_corners_4(corners, inner_codes, outer_codes);
        if (all_corners_outside(outer_codes)) continue;

        const uint32_t frame_idx = lwu(OV(0x00161220));
        nov_water_grid_in_l1(0x70000000u, tiles + 0x50u + frame_idx * 0x5c0u);
        nov_water_grid_light_l1(tiles);
        nov_water_grid_xyz_l1(tiles);
        nov_water_uv_scroll_l1(tiles);

        const uint32_t flat = lwu(tiles + 0x44u);
        if ((flat & 0x1u) != 0) {
            for (uint32_t k = 0; k < 17; ++k) sw(0x70002008u + k * 0xcu, lwu(tiles + 0x8u));
        }
        if ((flat & 0x2u) != 0) {
            for (uint32_t k = 0; k < 17; ++k) sw(0x700020c8u + k * 0xccu, lwu(tiles + 0x8u));
        }
        if ((flat & 0x4u) != 0) {
            for (uint32_t k = 0; k < 17; ++k) sw(0x70002cc8u + k * 0xcu, lwu(tiles + 0x8u));
        }
        if ((flat & 0x8u) != 0) {
            for (uint32_t k = 0; k < 17; ++k) sw(0x70002008u + k * 0xccu, lwu(tiles + 0x8u));
        }

        int32_t clip_mode = (inner_codes & 0x2f2f2f2fu) != 0 ? 1 : 0;
        if (clip_mode == 0) {
            clip_mode = (outer_codes & 0x2f2f2f2fu) != 0 ? 2 : 0;
        }

        const uint32_t tex_a = lwu(tiles + 0x14u);
        const uint32_t tex_b = lwu(tiles + 0x18u);
        const uint32_t alpha_a = lbu(tiles + 0x1cu);
        const uint32_t alpha_b = lbu(tiles + 0x1du);
        cell_mask = lhu(tiles + 0x1eu);

        if (tex_a != lwu(OV(0x00161228)) || tex_b != lwu(OV(0x0016122c)) ||
            alpha_a != lwu(OV(0x00161230)) || alpha_b != lwu(OV(0x00161234))) {
            const uint32_t gs = lwu(CUR);
            sw(gs + 0x00u, 0x10000005u);
            sw(gs + 0x04u, 0);
            sw(gs + 0x08u, 0x11000000u);
            sw(gs + 0x0cu, 0x50000005u);
            sd(gs + 0x10u, (0x8000ull << 47) | 0x8001ull);
            sd(gs + 0x18u, 0xeeeeull);
            sd(gs + 0x20u, (static_cast<uint64_t>(lbu(tiles + 0x1du)) << 32) | 0x64ull);
            sd(gs + 0x28u, 0x42ull);
            sd(gs + 0x30u, (static_cast<uint64_t>(lbu(tiles + 0x1cu)) << 32) | 0x64ull);
            sd(gs + 0x38u, 0x43ull);
            sd(gs + 0x40u, effect_tex0(lw(tiles + 0x18u)));
            sd(gs + 0x48u, 0x6ull);
            sd(gs + 0x50u, effect_tex0(lw(tiles + 0x14u)));
            sd(gs + 0x58u, 0x7ull);
            sw(CUR, gs + 0x60u);

            sw(OV(0x00161228), lwu(tiles + 0x14u));
            sw(OV(0x0016122c), lwu(tiles + 0x18u));
            sw(OV(0x00161230), lbu(tiles + 0x1cu));
            sw(OV(0x00161234), lbu(tiles + 0x1du));
        }

        for (int32_t cell = 0; cell < 16; ++cell, cell_mask >>= 1) {
            if ((cell_mask & 1u) != 0) {
                nov_water_cell_l1(tiles, cell, clip_mode);
            }
        }
    }

    const uint32_t end_pkt = lwu(CUR);
    sw(end_pkt + 0x0u, 0x10000000u);
    sw(end_pkt + 0x4u, 0);
    sw(end_pkt + 0x8u, 0x13000000u);
    sw(end_pkt + 0xcu, 0);
    sw(CUR, end_pkt + 0x10u);
}

// The draw callbacks: VU1 program 7 set up, then the class's strips (the moby is not read).
// orig: L1:0x002f60f8
void nov_strips_676_draw_l1(uint32_t) {
    const MrpFxScope fx(kMrpFxPrimList);
    nov_strips_vu7_setup_l1();
    nov_strips_draw_l1(8, L1_ADDR(0x001e2fc0u));
}

// orig: L1:0x002f6150
void nov_strips_678_draw_l1(uint32_t) {
    const MrpFxScope fx(kMrpFxPrimList);
    nov_strips_vu7_setup_l1();
    nov_strips_draw_l1(2, L1_ADDR(0x001e3380u));
}

// orig: L1:0x002feb28
void nov_strips_761_draw_l1(uint32_t) {
    const MrpFxScope fx(kMrpFxPrimList);
    nov_strips_vu7_setup_l1();
    nov_strips_draw_l1(4, L1_ADDR(0x001fbc80u));
}

// Class 1225's: its three strips drawn, then their wave height (+0x5c) set for the next frame to 0.5 + 0.5 sin(((frame & 63) - 32)
// x pi / 32) (the frame counter 0x15f5cc).
// orig: L1:0x00309bf8
void nov_strips_1225_draw_l1(uint32_t) {
    {
        const MrpFxScope fx(kMrpFxPrimList);
        nov_strips_vu7_setup_l1();
        nov_strips_draw_l1(3, L1_ADDR(0x00202d80u));
    }
    const float f = fpu_sub(int_to_float(static_cast<int32_t>(lw(L1_ADDR(0x0015f5ccu)) & 0x3f)), u2f(0x42000000u));
    const float s = sin_vu(fpu_mul(f, u2f(0x3dc90fdbu)));
    const float h = fpu_add(fpu_mul(s, u2f(0x3f000000u)), u2f(0x3f000000u));
    uint32_t p = L1_ADDR(0x00202d80u) + 0x11c;
    for (int32_t i = 2; i >= 0; --i, p -= 0x60) swc1(p, h);
}

// The water class's draw callback (Novalis's class 751; the same code on Rilgar, Umbris (class 902), Pokitaru, Hoven and
// Gemlik, each with its own tile records and their count): the level's water tiles drawn.
// orig: L1:0x002fd0c0 L5:0x0030d678 L7:0x0030cd20 L11:0x0030e708 L12:0x002bf748 L13:0x0030a460
void nov_water_751_draw_l1(uint32_t) {
    const MrpFxScope fx(kMrpFxPrimList);
    if (on_level(RILGAR)) nov_water_draw_l1(L5_ADDR(0x001d6880u), 0x30);
    else if (on_level(UMBRIS)) nov_water_draw_l1(L7_ADDR(0x001dce40u), 0x24);
    else if (on_level(POKITARU)) nov_water_draw_l1(L11_ADDR(0x001da880u), 0x14);
    else if (on_level(HOVEN)) nov_water_draw_l1(L12_ADDR(0x001cbf40u), 0x11);
    else if (on_level(GEMLIK)) nov_water_draw_l1(L13_ADDR(0x001d9bc0u), 0x16);
    else nov_water_draw_l1(L1_ADDR(0x001e34c0u), 0x15);
}

// orig: (none) — registers the draw callbacks for the indirect calls (ovl_call_moby_fn)
void novalis_water_fns_register() {
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f60f8u), nov_strips_676_draw_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f6150u), nov_strips_678_draw_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002feb28u), nov_strips_761_draw_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x00309bf8u), nov_strips_1225_draw_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002fd0c0u), nov_water_751_draw_l1);
    novalis_draw_cbs_register();
}

}  // namespace rac1
