// Metal Detector HUD slot 7 callbacks (start 0x00235dc0, update 0x0023a218, draw 0x0023a2d0).
#include <game/ovl/hud_slot7_tr.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }

// orig: 0x001fb470
void draw_detector_text(int32_t x, int32_t y, uint64_t color, uint32_t str, int32_t max_len) {
    const uint64_t tex0 = static_cast<uint64_t>(effect_tex0(3));
    text_draw(x, y, color, str, max_len, tex0, OV(0x001c3890u));
}
}  // namespace

// orig: 0x00235dc0
void hud_slot7_start_tr(uint32_t e) {
    const int32_t life = frame_scale_i(0xb4) + 0x1e;
    sh(e + 0x48, 0);
    sh(e + 0x4a, 0);
    sw(e + 0x7c, static_cast<uint32_t>(life));
    sw(e + 0x5c, 0x20u);
    sw(e + 0x58, 0x20u);
    hud_element_size(e);
}

// orig: 0x0023a218
void hud_slot7_update_tr(uint32_t e) {
    if (lw(0x00141398u) != 0) {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0xa)));
        const int32_t max_step = lw(OV(0x0015f7f0u));
        const uint8_t b0 = lbu(e + 0x70);
        if (static_cast<int32_t>(b0) < max_step) {
            sb(e + 0x70, b0 + 1u);
        } else {
            const uint8_t b1 = lbu(e + 0x71);
            if (static_cast<int32_t>(b1) < max_step) {
                sb(e + 0x71, b1 + 1u);
            }
        }
    } else {
        sw(e + 0x7c, 0);
        sw(e + 0x6c, 1u);
        const uint8_t b1 = lbu(e + 0x71);
        if (b1 != 0) {
            sb(e + 0x71, b1 - 1u);
        } else {
            const uint8_t b0 = lbu(e + 0x70);
            if (b0 != 0) {
                sb(e + 0x70, b0 - 1u);
            } else {
                sw(e + 0x6c, static_cast<uint32_t>(-6));
            }
        }
    }
}

// orig: 0x0023a2d0
void hud_slot7_draw_tr(uint32_t e) {
    const int32_t scr_h = lw(0x0013e504u);
    const int32_t y = lw(0x0015ed80u) != 0 ? scr_h - 0x2a : scr_h - 0x32;
    const int32_t x_base = lw(e + 0x50);

    if (lbu(0x001413f4u) == 2u || lw(0x001413d4u) == 0x32 || lbu(e + 0x70) == 0) {
        return;
    }

    float bar_t = fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(OV(0x0015f7f0u))));
    if (1.0f < bar_t) {
        bar_t = 1.0f;
    } else if (bar_t < 0.0f) {
        bar_t = 0.0f;
    }

    float text_t = fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(OV(0x0015f7f0u))));
    if (1.0f < text_t) {
        text_t = 1.0f;
    } else if (text_t < 0.0f) {
        text_t = 0.0f;
    }

    const int32_t icon_alpha = float_to_int(fpu_mul(text_t, 128.0f));
    const int32_t bar_alpha = float_to_int(fpu_mul(int_to_float(lw(OV(0x0015f8f8u))), bar_t));
    const int32_t right_x = x_base + lw(OV(0x0015f900u));
    const int32_t bar_w = float_to_int(fpu_mul(int_to_float(lw(OV(0x0015f8fcu))), bar_t));
    const int32_t mid_x = right_x - bar_w;

    hud_sprite_flip(hud_frame(0x7580u, 1), right_x + 0x10, y, 0x20, 0x20, sx64(bar_alpha));
    hud_sprite(hud_frame(0x7580u, 0), mid_x, y, bar_w + 0x10, 0x20, sx64(bar_alpha));
    hud_sprite(hud_frame(0x7580u, 1), mid_x - 0x20, y, 0x20, 0x20, sx64(bar_alpha));

    const int32_t icon_frame = ee_rem(lw(OV(0x0015f5ccu)), 0x3c) >> 1;
    hud_sprite_at(e, hud_frame(0x754fu, icon_frame), x_base - 0x20, y, 0u, sx64(icon_alpha));

    const uint32_t str_buf = ee_stack_alloc(0x20);
    libc_sprintf(str_buf, OV(0x0015f918u), {});

    const int32_t tx = x_base + lw(OV(0x0015f904u));
    const int32_t ty = y + lw(OV(0x0015f908u));
    const int32_t period = frame_scale_i(lw(OV(0x0015f914u)));
    const int32_t rem = ee_rem(lw(OV(0x0015f5ccu)), period);
    float phase = fpu_div(int_to_float(rem), int_to_float(period));
    phase = fpu_sub(fpu_mul(phase, u2f(0x40c90fd0u)), u2f(0x40490fd0u));
    float pulse = fpu_sub(fpu_mul(sin_vu(phase), 4.0f), 3.0f);
    if (pulse < 0.0f) {
        pulse = 0.0f;
    }
    text_t = fpu_mul(pulse, text_t);

    const uint64_t sh_col = static_cast<uint64_t>(rgba_lerp(0u, 0x80000000u, text_t));
    const uint64_t fg_col = static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f90cu)), lwu(OV(0x0015f910u)), text_t));
    draw_detector_text(tx + 1, ty + 1, sh_col, str_buf, -1);
    draw_detector_text(tx, ty, fg_col, str_buf, -1);

    ee_stack_free(0x20);
}

}  // namespace rac1
