// Ported overlay code: the widgets the menu's pages share (see pause_menu_pages.h): the text label (a title, a page's help
// text), the picture
// panel (an image read from the disc into one of the menu's buffers, unpacked and drawn),
// the menu's buffers (0x1ba1b8) and the image upload.
#include <game/enhance/advanced.h>
#include <host/level_slots.h>
#include <game/lib/libc.h>
#include <game/ovl/end_menu_tr.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/snd989.h>
#include <game/ovl/galactic_map.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/save_menus.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};    // the menu's state
constexpr uint32_t PAD = 0x0013c940;   // pad 1
constexpr OvConst<0x001ba1b8u> BUFS{};  // the menu's 5 buffers: address, flags (1 big, 2 taken, 4 being read)
// orig: (helper) the focused widget of the page shown
uint32_t focused() { return lwu(lwu(MS + 4) + 0x40); }
// orig: (helper) a halfword of a box (an EE address) plus `d`
void box_add(uint32_t a, uint32_t d) { sh(a, lhu(a) + d); }
}  // namespace

// orig: (helper) `jalr` to a widget function of the pages (a0 the widget, a1 1 for a change of
// focus): the ported ones
bool menu_page_widget_call(uint32_t fn, uint32_t w, int32_t a1, int32_t& r) {
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
        case 0x002779c8: r = menu_label_update(w); return true;
        case 0x00277f60: r = menu_label_focus(w); return true;
        case 0x00277f70: r = menu_label_draw(w); return true;
        case 0x00277f50: r = menu_nop_focus(w); return true;
        case 0x00277f58: r = menu_nop_focus2(w); return true;
        case 0x002799e0: r = menu_nop_update(w); return true;
        case 0x0027a728: r = menu_buffers_full(w); return true;
        case 0x0027da10: r = menu_picture_update(w); return true;
        case 0x0027df90: r = menu_picture_draw(w); return true;
        case 0x0027d928: r = menu_picture_focus_in(w); return true;
        case 0x0027d9c0: r = menu_picture_focus_out(w); return true;
        case 0x0027a778: r = menu_sound_update(w); return true;
        case 0x0027a748: r = menu_sound_leave(w); return true;
        case 0x0027aa48: r = menu_sound_draw(w); return true;
        case 0x0027ef00: r = menu_settings_update(w); return true;
        case 0x0027f0a8: r = menu_settings_draw(w); return true;
        case 0x0027ea70: r = menu_switches_update(w); return true;
        case 0x0027ec78: r = menu_switches_draw(w); return true;
        case 0x0027c410: r = menu_quit_update(w); return true;
        case 0x0027c4d8: r = menu_quit_draw(w); return true;
        case 0x002794a0: r = menu_grid_update(w); return true;
        case 0x002793f0: r = menu_grid_focus_in(w, a1); return true;
        case 0x0027b590: r = menu_grid_draw(w); return true;
        case 0x0027bb78: r = menu_model_focus_in(w); return true;
        case 0x0027bba0: r = menu_model_focus_out(w); return true;
        case 0x0027be78: r = menu_model_update(w); return true;
        case 0x0027c250: r = menu_model_draw(w); return true;
        case 0x0027bbe0: r = menu_ammo_update(w); return true;
        case 0x0027bd58: r = menu_ammo_draw(w); return true;
        case 0x0027cda0: r = menu_ammo_count_draw(w); return true;
        case 0x0027ba40: r = menu_hero_draw(w); return true;
        case 0x00281fb0: r = menu_hero_update(w); return true;
        case 0x00281d10: r = menu_hero_focus_in(w); return true;
        case 0x00281f00: r = menu_hero_focus_out(w); return true;
        case 0x0027a3e8: r = menu_quick_update(w); return true;
        case 0x0027a610: r = menu_quick_focus_in(w); return true;
        case 0x0027a6a8: r = menu_quick_focus_out(w); return true;
        case 0x0027e768: r = menu_quick_draw(w); return true;
        case 0x0027c690: r = menu_stats_focus_in(w); return true;
        case 0x0027c738: r = menu_stats_focus_out(w); return true;
        case 0x0027c768: r = menu_stats_draw(w); return true;
        case 0x00279aa8: r = planet_map_update(w); return true;
        case 0x002799e8: r = planet_map_leave(w); return true;
        case 0x0027a710: r = planet_map_enter(w); return true;
        case 0x0027cf78: r = planet_map_widget_draw(w); return true;
        case 0x0027e498: r = planet_picture_draw(w); return true;
        case 0x0027cfb0: r = planet_map_legend_draw(w); return true;
        case 0x0027a068: r = missions_focus_in(w); return true;
        case 0x0027a108: r = missions_update(w); return true;
        case 0x0027d2d0: r = missions_draw(w); return true;
        case 0x0027d5d8: r = mission_picture_update(w); return true;
        case 0x00280bd0: r = save_slots_update(w); return true;
        case 0x00280f20: r = load_slots_update(w); return true;
        case 0x00281200: r = new_game_slots_update(w); return true;
        case 0x00281628: r = save_slots_draw(w); return true;
        case 0x002809e0: r = save_slot_planet_draw(w); return true;
        case 0x00280b60: r = save_slots_focus_in(w); return true;
        case 0x00280ba0: r = save_slots_focus_out(w); return true;
        case 0x0027f690: r = question_focus_in(w); return true;
        case 0x0027f6d0: r = question_focus_out(w); return true;
        case 0x0027f700: r = question_update(w); return true;
        case 0x0027f798: r = question_draw(w); return true;
        case 0x0027d8b0: r = mission_picture_draw(w); return true;
        case 0x0027adb0: r = help_log_list_focus_in(w); return true;
        case 0x0027ae38: r = help_log_list_focus_out(w); return true;
        case 0x0027ae40: r = help_log_text_focus_in(w); return true;
        case 0x0027af10: r = help_log_text_focus_out(w); return true;
        case 0x0027af80: r = help_log_text_update(w); return true;
        case 0x0027b0f0: r = text_list_draw(w); return true;
        case 0x0027e290: r = two_pictures_update(w); return true;
        case 0x0027e3d8: r = two_pictures_draw(w); return true;
        case 0x0027a6e0: r = items_help_focus_in(w); return true;
        case 0x00277a58: r = icon_list_update(w); return true;
        case 0x00277be8: r = icon_list_draw(w); return true;
        case 0x0027f240: r = weapons_help_focus_in(w); return true;
        case 0x0027f308: r = gadgets_help_focus_in(w); return true;
        case 0x00277e28: r = cheats_list_focus_in(w); return true;
        case 0x0027f970: r = level_movies_focus_in(w); return true;
        case 0x0027f9b0: r = sketchbook_update(w); return true;
        case 0x0027fa98: r = epilogue_update(w); return true;
        case 0x0027fba0: r = epilogue_arrows_draw(w); return true;
        case 0x0027fd38: r = skill_points_text_draw(w); return true;
        default:
            if (end_menu_widget_call(ovl_code_to_veldin(fn), w, a1, r)) return true;  // (the menu at the end of the game: end_menu_tr.cpp)
            return galactic_map_widget_call(fn, w, a1, r);  // (the ship's galactic map)
    }
}

// A label's update: when focused, Start / Select / L1 close the menu (1, unless +0x124),
// triangle goes to the parent page (or -1, closing, on a root page without +0x124).
// (Eudora's, Rilgar's and the finale's other copies, the same instructions: this port, proved by batch 411.)
// orig: 0x002779c8 L4:0x0026acf8 L5:0x002a1cd0 L18:0x00279950
int32_t menu_label_update(uint32_t w) {
    if (focused() != w) return 0;
    if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) return 1;
    if ((lwu(PAD + 0x1c4) & 0x10) == 0) return 0;
    const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
    if (parent != 0) {
        sw(MS + 8, parent);
        return 0;
    }
    return lw(MS + 0x124) == 0 ? -1 : 0;
}

// A label's focus in: its highlight timer +0x44 restarted.
// orig: 0x00277f60
int32_t menu_label_focus(uint32_t w) {
    sw(w + 0x44, 0xffffffffu);
    return 0;
}

// orig: 0x00277f50
int32_t menu_nop_focus(uint32_t) { return 0; }
// orig: 0x00277f58
int32_t menu_nop_focus2(uint32_t) { return 0; }
// orig: 0x002799e0
int32_t menu_nop_update(uint32_t) { return 0; }

// A page's focus in: the menu's buffers laid out for the big images.
// orig: 0x0027a728
int32_t menu_buffers_full(uint32_t) {
    menu_vram_layout(1);
    return 0;
}

// A text label: which text it shows (+0x30 flags: 0x20 the level's, 0x40 the help planet's,
// 4 its own text +0x34, 0x80 / 0x100 / 0x1000 / else the focused widget's entry), faded in when
// that changes (+0x44 timer, +0x48 / +0x4c the item shown), scrolled up when it does not fit
// (+0x30 0x400, +0x3c the offset in 1/16 pixels), drawn with its shadow in the box.
// orig: 0x00277f70
int32_t menu_label_draw(uint32_t w) {
    uint32_t font = OV(0x001c3150);
    int32_t tex = 1;
    uint32_t s = OV(0x00160330);
    int32_t sub = 0;
    if ((lwu(w + 0x30) & 8) != 0) {
        tex = 3;
        font = OV(0x001c3890);
    }
    if ((lwu(w + 0x30) & 0x10) != 0) {
        tex = 2;
        font = OV(0x001c34f0);
    }
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x2004b);
    const uint32_t fl = lwu(w + 0x30);
    int32_t item;
    if ((fl & 0x20) != 0) {
        item = lw(0x0015ed84) - 1;
        if (!(static_cast<uint32_t>(item) < 0x12u)) item = -1;
    } else if ((fl & 0x40) != 0) {
        item = lw(OV(0x00184414)) - 1;
    } else if ((fl & 4) != 0) {
        if (lw(w + 0x44) < frame_scale_i(lw(OV(0x00160274)))) sw(w + 0x44, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00160274)))));
        item = 0;
        sw(w + 0x48, 0);
        sw(w + 0x4c, 0);
    } else if ((fl & 0x80) != 0) {
        item = lw(focused() + 0x40);
        if ((fl & 0x8000) != 0) sub = lw(0x0015ed80) != 0 ? 1 : 0;
    } else if ((fl & 0x100) != 0) {
        const uint32_t fw = focused();
        item = lw(fw + 0x3c);
        const uint32_t e = lwu(fw + 0x48) + static_cast<uint32_t>(item * 10);
        const uint32_t base = lh(e + 4) != 0 ? 0x0013d388u : 0x0013d4c0u;
        if (lbu(static_cast<uint32_t>(lh(e + 6)) + base) == 0) item = -1;
    } else if ((fl & 0x1000) != 0) {
        const uint32_t fw = focused();
        item = lw(fw + 0x40);
        const int32_t text = lh(lwu(fw + 0x34) + static_cast<uint32_t>(item * 12));
        sw(w + 0x34, 0xffff);
        sub_00204af8(text, 1, w + 0x34);
    } else {
        const uint32_t fw = focused();
        const int32_t v = lh(lwu(fw + 0x48) + static_cast<uint32_t>(lw(fw + 0x3c) * 10) + 6);
        item = v;
        sub = lbu(0x0013e520 + static_cast<uint32_t>(v)) != 0 ? 1 : 0;
    }
    bool same;
    if (lw(w + 0x44) == -1) {
        sw(w + 0x44, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00160274)))));
        sw(w + 0x48, static_cast<uint32_t>(item));
        sw(w + 0x4c, static_cast<uint32_t>(sub));
        same = item == lw(w + 0x48);
    } else {
        same = item == lw(w + 0x48);
    }
    if (same) {
        sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) + 3));
    } else {
        if (frame_scale_i(lw(OV(0x00160274))) < lw(w + 0x44)) sw(w + 0x44, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00160274)))));
        int32_t t = lw(w + 0x44);
        for (int k = 0; k < 3; ++k) t = t < 1 ? 0 : t - 1;
        sw(w + 0x44, static_cast<uint32_t>(t));
        if (t != 0) {
            item = lw(w + 0x48);
            sub = lw(w + 0x4c);
        } else {
            sw(w + 0x48, static_cast<uint32_t>(item));
            sw(w + 0x4c, static_cast<uint32_t>(sub));
            sw(w + 0x30, lwu(w + 0x30) & ~0x400u);
            sw(w + 0x3c, 0);
        }
    }
    const uint32_t f2 = lwu(w + 0x30);
    auto entry_text = [w, item, sub]() {
        const uint32_t a = lwu(w + 0x34);
        return lwu(((static_cast<uint32_t>(lw(w + 0x38) * item)) & ~3u) + static_cast<uint32_t>(sub) * 4 + a);
    };
    if ((f2 & 4) != 0) {
        if (lwu(w + 0x34) == 0) return 1;
        s = sub_00203ad8(lw(w + 0x34));
    } else if ((f2 & 0x1000) != 0) {
        if (lwu(w + 0x34) == 0xffff) return 1;
        s = sub_00203ad8(lw(w + 0x34));
    } else if ((f2 & 0x100) != 0 && item == -1) {
        s = OV(0x00160338);
    } else if (lwu(w + 0x34) != 0) {
        s = sub_00203ad8(static_cast<int32_t>(entry_text()));
    }
    if ((lwu(w + 0x30) & 0x11e4) == 0 && lbu(0x0013d4c0 + static_cast<uint32_t>(item)) == 0) s = OV(0x00160338);
    const uint32_t buf = ee_stack_alloc(0x80);  // the frame: the formatted text +0, the box +0x40, its template +0x60
    if ((lwu(w + 0x30) & 0x200) != 0) {
        const uint32_t t = lwu(lwu(w + 0x34) + ((static_cast<uint32_t>(lw(w + 0x38) * item)) & ~3u));
        if (t != 0x4ed2 && t != 0x4ed9 && t != 0x4edd && !level_slots_map_row()) {  // (enhancement: a level mod's row: its name alone)
            std::vector<uint64_t> args(1, sub_00203ad8(0x4ecc));
            args.push_back(s);
            libc_sprintf(buf, OV(0x00160340), args);
            s = buf;
        }
    }
    uint32_t f = lwu(w + 0x30);
    int32_t cx = 4, cy = 4;
    const uint32_t f0 = f;
    if ((f0 & 0x4004) == 0x4004 && lwu(w + 0x34) == TXT(0x523e)) {
        f |= 1;
        cy = 0xc;
    }
    if ((f0 & 0x800) != 0 && lbu(0x0013d408 + static_cast<uint32_t>(item)) == 0) {
        f |= 3;
        s = sub_00203ad8(0x4f54);
    }
    if (s == 0) s = OV(0x00160348);
    int32_t align = 8;
    if ((f & 1) != 0) {
        align = 9;
        cx = lw(w + 0x20) / 2;
    }
    if ((f & 2) != 0) {
        align |= 2;
        cy = lw(w + 0x24) / 2;
    }
    texture_block_begin(0);
    const uint64_t t0 = effect_tex0(tex);
    const uint32_t tpl = buf + 0x60, box = buf + 0x40;
    libc_memset(tpl, 0, 0x18);
    const uint32_t top = lhu(OV(0x00160318));
    sh(tpl + 2, lhu(w + 0x24) - top);
    sh(tpl + 4, 1);
    sh(tpl + 6, lhu(w + 0x20) - 4u);
    sh(tpl + 8, static_cast<uint32_t>(cx));
    sh(tpl + 0xa, static_cast<uint32_t>(cy - (lw(w + 0x3c) >> 4)));
    sh(tpl + 0x10, lhu(OV(0x00160328)));
    sh(tpl + 0x12, static_cast<uint32_t>(align));
    sh(tpl + 0x16, 0u - (lhu(w + 0x3c) & 0xfu));
    sh(tpl, top);
    for (uint32_t o = 0; o < 0x18; o += 2) sh(box + o, lhu(tpl + o));
    if ((lwu(w + 0x30) & 0x10000) != 0) sh(box + 2, lhu(w + 0x24) - 1u);
    const uint32_t c = rgba_lerp(lwu(OV(0x00160270)), 0x80ffa888u, 0.5f);
    const uint32_t colour = menu_highlight_colour(lw(w + 0x44), static_cast<int32_t>(c), static_cast<int32_t>(0x80ffa888u));
    sh(box + 0x12, lhu(box + 0x12) | 4u);
    text_box(box, colour, s, -1, t0, font);
    sh(box + 0x12, lhu(box + 0x12) ^ 4u);
    const uint32_t f3 = lwu(w + 0x30);
    if ((f3 & 0x2000) == 0 && !(lh(box + 0xe) + 4 < lh(box + 2) - lh(box))) {
        if ((f3 & 0x400) == 0) {
            sw(w + 0x30, f3 | 0x400);
            sw(w + 0x3c, static_cast<uint32_t>(-(lw(w + 0x24) << 3)));
        }
    } else if ((lwu(w + 0x30) & 0x400) != 0) {
        const uint32_t f4 = lwu(w + 0x30);
        sw(w + 0x3c, 0);
        sw(w + 0x30, f4 ^ 0x400);
    }
    // the shadow (offset by 0x16027c / 0x160278), then the text
    auto pass = [&](bool scrolled) {
        const uint32_t dy = lhu(OV(0x0016027c)), dx = lhu(OV(0x00160278));
        if (!scrolled) {
            sh(box + 0xa, static_cast<uint32_t>(cy - (lw(w + 0x3c) >> 4)) + dy);
        } else {
            sh(box + 0xa, lhu(box + 0xa) + lhu(box + 0xe) + static_cast<uint32_t>(lw(OV(0x00160328)) * 3) + dy);
        }
        box_add(box, dy);
        box_add(box + 2, dy);
        box_add(box + 4, dx);
        box_add(box + 6, dx);
        box_add(box + 8, dx);
        text_colour_codes_off();
        text_box(box, 0x80000000ull, s, -1, t0, font);
        sub_001fb028();
        const uint32_t ey = lhu(OV(0x0016027c)), ex = lhu(OV(0x00160278));
        box_add(box, 0u - ey);
        box_add(box + 6, 0u - ex);
        box_add(box + 0xa, 0u - ey);
        box_add(box + 2, 0u - ey);
        box_add(box + 4, 0u - ex);
        box_add(box + 8, 0u - ex);
        text_box(box, colour, s, -1, t0, font);
    };
    pass(false);
    if ((lwu(w + 0x30) & 0x400) != 0) {
        pass(true);
        if ((lwu(w + 0x30) & 0x400) != 0) {
            const int32_t h = lh(box + 0xe) + lw(OV(0x00160328)) * 3;
            const int32_t v = (lw(0x0013cae0) & 1) != 0 ? lw(w + 0x3c) + 10 : lw(w + 0x3c) + 3;
            sw(w + 0x3c, static_cast<uint32_t>(v));
            sw(w + 0x3c, static_cast<uint32_t>(ee_rem(v, h << 4)));
        }
    }
    ee_stack_free(0x80);
    texture_block_end();
    return 2;
}

// --- the menu's buffers ------------------------------------------------------------------------

// A free buffer of the kind asked (`big` nonzero: flag 1), taken (flag 2) and filled with
// 0xdeadbeef; 0 when none.
// orig: 0x00283a60
uint32_t menu_buf_alloc(int32_t big) {
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t e = BUFS + i * 8;
        const uint32_t k = big != 0 ? lwu(e + 4) ^ 1u : lwu(e + 4);
        if ((k & 1) != 0) continue;
        if (lwu(e) == 0) continue;
        const uint32_t f = lwu(e + 4);
        if ((f & 2) != 0) continue;
        sw(e + 4, f | 2);
        mem_fill32(lwu(e), 0xdeadbeefu, menu_buf_size(lwu(e)));
        return lwu(e);
    }
    return 0;
}

// A buffer given back: a disc read into it (flag 4) stopped first (0x265f00 when one is
// going, M+0xcb).
// orig: 0x00283b20
int32_t menu_buf_free(uint32_t buf) {
    advanced_picture_take(buf);  // enhancement: a picture of ours there is gone
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t e = BUFS + i * 8;
        if (lwu(e) != buf) continue;
        uint32_t f = lwu(e + 4);
        if ((f & 2) == 0) continue;
        if ((f & 4) != 0) {
            sw(e + 4, f ^ 4);
            if (lbu(MS + 0xcb) != 0) {
                disc_read_break();
                sb(MS + 0xcb, 0);
            }
            f = lwu(e + 4);
        }
        sw(e + 4, f & ~2u);
        return 0;
    }
    return 0;
}

// A buffer's size (0x4f000 big, 0x11800), -1 for an unknown address.
// orig: 0x00283bd0
int32_t menu_buf_size(uint32_t buf) {
    for (uint32_t i = 0; i < 5; ++i) {
        if (lwu(BUFS + i * 8) == buf) return (lwu(BUFS + i * 8 + 4) & 1) != 0 ? 0x4f000 : 0x11800;
    }
    return -1;
}

// A buffer marked as being read into (flag 4); 1 for an unknown address.
// orig: 0x00283c20
int32_t menu_buf_busy(uint32_t buf) {
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t e = BUFS + i * 8;
        if (lwu(e) == buf) {
            sw(e + 4, lwu(e + 4) | 4);
            return 0;
        }
    }
    return 1;
}

// A buffer's read done (flag 4 cleared); 1 for an unknown address.
// orig: 0x00283c68
int32_t menu_buf_ready(uint32_t buf) {
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t e = BUFS + i * 8;
        if (lwu(e) == buf) {
            sw(e + 4, lwu(e + 4) & ~4u);
            return 0;
        }
    }
    return 1;
}

// The stream system's disc read stopped (when one is going: 0x1516d8), its "broken" flag set.
// orig: 0x00265f00
void disc_read_break() {
    if (lh(0x001516d8) == 0) return;
    snd_cd_break();
    sb(0x001516da, 1);
}

// An image (+8 / +0xc width / height, +0x20 its 8-bit pixels, +0x420 its palette) queued for
// upload (0x16cd80, count 0x15f418, at most 0x40) at the GS texture cursor 0x15ee74 (the
// palette first, 0x400 words); its TEX0 (PSMT8, CSM1, the palette's CBP).
// orig: 0x00245a20
uint64_t image_upload(uint32_t img) {
    const int32_t lw_ = highest_bit_index(lw(img + 8));
    const int32_t lh_ = highest_bit_index(lw(img + 0xc));
    int32_t cur = lw(0x0015ee74);
    int32_t tbw = lw_ - 6;
    if (!(-1 < tbw)) tbw = 0;
    const int32_t pal = cur >> 8;
    cur += 0x400;
    const int64_t bw = static_cast<int32_t>(1u << tbw);
    const int32_t tbp = cur >> 8;
    uint64_t t = static_cast<uint64_t>(static_cast<int64_t>(tbp)) | (static_cast<uint64_t>(bw) << 14);
    t |= (static_cast<uint64_t>(static_cast<int64_t>(lw_)) << 26) | 0x1300000ull;
    t |= static_cast<uint64_t>(static_cast<int64_t>(lh_)) << 30;
    t |= (static_cast<uint64_t>(static_cast<int64_t>(pal)) << 37) | 0x400000000ull;
    t |= 0x8000000000000000ull;
    cur += static_cast<int32_t>(1u << ((lw_ + lh_) & 31));
    sw(0x0015ee74, static_cast<uint32_t>(cur));
    const int32_t n = lw(OV(0x0015f418));
    if (n < 0x40) {
        const uint32_t r = OV(0x0016cd80) + static_cast<uint32_t>(n) * 16;
        sw(r, img + 0x20);
        sh(r + 6, static_cast<uint32_t>(pal));
        sh(r + 4, 0);
        sb(r + 0xc, static_cast<uint32_t>(lw_));
        sb(r + 0xd, static_cast<uint32_t>(lh_));
        sw(r + 8, img + 0x420);
        sh(r + 0xe, static_cast<uint32_t>(tbp));
        sw(OV(0x0015f418), static_cast<uint32_t>(n + 1));
    }
    return t;
}

// --- the picture panel -------------------------------------------------------------------------
// +0x30 its images (sector, count), +0x34 flags (what picks the image: 1 +0x58, 2 the help
// planet, 4 / 8 / 0x100 / 0x1000 / else the focused widget's; 0x10 / 0x20 the read and its
// unpacking, 0x2000 the cheats), +0x38 / +0x3c the image's size, +0x44 the state (0 read into
// buffer +0x48, 2 into +0x4c, 1 / 3 / 5 unpack, 4 ready; -1 nothing), +0x50 / +0x54 the image in
// each buffer, +0x5c its frames, +0x60 the packed data's offset in the buffer.

// orig: (helper) repeated in 0x0027da10: the image `img`'s read into `buf` started (packed at
// the buffer's end with flag 0x20); its byte count, 0 when the disc is busy.
static int32_t picture_read(uint32_t w, uint32_t buf, int32_t img) {
    if (level_slots_picture_read(img, buf)) {  // enhancement: a level mod's level's picture, made in the buffer (src/host/level_slots.h)
        sw(w + 0x60, 0);
        return 1;
    }
    const uint32_t ent = static_cast<uint32_t>(img) * 8 + lwu(w + 0x30);
    if (advanced_picture(lwu(ent), buf)) {  // enhancement: one of our pictures, made in the buffer
        sw(w + 0x60, 0);
        return 1;
    }
    uint32_t dst = buf;
    if ((lwu(w + 0x34) & 0x20) != 0) {
        const int32_t off = menu_buf_size(buf) - static_cast<int32_t>(lwu(ent + 4) << 11);
        sw(w + 0x60, static_cast<uint32_t>(off));
        dst += static_cast<uint32_t>(off);
    }
    return stream_disc_read(dst, lwu(ent), lwu(ent + 4));
}

// orig: 0x0027da10
int32_t menu_picture_update(uint32_t w) {
    sw(w + 0x5c, lwu(w + 0x5c) + 1);
    const uint32_t fl = lwu(w + 0x34);
    int32_t img;
    if ((fl & 1) != 0) {
        img = lw(w + 0x58);
        if (img == -1) return 0;
    } else if ((fl & 2) != 0) {
        img = lw(OV(0x00184414));
    } else if ((fl & 4) != 0) {
        img = lw(focused() + 0x3c);
    } else if ((fl & 0x100) != 0) {
        constexpr uint32_t G = 0x0013d290;
        int32_t k = lw(focused() + 0x40);
        if (!(-1 < k)) k = 0;
        if (!(k < 5)) k = 4;
        img = k;
        if (lw(G + 0xd4) < 3 && !(lw(G + 0xdc) >= 0)) {
            if (lw(w + 0x44) == -1) sw(w + 0x44, 0);
            img = lw(G + static_cast<uint32_t>(k) * 0x1c + 0x20);
            level_slots_save_picture(k, img);  // enhancement: a save made on a level mod's level: its picture (src/host/level_slots.h)
            if (img == -1) sw(w + 0x44, 0xffffffffu);
            if (lw(G + 8) != 2) sw(w + 0x44, 0xffffffffu);
        } else {
            sw(w + 0x44, 0xffffffffu);
        }
    } else if ((fl & 8) != 0) {
        const uint32_t fw = focused();
        img = lh(lwu(fw + 0x48) + static_cast<uint32_t>(lw(fw + 0x3c) * 10) + 8);
        if (lw(fw + 0x40) == 0) sw(w + 0x44, 0xffffffffu);
    } else if ((fl & 0x400) != 0) {
        const int32_t v = frame_scale_i(0x12c);
        img = ee_rem(ee_div(lw(w + 0x5c), v), 0x13);
    } else if ((fl & 0x1000) != 0) {
        // (the original copies the 6-word table 0x1e9270 to its stack and indexes the copy)
        img = lw(lwu(w + 0x40)) + lw(OV(0x001e9270) + static_cast<uint32_t>(lw(0x0015ed88)) * 4);
    } else {
        const uint32_t fw = focused();
        img = lw(fw + 0x40);
        if (!(-1 < img)) img = 0;
        if ((fl & 0x4000) != 0 && lh(lwu(fw + 0x34) + static_cast<uint32_t>(img * 12) + 2) == 2) img = 9;
    }
    const uint32_t f2 = lwu(w + 0x34);
    if ((f2 & 0x2000) != 0 && lbu(0x0013d408 + static_cast<uint32_t>(img)) == 0) img = 0x1e;
    const uint32_t st = lwu(w + 0x44);
    if (!(st < 7u)) return 0;
    switch (st) {
        case 0: {
            const uint32_t buf = lwu(w + 0x48);
            if (buf == 0) return 0;
            if (lh(0x001516d8) != 0) return 0;
            if (!level_slots_picture_id(img) && lwu(static_cast<uint32_t>(img) * 8 + lwu(w + 0x30) + 4) == 0) return 0;  // (enhancement: ours have no entry)
            if (picture_read(w, buf, img) == 0) {
                sw(w + 0x44, 0xffffffffu);
                return 0;
            }
            menu_buf_busy(lwu(w + 0x48));
            const int32_t s = lw(w + 0x44);
            sw(w + 0x50, static_cast<uint32_t>(img));
            sw(w + 0x44, static_cast<uint32_t>(s + 1));
            return 0;
        }
        case 1:
        case 3:
        case 5: {
            if (lh(0x001516d8) != 0) return 0;
            if ((f2 & 0x20) != 0) {
                const uint32_t pb = w + 0x48 + (lw(w + 0x44) == 3 ? 4u : 0u);
                menu_buf_ready(lwu(pb));
                const bool level_pic = level_slots_picture_take(lwu(pb));  // enhancement: a level mod's picture is not packed either
                if (!advanced_picture_take(lwu(pb)) && !level_pic)  // enhancement: our pictures are not packed
                    wad_decompress(lwu(pb) + lwu(w + 0x60), lwu(pb));
                sw(w + 0x60, 0);
            }
            sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) + 1));
            return 0;
        }
        case 6:
        case 2: {
            if (st == 6) sw(w + 0x44, 2);
            if (img == lw(w + 0x50)) return 0;
            if (img == lw(w + 0x54)) {
                sw(w + 0x44, 4);
                return 0;
            }
            const uint32_t buf = lwu(w + 0x4c);
            if (buf == 0) {
                sw(w + 0x44, 0);
                return 0;
            }
            if (lh(0x001516d8) != 0) return 0;
            if (!level_slots_picture_id(img) && lwu(static_cast<uint32_t>(img) * 8 + lwu(w + 0x30) + 4) == 0) return 0;  // (enhancement: ours have no entry)
            if (picture_read(w, buf, img) == 0) {
                sw(w + 0x44, 0xffffffffu);
                return 0;
            }
            menu_buf_busy(lwu(w + 0x4c));
            const int32_t s = lw(w + 0x44);
            sw(w + 0x54, static_cast<uint32_t>(img));
            sw(w + 0x44, static_cast<uint32_t>(s + 1));
            return 0;
        }
        case 4: {
            if (img == lw(w + 0x54)) return 0;
            if (img == lw(w + 0x50)) {
                sw(w + 0x44, 2);
                return 0;
            }
            const uint32_t buf = lwu(w + 0x48);
            if (buf == 0) {
                sw(w + 0x44, 0xffffffffu);
                return 0;
            }
            if (lh(0x001516d8) != 0) return 0;
            if (!level_slots_picture_id(img) && lwu(static_cast<uint32_t>(img) * 8 + lwu(w + 0x30) + 4) == 0) return 0;  // (enhancement: ours have no entry)
            if (picture_read(w, buf, img) == 0) {
                sw(w + 0x44, 0xffffffffu);
                return 0;
            }
            menu_buf_busy(lwu(w + 0x48));
            const int32_t s = lw(w + 0x44);
            sw(w + 0x50, static_cast<uint32_t>(img));
            sw(w + 0x44, static_cast<uint32_t>(s + 1));
            return 0;
        }
        default: return 0;
    }
}

// The picture panel: the image of the buffer read last (state 2..3 +0x48, from 4 +0x4c) over
// the whole box; before one is ready, with flag 0x100, a "loading" text (0x4fb9 or M+0x12c).
// Nothing (1) for an item not owned (flag 4).
// orig: 0x0027df90
int32_t menu_picture_draw(uint32_t w) {
    const int32_t st = lw(w + 0x44);
    if (st < 2) {
        if ((lwu(w + 0x34) & 0x100) == 0) return 1;
        constexpr uint32_t G = 0x0013d290;
        if (lw(G + 8) != 2) return 2;
        if (lw(G + 0xd4) < 3 && lw(G + 0xdc) < 0) return 2;
        texture_block_begin(0);
        const int32_t id = lw(MS + 0x128) != 0 ? lw(MS + 0x12c) : 0x4fb9;
        const uint32_t s = sub_00203ad8(id);
        const uint32_t buf = ee_stack_alloc(0x40);  // the box +0x10, its template +0x30
        const uint32_t tpl = buf + 0x30, box = buf + 0x10;
        libc_memset(tpl, 0, 0x18);
        sh(tpl + 2, lhu(w + 0x24) + 1u);
        sh(tpl + 4, 1);
        sh(tpl + 6, lhu(w + 0x20) + 1u);
        sh(tpl + 8, static_cast<uint32_t>(lw(w + 0x20) >> 1));
        sh(tpl + 0x10, 0x10);
        sh(tpl + 0x12, 5);
        sh(tpl, 1);
        sh(tpl + 0xa, 5);
        for (uint32_t o = 0; o < 0x18; o += 2) sh(box + o, lhu(tpl + o));
        text_box_draw(box, 0x80000000ull, s, -1);
        sh(box + 0xa, static_cast<uint32_t>((lw(w + 0x24) - lh(box + 0xe)) >> 1));
        sh(box + 0x12, lhu(box + 0x12) ^ 4u);
        text_box_draw(box, 0x80000000ull, s, -1);
        for (uint32_t o = 0; o <= 0xa; o += 2) sh(box + o, lhu(box + o) - 1u);
        text_box_draw(box, 0x80ffa888ull, s, -1);
        ee_stack_free(0x40);
        texture_block_end();
        return 2;
    }
    if ((lwu(w + 0x34) & 4) != 0) {
        const uint32_t which = st < 4 ? 0u : 1u;
        const int32_t img = lw(w + 0x50 + which * 4);
        const uint32_t e = lwu(focused() + 0x48) + static_cast<uint32_t>(img * 10);
        if (lh(e + 4) == 0 && lbu(static_cast<uint32_t>(lh(e + 6)) + 0x0013d4c0) == 0) return 1;
        if (lh(e + 4) == 1 && lbu(static_cast<uint32_t>(lh(e + 6)) + 0x0013d388) == 0) return 1;
    }
    texture_block_begin(0);
    const uint32_t img = lw(w + 0x44) < 4 ? lwu(w + 0x48) : lwu(w + 0x4c);
    const uint64_t t = image_upload(img);
    glyph_sprite(0, 0, lh(0x00151780 + 0x160), lh(0x00151780 + 0x162), 0, 0, lw(w + 0x38), lw(w + 0x3c), 0x80808080ull, t);
    texture_block_end();
    return 0x10;
}

// The picture panel focused in: its two buffers (big ones with flag 0x200, else small, or big
// when none is free), nothing read yet.
// orig: 0x0027d928
int32_t menu_picture_focus_in(uint32_t w) {
    sw(w + 0x44, 0);
    sw(w + 0x48, menu_buf_alloc(static_cast<int32_t>(lwu(w + 0x34) & 0x200)));
    sw(w + 0x4c, menu_buf_alloc(static_cast<int32_t>(lwu(w + 0x34) & 0x200)));
    if ((lwu(w + 0x34) & 0x200) == 0) {
        if (lwu(w + 0x48) == 0) sw(w + 0x48, menu_buf_alloc(1));
        if (lwu(w + 0x4c) == 0) sw(w + 0x4c, menu_buf_alloc(1));
    }
    sw(w + 0x5c, 0);
    sw(w + 0x54, 0xffffffffu);
    sw(w + 0x50, 0xffffffffu);
    return 0;
}

// The picture panel focused out: its buffers given back.
// orig: 0x0027d9c0
int32_t menu_picture_focus_out(uint32_t w) {
    sw(w + 0x48, static_cast<uint32_t>(menu_buf_free(lwu(w + 0x48))));
    sw(w + 0x4c, static_cast<uint32_t>(menu_buf_free(lwu(w + 0x4c))));
    sw(w + 0x44, 0xffffffffu);
    sw(w + 0x50, 0xffffffffu);
    sw(w + 0x54, 0xffffffffu);
    return 0;
}

}  // namespace rac1
