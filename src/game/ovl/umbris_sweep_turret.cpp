// Umbris's own moby classes: class 1126, the sweeping pop-up turret that fires class 880 bolts (0x30b538) across an arc
// between two marker mobys, and retracts into the floor for 3 seconds when struck — hand-ported from Umbris's own overlay.
// Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR,
// the base ELF's are bare (the hero 0x13f3d0, the frame time 0x15ed6c).
//
// Class 1126's pvars: its hit state +0x20, its colour pulse +0x60, its arc start / end marker mobys +0x70 / +0x74 (-1: none),
// its sweep speed in degrees per second +0x78, its fire / phase countdown +0x7c (halfword), its sweep direction +0x7e
// (halfword, 0 forward, 1 back), its start / end angles +0x80 / +0x84, its sweep offset +0x88 and span +0x8c, its home z
// +0x90, its burst count +0x94 (halfword), its pause count +0x98 (word; 0: every other tick), its burst / pause counter
// +0x9c (halfword) and its shot collision flag +0x9e (byte).
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

// Spawns a class 880 bolt (0x370) at `pos` with velocity `vel`, ignoring `ignore` in collisions, with flag word `flags`.
// orig: L7:0x0030b538
uint32_t umb_30b538_l7(uint32_t pos, uint32_t vel, uint32_t ignore, uint32_t flags) {
    const uint32_t s = moby_spawn(0x370);
    if (s == 0) return 0;
    sb(s + 0x31, 1);
    sw(s + 0x40, 0);
    sh(s + 0x32, 0x7f);
    sb(s + 0x30, 0xff);
    const uint32_t spv = lwu(s + 0x78);
    swc1(s + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 0x8)));
    swc1(s + 0x48, atan2_f(lwc1(vel + 0x0), lwc1(vel + 0x4)));
    V(s + 0x10) = V(pos);
    V(spv + 0x40) = V(pos);
    V(spv + 0x30) = V(vel);

    const uint32_t sp = ee_stack_alloc(0x10);
    V(sp) = V(spv + 0x30);
    sw(sp + 0x8, 0);
    vec2_set_length(V(sp), V(sp), 1.0f);
    swc1(sp + 0x8, 1.0f);
    sub_00259888(1.0f, spv, static_cast<int32_t>(s), 0x10001, sp);
    ee_stack_free(0x10);

    sb(spv + 0x19, 1);
    sb(spv + 0x18, 1);
    moby_anim_set(s, 1, 0, frame_scale_i(5));
    sh(s + 0x34, 0x208);
    sw(spv + 0x54, flags);
    sw(spv + 0x50, ignore);
    sb(s + 0x23, 0x10);
    moby_update_transform(s);
    return s;
}

// Class 1126, the sweeping pop-up turret (+0x20, jump table L7:0x002119f0: 0 set up arc, 1 sweeping & firing, 2 hit spin,
// 3 retracted below +0x90 - 0.7, 4 rising back to +0x90).
// orig: L7:0x0031a250
void umb_class1126_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0);
    int32_t hit_st = 0;
    moby_hit_react_r(m, hit, pv + 0x20, 0, hit_st, nullptr, nullptr, 4);
    if (lbu(m + 0x20) < 2 && hit_st >= 2) {
        moby_sound_start(1, 0, m);
        moby_drop_bolts(m, 0, -1);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        sb(pv + 0x67, 0xfa);
        sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
        color_pulse_start(m, pv + 0x60);
        sh(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(0x3c)));
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 5);
        sb(m + 0x20, 2);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);

    const float two_pi = u2f(0x40c90fdbu);
    const float four_pi = u2f(0x41490fdbu);

    switch (lbu(m + 0x20)) {  // (jump table L7:0x002119f0)
        case 0: {  // (0x0031a388)
            const int32_t m0 = lw(pv + 0x70);
            if (m0 != -1) {
                const uint32_t t0 = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(m0) << 8);
                float a = atan2_f(fpu_sub(lwc1(t0 + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t0 + 0x14), lwc1(m + 0x14)));
                if (a < 0.0f) a = fpu_add(a, two_pi);
                swc1(pv + 0x80, a);
            }
            const int32_t m1 = lw(pv + 0x74);
            if (m1 != -1) {
                const uint32_t t1 = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(m1) << 8);
                float a = atan2_f(fpu_sub(lwc1(t1 + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t1 + 0x14), lwc1(m + 0x14)));
                if (a < 0.0f) a = fpu_add(a, two_pi);
                swc1(pv + 0x84, a);
                if (lwc1(pv + 0x84) < lwc1(pv + 0x80)) {
                    swc1(pv + 0x84, fpu_add(lwc1(pv + 0x84), two_pi));
                }
            } else {
                swc1(pv + 0x84, fpu_add(lwc1(pv + 0x80), two_pi));
            }
            const float a0 = lwc1(pv + 0x80);
            const float a1 = lwc1(pv + 0x84);
            sw(pv + 0x88, 0);
            swc1(pv + 0x8c, fpu_sub(a1, a0));
            swc1(m + 0x48, angle_wrap_mod(a0));
            swc1(pv + 0x90, lwc1(m + 0x18));
            sb(pv + 0x29, 0);
            sb(m + 0xbc, 0);
            sh(pv + 0x9c, lhu(pv + 0x94));
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
            sb(m + 0x20, 1);
            return;
        }
        case 1: {  // (0x0031a4f8)
            if (lbu(m + 0x52) != 1 && (lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, 1);
            }
            const float dt = lwc1(0x0015ed6c);
            const float step = fpu_mul(fpu_mul(lwc1(pv + 0x78), u2f(0x3c8efa35u)), dt);  // (deg/s * deg * dt)
            if (lh(pv + 0x7e) != 0) {
                const float next = fpu_sub(lwc1(pv + 0x88), step);
                if (next < 0.0f) {
                    swc1(pv + 0x88, 0.0f);
                    sh(pv + 0x7e, 0);
                } else {
                    swc1(pv + 0x88, next);
                }
            } else {
                const float next = fpu_add(lwc1(pv + 0x88), step);
                const float hi = lwc1(pv + 0x8c);
                if (hi < next) {
                    swc1(pv + 0x88, hi);
                    sh(pv + 0x7e, 1);
                } else {
                    swc1(pv + 0x88, next);
                }
            }
            swc1(m + 0x48, angle_wrap_mod(fpu_add(lwc1(pv + 0x80), lwc1(pv + 0x88))));
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x7c))) == 0) return;

            if (lbu(m + 0xbc) == 0) {
                if (lw(pv + 0x98) == 0 || (lhu(pv + 0x9c) & 1u) != 0) {
                    const uint32_t sp = ee_stack_alloc(0x20);
                    moby_chain_point_v(m, 0, V(sp));
                    const float c = cos_vu(lwc1(m + 0x48));
                    const float vx = fpu_mul(c, fpu_mul(lwc1(0x0015ed6c), 15.0f));
                    swc1(sp + 0x10, vx);
                    const float s = sin_vu(lwc1(m + 0x48));
                    const float vy = fpu_mul(s, fpu_mul(lwc1(0x0015ed6c), 15.0f));
                    sw(sp + 0x18, 0);
                    swc1(sp + 0x14, vy);
                    moby_sound_start(0, 0, m);
                    umb_30b538_l7(sp, sp + 0x10, m, lbu(pv + 0x9e));
                    ee_stack_free(0x20);
                }
                if (lw(pv + 0x98) == 0) {
                    sh(pv + 0x9c, lhu(pv + 0x9c) ^ 1u);
                } else if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x9c))) != 0) {
                    sb(m + 0xbc, 1);
                    sh(pv + 0x9c, lhu(pv + 0x98));
                }
            } else if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x9c))) != 0) {
                sb(m + 0xbc, 0);
                sh(pv + 0x9c, lhu(pv + 0x94));
            }

            sh(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(6)));
            const float dist = vec2_distance(V(m + 0x10), V(0x0013f3d0));
            if (!(55.0f < dist)) {
                if (!(35.0f < dist) || lbu(m + 0x31) != 0) return;
            }
            if (80.0f < dist) {
                sh(pv + 0x7c, static_cast<uint32_t>(lh(pv + 0x7c) * 3));
            } else if (120.0f < dist) {
                sh(pv + 0x7c, static_cast<uint32_t>(lh(pv + 0x7c) << 2));
            } else {
                sh(pv + 0x7c, static_cast<uint32_t>(lh(pv + 0x7c) << 1));
            }
            return;
        }
        case 2: {  // (0x0031a798)
            if (lbu(m + 0x52) != 4 && (lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 4) {
                moby_anim_set(m, 4, 0, 1);
            }
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed6c), four_pi)));
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x7c))) != 0) {
                sh(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(0xb4)));
                sb(m + 0x20, 3);
            }
            return;
        }
        case 3: {  // (0x0031a820)
            if (lbu(m + 0x52) != 4 && (lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 4) {
                moby_anim_set(m, 4, 0, 0);
            }
            const float floor_z = fpu_sub(lwc1(pv + 0x90), u2f(0x3f333333u));  // (0.7)
            const float z = lwc1(m + 0x18);
            if (floor_z < z) {
                swc1(m + 0x18, fpu_sub(z, fpu_mul(lwc1(0x0015ed6c), 5.0f)));
            } else {
                swc1(m + 0x18, floor_z);
            }
            const float goal = angle_wrap_mod(fpu_add(lwc1(pv + 0x80), lwc1(pv + 0x88)));
            const float diff = angle_dist(lwc1(m + 0x48), goal);
            const float turn = fpu_mul(lwc1(0x0015ed6c), four_pi);
            if (turn < diff) {
                swc1(m + 0x48, angle_add(lwc1(m + 0x48), turn));
            } else {
                swc1(m + 0x48, angle_wrap_mod(fpu_add(lwc1(pv + 0x80), lwc1(pv + 0x88))));
            }
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x7c))) != 0) {
                moby_sound_start(2, 0, m);
                if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, 5);
                sb(m + 0x20, 4);
            }
            return;
        }
        case 4: {  // (0x0031a954)
            const float home_z = lwc1(pv + 0x90);
            const float z = lwc1(m + 0x18);
            if (z < home_z) {
                swc1(m + 0x18, fpu_add(z, fpu_mul(lwc1(0x0015ed6c), 5.0f)));
                return;
            }
            const uint32_t anim = lbu(m + 0x53);
            const uint32_t flags = lhu(m + 0x34) | 0x1000u;
            swc1(m + 0x18, home_z);
            sh(m + 0x34, flags);
            if (anim != 1) moby_anim_set(m, 1, 0, 5);
            sb(m + 0x20, 1);
            return;
        }
        default:  // (5 and >= 6: 0x0031a9c4)
            return;
    }
}

}  // namespace rac1
