// Ported overlay code: the Help page's sub-pages (see pause_menu_pages.h): the help log (the messages seen, their texts
// read from the disc
// into the menu's buffer and swapped in as the text table), the scrolling text lists, the
// skill / map pictures, the weapons / gadgets help lists.
#include <game/enhance/corrected_texts.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};    // the menu's state
constexpr uint32_t PAD = 0x0013c940;   // pad 1
constexpr OvConst<0x001ba1b8u> BUFS{};  // the menu's buffers
constexpr OvConst<0x00179ac0u> ITEMS{};
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// --- the help log ------------------------------------------------------------------------------

// The help log's list: the messages seen (0x15ee30 of them, their ids 0x141e08, last first), the
// text of each from the table 0x179452 (+id*4), the list 0x1b3d40 (12 bytes, ended by text 0).
// orig: 0x0027adb0
int32_t help_log_list_focus_in(uint32_t) {
    const int32_t n = lw(0x0015ee30);
    uint32_t e = OV(0x001b3d40);
    for (int32_t i = 0; i < n; ++i, e += 0xc) {
        sh(e + 2, 1);
        const uint32_t id = lbu(static_cast<uint32_t>(n - (i + 1)) + 0x00141e08);
        sh(e, lhu(id * 4 + OV(0x00179452)));
    }
    sh(static_cast<uint32_t>(n * 0xc) + OV(0x001b3d40), 0);
    return 0;
}

// orig: 0x0027ae38
int32_t help_log_list_focus_out(uint32_t) { return 0; }

// The help log's text panel focus in: the big buffers below M+0x10c taken, the help texts'
// file (0x137b80 + 0x1528) read into M+0x108 (+0x50: 1 reading, 3 failed), the panel hidden.
// orig: 0x0027ae40
int32_t help_log_text_focus_in(uint32_t w) {
    menu_vram_layout(1);
    sw(w + 0x54, 0);
    sw(w + 0x38, 0);
    for (int32_t i = 4; i >= 0; --i) {
        const uint32_t e = BUFS + static_cast<uint32_t>(i) * 8;
        const uint32_t a = lwu(e);
        if (a != 0 && a < lwu(MS + 0x10c)) sw(e + 4, lwu(e + 4) | 2);
    }
    sw(w + 0x50, 0);
    if (lh(0x001516d8) == 0) {
        const int32_t r = stream_disc_read(lwu(MS + 0x108), lwu(0x00137b80 + 0x1528), lwu(0x00137b80 + 0x152c));
        sw(w + 0x50, r != 0 ? 1u : 3u);
    }
    sw(w + 0x10, lwu(w + 0x10) | 4);
    return 0;
}

// The help log's text panel focus out: a read going stopped, the buffers laid out again, the
// game's text table (0x15f660, count 0x17943c) back.
// orig: 0x0027af10
int32_t help_log_text_focus_out(uint32_t w) {
    if (lh(0x001516d8) != 0 && lw(w + 0x50) == 1) disc_read_break();
    menu_vram_layout(1);
    const uint32_t t = lwu(w + 0x54);
    if (t != 0) {
        const uint32_t n = lwu(w + 0x38);
        sw(OV(0x0015f660), t);
        sw(OV(0x0017943c), n);
    }
    return 0;
}

// The help log's text panel update: the read retried (state 0); read (1): the language's
// section (0x15ed88) moved to the buffer's start, made the text table (0x15f660 / 0x17943c, the
// old ones kept in +0x54 / +0x38, its offsets relocated), the panel shown (state 2).
// orig: 0x0027af80
int32_t help_log_text_update(uint32_t w) {
    const int32_t st = lw(w + 0x50);
    if (st == 0) {
        if (lh(0x001516d8) != 0) return 0;
        const int32_t r = stream_disc_read(lwu(MS + 0x108), lwu(0x00137b80 + 0x1528), lwu(0x00137b80 + 0x152c));
        sw(w + 0x50, r != 0 ? 1u : 3u);
        return 0;
    }
    if (st != 1) return 0;
    if (lh(0x001516d8) != 0) return 0;
    const uint32_t buf = lwu(MS + 0x108);
    uint32_t src = buf + lwu(buf + static_cast<uint32_t>(lw(0x0015ed88)) * 4);
    const int32_t n = lw(src);
    src += 4;
    const int32_t size = ((lw(src) + 3) & -4) - 8;
    src += 4;
    mem_copy(buf, src, size);
    sw(w + 0x54, lwu(OV(0x0015f660)));
    sw(w + 0x38, lwu(OV(0x0017943c)));
    sw(OV(0x0017943c), static_cast<uint32_t>(n));
    const uint32_t base = lwu(MS + 0x108);
    sw(OV(0x0015f660), base);
    if (n > 0) {
        uint32_t p = base;
        for (int32_t i = 0; i < lw(OV(0x0017943c)); ++i, p += 0x10) sw(p, lwu(p) + base - 8);
    }
    corrected_texts_patch(base, n, lw(0x0015ed88));  // enhancement (off: nothing)
    sw(w + 0x10, lwu(w + 0x10) & ~4u);
    sw(w + 0x50, 2);
    return 0;
}

// A list of texts (entries 0xc bytes: text, active, +4 a level / argument, +8 a second text)
// in its box, scrolled (+0x44) to keep the selected one in view, coloured (selected / active /
// inactive; flag 2 all plain); flag 0x200 a check box for the cheats (0x13d408), 0x800 for the
// current level; fonts by flags 4 / 8, line height by 0x10. 1 (flag 0x8000 cleared) or 2.
// orig: 0x0027b0f0
int32_t text_list_draw(uint32_t w) {
    const bool focused = lwu(lwu(MS + 4) + 0x40) == w;
    uint32_t font = OV(0x001c3150);
    int32_t lh0 = 0xc, tex = 1;
    const uint32_t fl0 = lwu(w + 0x30);
    if ((fl0 & 4) != 0) {
        lh0 = 0xe;
        font = OV(0x001c3890);
        tex = 3;
    }
    if ((fl0 & 8) != 0) {
        lh0 = 0xa;
        font = OV(0x001c34f0);
        tex = 2;
    }
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x2004b);
    texture_block_begin(0);
    int32_t n = 0;
    for (uint32_t p = lwu(w + 0x34); lh(p) != 0; p += 0xc) ++n;
    int32_t step = lh0 + 3;
    if ((lwu(w + 0x30) & 0x10) == 0) step = ee_div(lw(w + 0x24), n + 1);
    const uint32_t sp = ee_stack_alloc(0x40);  // the box +0, its template +0x20
    const uint32_t tpl = sp + 0x20;
    libc_memset(tpl, 0, 0x18);
    sh(tpl + 2, lhu(w + 0x24) - 4u);
    sh(tpl, 4);
    sh(tpl + 6, lhu(w + 0x20) - 2u);
    sh(tpl + 0xa, static_cast<uint32_t>(step - lh0 / 2) - lhu(w + 0x44));
    sh(tpl + 0x10, static_cast<uint32_t>(lh0 + 2));
    for (uint32_t o = 0; o < 0x18; o += 2) sh(sp + o, lhu(tpl + o));
    const uint64_t t0 = effect_tex0(tex);
    if (lh(lwu(w + 0x34)) != 0) {
        uint32_t off = 0;
        int32_t i = 0;
        do {
            const bool sel = focused && lw(w + 0x40) == i;
            const bool active = lh(lwu(w + 0x34) + off + 2) != 0;
            uint64_t c;
            if ((lwu(w + 0x30) & 2) != 0) c = 0xffffffff80ffa888ull;
            else if (sel) c = active ? 0xffffffff8020ffffull : 0xffffffff80006060ull;
            else c = active ? 0xffffffff80ffa888ull : 0xffffffff80303030ull;
            if ((lwu(w + 0x30) & 0x10000) == 0 && sel && lh(sp + 0xa) < 4) sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) - 4));
            const uint32_t s = sub_00203ad8(lh(lwu(w + 0x34) + off));
            const uint32_t fl = lwu(w + 0x30);
            sh(sp + 8, (fl & 0xa00) != 0 ? 0x20u : 4u);
            if ((fl & 0x400) != 0) {
                sh(sp + 0x12, 1);
                sh(sp + 8, static_cast<uint32_t>(lw(w + 0x20) >> 1));
            }
            if (sel) text_colour_codes_off();
            text_box(sp, c, s, -1, t0, font);
            if (sel) sub_001fb028();
            if ((lwu(w + 0x30) & 0x200) != 0) mission_box(0xf, lh(sp + 0xa) + 9, lbu(0x0013d408 + static_cast<uint32_t>(i)) != 0 ? 1 : 0);
            if ((lwu(w + 0x30) & 0x800) != 0) mission_box(0xf, lh(sp + 0xa) + 9, lw(0x0015ed88) == lw(lwu(w + 0x34) + off + 4) ? 1 : 0);
            sh(sp + 0xa, lhu(sp + 0xa) + lhu(sp + 0xe));
            const int32_t sub = lh(lwu(w + 0x34) + off + 8);
            if (sub != 0) {
                const uint32_t s2 = sub_00203ad8(sub);
                sh(sp + 8, 0x14);
                text_box(sp, c, s2, -1, t0, font);
                sh(sp + 0xa, lhu(sp + 0xa) + static_cast<uint32_t>(step));
            }
            const uint32_t y = lhu(sp + 0xa) + 8u;
            sh(sp + 0xa, y);
            const uint32_t f2 = lwu(w + 0x30);
            if ((f2 & 0x10000) == 0 && sel) {
                const int32_t bottom = static_cast<int16_t>(y) + static_cast<int16_t>(lhu(sp + 0xe));
                const int32_t top = static_cast<int16_t>(lhu(sp + 2));
                if (top < bottom) {
                    if ((f2 & 0x8000) != 0) sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) + (bottom - top)));
                    else sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) + 4));
                }
            }
            off += 0xc;
            ++i;
        } while (lh(lwu(w + 0x34) + off) != 0);
    }
    ee_stack_free(0x40);
    texture_block_end();
    const uint32_t f = lwu(w + 0x30);
    if ((f & 0x8000) != 0) {
        sw(w + 0x30, f ^ 0x8000);
        return 1;
    }
    return 2;
}

// --- two pictures (the skill points page) -------------------------------------------------------

// Their reads: state 0 the level's first picture (0x137b80 + level*8 + 0x2c8) into +0x48, 2 the
// second (+0x2f8) into +0x4c, 1 / 3 waiting, 4 shown; -1 failed.
// orig: 0x0027e290
int32_t two_pictures_update(uint32_t w) {
    const int32_t st = lw(w + 0x44);
    if (st == 1) {
        if (lh(0x001516d8) != 0) return 0;
        sw(w + 0x44, 2);
        return 0;
    }
    if (st == 3) {
        if (lh(0x001516d8) != 0) return 0;
        sw(w + 0x44, 4);
        return 0;
    }
    if (st != 0 && st != 2) return 0;
    const uint32_t buf = lwu(w + (st == 0 ? 0x48 : 0x4c));
    if (buf == 0) return 0;
    if (lh(0x001516d8) != 0) return 0;
    const uint32_t e = 0x00137b80 + (static_cast<uint32_t>(lw(0x0015ed88)) << 3) + (st == 0 ? 0x2c8u : 0x2f8u);
    const uint32_t n = lwu(e + 4);
    if (n == 0) return 0;
    if (stream_disc_read(buf, lwu(e), n) == 0) {
        sw(w + 0x44, 0xffffffffu);
        return 0;
    }
    sw(w + 0x44, static_cast<uint32_t>(lw(w + 0x44) + 1));
    return 0;
}

// The two pictures side by side (256 x 256 each), once both are read.
// orig: 0x0027e3d8
int32_t two_pictures_draw(uint32_t w) {
    if (lw(w + 0x44) < 4) return 0;
    texture_block_begin(0);
    glyph_sprite(0, 0, 0x100, 0x100, 0, 0, 0x100, 0x100, 0x80808080ull, image_upload(lwu(w + 0x48)));
    glyph_sprite(0x100, 0, 0x100, 0x100, 0, 0, 0x100, 0x100, 0x80808080ull, image_upload(lwu(w + 0x4c)));
    texture_block_end();
    return 8;
}

// The items help's label: its texts by whether item 2 is owned (0x13d4c2).
// The levels' own copies (the same instructions, their own tables; handed over through the code pointer pairs) run this
// port: proved on each level by batch 411.
// orig: 0x0027a6e0 L1:0x002904a0 L2:0x0027bbf8 L3:0x00269350 L4:0x0026da10 L5:0x002a49e8 L6:0x002894c0 L7:0x002a3100 L8:0x00284e30 L9:0x00299500 L10:0x0026d600 L11:0x0029fa68 L12:0x00293388 L13:0x00286860 L14:0x00282e70 L15:0x0026a388 L16:0x002759d8 L17:0x00274790 L18:0x0027c668
int32_t items_help_focus_in(uint32_t w) {
    sw(w + 0x34, lbu(0x0013d4c2) != 0 ? OV(0x001b47d0u) : OV(0x001b4808u));
    return 0;
}

// --- the weapons / gadgets help: a vertical list of icons ---------------------------------------
// +0x3c the selected one, +0x40 their count, +0x48 their cells (10 bytes: HUD frame key / index,
// kind, item, place), +0x5c the scroll (1/16 pixels), +0x60 the first shown.

// orig: 0x00277a58
int32_t icon_list_update(uint32_t w) {
    if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) return 1;
    if ((lwu(PAD + 0x1c4) & 0x10) != 0) {
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) sw(MS + 8, parent);
        else if (lw(MS + 0x124) == 0) return -1;
    }
    const uint32_t k = lwu(PAD + 0x1c4);
    const int32_t old = lw(w + 0x3c);
    if ((k & 0x1000) != 0 && old != 0) sw(w + 0x3c, static_cast<uint32_t>(old - 1));
    if ((k & 0x4000) != 0) {
        const int32_t v = lw(w + 0x3c) + 1;
        if (v < lw(w + 0x40)) sw(w + 0x3c, static_cast<uint32_t>(v));
    }
    if (lw(w + 0x3c) != old) moby_sound_start(1, 0x11, lwu(w + 0x14));
    const int32_t h = lw(w + 0x24) << 4;
    if (!(ee_div(h, 0x260) < lw(w + 0x40))) {
        sw(w + 0x5c, 0x60);
        return 0;
    }
    int32_t rows = ee_div(h - 0x28, 0x260);
    const int32_t sel = lw(w + 0x3c);
    if (!(lw(w + 0x60) < sel)) {
        const int32_t v = sel - 1;
        sw(w + 0x60, static_cast<uint32_t>(v));
        if (v < 0) sw(w + 0x60, 0);
    }
    rows -= 2;
    const int32_t top = lw(w + 0x3c) - rows;
    if (lw(w + 0x60) < top) sw(w + 0x60, static_cast<uint32_t>(top));
    sw(w + 0x5c, static_cast<uint32_t>(0x190 - lw(w + 0x60) * 0x260));
    return 0;
}

// The icons (32 x 32, 0x260 apart), the selected one on a pulsing frame; arrows (frame 0xe99e 6)
// over bars when scrolled past the top / bottom.
// orig: 0x00277be8
int32_t icon_list_draw(uint32_t w) {
    int32_t y = lw(w + 0x5c);
    const int32_t y0 = y;
    const int32_t x = ((lw(w + 0x20) << 4) - 0x200) >> 1;
    texture_block_begin(0);
    if (lw(w + 0x40) > 0) {
        uint32_t off = 0;
        for (int32_t i = 0; i < lw(w + 0x40); ++i, off += 10, y += 0x260) {
            const uint32_t cell = lwu(w + 0x48) + off;
            if (lw(w + 0x3c) == i) {
                const int32_t a = abs_i(static_cast<int32_t>(lwu(OV(0x0015f3f8)) & 0x3f) - 0x20);
                const uint32_t col = (static_cast<uint32_t>(a + 0x40) * 0x10202u) | 0x80000000u;
                gs_flat_rect(x - 0x30, y - 0x30, x + 0x230, y + 0x230, col, 1);
                gs_flat_rect(x - 0x10, y - 0x10, x + 0x210, y + 0x210, sx64(lw(OV(0x00160270))), 1);
            }
            hud_sprite_xyz(hud_frame(lhu(cell), lh(cell + 2)), x, y, 0x200, 0x200, 0x80);
        }
    }
    if (y0 < 0) {
        gs_flat_rect(0, 0, lw(w + 0x20), 0x14, sx64(lw(OV(0x00160270))), 0);
        hud_sprite(hud_frame(0xe99e, 6), x >> 4, 2, 0x20, 0x10, 0x80);
    }
    const int32_t h = lw(w + 0x24);
    if ((h << 4) < y) {
        gs_flat_rect(0, h - 0x14, lw(w + 0x20), h, sx64(lw(OV(0x00160270))), 0);
        hud_sprite_flip(hud_frame(0xe99e, 6), x >> 4, lw(w + 0x24) - 0x12, 0x20, 0x10, 0x80);
    }
    texture_block_end();
    return 2;
}

// The weapons help's list: the owned weapons of the grid 0x1b3220 (15 cells) with their icon
// (+0x38 of the item table; +4 gold) and description (+0x40, gold +0x42) into 0x1ba1e0 / 0x1ba278.
// orig: 0x0027f240
int32_t weapons_help_focus_in(uint32_t) {
    int32_t n = 0;
    uint32_t cell = OV(0x001ba1e0), text = OV(0x001ba278);
    for (int32_t i = 0; i < 0xf; ++i) {
        const int32_t item = lh(OV(0x001b3226) + static_cast<uint32_t>(i) * 10);
        if (lbu(static_cast<uint32_t>(item) + 0x0013d4c0) == 0) continue;
        const uint32_t e = ITEMS + static_cast<uint32_t>(item * 0x4c);
        const uint32_t gold = lbu(static_cast<uint32_t>(item) + 0x0013e520);
        sh(cell + 6, static_cast<uint32_t>(item));
        sh(cell + 4, 0);
        sh(cell + 2, gold != 0 ? 4u : 0u);
        sh(cell, lhu(e + 0x38));
        sh(cell + 8, static_cast<uint32_t>(i));
        sw(text, static_cast<uint32_t>(gold != 0 ? lh(e + 0x42) : lh(e + 0x40)));
        text += 4;
        cell += 10;
        ++n;
    }
    sw(OV(0x001b4e00) + 0x40, static_cast<uint32_t>(n));
    return 0;
}

// The gadgets help's list: the owned items of the four gadget grids (0x1b2d20 / 0x1b2d60 /
// 0x1b2d80 / 0x1b2da0: 6, 3, 3, 2 cells) with their icon, description (+0x40) and +0x44 into
// 0x1ba2b8 / 0x1ba348 / 0x1ba380.
// orig: 0x0027f308
int32_t gadgets_help_focus_in(uint32_t) {
    int32_t n = 0;
    uint32_t cell = OV(0x001ba2b8), a = OV(0x001ba348), b = OV(0x001ba380);
    for (int32_t i = 0; i < 0xe; ++i) {
        uint32_t src;
        if (i < 6) src = OV(0x001b2d26);
        else if (i < 9) src = OV(0x001b2d2a);
        else if (i < 0xc) src = OV(0x001b2d2c);
        else src = OV(0x001b2d2e);
        const int32_t item = lh(src + static_cast<uint32_t>(i) * 10);
        if (lbu(static_cast<uint32_t>(item) + 0x0013d4c0) == 0) continue;
        ++n;
        const uint32_t e = ITEMS + static_cast<uint32_t>(item * 0x4c);
        sh(cell, lhu(e + 0x38));
        sh(cell + 6, static_cast<uint32_t>(item));
        sh(cell + 2, 0);
        sh(cell + 4, 0);
        sw(a, static_cast<uint32_t>(lh(e + 0x40)));
        sw(b, static_cast<uint32_t>(lh(e + 0x44)));
        a += 4;
        sh(cell + 8, static_cast<uint32_t>(i));
        b += 4;
        cell += 10;
    }
    sw(OV(0x001b5188) + 0x40, static_cast<uint32_t>(n));
    return 0;
}

}  // namespace rac1
