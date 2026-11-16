// Eudora's class 1343, the level's help-message controller: an invisible moby that, each frame, asks for the level's
// hints (sub_00203908: a message and its option) when the hero stands in its boxes and the hint's conditions hold, and
// keeps a few statistics and goal flags of the level. Like Novalis's class 1341 (help_controller_update_l1 in
// novalis_mobys.cpp). Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`);
// Eudora-only data is L4_ADDR, the base ELF's are bare.
//
// The help records (0x141968 + option * 8) and the statistics records before them (from 0x141848) are 8 bytes each: +0
// how often (hero_stat_use counts it), +2 when last (in 600-frame units of the play time 0x15eea4), +4 the levels it
// happened on (a bit per level, bit 31 always). sub_00203908 shows a message only while no other one is up.
//
// pvars (boxes are level box indices, -1 none; ids are level moby indices, -1 none):
//   +0    the box of the Glove of Doom's hints: the third entry on (counted in the statistics record +0xc0, one per
//         visit: +0x14, the hero's losses 0x15eeac at the visit's first frame, -1 before), 0xfa1 without the glove, 0xfa0
//         with it once its use (0x1417c0's time) is old
//   +4    the box of the Trespasser's hints: without it 0xfa3 (again every 18 seconds), with it 0xfa4 (counting the frames
//         +8, unless +0xc is set: cleared once the hero is out of the box) until the pad +0x48 is solved; on a
//         Trespasser pad in this box the pad's hints 0x7db and, after 0x708 frames (+0x1c), 0x7dc
//   +0x2c the box where the hero's leaving his states 0, 1, 9 is counted (+0x24, latched by +0x28): from the third time
//         the hint 0xfa5, until the statistics record +0xc8 is set (the hero in those states in the box +0x30)
//   +0x34, +0x38 the boxes of the Hydrodisplacer's hint 0x3ee (at the third and sixth frame +0x20 near an unfinished pump)
//   +0x3c the box of the hint 0x4e2d; +0x50 the box of the hint 0x3ef
//   +0x40, +0x44 two pumps (class 280) and +0x48, +0x4c two Trespasser pads (class 615): their goal flags 0x13d3a6 /
//         0x13d3a7 set once the pumps are in state 5, 0x13d3a4 / 0x13d3a5 once the pads are in state 4 (solved)
// States: 0 set up, 1 runs.
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;     // the hero's block
constexpr uint32_t HERO = 0x0013f3d0;  // (his position: P + 0x80)
constexpr uint32_t S = 0x00141848;     // the statistics records
constexpr uint32_t H = 0x00141968;     // the help records (option n's at H + n * 8)

// whether the hero stands in the box of the pvar `off`
bool in_box(uint32_t pv, uint32_t off) { return point_in_box(HERO, lw(pv + off)) != 0; }

// whether no message is up (the message state: +0 busy, +0x24 its message, -1 none)
bool message_idle() { return lw(L4_ADDR(0x00179590)) == 0 && lw(L4_ADDR(0x001795b4)) == -1; }

// whether the hero is in his states 0, 1 or 9 (P + 0x208c)
bool hero_at_ease() {
    const uint32_t st = lwu(P + 0x208c);
    return st < 2 || st == 9;
}

// the play time in 600-frame units
int32_t play_time_units() { return ee_div(frame_scale_i(lw(0x0015eea4)), 600); }

// the frames since the time of a record (`when`: its +2)
int32_t frames_since(uint32_t when) {
    return static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4))) - lhu(when) * 600u);
}

// the level's moby of index `id` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t id) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(id) << 8); }

// whether the moby of the pvar `off` (-1 none) is of class `cls` in state `st`
bool moby_in_state(uint32_t pv, uint32_t off, int32_t cls, uint32_t st) {
    const int32_t id = lw(pv + off);
    if (id == -1) return false;
    const uint32_t o = moby_at(id);
    return lh(o + 0xa6) == cls && lbu(o + 0x20) == st;
}

// The hint `msg` (option `opt`) when its record has no time yet or its time is more than 18 seconds (frame-scaled) old;
// else its time brought up to now.
void hint_again(int32_t msg, int32_t opt) {
    const uint32_t when = H + static_cast<uint32_t>(opt) * 8 + 2;
    const int32_t ago = frames_since(when);
    const int32_t wait = float_to_int(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f));
    if (wait < ago || lhu(when) * 600u == 0) {
        sub_00203908(msg, opt);
        return;
    }
    if (static_cast<int32_t>(lhu(when)) < play_time_units()) sh(when, static_cast<uint32_t>(play_time_units()));
}
}  // namespace

// orig: L4:0x002e4418
void eud_class1343_update_l4(uint32_t m) {
    ee_stack_alloc(0xa0);  // (the original's frame; it keeps no locals there)
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sw(pv + 0x14, 0xffffffffu);
        ee_stack_free(0xa0);
        return;
    }
    if (state != 1) {
        ee_stack_free(0xa0);
        return;
    }

    // ---- the box +0: a visit counted at its first frame; from the third visit on the Glove of Doom's hints
    // (the moby's byte of the level's table 0x14c050 + level * 16 not 0xff)
    if (in_box(pv, 0) && message_idle() && lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) != 0xff) {
        if (lw(pv + 0x14) == -1) {
            sw(pv + 0x14, lwu(0x0015eeac));
            hero_stat_use(S + 0xc0);
        } else if (!(lhu(S + 0xc0) < 3)) {
            if (lbu(0x0013d4d4) == 0) {  // (no Glove of Doom)
                if (lhu(H + 0xe8) == 0) sub_00203908(0xfa1, 0x1d);
            } else if (frame_scale_i(0x4650) < frames_since(0x001417c2) && lhu(H + 0xe0) == 0) {  // (5 minutes)
                sub_00203908(0xfa0, 0x1c);
            }
        }
    }

    // ---- the boxes +0x34 / +0x38, the Hydrodisplacer in hand (state 0x3b): each frame near an unfinished pump counted,
    // the hint at the third and the sixth
    if (lhu(H + 0x210) < 4 && (in_box(pv, 0x34) || in_box(pv, 0x38)) && lw(P + 0x2090) == 0x3b && lw(P + 0x198) == 0) {
        for (uint32_t o = lwu(L4_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {  // (the live mobys)
            if (lh(o + 0xa6) != 0x118) continue;
            if (!(vec3_distance(V(o + 0x10), V(HERO)) < 3.0f)) continue;
            if (lwc1(lwu(o + 0x78)) == 1.0f) continue;  // (the pump's work done)
            const uint32_t n = lwu(pv + 0x20) + 1;
            sw(pv + 0x20, n);
            if (n == 3 || n == 6) sub_00203908(0x3ee, 0x42);
        }
    }

    // ---- the Suck Cannon's hint once its item flag 0x13d4e8 + 9 is set
    if (lbu(0x0013d4f1) != 0 && message_idle() && lhu(H + 0xf0) == 0) sub_00203908(0xfa2, 0x1e);

    // ---- the box +0x3c
    if (in_box(pv, 0x3c) && lhu(S + 0xf8) == 0 && message_idle() && lhu(H + 0x3b8) == 0) sub_00203908(0x4e2d, 0x77);

    // ---- the goal flags of the two Trespasser pads and the two pumps
    if (moby_in_state(pv, 0x48, 0x267, 4)) sb(0x0013d3a4, 1);
    if (moby_in_state(pv, 0x4c, 0x267, 4)) sb(0x0013d3a5, 1);
    if (moby_in_state(pv, 0x40, 0x118, 5)) sb(0x0013d3a6, 1);
    if (moby_in_state(pv, 0x44, 0x118, 5)) sb(0x0013d3a7, 1);

    // ---- the box +4: the Trespasser's hints
    if (in_box(pv, 4) && hero_at_ease() && message_idle()) {
        if (lbu(0x0013d4da) == 0) {  // (no Trespasser)
            hint_again(0xfa3, 0x1f);
        } else if (moby_in_state(pv, 0x48, 0x267, 4)) {  // (its pad solved)
            hero_stat_use(S + 0x88);
        } else if (lw(pv + 0xc) == 0 && lhu(S + 0x88) == 0) {
            sw(pv + 8, lwu(pv + 8) + 1);
            if (lhu(H + 0x100) != 0) {
                hint_again(0xfa4, 0x20);
            } else {
                hero_stat_use(H + 0x100);  // (the first time only its record: the hint 18 seconds later)
            }
        }
    } else if (lw(pv + 0xc) != 0 && !in_box(pv, 4)) {
        sw(pv + 0xc, 0);
    }

    // ---- the box +0x50: the hint 0x3ef, an hour (0x34bc0 frames) after the record 0x141890's time, on a level it was
    // not given on
    if (in_box(pv, 0x50) && hero_at_ease() && message_idle() && frame_scale_i(0x34bc0) < frames_since(0x00141892) &&
        (lwu(H + 0x21c) & (1u << (lwu(0x0015ed84) & 31))) == 0) {
        sub_00203908(0x3ef, 0x43);
    }

    // ---- on a Trespasser pad (the hero's state 0x72, the moby P + 0x2fc of class 615) in the box +4: the pad's hints, the
    // second after 0x708 frames there
    if (lhu(H + 0x230) == 0 || lhu(H + 0x238) == 0) {
        const uint32_t pad = lw(P + 0x2084) == 0x72 ? lwu(P + 0x2fc) : 0;
        if (pad == 0 || lh(pad + 0xa6) != 0x267 || point_in_box(P + 0x80, lw(pv + 4)) == 0) {
            sw(pv + 0x1c, 0);
        } else if (lhu(H + 0x230) == 0) {
            sub_00203908(0x7db, 0x46);
        } else if (lhu(H + 0x238) == 0) {
            sw(pv + 0x1c, lwu(pv + 0x1c) + 1);
            if (frame_scale_i(0x708) < lw(pv + 0x1c)) sub_00203908(0x7dc, 0x47);
        }
    }

    // ---- the box +0x30: the statistics record +0xc8 (the hero there in his states 0, 1, 9)
    if (in_box(pv, 0x30) && lhu(S + 0xc8) == 0 && hero_at_ease()) hero_stat_use(S + 0xc8);

    // ---- the box +0x2c: the times the hero leaves his states 0, 1, 9 there (latched until he is back in them)
    if (in_box(pv, 0x2c) && lhu(S + 0xc8) == 0 && !hero_at_ease() && lw(pv + 0x28) == 0) {
        sw(pv + 0x24, lwu(pv + 0x24) + 1);
        sw(pv + 0x28, static_cast<uint32_t>(frame_scale_i(0xb4)));
    } else if (lw(pv + 0x28) != 0 && hero_at_ease()) {
        sw(pv + 0x28, 0);
    }
    // from the third time, the hint 0xfa5
    if (in_box(pv, 0x2c) && lhu(S + 0xc8) == 0 && hero_at_ease() && !(lw(pv + 0x24) < 3) && message_idle() && lhu(H + 0x248) == 0)
        sub_00203908(0xfa5, 0x49);
    ee_stack_free(0xa0);
}

}  // namespace rac1
