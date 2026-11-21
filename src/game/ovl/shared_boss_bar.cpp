// The bosses' health bar: one port of the copies four levels carry (Blarg, Umbris, Gemlik, the finale's Veldin). The
// copies are the same code relinked (compared with the relocated fields masked); what they name is the engine's (OV: the
// bar's x 0x15f8e8 and its two easing factors 0x15f8f0 / 0x15f8f4) or the base ELF's (bare: the screen's height
// 0x13e504, the word 0x15ed80, the level's number 0x15ed84). Both PAL builds have the same code: their table entries in it
// (one on 1.00, ten on 2.00) are relocated data (gp-relative words, the 2.00 core's moved words), no change of the code.
// Header shared_boss_bar.h.
#include <game/ovl/shared_boss_bar.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>

namespace rac1 {

// The bar: 3 segments (6 on Umbris and Gemlik, 7 on the finale's Veldin) of 32 pixels ending at the bar's x (0x15f8e8),
// its height from the screen's (0x2a above it, 0x32 with the word 0x15ed80 clear; 0x12 / 0xa from the top on Gemlik).
// Nothing while the fade +0x70 is 0; else its alpha is the fade / 8 of 128. The frame (frames 3 of HUD set 0x7562,
// flipped), the filled part (health / max, eased by 0x15f8f0 over the last segment and by 0x15f8f4 under the first,
// red to yellow to green; none: a blue-less red of no width), the cover (frames 1 / 2) and the icon (frames 4 / 0).
// orig: L6:0x00249738 L7:0x00262078 L13:0x00245cf8 L18:0x0023cd60
void boss_health_bar_draw(uint32_t m) {
    const int32_t screen_h = lw(0x0013e504);
    const int32_t layout_flag = lw(0x0015ed80);
    const int32_t level = lw(0x0015ed84);
    int32_t y = layout_flag == 0 ? screen_h - 0x32 : screen_h - 0x2a;
    int32_t segs = 3;
    const int32_t right = lw(OV(0x0015f8e8));
    if (level == 7 || level == 0xd) segs = 6;
    if (level == 0x12) segs = 7;
    if (lbu(m + 0x70) == 0) return;
    if (level == 0xd) y = layout_flag != 0 ? 0xa : 0x12;
    float fade = fpu_mul(int_to_float(static_cast<int32_t>(lbu(m + 0x70))), 0.125f);
    if (1.0f < fade) fade = 1.0f;
    else if (fade < 0.0f) fade = 0.0f;
    const float alpha_f = fpu_mul(fade, 128.0f);
    const int32_t bar_top = y + 8, bar_bottom = y + 0x1a, icon_y = y - 8;
    const int32_t frame_alpha = float_to_int(alpha_f);
    const int32_t cover_alpha = float_to_int(alpha_f);
    auto alpha = [](int32_t a) { return static_cast<uint64_t>(static_cast<int64_t>(a)); };
    const int32_t left = right - segs * 16;
    int32_t x = left;
    for (int32_t k = 0; k < segs; ++k, x += 0x20) hud_sprite_flip(hud_frame(0x7562, 3), x, y + 6, 0x20, 0x12, alpha(frame_alpha));

    int32_t width = 0;
    uint64_t colour = 0xffffffff800000ffull;  // (the original's lui / ori constant, sign-extended)
    if (lw(m + 0x74) != 0) {
        const float max = int_to_float(lw(m + 8));
        float share = fpu_div(int_to_float(lw(m + 0x74)), max);
        const float seg = fpu_div(1.0f, int_to_float(segs));
        const float last = fpu_mul(int_to_float(segs - 1), seg);
        if (last < share) share = fpu_add(fpu_mul(fpu_sub(share, last), lwc1(OV(0x0015f8f0))), last);
        else if (share < seg) share = fpu_add(fpu_mul(fpu_sub(share, seg), lwc1(OV(0x0015f8f4))), seg);
        width = float_to_int(fpu_mul(int_to_float(segs << 5), share));
        colour = 0.5f < share ? rgba_lerp(0x8000ffffu, 0x8000ff00u, fpu_sub(fpu_add(share, share), 1.0f))
                              : rgba_lerp(0x800000ffu, 0x8000ffffu, fpu_add(share, share));
    }
    gs_flat_rect(left, bar_top, left + width, bar_bottom, colour, 0);
    const int32_t icon_x = left - 0x28;
    x = left;
    for (int32_t k = 0; k < segs - 1; ++k, x += 0x20) hud_sprite(hud_frame(0x7562, 1), x, y, 0x20, 0x20, alpha(cover_alpha));
    hud_sprite(hud_frame(0x7562, 2), x, y, 0x20, 0x20, alpha(cover_alpha));
    hud_sprite(hud_frame(0x7562, 4), icon_x, icon_y, 0x30, 0x30, alpha(frame_alpha));
    hud_sprite(hud_frame(0x7562, 0), icon_x, icon_y, 0x30, 0x30, alpha(frame_alpha));
}

}  // namespace rac1
