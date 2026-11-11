// Hand-written overlay code: Aridia's ocean (class 854), background ship glows (classes 1212 and 1213), and class 1208
// (the Rocket Launcher's rocket).
#include <game/ovl/aridia_ocean.h>

#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/vu0_local.h>
#include <game/render/mrp_fx.h>

namespace rac1 {

namespace {

constexpr LvlConst<2, 0x001611c0u> CUR{};  // DMA chain packet cursor on Aridia

void load_vf(vu0l::Vu& u, int r, uint32_t addr) {
    const uint32_t a = addr & ~15u;
    u.set_words(r, lwu(a), lwu(a + 4), lwu(a + 8), lwu(a + 12));
}

void store_vf(const vu0l::Vu& u, int r, uint32_t addr) {
    const uint32_t a = addr & ~15u;
    sw(a, u.word(r, 0));
    sw(a + 4, u.word(r, 1));
    sw(a + 8, u.word(r, 2));
    sw(a + 12, u.word(r, 3));
}

void qmtc2_s32(vu0l::Vu& u, int r, uint32_t v) {
    u.set_words(r, v, (v & 0x80000000u) != 0 ? 0xffffffffu : 0u, 0u, 0u);
}

bool ocean_box_visible(uint32_t pvar_box_addr) {
    if (lw(L2_ADDR(0x0015f608u)) != 0) return true;
    const int32_t box = lw(pvar_box_addr);
    if (box == -1) return false;
    return point_in_box(L2_ADDR(0x001673c0u), box) != 0;
}

bool all_corners_outside(uint32_t codes) {
    static constexpr uint32_t kPlanes[] = {0x01010101u, 0x02020202u, 0x04040404u, 0x08080808u, 0x20202020u};
    for (uint32_t m : kPlanes) {
        if ((codes & m) == m) return true;
    }
    return false;
}

void unpack_clut_spr(vu0l::Vu& u, uint32_t src_spr, uint32_t dst_spr) {
    for (uint32_t i = 0; i < 0x100u; ++i) {
        const uint32_t c = lwu(src_spr + i * 4u);
        u.set_words(1, c & 0xffu, (c >> 8) & 0xffu, (c >> 16) & 0xffu, (c >> 24) & 0xffu);
        u.itof(1, 1, vu0l::XYZW, 1.0f);
        store_vf(u, 1, dst_spr + i * 16u);
    }
}

uint32_t pack_rgba_vf(const vu0l::Vu& u, int r) {
    return (u.word(r, 0) & 0xffu) | ((u.word(r, 1) & 0xffu) << 8) | ((u.word(r, 2) & 0xffu) << 16) |
           ((u.word(r, 3) & 0xffu) << 24);
}

void draw_bg_ship_glows(uint32_t m, int32_t glow_count, uint32_t col_off, float size, float fade) {
    const uint32_t sp = ee_stack_alloc(0x10);
    group_first(sp, static_cast<int32_t>(lbu(m + 0x21)), 0, 0);
    uint32_t cur = lwu(sp);
    while (cur != 0) {
        if (lbu(cur + 0x31) != 0) {
            const uint32_t pv = lwu(cur + 0x78);
            if (lh(cur + 0xa6) == lh(m + 0xa6) && lbu(pv + col_off) >= 129u) {
                uint32_t pos = pv + 0x80u;
                for (int32_t i = 0; i < glow_count; ++i, pos += 0x10u) {
                    ari_glow_quad_l2(pos, lwu(pv + col_off), size, fade);
                }
            }
        }
        group_next(sp, lwu(sp), 0, 0);
        cur = lwu(sp);
    }
    ee_stack_free(0x10);
}

}  // namespace

// orig: L2:0x002ea048
void ari_ocean_draw_tr_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    ari_ocean_setup_l2(0xfu, 0xfu, 0x19u, 0x1eu, 0x28u, 0x40u, L2_ADDR(0x001f3e80u), 0.0f, u2f(0x487dfc00u),
                       u2f(0x437f0000u), 0.0f);

    if (ocean_box_visible(pv + 0x0)) {
        ari_ocean_record_l2(L2_ADDR(0x001f3c00u));
    }
    if (ocean_box_visible(pv + 0x4)) {
        ari_ocean_record_l2(L2_ADDR(0x001f3cd0u));
        ari_ocean_record_l2(L2_ADDR(0x001f3d30u));
        ari_ocean_record_l2(L2_ADDR(0x001f3d90u));
        ari_ocean_record_l2(L2_ADDR(0x001f3dd0u));
        ari_ocean_record_l2(L2_ADDR(0x001f3e10u));
    }
    if (ocean_box_visible(pv + 0x8)) {
        ari_ocean_record_l2(L2_ADDR(0x001f3c60u));
    }
    ari_ocean_end_l2();
}

// orig: L2:0x002a4818
void ari_ocean_setup_l2(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t h0, uint32_t h1, uint32_t tex,
                        float f12, float f13, float f14, float f15) {
    const uint32_t sp = ee_stack_alloc(0x50);
    sw(sp + 0x1c, tex);
    swc1(sp + 0x20, f12);
    swc1(sp + 0x24, f13);
    swc1(sp + 0x28, f14);
    swc1(sp + 0x2c, f15);
    sb(sp + 0x30, c0);
    sb(sp + 0x31, c1);
    sb(sp + 0x32, c2);
    sb(sp + 0x33, c3);
    sh(sp + 0x3c, h0);
    sb(sp + 0x3e, h1);
    sb(sp + 0x3f, 0x80u);
    ari_ocean_vu7_setup_l2(sp);
    ari_spr_dma_to_l2(L2_ADDR(0x001cb400u), 0x70002800u, 0xf8u);
    ee_stack_free(0x50);
}

// orig: L2:0x002a40f0
void ari_ocean_vu7_setup_l2(uint32_t rec) {
    const uint32_t view = L2_ADDR(0x0016d040u);
    sw(L2_ADDR(0x001612f8u), lwu(view + 0x218));
    sw(L2_ADDR(0x001612fcu), lwu(view + 0x21c));
    sw(L2_ADDR(0x00161300u), lwu(view + 0x228));
    sw(L2_ADDR(0x00161304u), lwu(view + 0x22c));
    if (lw(L2_ADDR(0x0015f458u)) != 1) {
        sw(view + 0x218, lwu(rec + 0x20));
        sw(view + 0x21c, lwu(rec + 0x24));
        sw(view + 0x228, lwu(rec + 0x28));
        sw(view + 0x22c, lwu(rec + 0x2c));
    }

    projection_setup();
    render_view_setup();

    const uint32_t cam = L2_ADDR(0x001673c0u);
    Mat4 scale_m{};
    mat4_scale_diag(scale_m, 1024.0f);
    vec3_scale(scale_m.r[3], V(cam), -1024.0f);
    scale_m.r[3].w = 1.0f;

    packet_ref(0x0010e810u, lhu(0x0010e800u));

    const uint32_t s0 = lwu(CUR);
    sw(s0 + 0x00, 0x10000000u);
    sw(s0 + 0x04, 0);
    sw(s0 + 0x08, 0x11000000u);
    sw(s0 + 0x0c, 0x01000404u);
    sw(s0 + 0x10, 0);
    sw(s0 + 0x14, 0);
    sw(s0 + 0x18, 0);
    sw(s0 + 0x1c, 0x6c0c43a4u);
    mat4_mul(M(s0 + 0x20), M(cam - 0x100u), scale_m);
    mat4_mul(M(s0 + 0x60), M(cam - 0x80u), scale_m);
    sw(s0 + 0xa0, 0x8000u);
    sw(s0 + 0xa4, 0x303ec000u);
    sw(s0 + 0xa8, 0x412u);
    sw(s0 + 0xac, lwu(view + 0x210));
    lq_sq(s0 + 0xb0, view + 0x190u);
    lq_sq(s0 + 0xc0, view + 0x1a0u);
    sw(s0 + 0xd0, lwu(view + 0x22c));
    sw(s0 + 0xd4, lwu(view + 0x228));
    sw(s0 + 0xd8, 0);
    sw(s0 + 0xdc, 0);
    sw(s0 + 0xe0, 0x03000000u);
    sw(s0 + 0xe4, 0x020001d2u);
    sw(s0 + 0xe8, 0x15000000u);
    sw(s0 + 0xec, 0);
    mrp_fx_note_vu7(s0 + 0x20);

    const uint32_t after_vu = s0 + 0xf0u;
    sw(s0, lwu(s0) | static_cast<uint32_t>((static_cast<int32_t>(after_vu - s0) >> 4) - 1));
    sw(after_vu + 0x00, 0x30000003u);
    sw(after_vu + 0x04, L2_ADDR(0x001cb8f0u));
    sw(after_vu + 0x08, 0);
    sw(after_vu + 0x0c, 0x50000003u);
    sw(after_vu + 0x10, 0x10000009u);
    sw(after_vu + 0x14, 0);
    sw(after_vu + 0x18, 0);
    sw(after_vu + 0x1c, 0x50000009u);
    sw(CUR, after_vu + 0x20u);

    const int32_t frame = lw(L2_ADDR(0x0015f5ccu));
    const int32_t period = static_cast<int32_t>(lbu(rec + 0x33));
    if (period == 0) eer::ee_break(7);
    const int32_t nframes = static_cast<int32_t>(lbu(rec + 0x3e));
    const uint32_t base_frame = lhu(rec + 0x3c);
    const float f_div = fpu_div(int_to_float(frame), int_to_float(period));
    const int32_t step = ee_div(frame, period);
    const int32_t f0 = ee_rem(step, nframes);
    const int32_t f1 = ee_rem(step + 1, nframes);
    const float frac = fpu_sub(f_div, int_to_float(step));

    const uint32_t tex_buf = lwu(rec + 0x1c);
    const uint32_t tex_data = lwu(L2_ADDR(0x0015f420u));
    const uint32_t tbl = L2_ADDR(0x0016d780u);
    const uint32_t e0 = tbl + ((base_frame + static_cast<uint32_t>(f0)) << 4);
    const uint32_t e1 = tbl + ((base_frame + static_cast<uint32_t>(f1)) << 4);
    const uint32_t idx0 = tex_data + (static_cast<uint32_t>(lhu(e0 + 0x8)) << 4);
    const uint32_t clut0 = tex_data + (static_cast<uint32_t>(lhu(e0 + 0xa)) << 4);
    const uint32_t idx1 = tex_data + (static_cast<uint32_t>(lhu(e1 + 0x8)) << 4);
    const uint32_t clut1 = tex_data + (static_cast<uint32_t>(lhu(e1 + 0xa)) << 4);

    sd(tex_buf + 0x0, 0x0800000000000400ull);
    sd(tex_buf + 0x8, 0ull);
    ari_ocean_tex_upload_l2(tex_buf + 0x10u, idx0, clut0, idx1, clut1, 0x1000u, frac);

    const int32_t vram_ptr = lw(0x0015ee74u);
    const uint64_t tex0_val =
        static_cast<uint64_t>(static_cast<int64_t>(vram_ptr >> 8)) | 0x18100000ull | (0xb000ull << 19);
    sd(L2_ADDR(0x001612f0u), tex0_val);

    uint64_t fogcol = static_cast<uint64_t>(lbu(rec + 0x30)) | (static_cast<uint64_t>(lbu(rec + 0x31)) << 8) |
                      (static_cast<uint64_t>(lbu(rec + 0x32)) << 16);
    if (lw(L2_ADDR(0x0015f458u)) == 1) {
        const uint64_t r = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x230)));
        const uint64_t g = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x234) << 8));
        const uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x238) << 16));
        fogcol = r | g | b;
    }

    const uint32_t gs = lwu(CUR);
    const uint32_t alpha_fix = lbu(rec + 0x3f);
    sd(gs + 0x00, 0x8000000000008001ull);
    sd(gs + 0x08, 0xeeeeeeeeull);
    sd(gs + 0x10, (static_cast<uint64_t>(alpha_fix) << 32) | 0x64ull);
    sd(gs + 0x18, 0x42ull);
    sd(gs + 0x20, ld(L2_ADDR(0x001612f0u)));
    sd(gs + 0x28, 0x6ull);
    sd(gs + 0x30, fogcol);
    sd(gs + 0x38, 0x3dull);
    sd(gs + 0x40, (static_cast<uint64_t>(static_cast<int64_t>(lw(0x0015ee74u)) >> 8) << 32) | (0x8000ull << 33));
    sd(gs + 0x48, 0x50ull);
    sd(gs + 0x50, 0ull);
    sd(gs + 0x58, 0x51ull);
    sd(gs + 0x60, (0x8000ull << 23) | 0x40ull);
    sd(gs + 0x68, 0x52ull);
    sd(gs + 0x70, 0ull);
    sd(gs + 0x78, 0x53ull);
    sd(gs + 0x80, alpha_fix > 0x60u ? 0x5360aull : 0x5370bull);
    sd(gs + 0x88, 0x47ull);

    const uint32_t img = gs + 0x90u;
    sw(img + 0x00, 0x30000401u);
    sw(img + 0x04, lwu(rec + 0x1c));
    sw(img + 0x08, 0);
    sw(img + 0x0c, 0x50000401u);

    const uint32_t fl = img + 0x10u;
    sw(fl + 0x00, 0x10000002u);
    sw(fl + 0x04, 0);
    sw(fl + 0x08, 0);
    sw(fl + 0x0c, 0x50000002u);
    sd(fl + 0x10, 0x1000000000008001ull);
    sd(fl + 0x18, 0xeull);
    sd(fl + 0x20, 0ull);
    sd(fl + 0x28, 0x3full);
    sw(CUR, fl + 0x30u);

    sw(0x0015ee74u, lwu(0x0015ee74u) + 0x4000u);
}

// orig: L2:0x002101b8
void ari_spr_dma_to_l2(uint32_t madr, uint32_t sadr, uint32_t qwc) {
    eer::hw_sw(0x1000d480u, sadr);
    eer::hw_sw(0x1000d410u, madr);
    eer::hw_sw(0x1000d420u, qwc);
    eer::hw_sw(0x1000d400u, 0x100u);
}

// orig: L2:0x002a4880
void ari_ocean_end_l2() { ari_ocean_end_body_l2(0); }

// orig: L2:0x002a46e0
void ari_ocean_end_body_l2(uint32_t) {
    ari_vif_code_l2(0x13000000u);

    const uint32_t view = L2_ADDR(0x0016d040u);
    sw(view + 0x218, lwu(L2_ADDR(0x001612f8u)));
    sw(view + 0x21c, lwu(L2_ADDR(0x001612fcu)));
    sw(view + 0x228, lwu(L2_ADDR(0x00161300u)));
    sw(view + 0x22c, lwu(L2_ADDR(0x00161304u)));

    projection_setup();
    render_view_setup();

    const uint32_t p = lwu(CUR);
    sw(p + 0x00, 0x10000002u);
    sw(p + 0x04, 0);
    sw(p + 0x08, 0x11000000u);
    sw(p + 0x0c, 0x50000002u);
    sd(p + 0x10, 0x1000000000008001ull);
    sd(p + 0x18, 0xeull);
    const uint64_t r = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x230)));
    const uint64_t g = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x234))) << 8;
    const uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x238))) << 16;
    sd(p + 0x20, r | g | b);
    sd(p + 0x28, 0x3dull);
    sw(CUR, p + 0x30u);

    packet_ref_1c24f0();
}

// orig: L2:0x002a0368
void ari_vif_code_l2(uint32_t code) {
    const uint32_t p = lwu(CUR);
    sw(p + 0x0, 0x10000000u);
    sw(p + 0x4, 0);
    sw(p + 0x8, 0);
    sw(p + 0xc, code);
    sw(CUR, p + 0x10u);
}

// orig: L2:0x002a48a0
void ari_ocean_record_l2(uint32_t rec) {
    ari_ocean_cells_clip_l2(rec);
    ari_ocean_cells_draw_l2(rec);
}

// orig: L2:0x0024f748
void ari_ocean_cells_clip_l2(uint32_t rec) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();

    const uint32_t vbase = L2_ADDR(0x00167280u);
    for (int i = 0; i < 8; ++i) {
        load_vf(u, 16 + i, vbase + 0xc0u + static_cast<uint32_t>(i) * 0x10u);
    }
    load_vf(u, 31, vbase + 0x140u);
    load_vf(u, 13, rec + 0x10u);
    qmtc2_s32(u, 2, 0x40e00000u);
    load_vf(u, 24, rec + 0x00u);
    u.addbc(30, 0, u.lane(0, 3), vu0l::XYZ);
    qmtc2_s32(u, 15, 0x3f000000u);
    u.mulbc(13, 13, u.lane(2, 0), vu0l::XY);
    u.addbc(13, 0, u.lane(13, 0), vu0l::Z);
    u.mulbc(13, 0, u.lane(13, 1), vu0l::W);
    qmtc2_s32(u, 2, 0x44800000u);
    u.mulbc(13, 13, u.lane(15, 0), vu0l::Z | vu0l::W);
    u.addbc(15, 0, u.lane(2, 0), vu0l::Y);

    u.addbc(25, 24, u.lane(0, 1), vu0l::Y);
    u.addbc(26, 24, u.lane(0, 1), vu0l::Y);
    u.addbc(27, 24, u.lane(13, 1), vu0l::Y);
    u.addbc(28, 24, u.lane(13, 1), vu0l::Y);
    for (int r = 25; r <= 28; ++r) {
        u.addbc(r, 24, u.lane(0, 2), vu0l::Z);
    }
    u.addbc(29, 24, u.lane(13, 3), vu0l::Y);
    u.addbc(29, 24, u.lane(0, 2), vu0l::Z);

    uint32_t out_vis = 0x70002000u;
    uint32_t mask_ptr = lwu(rec + 0x38u);
    int32_t rem_y = static_cast<int32_t>(lhu(rec + 0x1au));
    do {
        u.addbc(25, 24, u.lane(0, 0), vu0l::X);
        u.addbc(26, 24, u.lane(13, 0), vu0l::X);
        u.addbc(27, 24, u.lane(0, 0), vu0l::X);
        u.addbc(28, 24, u.lane(13, 0), vu0l::X);
        u.addbc(29, 24, u.lane(13, 2), vu0l::X);
        int32_t rem_x = static_cast<int32_t>(lhu(rec + 0x18u));
        do {
            uint32_t vis = 0;
            const int8_t enabled = static_cast<int8_t>(lbu(mask_ptr++));
            if (enabled != 0) {
                u.sub(1, 29, 31, vu0l::XYZ);
                u.mul(1, 1, 1, vu0l::XYZ);
                u.addabc(1, u.lane(1, 1), vu0l::X);
                u.maddbc(1, 30, u.lane(1, 2), vu0l::X);
                u.subbc_flags(1, u.lane(24, 3), vu0l::X);
                if ((u.mac & 0x80u) != 0) {
                    for (int k = 0; k < 4; ++k) {
                        u.sub(1 + k, 25 + k, 31, vu0l::XYZ);
                    }
                    for (int k = 0; k < 4; ++k) {
                        u.mulbc(1 + k, 1 + k, u.lane(15, 1), vu0l::XYZ);
                    }
                    u.transform(6, 16, 17, 18, 19, 1, u.lane(0, 3));
                    u.transform(5, 20, 21, 22, 23, 1, u.lane(0, 3));

                    uint32_t inner_codes = 0;
                    uint32_t outer_codes = 0;
                    for (int next = 2; next <= 4; ++next) {
                        u.clipw(6);
                        u.transform(6, 16, 17, 18, 19, next, u.lane(0, 3));
                        inner_codes = (inner_codes << 8) | (u.clip & 0x3fu);

                        u.clipw(5);
                        u.transform(5, 20, 21, 22, 23, next, u.lane(0, 3));
                        outer_codes = (outer_codes << 8) | (u.clip & 0x3fu);
                    }
                    u.clipw(6);
                    inner_codes = (inner_codes << 8) | (u.clip & 0x3fu);
                    u.clipw(5);
                    outer_codes = (outer_codes << 8) | (u.clip & 0x3fu);

                    vis = (inner_codes & 0x2f2f2f2fu) != 0 ? 2u : 1u;
                    if (all_corners_outside(outer_codes)) vis = 0;
                }
            }
            sb(out_vis++, vis);
            for (int r = 25; r <= 29; ++r) {
                u.addbc(r, r, u.lane(13, 0), vu0l::X);
            }
            rem_x -= 7;
        } while (rem_x > 0);

        for (int r = 25; r <= 29; ++r) {
            u.addbc(r, r, u.lane(13, 1), vu0l::Y);
        }
        rem_y -= 7;
    } while (rem_y > 0);

    u.store();
}

// orig: L2:0x0024fa80
void ari_ocean_cells_draw_l2(uint32_t rec) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();

    load_vf(u, 13, rec + 0x10u);
    qmtc2_s32(u, 2, 0x40e00000u);
    load_vf(u, 24, rec + 0x00u);
    u.addbc(13, 0, u.lane(13, 0), vu0l::Z);
    u.mulbc(13, 0, u.lane(13, 1), vu0l::W);
    u.mulbc(13, 13, u.lane(2, 0), vu0l::XY);
    u.addbc(29, 24, u.lane(0, 2), vu0l::Z);
    u.addbc(29, 24, u.lane(0, 1), vu0l::Y);

    uint32_t vis_ptr = 0x70002000u;
    uint32_t uv_ptr = lwu(rec + 0x34u);
    int32_t rem_y = static_cast<int32_t>(lhu(rec + 0x1au));
    do {
        u.addbc(29, 24, u.lane(0, 0), vu0l::X);
        int32_t rem_x = static_cast<int32_t>(lhu(rec + 0x18u));
        do {
            const int32_t vis = static_cast<int32_t>(static_cast<int8_t>(lbu(vis_ptr++)));
            if (vis != 0) {
                uint32_t xy_out = 0x70000000u;
                u.addbc(25, 29, u.lane(0, 1), vu0l::Y);
                for (int iy = 0; iy < 8; ++iy) {
                    u.addbc(25, 29, u.lane(0, 0), vu0l::X);
                    for (int ix = 0; ix < 8; ++ix) {
                        sd(xy_out, static_cast<uint64_t>(u.word(25, 0)) | (static_cast<uint64_t>(u.word(25, 1)) << 32));
                        u.addbc(25, 25, u.lane(13, 2), vu0l::X);
                        xy_out += 8u;
                    }
                    u.addbc(25, 25, u.lane(13, 3), vu0l::Y);
                }

                uint32_t pkt = lwu(CUR);
                uint32_t at = pkt;
                uint32_t tmpl = L2_ADDR(0x001c4620u);
                for (int i = 0; i < 5; ++i, tmpl += 0x10u, at += 0x10u) {
                    lq_sq(at, tmpl);
                }
                sw(at - 0x40u, lwu(at - 0x40u) | 0x7cu);
                lq_sq(at, L2_ADDR(0x00160f00u));
                at += 0x10u;
                u.addbc(1, 0, u.lane(29, 2), vu0l::X);
                sw(at - 0x4u, lwu(at - 0x4u) | (0x7cu << 16));
                const uint32_t z_word = u.word(1, 0);

                uint32_t idx_ptr = 0x70002800u;
                for (int i = 0; i < 0x7c; ++i, idx_ptr += 2u, at += 0xcu) {
                    const uint32_t off = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(idx_ptr))));
                    const uint32_t src_xy = 0x70000000u | off;
                    sw(at + 0x8u, z_word);
                    sw(at + 0x0u, lwu(src_xy + 0x0u));
                    sw(at + 0x4u, lwu(src_xy + 0x4u));
                }
                while ((at & 0xfu) != 0) {
                    sw(at, 0);
                    at += 4u;
                }
                sw(pkt, lwu(pkt) | (((at - pkt) >> 4) - 1u));

                pkt = at;
                sw(pkt + 0x00u, 0x30000020u);
                sw(pkt + 0x04u, uv_ptr);
                sd(pkt + 0x08u, 0ull);
                pkt += 0x10u;

                sw(pkt + 0x00u, 0x3000003fu);
                sw(pkt + 0x04u, L2_ADDR(0x001cb9c0u));
                sd(pkt + 0x08u, 0ull);
                pkt += 0x10u;

                sw(pkt + 0x00u, 0x10000000u);
                sw(pkt + 0x04u, 0);
                sw(pkt + 0x08u, 0x15000000u | static_cast<uint32_t>(((vis - 1) << 1) + 0xc));
                sw(pkt + 0x0cu, 0);
                sw(CUR, pkt + 0x10u);
            }
            uv_ptr += 0x200u;
            rem_x -= 7;
            u.addbc(29, 29, u.lane(13, 0), vu0l::X);
        } while (rem_x > 0);

        rem_y -= 7;
        u.addbc(29, 29, u.lane(13, 1), vu0l::Y);
    } while (rem_y > 0);

    u.store();
}

// orig: L2:0x00264b50
void ari_glow_quad_l2(uint32_t pos, uint32_t rgba, float size, float fade) {
    const uint32_t sp = ee_stack_alloc(0x90);
    sd(sp + 0x70u, 5ull);
    sd(sp + 0x78u, effect_tex0(0xb));
    sd(sp + 0x80u, 0x0000ff9000000260ull);
    sd(sp + 0x88u, (0x8000ull << 24) | 0x48ull);
    sw(sp + 0x50u, 0);
    sw(sp + 0x54u, 0);
    sw(sp + 0x58u, 0);
    swc1(sp + 0x5cu, 1.0f);
    swc1(sp + 0x60u, 1.0f);
    sw(sp + 0x64u, 0);
    swc1(sp + 0x68u, 1.0f);
    swc1(sp + 0x6cu, 1.0f);

    const Vec4 up{0.0f, 0.0f, 1.0f, 1.0f};
    Mat4 basis{};
    basis.r[3] = V(pos);
    vec3_sub(basis.r[0], V(L2_ADDR(0x001673c0u)), basis.r[3]);
    vec3_set_length(basis.r[0], basis.r[0], 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], up);
    vec3_set_length(basis.r[1], basis.r[1], 1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    Vec4 pull{};
    vec3_scale(pull, basis.r[0], fade);
    vec3_add(basis.r[3], basis.r[3], pull);

    const uint32_t corners = L2_ADDR(0x001b0a70u);
    for (uint32_t i = 0; i < 4; ++i) {
        sw(sp + 0x40u + i * 4u, rgba);
        Vec4 c{};
        vec3_scale(c, V(corners + i * 0x10u), size);
        mat4_transform(V(sp + i * 0x10u), c, basis);
    }

    quad_draw(sp, 0);
    ee_stack_free(0x90);
}

// orig: L2:0x002ec308
void ari_bg_ship1_glow_draw_l2(uint32_t m) {
    draw_bg_ship_glows(m, 3, 0xb0u, u2f(0x3f666666u), u2f(0x3ee66666u));
}

// orig: L2:0x002ecc68
void ari_bg_ship2_glow_draw_l2(uint32_t m) {
    draw_bg_ship_glows(m, 6, 0xe0u, u2f(0x3f99999au), u2f(0x3f11eb85u));
}

// orig: L2:0x002ebf20
void ari_class1208_update_l2(uint32_t m) {
    const uint8_t state = lbu(m + 0x20u);
    const uint32_t pv = lwu(m + 0x78u);
    if (state == 0) {
        moby_retire(m);
        return;
    }
    if (state != 1) return;

    const uint32_t pos_addr = m + 0x10u;
    vec3_add(V(pos_addr), V(pos_addr), V(pv));
    approach_f(pv + 0x8u, 0.0f, fpu_mul(lwc1(L2_ADDR(0x00161f58u)), lwc1(0x0015ed70u)));
    const float xy_len = vec2_length(V(pv));
    swc1(m + 0x44u, fpu_sub(u2f(0x3fc90fdbu), atan2_f(xy_len, lwc1(pv + 0x8u))));

    const uint32_t sp = ee_stack_alloc(0x40);
    lq_sq(sp, pos_addr);
    vec3_scale(V(sp + 0x20u), V(pv), 0.5f);
    vec3_scale(V(sp + 0x10u), V(pv), u2f(0xbdcccccd));

    for (int k = 0; k < 2; ++k) {
        const int32_t life = frame_scale_i(float_to_int(rand_range_f(20.0f, 60.0f)));
        const int32_t col = lw(L2_ADDR(0x00161f5cu));
        const uint32_t rgb = static_cast<uint32_t>(col) & 0x00ffffffu;
        const uint32_t alpha = static_cast<uint32_t>(col >> 24);
        const float sz = rand_range_f(u2f(0x48927c00u), u2f(0x48f42400u));
        const uint32_t p = dev_2712b8(sp, sp + 0x10u, static_cast<uint32_t>(life), alpha, rgb, 3u, sz,
                                      u2f(0x461c4000u), 1.0f, u2f(0xb951b717u), 0.0f);
        if (p != 0) {
            sb(p + 0x2u, lbu(lwu(L2_ADDR(0x001b2680u) + 0x5cu)));
            sb(p + 0x9u, static_cast<uint32_t>(float_to_int(4.0f) - 0x80));
        }
        vec3_add(V(sp), V(sp), V(sp + 0x20u));
        rand_sphere_point(sp + 0x30u, 0.5f, 1.5f);
        vec3_add(V(sp), V(sp), V(sp + 0x30u));
    }

    const int32_t trail_life = frame_scale_i(6);
    const uint32_t trail_rgb = lwu(L2_ADDR(0x00161f5cu)) & 0x00ffffffu;
    const uint32_t trail_alpha = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(L2_ADDR(0x00161f63u)))));
    const uint32_t p2 = dev_2712b8(pos_addr, sp + 0x10u, static_cast<uint32_t>(trail_life), trail_alpha, trail_rgb, 3u,
                                   u2f(0x48742400u), u2f(0x459c4000u), 1.0f, u2f(0xb9d1b717u), 0.0f);
    if (p2 != 0) {
        sb(p2 + 0x9u, static_cast<uint32_t>(float_to_int(4.0f) - 0x80));
    }
    ee_stack_free(0x40);

    if (lbu(m + 0x31u) == 0) {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x10u)));
    } else {
        sw(pv + 0x10u, static_cast<uint32_t>(frame_scale_i(0x3c)));
    }
    if (lw(pv + 0x10u) == 0) {
        moby_retire(m);
    }
}

// orig: L2:0x0024fca0
void ari_ocean_tex_upload_l2(uint32_t dst, uint32_t idx0, uint32_t clut0, uint32_t idx1, uint32_t, uint32_t count,
                             float blend) {
    if (vu0::running()) vu0::drain();
    vu0l::Vu u;
    u.load();

    qmtc2_s32(u, 31, f2u(blend));
    u.subbc(31, 0, u.lane(31, 0), vu0l::W);
    while ((eer::hw_lw(0x1000d400u) & 0x100u) != 0) {}

    eer::hw_sw(0x1000e020u, eer::hw_lw(0x1000e020u) | 0x200u);
    eer::hw_sw(0x1000e010u, 0x200u);
    eer::hw_sw(0x1000d410u, clut0);
    eer::hw_sw(0x1000d480u, 0x70000c00u);
    eer::hw_sw(0x1000d420u, 0x40u);
    eer::hw_sw(0x1000d400u, 0x100u);

    eer::hw_sw(0x1000e010u, 0x200u);
    eer::hw_sw(0x1000d410u, idx0);
    eer::hw_sw(0x1000d480u, 0x70001000u);
    eer::hw_sw(0x1000d420u, count >> 4);
    eer::hw_sw(0x1000d400u, 0x100u);
    unpack_clut_spr(u, 0x70000c00u, 0x70000000u);

    // Preserves the original assembly's register quirk: the second CLUT/indices DMA also transfers clut0/idx0.
    eer::hw_sw(0x1000e010u, 0x200u);
    eer::hw_sw(0x1000d410u, clut0);
    eer::hw_sw(0x1000d480u, 0x70002c00u);
    eer::hw_sw(0x1000d420u, 0x40u);
    eer::hw_sw(0x1000d400u, 0x100u);

    eer::hw_sw(0x1000e010u, 0x200u);
    eer::hw_sw(0x1000d410u, idx0);
    eer::hw_sw(0x1000d480u, 0x70003000u);
    eer::hw_sw(0x1000d420u, count >> 4);
    eer::hw_sw(0x1000d400u, 0x100u);
    unpack_clut_spr(u, 0x70002c00u, 0x70002000u);

    for (uint32_t rem = count >> 2; rem != 0; --rem, idx0 += 4u, idx1 += 4u, dst += 0x10u) {
        for (int k = 0; k < 4; ++k) {
            load_vf(u, 2 + 2 * k, 0x70000000u + (lbu(idx0 + static_cast<uint32_t>(k)) << 4));
            load_vf(u, 3 + 2 * k, 0x70002000u + (lbu(idx1 + static_cast<uint32_t>(k)) << 4));
        }
        for (int k = 0; k < 4; ++k) u.mulbc(2 + 2 * k, 2 + 2 * k, u.lane(31, 3), vu0l::XYZW);
        for (int k = 0; k < 4; ++k) u.mulbc(3 + 2 * k, 3 + 2 * k, u.lane(31, 0), vu0l::XYZW);
        for (int k = 0; k < 4; ++k) u.add(2 + 2 * k, 2 + 2 * k, 3 + 2 * k, vu0l::XYZW);
        for (int k = 0; k < 4; ++k) u.ftoi(2 + 2 * k, 2 + 2 * k, vu0l::XYZW, 1.0f);
        for (int k = 0; k < 4; ++k) sw(dst + static_cast<uint32_t>(k) * 4u, pack_rgba_vf(u, 2 + 2 * k));
    }

    u.store();
}

// orig: (host) L2:0x2ea048 inside the MRP's prim-list scope (its set-up L2:0x2a40f0 notes the matrix as 0x1fc740 does)
void ari_ocean_draw_l2(uint32_t m) {
    const MrpFxScope fx(kMrpFxPrimList);
    ari_ocean_draw_tr_l2(m);
}

// orig: (none) -- registers the ocean's / glows' functions for the indirect calls (ovl_call_moby_fn)
void aridia_ocean_fns_register() {
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ea048u), ari_ocean_draw_l2);           // class 854's draw callback
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ec308u), ari_bg_ship1_glow_draw_l2);  // class 1212's draw callback
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ecc68u), ari_bg_ship2_glow_draw_l2);  // class 1213's draw callback
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ebf20u), ari_class1208_update_l2);    // class 1208's update
}

}  // namespace rac1
