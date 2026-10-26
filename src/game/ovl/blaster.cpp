// Ported overlay code: the Blaster (item 0x0f, hand moby class 0xa8). Its hand moby's update
// 0x2b49a8 aims (the camera's line in first person, else the hero's facing with the auto-aim
// 0x2b46a8 on the targets list 0x15ffe4, the lock-on reticles through the camera effects
// 0x1edf60), fires every 6 frames while the button is held with ammo (the muzzle burn hit, the
// shot 0x2caf70, muzzle sparks, the stats), and keeps a muzzle light after each shot. The shot
// (class 0x131, update 0x2cb418) flies 40 units a second toward its target or its aim,
// steering by 0x258110, burns what it hits (a type-1 record), bounces with the upgrade 0x13e52f
// (5 + upgrade bounces) and leaves a trail of 16 glows (type 0x48) behind its head glow, sparks
// (type 0x1b) where it hits; 0x2cbcc0 ends it. The hand's pvars: +0 the fire timer, +4 the
// lock-on timer, +0x10 the light slot, +0x14 the light's frames.
#include <game/enhance/advanced.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
constexpr OvConst<0x00173e40u> HIT{};  // the collision's result: +0x18 the moby, +0x20 the point, +0x40 the normal
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }
int32_t& w32ref(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
// orig: (helper) repeated in 0x002b49a8 / 0x002b46a8 / 0x002caf70 — a moby's aim point: its
// position raised by its pvar block's +0x10 (0x25c338), or by 0.5 without one
void aim_point(uint32_t out, uint32_t m) {
    V(out) = V(m + 0x10);
    const uint32_t w = moby_pvar_word0(m);
    const float up = w != 0 ? lwc1(w + 0x10) : 0.5f;
    swc1(out + 8, lwc1(out + 8) + up);
}
// orig: (helper) repeated in 0x002caf70 / 0x002cb418 — five sparks (type 0x1b) off the hit:
// random directions bent by the reflection of `dir` on the hit normal 0x173e80, 3..6 speed
void hit_sparks(uint32_t pos, uint32_t dir, uint32_t f) {
    const uint32_t a = f, b = f + 0x10;  // the two scratch vectors (the original's +0x40 / +0x30 or +0x60 / +0x50)
    for (int32_t k = 4; k >= 0;) {
        V(a) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        swc1(a, rand_range_f(-1.0f, 1.0f));
        --k;
        swc1(a + 4, rand_range_f(-1.0f, 1.0f));
        swc1(a + 8, rand_range_f(-1.0f, 1.0f));
        V(b) = V(a);
        vec3_reflect(V(a), V(dir), V(OV(0x00173e80)));
        vec3_set_length(V(b), V(b), vec3_length(V(a)) * 0.5f);
        vec3_add(V(a), V(a), V(b));
        const float fs = lwc1(0x0015ed6c);
        vec3_set_length(V(a), V(a), rand_range_f(fs * 3.0f, fs * 6.0f));
        const int32_t lo = frame_scale_i(10);
        const int32_t n = rand_range(lo, frame_scale_i(0xf));
        spawn_particle_1b(pos, a, 0x7f2f4f6fu, static_cast<uint32_t>(n), u2f(0x46ea6000u));
    }
}
}  // namespace

// A spark (type 0x1b) at `pos` moving by `vel`: colour `color` (its alpha byte, signed, at
// +0x38), `n` frames (+0x30 / +0x34), size `size`, a random spin, the texture *0x1b20ec.
// orig: 0x0026dd20
uint32_t spawn_particle_1b(uint32_t pos, uint32_t vel, uint32_t color, uint32_t n, float size) {
    const uint32_t p = alloc_particle(0x1b);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    V(p + 0x10) = V(pos);
    const int32_t r = libc_rand();
    sw(p + 4, color);
    sb(p + 8, static_cast<uint32_t>(r));
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    swc1(p + 0xc, size);
    sb(p + 1, 0);
    const int32_t a = static_cast<int8_t>(lbu(p + 7));
    sb(p + 2, lbu(lwu(OV(0x001b20ec))));
    sw(q + 0x18, static_cast<uint32_t>(a));
    V(q) = V(vel);
    sw(q + 0x14, n);
    sw(q + 0x10, n);
    return p;
}

// A glow (type 0x1a) on the moby `m` (its joint `joint`, pulled 0.4 toward the camera; at its
// position with -1): colour `color` (alpha >> 24 at +0x2c), `h` frames (+0x24 / +0x28), size
// `size`, a random spin, the texture *0x1b20e8.
// orig: 0x0026daa0
uint32_t spawn_particle_1a(uint32_t m, uint32_t color, uint32_t h, uint32_t joint, float size) {
    const uint32_t p = alloc_particle(0x1a);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    const int32_t j = static_cast<int16_t>(joint);
    sw(p + 0x20, m);
    sh(q + 4, h);
    sw(q + 0xc, static_cast<uint32_t>(static_cast<int32_t>(color) >> 24));
    sh(q + 6, joint);
    sw(q + 8, h);
    if (j == -1) {
        V(p + 0x10) = V(m + 0x10);
    } else {
        const uint32_t f = ee_stack_alloc(0x20);  // the joint's point, its pull toward the camera
        moby_chain_point(lwu(p + 0x20), static_cast<uint32_t>(j), f);
        vec3_sub(V(f + 0x10), V(OV(0x00166dc0)), V(f));
        vec3_set_length(V(f + 0x10), V(f + 0x10), u2f(0x3ecccccdu));
        vec3_add(V(p + 0x10), V(f), V(f + 0x10));
        ee_stack_free(0x20);
    }
    const int32_t r = libc_rand();
    sw(p + 4, color);
    sb(p + 8, static_cast<uint32_t>(r));
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x70));
    sb(p + 3, 0x48);
    swc1(p + 0xc, size);
    sb(p + 1, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20e8))));
    return p;
}

// A soft glow (type 0x48) at `pos` drifting by `vel`: white, size 20000..40000 (40000..80000
// with `mode` < 0), a random spin and texture *0x1b21a0, `k` at +0x2c, a spin rate -6..5,
// its fade a tenth (a twentieth with `mode` >= 0) of its size; with `mode` >= 0 one in 30 starts
// at -40, one in 30 of the others at -1 (+0x34).
// orig: 0x00275158
uint32_t spawn_particle_48(uint32_t pos, uint32_t vel, int32_t mode, float k) {
    const uint32_t p = alloc_particle(0x48);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    sw(p + 4, 0x7fffffffu);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    float lo, hi;
    if (mode >= 0) {
        lo = u2f(0x469c4000u);
        hi = u2f(0x471c4000u);
    } else {
        lo = u2f(0x471c4000u);
        hi = u2f(0x479c4000u);
    }
    const uint32_t q = p + 0x20;
    swc1(p + 0xc, rand_range_f(lo, hi));
    sb(p + 8, static_cast<uint32_t>(rand_mod(8) + 0x1c));
    sb(p + 2, lbu(lwu(OV(0x001b21a0))));
    sw(q + 0x10, static_cast<uint32_t>(mode));
    V(q) = V(vel);
    sw(q + 0x14, 0);
    const int32_t s = rand_mod(0xc) - 6;
    swc1(q + 0xc, k);
    sw(q + 0x18, static_cast<uint32_t>(s));
    swc1(q + 0x1c, lwc1(p + 0xc) * u2f(0x3dcccccdu));
    if (mode < 0) return p;
    swc1(q + 0x1c, lwc1(p + 0xc) * u2f(0x3d4ccccdu));
    if (rand_mod(0x1e) == 0) {
        sw(q + 0x14, static_cast<uint32_t>(-0x28));
    } else if (rand_mod(0x1e) == 0) {
        sw(q + 0x14, 0xffffffffu);
    }
    return p;
}

// Type 0x1a (a glow on a moby): freed when its frames (+0x24) run out or its moby is gone
// (none, or state negative); over its last fifth its alpha falls (+0x2c * frames left over a
// third of its frames +0x28); kept on its moby's joint +0x26 (pulled 0.4 toward the camera) or
// at the moby.
// orig: 0x0026dbe0
void particle_1a_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(q + 4)) != 0 || lwu(p + 0x20) == 0 || static_cast<int8_t>(lbu(lwu(p + 0x20) + 0x20)) < 0) {
        free_particle(p);
        return;
    }
    const int32_t h = lw(q + 8);
    const int32_t left = lh(q + 4);
    if (left < h / 5) {
        const int32_t third = h / 3;
        const int32_t a = third != 0 ? lw(q + 0xc) * left / third : 0;  // (left < h / 5 keeps third > 0)
        sw(p + 4, (lwu(p + 4) & 0x00ffffffu) + (static_cast<uint32_t>(a) << 24));
    }
    const int32_t j = lh(q + 6);
    if (j == -1) {
        V(p + 0x10) = V(lwu(p + 0x20) + 0x10);
        return;
    }
    const uint32_t f = ee_stack_alloc(0x20);  // the joint's point, its pull toward the camera
    moby_chain_point(lwu(p + 0x20), static_cast<uint32_t>(j), f);
    vec3_sub(V(f + 0x10), V(OV(0x00166dc0)), V(f));
    vec3_set_length(V(f + 0x10), V(f + 0x10), u2f(0x3ecccccdu));
    vec3_add(V(p + 0x10), V(f), V(f + 0x10));
    ee_stack_free(0x20);
}

// Type 0x1b (a spark): moves by +0x20, its alpha the start alpha +0x38 times the frames left
// +0x30 over its frames +0x34; freed when they run out.
// orig: 0x0026de00
void particle_1b_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    const int32_t n = lw(q + 0x14);
    const int32_t a = n != 0 ? lw(q + 0x18) * lw(q + 0x10) / n : 0;  // (a zero count traps on the EE: break 7)
    sw(p + 4, (lwu(p + 4) & 0x00ffffffu) + (static_cast<uint32_t>(a) << 24));
    if (timer_tick_i32(w32ref(q + 0x10)) != 0) free_particle(p);
}

// Type 0x48 (a soft glow): spins by +0x38; freed (its size zeroed) once its size is not positive.
// orig: 0x002752e0
void particle_48_update(uint32_t p) {
    sb(p + 8, lbu(p + 8) + lbu(p + 0x38));
    if (lwc1(p + 0xc) <= 0.0f) {
        swc1(p + 0xc, 0.0f);
        free_particle(p);
    }
}

// An angle `cur` steered toward `target`: the rate *v gains k * (the gap / (pi / 20), within
// +-1) less damp * *v, capped by vmax (when not 0) and by the gap itself; returns cur + *v.
// orig: 0x00258110
float angle_seek_rate(uint32_t v, float cur, float target, float k, float damp, float vmax) {
    const float gap = angle_sub(target, cur);
    float t = fpu_div(gap, u2f(0x3e20d97cu));
    if (1.0f < t) t = 1.0f;
    else if (t < -1.0f) t = -1.0f;
    const float r0 = lwc1(v);
    swc1(v, r0 + (k * t - damp * r0));
    const float r = lwc1(v);
    if (vmax != 0.0f) {
        if (vmax < r) swc1(v, vmax);
        else if (r < -vmax) swc1(v, -vmax);
    }
    if (abs_f(gap) < lwc1(v)) {
        swc1(v, abs_f(gap));
    } else if (lwc1(v) < -abs_f(gap)) {
        swc1(v, -abs_f(gap));
    }
    return angle_add(cur, lwc1(v));
}

// The shot `o` ends: its head and trail glows freed (pvars +0x2c, 17 of them); with `sparks`,
// five sparks; three smoke puffs (type 0x17, grey with a random alpha, 50..80 frames, half of
// them additive).
// orig: 0x002cbcc0
void blaster_bolt_end(uint32_t o, int32_t sparks) {
    const uint32_t f = ee_stack_alloc(0x20);  // the original's frame: a direction (by address)
    for (uint32_t i = 0; i < 0x11; ++i) {
        const uint32_t s = lwu(o + 0x78) + 0x2c + i * 4;
        if (lwu(s) != 0) free_particle(lwu(s));
        sw(s, 0);
    }
    const uint32_t pos = o + 0x10;
    if (sparks != 0) {
        for (int32_t k = 4; k >= 0;) {
            V(f + 0x10) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
            swc1(f + 0x10, rand_range_f(-1.0f, 1.0f));
            --k;
            swc1(f + 0x14, rand_range_f(-1.0f, 1.0f));
            swc1(f + 0x18, rand_range_f(-1.0f, 1.0f));
            const float fs = lwc1(0x0015ed6c);
            V(f) = V(f + 0x10);
            vec3_set_length(V(f + 0x10), V(f), rand_range_f(fs * 3.0f, fs * 6.0f));
            const int32_t lo = frame_scale_i(10);
            const int32_t n = rand_range(lo, frame_scale_i(0xf));
            spawn_particle_1b(pos, f + 0x10, 0x7f2f4f6fu, static_cast<uint32_t>(n), u2f(0x4708b800u));
        }
    }
    for (int32_t k = 2; k >= 0;) {
        const uint32_t color = (static_cast<uint32_t>(rand_range(0x20, 0x80)) << 24) | 0x00606060u;
        int32_t spin = 1;
        const int32_t r = rand_mod(2);
        --k;
        V(f) = Vec4{0.0f, 0.0f, 1.0f, 1.0f};
        if (r != 0) spin = -1;
        swc1(f + 8, rand_range_f(u2f(0x3c23d70au), u2f(0x3ccccccdu)) * lwc1(0x0015ed60));
        const uint32_t p = spawn_particle_17(pos, spin, f, color, u2f(0x3e4ccccdu), 1.0f, u2f(0x3f828f5cu), u2f(0x4808b800u));
        if (p == 0) continue;
        const uint32_t q = p + 0x20;
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(rand_range(0x32, 0x50))));
        sw(q + 4, 2);
        sb(q + 0xa, static_cast<uint32_t>(static_cast<int32_t>(color) >> 24));
        sb(q + 0xb, lbu(p + 0xa));
        if (rand_mod(2) != 0) sb(p + 3, 0x44);
    }
    ee_stack_free(0x20);
}

// The shot's frame (see the file's head): steered toward its target point (pvars +0, its
// rates +0x10 / +0x14) or, for its first 35 frames without a bounce, toward its aim (+0x18 /
// +0x1c), else straight; moved 40 units a second; gone off the level, 64 from the camera or when
// its frames (+0x24) run out; hits along its step (ignoring its shooter +0x20): a moby hit ends
// it a few frames later, a wall throws sparks and bounces it (+0x28 the bounces, the surfaces
// of the jump table 0x1e9da0 stop it) with the hand's bounce sound; then the head glow (+0x2c)
// and the trail (+0x30, 16 glows spread behind along the step) placed, and a stray spark.
// orig: 0x002cb418
void blaster_bolt_update(uint32_t o) {
    const uint32_t f = ee_stack_alloc(0x80);  // the original's frame (its records go by address)
    const uint32_t old = f, step = f + 0x10, rec = f + 0x20, pt = f + 0x50, tmp = f + 0x60, sp70 = f + 0x70;
    const uint32_t pv = lwu(o + 0x78);
    const uint32_t pos = o + 0x10;
    auto finish = [&](int32_t sparks) {
        blaster_bolt_end(o, sparks);
        moby_retire(o);
        ee_stack_free(0x80);
    };
    swc1(o + 0x44, angle_add(lwc1(o + 0x44), -lwc1(OV(0x00161894))));
    float yaw, pitch;
    if (vec3_length(V(pv)) < u2f(0x3dcccccdu)) {
        const int32_t left = lh(pv + 0x24);
        const int32_t a = frame_scale_i(0x23);
        if (left < a - frame_scale_i(1) && lw(pv + 0x28) == 0) {
            yaw = lwc1(pv + 0x18);
            pitch = lwc1(pv + 0x1c);
        } else {
            yaw = lwc1(o + 0x48);
            pitch = lwc1(o + 0x44);
        }
    } else {
        yaw = atan2_f(lwc1(pv) - lwc1(o + 0x10), lwc1(pv + 4) - lwc1(o + 0x14));
        pitch = -atan2_f(vec2_distance(V(pos), V(pv)), lwc1(pv + 8) - lwc1(o + 0x18));
    }
    const float tau = u2f(0x40c90fdbu), pi = u2f(0x40490fdbu);
    const float vmax = vec3_length(V(pv)) < u2f(0x3dcccccdu) ? tau : u2f(0x4096cbe4u);
    {
        const float g = lwc1(0x0015ed70);
        swc1(o + 0x48, angle_seek_rate(pv + 0x10, lwc1(o + 0x48), yaw, g * tau, g * pi, lwc1(0x0015ed6c) * vmax));
    }
    {
        const float g = lwc1(0x0015ed70);
        swc1(o + 0x44, angle_seek_rate(pv + 0x14, lwc1(o + 0x44), pitch, g * tau, g * pi, lwc1(0x0015ed6c) * vmax));
    }
    vec_from_spherical_v(V(step), lwc1(0x0015ed6c) * 40.0f, lwc1(o + 0x48), -lwc1(o + 0x44));
    V(old) = V(pos);
    vec3_add(V(pos), V(pos), V(step));
    if (lwc1(o + 0x10) < 0.0f || lwc1(o + 0x14) < 0.0f || lwc1(o + 0x18) < 0.0f || 64.0f < vec2_distance(V(pos), V(OV(0x00166dc0)))) {
        finish(0);
        return;
    }
    // the burn record: along the shot (flat), type 1, the shooter's damage 0.25
    sw(rec + 0x14, 0x00010001u);
    swc1(rec + 0x1c, 0.25f);
    sw(rec + 0x10, o);
    sw(rec + 0x20, 1);
    vec3_sub(V(rec), V(pos), V(0x0013f3d0));
    vec2_set_length(V(rec), V(rec), 1.0f);
    sb(rec + 0x19, 1);
    swc1(rec + 0xc, u2f(0x45afdf66u));
    sh(rec + 0x1a, lhu(o + 0xa6));
    swc1(rec + 8, 1.0f);
    sb(rec + 0x18, 1);
    if (timer_tick_i16(h16(pv + 0x24)) != 0) {
        finish(0);
        return;
    }
    if (segment_collide(old, pos, 0, lwu(pv + 0x20), rec) != 0) {
        if (sub_001f35d8() == 0) {
            const int32_t left = lh(pv + 0x24);
            if (frame_scale_i(3) < left) sh(pv + 0x24, static_cast<uint32_t>(frame_scale_i(3)));
        } else {
            V(pos) = V(HIT + 0x20);
            hit_sparks(pos, step, tmp - 0x10);  // (the original's +0x60 / +0x50)
            if (lbu(0x0013e52f) == 0) {
                finish(0);
                return;
            }
            sw(pv + 0x28, lwu(pv + 0x28) + 1u);
            const uint32_t kind = static_cast<uint32_t>(sub_001f35d8());
            if (kind < 0xe && lwu(OV(0x001e9da0) + kind * 4) == OVF(0x002cb978u)) sw(pv + 0x28, 1000);  // (the jump table's stop surfaces)
            if (static_cast<int32_t>(lbu(0x0013e52f) + 5u) < lw(pv + 0x28)) {
                finish(1);
                return;
            }
            vec3_reflect(V(pt), V(step), V(OV(0x00173e80)));
            swc1(o + 0x48, atan2_f(lwc1(pt), lwc1(pt + 4)));
            swc1(o + 0x44, -atan2_f(vec2_length(V(pt)), lwc1(pt + 8)));
            sw(pv + 0x10, 0);
            sw(pv + 0x14, 0);
            vec3_set_length(V(tmp), V(OV(0x00173e80)), u2f(0x3d4ccccdu));
            vec3_add(V(pos), V(OV(0x00173e60)), V(tmp));
            const uint32_t h = lwu(0x001403e0);
            if (h != 0 && lh(h + 0xa6) == 0xa8 && lbu(h + 0x20) != 0xfe && lbu(h + 0x20) != 0xfd) moby_sound_start(4, 0, h);
        }
    }
    swc1(o + 0x44, angle_add(lwc1(o + 0x44), lwc1(OV(0x00161894))));
    // the trail: the head glow at the shot, 16 glows back along the step, jittered
    const int32_t age = lh(pv + 0x26) - lh(pv + 0x24);
    float len;
    if (age < frame_scale_i(0xd)) len = fpu_div(int_to_float(age), int_to_float(frame_scale_i(0xd))) * lwc1(OV(0x00166c00u - 0x5380));
    else len = lwc1(OV(0x00166c00u - 0x5380));
    vec3_sub(V(step), V(old), V(pos));
    vec3_set_length(V(step), V(step), len);
    V(pt) = V(pos);
    if (lwu(pv + 0x2c) != 0) V(lwu(pv + 0x2c) + 0x10) = V(pt);
    for (uint32_t i = 0; i < 0x10; ++i) {
        const uint32_t g = lwu(pv + 0x30 + i * 4);
        if (g == 0) continue;
        vec3_add(V(g + 0x10), V(pt), V(step));
        V(pt) = V(g + 0x10);
        swc1(g + 0x10, lwc1(g + 0x10) + rand_range_signed_f(0.0f, lwc1(OV(0x00166c00u - 0x5384))));
        swc1(g + 0x14, lwc1(g + 0x14) + rand_range_signed_f(0.0f, lwc1(OV(0x00166c00u - 0x5384))));
        swc1(g + 0x18, lwc1(g + 0x18) + rand_range_signed_f(0.0f, lwc1(OV(0x00166c00u - 0x5384))));
        vec3_scale(V(step), V(step), lwc1(OV(0x00166c00u - 0x537c)));
    }
    if (rand_mod(5) == 0) {  // a stray spark across the shot
        vec3_cross_ba(V(tmp), V(step), V(OV(0x00166ff0)));
        vec_rotate_axis(V(tmp), V(tmp), V(step), rand_range_signed_f(0.0f, u2f(0x40490fdbu)));
        const float l = vec3_length(V(tmp));
        if (l != 0.0f) {
            const float fs = lwc1(0x0015ed6c);
            vec3_scale(V(sp70), V(tmp), fpu_div(rand_range_f(fs * 0.5f, fs + fs), l));
            const int32_t lo = frame_scale_i(10);
            const int32_t n = rand_range(lo, frame_scale_i(0xf));
            spawn_particle_1b(pos, sp70, 0x7f2f4f6fu, static_cast<uint32_t>(n), u2f(0x46ea6000u));
        }
    }
    ee_stack_free(0x80);
}

// A shot (class 0x131) for the shooter `from` at `pos`: its target `target`'s aim point (or
// none), its aim yaw / pitch (`yaw`, `pitch`) and the gun's (`gyaw`, `gpitch`: its own
// direction), 35 frames plus 10 per upgrade 0x13e52f; a wall between the shooter and the muzzle
// throws sparks and bounces it at once (0 when that ends it); else its head glow (type 0x1a)
// and 17 trail glows (type 0x48) sized along a curve. Returns the shot.
// orig: 0x002caf70
uint32_t blaster_bolt_spawn(uint32_t from, uint32_t pos, uint32_t target, float yaw, float pitch, float gyaw, float gpitch) {
    const uint32_t f = ee_stack_alloc(0x60);  // the original's frame (its records go by address)
    const uint32_t muzzle = f, start = f + 0x10, step = f + 0x20;
    V(muzzle) = V(pos);
    const uint32_t o = moby_spawn(0x131);
    if (o == 0) {
        ee_stack_free(0x60);
        return 0;
    }
    const uint32_t pv = lwu(o + 0x78);
    sw(pv + 0x20, from);
    sh(o + 0x32, 0x7f);
    sb(o + 0x30, 0x7f);
    sb(o + 0x31, 1);
    sd(o + 0x38, ld(from + 0x38));
    swc1(o + 0x40, lwc1(OV(0x00166c00u - 0x5370)));
    swc1(o + 0x44, angle_add(gpitch, lwc1(OV(0x00161894))));
    swc1(o + 0x48, gyaw);
    V(o + 0x10) = V(muzzle);
    if (target != 0) aim_point(pv, target);
    else vec_clear(V(pv));
    swc1(pv + 0x18, yaw);
    swc1(pv + 0x1c, pitch);
    sw(pv + 0x10, 0);
    sw(pv + 0x14, 0);
    const int32_t life = frame_scale_i(0x23);
    sw(pv + 0x28, 0);
    const uint32_t n = static_cast<uint32_t>(life) + lbu(0x0013e52f) * 10u;
    sh(pv + 0x26, n);
    sh(pv + 0x24, n);
    V(start) = V(from + 0x10);
    swc1(start + 8, lwc1(muzzle + 8));
    if (segment_collide(start, muzzle, 0, from, 0) != 0) {
        vec_from_spherical_v(V(step), lwc1(0x0015ed6c) * 40.0f, lwc1(o + 0x48), -lwc1(o + 0x44));
        V(o + 0x10) = V(HIT + 0x20);
        hit_sparks(o + 0x10, step, f + 0x40);
        if (lbu(0x0013e52f) == 0) {
            blaster_bolt_end(o, 0);
            moby_retire(o);
            ee_stack_free(0x60);
            return 0;
        }
        sw(pv + 0x28, lwu(pv + 0x28) + 1u);
        if (static_cast<int32_t>(lbu(0x0013e52f) + 5u) < lw(pv + 0x28)) {
            blaster_bolt_end(o, 1);
            moby_retire(o);
            ee_stack_free(0x60);
            return 0;
        }
        vec3_reflect(V(step), V(step), V(OV(0x00173e80)));
        swc1(o + 0x48, atan2_f(lwc1(step), lwc1(step + 4)));
        swc1(o + 0x44, -atan2_f(vec2_length(V(step)), lwc1(step + 8)));
        ee_stack_free(0x60);
        return o;
    }
    spawn_particle_1a(o, 0x2f4f7f7fu, static_cast<uint32_t>(lh(pv + 0x24)), 0xffffffffu, lwc1(OV(0x00166c00u - 0x5378)));
    for (int32_t i = 0; i < 0x11; ++i) {
        const uint32_t slot = pv + 0x2c + static_cast<uint32_t>(i) * 4;
        const uint32_t p = spawn_particle_48(o + 0x10, OV(0x0015f580), 1, 0.0f);
        if (p == 0) {
            sw(slot, 0);
            continue;
        }
        swc1(p + 0xc, rand_range_f(lwc1(OV(0x00166c00u - 0x5390)), lwc1(OV(0x00166c00u - 0x538c))));
        const float t = fpu_div(int_to_float(i), int_to_float(0x10));
        const float c = cubic_interp(-1.5f, 0.0f, 1.0f, 0.0f, t);
        const float s0 = lwc1(p + 0xc);
        const float a = c * -64.0f + 128.0f;
        swc1(p + 0xc, s0 + (lwc1(OV(0x00166c00u - 0x5388)) - s0) * c);
        sw(p + 4, (static_cast<uint32_t>(float_to_int(a)) << 24) | 0x00ffffffu);
        sw(slot, p);
    }
    ee_stack_free(0x60);
    return o;
}

// The auto-aim: of the live targets (list 0x15ffe4, +0x28 the next) flagged 0x1000 within the
// nearest so far (20 at most), the first whose angle off the aim line from `pos` (yaw *`yaw`,
// pitch *`pitch` plus half the hero's lean `P+0x2e4`), less its size (pvar block +0xa / 8), is
// under pi / 20, the camera and `pos` seeing it (no hit, flags 6, ignoring `m`): its pitch set,
// its yaw too within 10 degrees. Returns the target (0: none). A blocked line ends the search.
// orig: 0x002b46a8
uint32_t blaster_auto_aim(uint32_t m, uint32_t pos, uint32_t yaw, uint32_t pitch) {
    const uint32_t f = ee_stack_alloc(0x30);  // the original's frame (its records go by address)
    const uint32_t from = f, at = f + 0x10, line = f + 0x20;
    uint32_t best = 0;
    float near = 20.0f;
    V(from) = V(pos);
    for (uint32_t t = lwu(OV(0x0015ffe4)); t != 0; t = lwu(t + 0x28)) {
        if (static_cast<int8_t>(lbu(t + 0x20)) < 0) continue;
        aim_point(at, t);
        if ((lhu(t + 0x34) & 0x1000) == 0) continue;
        const float ty = atan2_f(lwc1(at) - lwc1(from), lwc1(at + 4) - lwc1(from + 4));
        const float tp = atan2_f(vec2_distance(V(from), V(at)), lwc1(at + 8) - lwc1(from + 8));
        const float off = angle_dist(lwc1(yaw), ty);
        const float d = vec3_distance(V(from), V(at));
        if (near < d) continue;
        vec_from_spherical_v(V(line), d, lwc1(yaw), lwc1(pitch) + lwc1(P + 0x2e4) * 0.5f);
        vec3_add(V(line), V(line), V(from));
        float ang = asin_f(fpu_div(vec3_distance(V(line), V(at)), d + d));
        ang = ang + ang;
        if (u2f(0x3e20d97cu) < ang) {
            const uint32_t w = moby_pvar_word0(t);
            if (w != 0) {
                const float size = int_to_float(lbu(w + 0xa)) * 0.125f;
                const float a = asin_f(fpu_div(size, d));
                if (size < d) {
                    ang = ang - a;
                    if (ang < 0.0f) ang = 0.0f;
                }
            }
        }
        if (!(ang < u2f(0x3e20d97cu))) continue;
        if (segment_collide(OV(0x00166dc0), at, 6, m, 0) != 0 || segment_collide(from, at, 6, m, 0) != 0) {
            ee_stack_free(0x30);
            return best;
        }
        near = d;
        swc1(pitch, -tp);
        if (off < u2f(0x3e32b8c2u)) swc1(yaw, ty);
        best = t;
    }
    ee_stack_free(0x30);
    return best;
}

// The Blaster's frame (see the file's head).
// orig: 0x002b49a8
void blaster_update(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0xd0);  // the original's frame (its records go by address)
    const uint32_t dir = f, aim = f + 0x10, muzzle = f + 0x20, v30 = f + 0x30, v40 = f + 0x40, mat = f + 0x60, hit = f + 0xa0;
    const uint32_t yaw_s = f + 0xc0, pitch_s = f + 0xc4;
    V(dir) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t target = 0;
    const uint32_t pv = lwu(m + 0x78);
    sw(pitch_s, 0);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sw(pv + 0x10, 0xffffffffu);
        sw(pv + 0x14, 0);
    }
    timer_tick_i16(h16(pv + 4));
    timer_tick_i32(w32ref(pv));
    if (lw(P + 0x2084) == 1) hero_set_state(0x1e, 1);
    if (lbu(m + 0x53) == 0 && lbu(m + 0x51) < 3) sw(pv, static_cast<uint32_t>(frame_scale_i(7)));
    // the aim: the camera's line in first person (and its target under the crosshair), else the
    // hero's facing from the muzzle
    const float fs0 = lwc1(0x0015ed6c);
    const int32_t st = lw(P + 0x2084);
    if (st == 0x1e && (lhu(lwu(P + 0x2080) + 0x34) & 1u) != 0) {
        mat3_copy(M(v30), M(OV(0x00166c80)));
        mat3_transpose(M(mat), M(v30));
        swc1(dir + 8, fs0 * 40.0f);
        mat4_transform(V(dir), V(dir), M(mat));
        vec_clear(V(muzzle));
        swc1(muzzle + 4, lwc1(muzzle + 4) + u2f(0x3e19999au));
        swc1(muzzle + 8, lwc1(muzzle + 8) + 0.75f);
        swc1(muzzle, lwc1(muzzle) + u2f(0x3e19999au));
        mat4_transform(V(muzzle), V(muzzle), M(mat));
        vec3_add(V(muzzle), V(muzzle), V(OV(0x00166dc0)));
        if (lw(P + 0x2084) == st && (lhu(lwu(P + 0x2080) + 0x34) & 1u) != 0 && lbu(P + 0x20ac) == 0) {
            cam_effect_add(m, 0xff0f0fffu, nullptr, 0x26, -1, 1.0f, 0.0f, 90.0f);
            vec3_set_length(V(hit), V(dir), 20.0f);
            vec3_add(V(hit + 0x10), V(hit), V(muzzle));
            V(hit) = V(hit + 0x10);
            if (segment_collide(OV(0x00166dc0), hit, 0, m, 0) != 0) {
                const uint32_t o = lwu(HIT + 0x18);
                if (o != 0 && (lhu(o + 0x34) & 0x1000u) != 0) target = o;
            }
        }
    } else {
        swc1(dir, fs0 * 40.0f);
        mat3_transform(V(dir), V(dir), M(0x0013f990));
        moby_chain_point(m, 0, muzzle);
        if (advanced_aiming()) {  // enhancement: the modern controls aim at the screen's centre, the reticle shown
            advanced_aim_direction(muzzle, dir, fs0 * 40.0f);
            if (lbu(P + 0x20ac) == 0) cam_effect_add(m, 0xff0f0fffu, nullptr, 0x26, -1, 1.0f, 0.0f, 90.0f);
        }
    }
    if (lw(P + 0x12b0) != 0) swc1(yaw_s, lwc1(P + 0x12bc));
    else swc1(yaw_s, atan2_f(lwc1(dir), lwc1(dir + 4)));
    const float pt = -atan2_f(vec2_length(V(dir)), lwc1(dir + 8));
    float yaw_r = lwc1(yaw_s);
    float pitch_r = pt;
    swc1(pitch_s, pt);
    if (lw(P + 0x2084) != 0x1e) {
        if (lh(P + 0x22c8) == 1) {
            swc1(yaw_s, lwc1(m + 0x48));
            yaw_r = lwc1(m + 0x48);
            swc1(pitch_s, lwc1(OV(0x0017acf4)));
            pitch_r = lwc1(OV(0x0017acf4));
        }
        target = blaster_auto_aim(m, muzzle, yaw_s, pitch_s);
    }
    if (target != 0 && lh(pv + 4) == 0 && sub_00233e98(-1) != 0) {  // the lock-on reticle
        const uint32_t w = moby_pvar_word0(target);
        const float d = fpu_div(vec3_distance(V(OV(0x00166dc0)), V(target + 0x10)), 50.0f);
        V(v30) = V(target + 0x10);
        const float up = w != 0 ? lwc1(w + 0x10) : 0.5f;
        const float k = u2f(0x3f666666u) - d * u2f(0x3f266666u);
        swc1(v30 + 8, lwc1(v30 + 8) + up);
        cam_effect_add(m, 0xff0fff0fu, &V(v30), 0x26, -1, abs_f(k), 0.0f, 90.0f);
    }
    // the shot
    bool fired_or_skipped = true;
    if (lbu(P + 0x20a8) != 0 && lw(P + 0x2084) != 0x72 && lw(OV(0x0015f5c4)) != 2 &&
        frame_scale_i(5 - static_cast<int32_t>(lbu(0x0013e52f))) < lw(P + 0x1bc) && lw(pv) == 0) {
        fired_or_skipped = false;
        if (sub_00233db8(-1, 1) != 0 && lbu(P + 0x20ac) == 0) {
            sw(pv, static_cast<uint32_t>(frame_scale_i(6)));
            if (target != 0) {
                const float r = lwc1(0x0015ed6c) * 40.0f;
                aim_point(aim, target);
                const float ty = atan2_f(vec2_distance(V(muzzle), V(aim)), lwc1(aim + 8) - lwc1(muzzle + 8));
                const float want = -ty;
                swc1(pitch_s, want);
                pitch_r = pitch_r - lwc1(P + 0x2e4) * 0.5f;
                const float ten = u2f(0x3e32b8c2u);
                if (angle_dist(pitch_r, want) < ten) {
                    pitch_r = lwc1(pitch_s);
                } else if (angle_add(pitch_r, ten) < lwc1(pitch_s)) {
                    pitch_r = angle_add(pitch_r, ten);
                } else if (lwc1(pitch_s) < angle_add(pitch_r, -ten)) {
                    pitch_r = angle_add(pitch_r, -ten);
                }
                vec_from_spherical_v(V(v30), r, yaw_r, -pitch_r);
                vec3_add(V(v30), V(v30), V(muzzle));
                swc1(yaw_s, atan2_f(lwc1(aim) - lwc1(v30), lwc1(aim + 4) - lwc1(v30 + 4)));
                const float tp = atan2_f(vec2_distance(V(v30), V(aim)), lwc1(aim + 8) - lwc1(v30 + 8));
                swc1(pitch_s, -tp);
            } else if (lw(P + 0x2084) != 0x1e) {
                const float lean = lwc1(P + 0x2e4) * 0.5f;
                swc1(pitch_s, lwc1(pitch_s) - lean);
                pitch_r = pitch_r - lean;
            }
            // the muzzle burn: a type-1 hit at (0.7, -0.15, 0.5) in the hero's frame
            sw(v40 + 4, 0x10000);
            swc1(v40 + 0xc, 0.25f);
            sw(v40, m);
            sw(v40 + 0x10, 1);
            swc1(v30, cos_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674))));
            const float sn = sin_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
            swc1(v30 + 4, sn);
            swc1(v30 + 0xc, u2f(0x45afdf66u));
            sb(v40 + 9, 1);
            sh(v40 + 0xa, lhu(m + 0xa6));
            swc1(v30 + 8, 1.0f);
            sb(v40 + 8, 1);
            sub_00233660(mat, u2f(0x3f333333u), u2f(0xbe19999au), 0.5f);
            mobys_in_sphere(mat, u2f(0x3ed70a3du), 5, lwu(P + 0x2080), v30);
            blaster_bolt_spawn(lwu(P + 0x2080), muzzle, target, lwc1(yaw_s), lwc1(pitch_s), yaw_r, pitch_r);
            sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0xf)));
            for (int32_t k = 4; k >= 0;) {  // muzzle sparks along the shot
                V(v40) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
                swc1(v40, rand_range_f(-1.0f, 1.0f));
                --k;
                swc1(v40 + 4, rand_range_f(-1.0f, 1.0f));
                swc1(v40 + 8, rand_range_f(-1.0f, 1.0f));
                V(v30) = V(v40);
                vec3_set_length(V(v30), V(v30), vec3_length(V(dir)) * u2f(0x3f266666u));
                vec3_add(V(v30), V(dir), V(v30));
                const float fs = lwc1(0x0015ed6c);
                vec3_set_length(V(v30), V(v30), rand_range_f(fs * 3.0f, fs * 6.0f));
                const int32_t lo = frame_scale_i(5);
                const int32_t n = rand_range(lo, frame_scale_i(10));
                spawn_particle_1b(muzzle, v30, 0x5f2f4f6fu, static_cast<uint32_t>(n), u2f(0x461c4000u));
            }
            vec3_set_length(V(dir), V(dir), lwc1(0x0015ed6c) * 0.5f);
            const int32_t lo = frame_scale_i(4);
            const int32_t n = rand_range(lo, frame_scale_i(7));
            spawn_particle_1b(muzzle, dir, 0x2f4f7f7fu, static_cast<uint32_t>(n), u2f(0x48435000u));
        }
        if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, frame_scale_i(7));
        // the stats: shots fired, the longest play time, the level bit
        constexpr uint32_t ST = 0x00141680;
        const uint32_t c = lhu(ST + 0x78);
        if (!(0xfffeu < c)) sh(ST + 0x78, c + 1u);
        const int32_t tens = frame_scale_i(lw(0x0015eea4)) / 0x258;
        if (static_cast<int32_t>(lhu(ST + 0x7a)) < tens) sh(ST + 0x7a, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 0x258));
        sw(ST + 0x7c, lwu(ST + 0x7c) | (1u << (lwu(0x0015ed84) & 31u)) | 0x80000000u);
    }
    (void)fired_or_skipped;
    // the hand put away / the fire let go
    bool release;
    if ((lw(PAD + 0x1a0) & lw(P + 0x10a0)) != 0 && sub_00233e98(-1) != 0) release = false;
    else if (frame_scale_i(9) < lw(P + 0x1bc)) release = true;
    else release = lbu(P + 0x20a8) != 0 ? false : true;
    if (!release) release = lw(P + 0x2084) == 0x72 || lw(OV(0x0015f5c4)) == 2;
    if (release) {
        const uint32_t s53 = lbu(m + 0x53);
        if (s53 != 0 || (lbu(m + 0x70) & 2u) != 0) {
            if (lw(P + 0x2084) == 0x72 || lw(OV(0x0015f5c4)) == 2) {
                moby_set_anim(m, 1, 0);
                moby_anim_advance(m);
            } else if (s53 != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(7));
            }
        } else if (lw(P + 0x2084) == 0x72 || lw(OV(0x0015f5c4)) == 2) {
            moby_set_anim(m, 1, 0);
            moby_anim_advance(m);
        }
        hero_hold_release();
        sw(pv, 0);
    }
    // the muzzle light: 0.7 white fading over 9 frames, 2 ahead of the hero
    if (lbu(m + 0x52) != 2 && 0 < lw(pv + 0x14)) {
        auto level = [&] {
            const int32_t n = lw(pv + 0x14) < frame_scale_i(9) ? lw(pv + 0x14) : frame_scale_i(9);
            const float a = int_to_float(n) * u2f(0x3f333333u);
            return fpu_div(a, static_cast<float>(frame_scale_i(9)));
        };
        const float r = level();
        const float g = level();
        const float b = level();
        swc1(v30, lwc1(P + 0x80) + (cos_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674))) * 2.0f));
        const float sn = sin_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
        swc1(v30 + 4, lwc1(P + 0x84) + (sn + sn));
        swc1(v30 + 8, lwc1(P + 0x88) + 1.0f);
        if (lw(pv + 0x10) == -1) {
            sw(pv + 0x10, static_cast<uint32_t>(light_add(v30, 6.0f, 0.0f, r, g, b)));
        } else {
            const uint32_t l = (lwu(pv + 0x10) << 5) + OV(0x001802c0u);
            V(l + 0x10) = V(v30);
            swc1(l + 8, b);
            swc1(l + 0x1c, 6.0f);
            swc1(l, r);
            swc1(l + 4, g);
        }
        timer_tick_i32(w32ref(pv + 0x14));
    } else if (lw(pv + 0x10) != -1) {
        region_clear(lwu(pv + 0x10));
        sw(pv + 0x10, 0xffffffffu);
        sw(pv + 0x14, 0);
    }
    ee_stack_free(0xd0);
}

// orig: (none) — registers the Blaster's moby functions for the moby update calls.
void blaster_fns_register() {
    ovl_register_moby_fn(0x002b49a8, blaster_update);
    ovl_register_moby_fn(0x002cb418, blaster_bolt_update);
    ovl_register_moby_fn(0x0026dbe0, particle_1a_update);
    ovl_register_moby_fn(0x0026de00, particle_1b_update);
    ovl_register_moby_fn(0x002752e0, particle_48_update);
}

}  // namespace rac1
