// Rilgar's class 1347, the level's hint giver: an invisible moby that, each frame, starts the level's help dialogues
// (sub_00203908: a dialogue record and its slot in the game record) when the hero is in its boxes and the hint's
// conditions hold, keeps a few of the game record's counters, and wakes a class-0x33e moby once two switches are done.
// Like Eudora's class 1343 (eudora_help.cpp) and Aridia's class 1324 (aridia_class1324.cpp, whose inlined hint helpers
// it shares). Hand-ported from Rilgar's overlay (`orig: L5:`); declared in rilgar_mobys.h, registered in
// rilgar_mobys.cpp. The base ELF's data (the hero 0x13f350, the game record 0x141968 and the statistics before it
// 0x141720 / 0x141848, the items owned 0x13d4c0, the switches 0x14c050, the difficulty rows 0x14ee90, the play time
// 0x15eea4) are bare; Rilgar's own (the frame counter, the moby table) are L5_ADDR. The dialogue ids are the records
// sub_00203908 starts, not text ids.
//
// The game record's slots are 8 bytes (slot n at 0x141968 + n * 8): +0 how often, +2 when last (play steps of 0x258
// frames), +4 the levels it happened on (a bit per level, bit 31 always: negative once given).
//
// pvars (boxes are level box indices; the switch words are columns of the level's row of 0x14c050):
//   +0    the hero's position, kept every frame          +0x10 his position while he is not in the air (+0x30e 0)
//   +0x20 a switch column (also the difficulty row)      +0x24 the box of the Hoverboard's hints (slots 0x23 / 0x24)
//   +0x2c the box of the hint 0x138d (slot 0x26)         +0x34, +0x38 the boxes of the falls' hint 0x138e (slot 0x5e)
//   +0x40 the box of the hint 0x7dd (slot 0x48)          +0x48 the box of the hint 0x4e20 (slot 0x4a)
//   +0x50 the box of the swimming hint 0x3f0 (slot 0x44) +0x54 a moby index to wake (-1 once woken), +0x58 a switch column
//   +0x60 the falls counted in the boxes +0x34 / +0x38   +0x68 the frames swum in the box +0x50
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;       // the hero's block (+0x80 his position, +0x2084 his state before, +0x208c his state)
constexpr uint32_t HERO = 0x0013f3d0;    // his position
constexpr uint32_t STATS = 0x00141848;   // the statistics records (8 bytes each, as the game record's)
constexpr uint32_t RECORD = 0x00141968;  // the game record (slot n at + n * 8)
constexpr uint32_t OWNED = 0x0013d4c0;   // the items owned (a byte each)

bool hero_in(uint32_t pv, uint32_t off) { return point_in_box(HERO, lw(pv + off)) != 0; }

// the frames since the time (in play steps, the half at `when`) of a record
int32_t frames_since(uint32_t when) {
    return static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4))) - lhu(when) * 0x258u);
}

// the bit of the current level in a record's +4
uint32_t level_bit() { return 1u << (lwu(0x0015ed84) & 31); }

// the level's switch of column `col` (0x14c050 + the level * 16) done (0xff)
bool switch_done(uint32_t col) { return lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + col) == 0xff; }
}  // namespace

// orig: L5:0x0031bf40
void ril_class1347_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x30, 0xff);

    // ---- the box +0x50: swimming (state 0x12) there for more than 6 seconds gives the hint 0x3f0 (unless given on this
    // level); swimming in state 0x11, or with the item byte 0x13d395, the hint is barred for good (its count 0xffff)
    if (hero_in(pv, 0x50)) {
        bool hinted = false;
        if (lw(P + 0x208c) == 0x12) {
            const int32_t frames = lw(pv + 0x68);
            sw(pv + 0x68, static_cast<uint32_t>(frames + 1));
            if (frame_scale_i(0x168) < frames) {
                if ((lwu(RECORD + 0x224) & level_bit()) == 0) sub_00203908(0x3f0, 0x44);
                hinted = true;
            }
        }
        if (!hinted && (lw(P + 0x208c) == 0x11 || lbu(0x0013d395) != 0)) sh(RECORD + 0x220, 0xffff);
    }

    // ---- the box +0x40: the hint 0x7dd an hour (scaled) after the statistic +0x48's time, unless given on this level
    if (hero_in(pv, 0x40)) {
        const int32_t since = frames_since(STATS + 0x4a);
        const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0xe10)), 60.0f))));
        if (limit < since && (lwu(RECORD + 0x244) & level_bit()) == 0) sub_00203908(0x7dd, 0x48);
    }

    // ---- in the level's first second: the hint 0x4e27 (once ever) when the statistic +0x60 is counted and +0x58 not; the
    // hint 0x4e28 (once ever) when the statistic +0xe8 is, which is cleared
    if (lw(L5_ADDR(0x0015f5cc)) < frame_scale_i(0x3c) && lhu(STATS + 0x60) != 0 && lhu(STATS + 0x58) == 0 &&
        lw(RECORD + 0x28c) >= 0) {
        sub_00203908(0x4e27, 0x51);
    }
    if (lw(L5_ADDR(0x0015f5cc)) < frame_scale_i(0x3c) && lhu(STATS + 0xe8) != 0) {
        if (lw(RECORD + 0x394) >= 0) sub_00203908(0x4e28, 0x72);
        sh(STATS + 0xe8, 0);
    }

    // ---- the box +0x24, the hero on no moby: without the Hoverboard (item 0x16) its hint 0x138a every 18 seconds (slot
    // 0x23); with it the hint 0x138b (slot 0x24: the first visit only counted), barred for good once he rides it (0x13)
    if (lbu(OWNED + 0x16) == 0) {
        if (hero_in(pv, 0x24) && lw(P + 0x2fc) == 0) {
            if (hint_due(RECORD + 0x11a)) {
                sub_00203908(0x138a, 0x23);
            } else {
                hint_seen(RECORD + 0x11a);
            }
        }
    } else {
        if (hero_in(pv, 0x24) && lw(P + 0x2fc) == 0) hint_visit(RECORD + 0x120, 0x138b, 0x24);
        if (lw(P + 0x208c) == 0x13) sh(RECORD + 0x120, 0xffff);
    }

    // ---- the slot 0x26 given fewer than twice: with the switch +0x20 done its neighbour slot 0x25 counted; else, at the
    // difficulty 3 or more, in the box +0x2c, with none of five weapons used for 5 minutes (scaled): the hint 0x138d (once
    // per level set)
    if (lhu(RECORD + 0x130) < 2) {
        if (switch_done(lwu(pv + 0x20))) {
            hero_stat_use(RECORD + 0x128);
        } else if (!(lw(0x0014ee90 + (lwu(pv + 0x20) << 2)) < 3) && hero_in(pv, 0x2c)) {
            bool unused = true;
            for (const uint32_t when : {0x001417a2u, 0x0014179au, 0x001417c2u, 0x00141772u, 0x001417aau}) {
                if (!(frame_scale_i(0x4650) < frames_since(when))) {
                    unused = false;
                    break;
                }
            }
            if (unused && lw(RECORD + 0x134) >= 0) sub_00203908(0x138d, 0x26);
        }
    }

    // ---- the flag 0x13d388 set, the slot 0x22 never given: the hint 0x1389 when the hero is in state 0x16
    if (lbu(0x0013d388) != 0 && lhu(RECORD + 0x110) == 0 && lw(P + 0x208c) == 0x16) sub_00203908(0x1389, 0x22);

    // ---- the falls' hint (slot 0x5e, until given): the hero in state 0xa before in a box +0x34 / +0x38 counts the slot;
    // a fall (state 0x12) whose last grounded point (+0x10) is in one of the boxes is counted at +0x60 (the point cleared);
    // from the second, the hint 0x138e while he is in one of the boxes
    if (lhu(RECORD + 0x2f0) == 0) {
        if (lw(P + 0x2084) == 0xa && (hero_in(pv, 0x34) || hero_in(pv, 0x38))) hero_stat_use(RECORD + 0x2f0);
        if (lw(P + 0x208c) == 0x12) {
            const uint32_t grounded = pv + 0x10;
            if (point_in_box(grounded, lw(pv + 0x34)) != 0 || point_in_box(grounded, lw(pv + 0x38)) != 0) {
                vec_clear(V(grounded));
                sw(pv + 0x60, lwu(pv + 0x60) + 1);
            }
        } else if (!(lw(pv + 0x60) < 2) && (hero_in(pv, 0x34) || hero_in(pv, 0x38))) {
            sub_00203908(0x138e, 0x5e);
        }
    }

    // ---- the box +0x48 with item 0xf owned and 20 or more of 0x13d464: the hint 0x4e20 (until the statistic +0x90 or the
    // slot 0x4a is counted)
    if (lhu(STATS + 0x90) == 0 && lhu(RECORD + 0x250) == 0 && lbu(OWNED + 0xf) != 0 && hero_in(pv, 0x48) &&
        !(lw(0x0013d464) < 20)) {
        sub_00203908(0x4e20, 0x4a);
    }

    // ---- the moby +0x54 woken (class 0x33e's wake) once both switches +0x58 and +0x20 are done: in the level's first 5
    // frames, or while the hero stands on a moby of class 0x3e6 or 0x33e
    const int32_t sleeper = lw(pv + 0x54);
    if (sleeper != -1 && switch_done(lwu(pv + 0x58)) && switch_done(lwu(pv + 0x20))) {
        bool wake = lw(L5_ADDR(0x0015f5cc)) < 5;
        if (!wake) {
            const uint32_t on = lwu(P + 0x2fc);
            wake = on != 0 && (lh(on + 0xa6) == 0x3e6 || lh(on + 0xa6) == 0x33e);
        }
        if (wake) {
            moby_33e_wake(lwu(L5_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(sleeper) << 8));
            sw(pv + 0x54, 0xffffffffu);
        }
    }

    // ---- the hero's position kept, and where he last was on the ground
    V(pv) = V(HERO);
    if (lh(P + 0x30e) == 0) V(pv + 0x10) = V(HERO);
}

}  // namespace rac1
