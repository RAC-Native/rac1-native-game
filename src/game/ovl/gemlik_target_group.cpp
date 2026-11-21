// Gemlik Base's (level 13) class 231: a shootable target of a group (its +0x21): hit, it lights green; when every
// member of the group is lit within the group's time, the group's mobys are given state 4, else 3 when the time runs
// out (a ticking that speeds up as it does). The group's first target (its leader) keeps the count and the time. Level
// 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Its pvars: +0x20 its hit record (+0x20 its health; +0x24 the health it is given back, 16 bits), +0x60 its colour
// pulse, +0x70 the leader's time left (16 bits), +0x72 the next tick (16 bits), +0x74 lit, +0x76 the blink's timer,
// +0x78 the blink's colour, +0x79 a member (not the leader), +0x7a the members lit (the leader's), +0x7b the group's
// size (the leader's), +0x7c the frames the group has (<= 0 none), +0x80 its leader, +0x84 the frames the group stays
// solved (16 bits; -1 for ever). States (the table L13:0x1f4df0): 0 set up, 1 waiting, 2 the time runs (the leader),
// 3 reset, 4 solved.
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

using namespace gml;

// The target's hits on its hit record `hs`: a hit that takes its health unlights its "can be hit" flag (+0x34 bit
// 0x1000) and flashes; a lesser one takes its damage. Whether the health went.
// orig: L13:0x002e4300
bool gml_2e4300_l13(uint32_t m, uint32_t pv, uint32_t hs) {
    const Frame f(128);
    const uint32_t sp = f.sp;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = lw(sp + 0);
    const int32_t react = moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
    if (react == 1 || react == 2) sw(hs + 0, 0);  // (the table L13:0x1f4d60: reactions 1 and 2 (0x002e438c))
    bool gone = false;
    if (st >= 2) {
        const float health = lwc1(hs + 0), dmg = lwc1(hit + 0x2c);
        if (health <= dmg) {
            sw(hs + 0, 0);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0x78);
            color_pulse_start(m, pv + 0x60);
            gone = true;
        } else {
            swc1(hs + 0, fpu_sub(health, dmg));
            sb(pv + 0x67, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            color_pulse_start(m, pv + 0x60);
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    return gone;
}

// orig: L13:0x002e4448
void gml_class231_update_l13(uint32_t m) {
    const Frame f(96);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    bool shot = false;
    if (lh(pv + 0x74) == 0) {
        const uint32_t st = lbu(m + 0x20);
        if (st == 1 || st == 2) shot = gml_2e4300_l13(m, pv, pv + 0x20);
    }
    // the next tick of the time left (a sound), sooner as the time runs out
    auto tick = [m, pv] {
        if (tick16(pv + 0x72) == 0) return;
        moby_sound_start(0, 0, m);
        const int32_t most = frame_scale_i(0x2d);
        const int32_t next = frame_scale_i(5) + ee_div(static_cast<int32_t>(lh(pv + 0x70)), frame_scale_i(0x28));
        if (most < next) sh(pv + 0x72, static_cast<uint32_t>(frame_scale_i(0x2d)));
        else sh(pv + 0x72, static_cast<uint32_t>(frame_scale_i(5) + ee_div(static_cast<int32_t>(lh(pv + 0x70)), frame_scale_i(0x28))));
    };
    // the time ran out: the group reset (state 3)
    auto time_out = [m, pv] {
        sh(pv + 0x74, 0);
        sb(pv + 0x7a, 0);
        moby_sound_start(2, 0, m);
        moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), 3);
        debug_printf_stub();
        sb(m + 0x20, 3);
    };
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4df0)
    case 0: {  // (0x002e44c0): the leader counts the group and names itself its members' leader
        if (lbu(pv + 0x79) == 0) {
            sb(pv + 0x7b, 0);
            sw(pv + 0x80, m);
            group_first(sp, static_cast<int32_t>(lbu(m + 0x21)), 0, 0);
            while (lwu(sp) != 0) {
                const uint32_t o = lwu(sp);
                if (o != m && lh(o + 0xa6) == 0xe7) {
                    sb(lwu(o + 0x78) + 0x79, 1);
                    sw(lwu(o + 0x78) + 0x80, m);
                }
                sb(pv + 0x7b, lbu(pv + 0x7b) + 1);
                group_next(sp, lwu(sp), 0, 0);
            }
        }
        sh(pv + 0x76, 0);
        sh(pv + 0x70, 0);
        sh(pv + 0x72, 0);
        sb(pv + 0x7a, 0);
        sh(pv + 0x74, 0);
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        sb(m + 0x20, 1);
        return;
    }
    case 1: case 2: {  // (0x002e457c): lit by a hit; the leader's time runs from the first
        if (lh(pv + 0x74) == 0 && shot) {
            const uint32_t lead = lwu(pv + 0x80);
            const uint32_t lpv = lwu(lead + 0x78);
            if (lbu(lead + 0x20) == 1) {
                sb(pv + 0x7a, 0);
                sb(lead + 0x20, 2);
                if (lw(lpv + 0x7c) > 0) sh(lpv + 0x70, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x7c))));
            }
            sb(lpv + 0x7a, lbu(lpv + 0x7a) + 1);
            moby_sound_start(3, 0, m);
            moby_set_color_hi(m, 0, 0xff, 0);
            sh(pv + 0x74, 1);
        }
        if (lbu(m + 0x20) != 2) return;
        if (lbu(pv + 0x7b) <= lbu(pv + 0x7a)) {  // all lit: solved
            moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), 4);
            moby_sound_start(1, 0, m);
            sh(pv + 0x70, lhu(pv + 0x84));
            debug_printf_stub();
            sb(m + 0x20, 4);
            return;
        }
        if (lw(pv + 0x7c) < 0) return;
        if (tick16(pv + 0x70) != 0) {
            time_out();
            return;
        }
        tick();
        return;
    }
    case 3:  // (0x002e476c): reset (unlit, its health back)
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        sb(pv + 0x7a, 0);
        swc1(pv + 0x20, int_to_float(static_cast<int32_t>(lh(pv + 0x24))));
        sh(pv + 0x74, 0);
        sb(m + 0x20, 1);
        return;
    case 4: {  // (0x002e47a8): solved; the leader's time (-1 for ever) runs out to the reset; it blinks
        if (lbu(pv + 0x79) == 0 && lh(pv + 0x70) != -1) {
            if (tick16(pv + 0x70) != 0) time_out();
            else tick();
        }
        if (tick16(pv + 0x76) == 0) return;
        sh(pv + 0x76, static_cast<uint32_t>(frame_scale_i(0x14)));
        if (lbu(pv + 0x78) != 0) {
            moby_set_color_hi(m, 0, 0xff, 0);
            sb(pv + 0x78, 0);
        } else {
            moby_set_color_hi(m, 0, 0, 0xff);
            sb(pv + 0x78, 1);
        }
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
