// Gaspar's own moby classes: the meteors that rain onto the level and the smoke clouds over the lava (classes 263 .. 266, one
// update for the four looks; class 417, the lava's splash) — hand-ported from Gaspar's own overlay. Header gaspar_mobys.h,
// registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR, the base ELF's are bare
// (the frame time 0x15ed6c / its square 0x15ed70, the time step 0x15ed60, the hero's moby 0x1413d0).
//
// Every one of them is drawn at most 100 from the camera (0x166f40): farther away it is put on the line to its true place 100
// away and shrunk by the ratio (its scale +0x2c from the pvars' +0x50).
// The pvars: the kind +0x4c (0 a cloud on a path or the controller of the clouds, 1 a meteor, 2 a puff of smoke); a meteor's
// place +0 (the true one), its target +0x20 (the ground under it, 25 at the least: the lava), its spin +0x40 / +0x44, its
// speed +0x54, its sound +0x58; a puff's velocity +0, place +0x20; the controller's path +0x10, the clouds' count +0x18 and
// timer +0x1c, the path's centre +0x30; a cloud's controller +0x50, its point on the path +0x14, its speed +0xc.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/shared_path_ahead.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

namespace {
// a stack frame of EE memory for the vectors handed to the engine by address (cleared when it goes)
struct Frame {
    uint32_t sp, n;
    explicit Frame(uint32_t size) : sp(ee_stack_alloc(size)), n(size) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float step() { return lwc1(0x0015ed60); }   // the time step
uint32_t camera() { return L9_ADDR(0x00166f40); }
const float kPi = u2f(0x40490fdbu);

// the moby at `at` (an EE vector), drawn at most 100 from the camera when `clamp` (shrunk by the ratio beyond); its scale from
// the pvars' +0x50. Returns the distance to the camera.
float near_camera(uint32_t m, uint32_t pv, uint32_t at, bool clamp_ok = true) {
    vec3_sub(V(m + 0x10), V(at), V(camera()));
    const float len = vec3_length(V(m + 0x10));
    float scale;
    if (100.0f < len && clamp_ok) {
        vec3_set_length(V(m + 0x10), V(m + 0x10), 100.0f);
        vec3_add(V(m + 0x10), V(m + 0x10), V(camera()));
        scale = fpu_mul(lwc1(pv + 0x50), fpu_div(100.0f, len));
    } else {
        lq_sq(m + 0x10, at);
        scale = lwc1(pv + 0x50);
    }
    swc1(m + 0x2c, scale);
    return len;
}

// the spin: x by +0x40, then y by +0x44 from the new x (the original's own order)
void spin(uint32_t m, uint32_t pv) {
    const float x = angle_add(lwc1(m + 0x40), lwc1(pv + 0x40));
    swc1(m + 0x40, x);
    swc1(m + 0x44, angle_add(x, lwc1(pv + 0x44)));
}

// a drag factor: 1 + k x the time step
float drag(float k) { return fpu_add(fpu_mul(step(), k), 1.0f); }

// one of the four looks (classes 263 .. 266), or 0
uint32_t spawn_look() { return moby_spawn(0x107 + static_cast<uint32_t>(rand_mod(4))); }

// a burst of sparks (particle 0x0b) at `at` moving by `vel` (EE vectors): colours from Gaspar's two tables of six (0x208b00,
// 0x208b18), size `size`, the speed 8 .. 10 a frame less `slow`
void sparks(uint32_t at, uint32_t vel, float size, float slow) {
    const float speed = fpu_sub(fpu_mul(rand_range_f(8.0f, 10.0f), dt()), fpu_mul(slow, dt()));
    const uint32_t c0 = lwu(L9_ADDR(0x00208b00) + (static_cast<uint32_t>(rand_mod(6)) << 2));
    const uint32_t c1 = lwu(L9_ADDR(0x00208b18) + (static_cast<uint32_t>(rand_mod(6)) << 2));
    const int32_t lo = frame_scale_i(15);
    const int32_t l1 = rand_range(lo, frame_scale_i(20));
    const int32_t lo2 = frame_scale_i(25);
    const int32_t l2 = rand_range(lo2, frame_scale_i(30));
    spawn_particle_0b(at, vel, c0, c1, static_cast<uint32_t>(l1), static_cast<uint32_t>(l2), 0, 0, size, speed);
}

// the impact's noise and shake: ten puffs of smoke thrown by `vel`, the sound 0xc1 (quieter), and when the hero is not in
// state 0x32 and the meteor within 15 of the camera, the camera shaken (0x166f60 / 0x166f68) by (64 - distance) / 100
void impact(uint32_t m, uint32_t pv, uint32_t vel) {
    for (int i = 0; i < 10; ++i) gas_2ea528_l9(m, vel);
    const int32_t snd = moby_sound_play(6, 0, m, 0xc1);
    sw(pv + 0x58, static_cast<uint32_t>(snd));
    if (snd != 0) sound_slot_volume_set(snd, 0x800);
    if (lwu(0x001413d4) == 0x32) return;
    const float d = vec3_distance(V(pv), V(camera()));
    if (!(d < 15.0f)) return;
    swc1(L9_ADDR(0x00166f60), fpu_mul(fpu_sub(64.0f, d), u2f(0x3c23d70au)));
    sw(L9_ADDR(0x00166f68), static_cast<uint32_t>(frame_scale_i(10)));
}
}  // namespace

// A meteor (one of the four looks) high over the level: 1000, 500 and 200 .. 250 up, plus 256 and a random point 0 .. 250 about
// the level's middle; its target the ground under that point (25 at the least: the lava), a random size and spin, its speed
// 110 .. 130 a frame, its place blurred by up to 100. The moby or 0.
// orig: L9:0x002ea778
uint32_t gas_2ea778_l9() {
    const uint32_t p = spawn_look();
    if (p == 0) return 0;
    const Frame f(0x30);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(p + 0x78);
    const float r = rand_range_f(0.0f, 250.0f);
    const float a = rand_range_f(-kPi, kPi);
    swc1(sp, fpu_mul(cos_vu(a), r));
    const float y = fpu_mul(sin_vu(a), r);
    swc1(sp + 8, 100.0f);
    swc1(sp + 4, fpu_add(y, 256.0f));
    swc1(sp, fpu_add(lwc1(sp), 256.0f));
    swc1(p + 0x10, 1000.0f);
    swc1(p + 0x14, 500.0f);
    swc1(p + 0x18, rand_range_f(200.0f, 250.0f));
    vec3_add(V(p + 0x10), V(p + 0x10), V(sp));
    lq_sq(pv, p + 0x10);
    swc1(p + 0x2c, fpu_mul(lwc1(p + 0x2c), rand_range_f(0.5f, 1.0f)));
    const float k = u2f(0x3c80adfdu);
    swc1(pv + 0x40, rand_range_f(-k, k));
    swc1(pv + 0x44, rand_range_f(-k, k));
    const float ground = floor_probe_v(0.5f, V(sp), 0);
    swc1(sp + 8, ground < 25.0f ? 25.0f : ground);
    lq_sq(pv + 0x20, sp);
    sb(p + 0x30, 0xff);
    sb(p + 0x31, 1);
    sh(p + 0x32, 0xff);
    sw(pv + 0x4c, 1);
    swc1(pv + 0x54, fpu_mul(rand_range_f(110.0f, 130.0f), dt()));
    swc1(pv + 0x50, lwc1(p + 0x2c));
    swc1(pv, fpu_add(lwc1(pv), rand_range_f(-100.0f, 100.0f)));
    swc1(pv + 4, fpu_add(lwc1(pv + 4), rand_range_f(-100.0f, 100.0f)));
    near_camera(p, pv, pv);
    sw(pv + 0x58, 0xffffffffu);
    moby_update_transform(p);
    return p;
}

// A cloud for the controller `ctl` (its count +0x18 one more): grey, faded in, a random size and slow spin, on the controller's
// path +0x10 (none: it stays where the controller is), off it sideways by -30 .. 30 and by up to 5 across, its speed by its
// offset. The moby or 0.
// orig: L9:0x002eaa50
uint32_t gas_2eaa50_l9(uint32_t ctl) {
    const uint32_t p = spawn_look();
    if (p == 0) return 0;
    const uint32_t cpv = lwu(ctl + 0x78), pv = lwu(p + 0x78);
    lq_sq(p + 0x10, ctl + 0x10);
    sw(cpv + 0x18, lwu(cpv + 0x18) + 1);
    sw(pv + 0x50, ctl);
    sw(pv + 0x48, 1);
    sw(pv + 0x10, lwu(cpv + 0x10));
    sw(pv + 0x14, 0);
    sb(p + 0x31, 1);
    sb(p + 0xbc, 1);
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    moby_set_color_hi(p, 0x80, 0x80, 0x80);
    sb(p + 0x23, 0);
    swc1(p + 0x2c, fpu_mul(lwc1(p + 0x2c), rand_range_f(u2f(0x3ecccccdu), u2f(0x3f59999au))));
    const float k = u2f(0x3bab92a6u);
    swc1(pv + 0x40, rand_range_f(-k, k));
    swc1(pv + 0x44, rand_range_f(-k, k));
    if (lw(pv + 0x10) >= 0) {
        const Frame f(0x30);
        const uint32_t sp = f.sp;
        const uint32_t path = lvd::path(lw(pv + 0x10));
        const float off = rand_range_f(-30.0f, 30.0f);
        const uint32_t last = path + (lwu(path) << 4);
        vec3_sub(V(sp), V(last), V(last - 0x10));
        vec3_sub(V(sp + 0x10), V(path + 0x20), V(path + 0x10));
        vec3_cross_ba(V(sp + 0x10), V(sp + 0x10), V(sp));
        sw(sp + 0x18, 0);
        vec3_set_length(V(pv), V(sp + 0x10), off);
        vec3_sub(V(sp), V(path + (lwu(path) << 4)), V(path + 0x10));
        vec3_cross_ba(V(sp + 0x10), V(sp + 0x10), V(sp));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), rand_range_f(0.0f, 5.0f));
        vec3_add(V(pv), V(pv), V(sp + 0x10));
        lq_sq(pv + 0x20, path + 0x10);
        lq_sq(pv + 0x30, cpv + 0x30);
        vec3_add(V(p + 0x10), V(path + 0x10), V(pv));
        vec3_add(V(p + 0x10), V(p + 0x10), V(camera()));
        vec3_sub(V(p + 0x10), V(p + 0x10), V(pv + 0x30));
        const float spd = fpu_add(fpu_div(off, 20.0f), 10.0f);
        const float grow = fpu_add(fpu_div(off, 100.0f), 1.0f);
        swc1(pv + 0xc, spd);
        swc1(p + 0x2c, fpu_mul(lwc1(p + 0x2c), grow));
        swc1(pv + 0xc, fpu_mul(lwc1(pv + 0xc), dt()));
    }
    moby_update_transform(p);
    return p;
}

// A puff of smoke (kind 2): fades (+0x23 down by 2; gone below 3), slows (2 % a step), falls (20 x dt^2), drifts, spins.
// orig: L9:0x002ead30
void gas_2ead30_l9(uint32_t m) {
    const uint32_t fade = lbu(m + 0x23);
    if (fade < 3) {
        moby_retire(m);
        return;
    }
    sb(m + 0x23, fade - 2);
    const uint32_t pv = lwu(m + 0x78);
    vec3_scale(V(pv), V(pv), drag(u2f(0xbca3d700u)));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt_sq(), 20.0f)));
    vec3_add(V(pv + 0x20), V(pv + 0x20), V(pv));
    near_camera(m, pv, pv + 0x20);
    spin(m, pv);
}

// A puff of smoke (kind 2) off the meteor `src` (at its true place, by up to 2), thrown at 50 a frame plus `vel` (an EE
// vector), slowed by 70 % a step; an eighth to a quarter of the meteor's size, a random spin, faded in at 0x7f. The moby or 0.
// orig: L9:0x002ea528
uint32_t gas_2ea528_l9(uint32_t src, uint32_t vel) {
    const uint32_t p = spawn_look();
    if (p == 0) return 0;
    const uint32_t pv = lwu(p + 0x78), spv = lwu(src + 0x78);
    swc1(pv, rand_range_f(-2.0f, 2.0f));
    swc1(pv + 4, rand_range_f(-2.0f, 2.0f));
    swc1(pv + 8, rand_range_f(-2.0f, 2.0f));
    vec3_add(V(pv + 0x20), V(pv), V(spv));
    vec3_set_length(V(pv), V(pv), fpu_mul(dt(), 50.0f));
    vec3_add(V(pv), V(pv), V(vel));
    vec3_scale(V(pv), V(pv), drag(u2f(0xbf333333u)));
    sb(p + 0x30, 0xff);
    sb(p + 0x31, 1);
    sh(p + 0x32, 0xff);
    sw(pv + 0x4c, 2);
    sb(p + 0x23, 0x7f);
    const float size = fpu_mul(lwc1(src + 0x2c), rand_range_f(0.125f, 0.25f));
    swc1(p + 0x2c, size);
    swc1(pv + 0x50, size);
    const float k = u2f(0x3d567750u);
    swc1(pv + 0x40, rand_range_f(-k, k));
    swc1(pv + 0x44, rand_range_f(-k, k));
    near_camera(p, pv, pv + 0x20);
    return p;
}

// A splash of the lava (class 417) at `at` (an EE vector), off the meteor's way `dir`: at a random heading, `lo` .. `hi` out,
// thrown outward by how far the heading is from the way and along it by the rest (squared), a third of that kept; bigger the
// nearer the way. The moby or 0. (The original takes `at` / `dir` in $a1 / $a2: its $a0 is unused.)
// orig: L9:0x002efe48
uint32_t gas_2efe48_l9(uint32_t at, uint32_t dir, float lo, float hi) {
    const uint32_t p = moby_spawn(0x1a1);
    if (p == 0) return 0;
    const Frame f(0x20);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(p + 0x78);
    const float a = rand_range_f(-kPi, kPi);
    const float r = rand_range_f(lo, hi);
    const float off = angle_dist(atan2_f(lwc1(dir), lwc1(dir + 4)), a);
    swc1(p + 0x10, fpu_mul(cos_vu(a), r));
    const float y = fpu_mul(sin_vu(a), r);
    sw(p + 0x18, 0);
    swc1(p + 0x14, y);
    vec3_set_length(V(pv), V(p + 0x10), fpu_mul(off, dt()));
    const float rest = fpu_sub(kPi, off);
    vec3_set_length(V(sp), V(dir), fpu_mul(fpu_mul(fpu_mul(rest, rest), 0.25f), dt()));
    vec3_add(V(pv), V(pv), V(sp));
    sw(pv + 8, 0);
    vec3_add(V(p + 0x10), V(p + 0x10), V(at));
    vec3_add(V(p + 0x10), V(p + 0x10), V(dir));
    vec3_scale(V(pv), V(pv), u2f(0x3eb33333u));
    swc1(p + 0x58, fpu_mul(lwc1(p + 0x58), 0.5f));
    swc1(p + 0x48, atan2_f(lwc1(pv), lwc1(pv + 4)));
    const float big = fpu_add(fpu_add(rest, rest), 1.0f);
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    sb(p + 0x31, 1);
    swc1(p + 0x2c, fpu_mul(lwc1(p + 0x2c), big));
    moby_update_transform(p);
    return p;
}

// A meteor (kind 1). It flies at its speed toward its target, drawn near the camera while high, spinning. Below 100 over the
// level (x, y under 500) it tests the next 25 of its way against the world: on the ground above the lava it bursts (eight
// sparks, ten puffs, its sound, the camera shaken when near) and dies. Aimed at the lava (a target of 25), below 27 it
// splashes (five splashes, slows, its hiss). Past its target's height: into the lava it is done, on the ground it bursts as
// above. Dying (state 1 on) it splashes twice a frame into the lava until 10 frames, or sparks three times a frame for 3.
// orig: L9:0x002eaea8
void gas_2eaea8_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xa0);
    const uint32_t sp = f.sp, ahead = sp + 0x10;
    vec3_sub(V(sp), V(pv + 0x20), V(pv));
    vec3_set_length(V(ahead), V(sp), 25.0f);
    vec3_set_length(V(sp), V(sp), lwc1(pv + 0x54));
    vec3_add(V(pv), V(pv), V(sp));
    const float dist = near_camera(m, pv, pv, 27.0f < lwc1(pv + 8));
    spin(m, pv);
    auto slow_near = [dist] { return dist < 7.0f ? fpu_sub(7.0f, dist) : 0.0f; };
    auto lava_splash = [m, pv, sp](int n) {
        lq_sq(sp + 0x20, pv);
        swc1(sp + 0x28, 25.0f);
        for (int i = 0; i < n; ++i) {
            const float s = lwc1(pv + 0x50);
            gas_2efe48_l9(sp + 0x20, sp, fpu_mul(s, 4.0f), fpu_mul(s, 12.0f));
        }
    };
    const uint32_t st = lbu(m + 0x20);
    if (st != 0) {  // dying
        sb(m + 0x20, st + 1);
        if (lwc1(pv + 0x28) == 25.0f) {
            sh(m + 0x32, 0);
            sb(m + 0x31, 0);
            lava_splash(2);
            if (frame_scale_i(10) < static_cast<int32_t>(lbu(m + 0x20))) moby_retire(m);
            return;
        }
        const float slow = slow_near();
        vec3_scale(V(sp + 0x20), V(sp), 0.25f);
        for (int i = 0; i < 3; ++i) {
            const float speed = fpu_sub(fpu_mul(rand_range_f(8.0f, 10.0f), dt()), fpu_mul(slow, dt()));
            swc1(sp + 0x30, rand_range_f(-1.0f, 1.0f));
            swc1(sp + 0x34, rand_range_f(-1.0f, 1.0f));
            swc1(sp + 0x38, rand_range_f(-1.0f, 1.0f));
            vec3_add(V(sp + 0x30), V(sp + 0x30), V(pv));
            const uint32_t c0 = lwu(L9_ADDR(0x00208b00) + (static_cast<uint32_t>(rand_mod(6)) << 2));
            const uint32_t c1 = lwu(L9_ADDR(0x00208b18) + (static_cast<uint32_t>(rand_mod(6)) << 2));
            const int32_t lo = frame_scale_i(15);
            const int32_t l1 = rand_range(lo, frame_scale_i(20));
            const int32_t lo2 = frame_scale_i(25);
            const int32_t l2 = rand_range(lo2, frame_scale_i(30));
            spawn_particle_0b(sp + 0x30, sp + 0x20, c0, c1, static_cast<uint32_t>(l1), static_cast<uint32_t>(l2), 0, 0, 1200000.0f, speed);
        }
        if (!(lbu(m + 0x20) < 3)) moby_retire(m);
        return;
    }
    // flying: low over the level, its way tested against the world
    if (lwc1(pv + 8) < 100.0f && lwc1(pv) < 500.0f && lwc1(pv + 4) < 500.0f) {
        swc1(sp + 0x20, rand_range_f(-5.0f, 5.0f));
        swc1(sp + 0x24, rand_range_f(-5.0f, 5.0f));
        swc1(sp + 0x28, rand_range_f(-5.0f, 5.0f));
        vec3_add(V(sp + 0x20), V(sp + 0x20), V(pv));
        vec3_add(V(sp + 0x30), V(sp + 0x20), V(ahead));
        if (segment_collide_v(V(sp + 0x20), V(sp + 0x30), 4, m, nullptr) != 0 && 26.5f < coll_hit_point().z) {
            const float slow = slow_near();
            vec3_scale(V(sp + 0x40), V(sp), 0.1f);
            for (int i = 0; i < 8; ++i) sparks(pv, sp + 0x40, 400000.0f, slow);
            impact(m, pv, sp);
            sb(m + 0x20, lbu(m + 0x20) + 1);
            return;
        }
    }
    if (lwc1(pv + 0x28) == 25.0f && lwc1(pv + 8) < 27.0f) {  // over the lava: its splash and hiss
        lava_splash(5);
        const float slowed = fpu_mul(lwc1(pv + 0x54), drag(u2f(0xbdccccd0u)));
        const int32_t snd = lw(pv + 0x58);
        swc1(pv + 0x54, slowed);
        if (snd <= 0) sw(pv + 0x58, static_cast<uint32_t>(moby_sound_play(0, 0, m, 0x107)));
    }
    if (!(lwc1(pv + 8) < lwc1(pv + 0x28))) return;
    if (26.5f < lwc1(pv + 0x28)) {  // on the ground: the burst
        const float slow = slow_near();
        vec3_scale(V(sp + 0x20), V(sp), 0.5f);
        for (int i = 0; i < 8; ++i) {
            const float speed = fpu_sub(fpu_mul(rand_range_f(8.0f, 10.0f), dt()), fpu_mul(slow, dt()));
            swc1(sp + 0x30, rand_range_f(-3.0f, 3.0f));
            swc1(sp + 0x34, rand_range_f(-3.0f, 3.0f));
            swc1(sp + 0x38, rand_range_f(-3.0f, 3.0f));
            vec3_add(V(sp + 0x30), V(sp + 0x30), V(pv));
            const uint32_t c0 = lwu(L9_ADDR(0x00208b00) + (static_cast<uint32_t>(rand_mod(6)) << 2));
            const uint32_t c1 = lwu(L9_ADDR(0x00208b18) + (static_cast<uint32_t>(rand_mod(6)) << 2));
            const int32_t lo = frame_scale_i(15);
            const int32_t l1 = rand_range(lo, frame_scale_i(20));
            const int32_t lo2 = frame_scale_i(25);
            const int32_t l2 = rand_range(lo2, frame_scale_i(30));
            spawn_particle_0b(sp + 0x30, sp + 0x20, c0, c1, static_cast<uint32_t>(l1), static_cast<uint32_t>(l2), 0, 0, 1200000.0f, speed);
        }
        impact(m, pv, sp);
    }
    sb(m + 0x20, lbu(m + 0x20) + 1);
}

// Classes 263 .. 266 (four looks of one cloud / meteor / smoke update): a puff (kind 2) or a meteor (kind 1), else a cloud on a
// path or the clouds' controller. The controller (+0xbc 0) sets itself up once: the path's centre, 23 meteors already on
// their way, 50 clouds along its path; then a new meteor now and then (one in 40 frames). A cloud (+0xbc 1) and the
// controller follow the path by +0xc a frame (the controller back to its start at the end, a cloud gone), faded in near the
// path's ends; the controller adds a cloud every 15 .. 45 frames while it has fewer than 50; drawn about the camera by the
// path's centre.
// orig: L9:0x002eb970
void gas_class263_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t kind = lw(pv + 0x4c);
    if (kind == 2) {
        gas_2ead30_l9(m);
        return;
    }
    if (kind == 1) {
        gas_2eaea8_l9(m);
        return;
    }
    const Frame f(0x40);
    const uint32_t sp = f.sp;
    if (lbu(m + 0xbc) == 0) {
        if (lw(pv + 0x48) == 0) {  // the controller's set-up
            swc1(pv + 0x30, 0.0f);
            swc1(pv + 0x38, 0.0f);
            swc1(pv + 0xc, fpu_mul(dt(), 10.0f));
            swc1(pv + 0x34, 0.0f);
            if (lw(pv + 0x10) >= 0) {
                const uint32_t path = lvd::path(lw(pv + 0x10));
                for (int32_t i = 0; i < lw(path); ++i) vec3_add(V(pv + 0x30), V(pv + 0x30), V(path + 0x10 + static_cast<uint32_t>(i) * 0x10));
                vec3_scale(V(pv + 0x30), V(pv + 0x30), fpu_div(1.0f, int_to_float(lw(path))));
                swc1(pv + 0x38, fpu_sub(lwc1(path + 0x18), 10.0f));
                for (int i = 0; i < 23; ++i) {  // meteors already on their way
                    const uint32_t p = gas_2ea778_l9();
                    if (p == 0) continue;
                    const uint32_t ppv = lwu(p + 0x78);
                    vec3_sub(V(sp), V(ppv + 0x20), V(ppv));
                    const float len = vec3_length(V(sp));
                    vec3_set_length(V(sp), V(sp), rand_range_f(0.0f, fpu_sub(len, 32.0f)));
                    vec3_add(V(ppv), V(ppv), V(sp));
                }
                for (int i = 0; i < 50; ++i) {  // clouds along the path
                    const uint32_t c = gas_2eaa50_l9(m);
                    if (c == 0) continue;
                    const uint32_t cpv = lwu(c + 0x78);
                    const int32_t k = rand_mod(lw(path) - 2);
                    const float t = rand_range_f(0.0f, 1.0f);
                    sw(cpv + 0x14, static_cast<uint32_t>(k));
                    const uint32_t p0 = path + 0x10 + (static_cast<uint32_t>(k) << 4);
                    vec3_sub(V(sp), V(p0 + 0x10), V(p0));
                    vec3_scale(V(sp), V(sp), t);
                    vec3_add(V(cpv + 0x20), V(sp), V(p0));
                    lq_sq(c + 0x10, cpv + 0x20);
                    vec3_add(V(c + 0x10), V(path + 0x10), V(cpv));
                    vec3_add(V(c + 0x10), V(c + 0x10), V(camera()));
                    vec3_sub(V(c + 0x10), V(c + 0x10), V(cpv + 0x30));
                }
            }
            sw(pv + 0x48, 1);
        }
        if (lbu(m + 0xbc) == 0 && rand_mod(frame_scale_i(40)) == 0) gas_2ea778_l9();
    }
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    if (lw(pv + 0x10) < 0) return;
    lq_sq(m + 0x10, pv + 0x20);
    const uint32_t path = lvd::path(lw(pv + 0x10));
    if (lbu(m + 0xbc) == 0 && lw(pv + 0x18) < 50 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c))) != 0) {
        const int32_t base = frame_scale_i(15);
        sw(pv + 0x1c, static_cast<uint32_t>(base + rand_mod(frame_scale_i(30))));
        gas_2eaa50_l9(m);
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x40)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x44)));
    const int32_t k = path_point_ahead(m, path, m + 0x10, lw(pv + 0x14), 1, lwc1(pv + 0xc));
    sw(pv + 0x14, static_cast<uint32_t>(k));
    if (k == lw(path) - 1) {
        if (lbu(m + 0xbc) != 0) {  // a cloud at the end: gone, its controller told
            const uint32_t cpv = lwu(lwu(lwu(m + 0x78) + 0x50) + 0x78);
            sw(cpv + 0x18, lwu(cpv + 0x18) - 1);
            moby_retire(m);
        } else {
            sw(pv + 0x14, 0);
            lq_sq(m + 0x10, path + 0x10);
        }
    }
    // faded in over the first and the last segment (by how far along it the moby is), else full (0x80)
    const int32_t at = lw(pv + 0x14);
    const int32_t n = lw(path);
    bool edge = false;
    float seg = 0.0f;
    if (at == 0) {
        vec3_sub(V(sp), V(path + 0x20), V(path + 0x10));
        seg = vec3_length(V(sp));
        vec3_set_length(V(sp), V(sp), 1.0f);
        vec3_sub(V(sp + 0x10), V(m + 0x10), V(path + 0x10));
        edge = true;
    } else if (at == n - 2) {
        const uint32_t end = path + (static_cast<uint32_t>(n) << 4);
        vec3_sub(V(sp), V(end - 0x10), V(end));
        seg = vec3_length(V(sp));
        vec3_set_length(V(sp), V(sp), 1.0f);
        vec3_sub(V(sp + 0x10), V(m + 0x10), V(path + (lwu(path) << 4)));
        edge = true;
    }
    if (edge) {
        float d = vec3_dot(V(sp + 0x10), V(sp));
        if (d < 0.0f) d = 0.0f;
        const uint32_t a = f2u(eer::cvt_w(fpu_div(fpu_mul(d, 128.0f), seg)));
        sb(m + 0x23, (a & 0xff) < 129 ? a : 0x80);
    } else {
        sb(m + 0x23, 0x80);
    }
    lq_sq(pv + 0x20, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    vec3_add(V(m + 0x10), V(m + 0x10), V(camera()));
    vec3_sub(V(m + 0x10), V(m + 0x10), V(pv + 0x30));
}

// Class 417: a splash of the lava: moves, slows (2 % a step), gone when its animation ends (+0x70 bit 2).
// orig: L9:0x002f0040
void gas_class417_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    vec3_scale(V(pv), V(pv), drag(u2f(0xbca3d700u)));
    if ((lbu(m + 0x70) & 2) != 0) moby_retire(m);
}

}  // namespace rac1
