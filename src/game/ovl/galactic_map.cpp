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
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/galactic_map_tr.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/port_stubs.h>
#include <host/level_slots.h>

#include <vector>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};  // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1 (+0x1c4: the buttons pressed this frame)
constexpr uint32_t UNLOCKED = 0x0013d510;  // the planets the ship can fly to (words, 0 ends; base ELF data)

inline uint64_t sxt64(uint32_t v) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v)));
}

inline int32_t cvt_w_i32(float f) {
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) {
        return static_cast<int32_t>((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu);
    }
    return static_cast<int32_t>(f);
}
}  // namespace

// orig: 0x0023bef8
TrRet ovl_23bef8_tr(uint32_t tex_idx, uint32_t x16, uint32_t y16, uint32_t w16, uint32_t h16, uint32_t u16, uint32_t v16, uint32_t alpha, float, float, float, float) {
    const uint32_t p = lwu(OV(0x001611c0u));
    const uint32_t entry_ptr = lwu(OV(0x0017e4f8u)) + (tex_idx << 2);
    const int32_t desc_idx = lh(entry_ptr + 2u);
    const uint32_t desc = lwu(OV(0x0017e4fcu)) + static_cast<uint32_t>(desc_idx << 3);
    const uint32_t th_uv = 1u << ((lbu(desc + 7u) + 4u) & 31u);
    const uint32_t tw_uv = 1u << ((lbu(desc + 6u) + 4u) & 31u);

    sw(p + 0x0u, 0x10000005u);
    sw(p + 0x4u, 0u);
    sw(p + 0x8u, 0u);
    sw(p + 0xcu, 0x50000005u);
    const uint32_t q = p + 0x10u;
    sw(OV(0x001611c0u), q);

    sd(q + 0x00u, (0xe800ull << 47) | 0x8001ull);
    sd(q + 0x08u, 0x05353106ull);

    const uint64_t tex0 = static_cast<uint64_t>(hud_tex0(static_cast<int32_t>(tex_idx)));
    const uint64_t rgba = (sxt64(alpha) << 24) | 0x007f7f7full;
    const uint64_t uv0 = sxt64(u16) | (sxt64(v16) << 16);
    sd(q + 0x20u, rgba);
    sd(q + 0x10u, tex0);
    sd(q + 0x18u, 0x156ull);
    sd(q + 0x28u, uv0);

    const uint32_t x1_16 = x16 + w16;
    const uint32_t y1_16 = y16 + h16;
    const uint32_t v1_16 = v16 + th_uv;
    const uint32_t u1_16 = u16 + tw_uv;

    const uint32_t oy0 = lwu(0x0013e514u);
    const uint32_t ox0 = lwu(0x0013e510u);
    const uint64_t z0 = sxt64(lwu(OV(0x0017e4d8u) + 0xcu)) << 32;
    const uint64_t xyz0 = sxt64(x16 + ox0 - 8u) | (sxt64(y16 + oy0 - 8u) << 16) | z0;
    const uint64_t uv1 = sxt64(u1_16) | (sxt64(v1_16) << 16);
    sd(q + 0x38u, uv1);
    sd(q + 0x30u, xyz0);

    const uint32_t oy1 = lwu(0x0013e514u);
    const uint32_t ox1 = lwu(0x0013e510u);
    const uint64_t z1 = sxt64(lwu(OV(0x0017e4d8u) + 0xcu)) << 32;
    const uint64_t xyz1 = sxt64(x1_16 + ox1 - 8u) | (sxt64(y1_16 + oy1 - 8u) << 16) | z1;
    sd(q + 0x48u, 0ull);
    sd(q + 0x40u, xyz1);

    const uint32_t end_p = lwu(OV(0x001611c0u)) + 0x50u;
    sw(OV(0x001611c0u), end_p);
    return TrRet{end_p, 0.0f};
}

// orig: 0x0023c920
TrRet ovl_23c920_tr(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t colour, uint32_t subpixel, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t p = lwu(OV(0x001611c0u));
    sw(p + 0x0u, 0x10000003u);
    sw(p + 0x4u, 0u);
    sw(p + 0x8u, 0u);
    sw(p + 0xcu, 0x50000003u);
    const uint32_t q = p + 0x10u;
    sw(OV(0x001611c0u), q);

    sd(q + 0x00u, (0x8800ull << 47) | 1ull);
    sd(q + 0x08u, 0x4410ull);
    sd(q + 0x10u, 0x41ull);
    sd(q + 0x18u, static_cast<uint64_t>(colour));

    constexpr uint64_t kLineZ = 0xfffff000ull << 24;
    if (subpixel != 0) {
        const uint32_t oy0 = lwu(0x0013e514u);
        const uint32_t ox0 = lwu(0x0013e510u);
        sd(q + 0x20u, sxt64(x0 + ox0 - 8u) | (sxt64(y0 + oy0 - 8u) << 16) | kLineZ);
        const uint32_t oy1 = lwu(0x0013e514u);
        const uint32_t ox1 = lwu(0x0013e510u);
        sd(q + 0x28u, sxt64(x1 + ox1 - 8u) | (sxt64(y1 + oy1 - 8u) << 16) | kLineZ);
    } else {
        const uint32_t oy0 = lwu(0x0013e514u);
        const uint32_t ox0 = lwu(0x0013e510u);
        sd(q + 0x20u, sxt64((x0 << 4) + ox0 - 0x10u) | (sxt64((y0 << 4) + oy0 - 0x10u) << 16) | kLineZ);
        const uint32_t oy1 = lwu(0x0013e514u);
        const uint32_t ox1 = lwu(0x0013e510u);
        sd(q + 0x28u, sxt64((x1 << 4) + ox1 - 0x10u) | (sxt64((y1 << 4) + oy1 - 0x10u) << 16) | kLineZ);
    }

    const uint32_t end_p = lwu(OV(0x001611c0u)) + 0x30u;
    sw(OV(0x001611c0u), end_p);
    return TrRet{end_p, 0.0f};
}

// orig: 0x002033c0
TrRet galaxy_view_draw_u(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    texture_block_begin(0);
    sub_002a0ba8(0x42u, (0x8000ull << 24) | 0x44ull);
    sub_002a0ba8(0x47u, 0x4bull);
    gs_flat_rect(0, 0, 0x200, 0x1c0, 0x80000000ull, 0);

    const uint32_t w16 = (x1 - x0) << 4;
    const uint32_t h16 = (y1 - y0) << 4;
    ovl_23bef8_tr(static_cast<uint32_t>(hud_frame(0xe99au, 0xe)), 0u, 0u, w16, h16, 0u, 0u, 0x80u, 0.0f, 0.0f, 0.0f, 0.0f);
    sub_002a0ba8(0x8u, 0ull);
    const uint32_t f15 = static_cast<uint32_t>(hud_frame(0xe99au, 0xf));
    const uint32_t scroll_u = lwu(OV(0x0015f3f8u)) & 0xfffu;
    ovl_23bef8_tr(f15, 0u, 0u, w16, h16, scroll_u, 0u, 0x80u, 0.0f, 0.0f, 0.0f, 0.0f);
    sub_002a0ba8(0x8u, 5ull);

    uint32_t row_ptr = OV(0x001c1f38u);
    uint32_t off_1c = 0x10u;
    for (int32_t planet = 1; planet < 20; ++planet, row_ptr += 0x10u, off_1c += 0x10u) {
        const int32_t px = lw(row_ptr + 0x0u);
        if (px == 0) continue;

        int32_t vis = 0;
        if (lbu(0x0013dd58u + static_cast<uint32_t>(planet)) != 0) {
            vis = 3;
        } else if (lbu(0x0013dd40u + static_cast<uint32_t>(planet)) != 0) {
            vis = 2;
        }
        if (vis == 0) continue;

        int32_t py = lw(row_ptr + 0x4u);
        if (lw(0x0015ed80u) != 0) {
            py = ee_div(py * 0x1c0, 0x1a0);
        }

        if (vis == 3) {
            const int32_t dot = hud_frame(0xe99au, 0xc);
            hud_sprite(dot, px - 5, py - 5, 0xa, 0xa, 0x80ull);
        } else if (vis == 2) {
            const int32_t t_on = frame_scale_i(0x16);
            const int32_t period = t_on + frame_scale_i(0x8);
            const int32_t phase = ee_rem(lw(OV(0x0015f3f8u)), period);
            if (phase < frame_scale_i(0x16)) {
                const int32_t dot = hud_frame(0xe99au, 0xc);
                hud_sprite(dot, px - 5, py - 5, 0xa, 0xa, 0x80ull);
            }
        }

        if (planet != lw(OV(0x001841f0u) + 0x224u)) continue;

        const uint32_t dx_ptr = OV(0x001c1f30u) + off_1c;
        const int32_t dy = lw(row_ptr + 0xcu);
        const int32_t dx = lw(dx_ptr);
        const int32_t lx = px + dx;
        const int32_t ly = py + dy;
        const int32_t ly_sh = ly + 1;
        const int32_t lx_sh = lx + 1;

        const Vec4 dv{static_cast<float>(dx), static_cast<float>(dy), 0.0f, 0.0f};
        const int32_t dist_milli = cvt_w_i32(fpu_mul(vec2_length(dv), 1000.0f));
        const int32_t seg = dist_milli - 0x1f40;
        const int32_t sx = lx + ee_div((px - lx) * seg, dist_milli);
        const int32_t sy = ly + ee_div((py - ly) * seg, dist_milli);

        const uint32_t name_id_addr = OV(0x001c1e44u) + static_cast<uint32_t>(planet * 0xc);
        const uint32_t name_str = static_cast<uint32_t>(sub_00203ad8(lw(name_id_addr)));
        const int32_t tw = text_width_small(name_str, -1);
        const int32_t end_x = (-1 < lw(dx_ptr)) ? (lx + tw) : (lx - tw);
        int32_t tx = end_x;

        ovl_23c920_tr(static_cast<uint32_t>(sx + 1), static_cast<uint32_t>(sy + 1), static_cast<uint32_t>(lx_sh), static_cast<uint32_t>(ly_sh), 0x80000000u, 0u, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        ovl_23c920_tr(static_cast<uint32_t>(lx_sh), static_cast<uint32_t>(ly_sh), static_cast<uint32_t>(end_x + 1), static_cast<uint32_t>(ly_sh), 0x80000000u, 0u, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        if (!(end_x < lx)) tx = lx;
        text_small(tx + 1, (ly - lw(OV(0x0015f650u))) + 1, 0x80000000ull, name_str, -1);

        ovl_23c920_tr(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy), static_cast<uint32_t>(lx), static_cast<uint32_t>(ly), 0x80f0f0f0u, 0u, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        ovl_23c920_tr(static_cast<uint32_t>(lx), static_cast<uint32_t>(ly), static_cast<uint32_t>(end_x), static_cast<uint32_t>(ly), 0x80f0f0f0u, 0u, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        text_small(tx, ly - lw(OV(0x0015f650u)), 0x80f0f0f0ull, name_str, -1);

        const int32_t ring = hud_frame(0xe99au, 0xd);
        hud_sprite(ring, px - 0xa, py - 0xa, 0x14, 0x14, 0x80ull);
    }

    texture_block_end();
    return TrRet{0u, 0.0f};
}

// orig: 0x0027cbc0
TrRet galmap_planet_draw_u(uint32_t w, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t m = lwu(w + 0x44u);
    sw(m + 0x10u, f2u(fpu_add(u2f(lwu(OV(0x00166dc0u))), 8.0f)));
    sw(m + 0x14u, f2u(fpu_add(u2f(lwu(OV(0x00166dc4u))), u2f(lwu(OV(0x00160364u))))));
    sw(m + 0x18u, f2u(fpu_sub(u2f(lwu(OV(0x00166dc8u))), u2f(0x3dcccccdu))));
    if (const uint32_t m_draw = lwu(w + 0x44u); m_draw != 0) {
        moby_draw_list(m_draw, 1);
    }

    texture_block_begin(0);
    uint32_t gb_found = 0;
    uint32_t gb_ptr = 0x0014bec0u + (lwu(OV(0x00184414u)) << 2);
    for (int32_t i = 3; i >= 0; --i, ++gb_ptr) {
        if (lbu(gb_ptr) != 0) ++gb_found;
    }

    const uint32_t buf = ee_stack_alloc(0x100);
    const uint32_t fmt = OV(0x00160368u);
    const uint32_t s_4f4f = static_cast<uint32_t>(sub_00203ad8(0x4f4f));
    const uint32_t s_4f53 = static_cast<uint32_t>(sub_00203ad8(0x4f53));
    const uint32_t help_planet = lwu(OV(0x001841f0u) + 0x224u);
    const uint32_t total_gb = lwu(OV(0x001c4988u) + (help_planet << 2));
    const std::vector<uint64_t> args = {
        sxt64(s_4f4f),
        sxt64(gb_found),
        sxt64(s_4f53),
        sxt64(total_gb),
    };
    libc_sprintf(buf, fmt, args);

    text_right(lw(w + 0x20u) - 0x10, (lw(0x0013e504u) >> 1) - 8, 0x80000000ull, buf, -1);
    text_right(lw(w + 0x20u) - 0x11, (lw(0x0013e504u) >> 1) - 9, 0x80ffa888ull, buf, -1);
    texture_block_end();
    ee_stack_free(0x100);
    return TrRet{8u, 0.0f};
}

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
    n = level_slots_list_rows(list, n);  // enhancement: the level mods' rows, the prompt's order (src/host/level_slots.h; else nothing)
    sh(list + n * 0xc, 0);
    const uint32_t order = OV(0x00166c00u - 0x6960);
    sw(w + 0x40, 0);
    sw(w + 0x30, lwu(w + 0x30) | 0x8000u);
    if (const int32_t row = level_slots_list_focus(n); row >= 0) {  // enhancement: on a level mod's level, its row (level_slots.h)
        sw(w + 0x40, static_cast<uint32_t>(row));
        return 0;
    }
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
        level_slots_list_back();  // enhancement: the current level again (src/host/level_slots.h; else nothing)
        return 1;
    }
    if ((b & 0x40) != 0) {
        moby_sound_start(0, 0x11, lwu(w + 0x14));
        return 1;
    }
    if ((b & 0x20) != 0 && !level_slots_map_row()) {  // (enhancement: a level mod's row has no movie)
        sw(MS + 0xe4, lwu(OV(0x00184414)));
        sw(MS + 0xf0, lwu(MS + 4));
        sw(MS + 0xf4, 0xf);
        sw(MS + 0xc, 3);
        moby_sound_start(0, 0x11, lwu(w + 0x14));
    }
    return 0;
}

// The planet page's model (0x27cbc0).
// orig: 0x0027cbc0
int32_t galmap_planet_draw(uint32_t w) {
    if (level_slots_map_row()) return 8;  // enhancement: a level mod's row: not the base level's gold bolts (src/host/level_slots.h)
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
