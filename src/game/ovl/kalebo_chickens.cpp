// Kalebo III's (level 16) class 1923, a chicken dispenser: while Ratchet is in its box (pvar +0x8) and more than 3 (flat)
// from its door (the moby of index pvar +0x4), every 3 to 6 seconds (pvar +0xc) it opens the door when a chicken (class 270)
// of its group (pvar +0x0) waits in the state 6; once the door is open far enough (its pvar +0x34 above 0.2 in its state 8)
// the chicken is put on the floor under the door and let go (state 4), or, when something is already there (a sphere of 0.2),
// bursts into four feathers (chicken_debris_spawn) with sound 1. Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF
// bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class1923 and the P16.4 checks).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapon_classes.h>

namespace rac1 {

namespace {
constexpr int16_t kChickenClass = 0x10e;  // class 270
constexpr uint32_t kHeroPos = 0x0013f3d0;

// The next wait: 180 .. 360 frames (scaled), in pvar +0xc.
void next_wait(uint32_t pv) { sw(pv + 0xc, static_cast<uint32_t>(frame_scale_i(rand_range(0xb4, 0x168)))); }
}  // namespace

// The first chicken (class 270) of group `group`'s list in the state 6 (waiting), or 0.
// orig: L16:0x002e8b08
uint32_t kal_chicken_waiting_l16(int32_t group) {
    uint32_t e = lvd::group(group);  // (the level's table 0x1abcc0)
    if (e == 0) return 0;
    for (;; e += 2) {
        const uint32_t m = kalebo::moby_at(lhu(e));
        if (lh(m + 0xa6) == kChickenClass && moby_in_state(m, 6)) return m;
        if (lh(e) < 0) return 0;
    }
}

// The door opened: in its state 1, sound 0 and the state 7 (opening).
// orig: L16:0x002e0de0
void kal_chicken_door_open_l16(uint32_t door) {
    if (lbu(door + 0x20) != 1) return;
    moby_sound_start(0, 0, door);
    sb(door + 0x20, 7);
}

// The door's stage: 0 not opening (states other than 7 / 8), 2 open far enough (state 8, its pvar +0x34 above 0.2),
// 1 otherwise.
// orig: L16:0x002e0e28
int32_t kal_chicken_door_stage_l16(uint32_t door) {
    const uint32_t st = lbu(door + 0x20);
    if (st - 7 >= 2) return 0;
    if (st == 8 && u2f(0x3e4ccccdu) < lwc1(lwu(door + 0x78) + 0x34)) return 2;  // (0.2)
    return 1;
}

// A chicken let go at `point`: moved there; when a sphere of 0.2 there meets something, four feathers fly from it (each:
// a random point about it, raised by L16 0x1618f4, a random speed and a heading, a rise of 3.5 .. 5.5 a second) and sound 1;
// else it walks (animation 2, +0x34 | 0x1000 and & ~0x41, state 4, its colour from its class) with a timer of 180 frames.
// orig: L16:0x002c44f0
void kal_chicken_release_l16(uint32_t m, const Vec4& point) {
    V(m + 0x10) = point;
    kalebo::Frame f(0xc0);  // (the original's frame: the point +0x10, a feather's velocity +0x20, its place +0x30)
    const uint32_t sp = f.sp;
    V(sp + 0x10) = point;
    if (sphere_collide(sp + 0x10, u2f(0x3e4ccccdu), 0, m) != 0) {
        const uint32_t place = sp + 0x30, vel = sp + 0x20;
        for (int i = 0; i < 4; ++i) {
            const float r = rand_range_f(lwc1(L16_ADDR(0x001618f8)), lwc1(L16_ADDR(0x001618fc)));
            const float a = rand_angle();
            const float b = rand_angle();
            vec_from_spherical_v(V(place), r, a, b);
            swc1(place + 8, fpu_add(lwc1(place + 8), lwc1(L16_ADDR(0x001618f4))));
            vec3_add(V(place), V(place), V(m + 0x10));
            const float dt = lwc1(0x0015ed6c);
            const float speed = rand_range_f(fpu_mul(lwc1(L16_ADDR(0x001618ec)), dt), fpu_mul(lwc1(L16_ADDR(0x001618f0)), dt));
            const float heading = rand_angle();
            vec_from_spherical_v(V(vel), speed, heading, fpu_mul(lwc1(L16_ADDR(0x001618e8)), u2f(0x3c8efa35u)));  // (degrees)
            swc1(vel + 8, fpu_mul(rand_range_f(3.5f, 5.5f), lwc1(0x0015ed6c)));
            chicken_debris_spawn(m, place, vel);
        }
        moby_sound_start(1, 0, m);
        return;
    }
    const uint32_t pv = lwu(m + 0x78);
    swc1(pv + 0x78, 1.0f);
    if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 1);
    sh(m + 0x34, (lhu(m + 0x34) | 0x1000) & 0xffbe);
    sb(m + 0x20, 4);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sh(pv + 0x66, static_cast<uint32_t>(frame_scale_i(0xb4)));
}

// Class 1923: nothing while Ratchet is outside its box. State 0: the first wait, 1. State 1: Ratchet more than 3 from the
// door, a chicken waiting and the wait over: the door opened, state 2, the next wait. State 2: back to 1 when the door is
// not opening; when it is open far enough, back to 1 and the waiting chicken let go on the floor under the door (probed
// from 1 above it, 0.5 up).
// orig: L16:0x002e8970
void kal_class1923_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (point_in_box(kHeroPos, lw(pv + 8)) == 0) return;
    const uint32_t door = lwu(L16_ADDR(0x0015ffd8)) + (lwu(pv + 4) << 8);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        next_wait(pv);
        return;
    case 1:
        if (!(3.0f < vec2_distance(V(kHeroPos), V(door + 0x10)))) return;
        if (kal_chicken_waiting_l16(lw(pv)) == 0) return;
        if (kalebo::tick32(pv + 0xc) == 0) return;
        sb(m + 0x20, 2);
        kal_chicken_door_open_l16(door);
        next_wait(pv);
        return;
    case 2: {
        const int32_t stage = kal_chicken_door_stage_l16(door);
        if (stage == 0) {
            sb(m + 0x20, 1);
            return;
        }
        if (stage != 2) return;
        sb(m + 0x20, 1);
        const uint32_t chicken = kal_chicken_waiting_l16(lw(pv));
        if (chicken == 0) return;
        Vec4 p = V(door + 0x10);
        p.z = fpu_add(p.z, 1.0f);
        p.z = floor_probe_v(0.5f, p, 0);
        kal_chicken_release_l16(chicken, p);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
