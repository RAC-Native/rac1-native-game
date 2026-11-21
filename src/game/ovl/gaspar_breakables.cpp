// Gaspar's own moby classes: the things that break (hand-ported from Gaspar's own overlay). Header gaspar_mobys.h, registered
// in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR, the base ELF's are bare (the
// frame time 0x15ed6c / its square 0x15ed70, the time step 0x15ed60, the level number 0x15ed84, the per-level flag bytes
// 0x14c050).
//   class 276 and its damaged builds 1298 / 1299 / 1300: hit (mask 0x40000) each becomes the next (the third drops its bolts),
//              in a burst of chunks (classes 320 / 321) and a blast; the last goes
//   class 279  a thing with hit points (+0): hit (mask 0x10000) it flashes, at 0 it bursts in sparks and leaves its remains
//              (class 284)
//   class 313  a stone (sparks when its time runs out) and classes 320 / 321, the chunks: they fly, bounce and spark out
//   class 1285 and its builds 1286 / 1287 / 1288: hit (mask 0x40000) each becomes the next, in a burst (the last's chunks) and
//              a blast; the last sets its story byte (pvars +0) and goes
#include <game/collision/collision.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/weapons.h>

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
uint32_t camera() { return L9_ADDR(0x00166f40); }

// `n` sparks (particle 0x1b, white-blue) off the moby's place, each a random direction plus half of it, at 3 .. 6 a frame,
// for 10 .. 15 frames, size `size`
void spark_out(uint32_t m, uint32_t sp, int n, float size) {
    for (int i = 0; i < n; ++i) {
        V(sp + 0x20) = Vec4{};
        swc1(sp + 0x20, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x24, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x28, rand_range_f(-1.0f, 1.0f));
        V(sp + 0x10) = V(sp + 0x20);
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), fpu_mul(vec3_length(V(sp + 0x20)), 0.5f));
        vec3_add(V(sp + 0x10), V(sp + 0x20), V(sp + 0x10));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
        const int32_t lo = frame_scale_i(10);
        const int32_t frames = rand_range(lo, frame_scale_i(15));
        spawn_particle_1b(m + 0x10, sp + 0x10, 0x7f2f4f6fu, static_cast<uint32_t>(frames), size);
    }
}

// a flying piece's move (its old place at `sp`), still in reach: not below `lo` (nor above 1021 when `capped`) on an axis,
// within 64 (x, y) of the camera
bool move_and_check(uint32_t m, uint32_t pv, uint32_t sp, float lo, bool capped) {
    lq_sq(sp, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < lo || (capped && 1021.0f < c)) return false;
    }
    return !(64.0f < vec2_distance(V(m + 0x10), V(camera())));
}

// a bounce off the last hit: its normal (in the hit record) shortened to 0.05, the piece put that far off the contact point
void off_the_hit(uint32_t m) {
    Vec4 n = coll_hit_normal();
    vec3_set_length(n, n, u2f(0x3d4ccccdu));
    coll_hit_set_normal(n);
    vec3_add(V(m + 0x10), coll_hit_point(), n);
}
}  // namespace

// The next build `cls` of the breakable `m` in its place and turn: safe from hits for 10 frames (+0xbc), the hero's colour, its
// draw flags; the last build (1300) half the time with the flag 0x8000. The moby or 0.
// orig: L9:0x002edb60
uint32_t gas_2edb60_l9(uint32_t m, uint32_t cls) {
    const uint32_t n = moby_spawn(cls);
    if (n == 0) return 0;
    sh(n + 0x32, 0xff);
    sb(n + 0x30, 0xff);
    sb(n + 0x31, 1);
    sb(n + 0x20, 0);
    lq_sq(n + 0x10, m + 0x10);
    lq_sq(n + 0x40, m + 0x40);
    sb(n + 0xbc, static_cast<uint32_t>(frame_scale_i(10)));
    moby_update_transform(n);
    moby_copy_hero_color(n);
    sh(n + 0x34, lhu(m + 0x34));
    if (cls == 0x514 && (libc_rand() & 1) != 0) sh(n + 0x34, lhu(n + 0x34) | 0x8000);
    return n;
}

// A chunk (class 320, or 321 with `kind` 1) of the breakable `src` at `pos` moving by `vel` (EE vectors): scaled `scale` of its
// class's size, a random turn and spin, 90 .. 110 frames (+0x1c), its breakable +0x18. The moby or 0.
// orig: L9:0x002ef868
uint32_t gas_chunk_l9(uint32_t src, uint32_t pos, uint32_t vel, uint32_t kind, float scale) {
    const Frame f(0x20);
    const uint32_t sp = f.sp;
    lq_sq(sp + 0x10, vel);
    lq_sq(sp, pos);
    const uint32_t p = moby_spawn(kind + 0x140);
    if (p == 0) return 0;
    const uint32_t pv = lwu(p + 0x78);
    sw(pv + 0x18, src);
    sh(p + 0x32, 0x7f);
    sb(p + 0x31, 1);
    sb(p + 0x30, 0x7f);
    sw(p + 0x38, lwu(src + 0x38));
    sw(p + 0x3c, lwu(src + 0x3c));
    swc1(p + 0x40, rand_angle());
    swc1(p + 0x44, rand_angle());
    swc1(p + 0x48, rand_angle());
    lq_sq(p + 0x10, sp);
    lq_sq(pv, sp + 0x10);
    const float sx = fpu_mul(dt(), u2f(0x4111361eu));
    swc1(p + 0x2c, fpu_mul(lwc1(lwu(p + 0x24) + 0x24), scale));
    swc1(pv + 0x10, rand_range_f(-sx, sx));
    const float sy = fpu_mul(dt(), u2f(0x40c90fdbu));
    swc1(pv + 0x14, rand_range_f(-sy, sy));
    sw(pv + 0x1c, static_cast<uint32_t>(frame_scale_i(rand_range(90, 110))));
    moby_update_transform(p);
    return p;
}


// Classes 276, 1298, 1299 and 1300. Gone when its flag byte of the level is set. Hit (mask 0x40000) while not safe (+0xbc
// counted down): the next build in its place (1298, 1299, 1300; the bolts before the last), safe again for 10 frames,
// shaken, its sound, and a burst: 20 chunks thrown from its body away from the camera (each the last build's, else one in
// four), a small blast every third, a big one. Gone when its next build came out, or when it was the last.
// orig: L9:0x002edc30
void gas_class276_update_l9(uint32_t m) {
    const uint32_t idx = lbu(m + 0xb0);
    if (idx != 0xff && lbu(0x0014c050 + idx + (lwu(0x0015ed84) << 4)) == 0xff) {
        moby_retire(m);
        return;
    }
    const Frame f(0x90);
    const uint32_t sp = f.sp;
    sw(sp + 0x80, 0);  // (whether the next build came out)
    const uint32_t hit = sub_0025a420(m, 0x40000u, 0);
    if (lbu(m + 0x20) == 0) sb(m + 0x20, 1);
    if (countdown_u8(m + 0xbc) == 0) return;
    if (hit == 0) return;
    const int32_t cls = lh(m + 0xa6);
    if (cls < 1300) {
        uint32_t next = 0x512;
        if (cls == 0x512) {
            next = 0x513;
        } else if (cls == 0x513) {
            lq_sq(sp + 0x20, m + 0x10);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 4.0f));
            moby_drop_bolts(m, 0, -1);
            lq_sq(m + 0x10, sp + 0x20);
            next = 0x514;
        }
        sw(sp + 0x80, gas_2edb60_l9(m, next) != 0 ? 1u : 0u);
    }
    sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(10)));
    const uint32_t at = sp + 0x20, top = sp + 0x30, away = sp + 0x40;
    lq_sq(top, m + 0x10);
    swc1(top + 8, fpu_add(lwc1(top + 8), 1.25f));
    lq_sq(at, top);
    metal_25f8e0(at, u2f(0x3ecccccdu));
    moby_sound_start(0, 0, m);  // (the original branches on the build, below 1299 or not, to the same call)
    sb(m + 0xa4, 0xff);
    vec3_sub(V(away), V(camera()), V(at));
    vec3_set_length(V(away), V(away), 2.0f);
    const float half = cls == 0x114 ? 4.0f : 2.0f;
    const uint32_t piece = sp + 0x50, local = sp + 0x60, push = sp + 0x70;
    for (int32_t i = 0; i < 20; ++i) {
        V(local) = Vec4{};
        swc1(local, rand_range_f(-0.5f, 0.5f));
        swc1(local + 4, rand_range_f(-2.0f, 2.0f));
        swc1(local + 8, rand_range_f(-half, half));
        V(piece) = V(local);
        mat3_transform(V(piece), V(piece), M(m + 0xc0));
        vec3_add(V(piece), V(piece), V(top));
        vec3_sub(V(local), V(piece), V(at));
        const float k = fpu_div(abs_f(fpu_sub(3.0f, vec3_length(V(local)))), 3.0f);
        vec3_set_length(V(push), V(hit + 0x10), fpu_mul(fpu_mul(k, k), 8.0f));
        vec3_add(V(local), V(local), V(push));
        vec3_set_length(V(local), V(local), fpu_mul(k, fpu_mul(rand_range_f(6.0f, 10.0f), dt())));
        vec3_add(V(piece), V(piece), V(away));
        if (cls == 0x514 || rand_mod(4) == 0) gas_chunk_l9(m, piece, local, static_cast<uint32_t>(i) & 1, rand_range_f(1.0f, 2.0f));
        if (ee_rem(i, 3) == 0) explosion_burst(m, piece, -1, rand_range_f(1.0f, u2f(0x3fcccccdu)), 0.0f);
    }
    vec3_scale(V(piece), V(hit + 0x10), 0.5f);
    vec3_add(V(at), V(at), V(away));
    explosion_effect(m, hit + 0x10, at, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 2.0f, 1.0f, 1.0f, 15.0f);
    if (cls == 0x514 || lw(sp + 0x80) != 0) moby_retire(m);
}

// The remains (class 284) of `m` at `pos` (an EE vector) heading `yaw`, in its draw state. The moby or 0.
// orig: L9:0x002ee380
uint32_t gas_2ee380_l9(uint32_t m, uint32_t pos, float yaw) {
    const Frame f(0x10);
    lq_sq(f.sp, pos);
    const uint32_t n = moby_spawn(0x11c);
    if (n == 0) return 0;
    sb(n + 0x30, lbu(m + 0x30));
    sb(n + 0x31, 1);
    sh(n + 0x32, lbu(m + 0x30));
    lq_sq(n + 0x10, f.sp);
    lq_sq(n + 0x40, m + 0x40);
    swc1(n + 0x48, yaw);
    sw(n + 0x38, lwu(m + 0x38));
    sw(n + 0x3c, lwu(m + 0x3c));
    moby_update_transform(n);
    return n;
}

// Class 279. Hit (mask 0x10000): its hit points (+0) less the damage; still above 0 it flashes (+0x40, 0x46); else 25 sparks
// burst from it along its heading (slowed 35 % a step), its remains stay, it goes. Its hit slot cleared when it lives.
// orig: L9:0x002ee110
void gas_class279_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    if (hit == 0) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const float hp = fpu_sub(lwc1(pv), lwc1(hit + 0x2c));
    swc1(pv, hp);
    if (0.0f < hp) {
        sb(pv + 0x47, 0x46);
        color_pulse_start(m, pv + 0x40);
        sb(m + 0xa4, 0xff);
        return;
    }
    const Frame f(0x30);
    const uint32_t sp = f.sp;
    swc1(sp, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), 20.0f)));
    const float y = fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(dt(), 20.0f));
    swc1(sp + 8, 0.0f);
    swc1(sp + 4, y);
    for (int i = 0; i < 25; ++i) {
        V(sp + 0x20) = Vec4{};
        swc1(sp + 0x20, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x24, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x28, rand_range_f(-1.0f, 1.0f));
        V(sp + 0x10) = V(sp + 0x20);
        const float len = vec3_length(V(sp));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), fpu_mul(len, fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbeb33334u)), 1.0f)));
        vec3_add(V(sp + 0x10), V(sp), V(sp + 0x10));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
        const int32_t lo = frame_scale_i(5);
        const int32_t frames = rand_range(lo, frame_scale_i(10));
        const float size = rand_range_f(20000.0f, 50000.0f);
        spawn_particle_1b(m + 0x10, sp + 0x10, 0x5f7f4f2fu, static_cast<uint32_t>(frames), size);
    }
    lq_sq(sp + 0x10, m + 0x10);
    gas_2ee380_l9(m, sp + 0x10, lwc1(m + 0x48));
    moby_retire(m);
}

// Class 313, a stone: flies, falls (9.8 x dt^2), spins; gone out of the level's positive octant or 64 from the camera; its
// time (+0x1c) out: two sparks, gone; else off the world it bounces (reflected, 45 .. 75 % kept, stopped when slow).
// orig: L9:0x002ef430
void gas_class313_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x30);
    const uint32_t sp = f.sp;
    if (!move_and_check(m, pv, sp, 0.0f, false)) {
        moby_retire(m);
        return;
    }
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c))) != 0) {
        spark_out(m, sp, 2, 30000.0f);
        moby_retire(m);
        return;
    }
    if (segment_collide_v(V(sp), V(m + 0x10), 2, m, nullptr) == 0) return;
    vec3_reflect(V(pv), V(pv), coll_hit_normal());
    vec3_scale(V(pv), V(pv), rand_range_f(u2f(0x3ee66666u), 0.75f));
    if (vec3_length(V(pv)) < fpu_mul(dt_sq(), 10.0f)) vec_clear(V(pv));
    off_the_hit(m);
}

// Classes 320 / 321, the chunks: fly, fall (12.74 x dt^2), spin; gone out of the level's box (2 .. 1021) or 64 from the
// camera; their time (+0x1c) out: eight sparks, gone; else off the world: on the lava's surface (type 1) slowed hard, on
// anything else shrunk, bounced (10 .. 30 % kept; slow: its time ended), its spin turned back at random.
// orig: L9:0x002ef9b0
void gas_class320_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x30);
    const uint32_t sp = f.sp;
    if (!move_and_check(m, pv, sp, 2.0f, true)) {
        moby_retire(m);
        return;
    }
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt_sq(), u2f(0x414bd70au))));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c))) != 0) {
        spark_out(m, sp, 8, 100000.0f);
        moby_retire(m);
        return;
    }
    if (segment_collide_v(V(sp), V(m + 0x10), 2, m, nullptr) == 0) return;
    if (sub_001f35d8() == 1) {
        vec3_scale(V(pv), V(pv), u2f(0x3ecccccdu));
        const float x = fpu_mul(lwc1(pv + 0x10), 0.25f);
        const float y = fpu_mul(lwc1(pv + 0x14), 0.25f);
        swc1(pv + 0x10, x);
        swc1(pv + 0x14, y);
        return;
    }
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3f4ccccdu)));
    vec3_reflect(V(pv), V(pv), coll_hit_normal());
    vec3_scale(V(pv), V(pv), rand_range_f(u2f(0x3e99999au), u2f(0x3dcccccdu)));
    if (vec3_length(V(pv)) < fpu_mul(dt(), 5.0f)) sw(pv + 0x1c, 0);
    swc1(pv + 0x10, rand_range_f(0.0f, -lwc1(pv + 0x10)));
    swc1(pv + 0x14, rand_range_f(0.0f, -lwc1(pv + 0x14)));
    off_the_hit(m);
}

// The next build `cls` of the class 1285 `m` in its place and turn: safe from hits for 10 frames (+0xbc), the hero's colour,
// its size, its flags and its story byte (pvars +0). The moby or 0.
// orig: L9:0x00308220
uint32_t gas_308220_l9(uint32_t m, uint32_t cls) {
    const uint32_t n = moby_spawn(cls);
    if (n == 0) return 0;
    sh(n + 0x32, 0xff);
    sb(n + 0x30, 0xff);
    sb(n + 0x31, 1);
    sb(n + 0x20, 0);
    lq_sq(n + 0x10, m + 0x10);
    lq_sq(n + 0x40, m + 0x40);
    sb(n + 0xbc, static_cast<uint32_t>(frame_scale_i(10)));
    moby_update_transform(n);
    moby_copy_hero_color(n);
    swc1(n + 0x2c, lwc1(m + 0x2c));
    sh(n + 0x34, lhu(m + 0x34));
    sw(lwu(n + 0x78), lwu(lwu(m + 0x78)));
    return n;
}

// Classes 1285 .. 1288 (one update). Set up: its story byte (pvars +0) cleared. Gone when its flag byte of the level is set (its
// story byte set). Hit (mask 0x40000) while not safe (+0xbc counted down): the next build in its place (below 1288), safe again
// for 10 frames, its sound, and a burst: 40 points round its body (30 when the frame ran long, 0x15f5d0 / 0x15f5d4 over 0.85),
// thrown from it and away along the hit; the last build's chunks, a small blast every third, a big one. The last sets its story
// byte and goes; the others go when their next build came out.
// orig: L9:0x003082d8
void gas_class1285_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        if (lw(pv) != -1) sb(0x0013d388 + lwu(pv) + 0x43, 0);
        sb(m + 0x20, 1);
    }
    const uint32_t idx = lbu(m + 0xb0);
    if (idx != 0xff && lbu(0x0014c050 + idx + (lwu(0x0015ed84) << 4)) == 0xff) {
        if (lw(pv) != -1) sb(0x0013d388 + lwu(pv) + 0x43, 1);
        moby_retire(m);
        return;
    }
    const Frame f(0x80);
    const uint32_t sp = f.sp;
    uint32_t built = 0;
    const uint32_t hit = sub_0025a420(m, 0x40000u, 0);
    if (countdown_u8(m + 0xbc) == 0) return;
    if (hit == 0) return;
    const int32_t cls = lh(m + 0xa6);
    if (cls < 1288) built = gas_308220_l9(m, static_cast<uint32_t>(cls + 1)) != 0 ? 1u : 0u;
    sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(10)));
    const uint32_t at = sp + 0x20, centre = sp + 0x30, piece = sp + 0x40, vel = sp + 0x50, push = sp + 0x60;
    lq_sq(at, m + 0x10);
    lq_sq(centre, m + 0x10);
    moby_sound_start(0, 0, m);  // (the original branches on the build, below 1288 or not, to the same call)
    sb(m + 0xa4, 0xff);
    const float slow = u2f(0x3f59999au);
    const int32_t count = (slow < lwc1(L9_ADDR(0x0015f5d0)) || slow < lwc1(L9_ADDR(0x0015f5d4))) ? 0x1e : 0x28;
    float a = u2f(0xc0490fdbu);
    for (int32_t i = 0; i < count; ++i) {
        V(vel) = Vec4{};
        swc1(vel, rand_range_f(u2f(0xbecccccdu), u2f(0x3ecccccdu)));
        swc1(vel + 4, rand_range_f(u2f(0xbf19999au), u2f(0x3f19999au)));
        swc1(vel + 8, rand_range_f(u2f(0xbecccccdu), u2f(0x3ecccccdu)));
        lq_sq(piece, vel);
        const float r = rand_range_f(u2f(0x3e99999au), u2f(0x40266666u));
        swc1(piece, fpu_add(lwc1(piece), fpu_mul(r, cos_vu(a))));
        const float z = fpu_mul(r, sin_vu(a));
        swc1(piece + 8, fpu_add(lwc1(piece + 8), z));
        mat3_transform(V(piece), V(piece), M(m + 0xc0));
        vec3_add(V(piece), V(piece), V(centre));
        vec3_sub(V(vel), V(piece), V(at));
        const float k = fpu_div(abs_f(fpu_sub(6.0f, vec3_length(V(vel)))), 6.0f);
        vec3_set_length(V(push), V(hit + 0x10), fpu_mul(fpu_mul(k, k), 8.0f));
        vec3_add(V(vel), V(vel), V(push));
        vec3_set_length(V(vel), V(vel), fpu_mul(k, fpu_mul(rand_range_f(5.0f, 8.0f), dt())));
        if (lh(m + 0xa6) == 0x508) gas_chunk_l9(m, piece, vel, static_cast<uint32_t>(i) & 1, rand_range_f(1.0f, u2f(0x3fa66666u)));
        if (ee_rem(i, 3) == 0) explosion_burst(m, piece, -1, rand_range_f(u2f(0x3e99999au), u2f(0x3f19999au)), 0.0f);
        a = fpu_add(a, fpu_div(u2f(0x40c90fdbu), int_to_float(count)));
    }
    vec3_scale(V(piece), V(hit + 0x10), 0.5f);
    explosion_effect(m, hit + 0x10, at, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 2.0f, 1.0f, 1.0f, 15.0f);
    if (lh(m + 0xa6) == 0x508) {
        if (lw(pv) != -1) sb(0x0013d388 + lwu(pv) + 0x43, 1);
        moby_retire(m);
    } else if (built != 0) {
        moby_retire(m);
    }
}

}  // namespace rac1
