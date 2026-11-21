// Aridia Skid McMarx Sandshark counter HUD element callbacks (slots 5 / 7, keys 0x7d0 / 0x7d1).
#include <game/ovl/aridia_hud.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

namespace {
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// orig: L2:0x0023c990
void ari_skid_hud_start_l2(uint32_t e) {
    sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0x78) + 0x1e));
    sw(e + 0x5c, 0x20u);
    sw(e + 0x58, 0x20u);
    hud_element_size(e);
}

// orig: L2:0x0023ccc8
void ari_skid_hud_update_l2(uint32_t e) {
    const int32_t min_life = frame_scale_i(5);
    if (lw(e + 0x7c) < min_life) {
        if (lbu(e + 0x71) != 0 || lbu(e + 0x70) != 0) {
            sw(e + 0x6c, 1u);
        }
        const uint8_t b1 = lbu(e + 0x71);
        if (b1 != 0) {
            sb(e + 0x71, b1 - 1u);
        } else {
            const uint8_t b0 = lbu(e + 0x70);
            if (b0 != 0) {
                sb(e + 0x70, b0 - 1u);
            }
        }
    } else if (static_cast<int32_t>(lbu(e + 0x70)) < frame_scale_i(8)) {
        sb(e + 0x70, lbu(e + 0x70) + 1u);
    } else if (static_cast<int32_t>(lbu(e + 0x71)) < frame_scale_i(8)) {
        sb(e + 0x71, lbu(e + 0x71) + 1u);
    }
    sw(e + 0x74, lwu(lwu(e + 0xc)));
}

// orig: L2:0x0023cda8
void ari_skid_hud_draw_l2(uint32_t e) {
    if (lbu(e + 0x70) == 0) {
        return;
    }

    const uint32_t sp = ee_stack_alloc(0x20);
    sw(sp + 0x14, lwu(e + 0x54));
    sw(sp + 0x10, lwu(e + 0x50));
    hud_element_place(e, sp + 0x10, sp + 0x14);

    const bool left_side = lw(sp + 0x10) < 257;
    sw(sp + 0x10, lwu(left_side ? L2_ADDR(0x0015f944u) : L2_ADDR(0x0015f948u)));

    const int32_t scr_h = lw(0x0013e504u);
    sw(sp + 0x14, static_cast<uint32_t>(lw(0x0015ed80u) != 0 ? scr_h - 0x2a : scr_h - 0x32));

    float bar_t = fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(L2_ADDR(0x0015f91cu))));
    if (1.0f < bar_t) {
        bar_t = 1.0f;
    } else if (bar_t < 0.0f) {
        bar_t = 0.0f;
    }

    float text_t = fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(L2_ADDR(0x0015f920u))));
    if (1.0f < text_t) {
        text_t = 1.0f;
    } else if (text_t < 0.0f) {
        text_t = 0.0f;
    }

    libc_sprintf(sp, L2_ADDR(0x0015f850u), {sx64(lw(e + 0x74))});

    const int32_t raw_alpha = float_to_int(fpu_mul(bar_t, 128.0f));
    const int32_t bar_alpha = float_to_int(fpu_mul(int_to_float(raw_alpha), u2f(0x3f333333u)));
    const int32_t max_w = lw(e + 0x8) < 10 ? lw(L2_ADDR(0x0015f958u)) : lw(L2_ADDR(0x0015f95cu));
    const int32_t x = lw(sp + 0x10);
    const int32_t x_inner = x + 4;
    const int32_t bar_w = float_to_int(fpu_mul(int_to_float(max_w), bar_t));

    if (left_side) {
        int32_t f = hud_frame(0x7580u, 1);
        hud_sprite(f, x - 0x1c, lw(sp + 0x14), 0x20, 0x20, sx64(bar_alpha));
        f = hud_frame(0x7580u, 0);
        hud_sprite(f, x_inner, lw(sp + 0x14), bar_w, 0x20, sx64(bar_alpha));
        f = hud_frame(0x7580u, 1);
        hud_sprite_flip(f, x_inner + bar_w, lw(sp + 0x14), 0x20, 0x20, sx64(bar_alpha));
    } else {
        int32_t f = hud_frame(0x7580u, 1);
        const int32_t x_left = x_inner - bar_w;
        hud_sprite_flip(f, x_inner, lw(sp + 0x14), 0x20, 0x20, sx64(bar_alpha));
        f = hud_frame(0x7580u, 0);
        hud_sprite(f, x_left, lw(sp + 0x14), bar_w, 0x20, sx64(bar_alpha));
        f = hud_frame(0x7580u, 1);
        hud_sprite(f, x_left - 0x20, lw(sp + 0x14), 0x20, 0x20, sx64(bar_alpha));
    }

    if (left_side) {
        hud_sprite_at(e, lw(e + 0x44), lw(sp + 0x10), lw(sp + 0x14), 0u, 0x80ull);
    } else {
        hud_sprite_at(e, lw(e + 0x44), lw(sp + 0x10) - 0x16, lw(sp + 0x14), 0u, 0x80ull);
    }

    const uint64_t fg_col = static_cast<uint64_t>(rgba_lerp(lwu(L2_ADDR(0x0015f950u)), lwu(L2_ADDR(0x0015f954u)), text_t));
    const uint64_t sh_col = static_cast<uint64_t>(rgba_lerp(0u, 0x80000000u, text_t));
    if (left_side) {
        text_right(lw(sp + 0x10) + 0x43, lw(sp + 0x14) + 9, sh_col, sp, -1);
        text_right(lw(sp + 0x10) + 0x42, lw(sp + 0x14) + 8, fg_col, sp, -1);
    } else {
        text_right(lw(sp + 0x10) - 0x19, lw(sp + 0x14) + 9, sh_col, sp, -1);
        text_right(lw(sp + 0x10) - 0x1a, lw(sp + 0x14) + 8, fg_col, sp, -1);
    }

    ee_stack_free(0x20);
}

}  // namespace rac1
