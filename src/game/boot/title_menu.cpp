// See title_menu.h. The addresses below are the boot module's.
#include <game/boot/title_menu.h>

#include <game/enhance/advanced.h>
#include <game/enhance/graphics.h>
#include <game/enhance/update_notice.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/port_stubs.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

namespace {
constexpr FrConst<0x001d5bf0u> MS{};     // the menu's block (the overlays' 0x1b9cf0)
constexpr FrConst<0x001d5d90u> MOBYS{};  // its 14 mobys (the overlays' 0x1b9e90)
constexpr FrConst<0x001ce2c0u> SLOTS{};  // the slots in use (the overlays' 0x1b23c0)
constexpr FrConst<0x001d45c8u> ROOT{};   // the main menu's root page (New Game, Load Game, Options)
constexpr FrConst<0x0015f604u> MODE{};   // the game mode

// orig: (helper) the widget in slot i of page p (+0x44)
uint32_t widget_of(uint32_t p, uint32_t i) { return lwu(p + 0x44 + i * 4); }
// orig: (helper) repeated in front:0x002192a8 / 0x002191b8: the focus-out (+0xc, a1 0) of every
// widget of the page shown
void page_widgets_leave() {
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t w = widget_of(lwu(MS + 4), i);
        if (w != 0 && lwu(w + 0xc) != 0) menu_widget_call(lwu(w + 0xc), w, 0);
    }
}
// orig: (helper) a menu moby's animation `set` at its last frame
void moby_anim_last(uint32_t m, uint32_t set) {
    const uint32_t seq = lwu(lwu(m + 0x24) + 0x48 + set * 4);
    moby_set_anim(m, set, static_cast<int32_t>(lbu(seq + 0x10)) - 1);
}
// orig: (helper) repeated in front:0x002196b8: the smallest power of two (log2, from 7: 128)
// not below n
int32_t log2_from_128(int32_t n) {
    int32_t s = 7;
    if (0x80 < n) {
        do {
            ++s;
        } while ((1 << s) < n);
    }
    return s;
}
}  // namespace

// The main menu's start (states 0 / 0x2d): its root page, the menu's light and camera, the
// frame counter, the menu's buffers above the level's (*(0x1940c4) / *(0x1940c8) + 0xa0000) and
// their VRAM records, its 14 mobys (class 0x472) spawned with the root page's animations at
// their last frame.
// orig: front:0x00218f98
void title_menu_page_start() {
    advanced_menu_install();       // enhancement: Options > Advanced, Graphics
    graphics_main_menu_install();  // enhancement: New Game's Original / Modern mode popup
    sw(MS, 2);
    sw(MS + 8, ROOT);
    sw(MS + 0x124, 0);
    sw(MS + 4, ROOT);
    vec3_scale(V(FR(0x0019c150)), V(FR(0x001601c0)), 1.0f);
    lq_sq(FR(0x0019c140), FR(0x001601d0));
    sd(FR(0x0019c170), 0);
    sd(FR(0x0019c178), 0);
    sd(FR(0x0019c160), 0);
    sd(FR(0x0019c168), 0);
    dma_wait(1);
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    const uint32_t t1 = lwu(FR(0x001940c8)) + 0xa0000, a3 = lwu(FR(0x001940c4)) + 0xa0000;
    const uint32_t a2 = a3 + 0x3c000, t0 = t1 + 0xe0000;
    sw(FR(0x0015f438), lwu(FR(0x0015f438)) + 1);
    sw(MS + 0xfc, a2 + 0xc1000);
    sw(MS + 0x100, t0 + 0x11800);
    sw(FR(0x00160f0c), 0xa0000);
    sw(MS + 0x104, a3);
    sw(MS + 0x10, t1);
    sw(MS + 0x108, a2);
    sw(MS + 0x10c, t0);
    menu_vram_layout(1);
    sub_002a07f8();
    sw(MS + 0x18, lwu(0x0015ee78));
    if (lwu(MS + 4) == 0) return;
    const uint32_t C = FR(0x00186f40u);  // the camera
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t m = menu_moby_spawn(0x472);
        sw(MOBYS + i * 4, m);
        if (m == 0) continue;
        sh(m + 0x34, lhu(m + 0x34) & 0xfffdu);
        sw(m + 0x74, FR(0x0023a2c0));  // (the overlays' 0x2df638, the menu mobys' update)
        swc1(m + 0x10, lwc1(C + 0x140));
        swc1(m + 0x14, lwc1(C + 0x144));
        swc1(m + 0x18, lwc1(C + 0x148));
        sw(m + 0x40, 0);
        sw(m + 0x44, 0);
        sw(m + 0x48, 0);
        moby_anim_last(m, lwu(lwu(MS + 4) + i * 4));
    }
}

// The menu's closing (from its 10th frame): the widgets' focus-out, the frame's uploads back to
// the menu's start, the mobys freed, state 0x14 for 2 frames.
// orig: front:0x002191b8
void title_menu_close() {
    if (lw(MS + 0x110) < 10) return;
    if (lwu(MS + 4) != 0) {
        page_widgets_leave();
        sw(MS + 4, 0);
    }
    sw(0x0015ee78, lwu(MS + 0x18));
    for (uint32_t i = 0; i < 0xe; ++i) sw(MOBYS + i * 4, static_cast<uint32_t>(menu_moby_free(lwu(MOBYS + i * 4))));
    sw(MS, 0x14);
    sw(MS + 0x14, 2);
}

// The main menu's frame (mode 3): the closing countdown then back to the title (mode 0), the
// start of a state, the memory card's screens asked (0x15eeb4 & 1), a page change (12 frames of
// the mobys' animations), the widgets' updates and the focus change, the mobys, the closing.
// orig: front:0x002192a8
void title_menu_update() {
    sw(MS + 0x110, lwu(MS + 0x110) + 1);
    menu_camera_set();
    const int32_t st = lw(MS);
    if (st == 0x14) {
        const int32_t t = lw(MS + 0x14);
        if (t != 0) {
            sw(MS + 0x14, static_cast<uint32_t>(t - 1));
            if (t - 1 != 0) return;
        }
        if (lh(0x001516d8) != 0) return;
        sw(FR(0x0015f5b8), 0x1e000);
        sub_002a07c8();
        fog_setup();
        sw(MS + 0x10, 0);
        sw(FR(0x0015f618), 1);
        sw(MS + 0x108, 0);
        sw(MS + 0x10c, 0);
        sw(MS + 0x104, 0);
        sw(MODE, 0);
        return;
    }
    if (st == 0 || st == 0x2d) title_menu_page_start();
    update_notice_menu_frame(MS, ROOT);  // enhancement: a newer version found: the update notice opened, once
    if ((lwu(0x0015eeb4) & 1) != 0) {
        screen_open(3, lwu(MS + 4));
        return;
    }
    if (lw(MS) == 1) {
        const int32_t t = lw(MS + 0x14);
        const int32_t v = t < 1 ? 0 : t - 1;
        sw(MS + 0x14, static_cast<uint32_t>(v));
        if (v == 0) {
            const uint32_t next = lwu(MS + 8);
            sw(MS + 8, 0);
            sw(MS + 4, next);
            sw(MS, lwu(next + 0x3c));
            for (uint32_t i = 0; i < 0xe; ++i) {
                const uint32_t w = widget_of(lwu(MS + 4), i);
                if (w != 0 && lwu(w + 8) != 0) menu_widget_call(lwu(w + 8), w, 0);
            }
        }
    } else if (lwu(MS + 8) != 0) {
        const uint32_t next = lwu(MS + 8);
        moby_sound_start(lwu(MS + 4) == next ? 3 : 4, 0x11, lwu(MOBYS));
        page_widgets_leave();
        const uint32_t cur = lwu(MS + 4), to = lwu(MS + 8);
        bool back = to == lwu(cur + 0x38);
        if (cur == to) back = !back;
        for (uint32_t i = 0; i < 0xe; ++i) {
            const uint32_t w = widget_of(lwu(MS + 8), i);
            if (w != 0) sw(w + 0x14, lwu(MOBYS + i * 4));
            if (back) {
                const uint32_t m = lwu(MOBYS + i * 4);
                moby_anim_last(m, lwu(lwu(MS + 4) + i * 4));
                swc1(lwu(MOBYS + i * 4) + 0x58, -1.0f);
            } else {
                moby_set_anim(lwu(MOBYS + i * 4), lwu(lwu(MS + 8) + i * 4), 0);
                swc1(lwu(MOBYS + i * 4) + 0x58, 1.0f);
            }
        }
        if (back) graphics_menu_back(cur, to, MOBYS);  // enhancement: leaving our pages, the parent's boxes
        else graphics_menu_forward(cur, to, MOBYS);    // enhancement: New Game's second window, the boxes held
        const uint32_t shown = lwu(MS + 4);
        sw(MS, 1);
        sw(MS + 0x14, 0xc);
        sw(MS + 0xd0, shown);
        sw(MS + 4, 0);
    }
    const uint32_t page = lwu(MS + 4);
    if (page != 0) {
        const uint32_t ws = page + 0x44;
        uint32_t a = ws;
        for (int32_t k = 0xd; k >= 0; --k, a += 4) {
            if (ws == 0) continue;
            const uint32_t w = lwu(a);
            if (w == 0 || lwu(w) == 0) continue;
            if (menu_widget_call(lwu(w), w, 0) != 0) sw(MS + 0xc, 1);
        }
        const uint32_t p = lwu(MS + 4);
        if (p != 0 && lwu(p + 0x80) != 0) {
            const uint32_t f = lwu(p + 0x40);
            if (lwu(f + 0xc) != 0) menu_widget_call(lwu(f + 0xc), f, 1);
            sw(lwu(MS + 4) + 0x40, lwu(lwu(MS + 4) + 0x80));
            sw(lwu(MS + 4) + 0x80, 0);
            const uint32_t g = lwu(lwu(MS + 4) + 0x40);
            if (lwu(g + 8) != 0) menu_widget_call(lwu(g + 8), g, 1);
        }
    }
    moby_update_visible();
    if (lw(MS + 0xc) != 0) title_menu_close();
}

// The main menu's Options list's focus in: its entries, Language / Sound (0x1d4840) or, in a PAL
// game (0x15ee90), Language / Video / Sound (0x1d4810). Then (an enhancement) the PC
// version's pages added: Graphics, Advanced.
// orig: front:0x0021a1b0
int32_t title_options_list_focus_in(uint32_t w) {
    sw(w + 0x34, lw(0x0015ee90) != 0 ? FR(0x001d4810u) : FR(0x001d4840u));
    graphics_main_options_entries(w);  // enhancement: Options > Graphics / Advanced
    return 0;
}

// A label's update (the boot module's copy of the overlays' 0x2779c8): when focused, Start /
// Select / L1 close the menu (1, unless +0x124), triangle goes to the parent page (or -1 on a
// root page without +0x124).
// orig: front:0x00219d80
int32_t title_label_update(uint32_t w) {
    if (lwu(lwu(MS + 4) + 0x40) != w) return 0;
    if ((lwu(0x0013c940 + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) return 1;
    if ((lwu(0x0013c940 + 0x1c4) & 0x10) == 0) return 0;
    const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
    if (parent != 0) {
        sw(MS + 8, parent);
        return 0;
    }
    return lw(MS + 0x124) == 0 ? -1 : 0;
}

// orig: front:0x0021a308
int32_t title_nop_focus(uint32_t) { return 0; }

// The main menu drawn (mode 3; the title's scene under it): the menu mobys (the pass's start,
// one moby list call, each slot's moby), the camera, each widget's box from its moby, the widgets
// drawn into textures and copied (flag 2 before the mobys' flush, the others after), the panels.
// orig: front:0x002196b8
void title_menu_draw() {
    sub_002a0ba8(0x47, 0x5360b);
    moby_pass_start();
    spr_moby_fill();
    spr_moby_save();
    moby_draw_list(lwu(FR(0x0015ff18)), 4);  // (the boot module's 0x20d330: the overlays' 0x24ff08)
    menu_camera_set();
    render_view_setup();
    for (uint32_t i = 0; i < 0xe; ++i) {
        if (lwu(SLOTS + i * 4) == 0) continue;
        const uint32_t m = lwu(MOBYS + i * 4);
        if (m == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        moby_draw_list(m, 1);
    }
    const uint32_t page = lwu(MS + 4);
    const uint32_t ws = page != 0 ? page + 0x44 : 0;
    const uint32_t box = ee_stack_alloc(0x60);  // the bounds and the box by address
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t m = lwu(MOBYS + i * 4);
        if (m == 0 || lwu(SLOTS + i * 4) == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        const uint32_t d = lwu(m + 0x78);
        lq_sq(box, d);
        lq_sq(box + 0x10, d + 0x30);
        menu_box_project(box, box + 0x10, box + 0x20, box + 0x24, box + 0x28, box + 0x2c);
        const int32_t x = lw(box + 0x28) + 1, y = lw(box + 0x2c) + 1, bw = lw(box + 0x20), bh = lw(box + 0x24);
        if (ws != 0) {
            const uint32_t w = lwu(ws + i * 4);
            if (w != 0) {
                sw(w + 0x20, static_cast<uint32_t>(bw));
                sw(w + 0x24, static_cast<uint32_t>(bh));
                sw(w + 0x18, static_cast<uint32_t>(x));
                sw(w + 0x1c, static_cast<uint32_t>(y));
            }
        }
        sw(d + 0x50, static_cast<uint32_t>(x));
        sw(d + 0x54, static_cast<uint32_t>(y));
        sw(d + 0x58, static_cast<uint32_t>(bw));
        sw(d + 0x5c, static_cast<uint32_t>(bh));
        gs_flat_rect(x + 1, y + 1, x + bw - 1, y + bh - 1, static_cast<uint64_t>(static_cast<int64_t>(lw(FR(0x001601b0)))), 0);
    }
    ee_stack_free(0x60);
    for (int32_t pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < 0xe; ++i) {
            const uint32_t m = lwu(MOBYS + i * 4);
            if (m == 0 || ws == 0) continue;
            const uint32_t w = lwu(ws + i * 4);
            if (w == 0) continue;
            const uint32_t df = lwu(w + 0x10);
            if ((df & 4) != 0) continue;
            if (lwu(SLOTS + i * 4) == 0) continue;
            const uint32_t fn = lwu(w + 4);
            if (fn == 0) continue;
            if (i == 6 && lw(MS + 0xd8) == 0) continue;
            if (pass == 0 && (df & 2) == 0) continue;
            if (pass == 1 && (df & 2) != 0) continue;
            if ((df & 1) != 0) {
                menu_widget_call(fn, w, 0);
                continue;
            }
            const uint32_t d = lwu(m + 0x78);
            const int32_t x = lw(d + 0x50), y = lw(d + 0x54), bw = lw(d + 0x58), bh = lw(d + 0x5c);
            const int32_t sx = log2_from_128(bw);
            int32_t sy = log2_from_128(bh);
            while (!(sx + sy < 0x12)) --sy;
            render_to_texture_begin(sx, sy, pass != 0 ? 1 : 0, 1.0f);
            const int32_t th = 1 << sy, tw = 1 << sx;
            gs_flat_rect_z(0, 0, tw, th, static_cast<uint64_t>(static_cast<int64_t>(lw(FR(0x001601b0)))), 0, 0);
            const uint32_t w2 = lwu(ws + i * 4);
            const int32_t r = menu_widget_call(lwu(w2 + 4), w2, 0);
            render_to_texture_end();
            if ((r & 1) != 0) continue;
            int32_t u0 = 0, v0 = 0, u1 = tw, v1 = th;
            if ((r & 2) != 0) {
                u1 = bw;
                v1 = bh;
            } else if ((r & 8) != 0) {
                const int32_t dx = tw - bw, dy = th - bh;
                u0 = dx / 2;
                v0 = dy / 2;
                u1 = tw - u0;
                v1 = th - v0;
                if (tw < u1) u1 = tw;
                if (th < v1) v1 = th;
                if (u0 < 0) u0 = 0;
                if (v0 < 0) v0 = 0;
            } else if ((r & 4) != 0) {
                if (bw < bh) {
                    u0 = tw / 2 - ee_div(bw * u1, bh * 2);
                    u1 = u1 - u0;
                } else {
                    v0 = th / 2 - ee_div(bh * v1, bw * 2);
                    v1 = v1 - v0;
                }
            } else if ((r & 0x10) == 0) {
                continue;
            }
            sub_002a0ba8(0x42, 0x8000000064ull);
            sub_002a0ba8(0x47, 0x43);
            glyph_sprite(x, y, bw, bh, u0, v0, u1 - u0, v1 - v0, 0x80808080ull, ld(0x0015eed0));
        }
        if (pass == 0) {
            spr_moby_load();
            moby_pass_end();
        }
    }
    texture_block_begin(0);
    for (uint32_t i = 0; i < 0xe; ++i) {
        if (lwu(SLOTS + i * 4) == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        menu_panel_draw(lwu(MOBYS + i * 4));
    }
    texture_block_end();
}

}  // namespace rac1
