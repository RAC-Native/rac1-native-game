// Veldin's finale (level 18): what the boss (class 1422: its update in veldin_finale_boss.cpp) does to the
// level's other mobys, hand-written (part 1 of its port): its moby groups shown and hidden, the gates (class 583) it
// opens and picks, the floating platforms (class 587) and the lift (class 586) it drives, the robots (class 1906) it
// counts, the raft platforms (class 1584) it flags, and the level's sound starter. The boss's update (veldin_finale_boss.cpp) calls them. Level 18 only: the addresses are the finale's (`orig: L18:`), its
// own data L18_ADDR, the base ELF's bare (the sound slots 0x13e550). Declared in veldin_finale_own.h.
//
// A moby group is the level's list of moby indices (lvd::group: halfwords, the last with bit 15 set); its mobys are the
// level's moby table (L18:0x15ffd8) + index * 0x100.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr int16_t GATE_CLASS = 0x247;      // class 583
constexpr int16_t ROBOT_CLASS = 0x772;     // class 1906
constexpr int16_t PLATFORM_CLASS = 0x630;  // class 1584 (a raft's platform)

uint32_t first_moby() { return lwu(L18_ADDR(0x0015ffd8)); }

// each moby of the group `group` in its list's order (`f(moby)`); nothing when the group has no list
template <typename F>
void each_in_group(int32_t group, F f) {
    uint32_t e = lvd::group(group);
    if (e == 0) return;
    for (;; e += 2) {
        f(first_moby() + ((lhu(e) & 0x7fff) << 8));
        if (lh(e) < 0) break;
    }
}

// a moby the game has not taken out (its state 0xfe / 0xfd)
bool live(uint32_t o) { return o != 0 && lbu(o + 0x20) != 0xfe && lbu(o + 0x20) != 0xfd; }
}  // namespace

// ---- the level's sound

// The level's sound `idx` (from the level's first, 0x15f574; none past the count 0x15f5f0) started on the moby `moby`
// with the flags `flags`, its record from the level's sound table (0x15f5f4, 0x20 each) and the volume 0x400; the sound
// slot it took (0x13e550, 0x70 each) keeps the sound's number (+0x7e) and its moby (+0x88). The slot, -1 for none. (The
// same code as Blarg's level_sound_play_l6, L6:0x29aec0, and Kerwan's kerwan_level_sound_start_l3, L3:0x27a618: word
// for word but the call of each level's sound_start.)
// orig: L18:0x0028d878
int32_t vf_level_sound_l18(int32_t idx, uint32_t flags, uint32_t moby) {
    const int32_t snd = idx + lw(L18_ADDR(0x0015f574));
    if (!(snd < lw(L18_ADDR(0x0015f5f0)))) return -1;
    const int32_t slot = sound_start(lwu(L18_ADDR(0x0015f5f4)) + (static_cast<uint32_t>(snd) << 5), flags, moby, 0, 0x400);
    if (slot >= 0) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        sh(e + 0x7e, static_cast<uint32_t>(snd));
        sw(e + 0x88, moby);
    }
    return slot;
}

// ---- moby groups

// The group's live mobys hidden (+0x31 0, flag 1 of +0x34, no colour +0x94).
// orig: L18:0x002f6fa8
void vf_group_hide_l18(int32_t group) {
    each_in_group(group, [](uint32_t o) {
        if (!live(o)) return;
        sw(o + 0x94, 0);
        sb(o + 0x31, 0);
        sh(o + 0x34, lhu(o + 0x34) | 1);
    });
}

// The group's live mobys shown again: +0x31 1, flag 1 of +0x34 off, their class's colour (+0x24's +0x10) at +0x94.
// orig: L18:0x002f7028
void vf_group_show_l18(int32_t group) {
    each_in_group(group, [](uint32_t o) {
        if (!live(o)) return;
        sb(o + 0x31, 1);
        sh(o + 0x34, lhu(o + 0x34) & 0xfffe);
        sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10));
    });
}

// The byte +0xbc of the group's raft platforms (class 1584) set to `value`.
// orig: L18:0x002fa888
void vf_group_platforms_flag_l18(int32_t group, uint32_t value) {
    each_in_group(group, [value](uint32_t o) {
        if (lh(o + 0xa6) == PLATFORM_CLASS) sb(o + 0xbc, value);
    });
}

// How many of the group's robots (class 1906) are out of their state 8 (a 16-bit count).
// orig: L18:0x002fc7e0
int32_t vf_robots_out_l18(int32_t group) {
    int16_t n = 0;
    each_in_group(group, [&n](uint32_t o) {
        if (lh(o + 0xa6) == ROBOT_CLASS && lbu(o + 0x20) != 8) n = static_cast<int16_t>(n + 1);
    });
    return n;
}

// ---- the gates (class 583: pvars +0x24 its opening's goal, +0x2c its timer, +0x30 held open)

// The group's gate whose bearing from the point `from` differs most from the bearing of `from` from the point `ref`
// (0: none, or none differing). `from` and `ref` are EE points.
// orig: L18:0x002d6b58
uint32_t vf_gate_farthest_round_l18(uint32_t from, uint32_t ref, int32_t group) {
    float most = 0.0f;
    uint32_t best = 0;
    each_in_group(group, [&](uint32_t o) {
        if (lh(o + 0xa6) != GATE_CLASS) return;
        const float back = atan2_f(fpu_sub(lwc1(from + 0), lwc1(ref + 0)), fpu_sub(lwc1(from + 4), lwc1(ref + 4)));
        const float to = atan2_f(fpu_sub(lwc1(from + 0), lwc1(o + 0x10)), fpu_sub(lwc1(from + 4), lwc1(o + 0x14)));
        const float d = angle_dist(back, to);
        if (most < d) {
            most = d;
            best = o;
        }
    });
    return best;
}

// The group's gate nearest to the EE point `at` (within 255; 0: none).
// orig: L18:0x002d6c80
uint32_t vf_gate_nearest_l18(uint32_t at, int32_t group) {
    float nearest = 255.0f;
    uint32_t best = 0;
    each_in_group(group, [&](uint32_t o) {
        if (lh(o + 0xa6) != GATE_CLASS) return;
        const float d = vec3_distance(V(at), V(o + 0x10));
        if (d < nearest) {
            nearest = d;
            best = o;
        }
    });
    return best;
}

// Every gate of the group opened and held open (+0x24 1.5, +0x30 1).
// orig: L18:0x002d6d50
void vf_gates_open_l18(int32_t group) {
    each_in_group(group, [](uint32_t o) {
        if (lh(o + 0xa6) != GATE_CLASS) return;
        const uint32_t pv = lwu(o + 0x78);
        swc1(pv + 0x24, 1.5f);
        sw(pv + 0x30, 1);
    });
}

// The gate `gate` opened for five frames of its timer (+0x2c 5, +0x24 1.5).
// orig: L18:0x002d6dc0
void vf_gate_open_l18(uint32_t gate) {
    const uint32_t pv = lwu(gate + 0x78);
    sw(pv + 0x2c, 5);
    swc1(pv + 0x24, 1.5f);
}

// ---- the floating platforms (class 587) and the lift (class 586)

// A floating platform in its state 2 put in its state 5 (it sinks); 1 when it is in its state 1, else 0.
// orig: L18:0x002d8858
uint32_t vf_float_sink_l18(uint32_t o) {
    const uint32_t st = lbu(o + 0x20);
    if (st == 1) return 1;
    if (st == 2) sb(o + 0x20, 5);
    return 0;
}

// A floating platform in its state 1 put in its state 3 with the waits `sink_wait` (+0x78) and `rock_wait` (+0x7c, the
// wait before it rocks); 1 when it was, 0 in any other state.
// orig: L18:0x002d8888
uint32_t vf_float_rock_l18(uint32_t o, int32_t rock_wait, int32_t sink_wait) {
    if (lbu(o + 0x20) != 1) return 0;
    const uint32_t pv = lwu(o + 0x78);
    sb(o + 0x20, 3);
    sw(pv + 0x78, static_cast<uint32_t>(sink_wait));
    sw(pv + 0x7c, static_cast<uint32_t>(rock_wait));
    return 1;
}

// The lift as the boss reads it: in its state 3 put back in its state 1, 1; in its state 2, -1 once its wait (+0x6c) is
// over, else 0; 0 in any other state.
// orig: L18:0x002d8050
int32_t vf_lift_poll_l18(uint32_t o) {
    const uint32_t st = lbu(o + 0x20);
    if (st == 3) {
        sb(o + 0x20, 1);
        return 1;
    }
    if (st != 2) return 0;
    return lw(lwu(o + 0x78) + 0x6c) == 0 ? -1 : 0;
}

// The lift sent up (state 2, its wait +0x6c `wait` frames) unless it is already, and its sound (the level's sound 0x12)
// started when its slot (+0x70) no longer plays it.
// orig: L18:0x002d7f68
void vf_lift_start_l18(uint32_t o, int32_t wait) {
    const uint32_t pv = lwu(o + 0x78);
    if (lbu(o + 0x20) != 2) {
        sb(o + 0x20, 2);
        sw(pv + 0x6c, static_cast<uint32_t>(wait));
    }
    if (sub_0028d8c0(o, lw(pv + 0x70)) == 0) sw(pv + 0x70, static_cast<uint32_t>(vf_level_sound_l18(0x12, 0, o)));
}

// The lift on its way up (state 2) stopped at its state 3, its sound stopped while its slot still plays it for the lift
// (the slot's +0x88 the lift, +0x74 playing); the slot word -1.
// orig: L18:0x002d7fd0
void vf_lift_stop_l18(uint32_t o) {
    if (lbu(o + 0x20) != 2) return;
    const uint32_t pv = lwu(o + 0x78);
    sb(o + 0x20, 3);
    const int32_t slot = lw(pv + 0x70);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == o && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x70, 0xffffffffu);
}

}  // namespace rac1
