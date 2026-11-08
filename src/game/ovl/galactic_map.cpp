// Ported overlay code: the galactic map, the ship's planet menu. The take-off
// (game mode 6, step 0) ends with the menu opened on it (0x276190(0), state 0xe -> page 0x1b6088, state 0xf); a nonzero
// return of a widget update on its pages (states 0xf / 0x10) closes the menu with exit kind 2: the planet travel 0x28ed58 to
// the planet 0x184414 (the menu's help planet).
//   page 0x1b6088 (0xf): the title (0x2779c8 / 0x277f70, focus-in 0x27f440), the planet list (0x278840 / 0x27b0f0, focus-in
//     0x27f448: the unlocked planets), the galaxy view (0x27f550 / 0x27f578 -> 0x2033c0), the help line, the planet's
//     picture (0x27d5d8 ..), the planet's map (0x279aa8 / 0x2799e8);
//   page 0x1b63f8 (0x10): two labels, the planet's info (0x27a108 / 0x27d2d0 / 0x27a068), its map, its model (0x27cbc0),
//     the command line (0x27f5b0 / 0x278e10).
#include <game/ovl/galactic_map.h>
#include <host/level_slots.h>
#include <game/ovl/galactic_map_tr.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};  // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1 (+0x1c4: the buttons pressed this frame)
constexpr uint32_t UNLOCKED = 0x0013d510;  // the planets the ship can fly to (words, 0 ends; base ELF data)
}  // namespace

// The galaxy page's title focus-in: nothing.
// orig: 0x0027f440
int32_t galmap_nop_focus(uint32_t) { return 0; }

// The planet list's focus-in: its entries (0x1ba3b8, 0xc bytes: text id, action 1, 0, the help text id) built from the
// unlocked planets 0x13d510 (at most 0x14) and the planets' texts 0x1c1e40 (0xc per planet: +0 the name, +4 the help
// line), ended by a 0 text; the selection +0x40 the help planet's place in the list *(0x1602a0) (0 when absent), the
// widget's flags +0x30 | 0x8000.
// orig: 0x0027f448
int32_t galmap_list_focus_in(uint32_t w) {
    const uint32_t list = OV(0x001ba3b8);
    const uint32_t texts = OV(0x001c1e40);
    uint32_t n = 0;
    if (lw(UNLOCKED) != 0) {
        uint32_t p = UNLOCKED;
        uint32_t e = list;
        uint32_t planet = lwu(p);
        n = 1;
        for (;;) {
            sh(e + 2, 1);
            p += 4;
            sw(e + 4, 0);
            const bool more = static_cast<int32_t>(n) < 0x14;
            sh(e + 8, lhu(planet * 0xc + texts + 4));
            sh(e, lhu(planet * 0xc + texts));
            e += 0xc;
            if (!more) break;
            planet = lwu(p);
            if (planet == 0) break;
            ++n;
        }
    }
    n = level_slots_list_rows(list, n);  // enhancement: a level mod's row (src/host/level_slots.h; none without one)
    sh(list + n * 0xc, 0);
    const uint32_t order = OV(0x00166c00u - 0x6960);
    sw(w + 0x40, 0);
    sw(w + 0x30, lwu(w + 0x30) | 0x8000u);
    uint32_t planet = lwu(lwu(order));
    if (planet == 0) return 0;
    uint32_t i = 1;
    if (lwu(OV(0x00184414)) == planet) {
        sw(w + 0x40, 0);
        return 0;
    }
    for (;;) {
        planet = lwu(lwu(order) + (i << 2));
        if (planet == 0) return 0;
        if (lwu(OV(0x00184414)) == planet) break;
        ++i;
    }
    sw(w + 0x40, i);
    return 0;
}

// The galaxy view's update: cross goes to the planet page 0x1b63f8.
// orig: 0x0027f550
int32_t galmap_view_update(uint32_t) {
    if ((lwu(PAD + 0x1c4) & 0x40) != 0) sw(MS + 8, OV(0x001b63f8));
    return 0;
}

// The galaxy view's draw: 0x2033c0 in the widget's box (x0, x0 + w, y0, y0 + h).
// orig: 0x0027f578
int32_t galmap_view_draw(uint32_t w) {
    const uint32_t x = lwu(w + 0x18), y = lwu(w + 0x1c);
    galaxy_view_draw_u(x, x + lwu(w + 0x20), y, y + lwu(w + 0x24), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    return 2;
}

// The planet page's command line: triangle back to the parent page (or -1, closing, on a root page without +0x124);
// Start the current planet as the destination (1: the menu closes, the landing again); cross a sound and 1 (the menu
// closes: the travel to the help planet); circle the planet's movie (exit kind 3 with the movie 0x184414, back to this
// page in state 0xf afterwards) and a sound.
// orig: 0x0027f5b0
int32_t galmap_planet_update(uint32_t w) {
    const uint32_t b = lwu(PAD + 0x1c4);
    if ((b & 0x10) != 0) {
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) {
            sw(MS + 8, parent);
            return 0;
        }
        return lw(MS + 0x124) != 0 ? 0 : -1;
    }
    if ((b & 0x800) != 0) {
        sw(OV(0x00184414), lwu(0x0015ed84));
        return 1;
    }
    if ((b & 0x40) != 0) {
        moby_sound_start(0, 0x11, lwu(w + 0x14));
        return 1;
    }
    if ((b & 0x20) != 0) {
        sw(MS + 0xe4, lwu(OV(0x00184414)));
        sw(MS + 0xf0, lwu(MS + 4));
        sw(MS + 0xf4, 0xf);
        sw(MS + 0xc, 3);
        moby_sound_start(0, 0x11, lwu(w + 0x14));
    }
    return 0;
}

// The planet page's model (transliterated: galactic_map_tr.cpp).
// orig: 0x0027cbc0
int32_t galmap_planet_draw(uint32_t w) {
    return static_cast<int32_t>(galmap_planet_draw_u(w, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f).v0);
}

// orig: (helper) `jalr` to a widget function of the galactic map's pages (the level's address as Veldin's)
bool galactic_map_widget_call(uint32_t fn, uint32_t w, int32_t, int32_t& r) {
    switch (ovl_code_to_veldin(fn)) {
        case 0x0027f440: r = galmap_nop_focus(w); return true;
        case 0x0027f448: r = galmap_list_focus_in(w); return true;
        case 0x0027f550: r = galmap_view_update(w); return true;
        case 0x0027f578: r = galmap_view_draw(w); return true;
        case 0x0027f5b0: r = galmap_planet_update(w); return true;
        case 0x0027cbc0: r = galmap_planet_draw(w); return true;
        default: return false;
    }
}

}  // namespace rac1
