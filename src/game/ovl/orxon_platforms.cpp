// Orxon's own moby classes (level 10): the moving platforms. Class 1033, a platform that rises 1.5 and sinks back, pausing
// two seconds at each end, and class 1117, a platform floating on the sludge that wobbles, sinks under the hero and floats
// back up. Header orxon_mobys.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// Class 1033: state 0 notes its height (pvar +0xa0) and starts rising; 1 rises by 1.5 over 45 frames up to 1.5 above it,
// 2 waits 120 frames and starts its sound, 3 sinks back, 4 waits and starts its sound again (back to 1).
// orig: L10:0x00295a38
void orx_class1033_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {  // (the original prints the class and index of a moby without pvars)
        debug_printf_stub();
        moby_retire(m);
        return;
    }
    auto step = [] { return fpu_div(1.5f, frame_scale_f(45.0f)); };
    auto rest = [&](uint32_t next) {
        sb(m + 0x20, next);
        sw(pv + 0xb0, static_cast<uint32_t>(frame_scale_i(0x78)));
    };
    switch (lbu(m + 0x20)) {  // (the table at L10:0x001dc560)
    case 0:  // (0x00295aa0)
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 5);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        lq_sq(pv + 0xa0, m + 0x10);
        sb(m + 0x20, 1);
        break;
    case 1: {  // rising (0x00295acc)
        const float z = fpu_add(lwc1(m + 0x18), step());
        swc1(m + 0x18, z);
        const float top = fpu_add(lwc1(pv + 0xa8), 1.5f);
        if (top <= z) {
            swc1(m + 0x18, top);
            rest(2);
        }
        break;
    }
    case 2:  // (0x00295b14)
        if (tick32(pv + 0xb0) != 0) {
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 3);
        }
        break;
    case 3: {  // sinking (0x00295b38)
        const float z = fpu_sub(lwc1(m + 0x18), step());
        swc1(m + 0x18, z);
        const float bottom = lwc1(pv + 0xa8);
        if (z <= bottom) {
            swc1(m + 0x18, bottom);
            rest(4);
        }
        break;
    }
    case 4:  // (0x00295b8c)
        if (tick32(pv + 0xb0) != 0) {
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 1);
        }
        break;
    default:
        break;
    }
}

// Class 1117: state 0 notes its height (pvar +0xa0) and picks a wobble (random phases +0xa8 / +0xb4 / +0xb8 and a rate
// +0xb0); 1 wobbles; 2 (the hero on it) sinks ever faster (the speed +0xa4 down by twice the frame's square, to twice the
// frame time at most), a sound every 40 to 60 frames, and floats back (3) once the hero is off; 3 rises back to its
// height, slowing down near it, then 1. Every state moves the riders by what the platform moved (attach_frame_set).
// orig: L10:0x00295c20
void orx_class1117_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0x20);
    const uint32_t start = frame, moved = frame + 0x10;  // (where the platform was, how far it moved)
    lq_sq(start, m + 0x10);
    if (moby_hero_on_l1(m) != 0) sb(m + 0x20, 2);
    tick32(pv + 0xbc);
    // (states 4 and up are no state of the game: the original then hands attach_frame_set a stale register for the
    // vector, a write into the null page; the port uses its own)
    switch (lbu(m + 0x20)) {
    case 0:  // (0x00295c9c)
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 5);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        swc1(pv + 0xa0, lwc1(m + 0x18));
        swc1(pv + 0xa8, rand_angle());
        swc1(pv + 0xb4, rand_angle());
        swc1(pv + 0xb8, rand_angle());
        {
            const float jitter = rand_range_signed_f(0.0f, fpu_mul(dt(), u2f(0x3e0efa35u)));  // (8 degrees a frame)
            swc1(pv + 0xb0, fpu_add(fpu_mul(dt(), u2f(0x3fc90fdbu)), jitter));
        }
        sb(m + 0x20, 1);
        [[fallthrough]];
    case 1:  // wobbling (0x00295d18)
        sw(pv + 0xa4, 0);
        sub_00262b00(m, pv + 0xa8, pv + 0xac, u2f(0x3dcccccdu), lwc1(pv + 0xb0));
        sub_00262b80(m, pv + 0xb4, pv + 0xb8, u2f(0x3d4104fbu), fpu_mul(dt(), u2f(0x3eb2b8c2u)), fpu_mul(dt(), u2f(0x3f060a92u)));
        break;
    case 2: {  // sinking under the hero (0x00295d88)
        const float accel = fpu_add(dt2(), dt2());
        const float fastest = -fpu_add(dt(), dt());
        const float speed = fpu_sub(lwc1(pv + 0xa4), accel);
        swc1(pv + 0xa4, speed < fastest ? fastest : speed);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0xa4)));
        if (moby_hero_on_l1(m) == 0) sb(m + 0x20, 3);
        if (lw(pv + 0xbc) <= 0) {
            moby_sound_start(0, 0, m);
            sw(pv + 0xbc, static_cast<uint32_t>(rand_range(0x28, 0x3c)));
        }
        break;
    }
    case 3: {  // floating back up (0x00295e18)
        const float speed = lwc1(pv + 0xa4);
        const float accel = dt2();
        bool speed_up = speed < 0.0f;
        float sq = 0.0f, left = 0.0f;
        if (!speed_up) {  // the distance it needs to stop against the distance left
            sq = fpu_mul(speed, speed);
            left = fpu_sub(lwc1(pv + 0xa0), lwc1(m + 0x18));
            speed_up = fpu_div(sq, fpu_add(fpu_add(accel, accel), fpu_add(accel, accel))) < left;
        }
        if (speed_up) {  // (0x00295e60)
            const float faster = fpu_add(speed, accel);
            const float fastest = fpu_add(dt(), dt());
            swc1(pv + 0xa4, fastest < faster ? fastest : faster);
        } else if (0.0f < left) {  // braking (0x00295e88)
            const float slower = fpu_sub(speed, fpu_div(sq, fpu_add(left, left)));
            swc1(pv + 0xa4, slower < 0.0f ? 0.0f : slower);
        }
        const float z = fpu_add(lwc1(m + 0x18), lwc1(pv + 0xa4));
        swc1(m + 0x18, z);
        const float top = lwc1(pv + 0xa0);
        if (top <= z) {
            swc1(m + 0x18, top);
            sb(m + 0x20, 1);
        }
        break;
    }
    default:
        break;
    }
    vec3_sub(V(moved), V(m + 0x10), V(start));
    attach_frame_set(pv + 0x60, moved, m + 0x40, m + 0x40);
    ee_stack_free(0x20);
}

}  // namespace rac1
