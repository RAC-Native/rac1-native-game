// Class 1356's update and helpers: one port of the copies Kalebo III and Veldin carry (the same code relinked; each level's
// jump table of the states is its own, switched on here by the state). Each level keeps its own tuning words (per level
// below: +0 the size's scale, +4 the shots' speed, +8 the cruise speed, +0xc its acceleration, +0x10 the bob's rate in
// degrees a second, +0x14 its height) and the drones' attach point; the rest is the engine's (OV: the paths 0x1b04b0, the
// mobys 0x15ffd8, the debris' zero vector 0x15f580, the scene state 0x15f5c4) or the base ELF's (bare: the hero 0x13f350,
// his moby 0x1413d0, the sound slots 0x13e550, the frame time 0x15ed6c). Header shared_class1356.h.
//
// The pvars: +0x20 its health, +0x60 the colour pulse, +0x70 the target point, +0xb0 the target, +0xb4 its state (2: none),
// +0xc0 .. +0xcc the drones (moby indices, -1: gone), +0xd0 the target area (a path, -1: none), +0xd4 the area that starts it,
// +0xd8 / +0xdc the paths in and out, +0xe0 its joint's spring, +0x160 / +0x164 / +0x184 timers, +0x168 / +0x16c the point and
// fraction along the path, +0x170 the speed, +0x174 the path's step, +0x178 / +0x180 / +0x188 turn rates, +0x17c the wings'
// tilt, +0x18c the bob's phase, +0x190 its sound, +0x194 the height it bobs about.
#include <game/ovl/shared_class1356.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;      // (0x3c8efa35)
constexpr float HALF_PI = 1.5707963705062866f;   // (0x3fc90fdb)
constexpr float PI = 3.1415927410125732f;        // (0x40490fdb)
constexpr float TWO_PI = 6.2831854820251465f;    // (0x40c90fdb)
constexpr float FOUR_PI = 12.566370964050293f;   // (0x41490fdb)
constexpr float NEAR_PI = 3.141590118408203f;    // (0x40490fd0: a turn the original writes a little short of pi)

uint32_t tuning() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x00162270) : L16_ADDR(0x00161d78); }
// the drones' attach point (on the carrier's axes)
uint32_t drone_point() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x001da310) : L16_ADDR(0x001d9680); }

float dt() { return lwc1(0x0015ed6c); }

uint32_t path_of(int32_t i) { return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(i) << 2)); }
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the wings' tilt (+0x17c) eased towards `to`
void tilt_to(uint32_t pv, float to) {
    const float rate = fpu_mul(lwc1(0x0015ed70), FOUR_PI);
    seek_1d(pv + 0x17c, pv + 0x180, to, rate, rate, fpu_mul(dt(), TWO_PI));
}

// its engine's sound (in the slot +0x190) stopped when the slot is still its own and playing
void sound_end(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x190);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot) * 0x70;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x190, 0xffffffffu);
}
}  // namespace

// orig: L16:0x002d3dc0 L18:0x002dd9e8
void class1356_attach(uint32_t drone, uint32_t carrier, uint32_t offset, uint32_t turn) {
    const uint32_t pv = lwu(drone + 0x78);
    sw(pv + 0x2d0, carrier);
    lq_sq(pv + 0x2c0, offset);
    swc1(pv + 0x2cc, lwc1(turn + 8));
}

// The drone (when attached: state 10) back to state 1 with the animation 1, shown, its post and hover point where it is,
// its timer `frames`, its seek speed 5 a second.
// orig: L16:0x002d3de0 L18:0x002dda08
void class1356_release(uint32_t drone, int32_t frames) {
    if (lbu(drone + 0x20) != 10) return;
    const uint32_t pv = lwu(drone + 0x78);
    sb(drone + 0x20, 1);
    if (lbu(drone + 0x53) != 1) moby_anim_set(drone, 1, 0, frame_scale_i(0x14));
    sb(drone + 0x31, 1);
    const uint32_t bits = lhu(drone + 0x34) & 0xfffeu;
    sh(drone + 0x34, bits);
    sh(drone + 0x34, bits | 0x1000u);
    sw(drone + 0x94, lwu(lwu(drone + 0x24) + 0x10));
    sh(pv + 0x2dc, static_cast<uint32_t>(frames));
    swc1(pv + 0x2bc, lwc1(drone + 0x18));
    lq_sq(pv + 0x2c0, drone + 0x10);
    swc1(pv + 0x2d4, fpu_mul(dt(), 5.0f));
}

// orig: L16:0x002e2fa0 L18:0x002f15e8
void class1356_path_headings(uint32_t path) {
    for (int32_t i = 0; i < lw(path) - 1; ++i) {
        const uint32_t p = path + 0x10 + (static_cast<uint32_t>(i) << 4);
        swc1(p + 0xc, atan2_f(fpu_sub(lwc1(p + 0x10), lwc1(p)), fpu_sub(lwc1(p + 0x14), lwc1(p + 4))));
    }
    const int32_t n = lw(path);
    if (n >= 2) swc1(path + (static_cast<uint32_t>(n - 1) << 4) + 0x1c, lwc1(path + (static_cast<uint32_t>(n - 2) << 4) + 0x1c));
}

// orig: L16:0x002e3050 L18:0x002f1698
void class1356_joint(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float step = lwc1(0x0015ed64);
    swc1(pv + 0x144, lwc1(pv + 0x17c));
    joint_node_spring(m, pv + 0xe0, 2, fpu_mul(step, u2f(0x3cf5c28fu)), fpu_mul(step, u2f(0x3e99999au)));  // (0.03, 0.3)
}

// One step along the path: the point and fraction (+0x168 / +0x16c) advanced by the speed over the path's step; at the last
// point 1, else the place and heading there (path_point), the bob's height from it, the speed eased towards the distance
// left (accelerating at +0xc, at most the cruise speed +8 of the level's tuning).
// orig: L16:0x002e2e88 L18:0x002f14d0
int32_t class1356_follow(uint32_t m, uint32_t path) {
    const uint32_t pv = lwu(m + 0x78);
    const float along = fpu_div(lwc1(pv + 0x170), lwc1(pv + 0x174));
    const float t = fpu_add(fpu_add(static_cast<float>(lw(pv + 0x168)), lwc1(pv + 0x16c)), along);
    const int32_t i = float_to_int(t);
    sw(pv + 0x168, static_cast<uint32_t>(i));
    swc1(pv + 0x16c, fpu_sub(t, static_cast<float>(i)));
    if (!(i < lw(path) - 1)) return 1;
    const uint32_t sp = ee_stack_alloc(0x70);
    path_point(t, path, 0, m + 0x10, m + 0x40, 0);
    swc1(pv + 0x194, lwc1(m + 0x18));
    sw(sp, 0);
    const float left = vec3_distance(V(m + 0x10), V(path + (static_cast<uint32_t>(lw(path)) << 4)));
    const uint32_t tune = tuning();
    const float accel = fpu_mul(lwc1(tune + 0xc), lwc1(0x0015ed70));
    seek_1d(sp, pv + 0x170, left, accel, accel, fpu_mul(lwc1(tune + 8), dt()));
    ee_stack_free(0x70);
    return 0;
}

// Its size from its class's by the level's scale; its engine's sound kept up in states 2 .. 5 (else stopped). A hit (by the
// reaction, or its health gone, or 2 of damage in the hero's mode 2): 3 .. 8 it shudders (a sound, a tilt at random), 1 / 2 it
// is downed (sounds, its drones gone, state 8 for 90 frames). Then its colour pulse and its target: the hero within its
// target area (none without one or in the hero's mode 0x16: 2), the hero's moby and place when it has none.
// orig: L16:0x002e2b60 L18:0x002f11a8
void class1356_hits(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t state = sp, dmg = sp + 4;
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(tuning())));
    if (lbu(m + 0x20) - 2u < 4u) {
        if (sub_0028d8c0(m, lw(pv + 0x190)) == 0) sw(pv + 0x190, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    } else if (sub_0028d8c0(m, lw(pv + 0x190)) != 0) {
        sound_end(m, pv);
    }
    swc1(dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t reaction;
    {
        int32_t st = lw(state);
        float d = lwc1(dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &d, nullptr, 4);
        sw(state, static_cast<uint32_t>(st));
        swc1(dmg, d);
    }
    const uint32_t pulse = pv + 0x60;
    if (lw(state) != 1 && lbu(m + 0x20) != 8) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f || (lbu(0x001413f4) == 2 && 2.0f <= lwc1(dmg))) reaction = 1;
        if (reaction > 0 && reaction < 3) {
            moby_sound_start(3, 0, m);
            if (lbu(0x001413f4) != 2) moby_sound_start(2, 0, m);
            for (uint32_t k = 0; k < 4; ++k) {
                const int32_t drone = lw(pv + 0xc0 + k * 4);
                if (drone >= 0) moby_retire(moby_at(drone));
            }
            sb(m + 0x20, 8);
            sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x5a)));
            sb(pv + 0x67, 0x78);
        } else if (reaction >= 3 && reaction < 9) {
            moby_sound_start(3, 0, m);
            sb(pv + 0x67, 0x78);
            swc1(pv + 0x188, fpu_mul(fpu_mul(rand_range_signed_f(30.0f, 50.0f), DEG), dt()));
        }
        color_pulse_start(m, pulse);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    const int32_t area = lw(pv + 0xd0);
    if (area == -1 || lw(0x001413dc) == 0x16) {
        sw(pv + 0xb4, 2);
    } else {
        const uint32_t p = path_of(area);
        target_select(m, pv + 0x70, 0, 0, p + 0x10, lw(p), 128.0f);
    }
    if (lwu(pv + 0xb0) == 0) {
        sw(pv + 0xb0, lwu(0x001413d0));
        lq_sq(pv + 0x70, 0x0013f3d0);
    }
    ee_stack_free(0x80);
}

// Its hits first. Its states: 0 sets it up (the paths' headings, at the in path's start: 1; no path: 6), 1 waits for the hero in
// its starting area (and no scene), 2 flies in along the in path (the wings tilted near its end while it carries drones): 3
// with drones, else 4 (or 6 without an out path), 3 lets its drones go one at a time (every 30 frames), 4 turns to the out
// path, 5 flies out along it (then leaves, or 6 with a target area), 6 hovers turned to its target and fires every 180 frames
// (7: a class 50 shot from its joint 1 at the target), 8 is downed: it sinks and tips, then bursts (debris, bolts) and
// leaves. Then the bob about its height.
// orig: L16:0x002e22d8 L18:0x002f0920
void class1356_update(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t at = sp + 0x20, tmp = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    class1356_hits(m);
    bool gone = false;
    switch (lbu(m + 0x20)) {  // (the level's jump table of nine states)
    case 0: {  // (L16:0x002e2330)
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        swc1(pv + 0x20, 12.0f);
        sh(pv + 0x24, 12);
        sb(pv + 0x28, 3);
        if (lw(pv + 0xd8) != -1) class1356_path_headings(path_of(lw(pv + 0xd8)));
        if (lw(pv + 0xdc) != -1) class1356_path_headings(path_of(lw(pv + 0xdc)));
        sw(pv + 0x16c, 0);
        sw(pv + 0x168, 0);
        swc1(pv + 0x170, fpu_mul(lwc1(tune + 8), dt()));
        swc1(pv + 0x194, lwc1(m + 0x18));
        if (lw(pv + 0xd8) == -1) {
            sb(m + 0x20, 6);
            break;
        }
        const uint32_t in = path_of(lw(pv + 0xd8));
        swc1(pv + 0x174, vec3_distance(V(in + 0x10), V(in + 0x20)));
        lq_sq(m + 0x10, in + 0x10);
        const uint32_t bits = lhu(m + 0x34);
        sb(m + 0x20, 1);
        sb(m + 0x31, 0);
        sw(m + 0x94, 0);
        sh(m + 0x34, (bits | 1u) & 0xefffu);
        break;
    }
    case 1: {  // (L16:0x002e2420)
        if (lw(OV(0x0015f5c4)) != 0) break;
        if (lw(pv + 0xd4) != -1) {
            const uint32_t p = path_of(lw(pv + 0xd4));
            if (point_in_polygon(0x0013f3d0, p + 0x10, lw(p)) == 0) break;
        }
        sb(m + 0x20, 2);
        const uint32_t bits = lhu(m + 0x34) & 0xfffeu;
        sh(m + 0x34, bits);
        sb(m + 0x31, 1);
        V(at) = Vec4{};
        swc1(at + 8, NEAR_PI);
        sh(m + 0x34, bits | 0x1000u);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t drone = lw(pv + 0xc0 + k * 4);
            if (drone >= 0) class1356_attach(moby_at(drone), m, drone_point(), at);
        }
        break;
    }
    case 2: {  // (L16:0x002e24f8)
        bool carrying = false;
        for (uint32_t k = 0; k < 4; ++k)
            if (lw(pv + 0xc0 + k * 4) >= 0) carrying = true;
        const uint32_t in = path_of(lw(pv + 0xd8));
        if (carrying && fpu_mul(static_cast<float>(lw(in) - lw(pv + 0x168)), lwc1(pv + 0x174)) < 16.0f) tilt_to(pv, HALF_PI);
        if (class1356_follow(m, in) == 0) break;
        if (carrying) sb(m + 0x20, 3);
        else sb(m + 0x20, lw(pv + 0xdc) == -1 ? 6 : 4);
        break;
    }
    case 3: {  // (L16:0x002e25fc)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x184))) == 0) break;
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t slot = pv + 0xc0 + k * 4;
            const int32_t drone = lw(slot);
            if (drone >= 0) {  // (the next drone let go below it)
                vec_clear(V(tmp));
                swc1(tmp + 8, angle_add(lwc1(m + 0x48), NEAR_PI));
                mat3_transform(V(at), V(drone_point()), M(m + 0xc0));
                vec3_add(V(at), V(at), V(m + 0x10));
                class1356_release(moby_at(drone), frame_scale_i(0x5a));
                sw(slot, 0xffffffffu);
                sw(pv + 0x184, static_cast<uint32_t>(frame_scale_i(0x1e)));
                break;
            }
            if (k == 3) sb(m + 0x20, lw(pv + 0xdc) == -1 ? 6 : 4);
        }
        break;
    }
    case 4: {  // (L16:0x002e2668)
        const float heading = lwc1(path_of(lw(pv + 0xdc)) + 0x1c);
        const float rate = fpu_mul(lwc1(0x0015ed70), TWO_PI);
        seek_1d(m + 0x48, pv + 0x178, heading, rate, rate, fpu_mul(dt(), TWO_PI));
        if (angle_dist(lwc1(m + 0x48), heading) < 0.01f) {
            sb(m + 0x20, 5);
            sw(pv + 0x168, 0);
            sw(pv + 0x16c, 0);
        }
        break;
    }
    case 5:  // (L16:0x002e26f8)
        tilt_to(pv, 0.0f);
        if (class1356_follow(m, path_of(lw(pv + 0xdc))) == 0) break;
        if (lw(pv + 0xd0) == -1) {
            moby_retire(m);
            gone = true;
            break;
        }
        sb(m + 0x20, 6);
        break;
    case 6: {  // (L16:0x002e277c)
        tilt_to(pv, 0.0f);
        const float yaw = atan2_f(fpu_sub(lwc1(pv + 0x70), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x74), lwc1(m + 0x14)));
        const float turn = fpu_mul(lwc1(0x0015ed70), PI);
        seek_1d(m + 0x48, pv + 0x178, yaw, turn, turn, fpu_mul(dt(), TWO_PI));
        const float roll = fpu_mul(lwc1(0x0015ed70), HALF_PI);
        seek_1d(m + 0x40, pv + 0x188, 0.0f, roll, roll, fpu_mul(dt(), TWO_PI));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x164))) == 0) break;
        if (lw(pv + 0xb4) == 2) break;
        sb(m + 0x20, 7);
        break;
    }
    case 7: {  // (L16:0x002e2878)
        const uint32_t vel = tmp;
        moby_chain_point_v(m, 1, V(at));
        vec3_sub(V(vel), V(pv + 0x70), V(at));
        vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(tune + 4), dt()));
        moby_sound_start(1, 0, m);
        class50_spawn(m, vel, at, pv + 0x70, frame_scale_i(0x2d));
        sb(m + 0x20, 6);
        sw(pv + 0x164, static_cast<uint32_t>(frame_scale_i(0xb4)));
        break;
    }
    case 8: {  // (L16:0x002e2904)
        const float step = dt();
        swc1(pv + 0x194, fpu_sub(lwc1(pv + 0x194), fpu_add(step, step)));
        swc1(m + 0x40, fpu_add(lwc1(m + 0x40), fpu_mul(step, u2f(0x3e860a92u))));  // (15 degrees)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) == 0 && lbu(0x001413f4) != 2) break;
        const uint32_t zero = OV(0x0015f580);
        explosion_effect(m, zero, m + 0x10, 0x14, 0xc, 8, -1, 0, 0, -1, 0, 0.0f, 0.0f, 4.0f, 2.5f, 9.0f, 2.0f, 0.0f);
        debris_spawn(m, 0x634, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        debris_spawn(m, 0x635, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        debris_spawn(m, 0x636, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
        gone = true;
        break;
    }
    default:  // (past the table)
        break;
    }
    if (!gone) {
        const float phase = angle_add(lwc1(pv + 0x18c), fpu_mul(fpu_mul(lwc1(tune + 0x10), DEG), dt()));
        swc1(pv + 0x18c, phase);
        swc1(m + 0x18, fpu_add(lwc1(pv + 0x194), fpu_mul(lwc1(tune + 0x14), sin_vu(phase))));
        class1356_joint(m);
    }
    ee_stack_free(0xb0);
}

}  // namespace rac1
