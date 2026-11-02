// Ported overlay code: the memory card's two machines run at each frame's end. The
// card's machine 0x24b690 (in the save block 0x13d290: +0xd4 its state, +0xcc a command running,
// +0xb8 / +0xbc the command and result of the last sceMcSync, +0xc4 the port, +0xd8 the operation's
// step, +0xdc an operation asked for, +0xe0 its port, +0xe4 / +0xe8 the operation's result and port,
// a record of 0xb8 bytes per port from +0: port, slot, type, free, format, +0x14 / +0x1c the card's
// status, ...) and the menus' card state 0x15eeb0 (handlers 0x184538; its flags 0x15eeb4: 1 / 2 /
// 4 the main menu's operations, 0x20 / 0x40 / 0x80 / 0x100 the screens' answers), which the memory
// card screens (game mode 4, kind 3) show and answer.
#include <game/enhance/language.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t S = 0x0013d290;   // the save block
constexpr uint32_t H = 0x0015eeb0;   // the menus' card state
constexpr uint32_t F = 0x0015eeb4;   // its flags
constexpr OvConst<0x0015f5a8u> FE{};  // the boot module (the front end) running: its 0x15f5e8

bool card_idle() { return lw(S + 0xd4) == 2 && lw(S + 0xdc) < 0; }                               // orig: (helper) repeated in 0x0024ac20's handlers

// ---- the menus' card state's handlers (table 0x184538) ----

// orig: 0x0024ac88
void ui_0() {
    sw(H, 3);
    const uint32_t r = lwu(S + 0xbc);
    sw(S + 0xf4, 0);
    sw(S + 0x1c, r);
}

// orig: 0x0024acb0
void ui_1() {
    const uint32_t f = lwu(F);
    const uint32_t a0 = f & ~4u & ~2u;
    sw(F, a0);
    if (lw(S + 0xf4) == 0) {
        sw(H, 3);
        return;
    }
    if ((f & 0x80) != 0) {
        sw(H, 0x15);
        sw(F, (a0 ^ 0x80) | 0x40);
        return;
    }
    if ((f & 0x100) != 0) {
        sw(H, 0x14);
        sw(F, (a0 ^ 0x100) | 0x40);
        return;
    }
    if (lw(S + 0x1c) != 0) {
        sw(S + 0xf4, 0);
        sw(F, a0 | 1);
        sw(H, 2);
        return;
    }
    if ((f & 0x200) != 0) {
        sw(F, a0 ^ 0x200);
        sw(H, 0x16);
    }
}

// orig: 0x0024ad60
void ui_2() {
    if (((lwu(F) ^ 1) & 1) != 0) sw(H, 3);
}

// orig: 0x0024ad88
void ui_3() {
    sw(S + 0xe0, 0xffffffffu);
    sw(H, 4);
    sw(S + 0xdc, 0xffffffffu);
}

// orig: 0x0024adb0
void ui_4() {
    sw(F, lwu(F) & ~0x20u);
    const int32_t st = lw(S + 0x1c);
    if (st == 0) {
        sw(H, 9);
        return;
    }
    if (st == -1) {
        sw(S + 0x1c, 0);
        sw(H, 9);
        return;
    }
    if (st == -2) sw(H, 5);
}

// orig: 0x0024ae18
void ui_5() {
    if (lw(S + 0x1c) != -2) {
        sw(H, 3);
        return;
    }
    if ((lwu(F) & 2) != 0) sw(H, 6);
}

// orig: 0x0024ae58
void ui_6() {
    if (lw(S + 0x1c) != -2) {
        sw(H, 3);
        return;
    }
    const uint32_t f = lwu(F);
    if ((f & 0x20) != 0) {
        sw(F, f ^ 0x20);
        sw(H, lw(FE) != 0 ? 0x17 : 5);
        return;
    }
    if ((f & 8) != 0) {
        sw(F, f ^ 8);
        sw(H, 7);
    }
}

// orig: 0x0024aed8
void ui_7() {
    const int32_t op = lw(S + 0xdc);
    sw(S + 0x1c, 0);
    if (op < 0) {
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 3);
    }
    sw(H, 8);
}

// orig: 0x0024af08
void ui_8() {
    if (!card_idle()) return;
    if (lw(S + 0xe4) != 0) {
        sw(H, 0x11);
        sw(F, lwu(F) | 0x40);
    } else {
        sw(H, 0xe);
    }
}

// orig: 0x0024af68
void ui_9() {
    if (lw(S + 0x1c) != 0) {
        sw(H, 3);
        return;
    }
    if ((lwu(F) & 6) != 0) sw(H, 0xa);
}

// orig: 0x0024afa0
void ui_10() {
    if (!card_idle()) return;
    sw(S + 0xdc, 7);
    sw(S + 0xe0, 0);
    sw(H, 0xb);
}

// orig: 0x0024afe0
void ui_11() {
    if (lw(S + 0x1c) < -1) {
        sw(H, 3);
        return;
    }
    const int32_t st = lw(S + 0x14);
    if (st == -2) {
        sw(H, lw(S + 0xc) < 0x15e ? 0x13 : 0xc);
        return;
    }
    if (!(st < -1)) sw(H, 0x10);
}

// orig: 0x0024b050
void ui_12() {
    if (lw(S + 0x1c) != 0) {
        sw(H, 3);
        return;
    }
    if ((lwu(F) & 2) != 0) sw(H, 0xd);
}

// orig: 0x0024b088
void ui_13() {
    if (lw(S + 0x1c) != 0) {
        sw(H, 3);
        return;
    }
    const uint32_t f = lwu(F);
    if ((f & 0x20) != 0) {
        sw(F, f ^ 0x20);
        sw(H, lw(FE) != 0 ? 0x18 : 0xc);
        return;
    }
    if ((f & 0x10) != 0) {
        sw(F, f ^ 0x10);
        sw(H, 0xe);
    }
}

// orig: 0x0024b100
void ui_14() {
    if (!card_idle()) return;
    sw(S + 0xdc, 9);
    sw(S + 0xe0, 0);
    sw(H, 0xf);
}

// orig: 0x0024b140
void ui_15() {
    if (!card_idle()) return;
    if (lw(S + 0xe4) != 0) {
        sw(H, 0x12);
        sw(F, lwu(F) | 0x40);
        return;
    }
    sw(S + 0xdc, 7);
    sw(S + 0xe0, 0);
    sw(H, 0x10);
    sw(S + 0xc0, 0);
    sw(S + 0x14, 0);
}

// orig: 0x0024b1b8
void ui_16() {
    const uint32_t f = lwu(F);
    const uint32_t a0 = f & ~4u & ~2u;
    sw(F, a0);
    if ((f & 0x80) != 0) {
        sw(H, 0x15);
        sw(F, (a0 ^ 0x80) | 0x40);
        return;
    }
    if ((f & 0x100) != 0) {
        sw(H, 0x14);
        sw(F, (a0 ^ 0x100) | 0x40);
        return;
    }
    if (lw(S + 0x1c) != 0) {
        sw(H, 3);
        return;
    }
    if (lw(S + 0xf4) != 0) sw(H, 1);
}

// orig: 0x0024b248
void ui_17() {
    if ((lwu(F) & 0x40) == 0) sw(H, 3);
}

// orig: 0x0024b270
void ui_18() {
    if ((lwu(F) & 0x40) == 0) sw(H, 3);
}

// orig: 0x0024b298
void ui_19() {
    if (lw(S + 0x1c) != 0) sw(H, 3);
}

// orig: 0x0024b2b8
void ui_20() {
    if ((lwu(F) & 0x40) == 0) sw(H, 3);
}

// orig: 0x0024b2e0
void ui_21() {
    if ((lwu(F) & 0x40) == 0) sw(H, 3);
}

// orig: 0x0024b308
void ui_22() {
    if (!(lw(S + 0xd4) < 3) || !(lw(S + 0xdc) < 0)) return;
    if (lw(S + 0xe4) != 0) {
        screen_open(3, 0);
        sw(H, 0x15);
        sw(F, lwu(F) | 0x40);
    } else {
        sw(H, 1);
    }
}

// orig: 0x0024b380
void ui_23() {
    if (lw(S + 0x1c) != -2) {
        sw(H, 3);
        return;
    }
    const uint32_t f = lwu(F);
    if ((f & 0x20) != 0) {
        sw(F, f ^ 0x20);
        sw(H, 5);
    }
}

// orig: 0x0024b3c8
void ui_24() {
    if (lw(S + 0x1c) != 0) {
        sw(H, 3);
        return;
    }
    const uint32_t f = lwu(F);
    if ((f & 0x20) != 0) {
        sw(F, f ^ 0x20);
        sw(H, 0xc);
    }
}

// orig: (helper) a handler of the table 0x184538 called by its address
void ui_call(uint32_t fn) {
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
        case 0x0024ac88: ui_0(); break;
        case 0x0024acb0: ui_1(); break;
        case 0x0024ad60: ui_2(); break;
        case 0x0024ad88: ui_3(); break;
        case 0x0024adb0: ui_4(); break;
        case 0x0024ae18: ui_5(); break;
        case 0x0024ae58: ui_6(); break;
        case 0x0024aed8: ui_7(); break;
        case 0x0024af08: ui_8(); break;
        case 0x0024af68: ui_9(); break;
        case 0x0024afa0: ui_10(); break;
        case 0x0024afe0: ui_11(); break;
        case 0x0024b050: ui_12(); break;
        case 0x0024b088: ui_13(); break;
        case 0x0024b100: ui_14(); break;
        case 0x0024b140: ui_15(); break;
        case 0x0024b1b8: ui_16(); break;
        case 0x0024b248: ui_17(); break;
        case 0x0024b270: ui_18(); break;
        case 0x0024b298: ui_19(); break;
        case 0x0024b2b8: ui_20(); break;
        case 0x0024b2e0: ui_21(); break;
        case 0x0024b308: ui_22(); break;
        case 0x0024b380: ui_23(); break;
        case 0x0024b3c8: ui_24(); break;
        default: TODO_PORT(fn, "the menus' card state: an unknown handler")(); break;
    }
}
}  // namespace

// The menus' card state: its handler (table 0x184538 by the state 0x15eeb0), then the frames in
// the state 0x161e00 counted (cleared when the state changed).
// orig: 0x0024ac20
void card_ui_machine() {
    const int32_t before = lw(H);
    ui_call(lwu(OV(0x00184538) + static_cast<uint32_t>(before) * 4));
    const uint32_t frames = lwu(OV(0x00161e00)) + 1;
    const int32_t now = lw(H);
    sw(OV(0x00161e00), frames);
    if (now != before) sw(OV(0x00161e00), 0);
}

// The card's machine 0x24b690: memory_card_ops.cpp.

// ---- a new game's state: the disc's default save (0x24b628) ------------------------------

// The room for a buffer of `size` bytes (at most 0x20000) at the top of the level's two areas
// (*(0x173e04) / *(0x173e08) past the level data 0x1611cc): its address in each to *a / *b; -1 (and
// 0, 0) when too big.
// orig: 0x00203358
int32_t level_buffer_room(uint32_t size, uint32_t a, uint32_t b) {
    if (0x20000u < size) {
        sw(a, 0);
        sw(b, 0);
        return -1;
    }
    sw(a, lwu(OV(0x00173e04)) + lwu(OV(0x001611cc)) - size);
    sw(b, lwu(OV(0x00173e08)) + lwu(OV(0x001611cc)) - size);
    return 0;
}

// A save block's list (0x1845c0, 0x1848c0: records of 16 bytes, address / size / id / read
// flag, ended by address 0): the block's size, 8 + each item's 8-byte header and data rounded to 4,
// + 8.
// orig: 0x0024cf40
uint32_t save_list_size(uint32_t list) {
    uint32_t n = 8;
    for (uint32_t p = list; lw(p) != 0; p += 0x10) n = (n + 8 + lwu(p + 4) + 3) & ~3u;
    return n + 8;
}

// A save block's checksum (+0 its length, +4 the checksum of the data from +8; 0 none): true when
// it is right.
// orig: 0x0024cff0
bool save_block_check(uint32_t p) {
    const uint32_t sum = lwu(p + 4);
    if (sum == 0) return false;
    return save_checksum(p + 8, lw(p)) == sum;
}

// A save block read into the game's memory: its items (id, size, data, up to the id -1) copied
// to the list's record of that id (its address + index * its size; the record's flag 1 the same
// size, -1 a shorter item, -2 a longer one: the smaller copied; the items found changed counted at
// 0x15ff28); the records' flags cleared first. The items missing or not in the list, a block of
// another size, and the records of the list not read up to the block's next id: counted; the
// count in the card record +0xac (port *(0x13d354)) and returned. 1 when the block's checksum is
// wrong.
// orig: 0x0024d1d8
int32_t save_block_read(uint32_t src, int32_t index, uint32_t list) {
    if (!save_block_check(src)) return 1;
    uint32_t p = src + 8;
    int32_t bad = 0;
    uint32_t done = 8;
    if (lw(list) != 0) {
        uint32_t r = list;
        sw(r + 0xc, 0);
        for (r += 0x10; lw(r) != 0; r += 0x10) sw(r + 0xc, 0);
    }
    while (lw(p) != -1) {
        const uint32_t id = lwu(p);
        uint32_t r = 0;
        if (lw(list) != 0) {
            uint32_t q = list;
            bool found = lwu(q + 8) == id;
            while (!found) {
                q += 0x10;
                if (lw(q) == 0) break;
                found = lwu(q + 8) == id;
            }
            if (found) r = q;
        }
        if (r != 0 && lw(r) != 0) {
            const uint32_t size = lwu(r + 4), have = lwu(p + 4);
            const uint32_t dst = static_cast<uint32_t>(index) * size + lwu(r);
            uint32_t n;
            if (have == size) {
                n = size;
                sw(r + 0xc, 1);
            } else if (static_cast<int32_t>(have) < static_cast<int32_t>(size)) {
                n = have;
                sw(r + 0xc, 0xffffffffu);
            } else {
                n = size;
                sw(r + 0xc, static_cast<uint32_t>(-2));
            }
            if (libc_memcmp(dst, p + 8, n) != 0) sw(OV(0x0015ff28), lwu(OV(0x0015ff28)) + 1);
            mem_copy(dst, p + 8, static_cast<int32_t>(n));
            done = done + 8 + ((n + 3) & ~3u);
        } else {
            ++bad;
        }
        p += ((lwu(p + 4) + 3) & ~3u) + 8;
    }
    done += 8;
    if (done != save_list_size(list)) ++bad;
    p += 8;
    if (lw(list) != 0 && lwu(list + 8) != lwu(p)) {
        const uint32_t next = lwu(p);
        uint32_t q = list;
        for (;;) {
            const int32_t flag = lw(q + 0xc);
            q += 0x10;
            if (!(0 < flag)) ++bad;
            if (lw(q) == 0) break;
            if (lwu(q + 8) == next) break;
        }
    }
    sw(S + static_cast<uint32_t>(lw(S + 0xc4) * 0xb8) + 0xac, static_cast<uint32_t>(bad));
    return bad;
}

// A save's two blocks read (the game's state 0x1845c0, then the 20 levels' blocks 0x1848c0, each
// of the list's size): the save's first words must be the two sizes ("save file wrong size"
// otherwise, 0x1e8c80).
// orig: 0x0024b550
void save_blocks_read(uint32_t p) {
    const uint32_t a = save_list_size(OV(0x001845c0));
    const uint32_t b = save_list_size(OV(0x001848c0));
    if (lwu(p) != a) {
        debug_printf_stub();
        return;
    }
    if (lwu(p + 4) != b) {
        debug_printf_stub();
        return;
    }
    uint32_t q = p + 8;
    save_block_read(q, 0, OV(0x001845c0));
    q += a;
    for (int32_t i = 0; i < 0x14; ++i) {
        save_block_read(q, i, OV(0x001848c0));
        q += b;
    }
}

// A new game: the disc's default save (table of contents 0x137b90 / 0x137b94: its sectors) read
// into a buffer at the top of the level's area and its blocks read into the game's state; the
// current level 0.
// orig: 0x0024b628
void new_game_state() {
    const uint32_t out = ee_stack_alloc(0x10);
    level_buffer_room(lwu(0x00137b80 + 0x14) << 11, out, out + 4);
    disc_read_sync(1);
    disc_read_wait(lwu(out), lwu(0x00137b80 + 0x10), lwu(0x00137b80 + 0x14));
    const uint32_t buf = lwu(out);
    save_blocks_read(buf + lwu(buf + 0x10));
    language_game_state_read();  // enhancement: the Subtitles choice (Options > Language) over the default save's
    ee_stack_free(0x10);
    sw(0x0015ed84, 0);
}

}  // namespace rac1
