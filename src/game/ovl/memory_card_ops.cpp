// Ported overlay code: the memory card's machine 0x24b690 with its operations, the
// save's names (0x24b410), the card library's start (0x24cf10), a save's image built
// (0x24ce68) and a slot's summary read (0x24d118). The save block 0x13d290 (S): a record of 0xb8
// bytes per port from +0 (+0 port, +4 slot, +8 type, +0xc free clusters, +0x10 formatted, +0x14
// the save's state / the slot chosen, +0x18 the slot being read, +0x1c the card's status, +0x20
// five slot summaries of 0x1c bytes (+0 level (-1 none), +4 bolts, +8 time played, +0xc
// challenge, +0x10 the clock (8 bytes), +0x18 a bad checksum), +0xac the save's problems, +0xb0 /
// +0xb4 the two block sizes read from a save file), +0xb8 / +0xbc the last sceMcSync's command /
// result, +0xc0 the port saved to (-1 none), +0xc4 the port, +0xc8 a level index, +0xcc a
// command running, +0xd0 the file open, +0xd4 the state, +0xd8 its step, +0xdc an operation
// asked for (its port +0xe0), +0xe4 / +0xe8 the last error and its port, +0xec / +0xf0 the data
// to write and its size, +0xf4 a save pending.
#include <game/ovl/memory_card_ops.h>
#include <host/custom_planet.h>

#include <game/enhance/advanced.h>
#include <game/lib/libc.h>
#include <game/lib/libmc.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

void card_step(uint32_t st, uint32_t sp);

namespace {
constexpr uint32_t S = 0x0013d290;    // the save block
constexpr uint32_t DIR = 0x0013d1d0;  // "/BASCUS-97199RATCHET" (the product code put in by 0x24b410)
constexpr uint32_t TOC = 0x00137b80;  // the disc's table of contents: +0x10 / +0x14 save_game.bin's sector / count
constexpr uint32_t GAME_BLOCK = 0x0014eed0, LEVEL_BLOCK = 0x001506d0;  // a save's two blocks as read / written

uint32_t rec(uint32_t field) { return S + static_cast<uint32_t>(lw(S + 0xc4) * 0xb8) + field; }  // orig: (helper) repeated in 0x0024b690
int32_t port() { return lw(rec(0)); }                                                            // orig: (helper) repeated in 0x0024b690
int32_t slot() { return lw(rec(4)); }                                                            // orig: (helper) repeated in 0x0024b690
void stop() {  // orig: (helper) repeated in 0x0024b690: no command, state 0 (idle), port 0
    sw(S + 0xcc, 0);
    sw(S + 0xd4, 0);
    sw(S + 0xc4, 0);
}
void fail(uint32_t e) {  // orig: (helper) repeated in 0x0024b690: the error and its port, then stop
    sw(S + 0xe4, e);
    sw(S + 0xe8, lwu(S + 0xc4));
    stop();
}
// orig: (helper) repeated in 0x0024b690: a file's name "save<slot>.bin" made in `buf`
void save_name(uint32_t buf, int32_t n) {
    const std::vector<uint64_t> a = {static_cast<uint64_t>(static_cast<int64_t>(n))};
    libc_sprintf(buf, 0x0013d270, a);
}
// orig: (helper) repeated in 0x0024b690: the size of a save file (both blocks, 20 levels)
uint32_t save_file_size() { return save_list_size(OV(0x001845c0)) + save_list_size(OV(0x001848c0)) * 0x14 + 8; }

// The open's error to a code: -7 / -5 / -4 / -3 the codes given, -2 `unformatted`, else `other`.
// orig: (helper) repeated in 0x0024b690
uint32_t open_error(int32_t r, uint32_t c7, uint32_t c5, uint32_t c4, uint32_t c3, uint32_t unformatted, uint32_t other) {
    if (r == -7) return c7;
    if (r == -5) return c5;
    if (r == -4) return c4;
    if (r == -3) return c3;
    return r != -2 ? other : unformatted;
}
// A read's / write's error to a code: -5 / -4 / -3 the codes given, -2 `unformatted`, else `other`.
// orig: (helper) repeated in 0x0024b690
uint32_t io_error(int32_t r, uint32_t c5, uint32_t c4, uint32_t c3, uint32_t unformatted, uint32_t other) {
    if (r == -5) return c5;
    if (r == -4) return c4;
    if (r == -3) return c3;
    return r != -2 ? other : unformatted;
}
// A short read / write (bc >= 0: `code`, the file closed, then stop) or its error (stopped with
// the code).
// orig: (helper) repeated in 0x0024b690 (0x24cb30 / 0x24c04c)
void io_failed(int32_t r, uint32_t code, uint32_t err) {
    if (r >= 0) {
        sw(S + 0xe4, code);
        if (mc_close(lw(S + 0xd0)) != 0) return;
        stop();
        return;
    }
    sw(S + 0xe4, err);
    stop();
}
}  // namespace

// The card's machine, a step a frame: with a command running its sceMcSync (busy until it
// finishes, the result at +0xbc); else the state's step (+0xcc set first: a step that issues no
// command clears it):
//   0 / 1   the port's card asked for (sceMcGetInfo), its answer;
//   2       idle: an operation asked for (+0xdc) started on its port (+0xe0);
//   3 / 4   format the card; 5 / 6 unformat it;
//   7 / 8   the save's directory (chdir) and save0.bin's two block sizes checked;
//   9 / 0xa the save made on the card: its directory, icon.sys, static.ico, save0..4.bin (the
//           disc's default save) and the settings file, from save_game.bin;
//   0xb / 0xc the save's directory emptied and deleted;
//   0xd / 0xe a save file (slot +0x14) read into the game (its two blocks, 20 levels);
//   0xf / 0x10 the game saved into its slot (the game's block, then the level's block);
//   0x11 / 0x12 save_game.bin read for a default save; 0x13 / 0x14 a save image (+0xec) written
//           to the slot;
//   0x15..0x17 the five slots' summaries read (for the slot lists).
// Errors leave a code at +0xe4 (and the port at +0xe8) that the menus' card state shows.
// orig: 0x0024b690
void card_machine() {
    if (lw(S + 0xcc) != 0) {
        sw(S + 0xcc, mc_sync(1, S + 0xb8, S + 0xbc) == 0 ? 1u : 0u);
        return;
    }
    const uint32_t st = lwu(S + 0xd4);
    sw(S + 0xcc, 1);
    if (!(st < 0x18)) return;
    const uint32_t sp = ee_stack_alloc(0x60);  // the frame: a file name at +0, buffers' addresses at +0x40..
    card_step(st, sp);
    ee_stack_free(0x60);
}

// orig: (helper) 0x0024b690's step of the state `st` (the frame at `sp`)
void card_step(uint32_t st, uint32_t sp) {
    const int32_t bc = lw(S + 0xbc);
    uint32_t d8 = lwu(S + 0xd8);
    switch (st) {
        case 0:
            if (lw(S + 0xc4) < 0) sw(S + 0xc4, 0);
            if (mc_get_info(lw(rec(0)), lw(rec(4)), rec(8), rec(0xc), rec(0x10)) == 0) sw(S + 0xd4, 1);
            return;
        case 1:
            if (lw(S + 0xbc) != 0) {
                sw(rec(0x14), static_cast<uint32_t>(-3));
                sw(rec(0x1c), lwu(S + 0xbc));
                sw(rec(0xac), 0xffffffffu);
            }
            {
                const int32_t p = lw(S + 0xc4) + 1;
                sw(S + 0xc4, static_cast<uint32_t>(p));
                sw(S + 0xd4, 0 < p ? 2u : 0u);
            }
            sw(S + 0xcc, 0);
            return;
        case 2: {
            const int32_t op = lw(S + 0xdc);
            if (op < 0) {
                const int32_t n = lw(S + 0xc4) + 1;
                sw(S + 0xc4, static_cast<uint32_t>(n));
                if (!(n < 0x1f)) {
                    sw(S + 0xd4, 0);
                    sw(S + 0xc4, 0);
                }
                sw(S + 0xcc, 0);
                return;
            }
            if (lw(S + 0x1c) == 0) {
                const int32_t p = lw(S + 0xe0);
                if (p < 0) {
                    sw(S + 0xe4, 0x271a);
                } else {
                    sw(S + 0xc4, static_cast<uint32_t>(p));
                    sw(S + 0xd4, static_cast<uint32_t>(op));
                    sw(S + 0xe4, 0);
                }
            }
            sw(S + 0xe0, 0xffffffffu);
            sw(S + 0xdc, 0xffffffffu);
            sw(S + 0xcc, 0);
            return;
        }
        case 3:  // format (an unformatted card only)
            if (lw(rec(0x10)) != 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xd4, 2);
                return;
            }
            if (mc_format(port(), slot()) != 0) return;
            sw(S + 0xd4, 4);
            return;
        case 4:
            if (bc != 0) {
                sw(S + 0xe4, 1);
                sw(S + 0xe8, lwu(S + 0xc4));
            }
            stop();
            return;
        case 5:  // unformat (an unformatted card only)
            if (lw(rec(0x10)) != 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xd4, 2);
                return;
            }
            if (mc_unformat(port(), slot()) != 0) return;
            sw(S + 0xd4, 6);
            return;
        case 6:
            if (bc != 0) {
                sw(S + 0xe4, 2);
                sw(S + 0xe8, lwu(S + 0xc4));
            }
            stop();
            return;
        case 7:  // the save's directory
            if (mc_chdir(port(), slot(), DIR, 0) != 0) return;
            sw(S + 0xd8, 0);
            sw(S + 0xd4, 8);
            return;
        case 8:
            if (!(d8 < 6)) return;
            switch (d8) {
                case 0:  // the chdir's answer: none (-4) the card has no save; -2 unformatted
                    if (bc == 0) {
                        if (lw(rec(0x14)) < 0) sw(rec(0x14), 0xffffffffu);
                        sw(S + 0xcc, 0);
                        sw(S + 0xd8, 1);
                        return;
                    }
                    sw(rec(0x14), static_cast<uint32_t>(-2));
                    if (bc == -2) {
                        fail(3);
                    } else if (bc == -4) {
                        stop();
                    } else {
                        fail(4);
                    }
                    return;
                case 1:
                    save_name(sp, 0);
                    if (mc_open(port(), slot(), sp, 1) != 0) return;
                    sw(S + 0xd8, lwu(S + 0xd8) + 1);
                    return;
                case 2:
                    if (bc >= 0) {
                        sw(S + 0xd0, static_cast<uint32_t>(bc));
                        sw(S + 0xd8, 3);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(rec(0x14), static_cast<uint32_t>(-2));
                    sw(S + 0xe8, lwu(S + 0xc4));
                    sw(S + 0xe4, open_error(bc, 0x2710, 0x2711, 0x2712, 0x2713, 3, 4));
                    sw(S + 0xc4, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xcc, 0);
                    return;
                case 3:
                    sw(S + 0xf0, 8);
                    if (mc_read(lw(S + 0xd0), rec(0xb0), 8) != 0) return;
                    sw(S + 0xd8, 4);
                    return;
                case 4:
                    if (bc == lw(S + 0xf0)) {  // the two sizes against the game's lists
                        sw(rec(0xac), 0);
                        if (lwu(rec(0xb0)) != save_list_size(OV(0x001845c0))) sw(rec(0xac), lwu(rec(0xac)) + 1);
                        if (lwu(rec(0xb4)) != save_list_size(OV(0x001848c0))) sw(rec(0xac), lwu(rec(0xac)) + 1);
                        sw(S + 0xcc, 0);
                        sw(S + 0xd8, 5);
                        return;
                    }
                    sw(rec(0x14), static_cast<uint32_t>(-2));
                    sw(S + 0xe8, lwu(S + 0xc4));
                    if (bc >= 0) {
                        sw(S + 0xe4, 0x2716);
                        if (mc_close(lw(S + 0xd0)) != 0) return;
                        sw(S + 0xcc, 0);
                        sw(S + 0xd4, 0);
                        sw(S + 0xc4, 0);
                        return;
                    }
                    sw(S + 0xe4, io_error(bc, 0x2711, 0x2714, 0x2715, 3, 4));
                    stop();
                    return;
                case 5:
                    if (mc_close(lw(S + 0xd0)) != 0) return;
                    sw(S + 0xcc, 0);
                    sw(S + 0xd4, 0x15);
                    return;
            }
            return;
        case 9:  // the save made: from step 0 of state 0xa
            sw(S + 0xd4, 0xa);
            sw(S + 0xd8, 0);
            sw(rec(0x14), 0);
            d8 = 0;
            [[fallthrough]];
        case 0xa:
            if (!(d8 < 0x16)) return;
            switch (d8) {
                case 0:  // room for it (350 KB)
                    if (lw(rec(0xc)) < 0x15e) {
                        sw(S + 0xe8, lwu(S + 0xc4));
                        sw(S + 0xe4, 7);
                        sw(S + 0xc4, 0);
                        sw(S + 0xd4, 0);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xd8, 1);
                    sw(S + 0xcc, 0);
                    return;
                case 1:
                    if (mc_mkdir(port(), slot(), DIR) != 0) return;
                    sw(S + 0xd8, 2);
                    return;
                case 2:  // made, or already there (-4)
                    if (bc == 0 || bc == -4) {
                        sw(S + 0xd8, 3);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    sw(S + 0xe4, bc == -3 ? 7u : bc != -2 ? 0xdu : 6u);
                    stop();
                    return;
                case 3: {  // save_game.bin read into the top of the level's area
                    level_buffer_room(lwu(TOC + 0x14) << 11, sp + 0x40, sp + 0x44);
                    stream_disc_read(lwu(sp + 0x40), lwu(TOC + 0x10), lwu(TOC + 0x14));
                    sw(S + 0xd8, 4);
                    sw(S + 0xcc, 0);
                    return;
                }
                case 4:  // the read done
                    if (lh(0x001516d8) != 0) {
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xd8, 5);
                    sw(S + 0xcc, 0);
                    return;
                case 5:
                case 9:
                case 0xd:
                case 0x12:  // a file of the save created
                    if (d8 == 9) {
                        libc_strcpy(sp, 0x0013d220);  // static.ico
                    } else if (d8 < 0xa) {
                        if (d8 == 5) libc_strcpy(sp, 0x0013d200);  // icon.sys
                    } else if (d8 == 0xd) {
                        save_name(sp, lw(rec(0x14)));  // save<n>.bin
                    } else if (d8 == 0x12) {
                        libc_strcpy(sp, 0x0013d240);  // the settings file
                    }
                    if (mc_open(port(), slot(), sp, 0x203) != 0) return;
                    sw(S + 0xd0, 0xffffffffu);
                    sw(S + 0xd8, lwu(S + 0xd8) + 1);
                    return;
                case 6:
                case 0xa:
                case 0xe:
                case 0x13: {  // its data written: from save_game.bin (icon.sys, static.ico, a default
                              // save) or the game's settings 0x15ed84
                    if (bc < 0) {
                        sw(S + 0xe4, 0xa);
                        sw(S + 0xe8, lwu(S + 0xc4));
                        stop();
                        return;
                    }
                    if (lw(S + 0xd0) < 0) sw(S + 0xd0, static_cast<uint32_t>(bc));
                    level_buffer_room(lwu(TOC + 0x14) << 11, sp + 0x48, sp + 0x4c);
                    const uint32_t buf = lwu(sp + 0x48);
                    if (d8 == 0xa) {
                        sw(S + 0xf0, lwu(buf + 0xc));
                        sw(S + 0xec, buf + lwu(buf + 8));
                    } else if (d8 < 0xb) {
                        if (d8 == 6) {
                            sw(S + 0xf0, 0x3c4);
                            sw(S + 0xec, buf + lwu(buf));
                        }
                    } else if (d8 == 0xe) {
                        sw(S + 0xf0, save_file_size());
                        sw(S + 0xec, buf + lwu(buf + 0x10));
                    } else if (d8 == 0x13) {
                        sw(S + 0xf0, lw(0x0015ee90) != 0 ? 0x3c00u : 0x3c04u);
                        sw(S + 0xec, 0x0015ed84);
                    }
                    if (mc_write(lw(S + 0xd0), lwu(S + 0xec), lw(S + 0xf0)) != 0) return;
                    sw(S + 0xd8, lwu(S + 0xd8) + 1);
                    return;
                }
                case 7:
                case 0xb:
                case 0xf:
                case 0x14:  // all written: the file closed
                    if (bc == lw(S + 0xf0)) {
                        if (mc_close(lw(S + 0xd0)) != 0) return;
                        sw(S + 0xd8, lwu(S + 0xd8) + 1);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    if (bc >= 0) {
                        io_failed(bc, 0xb, 0);
                        return;
                    }
                    sw(S + 0xe4, bc == -4 ? 8u : bc == -3 ? 7u : bc != -2 ? 0xdu : 6u);
                    stop();
                    return;
                case 8:
                case 0xc:
                case 0x10:  // closed
                    if (bc != 0) {
                        sw(S + 0xe4, 0xc);
                        sw(S + 0xe8, lwu(S + 0xc4));
                        sw(S + 0xc4, 0);
                        sw(S + 0xd4, 0);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xd8, lwu(S + 0xd8) + 1);
                    sw(S + 0xcc, 0);
                    return;
                case 0x11: {  // the next save file (5)
                    const int32_t n = lw(rec(0x14)) + 1;
                    sw(rec(0x14), static_cast<uint32_t>(n));
                    sw(S + 0xd8, n < 5 ? 0xdu : lwu(S + 0xd8) + 1);
                    sw(S + 0xcc, 0);
                    return;
                }
                case 0x15:  // done: port 0's save state -1, no problems
                    sw(S + 0x14, 0xffffffffu);
                    sw(S + 0xac, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xc4, 0);
                    return;
            }
            return;
        case 0xb:  // the save deleted: from step 0 of state 0xc
            sw(S + 0xd4, 0xc);
            sw(S + 0xd8, 0);
            sw(rec(0x14), static_cast<uint32_t>(-2));
            d8 = 0;
            [[fallthrough]];
        case 0xc:
            if (!(d8 < 5)) return;
            switch (d8) {
                case 0:
                case 2:  // the directory's first / next file
                    if (mc_get_dir(port(), slot(), 0x0013d1e8, d8 == 0 ? 0 : 1, 1, OV(0x00184980)) != 0) return;
                    sw(S + 0xd8, 1);
                    return;
                case 1:
                    if (bc == 1) {  // one found: deleted
                        const std::vector<uint64_t> a = {DIR, OV(0x001849a0)};
                        libc_sprintf(sp, OV(0x0015ff30), a);
                        debug_printf_stub();
                        if (mc_delete(port(), slot(), sp) != 0) return;
                        sw(S + 0xd8, 2);
                        return;
                    }
                    if (bc == 0) {  // none left: the directory
                        sw(S + 0xd8, 3);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    sw(S + 0xe4, bc == -5 ? 0x10u : bc == -4 ? 0xfu : bc != -2 ? 0x12u : 0xeu);
                    sw(S + 0xc4, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xcc, 0);
                    return;
                case 3:
                    if (mc_delete(port(), slot(), DIR) != 0) return;
                    debug_printf_stub();
                    sw(S + 0xd8, 4);
                    return;
                case 4:
                    if (bc != 0) {
                        sw(S + 0xe8, lwu(S + 0xc4));
                        sw(S + 0xe4, bc == -6 ? 0x11u : bc == -5 ? 0x10u : bc == -4 ? 0xfu : bc != -2 ? 0x12u : 0xeu);
                    }
                    stop();
                    return;
            }
            return;
        case 0xd:  // a save read into the game: the slot +0x14
            if (lw(rec(0x14)) < 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xe4, 0x13);
                sw(S + 0xd4, 0);
                sw(S + 0xc4, 0);
                return;
            }
            sw(S + 0xc8, 0);
            sw(S + 0xd4, 0xe);
            sw(S + 0xd8, 0);
            d8 = 0;
            [[fallthrough]];
        case 0xe:
        case 0x17:  // (0x17: a slot's summary, the slot +0x18)
            if (!(d8 < 9)) return;
            switch (d8) {
                case 0:
                    save_name(sp, lw(rec(lw(S + 0xd4) == 0x17 ? 0x18 : 0x14)));
                    if (mc_open(port(), slot(), sp, 1) != 0) return;
                    sw(S + 0xd8, lwu(S + 0xd8) + 1);
                    return;
                case 1:
                    if (bc >= 0) {
                        sw(S + 0xd0, static_cast<uint32_t>(bc));
                        sw(S + 0xd8, 2);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    sw(S + 0xe4, open_error(bc, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1c));
                    sw(S + 0xc4, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xcc, 0);
                    return;
                case 2:
                    sw(S + 0xf0, 8);
                    if (mc_read(lw(S + 0xd0), rec(0xb0), 8) != 0) return;
                    sw(S + 0xd8, 3);
                    return;
                case 3:
                    if (bc == lw(S + 0xf0)) {
                        sw(S + 0xcc, 0);
                        sw(S + 0xd8, 4);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    io_failed(bc, 0x1b, io_error(bc, 0x15, 0x19, 0x1a, 0x18, 0x1c));
                    return;
                case 4: {  // the game's block (at most 0x1800 bytes)
                    sw(S + 0xf0, lwu(rec(0xb0)));
                    if (!(lw(S + 0xf0) < 0x1801)) debug_trap();
                    if (mc_read(lw(S + 0xd0), GAME_BLOCK, lw(S + 0xf0)) != 0) return;
                    sw(S + 0xd8, 5);
                    return;
                }
                case 5:
                    if (bc == lw(S + 0xf0)) {
                        if (lw(S + 0xd4) == 0x17) {
                            save_slot_summary(GAME_BLOCK, lw(S + 0xc4), lw(rec(0x18)));
                            sw(S + 0xd8, 8);
                            sw(S + 0xcc, 0);
                            return;
                        }
                        sw(rec(0xac), static_cast<uint32_t>(save_block_read(GAME_BLOCK, 0, OV(0x001845c0))));
                        sw(S + 0xd8, 6);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    io_failed(bc, 0x1b, io_error(bc, 0x15, 0x19, 0x1a, 0x18, 0x1c));
                    return;
                case 6: {  // a level's block (at most 0x1000 bytes)
                    sw(S + 0xf0, lwu(rec(0xb4)));
                    if (!(lw(S + 0xf0) < 0x1001)) debug_trap();
                    if (mc_read(lw(S + 0xd0), LEVEL_BLOCK, lw(S + 0xf0)) != 0) return;
                    sw(S + 0xd8, 7);
                    return;
                }
                case 7:
                    if (bc == lw(S + 0xf0)) {
                        const int32_t bad = save_block_read(LEVEL_BLOCK, lw(S + 0xc8), OV(0x001848c0));
                        sw(rec(0xac), lwu(rec(0xac)) + static_cast<uint32_t>(bad));
                        const int32_t n = lw(S + 0xc8) + 1;
                        sw(S + 0xc8, static_cast<uint32_t>(n));
                        sw(S + 0xd8, n < 0x14 ? 6u : 8u);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    io_failed(bc, 0x1b, io_error(bc, 0x15, 0x19, 0x1a, 0x18, 0x1c));
                    return;
                case 8:
                    if (mc_close(lw(S + 0xd0)) != 0) return;
                    if (lw(S + 0xd4) != 0x17) {
                        advanced_save_loaded(lw(rec(0x14)));  // enhancement: the PC settings of this save (save<N>.pc)
                        custom_planet_save_loaded(true);      // (host, local experiment: whether it was made on the custom planet)
                        sw(S + 0xc4, 0);
                        sw(S + 0xd4, 0);
                        return;
                    }
                    sw(S + 0xcc, 0);
                    sw(S + 0xd4, 0x16);
                    return;
            }
            return;
        case 0xf:  // the game saved into its slot (+0x14), when its save is sound
            if (lw(rec(0xac)) != 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xe4, 0x2717);
                sw(S + 0xd4, 0);
                sw(S + 0xc4, 0);
                return;
            }
            if (lw(rec(0x14)) < 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xe4, 0x1d);
                sw(S + 0xd4, 0);
                sw(S + 0xc4, 0);
                return;
            }
            sw(S + 0xd8, 0);
            sw(S + 0xd4, 0x10);
            d8 = 0;
            [[fallthrough]];
        case 0x10:
            if (!(d8 < 0xb)) return;
            switch (d8) {
                case 0:
                    save_name(sp, lw(rec(0x14)));
                    if (mc_open(port(), slot(), sp, 2) != 0) return;
                    sw(S + 0xd8, 1);
                    return;
                case 1:
                    if (bc >= 0) {
                        sw(S + 0xd0, static_cast<uint32_t>(bc));
                        sw(S + 0xd8, 2);
                        sw(S + 0xcc, 0);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    sw(S + 0xe4, open_error(bc, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x26));
                    sw(S + 0xc4, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xcc, 0);
                    return;
                case 2:  // past the two sizes
                    if (mc_seek(lw(S + 0xd0), 8, 0) != 0) return;
                    sw(S + 0xd8, 3);
                    return;
                case 3:
                    if (bc < 0) {
                        stop();
                        return;
                    }
                    sw(S + 0xd8, 4);
                    return;
                case 4:  // the game's block
                    if (mc_write(lw(S + 0xd0), GAME_BLOCK, static_cast<int32_t>(save_list_size(OV(0x001845c0)))) != 0) return;
                    sw(S + 0xd8, 5);
                    return;
                case 5:
                    if (static_cast<uint32_t>(bc) == save_list_size(OV(0x001845c0))) {
                        sw(S + 0xd8, 6);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    io_failed(bc, 0x25, io_error(bc, 0x1f, 0x23, 0x24, 0x22, 0x26));
                    return;
                case 6:  // to the level's place (+0xc8)
                    if (lw(S + 0xc8) == 0) {
                        sw(S + 0xd8, 8);
                        return;
                    }
                    if (mc_seek(lw(S + 0xd0), lw(S + 0xc8) * static_cast<int32_t>(save_list_size(OV(0x001848c0))), 1) != 0) return;
                    sw(S + 0xd8, 7);
                    return;
                case 7:
                    if (bc < 0) {
                        stop();
                        return;
                    }
                    sw(S + 0xd8, 8);
                    return;
                case 8:  // the level's block
                    if (mc_write(lw(S + 0xd0), LEVEL_BLOCK, static_cast<int32_t>(save_list_size(OV(0x001848c0)))) != 0) return;
                    sw(S + 0xd8, 9);
                    return;
                case 9:
                    if (static_cast<uint32_t>(bc) == save_list_size(OV(0x001848c0))) {
                        sw(S + 0xd8, 0xa);
                        return;
                    }
                    sw(S + 0xe8, lwu(S + 0xc4));
                    io_failed(bc, 0x25, io_error(bc, 0x1f, 0x23, 0x24, 0x18, 0x1c));
                    return;
                case 0xa:
                    if (mc_close(lw(S + 0xd0)) != 0) return;
                    advanced_save_written(lw(rec(0x14)));  // enhancement: the PC settings next to the save (save<N>.pc)
                    sw(S + 0xc4, 0);
                    sw(S + 0xd4, 0);
                    return;
            }
            return;
        case 0x11: {  // save_game.bin read (its step 0x12)
            level_buffer_room(lwu(TOC + 0x14) << 11, sp + 0x50, sp + 0x54);
            stream_disc_read(lwu(sp + 0x50), lwu(TOC + 0x10), lwu(TOC + 0x14));
            sw(S + 0xd8, 0x12);
            sw(S + 0xcc, 0);
            return;
        }
        case 0x12:  // the read done: the default save the data to write (step 0x13)
            if (lh(0x001516d8) != 0) {
                sw(S + 0xcc, 0);
                return;
            }
            level_buffer_room(lwu(TOC + 0x14) << 11, sp + 0x58, sp + 0x5c);
            sw(S + 0xd8, 0x13);
            sw(S + 0xec, lwu(sp + 0x58) + lwu(lwu(sp + 0x58) + 0x10));
            sw(S + 0xcc, 0);
            return;
        case 0x13:  // a save image (+0xec) written to the slot +0x14
            if (lw(rec(0xac)) != 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xe4, 0x2718);
                sw(S + 0xd4, 0);
                sw(S + 0xc4, 0);
                return;
            }
            if (lw(rec(0x14)) < 0) {
                sw(S + 0xcc, 0);
                sw(S + 0xe4, 0x27);
                sw(S + 0xd4, 0);
                sw(S + 0xc4, 0);
                return;
            }
            sw(S + 0xc8, 0);
            sw(S + 0xd4, 0x14);
            sw(S + 0xd8, 0);
            d8 = 0;
            [[fallthrough]];
        case 0x14:
            if (d8 == 0) {
                save_name(sp, lw(rec(0x14)));
                if (mc_open(port(), slot(), sp, 2) == 0) sw(S + 0xd8, 1);
                return;
            }
            if (d8 == 1) {
                if (bc < 0) {
                    fail(0x2b);
                    return;
                }
                sw(S + 0xd0, static_cast<uint32_t>(bc));
                sw(S + 0xf0, save_file_size());
                if (mc_write(lw(S + 0xd0), lwu(S + 0xec), lw(S + 0xf0)) != 0) return;
                sw(S + 0xd8, 2);
                return;
            }
            if (d8 == 2) {
                if (bc == lw(S + 0xf0)) {
                    if (mc_close(lw(S + 0xd0)) != 0) return;
                    sw(S + 0xd8, 3);
                    return;
                }
                sw(S + 0xe8, lwu(S + 0xc4));
                if (bc >= 0) {
                    sw(S + 0xe4, 0xb);
                    if (mc_close(lw(S + 0xd0)) != 0) return;
                    sw(S + 0xcc, 0);
                    sw(S + 0xd4, 0);
                    sw(S + 0xc4, 0);
                    return;
                }
                sw(S + 0xe4, bc == -4 ? 0x28u : bc == -3 ? 0x29u : bc != -2 ? 0x2du : 0x2au);
                stop();
                return;
            }
            if (d8 == 3) {
                if (bc != 0) {
                    sw(S + 0xe4, 0x2c);
                    sw(S + 0xe8, lwu(S + 0xc4));
                } else {
                    advanced_save_written(lw(rec(0x14)));  // enhancement: the PC settings next to the save (save<N>.pc)
                }
                stop();
            }
            return;
        case 0x15:  // the slots' summaries: from slot 0
            sw(S + 0xd4, 0x16);
            sw(rec(0x18), 0xffffffffu);
            sw(S + 0xc8, 0);
            return;
        case 0x16: {  // the next slot (5)
            const int32_t n = lw(rec(0x18)) + 1;
            sw(rec(0x18), static_cast<uint32_t>(n));
            if (n < 5) {
                sw(S + 0xd8, 0);
                sw(S + 0xd4, 0x17);
                return;
            }
            stop();
            return;
        }
    }
}

// A slot's summary (record of the port `port`, slot `n`) from a save's game block `p`: its first
// four items' words (the level, the bolts, the time played, the challenge), its 8-byte clock, and
// +0x18 1 when the block's checksum is wrong.
// orig: 0x0024d118
void save_slot_summary(uint32_t p, int32_t port, int32_t n) {
    const uint32_t bad = save_block_check(p) ? 0u : 1u;
    const uint32_t r = S + static_cast<uint32_t>(n * 0x1c + port * 0xb8);
    sw(r + 0x38, bad);
    sw(r + 0x20, lwu(p + 0x10));
    sw(r + 0x24, lwu(p + 0x1c));
    sw(r + 0x28, lwu(p + 0x28));
    sw(r + 0x2c, lwu(p + 0x34));
    for (uint32_t i = 0; i < 8; ++i) sb(r + 0x30 + i, lbu(p + 0x40 + i));  // (ldl / ldr, sdl / sdr: unaligned)
}

// A save's image at `p`: the two block sizes, the game's block, then the 20 levels' blocks.
// orig: 0x0024ce68
void save_blocks_write(uint32_t p) {
    sw(p, save_list_size(OV(0x001845c0)));
    sw(p + 4, save_list_size(OV(0x001848c0)));
    uint32_t q = p + 8;
    q += static_cast<uint32_t>(save_block_write(q, 0, OV(0x001845c0)));
    for (int32_t i = 0; i < 0x14; ++i) q += static_cast<uint32_t>(save_block_write(q, i, OV(0x001848c0)));
}

// The save's names given the disc's product code from SYSTEM.CNF (`cnf`: "BOOT2 = cdrom0:\SCUS_971.99"
// ...): "/BA" ("/BE" for an "SCES" disc) + "SCUS-97199", then copied over the first 13 bytes of
// the other names.
// orig: 0x0024b410
void save_names_init(uint32_t cnf) {
    if (lbu(cnf + 0x12) == 0x45) sb(DIR + 2, lbu(cnf + 0x12));
    for (uint32_t i = 3; i < 7; ++i) sb(DIR + i, lbu(cnf + 0xd + i));
    for (uint32_t i = 8; i < 0xb; ++i) sb(DIR + i, lbu(cnf + 0xd + i));
    for (uint32_t i = 0xb; i < 0xd; ++i) sb(DIR + i, lbu(cnf + 0xe + i));
    libc_strncpy(0x0013d1e8, DIR, 0xd);
    libc_strncpy(0x0013d200, DIR, 0xd);
    libc_strncpy(0x0013d220, DIR, 0xd);
    libc_strncpy(0x0013d240, DIR, 0xd);
    libc_strncpy(0x0013d254, DIR, 0xd);
    libc_strncpy(0x0013d270, DIR, 0xd);
}

// The card library started (sceMcInit; a message when it fails).
// orig: 0x0024cf10
void card_lib_init() {
    if (mc_init() != 0) debug_printf_stub();
}

}  // namespace rac1
