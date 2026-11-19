// Pokitaru's (level 11) fighter-jet HUD draw-list callback (L11:0x311d50) and its radar/lock-on helpers
// (0x311710, 0x311a88, 0x311be8, 0x318050, 0x311948, 0x311c80), ported by hand.
// Carries the 10 HUD-width hooks (render_view_hud_begin / render_view_hud_end) for the radar and missiles.
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/variants_l11.h>
#include <game/render/render_view.h>

namespace rac1 {

// Draws a single radar blip for `target` relative to camera rotation `cam_rot` and position `cam_pos`.
// orig: L11:0x00311710
void pok_311710_l11(uint32_t cam_rot, uint32_t cam_pos, uint32_t target, uint32_t icon_idx, uint32_t color) {
    const uint32_t sp = ee_stack_alloc(0x80);
    sw(sp + 0x10, 0);
    sw(sp + 0x14, 0);
    sw(sp + 0x18, 0);
    sw(sp + 0x1c, 0);
    const float pitch = lwc1(cam_rot + 0x4);
    float yaw = lwc1(cam_rot + 0x8);
    if (u2f(0x3fc90fdbu) < pitch || pitch < u2f(0xbfc90fdbu)) {
        yaw = -yaw;
    }
    swc1(sp + 0x18, angle_sub(u2f(0x3fc90fdbu), yaw));
    euler_to_mat4_vu(M(sp + 0x20), V(sp + 0x10));
    vec3_sub(V(sp + 0x60), V(target + 0x10), V(cam_pos));
    sw(sp + 0x68, 0);
    mat4_transform(V(sp + 0x70), V(sp + 0x60), M(sp + 0x20));
    vec3_scale(V(sp + 0x70), V(sp + 0x70), u2f(0x3e124925u));
    const float dist = vec3_length(V(sp + 0x70));
    const float sign_x = (lbu(0x0015edb4u) != 0) ? -1.0f : 1.0f;
    const int32_t rx = float_to_int(fpu_mul(sign_x, lwc1(sp + 0x70)));
    const int32_t ry = float_to_int(-lwc1(sp + 0x74));
    if (dist < 46.0f) {
        if (38.0f < dist) {
            const int32_t a = static_cast<int32_t>((color >> 24) & 0xffu);
            const float fade = fpu_mul(fpu_mul(int_to_float(a), fpu_sub(46.0f, dist)), 0.125f);
            color = (static_cast<uint32_t>(float_to_int(fade)) << 24) | (color & 0x00ffffffu);
        }
        const uint32_t rec = L11_ADDR(0x001f1058u) + (icon_idx << 4);
        const uint64_t tex = effect_tex0(lh(rec + 0x0));
        const int32_t w = lh(rec + 0x6);
        const int32_t h = lh(rec + 0x8);
        const int32_t x = (rx + 0x1b0) - lh(rec + 0xc);
        const int32_t y = (ry - 0x50 + lw(0x0013e504u)) - lh(rec + 0xe);
        glyph_sprite(x, y, w, h, lh(rec + 0x2), lh(rec + 0x4), w, h, static_cast<uint64_t>(color), tex);
    }
    ee_stack_free(0x80);
}

// Returns squared screen-space distance from the crosshair to `target` if inside its lock-on radius, else 10000.0f.
// orig: L11:0x00311a88
float pok_311a88_l11(uint32_t jet, uint32_t target) {
    const uint32_t pv = lwu(jet + 0x78);
    if (lbu(target + 0x31) == 0) return 10000.0f;
    const uint32_t sp = ee_stack_alloc(0x20);
    hud_screen_point(target + 0x10, sp + 0x10, sp + 0x14, 0);
    int32_t dx = lw(sp + 0x10) - lw(pv + 0xe0);
    int32_t dy = lw(sp + 0x14) - lw(pv + 0xe4);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    lq_sq(sp + 0x0, L11_ADDR(0x001679e0u));
    vec3_scale(V(sp + 0x0), V(L11_ADDR(0x001679e0u)), fpu_div(lwc1(target + 0xc), 1280.0f));
    vec3_add(V(sp + 0x0), V(sp + 0x0), V(target + 0x10));
    hud_screen_point(sp + 0x0, sp + 0x18, sp + 0x1c, 0);
    const int32_t rx = lw(sp + 0x18) - lw(sp + 0x10);
    const int32_t ry = lw(sp + 0x1c) - lw(sp + 0x14);
    const int32_t lim = float_to_int(sqrt_f(int_to_float(rx * rx + ry * ry))) + 0x1c;
    ee_stack_free(0x20);
    if (dx < lim && dy < lim) {
        return static_cast<float>(dx * dx + dy * dy);
    }
    return 10000.0f;
}

// Counts active and visible bombers in the jet's target group into `*out_alive` and `*out_visible`.
// orig: L11:0x00311be8
void pok_311be8_l11(uint32_t jet, uint32_t out_alive, uint32_t out_visible) {
    const uint32_t pv = lwu(jet + 0x78);
    sh(out_alive, 0);
    sh(out_visible, 0);
    uint32_t e = lvd::group(lh(pv + 0x108));
    if (e == 0) return;
    for (;; e += 2) {
        const uint16_t raw = lhu(e);
        const uint32_t bm = lwu(L11_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(bm + 0x20)) >= 0) {
            sh(out_alive, lhu(out_alive) + 1);
            if (lbu(bm + 0x31) != 0) sh(out_visible, lhu(out_visible) + 1);
        }
        if (lh(e) < 0) break;
    }
}

// Writes `bomber` and its active turrets (pvar +0x70, count at +0x64) into `out_list`, returning the count.
// orig: L11:0x00318050
int32_t pok_318050_l11(uint32_t bomber, uint32_t out_list) {
    const uint32_t pv = lwu(bomber + 0x78);
    const int32_t n = lw(pv + 0x64);
    sw(out_list, bomber);
    for (int32_t i = 0; i < n; ++i) {
        sw(out_list + 4 + (static_cast<uint32_t>(i) << 2), lwu(pv + 0x70 + (static_cast<uint32_t>(i) << 2)));
    }
    return n + 1;
}

// Selects the closest lock-on target (preferring `cur_target` if still within 1000.0f^2 of the crosshair).
// orig: L11:0x00311948
uint32_t pok_311948_l11(uint32_t jet, uint32_t pv, uint32_t cur_target) {
    pok_311be8_l11(jet, pv + 0xe8, pv + 0xea);
    float best_d = 1000.0f;
    if (cur_target != 0 && pok_311a88_l11(jet, cur_target) < best_d) return cur_target;
    uint32_t e = lvd::group(lh(pv + 0x108));
    if (e == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0x30);
    uint32_t best = 0;
    for (;; e += 2) {
        const uint16_t raw = lhu(e);
        const uint32_t bm = lwu(L11_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(bm + 0x20)) >= 0) {
            const int32_t cnt = pok_318050_l11(bm, sp);
            for (int32_t i = 1; i < cnt; ++i) {
                const uint32_t sub = lwu(sp + (static_cast<uint32_t>(i) << 2));
                const float d = pok_311a88_l11(jet, sub);
                if (d < best_d) {
                    best = sub;
                    best_d = d;
                }
            }
        }
        if (lh(e) < 0) break;
    }
    ee_stack_free(0x30);
    return best;
}

// Updates the jet's missile lock-on target (pvar +0x88) and lock timer (pvar +0x8c).
// orig: L11:0x00311c80
void pok_311c80_l11(uint32_t jet, uint32_t pv) {
    if (lbu(0x00140946u) == 0) {
        sw(pv + 0x88, 0);
        return;
    }
    const uint32_t prev = lwu(pv + 0x88);
    if (prev != 0) {
        const uint32_t st = lbu(prev + 0x20);
        if (st == 0xfe || st == 0xfd) sw(pv + 0x88, 0);
    }
    const uint32_t next = pok_311948_l11(jet, pv, lwu(pv + 0x88));
    if (next != lwu(pv + 0x88)) {
        sw(pv + 0x88, next);
        sw(pv + 0x8c, lw(L11_ADDR(0x00162160u)) + lw(L11_ADDR(0x00162164u)));
    } else if (next != 0) {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x8c)));
    }
}

// Fighter-jet HUD draw-list callback: draws radar, enemy blips, missile rack, crosshair, lock-on ring,
// low-health/missile/controls help prompts, and updates the health-bar palette strip.
// orig: L11:0x00311d50
void pok_cb_311d50_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x44ULL);
    const uint64_t radar_col = 0x70808080ULL;
    const uint64_t tex_3c = effect_tex0(0x3c);
    render_view_hud_begin(2);  // enhancement: the HUD's width (the radar)
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_3c);
    render_view_hud_end();
    const uint64_t tex_3d = effect_tex0(0x3d);
    render_view_hud_begin(2);
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_3d);
    render_view_hud_end();

    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x48ULL);
    (void)effect_tex0(0x3e);

    const uint32_t sp = ee_stack_alloc(0x50);
    if (uint32_t e = lvd::group(lh(pv + 0x108)); e != 0) {
        for (;; e += 2) {
            const uint16_t raw = lhu(e);
            const uint32_t bm = lwu(L11_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
            if (static_cast<int8_t>(lbu(bm + 0x20)) >= 0) {
                const int32_t cnt = pok_318050_l11(bm, sp + 0x10);
                for (int32_t i = 0; i < cnt; ++i) {
                    const uint32_t sub = lwu(sp + 0x10 + (static_cast<uint32_t>(i) << 2));
                    render_view_hud_begin(2);  // (a blip)
                    pok_311710_l11(L11_ADDR(0x001677d0u), L11_ADDR(0x001677c0u), sub, 0, 0x75808080u);
                    render_view_hud_end();
                }
            }
            if (lh(e) < 0) break;
        }
    }

    if (lbu(0x00140947u) != 0) {
        int32_t x = 0x18;
        int32_t y = 0x40;
        int32_t i = 0;
        for (;;) {
            const bool loaded = i < static_cast<int32_t>(lbu(0x00140946u));
            render_view_hud_begin(1);  // (a missile)
            hud_line_strip(L11_ADDR(0x001f0cf8u), 0x19, 0x00fffff3u, loaded ? 0x50008f00ULL : 0x20004f00ULL,
                           static_cast<float>(x), static_cast<float>(y), 0.5f);
            render_view_hud_end();
            const int32_t cap = static_cast<int32_t>(lbu(0x00140947u));
            if (i == (cap >> 1)) {
                y = 0x2e;
                x += 0x1e;
            }
            ++i;
            if (i >= cap) break;
            y += 0x12;
        }
    }

    const float cx = static_cast<float>(lw(pv + 0xe0));
    const float cy = static_cast<float>(lw(pv + 0xe4));
    screen_sprite(0x3f, 0x3f, effect_tex0(0x11), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x3f, 0x3f, effect_tex0(0x12), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x1f, 0x1f, effect_tex0(0x08), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 10.0f, 10.0f, 0.0f, 0.5f, 0.5f);

    pok_311c80_l11(m, pv);
    if (const uint32_t tgt = lwu(pv + 0x88); tgt != 0) {
        hud_screen_point(tgt + 0x10, sp + 0x40, sp + 0x44, 0);
        const int32_t lock_t = lw(pv + 0x8c);
        const int32_t lock_base = lw(L11_ADDR(0x00162160u));
        if (lock_base < lock_t) {
            const float frac = fpu_div(int_to_float(lock_t - lock_base), int_to_float(lw(L11_ADDR(0x00162164u))));
            const float scale = fpu_add(fpu_mul(frac, 5.0f), 1.0f);
            float a = fpu_sub(1.0f, frac);
            a = fpu_add(a, a);
            if (1.0f < a) a = 1.0f;
            const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(a, 96.0f))) & 0xffu;
            const float ang = angle_wrap_mod(fpu_div(int_to_float(lw(L11_ADDR(0x0015f5ccu))), 30.0f));
            hud_ring(0, 0xff, 0, alpha, int_to_float(lw(sp + 0x40)), int_to_float(lw(sp + 0x44)), scale, ang);
            sw(pv + 0xec, 0);
        } else {
            const int32_t period = frame_scale_i(0x14);
            const uint32_t g = ((ee_div(lw(pv + 0x8c), period) & 1) != 0) ? 0u : 0xffu;
            hud_ring(0xff, g, 0, 0x60, int_to_float(lw(sp + 0x40)), int_to_float(lw(sp + 0x44)), 1.0f, 0.0f);
            sw(pv + 0xec, lwu(pv + 0x88));
        }
    }

    // Low-health help prompt (0x2aff, slot 0x58)
    const uint32_t help_base = 0x00141968u;
    if (lwc1(0x0014094cu) < fpu_mul(lwc1(0x00140950u), 0.25f)) {
        const uint32_t shown = lhu(help_base + 0x2c0);
        if (shown == 0) {
            sub_00203908(TXT(0x2aff), 0x58);
        } else if (shown < 2) {
            const int32_t t = lw(pv + 0x11c) + 1;
            sw(pv + 0x11c, t);
            if (frame_scale_i(0x708) < t) sub_00203908(TXT(0x2aff), 0x58);
        }
    } else if (lw(pv + 0x11c) != 0) {
        const uint32_t shown = lhu(help_base + 0x2c0);
        if (shown <= 0xfffeu) sh(help_base + 0x2c0, shown + 1);
        const int32_t mins = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
        if (static_cast<int32_t>(lhu(help_base + 0x2c2)) < mins) {
            sh(help_base + 0x2c2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258)));
        }
        sw(help_base + 0x2c4, lwu(help_base + 0x2c4) | (1u << (lwu(0x0015ed84u) & 31u)) | 0x80000000u);
        sw(pv + 0x11c, 0);
    }

    // Low-missiles help prompt (0x2afe, slot 0x57)
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x120)));
    const uint32_t missiles = lbu(0x00140946u);
    if ((missiles < 5 && lhu(help_base + 0x2b8) == 0) ||
        (missiles == 0 && lhu(help_base + 0x2b8) < 2 && lw(pv + 0x120) == 0)) {
        sub_00203908(TXT(0x2afe), 0x57);
        sw(pv + 0x120, static_cast<uint32_t>(frame_scale_i(0x4b0)));
    }

    // Flight-controls help prompt (0x2afd, slot 0x56)
    if (lhu(help_base + 0x2b0) == 0) {
        const int32_t prev = lw(pv + 0x128);
        sw(pv + 0x128, prev + 1);
        if (frame_scale_i(0xe10) < prev) sub_00203908(TXT(0x2afd), 0x56);
    }

    if (lbu(m + 0x20) == 8) {
        text_centered(0x100, 0xc8, 0x80005080u, sub_00203ad8(TXT(0x523e)), 0x11);
    }

    // Smoothly approach ship health and update the 256-entry CLUT strip at *(L11:0x16216c).
    float delta = fpu_mul(fpu_sub(lwc1(0x0014094cu), lwc1(pv + 0xf0)), u2f(0x3e4ccccdu));
    const float abs_d = abs_f(delta);
    if (1.0f < abs_d) delta = fpu_div(delta, abs_d);
    const float cur = fpu_add(lwc1(pv + 0xf0), delta);
    swc1(pv + 0xf0, cur);
    int32_t bar = float_to_int(fpu_div(fpu_mul(cur, 251.0f), lwc1(0x00140950u))) + 2;
    if (bar >= 254) bar = 0xfd;
    if (bar <= 1) bar = 2;
    const int32_t prev_bar = lw(pv + 0xf4);
    const int32_t lo_i = (bar < prev_bar) ? bar : prev_bar;
    const int32_t hi_i = (prev_bar < bar) ? bar : prev_bar;
    for (int32_t k = lo_i; k <= hi_i; ++k) {
        const uint32_t idx = (static_cast<uint32_t>(k) & 0xe7u) | ((static_cast<uint32_t>(k) & 0x10u) >> 1) |
                             ((static_cast<uint32_t>(k) & 0x08u) << 1);
        const uint32_t dst = lwu(L11_ADDR(0x0016216cu)) + (idx << 2);
        sw(dst, (k < bar) ? lwu(L11_ADDR(0x001f1088u) + (idx << 2)) : 0x80000000u);
    }
    sw(pv + 0xf4, static_cast<uint32_t>(bar));
    ee_stack_free(0x50);
}

}  // namespace rac1
