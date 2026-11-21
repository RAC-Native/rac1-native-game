// Ported overlay code: the widget functions of the pause menu's Goodies page (state 9, 0x1b6820: the entry 0x4fc2, shown only
// once the game has been finished, menu_open's MS+0xd8) and its pages: Skill Points 0x24 (0x1b6d40), Cheats 0x26 (0x1b7b30),
// In-Level Movies 0x28 (0x1b80e0), Sketchbook 0x29 (0x1b7658, 15 skill points) and Epilogue 0x2a (0x1b74b8, 30 skill points;
// cheats_menu_entries). See pause_menu_pages.h.
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/region_core.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};   // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1
constexpr uint64_t TEXT_COLOUR = 0x80ffa888ull;

// orig: (helper) the start of 0x27f9b0 / 0x27fa98, as menu_label_update's: Start / Select / L1 close the menu (1, unless
// MS+0x124), triangle goes to the parent page (or -1 on a root page without +0x124). True when it returns `r`.
bool special_back(int32_t& r) {
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
}  // namespace

// The Cheats page's switch list (widget 0x1b7d18) focus in: its entries at 0x1b7c10 (0x14 bytes: text, the switch's byte
// 0x15edb0 + n, the texts 0x4f5a / 0x4f5b of its two values, 0), one for each cheat whose unlock byte 0x15edc0 + n is set, in
// the order of the table 0x1e91c8 (cheat numbers, -1 none) with the texts of 0x1e91e8, ended by a 0 text. The original copies
// the two tables (8 and 7 words) into its 0x40-byte frame and walks 12 rows of it: rows 8..11 read the text table as cheat
// numbers (bytes 0x15edc0 + 0x501a.., zero in every level's data) and their texts above the frame; kept as is, the frame in
// the EE's memory. a0 (the widget) is not read.
// orig: 0x00277e28
int32_t cheats_list_focus_in(uint32_t) {
    const uint32_t f = ee_stack_alloc(0x40);
    for (uint32_t k = 0; k < 8; ++k) sw(f + 4 * k, lwu(OV(0x001e91c8) + 4 * k));
    for (uint32_t k = 0; k < 7; ++k) sw(f + 0x20 + 4 * k, lwu(OV(0x001e91e8) + 4 * k));
    const uint32_t list = OV(0x001b7c10);
    uint32_t e = list;
    int32_t n = 0;
    // region: the 1.02 build (the PAL disc 2.00) ends the walk at the cheat table's -1 (its row 7), within the 12 rows
    // (2.00 0x00278dcc..0x00278e54): its rows 8..11 would read the raw bytes 0x15edc0 + 0x501a.., which on that disc's
    // layout hold another object of the level's data than on the North American one
    const bool b102 = region_core_moved();
    for (uint32_t k = 0; k < 12; ++k) {
        const int32_t id = lw(f + 4 * k);
        if (id == -1 && b102) break;
        if (id == -1) continue;
        if (lbu(0x0015edc0 + static_cast<uint32_t>(id)) == 0) continue;
        sw(e + 4, 0x0015edb0 + static_cast<uint32_t>(id));
        sw(e, lwu(f + 0x20 + 4 * k));
        sw(e + 8, 0x4f5a);
        sw(e + 0xc, 0x4f5b);
        sw(e + 0x10, 0);
        e += 0x14;
        ++n;
    }
    sw(list + static_cast<uint32_t>(n) * 0x14, 0);
    ee_stack_free(0x40);
    return 0;
}

// The In-Level Movies page's list (0x28, widget 0x1b8678) focus in: the movies of the current planet, 0x1b8628[level % 19].
// orig: 0x0027f970
int32_t level_movies_focus_in(uint32_t w) {
    const uint32_t level = lwu(0x0015ed84);
    sw(w + 0x34, lwu(OV(0x001b8628) + (level % 0x13u) * 4));
    return 0;
}

// The Sketchbook's pages (0x29, widget 0x1b77d0): cross / circle (pad bits 0x40 / 0x20 of the presses) step its picture +0x40
// round 30, with the menu's sound 0x11 when it changed.
// orig: 0x0027f9b0
int32_t sketchbook_update(uint32_t w) {
    int32_t r = 0;
    if (special_back(r)) return r;
    const uint32_t pressed = lwu(PAD + 0x1a4);
    const int32_t old = lw(w + 0x40);
    if ((pressed & 0x40) != 0) {
        sw(w + 0x40, static_cast<uint32_t>((old + 1) % 0x1e));
    } else if ((pressed & 0x20) != 0) {
        sw(w + 0x40, static_cast<uint32_t>((old + 0x1d) % 0x1e));
    }
    if (lw(w + 0x40) != old) moby_sound_start(1, 0x11, lwu(w + 0x14));
    return 0;
}

// The Epilogue's pages (0x2a, widget 0x1b7598): the presses 0x2040 / 0x8020 (cross or right / circle or left) step its page
// +0x54 round 12, with the menu's
// sound 0x11.
// orig: 0x0027fa98
int32_t epilogue_update(uint32_t w) {
    int32_t r = 0;
    if (special_back(r)) return r;
    const uint32_t pressed = lwu(PAD + 0x1a4);
    if ((pressed & 0x2040) != 0) {
        const uint32_t m = lwu(w + 0x14);
        sw(w + 0x54, static_cast<uint32_t>((lw(w + 0x54) + 1) % 0xc));
        moby_sound_start(1, 0x11, m);
        return 0;
    }
    if ((pressed & 0x8020) != 0) {
        const uint32_t m = lwu(w + 0x14);
        sw(w + 0x54, static_cast<uint32_t>((lw(w + 0x54) + 0xb) % 0xc));
        moby_sound_start(1, 0x11, m);
    }
    return 0;
}

// The Epilogue's arrows (widgets 0x1b7540 and 0x1b7598): with +0x38 (a widget above) the glyph string 0x1603b0 at x 4 and the HUD
// frame 0xe99e (6) drawn at x 640, else the string 0x1603b8 at the box's width - 0x18 and the frame turned by pi at x 192; y
// the box's height / 2 - 8 (text) and height x 8 (sprite). Result 2.
// orig: 0x0027fba0
int32_t epilogue_arrows_draw(uint32_t w) {
    const uint32_t s = ee_stack_alloc(0x10);  // (the two bytes of the string copied into the frame)
    texture_block_begin(0);
    if (lw(w + 0x38) != 0) {
        const int32_t y = lw(w + 0x24) / 2 - 8;
        sb(s, lbu(OV(0x001603b0)));
        sb(s + 1, lbu(OV(0x001603b0) + 1));
        text_small(4, y, TEXT_COLOUR, s, -1);
        const float fy = static_cast<float>(lw(w + 0x24) << 3);
        const uint64_t t = hud_tex0(hud_frame(0xe99e, 6));
        sprite_rotated(0x20, 0x10, t, 640.0f, fy, 128.0f, 256.0f, 0.0f);
    } else {
        const int32_t y = lw(w + 0x24) / 2 - 8;
        const int32_t x = lw(w + 0x20) - 0x18;
        sb(s, lbu(OV(0x001603b8)));
        sb(s + 1, lbu(OV(0x001603b8) + 1));
        text_small(x, y, TEXT_COLOUR, s, -1);
        const float fy = static_cast<float>(lw(w + 0x24) << 3);
        const uint64_t t = hud_tex0(hud_frame(0xe99e, 6));
        sprite_rotated(0x20, 0x10, t, 192.0f, fy, 128.0f, 256.0f, u2f(0x40490fdbu));
    }
    texture_block_end();
    ee_stack_free(0x10);
    return 2;
}

// The Skill Points page's text panel (0x24, widget 0x1b6e20): the selected entry of the focused list; its argument (+4) -1: the text 0x5019
// centred at the box's middle, else the two texts of its row of 0x1c1e40 (0xc bytes) at a third and two thirds of the
// box's height. Result 2.
// orig: 0x0027fd38
int32_t skill_points_text_draw(uint32_t w) {
    const uint32_t fw = lwu(lwu(MS + 4) + 0x40);
    const uint32_t e = static_cast<uint32_t>(lw(fw + 0x40)) * 0xc + lwu(fw + 0x34);
    const int32_t row = lw(e + 4);
    texture_block_begin(0);
    if (row == -1) {
        const int32_t y = lw(w + 0x24) / 2 - 8;
        const int32_t x = lw(w + 0x20) / 2;
        text_centered_small(x, y, TEXT_COLOUR, sub_00203ad8(0x5019), -1);
    } else {
        const uint32_t r = OV(0x001c1e40) + static_cast<uint32_t>(row) * 0xc;
        const int32_t x = lw(w + 0x20) / 2;
        const int32_t y = lw(w + 0x24) / 3 - 8;
        text_centered_small(x, y, TEXT_COLOUR, sub_00203ad8(lw(r)), -1);
        const int32_t y2 = (lw(w + 0x24) << 1) / 3 - 8;
        const int32_t x2 = lw(w + 0x20) / 2;
        text_centered_small(x2, y2, TEXT_COLOUR, sub_00203ad8(lw(r + 4)), -1);
    }
    texture_block_end();
    return 2;
}

}  // namespace rac1
