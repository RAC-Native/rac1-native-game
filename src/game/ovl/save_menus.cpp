// Ported overlay code: the full-screen menu's save slot widgets. Five slots (the
// card's record of port 0 has their summaries at 0x13d2b0, 0x1c bytes each: +0 the level (-1 an
// empty slot), +4 the bolts, +8 the challenge mode, +0xc the time played (frames), +0x10 the
// clock (8 BCD bytes)), chosen with up / down (0x15ee34 the last one), cross takes it: the pause
// menu's Save page (0x280bd0: the game saved, asking first over a used slot), its Load page
// (0x280f20: the save read, then the level left for the saved one) and the main menu's New
// Game page (0x281200: a new game, or the challenge mode's new game keeping the items, saved
// into the slot, then Veldin). The drawing: the slot list (0x281628) and the focused slot's
// planet (0x2809e0).
#include <game/ovl/save_menus.h>

#include <game/lib/libc.h>
#include <game/lib/libcdvd.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/memory_card_ops.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <host/quick_rings.h>

#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t S = 0x0013d290;     // the save block (port 0's record first)
constexpr uint32_t PAD = 0x0013c940;   // pad 1
constexpr uint32_t FLAGS = 0x0015eeb4; // the menus' card flags
constexpr uint32_t LAST = 0x0015ee34;  // the slot last chosen
constexpr OvConst<0x001b9cf0u> MS{};   // the full-screen menu's state

uint32_t summary(int32_t n) { return S + static_cast<uint32_t>(n * 0x1c); }  // orig: (helper) a slot's summary (+0x20: its level)
bool card_ready() { return lw(S + 0xd4) < 3 && lw(S + 0xdc) < 0; }           // orig: (helper) repeated in 0x00280bd0 / 0x00280f20 / 0x00281200

// The slot saved (+0x14) gets the game's summary: the level, the bolts, the challenge mode, the
// time played and the clock.
// orig: (helper) repeated in 0x00280bd0 / 0x00281200
void summary_write() {
    const uint32_t r = summary(lw(S + 0x14));
    sw(r + 0x24, lwu(0x0015ed98));
    sw(r + 0x20, lwu(0x0015ed84));
    sw(r + 0x2c, lwu(0x0015ee24));
    for (uint32_t i = 0; i < 8; ++i) sb(r + 0x30 + i, lbu(0x0015ee98 + i));  // (ldl / ldr, sdl / sdr)
    sw(r + 0x28, lwu(0x0015ee20));
}

// The page's exit keys (as the other list widgets): L1 / R1 / start (0xd00) leave the menu
// (1) unless it stays open (+0x124); triangle (0x10) goes to the page's parent (+0x38) or, with
// none and the menu staying open, on; then with the card's state 0x15eeb0 neither 0x10 nor 1
// the parent (0). `out` the value to return when the slot keys are not reached.
// orig: (helper) repeated in 0x00280bd0 / 0x00280f20 / 0x00281200
bool exit_keys(int32_t& out) {
    if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) {
        out = 1;
        return true;
    }
    if ((lwu(PAD + 0x1c4) & 0x10) != 0) {
        const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
        if (parent != 0) {
            sw(MS + 8, parent);
        } else if (lw(MS + 0x124) == 0) {
            out = -1;
            return true;
        }
    }
    const int32_t h = lw(0x0015eeb0);
    if (h != 0x10 && h != 1) {
        sw(MS + 8, lwu(lwu(MS + 4) + 0x38));
        out = 0;
        return true;
    }
    if (!card_ready() || lw(S + 8) != 2) {
        out = 0;
        return true;
    }
    return false;
}

// Up / down over the five slots (from the last one chosen); the slot to 0x15ee34; the pad's
// pressed (`pressed`) or repeated keys (+0x30 & 1).
// orig: (helper) repeated in 0x00280bd0 / 0x00280f20 / 0x00281200
uint32_t slot_keys(uint32_t w, uint32_t pressed) {
    const uint32_t keys = (lwu(w + 0x30) & 1) != 0 ? lwu(PAD + 0x1b4) : lwu(PAD + pressed);
    const int32_t last = lw(LAST);
    sw(w + 0x40, static_cast<uint32_t>(last));
    if ((keys & 0x1000) != 0 && last != 0) sw(w + 0x40, static_cast<uint32_t>(last - 1));
    if ((keys & 0x4000) != 0) {
        const int32_t n = lw(w + 0x40);
        if (n < 4) sw(w + 0x40, static_cast<uint32_t>(n + 1));
    }
    sw(LAST, lwu(w + 0x40));
    return keys;
}
}  // namespace

// The pause menu's Save page: a slot chosen (cross) saves the game there (0x284808; an used slot
// asks first: page 0x1b6678, whose answer +0x84 brings it back with +0x4c 1); while the card
// works M+0x128 is set; done: an error shows the card's screens (flag 0x80), else the slot's
// summary is the game's.
// orig: 0x00280bd0
int32_t save_slots_update(uint32_t w) {
    if (lwu(lwu(MS + 4) + 0x40) != w) return 0;
    if (lw(w + 0x4c) == 0 && lwu(MS + 0xd0) == OV(0x001b6678) && lw(lwu(MS + 0xd0) + 0x84) != 0) sw(w + 0x4c, 1);
    if (lw(w + 0x4c) == 1) {
        save_game_image(lwu(w + 0x48), lw(w + 0x40));
        sw(MS + 0x12c, 0x4fb5);
        sw(MS + 0x128, 1);
    }
    sw(w + 0x4c, 2);
    const int32_t before = lw(w + 0x40);
    if (lw(MS + 0x128) != 0) {
        if (!card_ready()) return 0;
        sw(MS + 0x128, 0);
        if (lw(S + 0xe4) != 0) {
            sw(FLAGS, lwu(FLAGS) | 0x80);
            screen_open(3, lwu(MS + 4));
            return 0;
        }
        summary_write();
    }
    int32_t out = 0;
    if (exit_keys(out)) return out;
    const uint32_t keys = slot_keys(w, 0x1a4);
    if ((keys & 0x40) != 0 && lw(S + 8) == 2) {
        if (lw(summary(lw(w + 0x40)) + 0x20) != -1) {
            sw(MS + 0xd4, 0);
            sw(MS + 8, OV(0x001b6678));
            sw(OV(0x001b6740), lwu(w + 0x40));
        } else {
            sw(w + 0x4c, 1);
        }
    }
    if (lw(w + 0x40) != before) moby_sound_start(1, 0x11, lwu(w + 0x14));
    return 0;
}

// The pause menu's Load page: a used slot chosen (cross) is read (the card's operation 0xd);
// done: an error shows the card's screens (flag 0x100), else the sound volumes are set again
// from the settings read and the level left for the saved one.
// orig: 0x00280f20
int32_t load_slots_update(uint32_t w) {
    const int32_t before = lw(w + 0x40);
    if (lw(MS + 0x128) != 0) {
        if (!card_ready()) return 0;
        sw(MS + 0x128, 0);
        if (lw(S + 0xe4) != 0) {
            sw(FLAGS, lwu(FLAGS) | 0x100);
            screen_open(3, lwu(MS + 4));
            return 0;
        }
        const int32_t fx = lw(0x0015edf0);
        sw(S + 0xf4, 1);
        constexpr uint32_t T = 0x0013e550;
        sw(T + 0x4c, lwu(0x0015edec));
        const int32_t fx8 = fx * 8 / 10;
        sw(T + 0x48, static_cast<uint32_t>(fx8));
        sw(T + 0x50, static_cast<uint32_t>(fx8));
        const int32_t fx7 = (fx * 8 - fx) / 10;
        sw(T + 0x54, static_cast<uint32_t>(fx7));
        sw(T + 0x5c, static_cast<uint32_t>(fx));
        sw(T + 0x58, static_cast<uint32_t>(fx7));
        level_exit_to(lw(0x0015ed84));
        sh(0x0013e05a, 0);
    }
    int32_t out = 0;
    if (exit_keys(out)) return out;
    const uint32_t keys = slot_keys(w, 0x1c4);
    if ((keys & 0x40) != 0 && lw(S + 8) == 2 && lw(summary(lw(w + 0x40)) + 0x20) >= 0) {
        moby_sound_start(0, 0x11, lwu(w + 0x14));
        sw(S + 0xc0, 0);
        sw(S + 0x14, lwu(w + 0x40));
        if (lw(S + 0xdc) < 0) {
            sw(S + 0xe0, 0);
            sw(S + 0xdc, 0xd);
        }
        sw(MS + 0x128, 1);
        sw(MS + 0x12c, 0x4fb6);
    }
    if (lw(w + 0x40) != before) moby_sound_start(1, 0x11, lwu(w + 0x14));
    return 0;
}

// The main menu's New Game page: a slot chosen (cross; a used one asks first: page 0x1b8f38, or
// 0x1b9098 for the challenge mode (+0x30 0x2000)) gets a new game (0x2848b8, or 0x284950 the
// challenge mode's, keeping the items); done: an error shows the card's screens (flag 0x80),
// else the slot's summary is the game's and the level left for Veldin. Circle: a new game
// without saving.
// orig: 0x00281200
int32_t new_game_slots_update(uint32_t w) {
    if (lwu(lwu(MS + 4) + 0x40) != w) return 0;
    const int32_t before = lw(w + 0x40);
    if (lw(w + 0x4c) == 0) {
        if (lwu(MS + 0xd0) == OV(0x001b8f38) && lw(lwu(MS + 0xd0) + 0x84) != 0) sw(w + 0x4c, 1);
        if (lw(w + 0x4c) == 0 && lwu(MS + 0xd0) == OV(0x001b9098) && lw(lwu(MS + 0xd0) + 0x84) != 0) sw(w + 0x4c, 1);
    }
    if (lw(w + 0x4c) == 1) {
        if ((lwu(w + 0x30) & 0x2000) != 0) {
            challenge_game_image(lw(w + 0x40));
        } else {
            new_game_image(lwu(w + 0x48), lw(w + 0x40));
        }
        sw(MS + 0x128, 1);
        sw(MS + 0x12c, 0x4fb5);
    }
    sw(w + 0x4c, 2);
    if (lw(MS + 0x128) != 0) {
        if (!card_ready()) return 0;
        sw(MS + 0x128, 0);
        if (lw(S + 0xe4) != 0) {
            sw(FLAGS, lwu(FLAGS) | 0x80);
            screen_open(3, lwu(MS + 4));
            return 0;
        }
        summary_write();
        level_exit_to(0);
        sh(0x0013e05a, 1);
    }
    int32_t out = 0;
    if (exit_keys(out)) return out;
    const uint32_t keys = slot_keys(w, 0x1a4);
    if ((keys & 0x40) != 0 && lw(S + 8) == 2) {
        if (lw(summary(lw(w + 0x40)) + 0x20) == -1) {
            sw(w + 0x4c, 1);
        } else {
            const bool challenge = (lwu(w + 0x30) & 0x2000) != 0;
            sw(MS + 8, challenge ? OV(0x001b9098) : OV(0x001b8f38));
            sw(MS + 0xd4, challenge ? 2u : 1u);
            sw(OV(0x001b6740), lwu(w + 0x40));
        }
    } else if ((keys & 0x20) != 0) {
        sw(S + 0xf4, 0);
        sw(FLAGS, lwu(FLAGS) & ~2u & ~4u);
        new_game_state();
        level_exit_to(0);
        sh(0x0013e05a, 1);
    }
    if (lw(w + 0x40) != before) moby_sound_start(1, 0x11, lwu(w + 0x14));
    return 0;
}

// The five slots drawn: each a frame (the focused one highlighted while the card is idle) with,
// for a PS2 card, "Empty" or the time played (hours:minutes), the challenge mode, the bolts and
// the date.
// orig: 0x00281628
int32_t save_slots_draw(uint32_t w) {
    const uint32_t sp = ee_stack_alloc(0x50);
    int32_t y = 4;
    texture_block_begin(0);
    for (int32_t i = 0; i < 5; ++i) {
        const int32_t width = lw(w + 0x20);
        if (lw(S + 0xd4) < 3 && lw(S + 0xdc) < 0 && lw(w + 0x40) == i) {
            gs_flat_rect(0, y - 4, width, y + 0x34, 0x8020ffffu, 0);
            gs_flat_rect(3, y - 1, width - 3, y + 0x31, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00160270)))), 0);
        }
        gs_flat_rect(3, y - 1, width - 3, y + 0x31, 0x80303030u, 0);
        const uint32_t r = 0x0013d2b0 + static_cast<uint32_t>(i * 0x1c);
        if (!(lw(S + 0xd4) < 3) || lw(S + 0xdc) >= 0 || lw(S + 8) != 2) {
            y += 0x30;
        } else if (lw(r) == -1) {
            y += 0x10;
            text_centered_med(width / 2, y, 0x80ffa888u, sub_00203ad8(TXT(0x5217)), -1);
            y += 0x20;
        } else {
            const int32_t t = lw(r + 0xc);
            int32_t hours = t / 216000;
            const int32_t minutes = t / 3600 - hours * 60;
            // (lui: these colours are sign-extended to 64 bits, unlike the dsll-built ones above)
            const uint64_t colour = (lw(w + 0x40) ^ i) == 0 ? 0xffffffff8020ffffull : 0xffffffff80ffa888ull;
            if (!(hours < 100)) hours = 0x63;
            {
                const std::vector<uint64_t> a = {static_cast<uint64_t>(static_cast<int64_t>(hours)), static_cast<uint64_t>(static_cast<int64_t>(minutes))};
                libc_sprintf(sp, OV(0x001603c0), a);
            }
            hud_sprite(hud_frame(0xe99e, 3), 4, y, 0x10, 0x10, 0x80);
            text_med(0x16, y, colour, sp, -1);
            if (lw(r + 8) != 0) {
                hud_sprite(hud_frame(0xe99e, 4), 0x4e, y, 0x10, 0x10, 0x80);
                const int32_t c = lw(r + 8);
                {
                    const std::vector<uint64_t> a = {static_cast<uint64_t>(static_cast<int64_t>(c < 100 ? c : 0x63))};
                    libc_sprintf(sp, OV(0x00160360), a);
                }
                text_med(0x60, y, colour, sp, -1);
            }
            int32_t bolts = lw(r + 4);
            if (0x98967f < bolts) bolts = 0x98967f;
            y += 0x10;
            if (bolts < 1000) {
                {
                    const std::vector<uint64_t> a = {static_cast<uint64_t>(static_cast<int64_t>(bolts))};
                    libc_sprintf(sp, OV(0x00160360), a);
                }
            } else if (0xf423f < bolts) {
                libc_sprintf(sp, OV(0x001603d0),
                             {static_cast<uint64_t>(static_cast<int64_t>(bolts / 1000000)), static_cast<uint64_t>(static_cast<int64_t>(bolts % 1000000 / 1000)),
                              static_cast<uint64_t>(static_cast<int64_t>(bolts % 1000))});
            } else {
                {
                    const std::vector<uint64_t> a = {static_cast<uint64_t>(static_cast<int64_t>(bolts / 1000)), static_cast<uint64_t>(static_cast<int64_t>(bolts % 1000))};
                    libc_sprintf(sp, OV(0x00160390), a);
                }
            }
            hud_sprite(hud_frame(0x754f, 0xf), 4, y, 0x10, 0x10, 0x80);
            text_med(0x16, y, colour, sp, -1);
            y += 0x10;
            {
                const std::vector<uint64_t> a = {lbu(r + 0x16), lbu(r + 0x15), lbu(r + 0x17)};
                libc_sprintf(sp, OV(0x001603e0), a);
            }
            hud_sprite(hud_frame(0xe99e, 2), 4, y, 0x10, 0x10, 0x80);
            text_med(0x16, y, colour, sp, -1);
            y += 0x10;
        }
        y += lw(0x0015ed80) == 0 ? 0x1b : 0x24;
    }
    texture_block_end();
    ee_stack_free(0x50);
    return 2;
}

// The focused slot's planet (its two names centred), or "Empty", for a PS2 card with the card
// idle.
// orig: 0x002809e0
int32_t save_slot_planet_draw(uint32_t w) {
    const int32_t level = lw(summary(lw(lwu(lwu(MS + 4) + 0x40) + 0x40)) + 0x20);
    texture_block_begin(0);
    if (card_ready() && lw(S + 8) == 2) {
        if (level == -1) {
            const int32_t y = lw(w + 0x24) / 2 - 8;
            text_centered_small(lw(w + 0x20) / 2, y, 0x80ffa888u, sub_00203ad8(TXT(0x5217)), -1);
        } else {
            const uint32_t names = OV(0x001c1e40) + static_cast<uint32_t>(level * 0xc);
            text_centered_small(lw(w + 0x20) / 2, 4, 0x80ffa888u, sub_00203ad8(lw(names)), -1);
            text_centered_small(lw(w + 0x20) / 2, 0x14, 0x80ffa888u, sub_00203ad8(lw(names + 4)), -1);
        }
    }
    texture_block_end();
    return 2;
}

// The slot list focused: the menu's VRAM layout for pictures, a small buffer of the menu's (for
// the save's image) at +0x48, no answer yet (+0x4c).
// orig: 0x00280b60
int32_t save_slots_focus_in(uint32_t w) {
    menu_vram_layout(1);
    sw(w + 0x48, menu_buf_alloc(0));
    sw(w + 0x4c, 0);
    return 0;
}

// The slot list left: its buffer given back.
// orig: 0x00280ba0
int32_t save_slots_focus_out(uint32_t w) {
    sw(w + 0x48, static_cast<uint32_t>(menu_buf_free(lwu(w + 0x48))));
    return 0;
}

// The question page's (0x1b6678 "overwrite?", 0x1b8f38 / 0x1b9098) answer widget focused: no
// answer yet on the page (+0x84), a small menu buffer at +0x54.
// orig: 0x0027f690
int32_t question_focus_in(uint32_t w) {
    sw(lwu(MS + 4) + 0x84, 0);
    sw(w + 0x54, menu_buf_alloc(0));
    return 0;
}

// The question's widget left: its buffer given back.
// orig: 0x0027f6d0
int32_t question_focus_out(uint32_t w) {
    sw(w + 0x54, static_cast<uint32_t>(menu_buf_free(lwu(w + 0x54))));
    return 0;
}

// The question answered: circle yes (the page's +0x84 1, M+0xd4 0), triangle no (+0x84 0), each
// back to the parent page; with the card's state not ready (0x15eeb0 neither 0x10 nor 1) back at
// once.
// orig: 0x0027f700
int32_t question_update(uint32_t w) {
    (void)w;
    const int32_t h = lw(0x0015eeb0);
    if (h != 0x10 && h != 1) {
        sw(MS + 8, lwu(lwu(MS + 4) + 0x38));
        return 0;
    }
    const uint32_t pressed = lwu(PAD + 0x1a4);
    if ((pressed & 0x20) != 0) {
        sw(MS + 0xd4, 0);
        const uint32_t page = lwu(MS + 4);
        sw(MS + 8, lwu(page + 0x38));
        sw(page + 0x84, 1);
    } else if ((pressed & 0x10) != 0) {
        const uint32_t page = lwu(MS + 4);
        sw(MS + 8, lwu(page + 0x38));
        sw(page + 0x84, 0);
    }
    return 0;
}

// The question's text (by M+0xd4: 0..2 text 0x4fb3, 3 text 0x4fb5, else the default string
// 0x1603a8) in a box over the widget: twice in black (the shadow, the second a line lower
// centred on the text's height), then in the menu's colour one pixel up-left.
// orig: 0x0027f798
int32_t question_draw(uint32_t w) {
    const uint32_t sp = ee_stack_alloc(0x40);
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x2004b);
    texture_block_begin(0);
    uint32_t text = OV(0x001603a8);
    const int32_t q = lw(MS + 0xd4);
    if (q >= 0) {
        if (q < 3) {
            text = sub_00203ad8(0x4fb3);
        } else if (q == 3) {
            text = sub_00203ad8(0x4fb5);
        }
    }
    const uint32_t b = sp + 0x20;
    libc_memset(b, 0, 0x18);
    sh(b + 2, lhu(w + 0x24) + 1);
    sh(b + 4, 1);
    sh(b + 6, lhu(w + 0x20) + 1);
    sh(b + 8, static_cast<uint32_t>(lw(w + 0x20) >> 1));
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 5);
    sh(b, 1);
    sh(b + 0xa, 5);
    for (uint32_t i = 0; i < 0x18; ++i) sb(sp + i, lbu(b + i));  // (ldl / ldr, sdl / sdr)
    text_box_draw(sp, 0x80000000u, text, -1);
    const int32_t h = lh(sp + 0xe);
    sh(sp + 0x12, lhu(sp + 0x12) ^ 4);
    sh(sp + 0xa, static_cast<uint32_t>((lw(w + 0x24) - h) >> 1));
    text_box_draw(sp, 0x80000000u, text, -1);
    for (uint32_t o = 0; o < 0xc; o += 2) sh(sp + o, lhu(sp + o) - 1);
    text_box_draw(sp, 0x80ffa888u, text, -1);
    texture_block_end();
    ee_stack_free(0x40);
    return 2;
}

// The game saved into a buffer: the clock read, the level's talkers and map area kept, the save's
// image built at `buf`, then the card asked to write it to slot `n` (operation 0x13).
// orig: 0x00284808
void save_game_image(uint32_t buf, int32_t n) {
    cd_read_clock(0x0015ee98);
    clock_to_local(0x0015ee98);
    talkers_save();
    map_area_save(0x00141ec0 + (lwu(0x0015ed84) << 11));
    save_blocks_write(buf);
    sw(S + 0xf4, 1);
    sw(S + 0xec, buf);
    sw(S + 0x14, static_cast<uint32_t>(n));
    sw(S + 0xc0, 0);
    if (lw(S + 0xdc) < 0) {
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 0x13);
    }
}

// A new game into slot `n`: the disc's default save read into the game, the clock, the save's
// image built at `buf`, the slot's level 0, the card asked to write it (operation 0x13).
// orig: 0x002848b8
void new_game_image(uint32_t buf, int32_t n) {
    new_game_state();
    cd_read_clock(0x0015ee98);
    clock_to_local(0x0015ee98);
    save_blocks_write(buf);
    sw(S + 0xc0, 0);
    sw(S + 0x14, static_cast<uint32_t>(n));
    sw(summary(n) + 0x20, 0);
    sw(S + 0xf4, 1);
    sw(S + 0xec, buf);
    if (lw(S + 0xdc) < 0) {
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 0x13);
    }
}

// The challenge mode's new game: what the game keeps (the bolts 0x13e520.., the items owned
// 0x13d4c0 (only the entries of the table 0x1b9ca0), 0x13d428, 0x14bec0, the quick select
// 0x141ea0 (an item no longer owned dropped), 0x15edd0, 0x13d408, the bolts count 0x15ed98, two
// flags of 0x13d388) put by the scratchpad, the disc's default save read, then put back; the
// challenge count 0x15ee20 + 1; the clock; with a slot (`n` >= 0) the save's image built in the
// menu's buffer M+0xe0 and the card asked to write it (operation 0x13).
// orig: 0x00284950
void challenge_game_image(int32_t n) {
    mem_copy(0x70000000, 0x0013e520, 0x28);
    mem_copy(0x70000030, 0x0013d4c0, 0x25);
    mem_copy(0x70000060, 0x0013d428, 0x94);
    mem_copy(0x70000100, 0x0014bec0, 0x50);
    mem_copy(0x70000150, 0x00141ea0, 0x20);
    mem_copy(0x70000170, 0x0015edd0, 0xc);
    mem_copy(0x70000180, 0x0013d408, 0x20);
    quick_rings_challenge_keep();  // (enhancement: rings 2 and 3 kept as ring 1, src/host/quick_rings.h)
    const uint32_t bolts = lwu(0x0015ed98);
    const int32_t count = lw(0x0015ee20);
    const bool f4 = lbu(0x0013d388 + 4) != 0, f5 = lbu(0x0013d388 + 5) != 0;
    new_game_state();
    mem_copy(0x0013e520, 0x70000000, 0x28);
    for (uint32_t t = OV(0x001b9ca0); lw(t) != -1; t += 4) {
        const uint32_t k = lwu(t);
        sb(0x0013d4c0 + k, lbu(0x70000030 + k));
        if (lw(t + 4) == -1) break;
    }
    mem_copy(0x0013d428, 0x70000060, 0x94);
    mem_copy(0x0014bec0, 0x70000100, 0x50);
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t item = lwu(0x70000150 + i * 4);
        sw(0x00141ea0 + i * 4, lbu(item + 0x0013d4c0) == 0 ? 0u : item);
    }
    quick_rings_challenge_restore();  // (enhancement)
    mem_copy(0x0015edd0, 0x70000170, 0xc);
    mem_copy(0x0013d408, 0x70000180, 0x20);
    sw(0x0015ed98, bolts);
    if (f4) {
        sb(0x0013d388 + 4, 1);
        sw(0x0015eda0, 5);
    }
    if (f5) {
        sb(0x0013d388 + 5, 1);
        sw(0x0015eda0, 8);
    }
    sw(0x0015ee20, static_cast<uint32_t>(count + 1));
    sb(0x0015ee1c, 0);
    sb(0x0015ee1d, 0);
    cd_read_clock(0x0015ee98);
    clock_to_local(0x0015ee98);
    if (n < 0) return;
    sw(S + 0x14, static_cast<uint32_t>(n));
    sw(summary(n) + 0x20, 0);
    save_blocks_write(lwu(MS + 0xe0));
    sw(S + 0xec, lwu(MS + 0xe0));
    sw(S + 0xc0, 0);
    if (lw(S + 0xdc) < 0) {
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 0x13);
    }
}

// Text centred on x in the medium font (effect texture 2); returns its left edge.
// orig: 0x001fba40
int32_t text_centered_med(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    x -= text_width_med(s, n) >> 1;
    text_draw(x, y, colour, s, n, effect_tex0(2), OV(0x001c34f0));
    return x;
}

}  // namespace rac1
