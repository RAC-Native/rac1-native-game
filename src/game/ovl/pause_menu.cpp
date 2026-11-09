// Ported overlay code: the full-screen menu's update (game mode 3): its opening from the
// gameplay frame, the page changes, the widgets' updates, the cheat codes, its closing. The
// pages and widgets are game data (0x1b2588. in the overlay); see pause_menu.h Callees not ported are TODO_PORT (the other
// pages' functions,
// the frame capture, the planet map / ship screens).
#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/enhance/graphics.h>
#include <game/enhance/update_notice.h>
#include <game/boot/title_menu.h>
#include <game/ovl/credits_tr.h>
#include <game/ovl/memory_card.h>
#include <host/level_slots.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/snd989.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/movie_mode.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/veldin_mobys.h>
#include <game/port_stubs.h>
#include <runtime/ee/ee_thread.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};      // the menu's state
constexpr OvConst<0x001b9e90u> MOBYS{};  // its 14 mobys
constexpr uint32_t PAD = 0x0013c940;    // pad 1
// orig: (helper) the widget in slot i of page p (+0x44)
uint32_t widget_of(uint32_t p, uint32_t i) { return lwu(p + 0x44 + i * 4); }
// orig: (helper) repeated in 0x00276bd0 / 0x00276908: the focus-out (+0xc, a1 0) of every
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
}  // namespace

// orig: (helper) `jalr` to a widget function: a0 the widget, a1 (focus functions) 1 for a
// change of focus; the ported ones, others reported once (0 returned)
int32_t menu_widget_call(uint32_t fn, uint32_t w, int32_t a1) {
    if (ovl_reloc_level() == OVL_FRONT) {  // (the boot module's own widget functions, its main menu's Options)
        switch (region_to_ntsc(OVL_FRONT, fn)) {  // (the NTSC-U boot module's addresses)
            case 0x0021a1b0: return title_options_list_focus_in(w);
            case 0x00219d80: return title_label_update(w);
            case 0x0021a308: return title_nop_focus(w);
            default: break;
        }
    }
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
        case 0x00278840: return menu_button_update(w);
        case 0x00278e10: return menu_button_draw(w);
        case 0x0027ba18: return menu_save_entry_focus(w);
        default: break;
    }
    int32_t r = 0;
    if (menu_page_widget_call(fn, w, a1, r)) return r;
    TODO_PORT(fn, "menu widget function");
    return 0;
}

// Open the menu in state `state` (0 the pause menu): the music faded, the quit entry shown
// (0x1b2898) when the game can be saved / left, the help's planet (0x184414), mode 3.
// orig: 0x00276190
void menu_open(int32_t state) {
    snd_cmd_16(0x1d);
    sub_00265828(0);
    snd_flush();
    if (lw(OV(0x0016c06c)) != 0) {
        sw(OV(0x0015f634), 1);
        return;
    }
    if (lw(0x00141660) == 0x24) sw(0x00141660, 0);
    const int32_t level = lw(0x0015ed84);
    sw(MS + 0x134, (level == 0xd || lbu(0x0014161b) != 0) ? 1u : 0u);
    sw(MS + 0x138, (level == 0 || level == 0xe) ? 1u : 0u);
    sw(MS + 0xd8, (lw(0x0015eea0) != 0 || lw(0x0015ee20) != 0 || lw(MS + 0xf8) != 0) ? 1u : 0u);
    sw(MS + 0xdc, state == 0x23 ? 1u : 0u);
    sw(OV(0x001b26b8) + 0x38, lw(MS + 0xd8) != 0 ? OV(0x001b2898u) : OV(0x001b2848u));
    sw(OV(0x001b2848) + 0x3c, lw(MS + 0xd8) != 0 ? OV(0x001b2898u) : OV(0x001b26b8u));
    const int32_t lv = lw(0x0015ed84);
    sw(OV(0x0015f5c4), 3);
    sw(MS, static_cast<uint32_t>(state));
    sw(MS + 0x110, 0);
    sw(MS + 0xc, 0);
    sw(MS + 0x10, 0);
    sw(OV(0x00184414), lv < 0x13 ? static_cast<uint32_t>(lv) : 0u);
    goals_update();
    sw(OV(0x001b4498), 0);
    cheats_menu_entries();
    sw(MS + 0x13c, 1);
    sw(MS + 0x140, 0);
    advanced_menu_install();  // enhancement: Options > Advanced
}

// The menu camera: at (256, 256, 64), no rotation (0x166c80 +0x140, the rows +0x350..).
// orig: 0x00276128
void menu_camera_set() {
    constexpr OvConst<0x00166c80u> C{};
    swc1(C + 0x144, 256.0f);
    swc1(C + 0x148, 64.0f);
    swc1(C + 0x140, 256.0f);
    sd(C + 0x350, 0); sd(C + 0x358, 0);
    sd(C + 0x360, 0); sd(C + 0x368, 0);
    sd(C + 0x370, 0); sd(C + 0x378, 0);
    swc1(C + 0x37c, 1.0f);
    swc1(C + 0x350, 1.0f);
    swc1(C + 0x364, 1.0f);
    swc1(C + 0x378, 1.0f);
}

// The menu's VRAM buffers (0x1c'a1b8: 5 records of address, kind): one or two (hi-res)
// 0x11800-byte ones from MS+0x108, as many from MS+0x10c, then the 0x4f000 ones.
// orig: 0x00283908
void menu_vram_layout(int32_t full) {
    constexpr OvConst<0x001ba1b8u> T{};
    uint32_t a = lwu(MS + 0x108), b = lwu(MS + 0x10c);
    const int32_t n = full == 0 ? 1 : 2, extra = full == 0 ? 0 : 1, big = full == 0 ? 0 : 2;
    const int32_t end1 = n + extra;
    int32_t i = 0;
    for (; i < n; ++i) {
        sw(T + static_cast<uint32_t>(i) * 8, a);
        sw(T + static_cast<uint32_t>(i) * 8 + 4, 0);
        a += 0x11800;
    }
    for (; i < end1; ++i) {
        sw(T + static_cast<uint32_t>(i) * 8, b);
        sw(T + static_cast<uint32_t>(i) * 8 + 4, 0);
        b += 0x11800;
    }
    for (; i < end1 + big; ++i) {
        sw(T + static_cast<uint32_t>(i) * 8, a);
        sw(T + static_cast<uint32_t>(i) * 8 + 4, 1);
        a += 0x4f000;
    }
    for (; i < 5; ++i) {
        sw(T + static_cast<uint32_t>(i) * 8 + 4, 0);
        sw(T + static_cast<uint32_t>(i) * 8, 0);
    }
}

// A menu moby of class `cls` (when the class is loaded): drawn (+0x30 / +0x32 0xff), updated
// (+0x31 1), its transform set, colour 0x202020 / 0xe / 0xe, +0x73 0x18 for a class with +6.
// orig: 0x002830d8
uint32_t menu_moby_spawn(uint32_t cls) {
    if (lbu(OV(0x00197bc0) + cls) == 0xff) return 0;
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    sh(m + 0x32, 0xff);
    sb(m + 0x30, 0xff);
    sb(m + 0x20, 0);
    sb(m + 0x31, 1);
    moby_update_transform(m);
    moby_set_color_packed(m, NOT_ADDR(0x202020), 0xe, 0xe, 0);
    if (lbu(lwu(m + 0x24) + 6) != 0) sb(m + 0x73, 0x18);
    return m;
}

// Retire a menu moby (its retire time +0x38 the frame count 0x15f5cc); 0.
// orig: 0x00283178
int32_t menu_moby_free(uint32_t m) {
    if (m == 0) return 0;
    moby_retire(m);
    sd(m + 0x38, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x0015f5cc)))));
    return 0;
}

// The menu's HUD texture sets from the IOP stash into the buffer MS+0x104 (each set: its
// stash index +0x94.. of the HUD header, its size +0x54..; decompressed, relocated): sets 0
// and 2 always, 3 for `which` 0 / 2, 4 for `which` 1 / 2.
// orig: 0x00234fd0
void menu_hud_tex_load(int32_t which) {
    constexpr OvConst<0x0017e4d8u> H{};
    uint32_t dst = lwu(MS + 0x104);
    const uint32_t tmp = lwu(MS + 0x10);
    auto one = [&](uint32_t size_off, uint32_t idx_off, uint32_t len_addr, int32_t set) {
        const uint32_t hdr = lwu(H + 0x18);
        const int32_t size = lw(hdr + size_off);
        if (size == 0) return;
        const int32_t n = lw(len_addr);
        const int32_t q = (n < 0 ? n + 0xf : n) >> 4;
        stash_read(tmp, lwu(hdr + idx_off), 0, q, 0);
        wad_decompress(tmp, dst);
        hud_tex_relocate(set, dst);
        dst += static_cast<uint32_t>(size);
    };
    one(0x54, 0x94, OV(0x0017e510), 0);
    one(0x5c, 0x9c, OV(0x0017e518), 2);
    if (which == 0 || which == 2) one(0x60, 0xa0, OV(0x0017e51c), 3);
    if (static_cast<uint32_t>(which - 1) < 2u) {
        const uint32_t hdr = lwu(H + 0x18);
        if (lw(hdr + 0x64) != 0) {
            const int32_t n = lw(OV(0x0017e520));
            const int32_t q = (n < 0 ? n + 0xf : n) >> 4;
            stash_read(tmp, lwu(hdr + 0xa4), 0, q, 0);
            wad_decompress(tmp, dst);
            hud_tex_relocate(4, dst);
        }
    }
}

// Swap the game's moby list (0x15ffd8 / dc / e0 / e8) with the menu's (0x15fa70 .. 0x15fa7c);
// `menu` the side wanted (0x15f728 the side in use).
// orig: 0x00235668
void moby_lists_swap(int32_t menu) {
    const int32_t cur = lw(OV(0x0015f728));
    if (menu == cur) return;
    const uint32_t g[4] = {OV(0x0015ffd8), OV(0x0015ffdc), OV(0x0015ffe0), OV(0x0015ffe8)};
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t a = lwu(g[k]), b = lwu(OV(0x0015fa70) + k * 4);
        sw(g[k], b);
        sw(OV(0x0015fa70) + k * 4, a);
    }
    sw(OV(0x0015f728), static_cast<uint32_t>(cur ^ 1));
}

// A HUD texture set given back (set 0 also marks the palettes not resident): its textures'
// (0x17e500) and palettes' (0x17e4fc) records of the set back to offsets (less the set's base
// +0x74[set]) and flagged 0x80000000; the base cleared.
// orig: 0x00235310
void hud_tex_unrelocate(int32_t set) {
    constexpr OvConst<0x0017e4d8u> H{};
    const uint32_t s = static_cast<uint32_t>(set);
    const uint32_t base = lwu(lwu(H + 0x18) + s * 4 + 0x74);
    if (set == 0) {
        for (int32_t i = 0; i < lw(lwu(H + 0x18) + 0x34); ++i) sh(lwu(H + 0x24) + static_cast<uint32_t>(i) * 8 + 4, 0);
    }
    const int32_t a = set != 0 ? lw(lwu(H + 0x18) + (s - 1) * 4 + 0x14) : 0;
    const int32_t b = lw(lwu(H + 0x18) + s * 4 + 0x14);
    for (int32_t i = a; i < b; ++i) {
        const uint32_t p = lwu(H + 0x28) + static_cast<uint32_t>(i) * 8;
        sw(p, lwu(p) - base);
        sw(p, lwu(p) | 0x80000000u);
    }
    const int32_t c = set != 0 ? lw(lwu(H + 0x18) + (s - 1) * 4 + 0x34) : 0;
    const int32_t d = lw(lwu(H + 0x18) + s * 4 + 0x34);
    for (int32_t i = c; i < d; ++i) {
        const uint32_t p = lwu(H + 0x24) + static_cast<uint32_t>(i) * 8;
        sw(p, lwu(p) - base);
        sw(p, lwu(p) | 0x80000000u);
    }
    sw(lwu(H + 0x18) + s * 4 + 0x74, 0);
}

// The start of a menu state (0 the pause menu, 0xa / 0xe / 0x21 / 0x23 / 0x2d others): its
// page, a saved page to come back to (MS+0xf0), the frame captured (0x2a0f18), the menu's
// buffers, HUD textures and camera, the menu mobys spawned with their page animations.
// orig: 0x00276368
void menu_page_start() {
    sw(OV(0x00173e50), lwu(OV(0x00173e50)) | 0x80000000u);
    sw(MS + 0x124, 0);
    sw(MS + 0xd0, 0);
    const int32_t st = lw(MS);
    bool hires = false;
    uint32_t page;
    if (st == 0xa) {
        sw(MS, 0xb);
        page = OV(0x001b3518);
        sw(MS + 8, page);
        hires = true;
    } else if (st == 0xe) {
        sw(MS, 0xf);
        page = OV(0x001b6088);
        sw(MS + 8, page);
        hires = true;
    } else if (st == 0x21) {
        sw(MS + 0x124, 1);
        sw(MS, 0x22);
        page = OV(0x001b71f0);
        sw(MS + 8, page);
    } else if (st == 0x23) {
        page = OV(0x001b6b38);
        sw(MS + 8, page);
    } else if (st == 0x2d) {
        sw(MS, 2);
        page = OV(0x001b86c8);
        sw(MS + 8, page);
    } else {
        sw(MS, 2);
        page = OV(0x001b2588);
        sw(MS + 8, page);
    }
    sw(MS + 4, page);
    const uint32_t saved = lwu(MS + 0xf0);
    if (saved != 0) {
        const int32_t s = lw(MS + 0xf4);
        sw(0x0015eed8, lwu(MS + 0x130));
        sw(MS + 8, saved);
        sw(MS + 4, saved);
        sw(MS, static_cast<uint32_t>(s));
        if (s == 0xb || s == 0xf) {
            sw(OV(0x001841f0) + 0x224, lwu(MS + 0xe4));
            // (1.02 Fixes, fix 16: the 1.02 build, 0x277208, updates the goals on the way back to the page)
            if (fixes102_on(16)) goals_update();
            hires = true;
        }
        sw(MS + 0xf4, 0);
        sw(MS + 0xf0, 0);
    }
    vec3_scale(V(OV(0x00180250)), V(OV(0x00160280)), 1.0f);
    lq_sq(OV(0x00180240), OV(0x00160290));
    sd(OV(0x00180270), 0); sd(OV(0x00180278), 0);
    sd(OV(0x00180260), 0); sd(OV(0x00180268), 0);
    lq_sq(MS + 0x40, OV(0x00166dc0));
    lq_sq(MS + 0x50, OV(0x00166dd0));
    constexpr OvConst<0x0016ca40u> R{};
    swc1(MS + 0xe8, lwc1(R + 0xb0));
    swc1(R + 0xb0, u2f(0x3f2147ae));
    swc1(R + 0x21c, 524288.0f);
    swc1(R + 0x228, 255.0f);
    sw(R + 0x218, 0);
    sw(R + 0x22c, 0);
    projection_setup();
    menu_camera_set();
    render_view_setup();
    dma_wait(1);
    // the vsync wait 0x122298: nothing was kicked since the frame shown, which stays (a vsync passes: the host's per-vsync
    // work; on the game's coroutine nothing is shown, ADR 0012); the frame buffer still holds that frame, stored below
    ee_thread_vsync(EeVsyncFrame::Held, false);
    sw(OV(0x0015f3f8), lwu(OV(0x0015f3f8)) + 1);
    if (hires) {
        const uint32_t a3 = lwu(OV(0x00173e04)) + 0x30000, a2 = lwu(OV(0x00173e08)) + 0x30000;
        const uint32_t a1 = a3 + 0x3c000;
        sw(MS + 0x100, a2 + 0xe0000);
        sw(MS + 0xfc, a1 + 0x11800);
        sw(OV(0x001611cc), 0x30000);
        sw(MS + 0x104, a3);
        sw(MS + 0x10, a2);
        sw(MS + 0x108, a1);
        sw(MS + 0x10c, a2 + 0xe0000);
        menu_vram_layout(0);
    } else {
        const uint32_t t0 = lwu(OV(0x00173e08)) + 0x60000, a2 = lwu(OV(0x00173e04)) + 0x60000;
        const uint32_t a1 = a2 + 0x3c000, a3 = t0 + 0xe0000;
        sw(MS + 0x100, a3 + 0x11800);
        sw(MS + 0xfc, a1 + 0xc1000);
        sw(OV(0x001611cc), 0x60000);
        sw(MS + 0x104, a2);
        sw(MS + 0x10, t0);
        sw(MS + 0x108, a1);
        sw(MS + 0x10c, a3);
        menu_vram_layout(1);
    }
    sw(OV(0x0015f578), 0x2000);
    sub_002a07f8();
    if (lw(MS) == 0xb || lw(MS) == 0xf) planet_map_page_start();
    hud_tex_unrelocate(0);
    const int32_t st2 = lw(MS);
    menu_hud_tex_load((st2 == 0xb || st2 == 0xf || st2 == 0x12) ? 1 : 0);
    gs_store_rows(lwu(MS + 0x10), lw(0x0015ee84), lw(0x0013e504) << 11);  // the frame captured
    const uint32_t buf = lwu(0x0015ee78);
    sw(MS + 0x18, buf);
    if (lw(MS) == 0xb || lw(MS) == 0xf) {
        constexpr OvConst<0x001841f0u> P{};
        sw(P + 0x240, buf);
        sw(P + 0x244, buf + 0x40000);
        sw(0x0015ee78, buf + 0x40400);
        for (uint32_t k = 0; k < 4; ++k) {
            sw(P + 0x248 + k * 4, lwu(0x0015ee78));
            sw(0x0015ee78, lwu(0x0015ee78) + 0x4000);
        }
    } else {
        constexpr OvConst<0x001841f0u> P{};
        sw(P + 0x240, 0);
        sw(P + 0x244, 0);
        for (int32_t k = 3; k >= 0; --k) sw(P + 0x248 + static_cast<uint32_t>(k) * 4, 0);
    }
    moby_lists_swap(1);
    const uint32_t ml = lwu(OV(0x00197300));
    sw(MS + 0xcc, lbu(ml + 0xc));
    sb(ml + 0xc, lbu(ml + 0xc) + 0x1c);
    for (uint32_t k = 0; k < 4; ++k) sw(MS + 0x30 + k * 4, lwu(0x00141660 + k * 4));
    if (lwu(MS + 4) == 0) return;
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t m = menu_moby_spawn(0x472);
        sw(MOBYS + i * 4, m);
        if (m == 0) continue;
        sh(m + 0x34, lhu(m + 0x34) & 0xfffdu);
        sw(m + 0x74, OVF(0x002df638));
        swc1(m + 0x10, lwc1(OV(0x00166c80) + 0x140));
        swc1(m + 0x14, lwc1(OV(0x00166c80) + 0x144));
        swc1(m + 0x18, lwc1(OV(0x00166c80) + 0x148));
        sw(m + 0x40, 0);
        sw(m + 0x44, 0);
        sw(m + 0x48, 0);
        moby_anim_last(m, lwu(lwu(MS + 4) + i * 4));
    }
}

// The menu's closing (from its 10th frame): the widgets' focus-out, the hero's equipment back
// (0x141408 from MS+0x30), the HUD textures reloaded, the menu mobys freed, the HUD's bolt
// counter and its camera requests, the moby class list restored, state 0x14 for 2 frames.
// orig: 0x00276908
void menu_close() {
    if (lw(MS + 0x110) < 10) return;
    dma_wait(1);
    sub_002a07f8();
    if (lwu(MS + 4) != 0) {
        page_widgets_leave();
        sw(MS + 4, 0);
    }
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t cur = lwu(0x00141660 + k * 4), saved = lwu(MS + 0x30 + k * 4);
        if (cur != saved) sw(0x00141408 + k * 4, saved != 0 ? saved : 0x26u);
    }
    constexpr OvConst<0x0017e4d8u> H{};
    sw(0x0015ee78, lwu(MS + 0x18));
    hud_tex_upload_set(0, lwu(lwu(H + 0x18) + 0x74), 0);
    hud_tex_unrelocate(2);
    if (lwu(lwu(H + 0x18) + 0x80) != 0) hud_tex_unrelocate(3);
    if (lwu(lwu(H + 0x18) + 0x84) != 0) hud_tex_unrelocate(4);
    for (uint32_t i = 0; i < 0xe; ++i) sw(MOBYS + i * 4, static_cast<uint32_t>(menu_moby_free(lwu(MOBYS + i * 4))));
    {
        const int32_t k = camera_request_7535();
        sub_00235e18(static_cast<uint32_t>(k), frame_scale_i(0xb4));
        const int32_t k2 = camera_request(2, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), 0x0015ed98, 0x0098967f);
        sub_00235e18(static_cast<uint32_t>(k2), frame_scale_i(0xb4));
    }
    sw(MS, 0x14);
    sw(MS + 0x14, 2);
    for (uint32_t i = lwu(MS + 0xcc); static_cast<int32_t>(i) < static_cast<int32_t>(lbu(lwu(OV(0x00197300)) + 0xc)); ++i) sw(lwu(OV(0x00197300)) + i * 4 + 0x48, 0);
    sb(lwu(OV(0x00197300)) + 0xc, lbu(MS + 0xcc));
    sw(OV(0x0015f5d8), 1);
    sw(OV(0x00173e50), lwu(OV(0x00173e50)) & 0x7fffffffu);
    TODO_PORT(0x002a08c8, "DMA: packet kick")();
    dma_wait(1);
}

// The cheat codes: with the pad in its menu state (+0x1c0 & 0xf == 6) the last 20 presses of
// up / down / left / right / circle / square (0x1ba4a8) are matched against the table
// 0x1b9ba0 (each code at a stride); a match unlocks its gold bolt, level, item or skill point.
// orig: 0x002831c0
void cheat_code_input() {
    const uint32_t N = OV(0x001603f8), SEQ = OV(0x001ba4a8), TAB = OV(0x001b9ba0);
    if ((ld(PAD + 0x1c0) & 0xf) != 6) {
        sw(N, 0);
        return;
    }
    const uint32_t b = lwu(PAD + 0x1c4);
    if ((b & 0xf0a0) == 0) return;
    const int32_t n = lw(N);
    if (!(n < 0x14)) return;
    int32_t k;
    if ((b & 0x1000) != 0) k = 0;
    else if ((b & 0x4000) != 0) k = 1;
    else if ((b & 0x8000) != 0) k = 2;
    else if ((b & 0x2000) != 0) k = 3;
    else k = (b & 0x80) != 0 ? 4 : 5;
    sw(N, static_cast<uint32_t>(n + 1));
    sh(SEQ + static_cast<uint32_t>(n) * 2, static_cast<uint32_t>(k));
    if (n + 1 != 0x14) return;
    const int32_t first = lh(SEQ);
    int32_t code = -1;
    for (int32_t s = 2; s < 0x93; ++s) {
        bool match = true;
        if (first != lbu(TAB + (static_cast<uint32_t>(s) & 0xff))) {
            match = false;
        } else {
            for (int32_t i = 1; i < 0x14; ++i) {
                if (lh(SEQ + static_cast<uint32_t>(i) * 2) != lbu(TAB + (static_cast<uint32_t>(i * s + s) & 0xff))) {
                    match = false;
                    break;
                }
            }
        }
        if (match) {
            code = s - 2;
            break;
        }
    }
    if (code == -1) return;
    if (code < 0x25) {
        sb(0x0013d4e8 + static_cast<uint32_t>(code), 1);
        sb(0x0013d4c0 + static_cast<uint32_t>(code), 1);
    } else if (static_cast<uint32_t>(code - 0x25) < 0x12u) {
        level_mark_visited(code - 0x24);
    } else if (static_cast<uint32_t>(code - 0x37) < 6u) {
        sb(0x0013d388 + static_cast<uint32_t>(code - 0x37), 1);
    } else if (static_cast<uint32_t>(code - 0x3d) < 0x1eu) {
        const uint32_t a = 0x0013d408 + static_cast<uint32_t>(code - 0x3d);
        if (lbu(a) != 0) return;
        sb(a, 1);
        sound_play_indexed(1, 0, 0);
        sub_00263d40(TXT(0x53d6), -1);
    }
}

// The focus-in of the save entries: their action 3 (a page) unless 0x1413f4 is 1 (0).
// orig: 0x0027ba18
int32_t menu_save_entry_focus(uint32_t w) {
    sh(lwu(w + 0x34) + 2, lbu(0x001413f4) == 1 ? 0u : 3u);
    return 0;
}

// A list of entries (the pause menu's buttons): their highlight timers, then when focused the
// pad: start / select / L2 (0xd00) close the menu (-1), triangle the parent page (or closes),
// cross the selected entry's action (2 a sound, 3 a page, 4 / 5 the help screens, 6 / 7 / 8 /
// 0xa / 0xb the mode-3 screens, 9 the next level), up / down (L1 / R1 with flag 0x100) the
// selection or the widgets above / below.
// orig: 0x00278840
int32_t menu_button_update(uint32_t w) {
    const uint32_t page = lwu(MS + 4);
    const bool focused = lwu(page + 0x40) == w;
    if (lh(lwu(w + 0x34)) != 0) {
        int32_t idx = 0;
        for (uint32_t off = 0;; off += 0xc, ++idx) {
            const uint32_t items = lwu(w + 0x34);
            if (lh(items + off) == 0) break;
            const uint32_t e = items + off;
            if (focused && lw(w + 0x40) == idx) {
                sh(e + 0xa, lhu(e + 0xa) + 1u);
                continue;
            }
            const int32_t t = lh(e + 0xa);
            if (frame_scale_i(lw(OV(0x00160274))) < t) sh(lwu(w + 0x34) + off + 0xa, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00160274)))));
            const uint32_t e2 = lwu(w + 0x34) + off;
            const int32_t v = lh(e2 + 0xa);
            sh(e2 + 0xa, v > 0 ? static_cast<uint32_t>(lhu(e2 + 0xa) - 1u) : 0u);
        }
    }
    if (!focused) return 0;
    const uint32_t pressed = lwu(PAD + 0x1c4);
    if ((pressed & 0xd00) != 0) {
        if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(0x0015ed84));
        level_slots_list_back();  // enhancement: the ship's map left for the current level (src/host/level_slots.h; else nothing)
        return -1;
    }
    if ((pressed & 0x10) != 0) {
        if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(0x0015ed84));
        level_slots_list_back();  // enhancement: (the same)
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) {
            sw(MS + 8, parent);
        } else if (lw(MS + 0x124) == 0) {
            return -1;
        }
    }
    if ((lwu(PAD + 0x1c4) & 0x40) != 0) {
        const int32_t sel = lw(w + 0x40);
        const uint32_t items = lwu(w + 0x34);
        const uint32_t e = items + static_cast<uint32_t>(sel * 0xc);
        const int32_t action = lh(e + 2);
        auto page_go = [w] { sw(MS + 8, lwu(lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x40) * 0xc) + 4)); };
        auto mode3 = [w](int32_t kind) {
            sw(MS + 0xc, static_cast<uint32_t>(kind));
            sw(MS + 0xf4, 2);
            sw(MS + 0xf0, lwu(MS + 4));
            sw(MS + 0xe4, lwu(lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x40) * 0xc) + 4));
            moby_sound_start(0, 0x11, lwu(w + 0x14));
        };
        switch (static_cast<uint32_t>(action) < 0xcu ? action : -1) {
            case 2: moby_sound_start(2, 0x11, lwu(w + 0x14)); break;
            case 3:
                update_notice_choice(w);  // enhancement: the update notice's Skip Version
                page_go();
                break;
            case 4:
            case 5: {
                if (action == 4) graphics_new_game_choice(w);  // enhancement: the new game popup's mode
                moby_sound_start(0, 0x11, lwu(w + 0x14));
                const int32_t h = lw(0x0015eeb0);
                if (h == 1 || h == 0x10) {
                    page_go();
                    break;
                }
                sw(0x0015eeb4, lwu(0x0015eeb4) | (action == 4 ? 2u : 4u));
                screen_open(3, lwu(lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x40) * 0xc) + 4));
                break;
            }
            case 6: {
                const int32_t t = lh(e + 6);
                if (t != 0) sw(MS + 0xec, lwu(OV(0x001791b8) + static_cast<uint32_t>(t) * 4));
                sw(MS + 0xc, 5);
                sw(MS + 0xf0, lwu(MS + 4));
                sw(MS + 0xf4, 0);
                sw(MS + 0xe4, lhu(lwu(w + 0x34) + static_cast<uint32_t>(lw(w + 0x40) * 0xc) + 4));
                moby_sound_start(0, 0x11, lwu(w + 0x14));
                return 0;
            }
            case 7: mode3(3); return 0;
            case 8: mode3(4); return 0;
            case 10: mode3(6); return 0;
            case 11:
                sw(MS + 0xc, 7);
                sw(MS + 0xf0, lwu(MS + 4));
                sw(MS + 0xf4, 2);
                moby_sound_start(0, 0x11, lwu(w + 0x14));
                return 0;
            case 9: sw(0x0015ed88, lwu(e + 4)); return 0;
            default: break;
        }
    }
    const uint32_t items = lwu(w + 0x34);
    const int32_t old = lw(w + 0x40);
    int32_t count = 0;
    for (uint32_t p = items; lh(p) != 0; p += 0xc) ++count;
    const uint32_t flags = lwu(w + 0x30);
    const uint32_t b = (flags & 1) != 0 ? lwu(PAD + 0x1b4) : lwu(PAD + 0x1c4);
    if ((b & 0x1000) != 0 || ((flags & 0x100) != 0 && (b & 4) != 0)) {
        if (lw(w + 0x40) != 0) sw(w + 0x40, static_cast<uint32_t>(lw(w + 0x40) - 1));
        else if ((flags & 0x1000) != 0) sw(w + 0x40, static_cast<uint32_t>(count - 1));
        else sw(lwu(MS + 4) + 0x80, lwu(w + 0x38));
    }
    if ((b & 0x4000) != 0 || ((lwu(w + 0x30) & 0x100) != 0 && (b & 8) != 0)) {
        const int32_t s = lw(w + 0x40);
        const uint32_t next = lwu(w + 0x34) + static_cast<uint32_t>(s * 0xc) + 0xc;
        if (lh(next) != 0 && lh(next + 2) != 0) sw(w + 0x40, static_cast<uint32_t>(s + 1));
        else if ((lwu(w + 0x30) & 0x1000) != 0) sw(w + 0x40, 0);
        else sw(lwu(MS + 4) + 0x80, lwu(w + 0x3c));
    }
    if (lw(w + 0x40) == old && lwu(lwu(MS + 4) + 0x80) == 0) return 0;
    moby_sound_start(1, 0x11, lwu(w + 0x14));
    if ((lwu(w + 0x30) & 0x20) != 0) sw(OV(0x00184414), lwu(lwu(OV(0x001602a0)) + static_cast<uint32_t>(lw(w + 0x40)) * 4));
    level_slots_list_select(lwu(w + 0x34), lw(w + 0x40));  // enhancement: the ship's list's level rows (level_slots.h; else nothing)
    return 0;
}

// The menu's frame (mode 3): the closing countdown then the return to the screen chosen
// (MS+0xc: 1 the game, 2 the planet map, 3 .. 7 the mode-3 screens), the start of a state, the
// help screen, the cheat codes, a page change (12 frames of the mobys' animations), the
// widgets' updates and the focus change, the mobys and the sound.
// orig: 0x00276bd0
void menu_update() {
    const int32_t st0 = lw(MS);
    sw(MS + 0x110, lwu(MS + 0x110) + 1);
    // (1.02 Fixes, fix 16: the 1.02 build, 0x277a88, holds the counter at 32000)
    if (fixes102_on(16) && 0x7d00 < lw(MS + 0x110)) sw(MS + 0x110, 0x7d00);
    if (st0 == 2 || st0 == 0x1a || st0 == 0x1e || st0 == 0x1f) sw(OV(0x0015f5ac), 1);
    const int32_t st = lw(MS);
    if (st == 0x14) {
        const int32_t t = lw(MS + 0x14);
        if (t != 0) {
            sw(MS + 0x14, static_cast<uint32_t>(t - 1));
            if (t - 1 != 0) return;
        }
        if (lh(0x001516d8) != 0) return;
        sw(OV(0x0015f578), 0x1e000);
        sub_002a07c8();
        lq_sq(OV(0x00166dc0), MS + 0x40);
        lq_sq(OV(0x00166dd0), MS + 0x50);
        swc1(OV(0x0016caf0), lwc1(MS + 0xe8));
        moby_lists_swap(0);
        if (lwu(MS + 0x140) != 0) {
            sw(OV(0x0016000c), 0xffffffffu);
            item_class_load(lwu(MS + 0x140), lw(MS + 0x144));
        }
        fog_setup();
        sw(OV(0x0015f5d8), 1);
        sw(MS + 0x108, 0);
        sw(MS + 0x10c, 0);
        sw(MS + 0x104, 0);
        sw(MS + 0x10, 0);
        const int32_t kind = lw(MS + 0xc);
        auto to_screen = [](uint32_t eed8) {
            const uint32_t prev = lwu(0x0015eed8);
            if (eed8 != 0) sw(0x0015eed8, eed8);
            sw(MS + 0x130, prev);
            screen_fade(frame_scale_i(0x10));
            sw(OV(0x0015f5c4), 0);
        };
        if (kind == 2) {
            debug_printf_stub();
            planet_travel(lw(OV(0x00184414)));
        } else if (kind == 3) {
            to_screen(2);
            movie_play(lw(MS + 0xe4));
        } else if (kind == 4) {
            to_screen(1);
            movie_story_play(lw(MS + 0xe4));
        } else if (kind == 6) {
            to_screen(1);
            movie_table2_play(lw(MS + 0xe4));
        } else if (kind == 5) {
            to_screen(1);
            cutscene_start(lw(MS + 0xe4));
        } else if (kind == 7) {
            to_screen(0);
            credits_start_tr(0);  // (the credits)
        } else {
            sw(OV(0x0015f5c4), 0);
        }
        snd_cmd_17(0x1d);
        if (lw(MS + 0xc) != 2) music_volumes_default();
        snd_flush();
        sound_frame();
        return;
    }
    if (st == 0 || st == 0xa || st == 0xe || st == 0x11 || st == 0x21 || st == 0x2d || st == 0x23) menu_page_start();
    if ((lwu(0x0015eeb4) & 1) != 0 && !(lw(OV(0x0015f5c8)) < 8)) {
        screen_open(3, lwu(MS + 4));
        return;
    }
    cheat_code_input();
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
        if (cur == to && cur != OV(0x001b71f0) && cur != OV(0x001b7dd0) && cur != OV(0x001b80e0) && cur != OV(0x001b63f8) && cur != OV(0x001b78f0) && cur != OV(0x001b6820)) {
            if ((lw(MS) ^ 0x23) != 0) back = !back;
        }
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
        sw(MS, 1);
        sw(MS + 0x14, 0xc);
        sw(MS + 0xd0, lwu(MS + 4));
        sw(MS + 4, 0);
    }
    if (lwu(MS + 4) != 0) {
        const uint32_t slots = lwu(MS + 4) + 0x44;
        for (uint32_t i = 0; i < 0xe; ++i) {
            const uint32_t w = lwu(slots + i * 4);
            if (w == 0 || lwu(w) == 0) continue;
            if (menu_widget_call(lwu(w), w, 0) != 0) sw(MS + 0xc, static_cast<uint32_t>(lw(MS) - 0xf) > 1u ? 1u : 2u);
        }
        const uint32_t page = lwu(MS + 4);
        if (page != 0 && lwu(page + 0x80) != 0) {
            const uint32_t old = lwu(page + 0x40);
            if (lwu(old + 0xc) != 0) menu_widget_call(lwu(old + 0xc), old, 1);
            const uint32_t p = lwu(MS + 4);
            sw(p + 0x40, lwu(p + 0x80));
            sw(lwu(MS + 4) + 0x80, 0);
            const uint32_t nw = lwu(lwu(MS + 4) + 0x40);
            if (lwu(nw + 8) != 0) menu_widget_call(lwu(nw + 8), nw, 1);
        }
    }
    moby_update_visible();
    sound_frame();
    if (lw(MS + 0xc) != 0) menu_close();
}

// The world positions of `n` joints of moby m (their indices at `list`, words) into `out`
// (vectors): the joints of their chains marked in the mask 0x196e00 (the deepest + 1 at
// +0x7f, 0xff after it), the pose evaluated on the scratchpad (0x254108), each joint's
// translation row (+0x30 of its 0x40-byte matrix) copied.
// orig: 0x00253e48
void moby_joint_positions_local(uint32_t m, int32_t n, uint32_t list, uint32_t out) {
    constexpr OvConst<0x00196e00u> MASK{};
    const uint32_t chains = lwu(lwu(m + 0x24) + 0x1c);
    for (uint32_t o = 0; o < 0x80; o += 8) sd(MASK + o, 0);
    int32_t deepest = 0;
    int32_t k = 0;
    do {
        uint32_t c = lwu(chains + lwu(list + static_cast<uint32_t>(k) * 4) * 4 + 4);
        int32_t left = lhu(c);
        uint32_t j;
        do {
            j = lbu(c + 4);
            ++c;
            --left;
            sb(MASK + j, 1);
        } while (left > 0);
        sb(MASK + 0x80 + static_cast<uint32_t>(k), j);
        if (deepest < static_cast<int32_t>(j)) deepest = static_cast<int32_t>(j);
        ++k;
    } while (k < n);
    sb(MASK + 0x7f, static_cast<uint32_t>(deepest + 1));
    sb(MASK + static_cast<uint32_t>(deepest), 0xff);
    moby_pose_matrices_masked(m, MASK);
    k = 0;
    do {
        const uint32_t j = lbu(MASK + 0x80 + static_cast<uint32_t>(k));
        lq_sq(out + static_cast<uint32_t>(k) * 0x10, 0x70000000u + j * 0x40 + 0x30);
        ++k;
    } while (k < n);
}

// The same in the world: scaled by the moby's scale / 1024, rotated (+0xc0), moved (+0x10).
// orig: 0x0024f850
void moby_joint_positions(uint32_t m, int32_t n, uint32_t list, uint32_t out) {
    const float k = fpu_mul(lwc1(m + 0x2c), u2f(0x3a800000));
    moby_joint_positions_local(m, n, list, out);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t v = out + static_cast<uint32_t>(i) * 0x10;
        vec3_scale(V(v), V(v), k);
        mat3_transform(V(v), V(v), M(m + 0xc0));
        vec3_add(V(v), V(v), V(m + 0x10));
    }
}

// A menu moby's panel corners (the joints listed at 0x161ba0, into its data +0x78: +0 .. +0x30)
// and the panel's width / height (+0x40 / +0x44: the lengths of corner 1 and 2 from corner 0).
// orig: 0x002df690
void menu_moby_corners(uint32_t m) {
    const uint32_t d = lwu(m + 0x78);
    const uint32_t tmp = ee_stack_alloc(0x30);
    // (the list read by ldl / ldr pairs, 0x2df6b4..0x2df6c0: unaligned on the levels whose copy is not on 16 bytes, Kerwan's
    // 0x161c58 among them; an lq there read the quadword below it)
    sd(tmp + 0x20, ld(OV(0x00161ba0)));
    sd(tmp + 0x28, ld(OV(0x00161ba0) + 8));
    moby_joint_positions(m, 4, tmp + 0x20, d);
    vec3_sub(V(tmp), V(d + 0x10), V(d));
    vec3_sub(V(tmp + 0x10), V(d + 0x20), V(d));
    swc1(d + 0x40, vec3_length(V(tmp)));
    swc1(d + 0x44, vec3_length(V(tmp + 0x10)));
    ee_stack_free(0x30);
}

// The menu moby's update (+0x74): at its animation's end (+0x70 & 2) the speed +0x58 stopped
// (+0x54 1, or 0 when it played forward); its corners.
// orig: 0x002df638
void menu_moby_update(uint32_t m) {
    if ((lbu(m + 0x70) & 2) != 0) {
        const float sp = lwc1(m + 0x58);
        swc1(m + 0x54, 0.0f < sp ? 0.0f : 1.0f);
        swc1(m + 0x58, 0.0f);
    }
    menu_moby_corners(m);
}

// orig: (none) — the menu moby's update for the moby list's indirect calls.
void pause_menu_fns_register() {
    ovl_register_moby_fn(0x002df638, menu_moby_update);
    ovl_register_moby_fn(0x0027c2e0, menu_item_moby_update);
    ovl_register_moby_fn(0x0027be40, menu_ammo_moby_update);
    ovl_register_moby_fn(0x0027cd68, menu_stats_moby_update);
    ovl_register_moby_fn(0x002827a8, menu_moby_nop_a);
    ovl_register_moby_fn(0x002827b0, menu_moby_nop_b);
    ovl_register_moby_fn(0x002827b8, menu_weapon_moby_update);
    ovl_register_moby_fn(0x00282970, menu_item2_moby_update);
    ovl_register_moby_fn(0x00282a60, menu_item1_moby_update);
    ovl_register_moby_fn(0x00282c08, menu_item3_moby_update);
    ovl_register_moby_fn(0x00282d38, menu_gadget_moby_update);
    ovl_register_moby_fn(0x00282dc8, menu_orb_moby_update);
}

}  // namespace rac1
