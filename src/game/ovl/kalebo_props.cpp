// Kalebo III's (level 16) small classes: class 800, a crate that breaks when hit (damage kind 0x10000) into bolts and pieces;
// class 1387, the glow a barrier switch (class 546) turns off and on, its two half-axes scaled once; class 1561, the skill
// point of the level (Ratchet enters one box on foot, sub-state 0xf, then the other: "You just earned a Skill Point!");
// class 1812, the smoke of a scene's moby (L16 0x16cde4) from four of its chain points while a scene plays; class 1891,
// something high above the level (z 160) that turns slowly; classes 1439 and 1441, two builds of a pad that glows on a
// one-second cycle between two colours and, when Ratchet stands in its box (its own frame), lights up for a while; class
// 1442, a two-way switch that flips when hit (0x330000) or when Ratchet, swinging past it in the hero state 0x28, moves
// across it the way it faces (hero +0x5a8: 3 or 2). Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare.
// Registered in kalebo_mobys.cpp; checked by batch 250 (class800 .. class1891 and the P16.4 checks).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
using kalebo::Frame;
using kalebo::tick32;

constexpr uint32_t kHeroPos = 0x0013f3d0;  // the hero's position (PLAYER + 0x80)

// One of the two pad builds: its two colours, its box in its own frame (six floats: x above / below, y, z), its lit time.
struct PadBuild {
    uint32_t colour_a, colour_b, box, time;
    bool w_cleared;  // (class 1441: the offset's w lane cleared before the transform; class 1439 keeps the hero's)
};

// The pads: the colour on a 30-frame cycle (sin, 0 .. 1 from the level's frame counter) every frame; state 0 set-up, 1
// waits for Ratchet within 16 and inside its box, 2 lit until its timer (pvar +0x0) runs out.
void pad_update(uint32_t m, const PadBuild& b) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t period = frame_scale_i(30);
    const int32_t phase = ee_rem(lw(L16_ADDR(0x0015f5cc)), period);  // (the divide's zero trap: a period of 0 never comes)
    const float cycle = fpu_mul(fpu_div(int_to_float(phase), int_to_float(period)), u2f(0x40c90fd0u));  // (x 6.28318)
    const float s = sin_vu(fpu_sub(cycle, u2f(0x40490fd0u)));  // (- 3.14159)
    sw(m + 0x90, rgba_lerp(lwu(b.colour_a), lwu(b.colour_b), fpu_add(fpu_mul(s, 0.5f), 0.5f)));
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    case 1: {
        if (!(vec3_distance(V(m + 0x10), V(kHeroPos)) < 16.0f)) return;
        Mat4 to_local;
        mat3_transpose(to_local, M(m + 0xc0));
        Vec4 d;
        vec3_sub(d, V(kHeroPos), V(m + 0x10));
        if (b.w_cleared) d.w = 0.0f;
        mat4_transform(d, d, to_local);
        const uint32_t box = b.box;
        if (!(lwc1(box) < d.x) || !(d.x < lwc1(box + 4))) return;
        if (!(lwc1(box + 8) < d.y) || !(d.y < lwc1(box + 0xc))) return;
        if (!(lwc1(box + 0x10) < d.z) || !(d.z < lwc1(box + 0x14))) return;
        if (b.w_cleared) moby_sound_start(1, 0, m);
        sb(m + 0xbc, 1);
        sb(m + 0x20, 2);
        sw(pv, static_cast<uint32_t>(frame_scale_i(lw(b.time))));
        if (!b.w_cleared) moby_sound_start(1, 0, m);
        return;
    }
    case 2:
        if (tick32(pv) == 0) return;
        sb(m + 0x20, 1);
        sb(m + 0xbc, 0);
        return;
    default:
        return;
    }
}
}  // namespace

// Class 800: state 0 -> 1; 1 waits for a hit of kind 0x10000 with a damage above 0 (-> 2); 2 breaks: sound 0, the bolts,
// the plants' bolts, the pieces (class 0x74e, one, then 1 .. 11 at random, they stay 2), retired.
// orig: L16:0x002d6740
void kal_class800_update_l16(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        return;
    case 1:
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    case 2:
        moby_sound_start(0, 0, m);
        moby_drop_bolts(m, 0, -1);
        plant_bolts_drop_l1(m);
        debris_burst(m, 0x74e, 1, 0x74e, 1, 0xb, 2);
        moby_retire(m);
        return;
    default:
        return;
    }
}

// Class 1387: state 0 only: its half-axes +0xd0 / +0xe0 made pvar +0x0 / +0x4 times a quarter long, then state 1, shown
// (+0x34 | 0x101) and its matrix made.
// orig: L16:0x002e36e8
void kal_class1387_update_l16(uint32_t m) {
    if (lbu(m + 0x20) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    vec3_set_length(V(m + 0xd0), V(m + 0xd0), fpu_mul(lwc1(pv), 0.25f));
    vec3_set_length(V(m + 0xe0), V(m + 0xe0), fpu_mul(lwc1(pv + 4), 0.25f));
    sb(m + 0x20, 1);
    sh(m + 0x34, lhu(m + 0x34) | 0x101);
    moby_update_transform(m);
}

// Class 1561, the skill point: pvar +0x8 its stage. 0: Ratchet on foot (sub-state 0xf) in box pvar +0x0 -> 1. 1: Ratchet in
// the state 0x42 -> 0; else in box pvar +0x4 with the point not yet earned (0x13d421): earned, sound 1, the message.
// orig: L16:0x002e7208
void kal_class1561_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t stage = lwu(pv + 8);
    if (stage == 0) {
        if (point_in_box(kHeroPos, lw(pv)) != 0 && lw(0x001413dc) == 0xf) sw(pv + 8, 1);
    } else if (stage == 1) {
        if (lw(0x001413d4) == 0x42) {
            sw(pv + 8, 0);
        } else if (point_in_box(kHeroPos, lw(pv + 4)) != 0 && lbu(0x0013d421) == 0) {
            sb(0x0013d421, stage);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);  // ("You just earned a Skill Point!")
        }
    }
}

// Class 1812: state 0 -> 1 (+0x30 = 0xff); 1: while a scene plays (0x15f5c4 == 2, 0x16cc90 clear) two puffs of smoke from
// the scene's moby (L16 0x16cde4), from its chain points 6 and 7, heading away from points 4 and 5.
// orig: L16:0x002e7e00
void kal_class1812_update_l16(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    case 1: {
        if (lw(L16_ADDR(0x0015f5c4)) != 2) return;
        if (lw(L16_ADDR(0x0016cc90)) != 0) return;
        const uint32_t src = lwu(L16_ADDR(0x0016cde4));
        Frame f(0xb0);  // (the original's frame: the points at +0x0 .. +0x30, the directions at +0x40 / +0x50)
        const uint32_t sp = f.sp;
        for (uint32_t k = 0; k < 0x20; k += 0x10) {
            const uint32_t from = sp + k, back = sp + 0x20 + k, dir = sp + 0x40 + k;
            moby_chain_point_v(src, 6 + (k >> 4), V(from));
            moby_chain_point_v(src, 4 + (k >> 4), V(back));
            vec3_sub(V(dir), V(from), V(back));
            vec3_set_length(V(dir), V(dir), u2f(0x3d99999au));  // (0.075)
            smoke_burst(from, from, dir, 20000.0f, 1000.0f);
        }
        return;
    }
    default:
        return;
    }
}

// Class 1891: state 0 its scale +0x2c times 4, state 1, +0x30 = 0xff, +0x32 = 0x200, raised to z 160; 1 turns (yaw
// -0.2618 a second).
// orig: L16:0x002e88d0
void kal_class1891_update_l16(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0x200);
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 4.0f));
        swc1(m + 0x18, 160.0f);
        return;
    case 1:
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed6c), u2f(0xbe860a92u))));
        return;
    default:
        return;
    }
}

// orig: L16:0x002e5010
void kal_class1439_update_l16(uint32_t m) {
    pad_update(m, PadBuild{L16_ADDR(0x00161ddc), L16_ADDR(0x00161de0), L16_ADDR(0x001d9750), L16_ADDR(0x00161dd8), false});
}

// orig: L16:0x002e5258
void kal_class1441_update_l16(uint32_t m) {
    pad_update(m, PadBuild{L16_ADDR(0x00161de8), L16_ADDR(0x00161dec), L16_ADDR(0x001d9768), L16_ADDR(0x00161de4), true});
}

// Class 1442's flip: its phase (pvar +0x0) to -pi, state 2, lit (+0xbc).
// orig: L16:0x002e56e0
void kal_switch_flip_l16(uint32_t m) {
    swc1(lwu(m + 0x78), u2f(0xc0490fdau));
    sb(m + 0x20, 2);
    sb(m + 0xbc, 1);
}

// Class 1442: state 0 a random phase, state 1; 1: once its timer (pvar +0x4) is not running, Ratchet within 4 (flat) in
// the state 0x28 with a direction (+0x5a8 not -1): the switch's point (L16 0x161df0 in its frame) seen from Ratchet's frame
// level with him (|y| < 0.1), on his right with the direction 3 or on his left with 2: flipped, sound 1. A hit (0x330000)
// in state 1 flips it too (sound 0). Every frame: its colour pulses (phase + 4 pi a second), green-tinted once lit.
// orig: L16:0x002e54a0
void kal_class1442_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t P = 0x0013f350;
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        swc1(pv, rand_angle());
        break;
    case 1: {
        if (tick32(pv + 4) != 0 && vec2_distance(V(m + 0x10), V(kHeroPos)) < 4.0f && lw(0x001413d4) == 0x28 &&
            lw(P + 0x5a8) != -1) {
            Vec4 v;
            mat3_transform(v, V(L16_ADDR(0x00161df0)), M(m + 0xc0));
            vec3_add(v, v, V(m + 0x10));
            vec3_sub(v, v, V(kHeroPos));
            mat3_transform(v, v, M(P + 0x40));
            if (abs_f(v.y) < u2f(0x3dcccccdu)) {  // (0.1)
                const float x = v.x;
                if ((0.0f < x && lw(P + 0x5a8) == 3) || (x < 0.0f && lw(P + 0x5a8) == 2)) {
                    kal_switch_flip_l16(m);
                    moby_sound_start(1, 0, m);
                }
            }
        }
        if (sub_0025a420(m, NOT_ADDR(0x330000), 0) != 0) {
            kal_switch_flip_l16(m);
            moby_sound_start(0, 0, m);
            sb(m + 0xa4, 0xff);
        }
        break;
    }
    default:
        break;
    }
    const float phase = angle_add(lwc1(pv), fpu_mul(lwc1(0x0015ed6c), u2f(0x41490fdbu)));  // (4 pi)
    swc1(pv, phase);
    const float t = fpu_mul(fpu_add(sin_vu(phase), 1.0f), 0.5f);
    sw(m + 0x90, rgba_lerp(lbu(m + 0xbc) != 0 ? 0x80208020u : 0x80202080u, 0x80202020u, t));
}

}  // namespace rac1
