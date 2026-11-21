// Quartu (level 15) class 1469, ported by hand from the level's own code: the level's help-message director. In state 0 it
// sets its wait (+0x34: 20 seconds) and goes to 1, where every frame it asks the help system (sub_00203908, when no message
// runs: 0x179a10 clear and 0x179a34 at -1) for the level's hints, each guarded by the hero's place and the game's flags:
//   - the hero in box +0xc next to the Giant Clank pad (its moby +0x10, class 78, alive and not in its state 3), free to
//     listen: 0x3a98 (before Giant Clank is had, 0x13d4df) or 0x3a99 (after), repeated each 18 seconds of game time (the
//     reminders' minute counters 0x141d42 / 0x141d4a, the second's "seen" 0x141d48 and its level bits 0x141d4c);
//   - 0x15f5a4 set: 0x3a9f once (0x15f5a4 cleared) unless 0x141db0;
//   - the hero in box +0x54 in his movement mode 3: 0x3a9e (unless 0x161ac0, 0x141da8 or +0x22e0);
//   - the hero in box +0 for longer than its wait, the pad there: 0x3a9a (with 0x13d4df; the wait then 30 seconds);
//   - Giant Clank in box +0x1c: 0x3a9c (until 0x13d3f7, set by Giant Clank's animation 0xc);
//   - box +0x20 without the level's bit in 0x141d7c: 0x3a9d (unless 0x13d4c4);
//   - the hero in one of the boxes +0x20 / +0x24 / +0x28 for a minute (sub_0020d568 allowing): 0x4e2e;
//   - the hero in box +0x2c: 0x13d3f6 set.
// The boxes' latest one (+0x40) keeps a message from repeating; +0x3c / +0x38 count the frames in the boxes. The
// original's order of calls is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/variants_l15.h>

namespace rac1 {

using namespace quartu;

namespace {

constexpr uint32_t ITEMS = 0x0013d388;  // the items' and the help's flag bytes
constexpr uint32_t HINTS = 0x00141968;  // the help system's counters (+0x3da, +0x3e0, +0x3e2, +0x3e4)

bool no_message_running() { return lw(L15_ADDR(0x00179a10u)) == 0 && lw(L15_ADDR(0x00179a34u)) == -1; }

// the hero free to listen (his substate +0x208c below 2, or 9)
bool hero_free(uint32_t mode_word) { return mode_word < 2 || mode_word == 9; }

int32_t game_minutes10() { return ee_div(frame_scale_i(lw(0x0015eea4)), 0x258); }  // (the game time in 600-frame steps)

// The reminder `text` whose counter is the halfword `at`: asked when 18 seconds have passed since the counter's step or
// the counter is 0; else the counter moved up to the game time's step.
void reminder(uint32_t at, int32_t text, int32_t opt) {
    const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(at)) * 0x258;
    const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f))));
    if (limit < since || static_cast<int32_t>(lhu(at)) * 0x258 == 0) {
        sub_00203908(text, opt);
        return;
    }
    if (static_cast<int32_t>(lhu(at)) < game_minutes10()) sh(at, static_cast<uint32_t>(game_minutes10()));
}

}  // namespace

// orig: L15:0x002ed398
void qua_class1469_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x30, 0xff);
        sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x4b0)));
        sb(m + 0x20, 1);
        return;
    }
    if (state != 1) return;

    // ---- the pad's hint
    if (const int32_t k = lw(pv + 0x10); k != -1) {
        const uint32_t pad = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(k) << 8);
        const uint32_t st = pad != 0 ? lbu(pad + 0x20) : 0;
        if (pad != 0 && lh(pad + 0xa6) == 0x4e && st != 0xfe && st != 0xfd && st != 3 && point_in_box(HERO_POS, lw(pv + 0xc)) != 0 &&
            hero_free(lwu(HERO + 0x208c)) && no_message_running()) {
            if (lbu(0x0013d4df) == 0) {
                reminder(HINTS + 0x3da, TXT(0x3a98), 0x7b);
            } else if (lhu(HINTS + 0x3e0) != 0) {
                reminder(HINTS + 0x3e2, TXT(0x3a99), 0x7c);
            } else {
                sh(HINTS + 0x3e0, 1);
                if (static_cast<int32_t>(lhu(HINTS + 0x3e2)) < game_minutes10()) sh(HINTS + 0x3e2, static_cast<uint32_t>(game_minutes10()));
                sw(HINTS + 0x3e4, lwu(HINTS + 0x3e4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);  // (1: the state)
            }
        }
    }
    if (lw(L15_ADDR(0x0015f5a4u)) != 0 && no_message_running() && lhu(0x00141db0) == 0) {
        sub_00203908(TXT(0x3a9f), 0x89);
        sw(L15_ADDR(0x0015f5a4u), 0);
    }

    // ---- box +0x54: the hero in his movement mode 3
    if (point_in_box(HERO_POS, lw(pv + 0x54)) != 0 && lbu(HERO + 0x20a4) == 3 && lh(HERO + 0x22e0) == 0 && lw(L15_ADDR(0x00161ac0u)) == 0 &&
        lhu(0x00141da8) == 0)
        sub_00203908(TXT(0x3a9e), 0x88);
    int32_t box = -1, pad_index = -1;  // (the box the hero is in and the pad moby it names)
    bool in_box = false;
    // ---- box +0: the hero near the pad for longer than the wait
    if (point_in_box(HERO_POS, lw(pv + 0x0)) != 0) {
        box = lw(pv + 0x0);
        pad_index = lw(pv + 0x10);
        in_box = true;
    }
    if (in_box) {
        sw(pv + 0x3c, lwu(pv + 0x3c) + 1);
    } else {
        sw(pv + 0x3c, 0);
        sw(pv + 0x40, 0xffffffffu);
    }
    if (pad_index != -1) {
        const uint32_t pad = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(pad_index) << 8);
        const uint32_t st = pad != 0 ? lbu(pad + 0x20) : 0;
        if (pad != 0 && lh(pad + 0xa6) == 0x4e && st != 0xfe && st != 0xfd && st != 3 && lw(pv + 0x34) < lw(pv + 0x3c) && box != -1 &&
            box != lw(pv + 0x40) && hero_free(lwu(HERO + 0x208c)) && no_message_running()) {
            if (lbu(0x0013d4df) != 0) {
                sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x708)));
                if (lw(0x00141d54) >= 0) sub_00203908(TXT(0x3a9a), 0x7d);
                sw(pv + 0x40, static_cast<uint32_t>(box));
                sw(pv + 0x3c, 0);
            }
        }
    }
    // Giant Clank's animation 0xc in his mode 2: 0x13d3f7 set
    if (lbu(lwu(HERO_MOBY) + 0x52) == 0xc && lbu(HERO_FORM) == 2) sb(ITEMS + 0x6f, 1);

    // ---- box +0x1c: Giant Clank there
    if (point_in_box(HERO_POS, lw(pv + 0x1c)) != 0 && lbu(HERO + 0x20a4) == 2 && lbu(ITEMS + 0x6f) == 0 && no_message_running() &&
        lw(0x00141d6c) >= 0)
        sub_00203908(TXT(0x3a9c), 0x80);

    // ---- box +0x20: the level's bit not in 0x141d7c
    if (point_in_box(HERO_POS, lw(pv + 0x20)) != 0 && no_message_running() && lbu(0x0013d4c4) == 0 &&
        (lwu(0x00141d7c) & (1u << (lwu(0x0015ed84) & 31))) == 0)
        sub_00203908(TXT(0x3a9d), 0x82);
    box = -1;
    in_box = false;
    // ---- the boxes +0x20 .. +0x28: the hero in one of them for a minute
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t e = pv + 0x20 + (k << 2);
        if (point_in_box(HERO_POS, lw(e)) == 0) continue;
        in_box = true;
        if (lw(pv + 0x40) != lw(e)) box = lw(e);
        break;
    }
    if (in_box) sw(pv + 0x38, lwu(pv + 0x38) + 1);
    if (!in_box || lw(HERO + 0x2084) == 0x35) {
        sw(pv + 0x38, 0);
        sw(pv + 0x40, 0xffffffffu);
    }
    if (frame_scale_i(0xe10) < lw(pv + 0x38) && box != -1 && sub_0020d568() != 0 && no_message_running() && lbu(0x0013d4c4) != 0) {
        sub_00203908(TXT(0x4e2e), 0x78);
        sw(pv + 0x40, static_cast<uint32_t>(box));
        sw(pv + 0x38, 0);
    }
    // ---- box +0x2c: 0x13d3f6 set
    if (lbu(ITEMS + 0x6e) == 0 && point_in_box(HERO_POS, lw(pv + 0x2c)) != 0) sb(ITEMS + 0x6e, 1);
}

}  // namespace rac1
