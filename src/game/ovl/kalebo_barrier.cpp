// Kalebo III's (level 16) energy barriers: class 552, a barrier's post (the posts of a group at the same height pair up: one
// of each pair draws the beam, L16:0x2cf7a8, and hums while it is up), and class 546, the switch that takes it down. A switch
// glows dimly until it is hit (damage kinds 0x330000); then it is lit (its +0xbc set) and the barrier of its group is down
// while any switch of the group is lit; a switch with a time (pvar +0x64) goes out again when it runs out, blinking faster
// in its last 1.5 seconds. A switch also turns the level's class 1387 moby it names (pvar +0x60) off and on (that moby's +0x94).
// Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250
// (class546, class552, barrier_*).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
using kalebo::for_each_group_moby;
using kalebo::moby_at;
using kalebo::tick32;

constexpr int16_t kSwitchClass = 0x222;  // class 546
constexpr int16_t kPostClass = 0x228;    // class 552

// A class 1387 moby turned off (its +0x94 cleared) or on (+0x94 = its class's word +0x10); other classes are left alone.
// orig: L16:0x002e3770
void switch_link_set(uint32_t target, bool on) {
    if (lh(target + 0xa6) != 0x56b) return;
    sw(target + 0x94, on ? lwu(lwu(target + 0x24) + 0x10) : 0u);
}

// The switch's grey level from a glow value in 0 .. 1 (x 128, kept in 0x20 .. 0x80).
uint32_t switch_level(float glow) {
    const int32_t level = float_to_int(fpu_mul(glow, 128.0f));
    if (!(level < 129)) return 0x80;
    if (!(0x1f < level)) return 0x20;
    return static_cast<uint32_t>(level);
}

// The post of the same group at the same height as `post` (within 0.1), or 0.
// orig: L16:0x002cf678
uint32_t post_partner(uint32_t post) {
    uint32_t found = 0;
    uint32_t e = lvd::group(lbu(post + 0x21));
    if (e == 0) return 0;
    for (;; e += 2) {
        const uint32_t other = moby_at(lhu(e));
        if (lh(other + 0xa6) == kPostClass && other != post &&
            abs_f(fpu_sub(lwc1(other + 0x18), lwc1(post + 0x18))) < u2f(0x3dcccccdu)) {
            found = other;
            break;
        }
        if (lh(e) < 0) break;
    }
    return found;
}

// The posts of `post`'s group paired: every post but `post` hidden (+0x34 bit 2: it no longer runs), each one's partner put
// in its pvar +0x0.
// orig: L16:0x002cf5c8
void posts_pair_up(uint32_t post) {
    for_each_group_moby(lbu(post + 0x21), [post](uint32_t other) {
        if (lh(other + 0xa6) != kPostClass) return;
        if (other != post) sh(other + 0x34, lhu(other + 0x34) | 2u);
        const uint32_t pv = lwu(other + 0x78);
        sw(pv, post_partner(other));
    });
}

// 1 when no switch of `post`'s group is lit (or the group is empty), else 0.
// orig: L16:0x002cf738
uint32_t switches_all_out(uint32_t post) {
    uint32_t e = lvd::group(lbu(post + 0x21));
    if (e == 0) return 1;
    for (;; e += 2) {
        const uint32_t sw_moby = moby_at(lhu(e));
        if (lh(sw_moby + 0xa6) == kSwitchClass && lbu(sw_moby + 0xbc) != 0) return 0;
        if (lh(e) < 0) return 1;
    }
}

}  // namespace

// Class 546, the switch: state 0 sets it up (a random glow phase; lit and down for good when its byte in the level's table at
// 0x14c050 (16 a level) is 0xff and it has no time); state 1 glows dimly (a 1 Hz pulse) and waits for a hit; state 2 is lit.
// Its hit slot +0xa4 is cleared every frame.
// orig: L16:0x002cf198
void kal_class546_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    auto link = [pv](bool on) {
        const int32_t target = lw(pv + 0x60);
        if (target != -1) switch_link_set(moby_at(static_cast<uint32_t>(target)), on);
    };
    switch (lbu(m + 0x20)) {
    case 0: {
        swc1(pv + 0x68, rand_angle());
        swc1(pv + 0x30, u2f(0x3a83126fu));  // 0.001
        const uint32_t done = lbu(0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0));
        if (done == 0xff && lw(pv + 0x64) == -1) {
            sb(m + 0xbc, 1);
            sb(m + 0x20, 2);
            link(false);
        } else {
            sb(m + 0x20, 1);
        }
        break;
    }
    case 1: {
        const float phase = angle_add(lwc1(pv + 0x68), fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu)));  // 2 pi a second
        swc1(pv + 0x68, phase);
        const uint32_t level = switch_level(fpu_sub(fpu_mul(sin_vu(phase), 4.0f), 3.0f));
        sw(m + 0x90, 0x80000000u | (level << 16) | (level << 8) | level);
        if (sub_0025a420(m, NOT_ADDR(0x330000u), 0) == 0) break;
        sb(m + 0x20, 2);
        sw(m + 0x90, 0x80208020u);
        sb(m + 0xbc, 1);
        swc1(pv + 0x68, u2f(0x3fc90fdbu));  // pi / 2
        moby_sound_start(0, 0, m);
        link(false);
        if (lw(pv + 0x64) > 0) sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x64))));
        break;
    }
    case 2: {
        const uint32_t level = switch_level(fpu_add(fpu_mul(sin_vu(lwc1(pv + 0x68)), 0.5f), 0.5f));
        sw(m + 0x90, (level << 8) | 0x80200020u);
        if (lw(pv + 0x64) <= 0) break;
        if (tick32(pv + 0x6c) != 0) {
            sb(m + 0xbc, 0);
            sb(m + 0x20, 1);
            swc1(pv + 0x68, u2f(0x3fc90fdbu));
            link(true);
        } else if (lw(pv + 0x6c) < frame_scale_i(0x5a)) {
            swc1(pv + 0x68, angle_add(lwc1(pv + 0x68), fpu_mul(lwc1(0x0015ed6c), u2f(0x41c90fdbu))));  // 8 pi a second
        }
        break;
    }
    default:
        break;
    }
    sb(m + 0xa4, 0xff);
}

// Class 552, the post (hidden posts do nothing): state 0 pairs the group's posts up (when its own partner word is still 0);
// state 1 keeps the beam up while no switch of the group is lit (pvar +0x8 set, the beam's draw queued, its hum +0xc kept
// playing), and takes it down otherwise (+0x8 cleared, the hum stopped).
// orig: L16:0x002cf4a8
void kal_class552_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if ((lhu(m + 0x34) & 2u) != 0) return;
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (lwu(pv) != 0) return;
        posts_pair_up(m);
        sb(m + 0x20, 1);
        return;
    }
    if (state != 1) return;
    if (switches_all_out(m) != 0) {
        sw(pv + 0x8, state);
        sub_001f93e0(L16_ADDR(0x002cf7a8), m);
        if (sub_0028d8c0(m, lw(pv + 0xc)) == 0) sw(pv + 0xc, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        return;
    }
    if (lwu(pv + 0x8) == 0) return;
    const int32_t slot = lw(pv + 0xc);
    sw(pv + 0x8, 0);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0xc, 0xffffffffu);
}

}  // namespace rac1
