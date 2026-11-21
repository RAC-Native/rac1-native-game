// Kalebo III (level 16), class 1445: a walker that keeps to a path's ground (pvars +0x1f0, the level's path: its points
// are its places to pick a target in, and the line it is pushed back behind) and goes for whatever it targets there
// (target_select within 16 of its home, pvars +0x1d0; the hero when nothing else). Its record at pvars +0x180 is
// target_select's (+0 the point, +0x40 the moby, +0x44 the result: 2 lost). It walks to the target (4), and within 1.25
// stops and swings (5: its chain's contact at the animation's frame 13), walking on again when the target moves away
// past 1.5; with the target lost it walks home (6) and waits there (1). Hit (kind 0x330000) it is knocked away (8:
// enemy_physics on its move record +0x120) and bursts, dropping bolts, where it lands; it can be carried (7: carry_update
// on the same record) and is put back to wait (1). Its head joint scaled (2.1), its floor band near the camera, its
// colour pulse (+0x110). Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered in
// kalebo_mobys.cpp; checked by batch 250 (class1445, the P16.5 checks).
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/walker.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;

float dt() { return lwc1(0x0015ed6cu); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70u); }  // its square
float turn() { return u2f(0x41490fdbu); }  // (4 pi: the turn's rate)

// the animation `set` over 20 frames, unless it plays already
void anim_to(uint32_t m, uint32_t set) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(20));
}
}  // namespace

// Every frame but in state 0: a hit of kind 0x330000 knocks it away (its move record +0x120: the level's speeds
// 0x161e54 .. 0x161e60, state 8, its colour pulse started); its colour pulse; its target (target_select on its path's
// points, within 16: the record +0x180) lost (+0x1c4 = 2) when farther than 16 from its home (+0x1d0, flat) or more than
// 3 above or below it; with no target moby (+0x1c0) the hero, at his place.
// orig: L16:0x002e6208
void kal_walker_frame_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    const Frame f(0x80);
    const uint32_t sp = f.sp;
    if (const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0); hit != 0) {
        const float t2 = dt2(), t = dt();
        sb(pv + 0x15d, 0);
        swc1(pv + 0x148, 0.5f);
        swc1(pv + 0x13c, fpu_mul(lwc1(L16_ADDR(0x00161e5c)), t));
        swc1(pv + 0x130, fpu_mul(lwc1(L16_ADDR(0x00161e54)), t2));
        swc1(pv + 0x134, fpu_mul(lwc1(L16_ADDR(0x00161e58)), t2));
        swc1(pv + 0x16c, fpu_add(dt(), dt()));
        swc1(pv + 0x138, fpu_mul(lwc1(L16_ADDR(0x00161e60)), t));
        sw(pv + 0x144, 0x29);
        sw(pv + 0x140, 0x200);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        lq_sq(sp, hit + 0x10);  // (the hit's direction)
        vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x10)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                         *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
        move_state_start(m, pv + 0x120, 5, 1, 0, lwc1(sp + 0x10));
        swc1(pv + 0x170, 8.0f);
        swc1(pv + 0x174, 16.0f);
        sb(m + 0x20, 8);
        sw(m + 0x94, 0);
        sb(pv + 0x117, 0x78);
        color_pulse_start(m, pv + 0x110);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x110);
    const uint32_t path = lvd::path(lw(pv + 0x1f0));
    if (target_select(m, pv + 0x180, 0, 0, path + 0x10, lw(path), 16.0f) != 2) {
        if (16.0f < vec2_distance(V(pv + 0x1d0), V(pv + 0x180))) sw(pv + 0x1c4, 2);
        else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x188)))) sw(pv + 0x1c4, 2);
    }
    if (lwu(pv + 0x1c0) == 0) {  // (0x002e63e4) the hero then
        sw(pv + 0x1c0, lwu(0x001413d0u));
        lq_sq(pv + 0x180, 0x0013f3d0u);
    }
}

// A step toward the point `p`: turned toward it (plus its yaw offset +0x1f8), its speed (+0x204) sought toward the distance
// left, along its facing (+0xc0) with its fall (+0x208, less 10 a frame squared), walker_move'd and pushed back behind its
// path's line (0.333); the fall is what it came down by. The distance it had to the point.
// orig: L16:0x002e6428
float kal_walker_walk_to_l16(uint32_t m, uint32_t p) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x90);
    const uint32_t sp = f.sp;
    const float yaw = angle_add(atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14))), lwc1(pv + 0x1f8));
    swc1(sp + 0x10, 0.0f);
    const float z0 = lwc1(m + 0x18);
    const float d = vec3_distance(V(m + 0x10), V(p));
    yaw_seek(m + 0x48, pv + 0x1f4, yaw, fpu_mul(dt2(), turn()), fpu_mul(dt2(), turn()), fpu_mul(dt(), turn()));
    seek_1d(sp + 0x10, pv + 0x204, d, fpu_mul(dt2(), 8.0f), fpu_mul(dt2(), 12.0f), fpu_mul(dt(), 4.0f));
    vec3_set_length(V(sp), V(m + 0xc0), lwc1(pv + 0x204));
    swc1(sp + 8, fpu_sub(lwc1(pv + 0x208), fpu_mul(dt2(), 10.0f)));
    walker_move(m, sp, 0x10, 0.5f, 0.5f, 0.0f);
    path_push_out_2d(lwu(pv + 0x1f0), m + 0x10, m + 0x10, u2f(0x3eaa7efau));
    swc1(pv + 0x208, fpu_sub(z0, lwc1(m + 0x18)));
    return d;
}

// orig: L16:0x002e5e08
void kal_class1445_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    kal_walker_frame_l16(m);
    joint_node_scale(m, 1, pv + 0x220, u2f(0x40066666u));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L16_ADDR(0x001671c0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    switch (lbu(m + 0x20)) {  // (the table 0x1e8840)
    case 0:  // (0x002e5eb0) set up at home, or gone with no path
        if (lw(pv + 0x1f0) == -1) {
            moby_retire(m);
            return;
        }
        sh(pv + 0x24, 1);
        sb(pv + 0x28, 0);
        swc1(pv + 0x20, 1.0f);
        sw(pv + 0xd0, L16_ADDR(0x00161e48));
        if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        lq_sq(pv + 0x1d0, m + 0x10);
        sb(m + 0x20, 1);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(20));
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(8.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x404ccccdu))));
        break;
    case 1:  // (0x002e5f6c) waits at home: a target, walk to it
        if (lw(pv + 0x1c4) == 2) break;
        sb(m + 0x20, 4);
        anim_to(m, 0);
        break;
    case 4:  // (0x002e5fa0) walks to its target: within 1.25 swings; lost, walks home
        if (kal_walker_walk_to_l16(m, pv + 0x180) < 1.25f) {
            sb(m + 0x20, 5);
            anim_to(m, 1);
        } else if (lw(pv + 0x1c4) == 2) {
            sb(m + 0x20, 6);
        }
        break;
    case 5:  // (0x002e600c) swings, turned toward its target; walks on when it moved past 1.5
        yaw_seek(m + 0x48, pv + 0x1f4,
                 atan2_f(fpu_sub(lwc1(pv + 0x180), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x184), lwc1(m + 0x14))),
                 fpu_mul(dt2(), turn()), fpu_mul(dt2(), turn()), fpu_mul(dt(), turn()));
        if (anim_crossed(m, 13.0f) != 0) {
            moby_chain_contact(m, 0, 1, 0, 1, 0, u2f(0x3eaa7efau), 1.0f);
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (!(1.5f < vec3_distance(V(m + 0x10), V(pv + 0x180)))) break;
        sb(m + 0x20, 4);
        anim_to(m, 0);
        break;
    case 6:  // (0x002e610c) walks home; there, waits (1); a new target on the way: walks to it
        if (kal_walker_walk_to_l16(m, pv + 0x1d0) < 1.0f) {
            sb(m + 0x20, 1);
            anim_to(m, 2);
        } else if (lw(pv + 0x1c4) != 2) {
            sb(m + 0x20, 4);
        }
        break;
    case 7:  // (0x002e6180) carried, until it settles
        if (carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, 1);
            sh(pv + 0xc8, 0);
        }
        break;
    case 8:  // (0x002e61a0) knocked away: a burst where it lands
        if ((enemy_physics(m, pv + 0x120) & 0x140u) != 0) {
            explosion_burst(m, m + 0x10, 6, 0.5f, 13.0f);
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
        break;
    default:  // (2, 3 and 9 and past: nothing, 0x002e61ec)
        break;
    }
}

}  // namespace rac1
