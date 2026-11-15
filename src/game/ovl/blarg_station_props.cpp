// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1054, a double door (its other half class 0x41f), class
// 1062, which picks the station's meshes to draw by the camera's place, and class 1066, a vent that lets out its group's
// creatures. Header blarg_own.h; registered in blarg_mobys.cpp (class 1062's draw callbacks 0x2fd460 / 0x2fd1a0 /
// 0x2fd748 are blarg_part2's). Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position
const float kDegree = u2f(0x3c8efa35u);    // pi / 180

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
}  // namespace

// Class 1054, a double door: itself and its other half (class 0x41f, spawned at its place: pvar +0x40) slide apart along
// its axis (+0x30, kept with its place +0x10 and rotation +0x20 at the start) to the distances 0x161f1c and 0x161f20. A
// door with a box (+0) waits shut for Ratchet in it (1): it opens (3), or its group (+0x21) is set to state 3, or with
// +4 = 1 it starts to judder (4: its halves moving and tilting about y (+0x44) toward the level's three poses 0x161f24 ..
// 0x161f30 / random ones / shut, by the rates 0x161f34 .. 0x161f48, a new pose every 30 frames or the times 0x161f4c ..
// 0x161f58) until it stands open; +4 = 2 holds it shut (2); without a box it starts open (5). Open, the level's word
// 0x161f5c set shuts it again (1).
// orig: L6:0x002fbfb0
void blg_class1054_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t off = sp, gap_a = sp + 0x10, gap_b = sp + 0x14;
    const uint32_t home = pv + 0x10, axis = pv + 0x30;
    // both halves at the distances `a` / `b` from the door's place along its axis
    auto place = [&](float a, float b) {
        vec3_set_length(V(off), V(axis), a);
        vec3_add(V(m + 0x10), V(home), V(off));
        vec3_set_length(V(off), V(axis), -b);
        vec3_add(V(lwu(pv + 0x40) + 0x10), V(home), V(off));
        moby_update_transform(lwu(pv + 0x40));
    };
    auto near = [](float v, float goal) { return abs_f(fpu_sub(v, goal)) < u2f(0x3c23d70au); };  // (within 0.01)
    switch (lbu(m + 0x20)) {  // (the table 0x201790: one case a state, 0 .. 5; 2 holds)
    case 0: {
        lq_sq(home, m + 0x10);
        lq_sq(pv + 0x20, m + 0x40);
        lq_sq(axis, m + 0xe0);
        const uint32_t other = moby_spawn(0x41f);
        sw(pv + 0x40, other);
        sh(other + 0x32, 0x40);
        sb(lwu(pv + 0x40) + 0x31, 1);
        sd(lwu(pv + 0x40) + 0x38, ld(m + 0x38));
        sh(lwu(pv + 0x40) + 0x34, lhu(m + 0x34));
        lq_sq(lwu(pv + 0x40) + 0x10, m + 0x10);
        lq_sq(lwu(pv + 0x40) + 0x40, m + 0x40);
        if (lw(pv) >= 0) {
            sb(m + 0x20, 1);
        } else if (lw(pv + 4) == 2) {
            sb(m + 0x20, 2);
        } else {
            place(lwc1(L6_ADDR(0x00161f1c)), lwc1(L6_ADDR(0x00161f20)));
            sb(m + 0x20, 5);
        }
        break;
    }
    case 1:
        if (point_in_box(HERO_POS, lw(pv)) == 0) break;
        if (lw(pv + 4) == 1) {
            sb(m + 0x20, 4);
            sw(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(0x14)));
        } else if (lbu(m + 0x21) != 0xff) {
            moby_group_set_state(lbu(m + 0x21), 3);
        } else {
            sb(m + 0x20, 3);
        }
        moby_sound_start(0, 0, lwu(pv + 0x40));
        break;
    case 3: {
        swc1(gap_a, vec3_distance(V(m + 0x10), V(home)));
        swc1(gap_b, vec3_distance(V(lwu(pv + 0x40) + 0x10), V(home)));
        const float turn = fpu_mul(dt_sq(), 10.0f), accel = fpu_mul(dt_sq(), 20.0f), top = fpu_mul(dt(), 5.0f);
        seek_1d(gap_a, pv + 0x8, lwc1(L6_ADDR(0x00161f1c)), turn, accel, top);
        seek_1d(gap_b, pv + 0x44, lwc1(L6_ADDR(0x00161f20)), fpu_mul(dt_sq(), 10.0f), fpu_mul(dt_sq(), 20.0f), fpu_mul(dt(), 5.0f));
        place(lwc1(gap_a), lwc1(gap_b));
        if (near(lwc1(gap_a), lwc1(L6_ADDR(0x00161f1c))) && near(lwc1(gap_b), lwc1(L6_ADDR(0x00161f20)))) sb(m + 0x20, 5);
        break;
    }
    case 4: {
        const uint32_t gap_c = sp + 0x18, gap_d = sp + 0x1c;
        const uint32_t other = lwu(pv + 0x40);
        swc1(gap_c, vec3_distance(V(m + 0x10), V(home)));
        swc1(gap_d, vec3_distance(V(other + 0x10), V(home)));
        // the pose it moves toward (the halves' distances, their tilts) and the rates, by the step +0xbc
        float to_a, to_b, tilt_a, tilt_b, turn, accel, top, tilt_turn, tilt_accel, tilt_top;
        int32_t next_frames = 0x1e;
        uint32_t next_step = 1;
        const uint32_t step = lbu(m + 0xbc);
        if (step == 1 || step == 2) {
            float lo, hi;
            if (step == 1) {  // a random pose about where it is
                lo = static_cast<float>(lw(L6_ADDR(0x00161f54)));
                hi = static_cast<float>(lw(L6_ADDR(0x00161f58)));
                to_a = lwc1(gap_c);
                tilt_b = lwc1(other + 0x44);
                to_b = lwc1(gap_d);
                tilt_a = lwc1(m + 0x44);
                next_step = 2;
            } else {  // the level's pose
                lo = static_cast<float>(lw(L6_ADDR(0x00161f4c)));
                hi = static_cast<float>(lw(L6_ADDR(0x00161f50)));
                to_a = lwc1(L6_ADDR(0x00161f24));
                to_b = lwc1(L6_ADDR(0x00161f28));
                tilt_a = lwc1(L6_ADDR(0x00161f2c));
                next_step = 1;
                tilt_b = lwc1(L6_ADDR(0x00161f30));
            }
            turn = lwc1(L6_ADDR(0x00161f34));
            accel = lwc1(L6_ADDR(0x00161f38));
            top = lwc1(L6_ADDR(0x00161f3c));
            tilt_turn = lwc1(L6_ADDR(0x00161f40));
            tilt_accel = lwc1(L6_ADDR(0x00161f44));
            tilt_top = lwc1(L6_ADDR(0x00161f48));
            next_frames = float_to_int(rand_range_f(lo, hi));
        } else {  // its opening pose, tilts kept
            turn = 10.0f;
            tilt_b = lwc1(m + 0x44);
            top = 5.0f;
            tilt_accel = turn;
            to_a = lwc1(L6_ADDR(0x00161f24));
            tilt_a = tilt_b;
            to_b = lwc1(L6_ADDR(0x00161f28));
            tilt_turn = tilt_accel;
            accel = 20.0f;
            tilt_top = top;
        }
        seek_1d(gap_c, pv + 0x8, to_a, fpu_mul(turn, dt_sq()), fpu_mul(accel, dt_sq()), fpu_mul(top, dt()));
        seek_1d(gap_d, pv + 0x44, to_b, fpu_mul(turn, dt_sq()), fpu_mul(accel, dt_sq()), fpu_mul(top, dt()));
        const float t_top = fpu_mul(tilt_top, kDegree), t_turn = fpu_mul(tilt_turn, kDegree), t_accel = fpu_mul(tilt_accel, kDegree);
        seek_1d(m + 0x44, pv + 0xc, tilt_a, fpu_mul(t_turn, dt_sq()), fpu_mul(t_accel, dt_sq()), fpu_mul(t_top, dt()));
        seek_1d(lwu(pv + 0x40) + 0x44, pv + 0x48, tilt_b, fpu_mul(t_turn, dt_sq()), fpu_mul(t_accel, dt_sq()), fpu_mul(t_top, dt()));
        place(lwc1(gap_c), lwc1(gap_d));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x4c))) == 0) break;
        sb(m + 0xbc, next_step);
        sw(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(next_frames)));
        if (near(lwc1(gap_c), lwc1(L6_ADDR(0x00161f24))) && near(lwc1(gap_d), lwc1(L6_ADDR(0x00161f28)))) sb(m + 0x20, 5);
        break;
    }
    case 5:
        if (lwc1(L6_ADDR(0x00161f5c)) == 0.0f) break;
        sb(m + 0xbc, 0);
        sb(m + 0x20, 1);
        lq_sq(m + 0x10, home);
        lq_sq(m + 0x40, pv + 0x20);
        lq_sq(lwu(pv + 0x40) + 0x10, m + 0x10);
        lq_sq(lwu(pv + 0x40) + 0x40, m + 0x40);
        swc1(pv + 0x48, 0.0f);
        swc1(pv + 0x8, 0.0f);
        swc1(pv + 0xc, 0.0f);
        swc1(pv + 0x44, 0.0f);
        break;
    default:
        break;
    }
    ee_stack_free(0xf0);
}

// Class 1062: its meshes drawn (the callback 0x2fd460 each frame; 0x2fd1a0 while the camera is in one of the boxes pvars
// +0 .. +0xc; 0x2fd748 while it is in one of +0x40 .. +0x4c, or while the hero is in the state 0xf).
// orig: L6:0x002fd088
void blg_class1062_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam = L6_ADDR(0x00167540);  // (the camera's place)
    sub_001f93e0(L6_ADDR(0x002fd460), m);
    for (uint32_t i = 0; i < 4; ++i) {
        const int32_t box = lw(pv + i * 4);
        if (box < 0) continue;
        if (point_in_box(cam, box) != 0) {
            sub_001f93e0(L6_ADDR(0x002fd1a0), m);
            break;
        }
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (lw(0x0013f350 + 0x208c) == 0xf) {
            sub_001f93e0(L6_ADDR(0x002fd748), m);
            return;
        }
        const int32_t box = lw(pv + 0x40 + i * 4);
        if (box < 0) continue;
        if (point_in_box(cam, box) != 0) {
            sub_001f93e0(L6_ADDR(0x002fd748), m);
            return;
        }
    }
}

// Class 1066, a vent: Ratchet in its box (+4; none when negative) opens it (1, the animation 1); its animation ended, it
// lets out its group's creatures (+0; 2), else closes (3); in state 2 one creature waiting (state 0xf) is let out at its
// place every 20 frames (its timer +8), until none waits (3).
// orig: L6:0x002fda30
void blg_class1066_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        const int32_t box = lw(pv + 4);
        if (box < 0 || point_in_box(HERO_POS, box) == 0) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 1);
        if (set != 1) moby_anim_set(m, 1, 0, 5);
        break;
    }
    case 1: {
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, lw(pv) >= 0 ? 2 : 3);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 5);
        break;
    }
    case 2: {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) == 0) break;
        bool let_out = false;
        const uint32_t list = lvd::group(lw(pv));
        if (list != 0) {
            for (uint32_t e = list;; e += 2) {
                const uint32_t c = lwu(L6_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
                if (creature_let_out_l6(c, m + 0x10) != 0) {
                    let_out = true;
                    break;
                }
                if (lh(e) < 0) break;
            }
        }
        if (!let_out) sb(m + 0x20, 3);
        else sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x14)));
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
