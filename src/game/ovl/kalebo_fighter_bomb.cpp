// Kalebo III's (level 16) class 281: the bomb class 541's fighters throw (spawned by L16:0x2c6268 with its velocity +0x10,
// its fall a frame +0x24 and the thrower +0x20). It flies, bounces off what it meets (at a quarter of its speed), comes to
// rest on a floor, blinks faster and faster (its colour pulse, pvars +0x0 ..) and blows up 1.5 seconds after it came to
// rest, or at once when its fuse +0x28 runs out in flight. Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare.
// Registered in kalebo_mobys.cpp; checked by batch 250 (class281, fighter_bomb_*).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

// The bomb's velocity bounced off the surface of the last collision test (its normal set to `len`, into `n`), when it
// goes into it: reflected and slowed to a quarter.
void bomb_bounce(uint32_t vel, Vec4& n, float len) {
    vec3_set_length(n, coll_hit_normal(), len);
    if (vec3_dot(n, V(vel)) < 0.0f) {
        vec3_reflect(V(vel), V(vel), n);
        vec3_scale(V(vel), V(vel), 0.25f);
    }
}

// The blink: every `every` frames of the fuse's last `last` frames, the colour pulse restarted at 0xfa.
void bomb_blink(uint32_t m, uint32_t pv, int32_t last, int32_t every) {
    if (!(lw(pv + 0x28) < frame_scale_i(last))) return;
    const int32_t step = frame_scale_i(every);
    if (ee_rem(lw(pv + 0x28), step) != (0)) return;
    sb(pv + 7, 0xfa);
    color_pulse_start(m, pv);
}

}  // namespace

// Class 281. States: 1 in flight (moved by its velocity, tested along the move and as a sphere of 0.333; when it lands on a
// floor (the normal's z at least 0.3) slower than dt / 4 it rests, its fuse set to 1.5 seconds; it blinks in the fuse's
// last 0.75 seconds, every 10 frames; gravity), 2 resting (it blinks in the last second, every 20 frames), 3 blows up.
// orig: L16:0x002c5eb0
void kal_class281_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t vel = pv + 0x10;
    switch (lbu(m + 0x20)) {
    case 1: {
        const Vec4 from = V(m + 0x10);
        vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
        Vec4 n;
        if (segment_collide_v(from, V(m + 0x10), 5, lwu(pv + 0x20), nullptr) != 0) bomb_bounce(vel, n, 1.0f);
        const float radius = u2f(0x3eaa7efau);  // 0.333
        if (sphere_collide(m + 0x10, radius, 2, 0) != 0) {
            bomb_bounce(vel, n, radius);
            V(m + 0x10) = coll_hit_point();
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), n.z));
            if (u2f(0x3e99999au) <= n.z && vec3_length(V(vel)) < fpu_mul(lwc1(0x0015ed6c), 0.25f)) {
                sb(m + 0x20, 2);
                sw(pv + 0x28, static_cast<uint32_t>(frame_scale_i(0x5a)));
            }
        }
        if (kalebo::tick32(pv + 0x28) != 0) sb(m + 0x20, 3);
        else bomb_blink(m, pv, 0x2d, 0xa);
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), lwc1(pv + 0x24)));
        color_pulse_update(m, pv);
        break;
    }
    case 2:
        if (kalebo::tick32(pv + 0x28) != 0) sb(m + 0x20, 3);
        else bomb_blink(m, pv, 0x3d, 0x14);
        color_pulse_update(m, pv);
        break;
    case 3:
        moby_sound_start(0, 0, m);
        explosion_effect(m, L16_ADDR(0x0015f580), 0, 7, 0xa, 0x14, -1, 0, 7, -1, 0, 1.5f, 1.0f, 3.0f, u2f(0x3fd9999au), 4.0f, 1.0f,
                         7.0f);
        moby_retire(m);
        break;
    default:
        color_pulse_update(m, pv);
        break;
    }
}

}  // namespace rac1
