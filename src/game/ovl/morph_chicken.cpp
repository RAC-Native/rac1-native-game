// Ported overlay code: the Morph-o-Ray's chicken (class 270, made by 0x2c9348 from a morphed enemy): its update 0x2c97e0,
// its class hook +0xc 0x2caf10 (the class table 0x1ea7d4), and the debris its burst throws (class 428: spawned by
// 0x2cbf68, updated by 0x2cc210).
//
// The chicken's pvar: +0x10 / +0x14 the carry data (carry_update), +0x20 the joint node the gold flag scales, +0x64 the
// cluck timer, +0x66 the state timer, +0x68 the turn rate, +0x6c the fall speed, +0x70 the countdown to its end under load
// (-1: none), +0x74 the puffs (an explosion when it bursts), +0x78 its health (it bursts below 1), +0x7c the hit timer.
// Its states: 1 idle (pecking), 2 turning away from the hero, 3 running away, 4 fleeing, 5 carried, 6 gone (not drawn).
#include <game/math/vecmath.h>
#include <game/lib/libc.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/weapon_classes.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t GOLD_MORPH_FLAG = 0x0013e535u;  // (0x13e520 + item 0x15)
constexpr uint32_t FRAME_DT = 0x0015ed60u;
constexpr uint32_t FRAME_SCALE = 0x0015ed6cu;
constexpr uint32_t FRAME_SCALE_2 = 0x0015ed70u;

constexpr OvConst<0x0015f5d0u> LOAD_A{};  // the frame's load factors (above 0.85 .. 1.0: effects cut)
constexpr OvConst<0x0015f5d4u> LOAD_B{};
constexpr OvConst<0x00161850u> CHICKEN_JOINT{};
constexpr OvConst<0x00161858u> DEBRIS_PITCH{};  // degrees
constexpr OvConst<0x0016185cu> DEBRIS_SPEED_LO{};
constexpr OvConst<0x00161860u> DEBRIS_SPEED_HI{};
constexpr OvConst<0x00161864u> DEBRIS_RISE{};
constexpr OvConst<0x00161868u> DEBRIS_SPREAD_LO{};
constexpr OvConst<0x0016186cu> DEBRIS_SPREAD_HI{};
constexpr OvConst<0x001618a0u> DEBRIS_SCALE{};
constexpr OvConst<0x001618a4u> DEBRIS_LIFE{};  // frames (an integer)
constexpr OvConst<0x001618a8u> DEBRIS_DRAG{};
constexpr OvConst<0x00173e68u> FLOOR_Z{};       // the last floor probe's height
constexpr OvConst<0x00173e80u> FLOOR_NORMAL{};  // and its normal
constexpr OvConst<0x00177f00u> HIT_LIST{};

constexpr float DEG2RAD = std::bit_cast<float>(0x3c8efa35u);
constexpr float QUARTER_PI = std::bit_cast<float>(0x3f490fdbu);
constexpr float HALF_PI = std::bit_cast<float>(0x3fc90fdbu);
constexpr float TWO_PI = std::bit_cast<float>(0x40c90fdbu);
constexpr float STEP_SLOPE = std::bit_cast<float>(0x3e860a92u);  // 15 degrees

// One piece of debris thrown from around the chicken (`home` its position): `pos` and `vel` are EE buffers.
// orig: (helper) repeated in 0x002c97e0 (the burst's loop, state 5)
void throw_debris(uint32_t m, const Vec4& home, uint32_t pos, uint32_t vel, float size) {
    const float r = fpu_mul(rand_range_f(lwc1(DEBRIS_SPREAD_LO), lwc1(DEBRIS_SPREAD_HI)), size);
    const float a = rand_angle();
    const float b = rand_angle();
    vec_from_spherical_v(V(pos), r, a, b);
    swc1(pos + 8, fpu_add(lwc1(pos + 8), fpu_mul(lwc1(DEBRIS_RISE), size)));
    vec3_add(V(pos), V(pos), home);
    const float dt = lwc1(FRAME_SCALE);
    const float speed = rand_range_f(fpu_mul(lwc1(DEBRIS_SPEED_LO), dt), fpu_mul(lwc1(DEBRIS_SPEED_HI), dt));
    const float yaw = rand_angle();
    vec_from_spherical_v(V(vel), speed, yaw, fpu_mul(lwc1(DEBRIS_PITCH), DEG2RAD));
    swc1(vel + 8, fpu_mul(rand_range_f(3.5f, 5.5f), lwc1(FRAME_SCALE)));
    chicken_debris_spawn(m, pos, vel);
}

// A step of `len` along the chicken's heading from `home`, into the EE buffer `step`.
// orig: (helper) repeated in 0x002c97e0's states
void heading_step(uint32_t m, uint32_t step, const Vec4& home, float len, float scale) {
    swc1(step + 0, fpu_mul(fpu_mul(cos_vu(lwc1(m + 0x48)), len), scale));
    swc1(step + 4, fpu_mul(fpu_mul(sin_vu(lwc1(m + 0x48)), len), scale));
    swc1(step + 8, 0.0f);
    vec3_add(V(step), V(step), home);
}

// walker_step with the chicken's sizes: from `from` toward `to` (EE buffers), `fall` the drop it may take
int32_t chicken_step(uint32_t m, uint32_t from, uint32_t to, float size, float fall) {
    return walker_step(m, from, to, 0, fpu_mul(size, u2f(0x3e4ccccdu)), fpu_mul(size, 0.5f), fall, STEP_SLOPE);
}

// turn away from the hero (`away`: the heading from the hero to the chicken)
void turn_away(uint32_t m, uint32_t pv, float away) {
    const float k = lwc1(FRAME_SCALE_2);
    yaw_turn_damped(m, pv + 0x68, away, fpu_mul(k, QUARTER_PI), fpu_mul(k, HALF_PI), fpu_mul(lwc1(FRAME_SCALE), QUARTER_PI));
}

// a new state with an animation of 10 frames
void set_state_anim(uint32_t m, uint32_t state, uint32_t anim) {
    sb(m + 0x20, state);
    moby_anim_set(m, anim, 0, frame_scale_i(10));
}

// back to idle (state 1 or the given one) for 1 .. 3 s, pecking (one of the two idle animations)
void settle(uint32_t m, uint32_t pv, uint32_t state) {
    sb(m + 0x20, state);
    sh(pv + 0x66, static_cast<uint32_t>(rand_range(frame_scale_i(0x3c), frame_scale_i(0xb4))));
    const uint32_t anim = rand_mod(2) == 0 ? 1u : 0u;
    moby_anim_set(m, anim, 0, frame_scale_i(5));
}

// After a step that did not stall: settled (state 1) when it turned around (more than 90 degrees off the step) or barely
// moved; true then.
bool step_ended(uint32_t m, uint32_t home, float size) {
    const float to_step = atan2_f(fpu_sub(lwc1(home + 0), lwc1(m + 0x10)), fpu_sub(lwc1(home + 4), lwc1(m + 0x14)));
    if (HALF_PI < angle_dist(lwc1(m + 0x48), to_step)) return true;
    return vec3_distance(V(m + 0x10), V(home)) < fpu_mul(size, u2f(0x3c23d70au));
}

// The idle chicken's step: kept (it starts running) when it goes less than 90 degrees off its heading and moved it.
// (Not quite !step_ended: both compares the other way round, strict.)
bool step_kept(uint32_t m, uint32_t home, float size) {
    const float to_step = atan2_f(fpu_sub(lwc1(home + 0), lwc1(m + 0x10)), fpu_sub(lwc1(home + 4), lwc1(m + 0x14)));
    if (!(angle_dist(lwc1(m + 0x48), to_step) < HALF_PI)) return false;
    return fpu_mul(size, u2f(0x3c23d70au)) < vec3_distance(V(m + 0x10), V(home));
}
}  // namespace

// Class 428, a piece of the chicken's debris: it flies on its velocity (pvar +0, the fall limited by +0x20, slowed by the
// level's drag), spins (+0x28 heading, +0x2c tumble whose direction +0x24 swaps past 45 degrees) and fades out when its life
// (+0x1c) runs low; retired when its life ends or it is not drawn.
// orig: 0x002cc210
void class428_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    const float gravity = fpu_mul(lwc1(FRAME_SCALE_2), 15.0f);
    const float vz = fpu_sub(lwc1(pv + 8), gravity);
    const float drag = lwc1(DEBRIS_DRAG);
    const float min_vz = lwc1(pv + 0x20);
    swc1(pv + 8, vz);
    swc1(pv + 4, fpu_mul(lwc1(pv + 4), drag));
    swc1(pv + 0, fpu_mul(lwc1(pv + 0), drag));
    if (vz < min_vz) swc1(pv + 8, min_vz);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x28)));
    const float tumble = angle_add(lwc1(m + 0x40), fpu_mul(lwc1(pv + 0x2c), static_cast<float>(lw(pv + 0x24))));
    swc1(m + 0x40, tumble);
    if (QUARTER_PI < abs_f(tumble)) sw(pv + 0x24, 0u - lwu(pv + 0x24));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c))) != 0 || lbu(m + 0x31) == 0) {
        moby_retire(m);
        return;
    }
    if (lw(pv + 0x1c) < frame_scale_i(0x1e) && countdown_u8(m + 0x23) != 0) moby_retire(m);
}

// A piece of debris (class 428) at `pos` (raised a little at random) thrown along `vel`: a random spin, a life of 0.7 .. 1.2
// times the level's, the size of the level's (twice it with the gold Morph-o-Ray). 0 when none could be made.
// orig: 0x002cbf68
uint32_t chicken_debris_spawn(uint32_t chicken, uint32_t pos, uint32_t vel) {
    (void)chicken;
    const uint32_t m = moby_spawn(0x1ac);
    if (m == 0) return 0;
    sb(m + 0x30, 0x40);
    sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
    sh(m + 0x32, 0x20);
    sb(m + 0x31, 1);
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x20, 0);
    sb(m + 0xbc, 0);
    V(m + 0x10) = V(pos);
    const float rise = rand_range_f(u2f(0x3e4ccccdu), u2f(0x3f333333u));
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), rise));
    swc1(m + 0x10, fpu_add(lwc1(m + 0x10), fpu_mul(lwc1(vel + 0), 3.0f)));
    swc1(m + 0x14, fpu_add(lwc1(m + 0x14), fpu_mul(lwc1(vel + 4), 3.0f)));
    V(pv) = V(vel);
    const float ten_deg = u2f(0x3e32b8c2u);
    swc1(m + 0x44, rand_range_signed_f(ten_deg, u2f(0x3f060a92u)));
    swc1(m + 0x40, rand_range_f(ten_deg, QUARTER_PI));
    sw(pv + 0x24, 0xffffffffu);
    swc1(pv + 0x2c, fpu_mul(fpu_mul(rand_range_f(200.0f, 370.0f), DEG2RAD), lwc1(FRAME_SCALE)));
    if ((libc_rand() & 1) != 0) {
        sw(pv + 0x24, 1);
        swc1(m + 0x44, -lwc1(m + 0x44));
    }
    swc1(m + 0x48, rand_angle());
    swc1(pv + 0x28, -fpu_mul(fpu_mul(rand_range_f(200.0f, 550.0f), DEG2RAD), lwc1(FRAME_SCALE)));
    swc1(pv + 0x10, rand_angle());
    swc1(pv + 0x14, rand_angle());
    const int32_t life_lo = float_to_int(fpu_mul(static_cast<float>(lw(DEBRIS_LIFE)), u2f(0x3f333333u)));
    const int32_t life_hi = float_to_int(fpu_mul(static_cast<float>(lw(DEBRIS_LIFE)), u2f(0x3f99999au)));
    sw(pv + 0x1c, static_cast<uint32_t>(frame_scale_i(rand_range(life_lo, life_hi))));
    float scale = fpu_mul(lwc1(m + 0x2c), lwc1(DEBRIS_SCALE));
    swc1(m + 0x2c, scale);
    if (lbu(GOLD_MORPH_FLAG) != 0) {
        scale = fpu_add(scale, scale);
        swc1(m + 0x2c, scale);
    }
    swc1(pv + 0x20, fpu_mul(rand_range_f(u2f(0xbf99999au), u2f(0xbecccccdu)), lwc1(FRAME_SCALE)));
    moby_update_transform(m);
    moby_copy_hero_color(m);
    return m;
}

// Class 270's hook +0xc (when its carrier lets it go: 0x2dc9f0's countdown): carry_hookc, then state 5.
// orig: 0x002caf10
void chicken_hook_c(uint32_t m) { (void)enemy10e_hookc(m, 0, 0, 0.0f); }

// Class 270, the Morph-o-Ray's chicken: it pecks about, turns and runs from the hero when it comes near, falls to the
// floor and leans with it, clucks now and then, and bursts into feathers (or, puffed, an explosion) and debris when its
// health runs out; under a heavy frame load it may end early.
// orig: 0x002c97e0
void chicken_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO_POS + 0)), fpu_sub(lwc1(m + 0x14), lwc1(HERO_POS + 4)));
    const float saved_58 = lwc1(m + 0x58);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sh(pv + 0x66, 0);
        sw(pv + 0x70, 0xffffffffu);
        sh(pv + 0x64, static_cast<uint32_t>(rand_range(frame_scale_i(0x78), frame_scale_i(0x12c))));
        swc1(pv + 0x78, 1.0f);
    }
    if (lbu(m + 0x20) == 6) return;
    joint_node_scale(m, lwu(CHICKEN_JOINT), pv + 0x20, lbu(GOLD_MORPH_FLAG) != 0 ? 5.0f : u2f(0x40f66666u));

    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t home = frame;          // where it stands (the walker steps from it, and move it)
    const uint32_t step = frame + 0x10;   // where it steps to
    const uint32_t vel = frame + 0x20;    // a thrown piece's velocity
    const uint32_t pos = frame + 0x30;    // and position
    auto done = [frame] { ee_stack_free(0x40); };
    auto retire = [m, frame] {
        moby_retire(m);
        ee_stack_free(0x40);
    };
    V(home) = V(m + 0x10);

    // under load, the countdown to its end (pvar +0x70: set at random when the load is high, cut short when very high)
    if (lw(pv + 0x70) >= 0) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x70))) != 0 && lbu(m + 0x31) == 0) return retire();
        const int32_t left = lw(pv + 0x70);
        if (left >= 0) {
            const float high = u2f(0x3f666666u);
            if ((high < lwc1(LOAD_B) || high < lwc1(LOAD_A)) && lbu(m + 0x31) == 0 && left != -1) {
                if (rand_mod(0xc7) == 0) return retire();
                sw(pv + 0x70, static_cast<uint32_t>(rand_range(0x14, 0x1e)));
            }
            if (1.0f < lwc1(LOAD_B) || 1.0f < lwc1(LOAD_A)) {
                if (lbu(m + 0x31) != 0) {
                    if (rand_mod(0x63) == 0) sw(pv + 0x78, 0);
                    else sw(pv + 0x70, static_cast<uint32_t>(rand_range(0x14, 0x1e)));
                } else if (rand_mod(0x18) == 0) {
                    return retire();
                }
            }
        }
    }

    // puffed: grows toward 1 + 3 x the puffs of its class's size
    const int32_t puffs = lw(pv + 0x74);
    if (puffs > 0) {
        const float want = fpu_mul(fpu_add(fpu_mul(int_to_float(puffs), 3.0f), 1.0f), lwc1(lwu(m + 0x24) + 0x24));
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(scale, fpu_mul(fpu_sub(want, scale), u2f(0x3dcccccdu))));
    }
    const float size = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));  // its scale against its class's

    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c))) != 0) {
        const uint32_t hit = sub_0025a420(m, 0xffffffffu, 0);
        if (hit != 0) {
            moby_sound_start(1, 0, m);
            swc1(pv + 0x78, fpu_sub(lwc1(pv + 0x78), lwc1(hit + 0x2c)));
            sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(0x19)));
        }
    }
    sb(m + 0xa4, 0xff);

    if (lwc1(pv + 0x78) < 1.0f) {  // the burst
        if (lw(pv + 0x74) == 0) {
            // feathers: 20 for each unit of its size
            for (int32_t i = 0; i < float_to_int(size) * 20; ++i) {
                const float r = fpu_mul(rand_range_f(0.0f, u2f(0x3ebd70a4u)), size);
                const float a = rand_angle();
                const float b = rand_angle();
                vec_from_spherical_v(V(pos), r, a, b);
                swc1(pos + 8, fpu_add(lwc1(pos + 8), fpu_mul(rand_range_f(0.25f, 0.5f), size)));
                vec3_add(V(pos), V(pos), V(home));
                const float dt = lwc1(FRAME_SCALE);
                const float speed = fpu_mul(rand_range_f(fpu_mul(dt, u2f(0x3dcccccdu)), dt), size);
                const float va = rand_angle();
                const float vb = rand_angle();
                vec_from_spherical_v(V(vel), speed, va, vb);
                const float feather = fpu_mul(size, u2f(0x48127c00u));
                const int32_t frames = rand_range(frame_scale_i(0x1e), frame_scale_i(0x39));
                particle_16_spawn(pos, vel, 0x3f7f7f7fu, NOT_ADDR(0x002f2f2fu), frames, feather);
            }
        } else {
            // an explosion that hurts what is around
            const float dt = lwc1(FRAME_DT);
            swc1(pos + 0, fpu_mul(dt, 0.5f));
            swc1(pos + 4, fpu_mul(dt, 0.5f));
            swc1(pos + 8, fpu_mul(dt, u2f(0x3f333333u)));
            sw(pos + 12, 0);
            V(vel) = V(pos);
            explosion_effect(m, vel, 0, 0, 0, 0, -1, 1, 1, -1, lbu(pv + 0x74), 0.0f, 2.0f, 4.0f, 3.0f, 9.0f, 1.0f, 12.0f);
            const int32_t n = mobys_in_sphere(m + 0x10, 3.0f, 0x10, m, 0);
            V(pos) = V(m + 0x10);
            moby_list_hit(m, pos, HIT_LIST, n, 0, NOT_ADDR(0x00830000u), 2, 1, 1.0f, 1.0f, 1.0f);
        }
        // then 5 .. 8 pieces of debris, halved under load, one under a heavy one
        int32_t count = rand_range(5, 8);
        const float load_a = lwc1(LOAD_A);
        if (u2f(0x3f59999au) < load_a || u2f(0x3f59999au) < lwc1(LOAD_B)) {
            if (1.0f < load_a || 1.0f < lwc1(LOAD_B)) count = 1;
            else count /= 2;
        }
        for (; count > 0; --count) throw_debris(m, V(home), pos, vel, size);
        moby_sound_start(1, 0, m);
        if (lw(pv + 0x70) >= 0) return retire();
        // kept (no countdown): hidden, state 6
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) & 0xefffu) | 0x41u);
        sw(pv + 0x74, 0);
        sb(m + 0x20, 6);
        sb(m + 0xbc, 0);
        return done();
    }

    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x64))) != 0) {  // a cluck
        moby_sound_start(rand_mod(100) < 75 ? 0 : 1, 0, m);
        sh(pv + 0x64, static_cast<uint32_t>(rand_range(frame_scale_i(0x78), frame_scale_i(0x12c))));
    }
    // the drop a step may take
    const float fall = lw(pv + 0x70) < 0 ? u2f(0x3eaa7efau) : fpu_mul(fpu_add(lwc1(FRAME_SCALE), lwc1(FRAME_SCALE)), size);

    int32_t blocked = 0;  // the walker's result (a step that stalled skips the floor check below)
    const float near = fpu_mul(size, 5.0f);
    // (the original dispatches through its jump table 0x1e9d80 by the state, 0 .. 5; here the state itself, the table's NTSC-U
    // targets in the comments)
    switch (lbu(m + 0x20)) {
    case 1: {  // (0x002c9fa4) idle: pecking, now and then a step
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, rand_mod(2) == 0 ? 1u : 0u, 0, frame_scale_i(5));
        const bool ticked = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x66))) != 0;
        if (ticked && vec2_distance(V(m + 0x10), V(HERO_POS)) < near) {
            heading_step(m, step, V(home), lwc1(FRAME_SCALE), size);
            blocked = chicken_step(m, home, step, size, fall);
            if (blocked != 0 || step_kept(m, home, size)) {  // off it runs
                set_state_anim(m, 3, 3);
            } else {
                sh(pv + 0x66, static_cast<uint32_t>(rand_range(frame_scale_i(0x3c), frame_scale_i(0xb4))));
            }
        } else if (vec2_distance(V(m + 0x10), V(HERO_POS)) < near && u2f(0x3f060a92u) < angle_dist(away, lwc1(m + 0x48))) {
            chicken_step(m, home, home, size, fall);
            set_state_anim(m, 2, 3);
        } else {
            chicken_step(m, home, home, size, fall);
        }
        break;
    }
    case 2: {  // (0x002ca21c) turning away from the hero
        turn_away(m, pv, away);
        chicken_step(m, home, home, size, fall);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x66))) != 0 &&
            vec2_distance(V(m + 0x10), V(HERO_POS)) < near) {
            heading_step(m, step, V(home), lwc1(FRAME_SCALE), size);
            const float floor = floor_probe_v(0.5f, V(step), 0);
            if (abs_f(fpu_sub(floor, lwc1(step + 8))) <= fpu_mul(fpu_add(lwc1(FRAME_SCALE), lwc1(FRAME_SCALE)), size))
                set_state_anim(m, 3, 3);
        } else if (angle_dist(away, lwc1(m + 0x48)) < u2f(0x3db2b8c2u) ||
                   fpu_mul(size, 6.0f) < vec2_distance(V(m + 0x10), V(HERO_POS))) {
            settle(m, pv, 1);
        }
        break;
    }
    case 3: {  // (0x002ca3dc) running away
        const float dist = vec2_distance(V(m + 0x10), V(HERO_POS));
        if (fpu_mul(size, 6.0f) < dist) {  // far enough: idle
            settle(m, pv, 1);
            chicken_step(m, home, home, size, fall);
            break;
        }
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < fpu_mul(size, 3.0f)) {  // close: a big step, then flee
            turn_away(m, pv, away);
            const float two = fpu_add(lwc1(FRAME_SCALE), lwc1(FRAME_SCALE));
            swc1(step + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), two));
            swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_add(lwc1(FRAME_SCALE), lwc1(FRAME_SCALE))));
            swc1(step + 8, 0.0f);
            vec3_add(V(step), V(step), V(home));
            blocked = chicken_step(m, home, step, size, fall);
            if (blocked == 0 && step_ended(m, home, size)) settle(m, pv, 1);
            else set_state_anim(m, 4, 2);
            break;
        }
        turn_away(m, pv, away);
        heading_step(m, step, V(home), lwc1(FRAME_SCALE), size);
        blocked = chicken_step(m, home, step, size, fall);
        if (blocked == 0 && step_ended(m, home, size)) settle(m, pv, 1);
        break;
    }
    case 4: {  // (0x002ca6c0) fleeing
        const bool ticked = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x66))) != 0;
        if (ticked && fpu_mul(size, 4.0f) < vec2_distance(V(m + 0x10), V(HERO_POS))) {  // far: back to running
            turn_away(m, pv, away);
            heading_step(m, step, V(home), lwc1(FRAME_SCALE), size);
            blocked = chicken_step(m, home, step, size, fall);
            if (blocked == 0 && step_ended(m, home, size)) settle(m, pv, 1);
            else set_state_anim(m, 3, 3);
            break;
        }
        turn_away(m, pv, away);
        heading_step(m, step, V(home), fpu_add(lwc1(FRAME_SCALE), lwc1(FRAME_SCALE)), size);
        blocked = chicken_step(m, home, step, size, fall);
        if (blocked == 0 && step_ended(m, home, size)) settle(m, pv, 1);
        break;
    }
    case 5:  // (0x002caa04) carried: let go when carry_update says so; a piece of debris now and then
        if (carry_update(m, lwu(pv + 0x10)) != 0) {
            sb(m + 0x20, 0);
            sh(lwu(pv + 0x14) + 0x68, 0);
        }
        if (rand_mod(4) == 0) throw_debris(m, V(home), pos, vel, size);
        return done();
    default:  // (0x002cab18: state 0 and above 5)
        break;
    }

    // the step taken (limited to a frame's length) kept unless it would leave the floor; then the fall
    if (blocked == 0) {
        Vec4 d{};
        vec3_sub(d, V(home), V(m + 0x10));
        const float max_len = fpu_mul(lwc1(FRAME_SCALE), size);
        if (max_len < vec3_length(d)) vec3_set_length(d, d, max_len);
        vec3_add(V(home), V(m + 0x10), d);
        const float floor = floor_probe_20(home);
        if (abs_f(fpu_sub(floor, lwc1(m + 0x18))) <= fpu_mul(lwc1(FRAME_SCALE), size)) V(m + 0x10) = V(home);
    } else {
        V(m + 0x10) = V(home);
    }
    swc1(pv + 0x6c, fpu_add(lwc1(pv + 0x6c), fpu_mul(lwc1(FRAME_SCALE_2), u2f(0x411ccccdu))));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x6c)));
    const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
    const float z = lwc1(m + 0x18);
    if (z < floor && floor < fpu_add(z, fpu_mul(size, 0.25f))) {
        sw(pv + 0x6c, 0);
        swc1(m + 0x18, floor);
    }
    if (sub_001f35d8() == 0) sw(pv + 0x78, 0);
    if (lbu(m + 0x20) != 5) {  // on the floor: lean with it
        swc1(m + 0x18, lwc1(FLOOR_Z));
        const Vec4 n = V(FLOOR_NORMAL);
        const float c = cos_vu(lwc1(m + 0x48));
        const float s = sin_vu(lwc1(m + 0x48));
        const float fwd = fpu_add(fpu_mul(n.x, c), fpu_mul(n.y, s));
        const float c2 = cos_vu(lwc1(m + 0x48));
        const float s2 = sin_vu(lwc1(m + 0x48));
        const float side = fpu_sub(fpu_mul(n.y, c2), fpu_mul(n.x, s2));
        const float pitch = -atan2_f(sqrt_f(fpu_add(fpu_mul(fwd, fwd), fpu_mul(n.z, n.z))), side);
        const float roll = atan2_f(n.z, fwd);
        const float dp = angle_sub(pitch, lwc1(m + 0x40));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dp, fpu_mul(lwc1(FRAME_SCALE), TWO_PI))));
        const float dr = angle_sub(roll, lwc1(m + 0x44));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(dr, fpu_mul(lwc1(FRAME_SCALE), TWO_PI))));
    }
    if (lwc1(m + 0x18) < 0.0f) return retire();
    swc1(m + 0x58, saved_58);
    done();
}

}  // namespace rac1
