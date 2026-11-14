// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 911, a flame jet (a nozzle that sprays fire, either
// sweeping to and fro or blowing in bursts), and its three helpers. Header blarg_own.h; registered in blarg_mobys.cpp.
// Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the jets' shared tuning at 0x161cec .. 0x161d28), the
// base ELF's are bare.
#include <algorithm>

#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position
const float kDegree = u2f(0x3c8efa35u);    // pi / 180

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// a word of the level's data read as an integer and turned into a float (cvt.s.w)
float int_word(uint32_t a) { return static_cast<float>(lw(a)); }

// The hit record a flame's line test hands segment_collide at `rec` (0x24 bytes): what it touches is burnt by 1.0 with
// the damage kind 3, the jet `m` its source (its class +0xa6); the flags 0x10001, or 0x10000 while 0x1404a8 is 6.
void flame_hit_record(uint32_t rec, uint32_t m) {
    sw(rec + 0x14, (lw(0x001404a8) ^ 6) != 0 ? 0x10001u : 0x10000u);
    sw(rec + 0x10, m);
    swc1(rec + 0x1c, 1.0f);
    sw(rec + 0x20, 1);
    vec_clear(V(rec));
    swc1(rec + 0x8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));  // 5627.92
    sb(rec + 0x18, 3);
    sb(rec + 0x19, 1);
    sh(rec + 0x1a, lhu(m + 0xa6));
}

// The jet's sound in its slot pvar +0x94 kept playing (started again when the slot no longer plays it).
void flame_sound_keep(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x94)) == 0) sw(pv + 0x94, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
}
}  // namespace

// The flames' particles (two of type 2 a frame) when the hero is near the jet (within 10, or 24 and the nozzle on
// screen): from a random point along the jet's direction (its rotation +0x40 applied to the speed 0x161cec), drifting by
// a random spread (0x161d20) and the jet's push (0x161d1c, 0x161d18 up); their colours 0x161cfc / 0x161d00, lives and
// sizes picked from the ranges 0x161d04 .. 0x161d14.
// orig: L6:0x002f3d70
void flame_jet_particles_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t pos = m + 0x10;
    const uint32_t rot = sp + 0x10, from = sp + 0x50, dir = sp + 0x60, push = sp + 0x70, along = sp + 0x80,
                   vel = sp + 0x90, nozzle = sp + 0xa0;
    lq_sq(nozzle, pos);
    swc1(nozzle + 0xc, 4.0f);
    if (10.0f < vec3_distance(V(HERO_POS), V(pos)) && point_on_screen(nozzle, 24.0f) == -1) {
        ee_stack_free(0x160);
        return;
    }
    if (24.0f < vec3_distance(V(HERO_POS), V(pos))) {
        ee_stack_free(0x160);
        return;
    }
    lq_sq(from, pos);
    euler_to_mat4_vu(M(rot), V(m + 0x40));
    vec_clear(V(dir));
    swc1(dir, fpu_mul(lwc1(L6_ADDR(0x00161cec)), dt()));
    mat4_transform(V(dir), V(dir), M(rot));
    swc1(dir + 0xc, lwc1(L6_ADDR(0x00161cf4)));
    vec3_scale(V(push), V(dir), lwc1(L6_ADDR(0x00161d1c)));
    const float rise = fpu_mul(lwc1(L6_ADDR(0x00161d18)), dt());
    swc1(vel + 0xc, lwc1(L6_ADDR(0x00161cf8)));
    swc1(push + 0x8, rise);
    // a random whole number of frames from the range [lo, hi) (as floats), scaled to the frame rate
    auto frames = [](uint32_t lo, uint32_t hi) { return float_to_int(frame_scale_f(rand_range_f(int_word(lo), int_word(hi)))); };
    int32_t life = frames(L6_ADDR(0x00161d04), L6_ADDR(0x00161d08));
    int32_t fade = frames(L6_ADDR(0x00161d0c), L6_ADDR(0x00161d10));
    life = std::max(life, frames(L6_ADDR(0x00161d04), L6_ADDR(0x00161d08)));
    fade = std::max(fade, frames(L6_ADDR(0x00161d0c), L6_ADDR(0x00161d08)));  // (the original's range: 0x161d0c to 0x161d08)
    const int32_t size = float_to_int(static_cast<float>(frame_scale_i(lw(L6_ADDR(0x00161d14)))));
    auto spawn = [&] {
        vec3_scale(V(along), V(dir), rand_range_f(0.0f, 1.0f));
        vec3_add(V(from), V(pos), V(along));
        rand_sphere_point(vel, 0.0f, fpu_mul(lwc1(L6_ADDR(0x00161d20)), dt()));
        vec3_add(V(vel), V(vel), V(push));
        spawn_particle_02(from, dir, vel, lwu(L6_ADDR(0x00161cfc)), lwu(L6_ADDR(0x00161d00)), static_cast<uint32_t>(life),
                          static_cast<uint32_t>(fade), static_cast<uint32_t>(size), 0x30);
    };
    spawn();
    life = frames(L6_ADDR(0x00161d04), L6_ADDR(0x00161d08));
    fade = frames(L6_ADDR(0x00161d0c), L6_ADDR(0x00161d10));
    spawn();
    ee_stack_free(0x160);
}

// The sweeping jet's flame tested against what it touches: four lines along the arc the nozzle swept in the last
// quarter of a flame's flight (the flame's reach 0x161cf0 over its speed 0x161cec), from the full reach inward by a
// quarter each, each line from one point of the sweep to the next (the yaw +0x84 swung by the amplitude +0x64 times the
// sine of the sweep's angle +0x7c, which turns by +0x80 a frame); what a line meets is burnt (flame_hit_record).
// orig: L6:0x002f4130
void flame_jet_sweep_hits_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t rec = sp, a = sp + 0x30, b = sp + 0x40;
    const uint32_t pv = lwu(m + 0x78);
    const float quarter = 0.25f;
    const float flight = fpu_mul(fpu_div(lwc1(L6_ADDR(0x00161cf0)), fpu_mul(lwc1(L6_ADDR(0x00161cec)), dt())), quarter);
    const float step = fpu_mul(lwc1(pv + 0x80), static_cast<float>(float_to_int(flight)));  // the sweep's turn in that time
    const float reach_step = fpu_mul(lwc1(L6_ADDR(0x00161cf0)), quarter);
    float reach = lwc1(L6_ADDR(0x00161cf0));
    float sweep = angle_sub(lwc1(pv + 0x7c), fpu_mul(step, 4.0f));
    flame_hit_record(rec, m);
    auto yaw_at = [&](float s) { return angle_add(lwc1(pv + 0x84), fpu_mul(fpu_mul(lwc1(pv + 0x64), kDegree), sin_vu(s))); };
    for (int k = 0; k < 4; ++k) {
        const float outer = reach;
        const float yaw_a_sin = sin_vu(sweep);
        reach = fpu_sub(reach, reach_step);
        const float yaw_a = angle_add(lwc1(pv + 0x84), fpu_mul(fpu_mul(lwc1(pv + 0x64), kDegree), yaw_a_sin));
        sweep = angle_add(sweep, step);
        const float yaw_b = yaw_at(sweep);
        swc1(a, fpu_mul(cos_vu(yaw_a), outer));
        swc1(a + 4, fpu_mul(sin_vu(yaw_a), outer));
        sw(a + 8, 0);
        swc1(b, fpu_mul(cos_vu(yaw_b), reach));
        swc1(b + 4, fpu_mul(sin_vu(yaw_b), reach));
        sw(b + 8, 0);
        vec3_add(V(a), V(a), V(m + 0x10));
        vec3_add(V(b), V(b), V(m + 0x10));
        segment_collide_v(V(a), V(b), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
    }
    ee_stack_free(0x110);
}

// The bursting jet's flame tested against what it touches: two lines along the jet's yaw +0x48 from points 1/8 to
// either side of the nozzle (along its side axis +0xd0) and 1/8 up, as long as the flame has flown since the burst began
// (the burst's length +0x70 in seconds less the frames left +0x88, at the speed 0x161cec, the reach 0x161cf0 at most).
// orig: L6:0x002f4390
void flame_jet_burst_hits_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t rec = sp, a = sp + 0x30, b = sp + 0x40;
    const uint32_t pv = lwu(m + 0x78);
    flame_hit_record(rec, m);
    const float flown = fpu_sub(fpu_mul(lwc1(pv + 0x70), 60.0f), static_cast<float>(lw(pv + 0x88)));
    float len = fpu_mul(flown, fpu_mul(lwc1(L6_ADDR(0x00161cec)), dt()));
    if (lwc1(L6_ADDR(0x00161cf0)) < len) len = lwc1(L6_ADDR(0x00161cf0));
    const float eighth = 0.125f;
    for (const float side : {eighth, -eighth}) {
        vec3_set_length(V(a), V(m + 0xd0), side);
        vec3_add(V(a), V(a), V(m + 0x10));
        swc1(a + 8, fpu_add(lwc1(a + 8), eighth));
        swc1(b, fpu_mul(cos_vu(lwc1(m + 0x48)), len));
        swc1(b + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), len));
        sw(b + 8, 0);
        vec3_add(V(b), V(b), V(a));
        segment_collide_v(V(a), V(b), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
    }
    ee_stack_free(0xc0);
}

// Class 911, a flame jet. While the hero is in its box (pvar +0x90, none when negative) the pvars +0x50 / +0x54 hold the
// level's values 0x161d24 / 0x161d28, else 0. A jet with a sweep (+0x60 set) sweeps for good (state 1): its yaw swings
// about the one it started at (+0x84) by the amplitude +0x64 degrees, the sweep's angle +0x7c (starting at +0x6c degrees)
// turning a full circle every +0x68 seconds. Without one it bursts: on (state 2) for +0x70 seconds, off (state 3) for +0x74
// (the first wait: +0x78 seconds and +0x74 frames), its timer +0x88. While on it roars (its sound slot +0x94), sprays its
// particles and burns what its flame meets.
// orig: L6:0x002f3ad8
void blg_class911_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t box = lw(pv + 0x90);
    if (box >= 0) {
        if (point_in_box(HERO_POS, box) != 0) {
            swc1(pv + 0x50, lwc1(L6_ADDR(0x00161d24)));
            swc1(pv + 0x54, lwc1(L6_ADDR(0x00161d28)));
        } else {
            sw(pv + 0x50, 0);
            sw(pv + 0x54, 0);
        }
    }
    auto seconds = [](float frames60) { return frame_scale_i(float_to_int(frames60)); };  // (a count of 60 Hz frames)
    switch (lbu(m + 0x20)) {
    case 0:
        if (lw(pv + 0x60) != 0) {
            sb(m + 0x20, 1);
            const float turn = fpu_div(360.0f, lwc1(pv + 0x68));
            swc1(pv + 0x7c, fpu_mul(lwc1(pv + 0x6c), kDegree));
            swc1(pv + 0x80, fpu_mul(fpu_mul(turn, kDegree), dt()));
            swc1(pv + 0x84, lwc1(m + 0x48));
        } else {
            sb(m + 0x20, 3);
            sw(pv + 0x88, static_cast<uint32_t>(seconds(fpu_add(lwc1(pv + 0x74), fpu_mul(lwc1(pv + 0x78), 60.0f)))));
        }
        sw(pv + 0x94, 0xffffffffu);
        break;
    case 1: {
        flame_sound_keep(m, pv);
        flame_jet_particles_l6(m);
        flame_jet_sweep_hits_l6(m);
        const float swing = fpu_mul(fpu_mul(lwc1(pv + 0x64), kDegree), sin_vu(lwc1(pv + 0x7c)));
        swc1(pv + 0x7c, angle_add(lwc1(pv + 0x7c), lwc1(pv + 0x80)));
        swc1(m + 0x48, angle_add(lwc1(pv + 0x84), swing));
        break;
    }
    case 2:
        flame_sound_keep(m, pv);
        flame_jet_particles_l6(m);
        flame_jet_burst_hits_l6(m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88))) != 0) {
            sb(m + 0x20, 3);
            sw(pv + 0x88, static_cast<uint32_t>(seconds(fpu_mul(lwc1(pv + 0x74), 60.0f))));
            blarg_sound_stop(m, lw(pv + 0x94));
            sw(pv + 0x94, 0xffffffffu);
        }
        break;
    case 3:
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88))) != 0) {
            sb(m + 0x20, 2);
            sw(pv + 0x88, static_cast<uint32_t>(seconds(fpu_mul(lwc1(pv + 0x70), 60.0f))));
        }
        break;
    default:
        break;
    }
}

}  // namespace rac1
