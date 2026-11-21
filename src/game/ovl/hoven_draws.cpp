// Hoven's (level 12) own draw-list callbacks: class 384's steam vent quad strip (0x2ecac0), class 1259's electric arc quad
// strips (0x3028c8), class 1267's turret HUD (0x304218) and exit fade (0x304d98), class 1345's energy barrier quads
// (0x308350), and class 1901's UV-scrolled waterfall / ice meshes (0x30be68, calling shadows_vu1_setup for L12:0x21f300 /
// L12:0x222928). Level 12 only: `orig: L12:`, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/variants_l12.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

uint64_t sx(uint32_t w) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w))); }

uint64_t alpha_mode(uint32_t words, uint64_t fix) {
    return sx(lwu(words)) | (sx(lwu(words + 4)) << 2) | (sx(lwu(words + 8)) << 4) | (sx(lwu(words + 0xc)) << 6) | fix;
}

constexpr uint64_t kClampRepeat = (0xff90ull << 32) | 0x260ull;

uint64_t gs_xyz_16(int32_t x_px, int32_t y_16) {
    constexpr uint64_t kBase = 0xfffff000ull << 24;
    const uint64_t y_part = static_cast<uint64_t>((static_cast<uint32_t>(y_16) << 16) - 0x80000000u);
    const uint64_t x_part = sx((static_cast<uint32_t>(x_px) << 4) + 0x8000u);
    return kBase + y_part + x_part;
}

uint64_t gs_xyz_subpx(int32_t x_16, int32_t y_16) {
    constexpr uint64_t kBase = 0xfffff000ull << 24;
    const uint64_t y_part = static_cast<uint64_t>((static_cast<uint32_t>(y_16) << 16) - 0x80000000u);
    const uint64_t x_part = sx(static_cast<uint32_t>(x_16) + 0x8000u);
    return kBase + y_part + x_part;
}
}  // namespace

// Class 384's draw callback: draws 8 curved steam-vent quads around `m` at height offset `pv + 8`, pulsing colour by `pv + 0`.
// orig: L12:0x002ecac0
void hov_cb_2ecac0_l12(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp, mat = sp + 0x90, pv = lwu(m + 0x78);
    mat3_copy_affine(M(mat), M(m + 0xc0));
    lq_sq(sp + 0xc0, m + 0x10);
    const float z = fpu_add(lwc1(sp + 0xc8), lwc1(pv + 8));
    swc1(sp + 0xcc, 1.0f);
    swc1(sp + 0xc8, z);

    const uint64_t tex0 = effect_tex0(0x10);
    const uint64_t alpha = alpha_mode(L12_ADDR(0x00161a0c), 0x8000ull << 24);
    sd(sp + 0x80, kClampRepeat);
    sd(sp + 0x78, tex0);
    sd(sp + 0x70, 0);
    sd(sp + 0x88, alpha);

    const float blend = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0)), 1.0f), 0.5f);
    const uint32_t col = rgba_lerp(lwu(L12_ADDR(0x00161a1c)), lwu(L12_ADDR(0x00161a20)), blend);
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(sp + 0x50 + k * 8, 0.5f);
        swc1(sp + 0x54 + k * 8, 0.5f);
        sw(sp + 0x40 + k * 4, col);
        lq_sq(sp + k * 0x10, L12_ADDR(0x001f5b20) + k * 0x10);
    }

    const float step = u2f(0x3dcccccdu);
    float y0 = u2f(0xbecccccdu);
    float y1 = fpu_add(y0, step);
    for (;;) {
        const bool flip = (lbu(m + 0xbc) & 1u) != 0;
        swc1(sp + 0x24, y0);
        swc1(sp + 0x4, y0);
        swc1(sp + 0x34, y1);
        swc1(sp + 0x14, y1);
        if (flip) {
            const float c0 = -cos_vu(fpu_mul(y0, lwc1(L12_ADDR(0x00161a24))));
            const float a1 = fpu_mul(y1, lwc1(L12_ADDR(0x00161a24)));
            swc1(sp + 0x8, fpu_add(c0, 2.0f));
            swc1(sp + 0x18, fpu_add(-cos_vu(a1), 2.0f));
        } else {
            const float c0 = cos_vu(fpu_mul(y0, lwc1(L12_ADDR(0x00161a24))));
            swc1(sp + 0x8, c0);
            swc1(sp + 0x18, cos_vu(fpu_mul(y1, lwc1(L12_ADDR(0x00161a24)))));
        }
        y0 = fpu_add(y0, step);
        quad_draw(sp, mat);
        if (!(y0 < u2f(0x3eb851ecu))) break;
        y1 = fpu_add(y0, step);
    }
}

// Class 1259's draw callback: builds two oriented frames (at partner `pv + 0x2314` and at `m`) and draws the four electric arc
// quad chains (`pv + arc * 0x460`, count `pv + 0x232c`).
// orig: L12:0x003028c8
void hov_cb_3028c8_l12(uint32_t m) {
    const Frame f(880);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    euler_to_mat4_vu(M(sp + 0x120), V(m + 0x40));
    sw(sp + 0x2b4, sp + 0x160);

    for (int32_t side = 0; side < 2; ++side) {
        const uint32_t dst = sp + 0x160 + static_cast<uint32_t>(side * 0x40);
        euler_to_mat4_vu(M(dst), V(L12_ADDR(0x001fb5b0) + static_cast<uint32_t>(side * 0x10)));
        mat4_mul(M(dst), M(sp + 0x120), M(dst));
        if ((side & 1) == 0 && lw(pv + 0x2314) != -1) {
            const uint32_t mate = lwu(L12_ADDR(0x0015ffd8)) + (lwu(pv + 0x2314) << 8);
            lq_sq(sp + 0x2a0, mate + 0x10);
        } else {
            lq_sq(sp + 0x2a0, m + 0x10);
        }
        vec3_add(V(dst + 0x30), V(dst + 0x30), V(sp + 0x2a0));
        swc1(dst + 0x3c, 1.0f);
    }

    sw(sp + 0x2b0, pv + 0x2300);
    const uint64_t tex0 = effect_tex0(0xe);
    const uint64_t alpha = alpha_mode(L12_ADDR(0x00161dcc), 0x8000ull << 24);
    sd(sp + 0x108, tex0);
    sd(sp + 0x118, alpha);
    sd(sp + 0x110, kClampRepeat);
    sd(sp + 0x100, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(sp + 0xe4 + k * 8, lwc1(L12_ADDR(0x001fb514) + k * 8));
    }

    for (int32_t arc = 0; arc < 4; ++arc) {
        const int32_t base = lw(L12_ADDR(0x00161e08));
        const int32_t str = lw(lwu(sp + 0x2b0) + static_cast<uint32_t>(arc << 2));
        float t;
        if (base < str) {
            t = fpu_div(int_to_float(str - base), int_to_float(lw(L12_ADDR(0x00161e04)) - base));
        } else {
            t = fpu_sub(1.0f, fpu_div(int_to_float(str), int_to_float(base)));
        }
        sw(sp + 0x2b8, static_cast<uint32_t>(arc + 1));
        const uint32_t col = rgba_lerp(lwu(L12_ADDR(0x00161dec)), lwu(L12_ADDR(0x00161df0)), t);
        sw(sp + 0xd0, col);
        sw(sp + 0xdc, col);
        sw(sp + 0xd8, col);
        sw(sp + 0xd4, col);

        if (lw(pv + 0x232c) - 1 <= 0) continue;
        const uint32_t arc_off = static_cast<uint32_t>(arc * 0x460);
        float u_acc = 0.0f;
        float u_step = lwc1(L12_ADDR(0x00161df4));
        for (int32_t seg = 0;;) {
            u_acc = fpu_add(u_acc, u_step);
            if (1.0f < u_acc) u_acc = fpu_sub(u_acc, 1.0f);
            const float scroll = lwc1(L12_ADDR(0x00161df8));
            for (uint32_t k = 0; k < 4; ++k) {
                swc1(sp + 0xe0 + k * 8, fpu_add(fpu_add(lwc1(L12_ADDR(0x001fb510) + k * 8), u_acc), scroll));
            }
            for (int32_t c = 0; c < 4; ++c) {
                const uint32_t pt = pv + arc_off + (static_cast<uint32_t>(seg + (c >> 1)) << 4);
                vec3_add(V(sp + 0x90 + static_cast<uint32_t>(c * 0x10)),
                         V(L12_ADDR(0x001fb530) + static_cast<uint32_t>(c * 0x10)), V(pt));
            }
            const uint32_t mats = lwu(sp + 0x2b4);
            quad_draw(sp + 0x90, mats);
            quad_draw(sp + 0x90, mats + 0x40);
            ++seg;
            if (!(seg < lw(pv + 0x232c) - 1)) break;
            u_step = lwc1(L12_ADDR(0x00161df4));
        }
    }
}

// Class 1267's turret HUD draw callback: draws the four corner brackets, the 32-segment health ring (`pv + 0x8c`), and the
// two rotating lock-on reticles (`pv + 0x88`, yaw `m + 0x48`, pitch `pv + 0x74`).
// orig: L12:0x00304218
void hov_cb_304218_l12(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    sw(sp + 0x40, m);
    sw(sp + 0x44, pv);
    sub_002a0ba8(0x42, (0x8000ull << 24) | 0x44ull);

    const uint32_t bracket_col = lwu(L12_ADDR(0x00161eb0));
    sw(sp + 0x0, lwu(L12_ADDR(0x00161f08)));
    sw(sp + 0x4, lwu(L12_ADDR(0x00161f0c)));
    sw(sp + 0x8, lwu(L12_ADDR(0x00161f10)));
    sw(sp + 0xc, lwu(L12_ADDR(0x00161f14)));
    sw(sp + 0x30, bracket_col);
    sw(sp + 0x3c, bracket_col);
    sw(sp + 0x38, bracket_col);
    sw(sp + 0x34, bracket_col);
    sd(sp + 0x48, effect_tex0(lw(L12_ADDR(0x00161e5c)) + 0x28));
    sw(sp + 0x54, sp + 0x10);
    sw(sp + 0x58, sp + 0x30);

    for (int32_t i = 0; i < 4; ++i) {
        const float sx_sign = (i & 1) == 0 ? 1.0f : -1.0f;
        const float sy_sign = i < 2 ? 1.0f : -1.0f;
        const float dy0 = fpu_mul(lwc1(L12_ADDR(0x00161e20)), sy_sign);
        const int32_t y0 = float_to_int(fpu_mul(fpu_add(lwc1(L12_ADDR(0x00161e18)), dy0), 16.0f));
        const int32_t x0 =
            float_to_int(fpu_add(lwc1(L12_ADDR(0x00161e14)), fpu_mul(lwc1(L12_ADDR(0x00161e1c)), sx_sign)));
        sd(sp + 0x10, gs_xyz_16(x0, y0));

        const int32_t y1 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e18)), 16.0f));
        const int32_t x1 =
            float_to_int(fpu_add(lwc1(L12_ADDR(0x00161e14)), fpu_mul(lwc1(L12_ADDR(0x00161e1c)), sx_sign)));
        sd(sp + 0x18, gs_xyz_16(x1, y1));

        const float dy2 = fpu_mul(lwc1(L12_ADDR(0x00161e20)), sy_sign);
        const int32_t y2 = float_to_int(fpu_mul(fpu_add(lwc1(L12_ADDR(0x00161e18)), dy2), 16.0f));
        const int32_t x2 = float_to_int(lwc1(L12_ADDR(0x00161e14)));
        sd(sp + 0x20, gs_xyz_16(x2, y2));

        const int32_t y3 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e18)), 16.0f));
        const int32_t x3 = float_to_int(lwc1(L12_ADDR(0x00161e14)));
        sd(sp + 0x28, gs_xyz_16(x3, y3));

        gs_quad_packet(lwu(sp + 0x54), sp, lwu(sp + 0x58), ld(sp + 0x48), 1);
    }

    sd(sp + 0x48, effect_tex0(6));
    sw(sp + 0x0, lwu(L12_ADDR(0x00161e2c)));
    sw(sp + 0x4, lwu(L12_ADDR(0x00161e30)));
    sw(sp + 0x8, lwu(L12_ADDR(0x00161e34)));
    sw(sp + 0xc, lwu(L12_ADDR(0x00161e38)));

    const float hp = lwc1(lwu(sp + 0x44) + 0x8c);
    const uint32_t full_col = 100.0f < hp ? rgba_lerp(0x6000ffffu, 0x6000ff00u, fpu_div(fpu_sub(hp, 100.0f), 100.0f))
                                          : rgba_lerp(0x600000ffu, 0x6000ffffu, fpu_div(hp, 100.0f));
    const int32_t dim_cnt = float_to_int(fpu_mul(fpu_sub(1.0f, fpu_div(lwc1(lwu(sp + 0x44) + 0x8c), 200.0f)), 32.0f));
    sw(sp + 0x50, static_cast<uint32_t>(dim_cnt));
    const uint32_t dim_col = (full_col & 0x00ffffffu) + 0x20000000u;

    const float kRingStep = u2f(0x3e490fdbu);
    for (int32_t seg = 0; seg < 32; ++seg) {
        const uint32_t c = seg < lw(sp + 0x50) ? dim_col : full_col;
        sw(sp + 0x3c, c);
        sw(sp + 0x38, c);
        sw(sp + 0x34, c);
        sw(sp + 0x30, c);

        const float a0 = angle_wrap(fpu_mul(static_cast<float>(seg), kRingStep));
        const float a1 = angle_add(a0, kRingStep);
        const int32_t rx0 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e24)), sin_vu(a0)));
        const int32_t ry0 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e28)), -cos_vu(a0)));
        const int32_t rx1 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e24)), sin_vu(a1)));
        const int32_t ry1 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e28)), -cos_vu(a1)));

        const int32_t y0 = float_to_int(fpu_mul(fpu_add(lwc1(L12_ADDR(0x00161e18)), static_cast<float>(ry1)), 16.0f));
        const int32_t x0 = float_to_int(fpu_add(lwc1(L12_ADDR(0x00161e14)), static_cast<float>(rx1)));
        sd(sp + 0x10, gs_xyz_16(x0, y0));

        const int32_t y1 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e18)), 16.0f));
        const int32_t x1 = float_to_int(lwc1(L12_ADDR(0x00161e14)));
        sd(sp + 0x18, gs_xyz_16(x1, y1));

        const int32_t y2 = float_to_int(fpu_mul(fpu_add(lwc1(L12_ADDR(0x00161e18)), static_cast<float>(ry0)), 16.0f));
        const int32_t x2 = float_to_int(fpu_add(lwc1(L12_ADDR(0x00161e14)), static_cast<float>(rx0)));
        sd(sp + 0x20, gs_xyz_16(x2, y2));

        const int32_t y3 = float_to_int(fpu_mul(lwc1(L12_ADDR(0x00161e18)), 16.0f));
        const int32_t x3 = float_to_int(lwc1(L12_ADDR(0x00161e14)));
        sd(sp + 0x28, gs_xyz_16(x3, y3));

        gs_quad_packet(lwu(sp + 0x54), sp, lwu(sp + 0x58), ld(sp + 0x48), 1);
    }

    uint32_t lock_col;
    if (frame_scale_i(0xf) < lw(lwu(sp + 0x44) + 0x88)) {
        const float num = int_to_float(lw(lwu(sp + 0x44) + 0x88) - frame_scale_i(0xf));
        const float den = static_cast<float>(frame_scale_i(0xf));
        lock_col = rgba_lerp(0x5000ffffu, 0x500000ffu, fpu_div(num, den));
    } else {
        const float num = int_to_float(lw(lwu(sp + 0x44) + 0x88));
        const float den = static_cast<float>(frame_scale_i(0xf));
        lock_col = rgba_lerp(0x5000ff00u, 0x5000ffffu, fpu_div(num, den));
    }

    sw(sp + 0x0, lwu(L12_ADDR(0x00161e4c)));
    sw(sp + 0x4, lwu(L12_ADDR(0x00161e50)));
    sw(sp + 0x8, lwu(L12_ADDR(0x00161e54)));
    sw(sp + 0xc, lwu(L12_ADDR(0x00161e58)));
    sw(sp + 0x30, lock_col);
    sw(sp + 0x3c, lock_col);
    sw(sp + 0x38, lock_col);
    sw(sp + 0x34, lock_col);

    const float kOffsets[4] = {0.0f, u2f(0x3fc90fdbu), u2f(0xbfc90fdbu), u2f(0x40490fdbu)};
    for (uint32_t c = 0; c < 4; ++c) {
        const float ang = angle_wrap(angle_add(fpu_mul(lwc1(lwu(sp + 0x40) + 0x48), 4.0f), kOffsets[c]));
        const int32_t y_16 = float_to_int(fpu_mul(fpu_mul(sin_vu(ang), lwc1(L12_ADDR(0x00161e48))), 16.0f));
        const int32_t x_16 = float_to_int(fpu_mul(fpu_mul(cos_vu(ang), lwc1(L12_ADDR(0x00161e44))), 16.0f));
        sd(sp + 0x10 + c * 8, gs_xyz_subpx(x_16, y_16));
    }
    gs_quad_packet(lwu(sp + 0x54), sp, lwu(sp + 0x58), effect_tex0(lw(L12_ADDR(0x00161e3c))), 1);

    for (uint32_t c = 0; c < 4; ++c) {
        const float ang = angle_wrap(angle_add(fpu_mul(lwc1(lwu(sp + 0x44) + 0x74), 8.0f), kOffsets[c]));
        const int32_t y_16 = float_to_int(fpu_mul(fpu_mul(sin_vu(ang), lwc1(L12_ADDR(0x00161e48))), 16.0f));
        const int32_t x_16 = float_to_int(fpu_mul(fpu_mul(cos_vu(ang), lwc1(L12_ADDR(0x00161e44))), 16.0f));
        sd(sp + 0x10 + c * 8, gs_xyz_subpx(x_16, y_16));
    }
    gs_quad_packet(lwu(sp + 0x54), sp, lwu(sp + 0x58), effect_tex0(lw(L12_ADDR(0x00161e40))), 1);
}

// Class 1267's exit fade callback: draws an overlay quad fading by timer `pv + 0xac` against `frame_scale_i(0x50)`.
// orig: L12:0x00304d98
void hov_cb_304d98_l12(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t max_t = frame_scale_i(0x50);
    const int32_t cur_t = lh(pv + 0xac);
    if (cur_t < max_t) {
        sub_001fa050(0xaa, 0, 0, ee_div((max_t - cur_t) * 0x50, max_t));
    }
}

// Class 1345's draw callback: draws a 5-row by 6-column jittered grid of glowing barrier quads in `m`'s affine frame.
// orig: L12:0x00308350
void hov_cb_308350_l12(uint32_t m) {
    const Frame f(512);
    const uint32_t sp = f.sp, mat = sp + 0x120;
    mat3_copy_affine(M(mat), M(m + 0xc0));
    lq_sq(sp + 0x150, m + 0x10);
    swc1(sp + 0x15c, 1.0f);

    const uint64_t tex0 = effect_tex0(0xe);
    const uint64_t alpha_a = alpha_mode(L12_ADDR(0x00161fbc), 0x8000ull << 24);
    const uint64_t alpha_b = alpha_mode(L12_ADDR(0x00161fcc), 0x8000ull << 24);
    const uint32_t col_a = lwu(L12_ADDR(0x00161f90));
    const uint32_t col_b = lwu(L12_ADDR(0x00161f94));

    sd(sp + 0x80, kClampRepeat);
    sd(sp + 0x88, alpha_a);
    sd(sp + 0x118, alpha_b);
    sd(sp + 0x110, kClampRepeat);
    sw(sp + 0x40, col_a);
    sw(sp + 0x4c, col_a);
    sw(sp + 0x48, col_a);
    sw(sp + 0x44, col_a);
    sd(sp + 0x78, tex0);
    sw(sp + 0xd0, col_b);
    sd(sp + 0x108, tex0);
    sd(sp + 0x100, 0);
    sd(sp + 0x70, 0);
    sw(sp + 0xdc, col_b);
    sw(sp + 0xd8, col_b);
    sw(sp + 0xd4, col_b);

    for (uint32_t k = 0; k < 4; ++k) {
        lq_sq(sp + 0x90 + k * 0x10, L12_ADDR(0x001fb6f0) + k * 0x10);
        lq_sq(sp + k * 0x10, L12_ADDR(0x001fb730) + k * 0x10);
        const float u0 = lwc1(L12_ADDR(0x001fb6d0) + k * 8);
        const float v0 = lwc1(L12_ADDR(0x001fb6d0) + k * 8 + 4);
        swc1(sp + 0x50 + k * 8, u0);
        swc1(sp + 0x54 + k * 8, v0);
        swc1(sp + 0xe0 + k * 8, u0);
        swc1(sp + 0xe4 + k * 8, v0);
    }

    for (int32_t row = 0; row < 5; ++row) {
        const float du = rand_range_f(-0.5f, 0.5f);
        for (uint32_t k = 0; k < 4; ++k) {
            swc1(sp + k * 0x10 + 8, fpu_add(lwc1(sp + k * 0x10 + 8), 1.0f));
            swc1(sp + 0x90 + k * 0x10 + 8, fpu_add(lwc1(sp + 0x90 + k * 0x10 + 8), 1.0f));
            swc1(sp + 0x50 + k * 8, fpu_add(lwc1(L12_ADDR(0x001fb6d0) + k * 8), du));
            swc1(sp + 0x54 + k * 8, lwc1(L12_ADDR(0x001fb6d0) + k * 8 + 4));
        }
        float jit = 0.0f;
        for (int32_t col = -3; col < 3;) {
            const int32_t next_col = col + 1;
            const bool rand_next = col < 4;
            const float y0 = fpu_add(static_cast<float>(col), jit);
            col = next_col;
            jit = 0.0f;
            swc1(sp + 0x14, y0);
            swc1(sp + 0x4, y0);
            if (rand_next) jit = rand_range_f(u2f(0xbe4ccccdu), u2f(0x3e4ccccdu));
            const float y1 = fpu_add(static_cast<float>(next_col), jit);
            swc1(sp + 0x34, y1);
            swc1(sp + 0x24, y1);
            quad_draw(sp, mat);
        }
    }
}

// Class 1901's draw callback: sets up VU1 program 7 (`shadows_vu1_setup`), draws 10 UV-offset prim lists when the camera is
// inside box `pv + 0`, and draws UV-scroll mesh sets `(7, 0)` and `(7, 1)`.
// orig: L12:0x0030be68
void hov_cb_30be68_l12(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    shadows_vu1_setup();
    sub_002a0ba8(0x42, (0xfe00ull << 23) | 0x64ull);
    sub_002a0ba8(0x8, 0);
    sub_002a0ba8(0x14, kClampRepeat);
    sub_002a0ba8(0x47, 0x5360au);

    if (point_in_box(L12_ADDR(0x00167240), lw(pv + 0)) != 0) {
        sub_002a0ba8(0x6, effect_tex0(0x2c));
        for (uint32_t k = 0; k < 10; ++k) {
            const int32_t cnt = lw(L12_ADDR(0x001fbd10) + k * 4);
            const uint32_t uvs = lwu(L12_ADDR(0x00205a08) + k * 4);
            uv_offset_pairs(L12_ADDR(0x00208b00), uvs, cnt, L12_ADDR(0x00162108));
            prim_list_packet(cnt, lwu(L12_ADDR(0x002059e0) + k * 4), lwu(L12_ADDR(0x00205a58) + k * 4),
                             L12_ADDR(0x00208b00), 1);
        }
    }

    sub_002a0ba8(0x6, effect_tex0(0x2f));
    uv_scroll_meshes(7, 0);
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L12_ADDR(0x001620d4))) << 32) | 0x68ull);
    sub_002a0ba8(0x6, effect_tex0(0x30));
    uv_scroll_meshes(7, 1);
}

}  // namespace rac1
