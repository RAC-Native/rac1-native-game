// Ported overlay code: the Quick Select page's ring and the Items page's statistics panel (see
// pause_menu_pages.h).
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <host/level_slots.h>
#include <host/quick_rings.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};   // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1
constexpr OvConst<0x00166dc0u> CAM{};  // the camera's position
constexpr uint32_t QS = 0x00141ea0;   // the quick select's 8 items
// orig: (helper) repeated in 0x0027a3e8 / 0x0027a610: the C remainder by 8 the original computes
// with a shift (x - ((x < 0 ? x + 7 : x) >> 3 << 3))
int32_t rem8(int32_t x) { return x - (((x < 0 ? x + 7 : x) >> 3) << 3); }
// orig: (helper) the item of the focused grid's selected cell
int32_t focused_item() {
    const uint32_t fw = lwu(lwu(MS + 4) + 0x40);
    return lh(lwu(fw + 0x48) + static_cast<uint32_t>(lw(fw + 0x3c) * 10) + 6);
}
// orig: (helper) the pulsing frame colour (0x15f3f8's low 6 bits), as 0x27b590 / 0x27e768 build it
uint32_t pulse_colour() {
    const int32_t a = abs_i(static_cast<int32_t>(lwu(OV(0x0015f3f8)) & 0x3f) - 0x20);
    return (static_cast<uint32_t>(a + 0x40) * 0x10202u) | 0x80000000u;
}
// orig: (helper) EE CVT.W.S: truncate toward zero, saturating
int32_t cvt_w_s(float f) {
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}
}  // namespace

// Text right aligned at x in the big font (TEX0 3, font 0x1c3890).
// orig: 0x001fb918
void text_right_big(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    const int32_t w = text_width_big(s, n);
    text_draw(x - w, y, colour, s, n, effect_tex0(3), OV(0x001c3890));
}

// The 80 bytes 0x14bec0: how many are set, at most 40.
// orig: 0x00264588
int32_t bytes_14bec0_count() {
    int32_t n = 0;
    for (uint32_t i = 0; i < 0x14; ++i)
        for (uint32_t k = 0; k < 4; ++k)
            if (lbu(0x0014bec0 + i * 4 + k) != 0) ++n;
    if (!(-1 < n)) n = 0;
    return n < 0x29 ? n : 0x28;
}

// 0x264588's count less four per gold bolt (0x2645f8), within 0..40.
// orig: 0x00264540
int32_t bytes_14bec0_left() {
    int32_t n = bytes_14bec0_count() - (gold_bolts_count() << 2);
    if (!(-1 < n)) n = 0;
    return n < 0x29 ? n : 0x28;
}

// --- Quick Select ------------------------------------------------------------------------------
// +0x30 the 8 items of the ring (copied from / back to 0x141ea0), +0x50 the selected place.

// L1 / R1 turn the ring's selection; cross puts the focused grid's item (owned) there (out of its
// old place), the selection moving on; the quick select's use counted (0x1418f0 / 0x1418f2 the
// time in 600ths of 0x15eea4, 0x1418f4 the levels).
// orig: 0x0027a3e8
int32_t menu_quick_update(uint32_t w) {
    quick_rings_menu_update(w);  // (enhancement: L2 / R2 change the ring edited, src/host/quick_rings.h)
    const int32_t item = focused_item();
    const int32_t old = lw(w + 0x50);
    if ((lwu(PAD + 0x1c4) & 8) != 0) sw(w + 0x50, static_cast<uint32_t>(rem8(old + 1)));
    if ((lwu(PAD + 0x1c4) & 4) != 0) sw(w + 0x50, static_cast<uint32_t>(rem8(lw(w + 0x50) + 7)));
    if (lw(w + 0x50) != old) moby_sound_start(1, 0x11, lwu(w + 0x14));
    if (item == 0) return 0;
    if (lbu(0x0013d4c0 + static_cast<uint32_t>(item)) == 0) return 0;
    if ((lwu(PAD + 0x1c4) & 0x40) == 0) return 0;
    constexpr uint32_t S = 0x00141848;
    const uint32_t uses = lhu(S + 0xa8);
    if (!(0xfffeu < uses)) sh(S + 0xa8, uses + 1);
    if (static_cast<int32_t>(lhu(S + 0xaa)) < ee_div(frame_scale_i(lw(0x0015eea4)), 0x258))
        sh(S + 0xaa, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    sw(S + 0xac, lwu(S + 0xac) | (1u << (lw(0x0015ed84) & 31)) | 0x80000000u);
    for (uint32_t i = 0; i < 8; ++i) {
        if (lw(w + 0x30 + i * 4) == item) {
            sw(w + 0x30 + i * 4, 0);
            break;
        }
    }
    sw(w + 0x30 + static_cast<uint32_t>(lw(w + 0x50)) * 4, static_cast<uint32_t>(item));
    sw(w + 0x50, static_cast<uint32_t>(rem8(lw(w + 0x50) + 1)));
    return 0;
}

// The ring focused in: its items from 0x141ea0, the selection on the first empty place.
// orig: 0x0027a610
int32_t menu_quick_focus_in(uint32_t w) {
    // (enhancement: with three rings, the ring the quick select shows: ring 1's are 0x141ea0's, src/host/quick_rings.h)
    for (uint32_t i = 0; i < 8; ++i) sw(w + 0x30 + i * 4, quick_rings_menu_item(w, static_cast<int>(i)));
    sw(w + 0x50, 0);
    if (lwu(w + 0x30) != 0) {
        for (;;) {
            const int32_t k = lw(w + 0x50) + 1;
            sw(w + 0x50, static_cast<uint32_t>(k));
            if (lwu(w + 0x30 + static_cast<uint32_t>(k) * 4) == 0) break;
            if (!(k < 8)) break;
        }
    }
    sw(w + 0x50, static_cast<uint32_t>(rem8(lw(w + 0x50))));
    return 0;
}

// The ring focused out: its items back to 0x141ea0.
// orig: 0x0027a6a8
int32_t menu_quick_focus_out(uint32_t w) {
    if (quick_rings_menu_focus_out(w)) return 0;  // (enhancement: ring 2 or 3 edited, src/host/quick_rings.h)
    for (uint32_t i = 0; i < 8; ++i) sw(QS + i * 4, lwu(w + 0x30 + i * 4));
    return 0;
}

// The ring: 8 places on a circle (from the top, clockwise), the selected one on a pulsing frame,
// an empty one a grey square, an item its HUD icon (+0x38 of the item table; +4 gold); the L1 /
// R1 arrows (frame 0xe99e) and their texts.
// orig: 0x0027e768
int32_t menu_quick_draw(uint32_t w) {
    texture_block_begin(0);
    const int32_t h = lw(w + 0x24), wd = lw(w + 0x20);
    const int32_t mn = wd < h ? wd : h;
    const float cx = fpu_mul(static_cast<float>(wd), 0.5f), cy = fpu_mul(static_cast<float>(h), 0.5f);
    const float r = fpu_sub(fpu_mul(static_cast<float>(mn), 0.5f), 40.0f);
    for (int32_t i = 0; i < 8; ++i) {
        const float a = angle_wrap(fpu_add(fpu_mul(static_cast<float>(i), u2f(0x3f490fdb)), u2f(0xbfc90fdb)));
        const float x = fpu_add(cx, fpu_mul(cos_vu(a), r));
        const float y = fpu_add(cy, fpu_mul(sin_vu(a), r));
        if (i == lw(w + 0x50)) {
            const int32_t px = cvt_w_s(x), py = cvt_w_s(y);
            gs_flat_rect(px - 0x13, py - 0x13, px + 0x13, py + 0x13, pulse_colour(), 0);
            gs_flat_rect(px - 0x12, py - 0x12, px + 0x12, py + 0x12, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00160270)))), 0);
        }
        const int32_t item = lw(w + 0x30 + static_cast<uint32_t>(i) * 4);
        if (item == 0) {
            const int32_t px = cvt_w_s(x), py = cvt_w_s(y);
            gs_flat_rect(px - 0xf, py - 0xf, px + 0xf, py + 0xf, 0x40404040ull, 0);
            continue;
        }
        const int32_t f = hud_frame(lhu(OV(0x00179ac0) + static_cast<uint32_t>(item * 0x4c) + 0x38), lbu(0x0013e520 + static_cast<uint32_t>(item)) != 0 ? 4 : 0);
        hud_sprite(f, cvt_w_s(x) - 0x11, cvt_w_s(y) - 0x11, 0x20, 0x20, 0x80);
    }
    hud_sprite(hud_frame(0xe99e, 0), 8, 0x27, 0x20, -0x20, 0x80);
    hud_sprite(hud_frame(0xe99e, 0), lw(w + 0x20) - 10, 0x27, -0x20, -0x20, 0x80);
    text_small(0x28, 0xf, 0x80ffa888ull, OV(0x00160398), -1);
    text_small(lw(w + 0x20) - 0x3c, 0xf, 0x80ffa888ull, OV(0x001603a0), -1);
    quick_rings_menu_draw(w);  // (enhancement: L2 / R2 and the ring edited, src/host/quick_rings.h)
    texture_block_end();
    return 2;
}

// --- Items: the statistics panel ---------------------------------------------------------------

// Focus in: its moby (class 0x46e) in front of the camera (x + 8, y + 0.5, z - 0.1), turning
// (0x27cd68).
// orig: 0x0027c690
int32_t menu_stats_focus_in(uint32_t w) {
    // (enhancement: on a level mod's level whose base level has no gold bolt class (Veldin's data has none: the game never
    // shows this panel there), the bolt class 0x10 stands in for it, src/host/level_slots.h)
    const uint32_t cls = level_slot_current() >= 0 && lbu(OV(0x00197bc0) + 0x46e) == 0xff ? 0x10u : 0x46eu;
    const uint32_t m = menu_moby_spawn(cls);
    if (m == 0) return 0;
    sw(w + 0x44, m);
    sh(m + 0x34, 0);
    const uint32_t d = lwu(m + 0x78);
    swc1(m + 0x10, fpu_add(lwc1(CAM), 8.0f));
    swc1(m + 0x14, fpu_add(lwc1(CAM + 4), 0.5f));
    swc1(m + 0x44, u2f(0xbff33333));
    sw(m + 0x74, OVF(0x0027cd68));
    swc1(m + 0x18, fpu_sub(lwc1(CAM + 8), u2f(0x3dcccccd)));
    sw(d, w);
    return 0;
}

// Focus out: its moby freed.
// orig: 0x0027c738
int32_t menu_stats_focus_out(uint32_t w) {
    sw(w + 0x44, static_cast<uint32_t>(menu_moby_free(lwu(w + 0x44))));
    return 0;
}

// The panel's moby: turning 0.02 a frame (+0x40).
// orig: 0x0027cd68
void menu_stats_moby_update(uint32_t m) { swc1(m + 0x40, angle_add(lwc1(m + 0x40), u2f(0x3ca3d70a))); }

// The statistics: its moby (the moby pass, 0x24ff08), a title (0x4f4e; on level 3 a space put after its
// '-'), three names (0x4f4f..0x4f51) and their counts (0x264588, gold bolts x 4, 0x264540), with
// shadows; a line under them.
// orig: 0x0027c768
int32_t menu_stats_draw(uint32_t w) {
    if (lwu(w + 0x44) != 0) moby_draw_list(lwu(w + 0x44), 1);
    texture_block_begin(0);
    const uint32_t sp = ee_stack_alloc(0xc0);  // the text +0, the box +0x80, its template +0xa0
    const uint32_t tpl = sp + 0xa0, box = sp + 0x80;
    libc_memset(tpl, 0, 0x18);
    const int32_t third = ee_div(lw(w + 0x20), 3);
    sh(tpl + 4, 8);
    sh(tpl + 2, lhu(w + 0x24));
    sh(tpl + 0x10, 0x10);
    sh(tpl + 0x12, 5);
    sh(tpl + 6, static_cast<uint32_t>(third));
    for (uint32_t o = 0; o < 0x18; o += 2) sh(box + o, lhu(tpl + o));
    sh(box + 8, static_cast<uint32_t>((lh(box + 4) + lh(box + 6)) >> 1));
    libc_strcpy(sp, sub_00203ad8(0x4f4e));
    if (lw(0x0015ed88) == 3) {
        const uint32_t p = libc_strchr(sp, 0x2d);
        if (p != 0) {
            int32_t n = libc_strlen(sp);
            uint32_t a = sp + static_cast<uint32_t>(n);
            while (p < a) {
                sb(a + 1, lbu(a));
                --a;
                --n;
            }
            sb(static_cast<uint32_t>(n) + sp + 1, 0x20);
        }
    }
    text_box_small(box, 0x8000c0c0ull, sp, -1);
    const int32_t y = (lw(0x0013e504) - lh(box + 0xe)) >> 1;
    sh(box + 0x12, lhu(box + 0x12) ^ 4u);
    sh(box + 0xa, static_cast<uint32_t>(y));
    text_box_small(box, 0x8000c0c0ull, sp, -1);
    static const int32_t ys[3] = {0x1d, 0x36, 0x54};
    for (int32_t k = 0; k < 3; ++k)
        text_right(lw(OV(0x00160278)) + 0xc8, lw(OV(0x0016027c)) + ys[k], 0x80000000ull, sub_00203ad8(0x4f4f + k), -1);
    for (int32_t k = 0; k < 3; ++k) text_right(0xc8, ys[k], 0x80ffa888ull, sub_00203ad8(0x4f4f + k), -1);
    for (int32_t k = 0; k < 3; ++k) {
        int32_t v;
        if (k == 0) v = bytes_14bec0_count();
        else if (k == 1) v = gold_bolts_count() << 2;
        else v = bytes_14bec0_left();
        std::vector<uint64_t> a(1, static_cast<uint64_t>(static_cast<int64_t>(v)));
        libc_sprintf(sp, OV(0x00160360), a);
        text_right_big(lw(OV(0x00160278)) + 0xf0, lw(OV(0x0016027c)) + ys[k], 0x80000000ull, sp, -1);
        text_right_big(0xf0, ys[k], 0x80ffa888ull, sp, -1);
    }
    gs_flat_rect(lw(OV(0x00160278)) + 0xd0, lw(OV(0x0016027c)) + 0x4d, lw(OV(0x00160278)) + 0xf2, lw(OV(0x0016027c)) + 0x50, 0x80000000ull, 0);
    gs_flat_rect(0xd0, 0x4d, 0xf2, 0x50, 0x80ffa888ull, 0);
    ee_stack_free(0xc0);
    texture_block_end();
    return 8;
}

}  // namespace rac1
