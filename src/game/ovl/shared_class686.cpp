// Class 686's update: one port of the copies Novalis and Oltanis carry (the same code relinked; the levels' copies of the
// smoke's particle 4 are Veldin's spawn_particle_4, the same code). Each level keeps its own two colour tables (per level
// below); the rest is the engine's (OV: the low-detail flag 0x15f404, the camera 0x166dc0, the collision's moby 0x173e58,
// the screen's fade 0x15f5d0, the moby list 0x15ffd8) or the base ELF's (bare: the hero 0x13f3d0, his moby 0x1413d0, the
// frame time 0x15ed6c). Header shared_class686.h.
//
// The pvars: +0 the velocity, +0x10 the target point, +0x20 four mobys to wake (indices, -1 none), +0x38 the reach at
// which it arrives.
#include <game/ovl/shared_class686.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the level's two tables of six colours the blast's sparks and smoke take theirs from at random
uint32_t colours_a() { return on_level(OLTANIS) ? L14_ADDR(0x001fcb50) : L1_ADDR(0x0020b470); }
uint32_t colours_b() { return on_level(OLTANIS) ? L14_ADDR(0x001fcb68) : L1_ADDR(0x0020b488); }

bool low_detail() { return lw(OV(0x0015f404)) != 0; }

// a random direction in a cube (each axis -1 .. 1) at `out`, its w 0
void rand_cube(uint32_t out) {
    V(out) = Vec4{};
    swc1(out, rand_range_f(-1.0f, 1.0f));
    swc1(out + 4, rand_range_f(-1.0f, 1.0f));
    swc1(out + 8, rand_range_f(-1.0f, 1.0f));
}

// a count of frames from lo .. hi (both scaled), divided by the detail divisor
int32_t frames(int32_t lo, int32_t hi, int32_t divisor) {
    const int32_t a = frame_scale_i(lo);
    return ee_div(rand_range(a, frame_scale_i(hi)), divisor);
}
}  // namespace

// Each frame it moves by its velocity and smokes (four puffs, two in low detail: a flame and a smoke trail from points along
// its last step, drifting back); out of the level's box (below 0 on an axis) it is gone; within its reach of the target it
// is put there and blows up: (in low detail, the hero's moby within 1 picks the blast's sound) sparks and smoke by the
// camera's distance, shards (more out of a dark screen, far from the camera), a shower of embers, then the mobys of class
// 700 it names, resting (their +4 and timer 0) with the hero and the camera in their box, wake in 1 .. 3 s; and it goes.
// orig: L1:0x002f6a30 L14:0x002ef840
void class686_update(uint32_t m) {
    const int32_t divisor = low_detail() ? 4 : 2;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(0x170);
    const uint32_t at = sp + 0x10, vel = sp + 0x20, rec = sp + 0x40, push = sp + 0x60;
    lq_sq(sp, pos);
    vec3_add(V(pos), V(pos), V(pv));
    const float speed = vec3_length(V(pv));
    for (int32_t i = 0; i < (low_detail() ? 2 : 4); ++i) {
        rand_cube(sp + 0x30);
        lq_sq(vel, sp + 0x30);
        const float drift = rand_range_f(0.5f, 1.0f);
        vec3_set_length(V(vel), V(vel), fpu_mul(drift, lwc1(0x0015ed6c)));
        const float ft = lwc1(0x0015ed6c);
        vec3_set_length(V(at), V(pv), -rand_range_f(fpu_mul(ft, 0.1f), ft));
        vec3_add(V(vel), V(vel), V(at));
        vec3_set_length(V(at), V(pv), fpu_mul(rand_range_f(0.0f, 1.0f), speed));
        vec3_add(V(at), V(at), V(pos));
        {
            const int32_t a = ee_div(frame_scale_i(rand_range(0x1e, 0x2d)), divisor);
            const int32_t b = ee_div(rand_range(0x50, 0x8c), divisor);
            spawn_particle_4(at, vel, 0x6f00afff, 0xff, static_cast<uint32_t>(a), 0x50, static_cast<uint32_t>(b), 1);
        }
        vec3_set_length(V(at), V(pv), fpu_mul(rand_range_f(0.0f, 1.0f), speed));
        vec3_add(V(at), V(at), V(pos));
        {
            const int32_t a = ee_div(frame_scale_i(rand_range(0x3c, 0x78)), divisor);
            const int32_t b = ee_div(rand_range(0xc8, 0x12c), divisor);
            spawn_particle_4(at, vel, 0x1fffffff, 0x4f4f4f, static_cast<uint32_t>(a), 0x50, static_cast<uint32_t>(b), 0);
        }
    }
    if (lwc1(pos) < 0.0f || lwc1(pos + 4) < 0.0f || lwc1(pos + 8) < 0.0f) {
        moby_retire(m);
        ee_stack_free(0x170);
        return;
    }
    if (vec3_distance(V(pv + 0x10), V(pos)) < lwc1(pv + 0x38)) {
        lq_sq(pos, pv + 0x10);
        sb(m + 0x20, 1);
    }
    if (lbu(m + 0x20) == 0) {
        ee_stack_free(0x170);
        return;
    }
    // the blast
    const float cam_far = vec3_distance(V(OV(0x00166dc0)), V(pos));
    const float scale = low_detail() ? 4.0f : 8.0f;
    vec3_scale(V(pv), V(pv), u2f(0xbf0ccccdu));  // (-0.55)
    if (low_detail()) {
        sw(rec + 0x14, 0x810001);
        sw(rec + 0x10, m);
        swc1(rec + 0x1c, 0.0f);
        vec_clear(V(rec));
        sw(rec + 0x20, 0);
        mobys_in_sphere(pos, 1.0f, 9, lwu(0x001413d0), rec);
        const uint32_t hit = lwu(OV(0x00173e58));
        const int32_t cls = hit != 0 ? lh(hit + 0xa6) : 0;
        moby_sound_start(hit != 0 && cls >= 709 && cls < 712 ? 2 : 1, 0, m);
    }
    const float cam_near = vec3_distance(V(pos), V(OV(0x00166dc0)));
    int32_t bursts = low_detail() ? 2 : 5;
    if (cam_near < fpu_add(int_to_float(bursts), int_to_float(bursts))) bursts = float_to_int(cam_near) / 2;
    float close = 0.0f;  // (how much nearer than 7 the camera is)
    if (cam_near < 7.0f) close = fpu_sub(7.0f, cam_near);
    const float big = fpu_add(scale, scale);
    const uint32_t ta = colours_a(), tb = colours_b();
    for (int32_t i = bursts; i > 0; --i) {
        const float r = rand_range_f(8.0f, 10.0f);
        const float ft = lwc1(0x0015ed6c);
        float spread = fpu_sub(fpu_mul(r, ft), fpu_mul(close, ft));
        const uint32_t ca = lwu(ta + static_cast<uint32_t>(rand_mod(6)) * 4);
        spread = fpu_mul(spread, scale);
        const uint32_t cb = lwu(tb + static_cast<uint32_t>(rand_mod(6)) * 4);
        const float size = fpu_mul(scale, 400000.0f);
        const int32_t n1 = frames(0x1e, 0x28, divisor);
        const int32_t l1 = frames(0x3c, 0x5a, divisor);
        spawn_particle_0b(pos, pv, ca, cb, static_cast<uint32_t>(n1), static_cast<uint32_t>(l1), 0, 8, size, spread);
        const int32_t n2 = frames(0xa, 0x14, divisor);
        const int32_t l2 = frames(0x1e, 0x28, divisor);
        spawn_particle_0b(pos, pv, 0x7fffffff, 0x00ffffff, static_cast<uint32_t>(n2), static_cast<uint32_t>(l2), 0, 8, size, fpu_mul(spread, 0.5f));
    }
    for (int32_t i = 0; i < 5; ++i) {
        rand_cube(sp + 0x70);
        lq_sq(push, sp + 0x70);
        const float r = rand_range_f(0.0f, 3.0f);
        vec3_set_length(V(push), V(push), fpu_mul(scale, fpu_mul(r, lwc1(0x0015ed6c))));
        const uint32_t ca = lwu(ta + static_cast<uint32_t>(rand_mod(6)) * 4);
        const uint32_t cb = lwu(tb + static_cast<uint32_t>(rand_mod(6)) * 4);
        spawn_particle_08(pos, push, ca, cb, frames(0xf, 0x14, divisor), 200000.0f);
    }
    auto shard = [m, pos, pv](int32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t group, float size) {
        shard_spawn(m, pos, pv, static_cast<uint32_t>(frame_scale_i(life)), r, g, b, group, size);
    };
    const bool dark = lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && 9.0f < cam_near;  // (0.95)
    if (dark) {
        shard(0xf, 0x7f, 0x7f, 0x7f, 0x20, fpu_mul(scale, 4.0f));
        shard(0x18, 0x7f, 0x7f, 0x7f, 0x20, fpu_mul(scale, 4.0f));
    }
    shard(0x14, 0x7f, 0x7f, 0, 0x30, fpu_mul(scale, 4.0f));
    shard(0x13, 0xff, 0xff, 0xff, 0x20, big);
    if (100.0f < cam_far) {
        if (lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && 9.0f < cam_near) {
            shard(0xf, 0x7f, 0x7f, 0x7f, 0x20, 30.0f);
            shard(0x18, 0x7f, 0x7f, 0x7f, 0x20, 30.0f);
        }
        shard(0x14, 0x7f, 0x7f, 0, 0x30, 30.0f);
        shard(0x13, 0xff, 0xff, 0xff, 0x20, 15.0f);
    }
    for (int32_t i = 0; i < (low_detail() ? 10 : 40); ++i) {
        rand_cube(sp + 0x30);
        lq_sq(vel, sp + 0x30);
        float len = fpu_mul(rand_range_f(10.0f, 40.0f), lwc1(0x0015ed6c));
        if (low_detail()) len = fpu_mul(len, u2f(0x3e4ccccdu));  // (0.2)
        vec3_set_length(V(vel), V(vel), len);
        vec3_add(V(vel), V(vel), V(pv));
        const float size = low_detail() ? 21000.0f : 105000.0f;
        spawn_particle_0f(pos, vel, 0x4f007fff, 0x1f00007f, static_cast<uint32_t>(frames(0x5a, 0x96, divisor)), 1, -1, -1, size);
    }
    // (the original divides by 200 the camera's distance short of 200 here and keeps nothing of it)
    for (uint32_t k = 0; k < 4; ++k) {
        if (low_detail()) continue;
        const int32_t idx = lw(pv + 0x20 + k * 4);
        if (idx == -1) continue;
        const uint32_t other = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
        if (lh(other + 0xa6) != 0x2bc) continue;
        const uint32_t opv = lwu(other + 0x78);
        if (lh(opv + 4) != 0 || lw(opv) != 0) continue;
        if (point_in_box(0x0013f3d0, lw(opv + 8)) == 0) continue;
        if (point_in_box(OV(0x00166dc0), lw(opv + 8)) == 0) continue;
        sw(opv, static_cast<uint32_t>(frame_scale_i(rand_range(0x3c, 0xb4))));
    }
    moby_retire(m);
    ee_stack_free(0x170);
}

}  // namespace rac1
