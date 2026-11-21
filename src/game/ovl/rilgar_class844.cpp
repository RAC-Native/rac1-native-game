// Rilgar's class 844: a float that rests at its home point swaying on the water, leaps out when the hero comes near,
// hangs over the water bobbing, sinks back and starts again from home. It keeps a rising bubble column under it while
// it leaps and sinks (ril_30f218_l5 / ril_29bbe0_l5), a splash ring as it leaves the water (particles 0x30), pushes the
// hero away when he is under it, carries what stands on it (attach_frame_set) and drags a companion moby of class 0x388
// along. Hand-ported from Rilgar's overlay (`orig: L5:`); declared in rilgar_mobys.h, registered in rilgar_mobys.cpp.
// Its tuning words are the level's (L5_ADDR 0x161dac .. 0x161e00); the base ELF's (the hero 0x13f350, the frame time
// 0x15ed6c and its square 0x15ed70, the sound slots 0x13e550) are bare.
//
// pvars: +0x20 the attachment frame (attach_frame_set), +0x60 the velocity, +0x68 the vertical speed, +0x70 the home
// point, +0x78 the water's height, +0x7c the height it bobs at above the water, +0x80 the leap's height, +0x84 the bob's
// centre, +0x88 the time it hangs (frames, a float), +0x8c a timer, +0x90 / +0x94 the sway's yaw speeds, +0x98 / +0x9c the
// sway's phases and +0xa0 / +0xa4 their steps, +0xa8 the sound slot, +0xac the companion moby.
// States: 0 set up, 1 at home (swaying), 2 leaping, 3 hanging, 4 sinking.
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/rilgar_own.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f3d0;  // the hero's position (the hero's block 0x13f350 + 0x80)
constexpr float DEG = 0.01745329238474369f;  // (0x3c8efa35)

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time_sq() { return lwc1(0x0015ed70); }

// a timer word ticked (timer_tick_i32 on the word, written back): 0 while it runs
int32_t tick(uint32_t a) {
    int32_t t = lw(a);
    const int32_t r = timer_tick_i32(t);
    sw(a, static_cast<uint32_t>(t));
    return r;
}

// the level's frame count word 0x161db8 frame-scaled, as a float
float scaled_frames() { return static_cast<float>(frame_scale_i(lw(L5_ADDR(0x00161db8)))); }

// The sway's random phases and steps (each step 70 .. 120 degrees a second, either way).
void sway_start(uint32_t pv) {
    swc1(pv + 0x98, rand_angle());
    swc1(pv + 0x9c, rand_angle());
    swc1(pv + 0xa0, fpu_mul(fpu_mul(rand_range_signed_f(70.0f, 120.0f), DEG), frame_time()));
    swc1(pv + 0xa4, fpu_mul(fpu_mul(rand_range_signed_f(70.0f, 120.0f), DEG), frame_time()));
}

}  // namespace

// A bubble: a particle of kind 0x30 at `pos` moving by `vel`, living `life` frames, of colour `colour` (its low 24 bits)
// and size `size`, with the spin `spin`. (Particle record: +4 / +0x34 the colour, +8 a random phase, +9 its texture's row,
// +0xa / +0x3c the life and the spin, +0xc the size, +0x10 the point, +0x20 the velocity, +0x30 the fade step.)
// orig: L5:0x0029bbe0
uint32_t ril_29bbe0_l5(uint32_t pos, uint32_t vel, int32_t life, int32_t spin, uint32_t colour, float size) {
    const uint32_t p = alloc_particle(0x30);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    const uint32_t rgb = colour & 0xffffffu;
    sw(p + 4, rgb);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x10));
    sb(p + 3, 0x44);
    sb(p + 1, 0);
    swc1(p + 0xc, fpu_mul(size, 210000.0f));
    sb(p + 2, lbu(lwu(L5_ADDR(0x001b25c0))));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0x100)));
    V(p + 0x20) = V(vel);
    swc1(p + 0x30, fpu_div(2.0f, int_to_float(life)));
    sh(p + 0xa, static_cast<uint32_t>(life));
    sw(p + 0x38, colour);
    sh(p + 0x3c, static_cast<uint32_t>(spin));
    sw(p + 0x34, rgb);
    return p;
}

// The bubble column under the float, between the heights `lo` and `hi`: one bubble per height step (0x161db4) at a random
// point of the disc of radius 0x161dd0 around it, rising (0x161dc8 a frame), lower by the rise of three quarters of its life.
// orig: L5:0x0030f218
void ril_30f218_l5(uint32_t m, float lo, float hi) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    vec_clear(V(sp));
    swc1(sp + 8, fpu_mul(lwc1(L5_ADDR(0x00161dc8)), frame_time()));
    const float life = int_to_float(lw(L5_ADDR(0x00161db8)));
    const float span = fpu_sub(hi, lo);
    const float rise = fpu_mul(fpu_mul(lwc1(sp + 8), 0.75f), life);
    const uint32_t at = sp + 0x10;
    for (int32_t n = 0; static_cast<float>(n) < fpu_div(span, lwc1(L5_ADDR(0x00161db4)));) {
        const float r = rand_range_f(0.0f, lwc1(L5_ADDR(0x00161dd0)));
        const float a = rand_angle();
        swc1(at, fpu_mul(cos_vu(a), r));
        swc1(at + 4, fpu_mul(sin_vu(a), r));
        swc1(at + 8, 0.0f);
        vec3_add(V(at), V(m + 0x10), V(at));
        swc1(at + 8, fpu_sub(rand_range_f(lo, hi), rise));
        const float z = fpu_sub(rand_range_f(fpu_mul(fpu_add(lo, hi), 0.5f), hi), rise);
        if (z < lwc1(at + 8)) swc1(at + 8, z);
        ++n;
        const float size = rand_range_f(lwc1(L5_ADDR(0x00161dbc)), lwc1(L5_ADDR(0x00161dc0)));
        const int32_t frames = frame_scale_i(lw(L5_ADDR(0x00161db8)));
        const float spin_max = static_cast<float>(lw(L5_ADDR(0x00161dc4)));
        const int32_t spin = float_to_int(rand_range_signed_f(0.0f, spin_max));
        ril_29bbe0_l5(at, sp, frames, spin, lwu(L5_ADDR(0x00161dcc)), size);
    }
    ee_stack_free(0xb0);
}

// The sway: the float's yaw and pitch (+0x40 / +0x44) seek a wobble of two sines (scaled by `k`), plus, while the hero
// stands on it (on the ground), his offset along its side / forward rows times `k`; `s` scales the seek's rates.
// orig: L5:0x0030f408
void ril_30f408_l5(uint32_t m, float k, float s) {
    const uint32_t pv = lwu(m + 0x78);
    float roll = 0.0f, pitch = 0.0f;
    if (lwu(0x0013f64c) == m && lh(0x0013f65e) == 0) {
        Vec4 d{};
        vec3_sub(d, V(HERO), V(m + 0x10));
        pitch = fpu_mul(k, vec3_dot(d, V(m + 0xc0)));
        roll = fpu_mul(-k, vec3_dot(d, V(m + 0xd0)));
    }
    swc1(pv + 0x98, angle_add(lwc1(pv + 0x98), lwc1(pv + 0xa0)));
    swc1(pv + 0x9c, angle_add(lwc1(pv + 0x9c), lwc1(pv + 0xa4)));
    roll = angle_add(roll, fpu_mul(fpu_mul(sin_vu(lwc1(pv + 0x98)), k), lwc1(L5_ADDR(0x00161dd8))));
    pitch = angle_add(pitch, fpu_mul(fpu_mul(sin_vu(lwc1(pv + 0x9c)), k), lwc1(L5_ADDR(0x00161dd8))));
    for (int axis = 0; axis < 2; ++axis) {
        const float accel = fpu_mul(fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161ddc)), s), DEG), frame_time_sq());
        const float decel = fpu_mul(fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161de0)), s), DEG), frame_time_sq());
        const float vmax = fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161de4)), DEG), frame_time());
        yaw_seek(m + 0x40 + static_cast<uint32_t>(axis) * 4, pv + 0x90 + static_cast<uint32_t>(axis) * 4, axis == 0 ? roll : pitch,
                 accel, decel, vmax);
    }
}

namespace {
// the splash ring as the float leaves the water: 20 particles 0x30 in a ring 0.75 out, at its height
void splash(uint32_t m, uint32_t sp) {
    const uint32_t c0 = lwu(L5_ADDR(0x00161df4)), c1 = lwu(L5_ADDR(0x00161df8));
    const uint32_t n = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(10.0f, 20.0f))));
    const uint32_t h32 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(10.0f, 20.0f))));
    const uint32_t h34 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(10.0f, 40.0f))));
    const uint32_t vel = sp + 0x40, spread = sp + 0x50, at = sp + 0x60;
    for (int i = 0; i < 20; ++i) {
        const float a = rand_angle();
        swc1(at, fpu_mul(cos_vu(a), 0.75f));
        swc1(at + 4, fpu_mul(sin_vu(a), 0.75f));
        sw(at + 8, 0);
        vec3_set_length(V(vel), V(at), fpu_mul(rand_range_f(lwc1(L5_ADDR(0x00161dec)), lwc1(L5_ADDR(0x00161df0))), frame_time()));
        swc1(vel + 8, fpu_mul(rand_range_f(0.0f, lwc1(L5_ADDR(0x00161dfc))), frame_time()));
        vec2_set_length(V(spread), V(vel), 0.0f);
        vec3_add(V(at), V(m + 0x10), V(at));
        const float z = lwc1(m + 0x18);
        swc1(at + 8, rand_range_f(fpu_sub(z, 0.25f), fpu_add(z, 0.5f)));
        const float f = rand_range_f(0.5f, 1.0f);
        swc1(vel + 0xc, f);
        swc1(spread + 0xc, f);
        spawn_particle_02(at, vel, spread, c0, c1, n, h32, h34, 0x30);
    }
}
}  // namespace

// orig: L5:0x0030e7f8
void ril_class844_update_l5(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t pv = lwu(m + 0x78);
    V(sp + 0x10) = V(m + 0x10);  // (where it was and how it was turned: what it carries moves by the difference)
    V(sp + 0x20) = V(m + 0x40);

    // its sound: stopped (and forgotten) at home, started again while away
    if (lbu(m + 0x20) < 2) {
        if (sub_0028d8c0(m, lw(pv + 0xa8)) != 0) {
            const int32_t slot = lw(pv + 0xa8);
            if (slot != -1) {
                const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
            }
            sw(pv + 0xa8, 0xffffffffu);
        }
    } else if (sub_0028d8c0(m, lw(pv + 0xa8)) == 0) {
        sw(pv + 0xa8, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }

    switch (lbu(m + 0x20)) {  // (the jump table L5:0x2187a0)
    case 0: {  // (0x0030e928)
        sb(m + 0x20, 1);
        sw(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(0x78)));
        sw(pv + 0xa8, 0xff);
        V(pv + 0x70) = V(m + 0x10);
        sway_start(pv);
        const float top = fpu_add(lwc1(pv + 0x84), 1.0f);
        if (lwc1(pv + 0x80) <= top) swc1(pv + 0x80, top);
        break;
    }
    case 1: {  // (0x0030e9fc) at home: leaps once the hero has been within 24 for the timer
        ril_30f408_l5(m, 0.0f, 8.0f);
        if (lw(L5_ADDR(0x0015f5c4)) == 0 && vec2_distance(V(m + 0x10), V(HERO)) < 24.0f && tick(pv + 0x8c) != 0) {
            sb(m + 0x20, 2);
            vec_clear(V(pv + 0x60));
            const float g = fpu_mul(lwc1(L5_ADDR(0x00161dac)), frame_time_sq());
            swc1(pv + 0x68, sqrt_f(fpu_mul(fpu_add(lwc1(pv + 0x80), lwc1(pv + 0x80)), g)));
            break;
        }
        // the last half of the wait: the bubbles come up; the last fifth: the splash ring too
        if (static_cast<float>(lw(pv + 0x8c)) < fpu_mul(scaled_frames(), 0.5f)) {
            for (int k = 0; k < 2; ++k) {
                const float water = lwc1(pv + 0x78);
                ril_30f218_l5(m, water, fpu_add(lwc1(L5_ADDR(0x00161de8)), water));
            }
            if (static_cast<float>(lw(pv + 0x8c)) < fpu_mul(scaled_frames(), 0.2f)) splash(m, sp);
        }
        break;
    }
    case 2: {  // (0x0030ecf0) leaping: touches what it meets every fourth frame, rises until its speed is spent
        if ((lwu(L5_ADDR(0x0015f5cc)) & 3) == 0) moby_sphere_contact(m, m + 0x10, 0x10000, 0, 1, 0, 1.0f, 2.0f, 1.0f);
        const float frames = scaled_frames();
        ril_30f218_l5(m, lwc1(pv + 0x78), fpu_add(fpu_mul(fpu_mul(lwc1(pv + 0x68), frames), 0.5f), lwc1(m + 0x18)));
        ril_30f408_l5(m, lwc1(L5_ADDR(0x00161dd4)), 1.0f);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x60));
        const float speed = fpu_sub(lwc1(pv + 0x68), fpu_mul(lwc1(L5_ADDR(0x00161dac)), frame_time_sq()));
        swc1(pv + 0x68, speed);
        if (speed <= 0.0f) {
            sb(m + 0x20, 3);
            swc1(pv + 0x7c, fpu_sub(lwc1(pv + 0x84), 0.5f));
            sw(pv + 0x8c, static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(pv + 0x88)))));
        }
        break;
    }
    case 3: {  // (0x0030edfc) hanging: bobs between the centre's +-0.5 above the water until the timer runs out
        ril_30f218_l5(m, lwc1(pv + 0x78), lwc1(m + 0x18));
        ril_30f408_l5(m, lwc1(L5_ADDR(0x00161dd4)), 1.0f);
        const float accel = fpu_mul(lwc1(L5_ADDR(0x00161db0)), frame_time_sq());
        seek_1d(m + 0x18, pv + 0x68, fpu_add(lwc1(pv + 0x78), lwc1(pv + 0x7c)), accel, accel, fpu_mul(frame_time(), 20.0f));
        if (lwc1(pv + 0x68) == 0.0f) {
            const float centre = lwc1(pv + 0x84);
            swc1(pv + 0x7c, centre < lwc1(pv + 0x7c) ? fpu_sub(centre, 0.5f) : fpu_add(centre, 0.5f));
        }
        if (tick(pv + 0x8c) != 0) sb(m + 0x20, 4);
        break;
    }
    case 4: {  // (0x0030eee4) sinking: back home (its sound, the sway's new phases) once under the water
        const float frames = scaled_frames();
        ril_30f218_l5(m, lwc1(pv + 0x78), fpu_add(fpu_mul(lwc1(pv + 0x68), frames), lwc1(m + 0x18)));
        const bool above = fpu_add(lwc1(pv + 0x78), 1.0f) < lwc1(m + 0x18);
        ril_30f408_l5(m, above ? lwc1(L5_ADDR(0x00161dd4)) : 0.0f, above ? 1.0f : 10.0f);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x60));
        swc1(pv + 0x68, fpu_sub(lwc1(pv + 0x68), fpu_mul(lwc1(L5_ADDR(0x00161dac)), frame_time_sq())));
        if (lwc1(m + 0x18) < lwc1(pv + 0x78)) {
            moby_sound_start(1, 0, m);
            sb(m + 0x20, 1);
            sw(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(0x78)));
            V(m + 0x10) = V(pv + 0x70);
            sway_start(pv);
        }
        break;
    }
    default:
        break;
    }

    // the hero under it (lower by 2) within the reach 0x161e00: pushed away from it
    if (lwc1(HERO + 8) < fpu_sub(lwc1(m + 0x18), 2.0f) && vec2_distance(V(m + 0x10), V(HERO)) < lwc1(L5_ADDR(0x00161e00))) {
        const uint32_t dir = sp + 0x40, at = sp + 0x50;
        swc1(dir, fpu_mul(cos_vu(atan2_f(fpu_sub(lwc1(HERO), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 4), lwc1(m + 0x14)))), 0.2f));
        swc1(dir + 4, fpu_mul(sin_vu(atan2_f(fpu_sub(lwc1(HERO), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 4), lwc1(m + 0x14)))), 0.2f));
        sw(dir + 8, 0);
        V(at) = V(HERO);
        swc1(at + 8, fpu_add(lwc1(at + 8), 0.75f));
        sub_00259bc8(lwu(0x001413d0), m, 1, at, dir, lwc1(dir + 8));
    }

    // what stands on it carried, the companion moby (class 0x388, spawned once) kept at its point
    vec3_sub(V(sp + 0x30), V(m + 0x10), V(sp + 0x10));
    attach_frame_set(pv + 0x20, sp + 0x30, sp + 0x20, m + 0x40);
    uint32_t mate = lwu(pv + 0xac);
    if (mate == 0) {
        mate = moby_spawn(0x388);
        sw(pv + 0xac, mate);
        if (mate != 0) {
            sb(mate + 0x30, 0);
            sh(mate + 0x34, lhu(mate + 0x34) | 1u);
            sh(mate + 0x32, 0);
            sb(mate + 0x31, 0);
            V(mate + 0x10) = V(m + 0x10);
            moby_update_transform(mate);
        }
    } else {
        V(mate + 0x10) = V(m + 0x10);
        moby_update_transform(mate);
    }
    ee_stack_free(0x160);
}

}  // namespace rac1
