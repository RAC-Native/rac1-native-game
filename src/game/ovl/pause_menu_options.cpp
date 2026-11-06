// Ported overlay code: the Options page's sub-pages (see pause_menu_pages.h): Sound (the effects / music volumes and the
// stereo switch),
// Camera (a list of settings, each a byte cycled through its texts), Subtitles and Help Desk
// (on / off switches), Quit Game.
#include <game/enhance/advanced.h>
#include <game/enhance/language.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libc.h>
#include <game/lib/snd989.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

namespace rac1 {

// (enhancement, advanced.cpp) Options > Sound's output row with 5.1 / 7.1. The game's row is Mono / Stereo, its byte
// 0x15ede8 (the game's options, in its save; 0 mono); 5.1 and 7.1 come after Stereo: the host's choice (the audio
// device's channels, kept in the preferences), the game's byte left at Stereo, so nothing the game keeps or reads changes.
// - advanced_audio_channels: the device's channels, 2 / 6 (5.1) / 8 (7.1) (RAC1_AUDIO_CHANNELS over the preferences;
//   2 while every enhancement is off)
// - advanced_sound_output_cycle: cross on the row: the next of Mono, Stereo, 5.1, 7.1 (the game's byte 0 for Mono, 1
//   for the others; the device reopened); false while every enhancement is off (the game's own toggle then)
// - advanced_sound_output_text: the row's text when the game's byte says Stereo: "5.1" / "7.1", else `stereo_text`
int advanced_audio_channels();
bool advanced_sound_output_cycle(uint32_t mono_byte);
int32_t advanced_sound_output_text(int32_t stereo_text);

namespace {
constexpr OvConst<0x001b9cf0u> MS{};   // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1
constexpr OvConst<0x001611c0u> CUR{};  // the frame packet's cursor
constexpr uint32_t VOL_FX = 0x0015edf0, VOL_MUSIC = 0x0015edec, MONO = 0x0015ede8;
// orig: (helper) repeated in the pages' updates: when the widget is focused, Start / Select /
// L1 close the menu (1, unless +0x124), triangle goes to the parent page (or closes it, -1, on
// a root page without +0x124); false when the update goes on
bool exit_keys(int32_t& r) {
    if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) {
        r = 1;
        return true;
    }
    if ((lwu(PAD + 0x1c4) & 0x10) != 0) {
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) {
            sw(MS + 8, parent);
        } else if (lw(MS + 0x124) == 0) {
            r = -1;
            return true;
        }
    }
    return false;
}
// orig: (helper) a 64-bit register holding a sign-extended word
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// Text in the small font (TEX0 1, font 0x1c3150).
// orig: 0x001fb370
void text_small(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    text_draw(x, y, colour, s, n, effect_tex0(1), OV(0x001c3150));
}

// Text in box `b` in the small font.
// orig: 0x001fc438
void text_box_small(uint32_t b, uint64_t colour, uint32_t s, int32_t n) {
    text_box(b, colour, s, n, effect_tex0(1), OV(0x001c3150));
}

// A HUD frame's sprite from (x0, y0) to (x1, y1) (1/16 pixels) with the texels (u0, v0) ..
// (u1, v1), alpha `a`, the HUD's z (0x17e4e4).
// orig: 0x0023c5f8
void hud_sprite_uv(int32_t frame, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t u0, int32_t v0, int32_t u1, int32_t v1, int32_t a) {
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000005);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000005);
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    sd(q, 0x7400000000008001ull);
    sd(q + 8, 0x05353106);
    const uint64_t t = hud_tex0(frame);
    sd(q + 0x20, (sx64(a) << 24) | 0x7f7f7f);
    sd(q + 0x28, sx64(u0) | (sx64(v0) << 16));
    sd(q + 0x10, t);
    sd(q + 0x18, 0x156);
    const uint64_t z = sx64(lw(OV(0x0017e4d8) + 0xc)) << 32;
    const int32_t ya = y0 + lw(0x0013e514) - 8, xa = x0 + lw(0x0013e510) - 8;
    sd(q + 0x38, sx64(u1) | (sx64(v1) << 16));
    sd(q + 0x30, sx64(xa) | (sx64(ya) << 16) | z);
    const int32_t yb = y1 + lw(0x0013e514) - 8, xb = x1 + lw(0x0013e510) - 8;
    const uint64_t z2 = sx64(lw(OV(0x0017e4d8) + 0xc)) << 32;
    sd(q + 0x48, 0);
    sd(q + 0x40, sx64(xb) | (sx64(yb) << 16) | z2);
    sw(CUR, lwu(CUR) + 0x50);
}

// --- Sound -------------------------------------------------------------------------------------

// The sound page: up / down pick the effects volume, the music volume or the stereo switch
// (+0x40, wrapping); left / right held move a volume by 3 (0..0x400), the volume table 0x13e550
// set from them; cross switches mono / stereo (989snd command 0xb).
// orig: 0x0027a778
int32_t menu_sound_update(uint32_t w) {
    int32_t r = 0;
    if (exit_keys(r)) return r;
    const int32_t old = lw(w + 0x40);
    if ((lwu(PAD + 0x1c4) & 0x1000) != 0) sw(w + 0x40, static_cast<uint32_t>(ee_rem(old + 2, 3)));
    if ((lwu(PAD + 0x1c4) & 0x4000) != 0) sw(w + 0x40, static_cast<uint32_t>(ee_rem(lw(w + 0x40) + 1, 3)));
    if (lw(w + 0x40) != old || lwu(lwu(MS + 4) + 0x80) != 0) {
        moby_sound_start(1, 0x11, lwu(w + 0x14));
        if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(lwu(OV(0x001602a0)) + static_cast<uint32_t>(lw(w + 0x40)) * 4));
    }
    const int32_t music0 = lw(VOL_MUSIC), fx0 = lw(VOL_FX);
    if ((lwu(PAD + 0x1c0) & 0x2000) != 0) {
        const int32_t k = lw(w + 0x40);
        if (k == 0) {
            const int32_t v = fx0 + 3;
            sw(VOL_FX, static_cast<uint32_t>(v < 0x401 ? v : 0x400));
        }
        if (k == 1) {
            const int32_t v = music0 + 3;
            sw(VOL_MUSIC, static_cast<uint32_t>(v < 0x401 ? v : 0x400));
        }
    }
    if ((lwu(PAD + 0x1c0) & 0x8000) != 0) {
        const int32_t k = lw(w + 0x40);
        if (k == 0) {
            int32_t v = lw(VOL_FX) - 3;
            if (!(-1 < v)) v = 0;
            sw(VOL_FX, static_cast<uint32_t>(v));
        }
        if (k == 1) {
            int32_t v = lw(VOL_MUSIC) - 3;
            if (!(-1 < v)) v = 0;
            sw(VOL_MUSIC, static_cast<uint32_t>(v));
        }
    }
    const int32_t music = lw(VOL_MUSIC), fx = lw(VOL_FX);
    if (music0 != music || fx0 != fx) {
        constexpr uint32_t T = 0x0013e550;
        sw(T + 0x50, static_cast<uint32_t>(music));
        sw(T + 0x4c, static_cast<uint32_t>(music));
        sw(T + 0x48, static_cast<uint32_t>(ee_div(fx * 8, 10)));
        const int32_t v = ee_div(fx * 8 - fx, 10);
        sw(T + 0x54, static_cast<uint32_t>(v));
        sw(T + 0x5c, static_cast<uint32_t>(fx));
        sw(T + 0x58, static_cast<uint32_t>(v));
    }
    if ((lwu(PAD + 0x1c4) & 0x40) != 0) {
        if (lw(w + 0x40) == 2 && !advanced_sound_output_cycle(MONO)) sw(MONO, lw(MONO) == 0 ? 1u : 0u);  // (enhancement: also 5.1 / 7.1)
        snd_cmd_0b(lw(MONO) == 0 ? 1u : 0u);
        moby_sound_start(0, 0x11, lwu(w + 0x14));
    }
    return 0;
}

// The sound page left: the volume table's word 0x13e5a0 = effects volume * 8 / 10.
// orig: 0x0027a748
int32_t menu_sound_leave(uint32_t) {
    sw(0x0013e5a0, static_cast<uint32_t>(ee_div(lw(VOL_FX) << 3, 10)));
    return 0;
}

// The sound page: the two volumes' names, their bars (a dark frame, the HUD frame 0xe99e's
// texels as the level) and the stereo switch's state; the selected line highlighted.
// orig: 0x0027aa48
int32_t menu_sound_draw(uint32_t w) {
    const int32_t cx = lw(w + 0x20) >> 1, q = lw(w + 0x24) >> 2;
    uint64_t c = 0x80ffa888ull;
    texture_block_begin(0);
    const int32_t y0 = q - 8;
    const int32_t xl = cx - 8, xf = cx + 7, xb = cx + 9;
    if (lw(w + 0x40) == 0) c = 0x8020ffffull;
    const uint32_t s0 = sub_00203ad8(0x5212);
    const int32_t yi = q - 6, xs = cx + 0x4a;
    const int32_t q2 = q << 1;
    const int32_t xb4 = xb << 4;
    text_right(xl, y0, c, s0, -1);
    gs_flat_rect(xf, y0, lw(w + 0x20) - 0x3f, q + 8, 0x80696969ull, 0);
    gs_flat_rect(xb, yi, lw(w + 0x20) - 0x41, q + 6, 0x80383838ull, 0);
    uint64_t sel = 0x8020ffffull;
    const int32_t xt = cx + 8;
    int32_t v = (lw(w + 0x20) - xs) * lw(VOL_FX);
    int32_t len = (-1 < v ? v : v + 0x3ff) >> 10;
    hud_sprite_uv(hud_frame(0xe99e, 8), xb4, yi << 4, (cx + len + 8) << 4, (q + 5) << 4, 0, 0xa0, 0x1f0, 0x150, 0x80);
    uint64_t c2 = sel;
    if ((lw(w + 0x40) ^ 1) != 0) c2 = 0x80ffa888ull;
    text_right(xl, q2 - 8, c2, sub_00203ad8(0x5213), -1);
    gs_flat_rect(xf, q2 - 8, lw(w + 0x20) - 0x3f, q2 + 8, 0x80696969ull, 0);
    gs_flat_rect(xb, q2 - 6, lw(w + 0x20) - 0x41, q2 + 6, 0x80383838ull, 0);
    v = (lw(w + 0x20) - xs) * lw(VOL_MUSIC);
    len = (-1 < v ? v : v + 0x3ff) >> 10;
    const int32_t f = hud_frame(0xe99e, 9);
    hud_sprite_uv(f, xb4, (q2 - 6) << 4, (cx + len + 8) << 4, (q2 + 5) << 4, 0, 0xa0, 0x1f0, 0x150, 0x80);
    const int32_t y2 = q2 + q - 8;
    uint64_t c3 = sel;
    if ((lw(w + 0x40) ^ 2) != 0) c3 = 0x80ffa888ull;
    text_right(xl, y2, c3, sub_00203ad8(0x5214), -1);
    text_small(xt, y2, 0x80ffa888ull, sub_00203ad8(lw(MONO) == 0 ? 0x5215 : advanced_sound_output_text(0x5216)), -1);  // (enhancement: 5.1 / 7.1)
    texture_block_end();
    return 2;
}

// enhancement (graphics.cpp; declared here: the headers of src/game are the generated code's fingerprint): our pages' greyed rows
bool graphics_row_dimmed(uint32_t w, int32_t i);

// --- Camera: a list of settings ----------------------------------------------------------------
// +0x34 the settings (0x18 bytes: text, the byte it sets, up to 4 texts of its values; ended by
// text 0), +0x38 the selected one.

// orig: 0x0027ef00
int32_t menu_settings_update(uint32_t w) {
    if (lwu(lwu(MS + 4) + 0x40) != w) return 0;
    int32_t r = 0;
    if (exit_keys(r)) return r;
    const int32_t old = lw(w + 0x38);
    if ((lwu(PAD + 0x1c4) & 0x1000) != 0 && old != 0) sw(w + 0x38, static_cast<uint32_t>(old - 1));
    if ((lwu(PAD + 0x1c4) & 0x4000) != 0) {
        const int32_t k = lw(w + 0x38);
        if (lwu(lwu(w + 0x34) + static_cast<uint32_t>(k * 0x18) + 0x18) != 0) sw(w + 0x38, static_cast<uint32_t>(k + 1));
    }
    if (old != lw(w + 0x38)) moby_sound_start(1, 0x11, lwu(w + 0x14));
    const uint32_t e = lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x38) * 0x18);
    int32_t n = 0;
    if (lwu(e + 8) != 0) {
        for (uint32_t p = e + 0xc;; p += 4) {
            const uint32_t v = lwu(p);
            ++n;
            if (v == 0 || !(n < 4)) break;
        }
    }
    if (advanced_settings_dpad(w, e)) return 0;  // enhancement: left / right on our pages' rows
    if ((lwu(PAD + 0x1c4) & 0x40) == 0) return 0;
    const uint32_t b = lwu(e + 4);
    if (b == 0) return 0;
    sb(b, static_cast<uint32_t>(ee_rem(static_cast<int32_t>(lbu(b)) + 1, n)));
    advanced_setting_changed(b);  // enhancement: the Advanced settings that go together
    moby_sound_start(0, 0x11, lwu(w + 0x14));
    return 0;
}

// The settings: each one's name (the selected one highlighted) and its value's text right
// aligned, on lines sharing the box's height.
// orig: 0x0027f0a8
int32_t menu_settings_draw(uint32_t w) {
    sub_002a0ba8(0x47, 0x2004b);
    texture_block_begin(0);
    int32_t n = 0;
    for (uint32_t p = lwu(w + 0x34); lwu(p) != 0; p += 0x18) ++n;
    const int32_t step = ee_div(lw(w + 0x24), n + 1);
    int32_t y = step - 8;
    if (lwu(lwu(w + 0x34)) != 0) {
        int32_t i = 0;
        uint32_t off = 0;
        do {
            const uint32_t e = lwu(w + 0x34) + off;
            uint64_t c = (i ^ lw(w + 0x38)) != 0 ? 0xffffffff80ffa888ull : 0xffffffff8020ffffull;
            uint64_t cv = 0x80ffa888ull;
            if (graphics_row_dimmed(w, i)) {  // enhancement: a row of ours that does nothing now, greyed (its selection a dim yellow)
                c = (i ^ lw(w + 0x38)) != 0 ? 0xffffffff80606060ull : 0xffffffff80207070ull;
                cv = 0x80606060ull;
            }
            off += 0x18;
            text_small(0xc, y, c, sub_00203ad8(lw(e)), -1);
            ++i;
            const int32_t xr = lw(w + 0x20) - 0xc;
            const uint32_t t = e + static_cast<uint32_t>(lbu(lwu(e + 4))) * 4;
            text_right(xr, y, cv, sub_00203ad8(lw(t + 8)), -1);
            y += step;
        } while (lwu(lwu(w + 0x34) + off) != 0);
    }
    texture_block_end();
    return 2;
}

// --- Subtitles / Help Desk: on / off switches --------------------------------------------------
// +0x34 the switches (0x14 bytes: text, the byte switched, the on / off texts, flags: 1 the help
// desk's, whose switch fades the screen or shows the help screen), +0x38 the selected one, +0x3c
// a countdown (the fade's).

// orig: 0x0027ea70
int32_t menu_switches_update(uint32_t w) {
    const bool focused = lwu(lwu(MS + 4) + 0x40) == w;
    const int32_t t = lw(w + 0x3c);
    if (t != 0) {
        const int32_t v = t - 1;
        const int32_t k = v < 5 ? v : 4;
        sw(w + 0x3c, static_cast<uint32_t>(v));
        swc1(OV(0x0015f3fc), fpu_mul(static_cast<float>(k), 0.25f));
        return 0;
    }
    if (!focused) return 0;
    int32_t r = 0;
    if (exit_keys(r)) return r;
    const int32_t old = lw(w + 0x38);
    if ((lwu(PAD + 0x1c4) & 0x1000) != 0 && old != 0) sw(w + 0x38, static_cast<uint32_t>(old - 1));
    if ((lwu(PAD + 0x1c4) & 0x4000) != 0) {
        const int32_t k = lw(w + 0x38);
        if (lwu(lwu(w + 0x34) + static_cast<uint32_t>(k * 0x14) + 0x14) != 0) sw(w + 0x38, static_cast<uint32_t>(k + 1));
    }
    if ((lwu(PAD + 0x1c4) & 0x40) != 0) {
        moby_sound_start(0, 0x11, lwu(w + 0x14));
        const uint32_t e = lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x38) * 0x14);
        if ((lwu(e + 0x10) & 1) != 0) {
            if (lbu(OV(0x0016040c)) != 0) {
                screen_open(6, lwu(MS + 4));
            } else {
                screen_fade(4);
                sw(w + 0x3c, static_cast<uint32_t>(frame_scale_i(0x10)));
                sb(OV(0x0016040c), lbu(OV(0x0016040c)) == 0 ? 1u : 0u);
            }
        } else {
            const uint32_t b = lwu(e + 4);
            if (b != 0) sb(b, lbu(b) == 0 ? 1u : 0u);
            language_switch_changed(b);  // enhancement: the Subtitles switch kept as the choice (Options > Language)
        }
    }
    if (lw(w + 0x38) != old) moby_sound_start(1, 0x11, lwu(w + 0x14));
    return 0;
}

// The switches: each one's name (the selected one highlighted) and "on" / "off" right aligned;
// with flag 1 and no switches, a text (0x4fc0) in the box.
// orig: 0x0027ec78
int32_t menu_switches_draw(uint32_t w) {
    sub_002a0ba8(0x47, 0x2004b);
    texture_block_begin(0);
    if ((lwu(w + 0x30) & 1) != 0 && lwu(lwu(w + 0x34)) == 0) {
        const uint32_t buf = ee_stack_alloc(0x40);  // the box +0, its template +0x20
        const uint32_t tpl = buf + 0x20;
        libc_memset(tpl, 0, 0x18);
        const int32_t third = ee_div(lw(w + 0x24), 3);
        sh(tpl + 2, lhu(w + 0x24) + 1u);
        sh(tpl + 6, lhu(w + 0x20) + 1u);
        sh(tpl + 8, static_cast<uint32_t>(lw(w + 0x20) >> 1));
        sh(tpl, 1);
        sh(tpl + 4, 1);
        sh(tpl + 0x12, 1);
        sh(tpl + 0x10, 0x10);
        sh(tpl + 0xa, static_cast<uint32_t>(third));
        for (uint32_t o = 0; o < 0x18; o += 2) sh(buf + o, lhu(tpl + o));
        text_box_small(buf, 0x80ffa888ull, sub_00203ad8(0x4fc0), -1);
        ee_stack_free(0x40);
    }
    int32_t n = 0;
    for (uint32_t p = lwu(w + 0x34); lwu(p) != 0; p += 0x14) ++n;
    const int32_t step = ee_div(lw(w + 0x24), n + 1);
    int32_t y = step - 8;
    if (lwu(lwu(w + 0x34)) != 0) {
        int32_t i = 0;
        uint32_t off = 0;
        do {
            const uint32_t e = lwu(w + 0x34) + off;
            uint32_t on = 0;
            if (lwu(e + 4) != 0) on = lbu(lwu(e + 4));
            const uint64_t c = (i ^ lw(w + 0x38)) == 0 ? 0xffffffff8020ffffull : 0xffffffff80ffa888ull;
            text_small(0xc, y, c, sub_00203ad8(lw(e)), -1);
            const int32_t xr = lw(w + 0x20) - 0xc;
            const int32_t id = on != 0 ? lw(e + 8) : lw(e + 0xc);
            off += 0x14;
            text_right(xr, y, 0x80ffa888ull, sub_00203ad8(id), -1);
            ++i;
            y += step;
        } while (lwu(lwu(w + 0x34) + off) != 0);
    }
    texture_block_end();
    return 2;
}

// --- Quit Game ---------------------------------------------------------------------------------

// The quit page: Start / Select / L1 close the menu (-1), triangle goes back; circle quits the
// game (0x13d384 0, 0x15f5c0 -1, 0x15f5d8 / 0x15f570 1). Flag 0x20: the help planet set to the
// level when leaving.
// orig: 0x0027c410
int32_t menu_quit_update(uint32_t w) {
    const uint32_t k = lwu(PAD + 0x1c4);
    if ((k & 0xd00) != 0) {
        if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(0x0015ed84));
        return -1;
    }
    if ((k & 0x10) != 0) {
        if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(0x0015ed84));
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) sw(MS + 8, parent);
        else if (lw(MS + 0x124) == 0) return -1;
    }
    if ((lwu(PAD + 0x1c4) & 0x20) != 0) {
        sw(0x0013d384, 0);
        sw(OV(0x0015f5c0), 0xffffffffu);
        sw(OV(0x0015f5d8), 1);
        sw(OV(0x0015f570), 1);
    }
    return 0;
}

// The quit page: its question in the box, a line under it, the two answers' texts left
// aligned on the wider of them.
// orig: 0x0027c4d8
int32_t menu_quit_draw(uint32_t w) {
    const int32_t bw = lw(w + 0x20), bh = lw(w + 0x24);
    const uint32_t b = ee_stack_alloc(0x20);
    libc_memset(b, 0, 0x18);
    const int32_t cx = bw / 2;
    sh(b + 6, static_cast<uint32_t>(bw - 4));
    sh(b + 0xa, 6);
    sh(b + 0xc, static_cast<uint32_t>(bw));
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 1);
    sh(b + 2, static_cast<uint32_t>(bh));
    sh(b + 8, static_cast<uint32_t>(cx));
    sh(b + 0xe, static_cast<uint32_t>(bh));
    sh(b + 4, 4);
    texture_block_begin(0);
    sub_002a0ba8(0x47, 0x2004b);
    text_box_small(b, 0x80ffa888ull, sub_00203ad8(0x4f6d), -1);
    text_centered_small(cx, bh - 0x40, 0x80ffa888ull, sub_00203ad8(0x4f3f), -1);
    int32_t wd = text_width_small(sub_00203ad8(0x524b), -1);
    const int32_t w2 = text_width_small(sub_00203ad8(0x524f), -1);
    if (!(w2 < wd)) wd = w2;
    const int32_t x = (lw(w + 0x20) - wd) >> 1;
    text_small(x, bh - 0x28, 0x80ffa888ull, sub_00203ad8(0x524b), -1);
    text_small(x, bh - 0x14, 0x80ffa888ull, sub_00203ad8(0x524f), -1);
    ee_stack_free(0x20);
    texture_block_end();
    return 2;
}

}  // namespace rac1
