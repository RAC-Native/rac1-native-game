// Ported overlay code: the missions page of the planet map (menu page 0x1b3778, state 0xc,
// opened with cross on the map; see planet_map.h): the planet's
// goals listed (0x24deb8), the selected one's text and picture, the wrapped-text renderer.
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <host/level_slots.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001841f0u> MAP{};
constexpr OvConst<0x001b9cf0u> MS{};   // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1
constexpr uint32_t SPR = 0x70000000;  // the scratchpad: the goals' texts +0, their pictures +0x100
uint32_t focused() { return lwu(lwu(MS + 4) + 0x40); }
}  // namespace

// The goals of the planet shown (the records *0x186d20, 0x28 bytes, ended by text 0; +0x24 the
// state: 0 hidden, 2 done; +0x10 flags 1 hide when done, 2 skip): their texts to `texts` (with
// `alt`: the current step's text +0x14, "done" 0x523e), bit i of *`done` for the done ones (bit
// 31 when all are), their pictures (+0x12 + step) to `pics`; their count.
// orig: 0x0024deb8
int32_t missions_list(uint32_t texts, uint32_t done, uint32_t pics, int32_t alt) {
    int32_t n = 0;
    uint32_t r = lwu(OV(0x00186d20));
    bool all = true;
    sw(texts, 0);
    if (done != 0) sw(done, 0);
    if (pics != 0) sw(pics, 0xffffffffu);
    if (r == 0) return 0;
    if (lh(r) != 0) {
        do {
            const int32_t st = lh(r + 0x24);
            const uint32_t fl = lhu(r + 0x10);
            if (st != 2 && (fl & 2) == 0) all = false;
            if ((fl & 2) == 0 && st != 0 && !((fl & 1) != 0 && st == 2)) {
                sw(texts, static_cast<uint32_t>(lh(r)));
                if (alt != 0) sw(texts, static_cast<uint32_t>(lh(r + 0x24) == 2 ? TXT(0x523e) : lh(r + 0x14 + static_cast<uint32_t>(lh(r + 0x26) << 1))));
                if (done != 0 && lh(r + 0x24) == 2) sw(done, lwu(done) | (1u << (n & 31)));
                texts += 4;
                if (pics != 0) {
                    sw(pics, static_cast<uint32_t>(lh(r + 0x12) + lh(r + 0x26)));
                    pics += 4;
                }
                ++n;
            }
            r += 0x28;
        } while (lh(r) != 0);
    }
    if (done != 0 && all) sw(done, lwu(done) | 0x80000000u);
    return n;
}

// Text in a box from (x, y), `w` wide, down to y + h, wrapped at spaces (words wider than the
// line split), colour codes 8..15 (0x16c838) when enabled (0x15f45c), the font `font`'s glyphs
// (accented ones 0x80..0xa7 with their accent +0x40, two-wide ones below 0x20); the height used.
// orig: 0x001fbb70
int32_t text_wrapped(int32_t x, int32_t y, int32_t w, int32_t h, uint64_t colour, uint32_t s, int32_t n, uint64_t tex0, uint32_t font) {
    uint64_t c = colour;
    sw(OV(0x0016c838), static_cast<uint32_t>(colour));
    int32_t px = x, py = y;
    int32_t i = 0;
    if (n == 0 || lbu(s) == 0) return py - y + 0x10;
    const int32_t bottom = y + h;
    if (bottom < py + 0x10) return py - y + 0x10;
    for (;;) {
        const uint32_t p = s + static_cast<uint32_t>(i);
        float wd = 0.0f;
        const float fx = static_cast<float>(px);
        const int32_t right = x + w;
        bool code = false;
        if (i != n) {
            const uint32_t ch = lbu(p);
            if (ch != 0x20) {
                if (ch < 0x10) {
                    code = true;
                } else {
                    uint32_t a = p;
                    int32_t k = i;
                    for (;;) {
                        const uint32_t g = lbu(a);
                        ++k;
                        ++a;
                        wd = fpu_add(wd, static_cast<float>(static_cast<int8_t>(lbu(font + g * 4 + 3))));
                        if (k == n) break;
                        const uint32_t nx = lbu(a);
                        if (nx == 0x20 || nx < 0x10) break;
                    }
                }
            }
        }
        const float end = code ? fx : fpu_add(fx, wd);
        if (static_cast<float>(right) < end) {
            const uint32_t ch = lbu(p);
            if (ch != 0x20 && !(ch < 0x10)) --i;
            px = x;
            py += 0x10;
        } else {
            const uint32_t ch = lbu(p);
            if (ch - 8u < 8u) {
                if (lw(OV(0x0015f45c)) != 0) c = (c & 0xff000000ull) | (lwu(OV(0x0016c838) + (ch - 8u) * 4) & 0xffffffu);
            } else if (static_cast<int8_t>(lbu(font + ch * 4 + 3)) != 0) {
                if (((ch + 0x80u) & 0xffu) < 0x28u) {
                    const uint32_t e = font + (ch + 0x40u) * 4;
                    glyph_sprite(px + static_cast<int8_t>(lbu(e + 3)), py + static_cast<int8_t>(lbu(e + 2)), 0x10, 0x10, lbu(e), lbu(e + 1), 0x10, 0x10, c, tex0);
                }
                const uint32_t ch2 = lbu(p);
                const uint32_t e = font + ch2 * 4;
                if (ch2 < 0x20) glyph_sprite(px, py + static_cast<int8_t>(lbu(e + 2)), 0x18, 0x10, lbu(e), lbu(e + 1), 0x18, 0x10, c, tex0);
                else if (!(ch2 < 0x21)) glyph_sprite(px, py + static_cast<int8_t>(lbu(e + 2)), 0x10, 0x10, lbu(e), lbu(e + 1), 0x10, 0x10, c, tex0);
                px += static_cast<int8_t>(lbu(font + lbu(p) * 4 + 3));
            }
        }
        ++i;
        if (i == n) break;
        if (lbu(s + static_cast<uint32_t>(i)) == 0) break;
        if (bottom < py + 0x10) break;
    }
    return py - y + 0x10;
}

// Wrapped text in the medium font (TEX0 2, font 0x1c34f0).
// orig: 0x001fbe88
int32_t text_wrapped_med(int32_t x, int32_t y, int32_t w, int32_t h, uint64_t colour, uint32_t s, int32_t n) {
    return text_wrapped(x, y, w, h, colour, s, n, effect_tex0(2), OV(0x001c34f0));
}

// A goal's check box at (x, y), ticked (HUD frame 0xe99e 1) when done.
// orig: 0x0027d530
void mission_box(int32_t x, int32_t y, int32_t done) {
    gs_flat_rect(x - 5, y - 5, x + 5, y + 5, 0x80ffa888ull, 0);
    gs_flat_rect(x - 4, y - 4, x + 4, y + 4, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00160270)))), 0);
    if (done != 0) hud_sprite(hud_frame(0xe99e, 1), x - 0xd, y - 0x12, 0x1e, 0x1e, 0x80);
}

// The goals list's focus in: the goals evaluated and listed, the selected one's (+0x30 + planet
// * 4) text and picture to the page's other widgets (0x1b3974, 0x1b3858).
// orig: 0x0027a068
int32_t missions_focus_in(uint32_t w) {
    goals_update();
    sw(w + 0x7c, static_cast<uint32_t>(missions_list(SPR, 0, SPR + 0x100, 1)));
    const uint32_t e = w + 0x30 + (lwu(OV(0x00184414)) << 2);
    if (lw(e) == -1) return 0;
    sw(OV(0x001b3974), lwu(SPR + (lwu(e) << 2)));
    sw(OV(0x001b3858), lwu(SPR + 0x100 + (lwu(e) << 2)));
    return 0;
}

// The goals list: not focused, its selection for the planet forgotten; the exit keys; L1 / R1
// the previous / next planet with goals (0x13dd40) or the current level; up / down the goal
// (wrapping); a change lists the goals again and shows the selected one's text and picture.
// orig: 0x0027a108
int32_t missions_update(uint32_t w) {
    const int32_t was = lw(MAP + 0x224);
    if (focused() != w) {
        if (was < 0x14) sw(w + 0x30 + static_cast<uint32_t>(was) * 4, 0xffffffffu);
        return 0;
    }
    if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) return 1;
    if ((lwu(PAD + 0x1c4) & 8) != 0) {
        const int32_t lv = lw(0x0015ed84);
        int32_t p = lw(MAP + 0x224) + 1;
        while (p < 0x14) {
            if (lbu(0x0013dd40 + static_cast<uint32_t>(p)) != 0 || lv == p) {
                sw(MAP + 0x224, static_cast<uint32_t>(p));
                break;
            }
            ++p;
        }
    }
    if ((lwu(PAD + 0x1c4) & 4) != 0) {
        const int32_t lv = lw(0x0015ed84);
        int32_t p = lw(MAP + 0x224) - 1;
        while (!(p < 0)) {
            if (lbu(0x0013dd40 + static_cast<uint32_t>(p)) != 0 || lv == p) {
                sw(MAP + 0x224, static_cast<uint32_t>(p));
                break;
            }
            --p;
        }
    }
    if (lw(MAP + 0x224) != was) {
        moby_sound_start(1, 0x11, lwu(w + 0x14));
        goals_update();
    }
    const int32_t n = lw(w + 0x7c);
    if (n != 0) {
        const uint32_t k = lwu(PAD + 0x1c4);
        const uint32_t sel = w + 0x30 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4;
        const int32_t old = lw(sel);
        if ((k & 0x1000) != 0) sw(sel, static_cast<uint32_t>(ee_rem(old + n - 1, n)));
        if ((k & 0x4000) != 0) {
            const uint32_t s2 = w + 0x30 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4;
            sw(s2, static_cast<uint32_t>(ee_rem(lw(s2) + 1, lw(w + 0x7c))));
        }
        if (lw(w + 0x30 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4) != old) moby_sound_start(1, 0x11, lwu(w + 0x14));
        if ((k & 0x5000) != 0 || lw(MAP + 0x224) != was) {
            sw(w + 0x7c, static_cast<uint32_t>(missions_list(SPR, 0, SPR + 0x100, 1)));
            const uint32_t e = w + 0x30 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4;
            sw(OV(0x001b3974), lwu(SPR + (lwu(e) << 2)));
            sw(OV(0x001b3858), lwu(SPR + 0x100 + (lwu(e) << 2)));
        }
    }
    if ((lwu(PAD + 0x1c4) & 0x10) != 0) {
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) sw(MS + 8, parent);
        else if (lw(MS + 0x124) == 0) return -1;
    }
    return 0;
}

// The goals list: its title (0x4f59), each goal's text wrapped (the selected one highlighted)
// with its check box; all done, a text (0x523d) under them.
// orig: 0x0027d2d0
int32_t missions_draw(uint32_t w) {
    if (level_slots_map_row()) return 2;  // enhancement: a level mod's row on the ship's map: no missions (the base level's)
    const uint32_t sp = ee_stack_alloc(0x50);  // a box +0, its template +0x20, the done bits +0x40
    sw(sp + 0x40, 0);
    texture_block_begin(0);
    const int32_t n = missions_list(SPR, sp + 0x40, 0, 0);
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x10b);
    text_small(4, 4, 0x80ffa888ull, sub_00203ad8(0x4f59), -1);
    // region: the PAL builds (1.00 and 2.00; the same code, 2.00's text ids 5 higher) draw each goal in a text box (the
    // box +0, made once from its template +0x20) and take its height from the lines the box wrapped it into, where the
    // North American build takes one line for a short goal and two for any other (a third line of a long translated goal
    // overlapped the next goal). The PAL discs only (their region flag); 1.00 0x0027ccfc..0x0027ce3c.
    const bool pal = lw(0x0015ee90) != 0;
    if (pal) {
        const uint32_t tpl = sp + 0x20;
        libc_memset(tpl, 0, 0x18);
        sh(tpl + 2, lhu(w + 0x24));
        sh(tpl + 6, lhu(w + 0x20));
        sh(tpl + 0x10, 0x10);
        for (uint32_t o = 0; o < 0x18; o += 2) sh(sp + o, lhu(tpl + o));
    }
    int32_t y = 0x18;
    if (n > 0) {
        uint32_t t = SPR;
        for (int32_t i = 0; i < n; ++i, t += 4) {
            uint64_t c = 0xffffffff80ffa888ull;
            const int32_t ybox = y + 0xa;
            const bool sel = lw(w + 0x30 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4) == i;
            if (sel) {
                c = 0xffffffff8020ffffull;
                text_colour_codes_off();
            }
            int32_t lines;
            if (pal) {
                sh(sp + 0xa, static_cast<uint32_t>(y));
                sh(sp + 8, 0x10);
                text_box_draw(sp, c, sub_00203ad8(lw(t)), -1);
                if (sel) sub_001fb028();
                // (the box's height +0xe over its line height +0x10: the lines)
                lines = ee_div(static_cast<int16_t>(lhu(sp + 0xe)), lh(sp + 0x10));
            } else {
                const int32_t h = text_wrapped_med(0x10, y, lw(w + 0x20) - 0x11, 0x3e8, c, sub_00203ad8(lw(t)), -1);
                if (sel) sub_001fb028();
                lines = h < 0x11 ? 1 : 2;
            }
            mission_box(9, lines != 2 ? ybox : y + 0x10, (lw(sp + 0x40) >> (i & 31)) & 1);
            y += lines << 4;
        }
    }
    if (lw(sp + 0x40) < 0 && pal) {
        // region: (the PAL builds) the same box, centred under the goals
        sh(sp + 0x12, 1);
        sh(sp + 0xa, static_cast<uint32_t>(y + 8));
        sh(sp + 8, static_cast<uint32_t>(lw(w + 0x24) >> 1));
        text_box_draw(sp, 0x8020ffffull, sub_00203ad8(TXT(0x523d)), -1);
    } else if (lw(sp + 0x40) < 0) {
        const uint32_t tpl = sp + 0x20;
        libc_memset(tpl, 0, 0x18);
        sh(tpl + 2, lhu(w + 0x24));
        sh(tpl + 0xa, static_cast<uint32_t>(y + 8));
        sh(tpl + 0x10, 0x10);
        sh(tpl + 0x12, 1);
        sh(tpl + 6, lhu(w + 0x20));
        sh(tpl + 8, static_cast<uint32_t>(lw(w + 0x24) >> 1));
        for (uint32_t o = 0; o < 0x18; o += 2) sh(sp + o, lhu(tpl + o));
        text_box_draw(sp, 0x8020ffffull, sub_00203ad8(TXT(0x523d)), -1);
    }
    texture_block_end();
    ee_stack_free(0x50);
    return 2;
}

// The goal's picture panel (+0x30 the pictures' (sector, count), +0x34 flags as 0x27da10's, +0x44
// state 0 / 2 read into the buffer +0x48, 1 / 3 unpacked and uploaded (0x245b60) as the map's
// TEX0 +0x258), the image +0x50.
// orig: 0x0027d5d8
int32_t mission_picture_update(uint32_t w) {
    const uint32_t fl = lwu(w + 0x34);
    if ((fl & 3) == 2 && level_slots_map_picture(w)) return 0;  // enhancement: a level mod's row: its picture (src/host/level_slots.h)
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
        } else {
            sw(w + 0x44, 0xffffffffu);
        }
    } else {
        img = lw(focused() + 0x40);
        if (!(-1 < img)) img = 0;
    }
    const int32_t st = lw(w + 0x44);
    if (st == 1 || st == 3) {
        if (lh(0x001516d8) != 0) return 0;
        menu_buf_ready(lwu(w + 0x48));
        if ((lwu(w + 0x34) & 0x20) != 0) {
            wad_decompress(lwu(w + 0x48) + lwu(w + 0x60), lwu(w + 0x48));
            sw(w + 0x60, 0);
        }
        const uint32_t hdr = lwu(w + 0x48);
        const int32_t lw_ = highest_bit_index(lw(hdr + 8));
        const int32_t lh_ = highest_bit_index(lw(hdr + 0xc));
        const uint64_t t = image_upload_at(lw_, lh_, hdr + 0x20, hdr + 0x420, lw(MAP + 0x244), lw(MAP + 0x248));
        sd(MAP + 0x258, t);
        texture_uploads();
        sw(w + 0x44, 2);
        return 0;
    }
    if (st != 0 && st != 2) return 0;
    if (img == lw(w + 0x50)) return 0;
    const uint32_t buf = lwu(w + 0x48);
    if (buf == 0) return 0;
    if (lh(0x001516d8) != 0) return 0;
    const uint32_t ent = static_cast<uint32_t>(img) * 8 + lwu(w + 0x30);
    if (lwu(ent + 4) == 0) return 0;
    uint32_t dst = buf;
    if ((lwu(w + 0x34) & 0x20) != 0) {
        const int32_t off = menu_buf_size(buf) - static_cast<int32_t>(lwu(ent + 4) << 11);
        sw(w + 0x60, static_cast<uint32_t>(off));
        dst += static_cast<uint32_t>(off);
    }
    if (stream_disc_read(dst, lwu(ent), lwu(ent + 4)) == 0) {
        sw(w + 0x44, 0xffffffffu);
        return 0;
    }
    menu_buf_busy(lwu(w + 0x48));
    const int32_t s = lw(w + 0x44);
    sw(w + 0x50, static_cast<uint32_t>(img));
    sw(w + 0x44, static_cast<uint32_t>(s + 1));
    return 0;
}

// The goal's picture (the map's TEX0 +0x258) over the whole target once uploaded.
// orig: 0x0027d8b0
int32_t mission_picture_draw(uint32_t w) {
    if (lw(w + 0x44) < 2) return 0;
    glyph_sprite(0, 0, lh(0x00151780 + 0x160), lh(0x00151780 + 0x162), 0, 0, lw(w + 0x38), lw(w + 0x3c), 0x80808080ull, ld(MAP + 0x258));
    return 0x10;
}

}  // namespace rac1
