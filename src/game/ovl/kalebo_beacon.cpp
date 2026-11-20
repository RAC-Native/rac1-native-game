// Kalebo III's (level 16) class 470: a revolving beacon. It settles on the floor below it, then turns at 240 degrees a
// second and every frame sets four glows (particle type 0x3b) in a short line from it toward the camera, each with its own
// frame of the glow's animation cycling through 255 and its colour pulsing between two blues. Level 16 only: `orig: L16:`,
// level data L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class470, beacon_*).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
using kalebo::tick32;

// The beacon's set-up (state 0): the four glows' frames at random in 0 .. 254 (pvars +0x0 .. +0xc), their steps a frame
// (+0x10 .. +0x1c), their colour periods' timers (+0x20 .. +0x2c: 63, 127, 191 and 255 frames) and sizes (+0x30 .. +0x3c).
// orig: L16:0x002c9568
void beacon_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 4; ++k) swc1(pv + k * 4, static_cast<float>(rand_mod(0xff)));
    swc1(pv + 0x10, -1.0f);
    swc1(pv + 0x14, -2.25f);
    swc1(pv + 0x18, 1.25f);
    swc1(pv + 0x1c, 2.5f);
    for (uint32_t k = 0; k < 4; ++k) sw(pv + 0x20 + k * 4, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(0x3f + 0x40 * k))));
    swc1(pv + 0x3c, 3.5f);
    swc1(pv + 0x38, u2f(0x40866666u));  // 4.2
    swc1(pv + 0x30, 3.5f);
    swc1(pv + 0x34, u2f(0x40866666u));
}

// The beacon's frame: the four glows set 0.1 apart on the line from 0.3 before the beacon (0.1 above it) toward the camera.
// Each glow's frame steps (wrapped into 0 .. 255); its colour is the blend of two blues by how far its timer is from half of
// 255 frames, the timer restarting when it runs out.
// orig: L16:0x002c9650
void beacon_glows(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float lift = u2f(0x3dcccccdu);  // 0.1
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lift));
    Vec4 at;
    vec3_sub(at, V(L16_ADDR(0x001671c0)), V(m + 0x10));
    vec3_set_length(at, at, u2f(0xbe99999au));  // -0.3
    Vec4 step;
    vec3_set_length(step, at, lift);
    vec3_add(at, at, V(m + 0x10));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lift));

    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t frame = pv + k * 4, timer = pv + 0x20 + k * 4;
        const float f = fpu_add(lwc1(frame), lwc1(pv + 0x10 + k * 4));
        swc1(frame, f);
        if (255.0f <= f) swc1(frame, fpu_sub(f, 255.0f));
        else if (f <= 0.0f) swc1(frame, fpu_add(f, 255.0f));

        if (tick32(timer) != 0) sw(timer, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float left = int_to_float(frame_scale_i(0xff) - lw(timer));
        const float part = fpu_div(left, static_cast<float>(frame_scale_i(0xff)));
        const uint32_t color = rgba_lerp(0x4040ffffu, 0x1040ffffu, abs_f(fpu_sub(0.5f, part)));
        const uint32_t cell = static_cast<uint32_t>(float_to_int(lwc1(frame))) & 0xffu;
        spawn_particle_3b_v(at, color, cell, 0x35, 1, 2, 0, lwc1(pv + 0x30 + k * 4));
        vec3_add(at, at, step);
    }
}

}  // namespace

// Class 470: state 0 sets the beacon up and puts it 0.9 above the floor found within 1 of it (its scale +0x2c times 0.9);
// state 1 turns it (4.18879 radians a second) and sets its glows.
// orig: L16:0x002c9480
void kal_class470_update_l16(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0: {
        const float above = u2f(0x3f666666u);  // 0.9
        beacon_init(m);
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), above));
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        if (abs_f(fpu_sub(floor, lwc1(m + 0x18))) < 1.0f) swc1(m + 0x18, fpu_add(floor, above));
        sb(m + 0x20, 1);
        break;
    }
    case 1:
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed6c), u2f(0x40860a92u))));
        beacon_glows(m);
        break;
    default:
        break;
    }
}

}  // namespace rac1
