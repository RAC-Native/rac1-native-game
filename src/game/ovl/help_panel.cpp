// Help message panel HUD callbacks (init 0x00237190, update 0x002371e0, draw 0x00237200).
#include <game/ovl/help_panel_tr.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// orig: 0x00237190
void help_panel_init_tr(uint32_t e) {
    const int32_t life = frame_scale_i(0xa) + 0x1e;
    sh(e + 0x48, 0);
    sh(e + 0x4a, 0);
    sw(e + 0x7c, static_cast<uint32_t>(life));
    sw(e + 0x5c, 0x20u);
    sw(e + 0x58, 0x20u);
    hud_element_size(e);
}

// orig: 0x002371e0
void help_panel_update_tr(uint32_t e) {
    hud_plain_update(e);
}

// orig: 0x00237200
void help_panel_draw_tr(uint32_t e) {
    const int32_t y_base = lw(OV(0x0015f770u)) + (lw(0x0015ed80u) != 0 ? 0xa : 0x12);

    float t = fpu_div(int_to_float(lbu(e + 0x70)), frame_scale_f(8.0f));
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (1.0f < t) {
        t = 1.0f;
    }

    const int32_t text_y = y_base + 8;
    const int32_t alpha = float_to_int(fpu_mul(t, 128.0f));
    const uint64_t text_col = sx64(static_cast<int32_t>((static_cast<uint32_t>(alpha) << 24) | 0x40f040u));

    const uint32_t msg = OV(0x0017e530u);
    int32_t split = 0;
    if (!(lbu(msg) < 2u)) {
        split = 1;
        while (!(lbu(msg + static_cast<uint32_t>(split)) < 2u) && split < 128) {
            ++split;
        }
    }

    if (lbu(msg + static_cast<uint32_t>(split)) == 1u) {
        sb(msg + static_cast<uint32_t>(split), 0);
        const int32_t w1 = text_width_small(msg, -1);
        const int32_t x1 = 0xe0 - (w1 >> 1);
        const int32_t w2 = text_width_small(msg + 1u + static_cast<uint32_t>(split), -1);
        const int32_t x2 = 0xe0 - (w2 >> 1);
        int32_t box_x = x1;
        int32_t box_w = w1;
        if (!(x1 < x2)) {
            box_x = x2;
            box_w = w2;
        }
        hud_sprite(hud_frame(0x7580u, 0), box_x + 0x20, y_base, box_w, 0x36, sx64(alpha));
        hud_sprite(hud_frame(0x7580u, 1), box_x, y_base, 0x20, 0x36, sx64(alpha));
        hud_sprite_flip(hud_frame(0x7580u, 1), box_x + box_w + 0x20, y_base, 0x20, 0x36, sx64(alpha));
        text_centered_small(0x100, text_y, text_col, msg, -1);
        text_centered_small(0x100, y_base + 0x1b, text_col, msg + 1u + static_cast<uint32_t>(split), -1);
    } else {
        const int32_t w = text_width_small(msg, -1);
        const int32_t box_x = 0xe0 - (w >> 1);
        hud_sprite(hud_frame(0x7580u, 1), box_x, y_base, 0x20, 0x20, sx64(alpha));
        hud_sprite(hud_frame(0x7580u, 0), box_x + 0x20, y_base, w, 0x20, sx64(alpha));
        hud_sprite_flip(hud_frame(0x7580u, 1), box_x + w + 0x20, y_base, 0x20, 0x20, sx64(alpha));
        text_centered_small(0x100, text_y, text_col, msg, -1);
    }

    const int32_t level = lw(0x0015ed84u);
    if (level != 5 && level != 0x10) {
        return;
    }

    const Vec4 kiosk = (level == 5)
        ? Vec4{u2f(0x43934000u), u2f(0x43616666u), u2f(0x42c80000u), 0.0f}
        : Vec4{u2f(0x42e76666u), u2f(0x438f8000u), u2f(0x42ef0000u), 0.0f};
    if (!(vec3_distance(V(0x0013f3d0u), kiosk) < 5.0f)) {
        return;
    }

    const uint32_t rec_off = (lw(0x0015ed84u) == 0x10) ? 4u : 0u;
    sw(OV(0x0015f648u), static_cast<uint32_t>(y_base + 0x64));
    const int32_t best_frames = lw(0x0015ee58u + rec_off);
    const int32_t high_score = lw(0x0015ee68u + rec_off);
    if (best_frames == 0) {
        return;
    }

    const int32_t frames_per_min = lw(0x0015ed80u) != 0 ? 0xbb8 : 0xe10;
    const int32_t mins = ee_div(best_frames, frames_per_min);
    const int32_t fps = ee_div(frames_per_min, 60);
    int32_t rem = best_frames - mins * frames_per_min;
    const int32_t secs = ee_div(rem, fps);
    rem -= secs * fps;
    const int32_t csecs = ee_div(rem * 100, fps);

    const uint32_t str_buf = ee_stack_alloc(0xa0);
    const int32_t time_label = sub_00203ad8(0x531b);
    libc_sprintf(str_buf, OV(0x001e84c0u), {sx64(time_label), sx64(mins), sx64(secs), sx64(csecs)});
    hud_message_box(0x100, y_base + 0x28, text_col, str_buf);

    if (high_score != 0) {
        const int32_t pts_label = sub_00203ad8(0x531c);
        libc_sprintf(str_buf, OV(0x0015f778u), {sx64(pts_label), sx64(high_score)});
        hud_message_box(0x100, y_base + 0x48, text_col, str_buf);
    }
    ee_stack_free(0xa0);
}

}  // namespace rac1
