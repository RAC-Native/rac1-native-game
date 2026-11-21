// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1035, an electric barrier in Clank's part of the
// station (four bolts of lightning across a gap, shocking whoever crosses; it shuts down when the Gadgebots have opened
// its four switches, class 1302), and its bolts' animation. Header blarg_own.h; registered in blarg_mobys.cpp (its draw
// callback, 0x2f6dd0, is blarg_part2's). Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the bolts'
// points at 0x1db0f0, their drift at 0x1db870, the barrier's tuning at 0x161dfc .. 0x161e54), the base ELF's are bare.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 its position, +0x23c the moby it touches, +0x2080 its moby)
constexpr uint32_t HELP = 0x00141968;    // the help messages' records (8 bytes each: count, longest time in 600ths, levels)
constexpr uint32_t HELP_GADGEBOTS = HELP + 0x2b * 8;  // help message 0x2b's
const float kDegree = u2f(0x3c8efa35u);  // pi / 180
const float kHalfPi = u2f(0x3fc90fdbu);

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

constexpr uint32_t BOLT_POINTS = 30;  // points of a bolt (16 bytes each, 0x1e0 a bolt)

// VU0's vf0, (0, 0, 0, 1), stored at `a` (the level's 0x217300)
void store_vf0(uint32_t a) {
    sw(a, 0);
    sw(a + 4, 0);
    sw(a + 8, 0);
    swc1(a + 0xc, 1.0f);
}

// The hero shocked: his count of shocks (pvar +0x14) up and the shock's grace time (+0x16) started.
void count_shock(uint32_t pv) {
    sh(pv + 0x14, lhu(pv + 0x14) + 1u);
    sh(pv + 0x16, static_cast<uint32_t>(frame_scale_i(0x1e)));
}

// One side of the barrier tested against what crosses it: a line 3 to either side of the point 1/2 along `side` (the yaw
// +0x48, or its back) from the barrier's position (0x161e18 lower), with a shock's hit record (sub_00259888: 5.0, flags
// 0x10001) at `rec`.
void barrier_line(uint32_t m, uint32_t frame, float side) {
    const uint32_t a = frame, b = frame + 0x10, off = frame + 0x20, across = frame + 0x30, rec = frame + 0x40;
    swc1(off, fpu_mul(cos_vu(side), 0.5f));
    swc1(off + 4, fpu_mul(sin_vu(side), 0.5f));
    sw(off + 8, 0);
    swc1(across, fpu_mul(cos_vu(angle_add(lwc1(m + 0x48), kHalfPi)), 3.0f));
    swc1(across + 4, fpu_mul(sin_vu(angle_add(lwc1(m + 0x48), kHalfPi)), 3.0f));
    sw(across + 8, 0);
    lq_sq(a, m + 0x10);
    swc1(a + 8, fpu_sub(lwc1(a + 8), lwc1(L6_ADDR(0x00161e18))));
    lq_sq(b, a);
    vec3_add(V(a), V(a), V(off));
    vec3_add(V(b), V(b), V(off));
    vec3_add(V(a), V(a), V(across));
    vec3_sub(V(b), V(b), V(across));
    sub_00259888(5.0f, rec, static_cast<int32_t>(m), 0x10001, off);
    segment_collide_v(V(a), V(b), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
}

// Whether the shock counts: no help shown yet for it (help message 0x2b), no grace time running (pvar +0x16)
bool shock_counts(uint32_t pv) { return lhu(HELP_GADGEBOTS) == 0 && lh(pv + 0x16) == 0; }
}  // namespace

// The barrier's four bolts animated: the barrier spins about x (0x161e10 degrees a second) at the size 0x161e14 times its
// class's; the shared phase 0x161dfc runs on (0x161e00 a second, wrapped at 1). Bolt k's countdown (0x161e40 + 4k) at 0
// lays it out again: its 30 points straight along y over the length 0x161e08, each with a new random drift (within
// 0x161e04 a frame) smoothed over its neighbours into the drift table, and the countdown back at 0x161e50; else its 28
// inner points move by their drift and the countdown goes down.
// orig: L6:0x002f7000
void barrier_bolts_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x290);
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x00161e10)), kDegree), dt())));
    const float phase = fpu_add(lwc1(L6_ADDR(0x00161dfc)), fpu_mul(lwc1(L6_ADDR(0x00161e00)), dt()));
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x00161e14))));
    swc1(L6_ADDR(0x00161dfc), phase);
    if (1.0f < phase) swc1(L6_ADDR(0x00161dfc), fpu_sub(phase, 1.0f));
    const float step = fpu_div(lwc1(L6_ADDR(0x00161e08)), 30.0f);
    float y = -fpu_mul(lwc1(L6_ADDR(0x00161e08)), 0.5f);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t countdown = L6_ADDR(0x00161e40) + k * 4;
        const uint32_t points = L6_ADDR(0x001db0f0) + k * BOLT_POINTS * 0x10;
        const uint32_t drift = L6_ADDR(0x001db870) + k * BOLT_POINTS * 0x10;
        if (lw(countdown) == 0) {
            for (uint32_t j = 0; j < BOLT_POINTS; ++j) {
                store_vf0(points + j * 0x10);
                swc1(points + j * 0x10 + 4, y);
                y = fpu_add(y, step);
                rand_sphere_point(sp + j * 0x10, 0.0f, fpu_mul(lwc1(L6_ADDR(0x00161e04)), dt()));
            }
            for (uint32_t j = 1; j < BOLT_POINTS - 1; ++j) {  // each inner point's drift: the mean of its and its neighbours'
                const uint32_t d = drift + j * 0x10;
                vec3_add(V(d), V(sp + (j - 1) * 0x10), V(sp + j * 0x10));
                vec3_add(V(d), V(d), V(sp + (j + 1) * 0x10));
                vec3_scale(V(d), V(d), u2f(0x3eaa7efau));  // 0.333
            }
            sw(countdown, lwu(L6_ADDR(0x00161e50)));
        } else {
            for (uint32_t j = 1; j < BOLT_POINTS - 1; ++j)
                vec3_add(V(points + j * 0x10), V(points + j * 0x10), V(drift + j * 0x10));
            sw(countdown, lwu(countdown) - 1u);
        }
    }
    ee_stack_free(0x290);
}

// Class 1035, the electric barrier. State 0 sets the bolts' countdowns (0, 0xb, 0x17, 0x22; their period 0x2d) and goes
// to 1. State 1 hums (its sound slot pvar +0x10), animates its bolts and, near the camera (48), brightens (+0x18 up to 1,
// else down to 0; drawn while above 0) and shocks: lines on both sides of the gap hit whoever crosses (the hero's count of
// shocks +0x14, with a grace time +0x16 of 30 frames), the hero touching it is thrown back to the side he came from
// (moby_hit_record), a hit of the hero's weapons (mask 0x330000) counts too; after three, while help message 0x2b was
// never shown, the help voice line 0x1774 plays. When its four switches (the mobys whose indices pvars +0 .. +0xc
// hold: class 1302 opened, state 2; any other class done, +0xbc) are all done, it shuts down (state 2: its sound
// stopped, the help message's record noted when no switch was the Gadgebots' doing), bursts into nine sprays of 20
// sparks (particle 0x3c) along its frame (0x1db060 / 0x1dafd0) and is retired (state 3).
// orig: L6:0x002f6470
void blg_class1035_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x180);
    const uint32_t pos = m + 0x10;
    switch (lbu(m + 0x20)) {
    case 0:
        sw(L6_ADDR(0x00161e44), 0xb);
        sw(L6_ADDR(0x00161e48), 0x17);
        sw(L6_ADDR(0x00161e4c), 0x22);
        sw(L6_ADDR(0x00161e40), 0);
        sw(L6_ADDR(0x00161e54), 0x16);
        sw(L6_ADDR(0x00161e50), 0x2d);
        sw(pv + 0x10, 0xffffffffu);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x4000u);
        break;
    case 1: {
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x16)));
        if (sub_0028d8c0(m, lw(pv + 0x10)) == 0) sw(pv + 0x10, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        barrier_bolts_l6(m);
        if (vec3_distance(V(pos), V(L6_ADDR(0x00167540))) < 48.0f) {  // (the camera's position)
            approach_f(pv + 0x18, 1.0f, fpu_add(dt(), dt()));
            barrier_line(m, sp, lwc1(m + 0x48));
            if (shock_counts(pv) && coll_hit_moby() == lwu(PLAYER + 0x2080)) count_shock(pv);
            barrier_line(m, sp, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
            if (shock_counts(pv) && coll_hit_moby() == lwu(PLAYER + 0x2080)) count_shock(pv);
            if (lwu(PLAYER + 0x23c) == m) {  // the hero against it: thrown back to his side
                const uint32_t rel = sp + 0x70, push = sp + 0x40;
                vec3_sub(V(rel), V(PLAYER + 0x80), V(pos));
                vec3_set_length(V(push), V(m + 0xc0), 0.0f < vec3_dot(V(rel), V(m + 0xc0)) ? 1.0f : -1.0f);
                moby_hit_record(lwu(PLAYER + 0x2080), push);
                if (shock_counts(pv)) count_shock(pv);
            }
        } else {
            approach_f(pv + 0x18, 0.0f, fpu_add(dt(), dt()));
        }
        if (lwc1(pv + 0x18) != 0.0f) sub_001f93e0(L6_ADDR(0x002f6dd0), m);  // (its draw callback queued)
        if (lhu(HELP_GADGEBOTS) == 0) {
            if (lh(pv + 0x16) == 0 && sub_0025a420(m, NOT_ADDR(0x330000), 0) != 0) count_shock(pv);
            sb(m + 0xa4, 0xff);
            if (lh(pv + 0x14) >= 3) sub_00203908(0x1774, 0x2b);
        }
        // its switches
        const uint32_t first = lwu(L6_ADDR(0x0015ffd8));
        bool all_done = true, none_opened = true;
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + k * 4);
            if (idx < 0) continue;
            const uint32_t s = first + (static_cast<uint32_t>(idx) << 8);
            if (lh(s + 0xa6) != 0x516) {
                if (lbu(s + 0xbc) == 0) all_done = false;
            } else if (lbu(s + 0x20) == 2) {
                none_opened = false;
            } else {
                all_done = false;
            }
        }
        if (!all_done) break;
        sw(m + 0x94, 0);
        sb(m + 0x20, 2);
        blarg_sound_stop(m, lw(pv + 0x10));
        sw(pv + 0x10, 0xffffffffu);
        moby_sound_start(1, 0, m);
        if (none_opened && !(0xfffeu < lhu(HELP_GADGEBOTS))) sh(HELP_GADGEBOTS, lhu(HELP_GADGEBOTS) + 1u);
        // the help record's longest time (the game's clock 0x15eea4 in 600ths) and the level's bit
        const int32_t time = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
        if (static_cast<int32_t>(lhu(HELP_GADGEBOTS + 2)) < time) sh(HELP_GADGEBOTS + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
        sw(HELP_GADGEBOTS + 4, lwu(HELP_GADGEBOTS + 4) | (1u << (lwu(0x0015ed84) & 31u)) | 0x80000000u);
        break;
    }
    case 2: {
        const uint32_t frame = sp, part = sp + 0x40, at = sp + 0x70, local = sp + 0x80, spark = sp + 0x90, vel = sp + 0xa0;
        euler_to_mat4_vu(M(frame), V(m + 0x40));
        mat4_scale_diag(M(part), lwc1(L6_ADDR(0x00161e0c)));
        mat4_mul(M(frame), M(frame), M(part));
        for (uint32_t j = 0; j < 9; ++j) {
            euler_to_mat4_vu(M(part), V(L6_ADDR(0x001db060) + j * 0x10));
            mat4_mul(M(part), M(frame), M(part));
            mat4_transform(V(local), V(L6_ADDR(0x001dafd0) + j * 0x10), M(frame));
            vec3_add(V(at), V(local), V(pos));
            swc1(at + 0xc, 1.0f);
            for (int32_t n = 0; n < 20; ++n) {
                const float along = rand_range_signed_f(0.0f, fpu_mul(lwc1(L6_ADDR(0x00161e08)), 0.5f));
                vec3_set_length(V(spark), V(part + 0x10), along);  // (along the part's y axis)
                vec3_add(V(spark), V(spark), V(at));
                const uint32_t colour = rgba_lerp(lwu(L6_ADDR(0x00161e1c)), lwu(L6_ADDR(0x00161e20)), rand_range_f(0.0f, 1.0f));
                const float size = rand_range_f(lwc1(L6_ADDR(0x00161e24)), lwc1(L6_ADDR(0x00161e28)));
                const int32_t life = float_to_int(rand_range_f(static_cast<float>(lw(L6_ADDR(0x00161e2c))),
                                                               static_cast<float>(lw(L6_ADDR(0x00161e30)))));
                rand_sphere_point(vel, fpu_mul(lwc1(L6_ADDR(0x00161e34)), dt()), fpu_mul(lwc1(L6_ADDR(0x00161e38)), dt()));
                const uint32_t frames = static_cast<uint32_t>(frame_scale_i(life)) & 0xffu;
                const uint32_t first_frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
                spawn_particle_3c(spark, vel, colour, frames, first_frame, 0, size);
            }
        }
        sb(m + 0x20, 3);
        break;
    }
    case 3:
        moby_retire(m);
        break;
    default:
        break;
    }
    ee_stack_free(0x180);
}

}  // namespace rac1
