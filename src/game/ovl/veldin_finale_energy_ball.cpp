// Veldin's finale (level 18) class 983: a ball of energy that crackles toward a point (its pvar +0) and blows up there.
// Flying (state 1) it fizzes (three particles of type 2 a frame), throws an arc of electricity to a point that jumps
// about it (the arc's points are the level's: the code of class 655's arc on Quartu and Drek's fleet, shared_class655.cpp,
// with the ball and that point as its ends and the level's own blocks), hurts what it touches (and what stands under
// it), and shrinks away when its life runs out; reaching its point it blows up (a flash, a sound, two sprays, up to three
// sets of shards, a shake of the camera). State 0 waits out a time and is gone. Every frame it turns its two glow angles
// and queues its glow's draw (L18:0x2ea1f8, on the level's draw list) and, flying, the arc's draw (class 655's,
// L18:0x2eb9f8). Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare.
// Registered in veldin_finale_mobys.cpp; the helpers' declarations in veldin_finale_own.h.
//
// The pvars: +0 the point it flies to, +0x10 the arc's far end, +0x20 the moby its touch hurts for, +0x24 its size, +0x28
// state 0's wait, +0x2c / +0x30 its glow's angles, +0x34 its life.
//
// The level's words (L18:0x161ed0 ..): 0x161ed0 / 0x161ed4 the glow's turns (degrees a second), 0x161ed8 the arc's reach by
// size, 0x161ee0 the speed; the fizz 0x161f1c .. 0x161f68; the blast 0x161f6c .. 0x161fe8; the arc's 0x161fec .. 0x1620dc
// (as class 655's words, its four branches' halfwords from 0x1620b8 on 4 bytes further on); its points L18:0x1d9d00 ..
// (as class 655's: the main arc +0x10, its drawn points +0x150, their heights +0x290, their sideways offsets +0x2e0, the
// drawn heights +0x420, the branches +0x470, from L18:0x1d9cf0).
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
constexpr uint32_t UP = 0x0013f5e0;      // the view's up axis
constexpr int32_t POINTS = 20;           // (the main arc's)

// an EE stack frame of the original's size (its locals are passed to the callees by address, and the stale lanes they
// read are the fresh frame's, as in the transliteration)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the arc's points (the block L18:0x1d9cf0): +0x10 the main arc's twenty points, +0x150 its drawn points, +0x290 the
// points' heights, +0x2e0 their sideways offsets, +0x420 the drawn points' heights, +0x470 the four branches (five points)
uint32_t main_point(uint32_t i) { return L18_ADDR(0x001d9d00) + i * 0x10; }
uint32_t drawn_point(uint32_t i) { return L18_ADDR(0x001d9e40) + i * 0x10; }
uint32_t height(uint32_t i) { return L18_ADDR(0x001d9f80) + i * 4; }
uint32_t side_offset(uint32_t i) { return L18_ADDR(0x001d9fd0) + i * 0x10; }
uint32_t drawn_height(uint32_t i) { return L18_ADDR(0x001da110) + i * 4; }
uint32_t branch(uint32_t j) { return L18_ADDR(0x001da160) + j * 0x50; }
// the branches' halfwords: their timers, their fades, their timers' lengths, the fade's references
uint32_t branch_timer(uint32_t j) { return L18_ADDR(0x001620b8) + j * 2; }
uint32_t branch_fade(uint32_t j) { return L18_ADDR(0x001620c0) + j * 2; }
uint32_t branch_length(uint32_t j) { return L18_ADDR(0x001620c8) + j * 2; }
uint32_t branch_ref(uint32_t j) { return L18_ADDR(0x001620d0) + j * 2; }

// a random time in [lo, hi] seconds-scaled frames (frame_scale_f), as an integer
int32_t scaled_frames(float lo, float hi) { return float_to_int(frame_scale_f(rand_range_f(lo, hi))); }
// `frames` frame-scaled, times `k`, as an integer
int32_t frames_times(int32_t frames, float k) { return float_to_int(fpu_mul(static_cast<float>(frame_scale_i(frames)), k)); }
}  // namespace

// The fizz: three particles of type 2 at random points of this frame's way toward the point, drifting and sinking.
// orig: L18:0x002ea598
void vf_ball_fizz_l18(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, at = sp + 0x10, drift = sp + 0x20, vel = sp + 0x30, way = sp + 0x40;
    vec3_sub(V(way), V(lwu(m + 0x78)), V(m + 0x10));
    vec3_set_length(V(way), V(way), fpu_mul(dt(), 60.0f));
    const float spread_lo = fpu_mul(lwc1(L18_ADDR(0x00161f54)), dt());
    const float spread_hi = fpu_mul(lwc1(L18_ADDR(0x00161f58)), dt());
    const float sink_lo = fpu_mul(lwc1(L18_ADDR(0x00161f60)), dt());
    const float sink_hi = fpu_mul(lwc1(L18_ADDR(0x00161f64)), dt());
    for (int k = 0; k < 3; ++k) {
        rand_sphere_point(drift, spread_lo, spread_hi);
        vec3_scale(V(vel), V(drift), lwc1(L18_ADDR(0x00161f5c)));
        const float sink = rand_range_f(sink_lo, sink_hi);
        swc1(vel + 8, fpu_sub(lwc1(vel + 8), sink));
        const int32_t life1 = scaled_frames(lwc1(L18_ADDR(0x00161f3c)), lwc1(L18_ADDR(0x00161f40)));
        const int32_t life2 = scaled_frames(lwc1(L18_ADDR(0x00161f44)), lwc1(L18_ADDR(0x00161f48)));
        const int32_t life3 = scaled_frames(lwc1(L18_ADDR(0x00161f4c)), lwc1(L18_ADDR(0x00161f50)));
        swc1(drift + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161f2c)), lwc1(L18_ADDR(0x00161f30))));
        swc1(vel + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161f34)), lwc1(L18_ADDR(0x00161f38))));
        vec3_scale(V(at), V(way), rand_range_f(0.0f, 1.0f));
        vec3_add(V(at), V(at), V(m + 0x10));
        const uint32_t c0 = rgba_lerp(lwu(L18_ADDR(0x00161f1c)), lwu(L18_ADDR(0x00161f20)), rand_range_f(0.0f, 1.0f));
        const uint32_t c1 = rgba_lerp(lwu(L18_ADDR(0x00161f24)), lwu(L18_ADDR(0x00161f28)), rand_range_f(0.0f, 1.0f));
        spawn_particle_02(at, drift, vel, c0, c1, static_cast<uint32_t>(life1), static_cast<uint32_t>(life2), static_cast<uint32_t>(life3),
                          lw(L18_ADDR(0x00161f68)));
    }
}

// The blast at the ball: a flash, the sound 0x58e, a spray of particles of type 2 and one of type 0x0b (the level's
// counts), up to three sets of shards, and the camera's shake (the level's words).
// orig: L18:0x002ea800
void vf_ball_blast_l18(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp, vel = sp + 0x10, at = sp + 0x20, pos = m + 0x10;
    lq_sq(vel, pos);
    flash_spawn(vel, frame_scale_i(30), 0x80806060u, 0x00801010u);
    moby_sound_play(0x10, 0, m, 0x58e);
    for (int32_t k = 0; k < lw(L18_ADDR(0x00161f6c)); ++k) {
        const float pitch = rand_range_f(u2f(0xbeb2b8c2u), u2f(0x3eb2b8c2u));  // (20 degrees either way)
        const float yaw = rand_angle();
        const float speed = rand_range_f(lwc1(L18_ADDR(0x00161f70)), lwc1(L18_ADDR(0x00161f74)));
        vec_from_spherical_v(V(vel), fpu_mul(speed, dt()), yaw, pitch);
        vec3_scale(V(at), V(vel), lwc1(L18_ADDR(0x00161f94)));
        const float lift = fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161f90)), rand_range_f(0.8f, 1.2f)), dt());
        swc1(at + 0xc, lwc1(L18_ADDR(0x00161f8c)));
        swc1(vel + 0xc, lwc1(L18_ADDR(0x00161f8c)));
        swc1(at + 8, fpu_add(lwc1(at + 8), lift));
        const float k_life = rand_range_f(0.8f, 1.2f);
        const int32_t life1 = frames_times(lw(L18_ADDR(0x00161f80)), k_life);
        const int32_t life2 = frames_times(lw(L18_ADDR(0x00161f84)), k_life);
        const int32_t life3 = frames_times(lw(L18_ADDR(0x00161f88)), k_life);
        spawn_particle_02(pos, vel, at, lwu(L18_ADDR(0x00161f78)), lwu(L18_ADDR(0x00161f7c)), static_cast<uint32_t>(life1),
                          static_cast<uint32_t>(life2), static_cast<uint32_t>(life3), lw(L18_ADDR(0x00161f68)));
    }
    for (int32_t k = 0; k < lw(L18_ADDR(0x00161f98)); ++k) {
        const float speed = fpu_mul(rand_range_f(lwc1(L18_ADDR(0x00161fa0)), lwc1(L18_ADDR(0x00161fa4))), dt());
        const float pitch = rand_range_f(u2f(0x3f860a92u), u2f(0x3fb2b8c2u));  // (60 .. 80 degrees)
        const float yaw = rand_angle();
        vec_from_spherical_v(V(vel), speed, yaw, pitch);
        const float a = rand_angle();
        const float reach_x = rand_range_f(0.0f, lwc1(L18_ADDR(0x00161fbc)));
        swc1(at + 0, fpu_mul(cos_vu(a), reach_x));
        const float reach_y = rand_range_f(0.0f, lwc1(L18_ADDR(0x00161fbc)));
        swc1(at + 8, 0.0f);
        swc1(at + 4, fpu_mul(sin_vu(a), reach_y));
        vec3_add(V(at), V(at), V(pos));
        const uint32_t c0 = lwu(L18_ADDR(0x001d9c70) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const uint32_t c1 = lwu(L18_ADDR(0x001d9c88) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const float size = fpu_mul(lwc1(L18_ADDR(0x00161fa8)), 400000.0f);
        const int32_t life1 = frame_scale_i(rand_range(lw(L18_ADDR(0x00161fac)), lw(L18_ADDR(0x00161fb0))));
        const int32_t life2 = frame_scale_i(rand_range(lw(L18_ADDR(0x00161fb4)), lw(L18_ADDR(0x00161fb8))));
        spawn_particle_0b(at, vel, c0, c1, static_cast<uint32_t>(life1), static_cast<uint32_t>(life2), 0, 0, size, speed);
    }
    // the shards: three sets (a size, a life, four bytes), each when its size is above 0
    if (0.0f < lwc1(L18_ADDR(0x00161fc0)))
        shard_spawn(m, pos, L18_ADDR(0x0015f580), static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161fc4)))), lbu(L18_ADDR(0x00161fc8)),
                    lbu(L18_ADDR(0x00161fc9)), lbu(L18_ADDR(0x00161fca)), lbu(L18_ADDR(0x00161fcb)), lwc1(L18_ADDR(0x00161fc0)));
    if (0.0f < lwc1(L18_ADDR(0x00161fcc)))
        shard_spawn(m, pos, L18_ADDR(0x0015f580), static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161fd0)))), lbu(L18_ADDR(0x00161fd4)),
                    lbu(L18_ADDR(0x00161fd5)), lbu(L18_ADDR(0x00161fd6)), lbu(L18_ADDR(0x00161fd7)), lwc1(L18_ADDR(0x00161fcc)));
    if (0.0f < lwc1(L18_ADDR(0x00161fd8)))
        shard_spawn(m, pos, L18_ADDR(0x0015f580), static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161fdc)))), lbu(L18_ADDR(0x00161fe0)),
                    lbu(L18_ADDR(0x00161fe1)), lbu(L18_ADDR(0x00161fe2)), lbu(L18_ADDR(0x00161fe3)), lwc1(L18_ADDR(0x00161fd8)));
    const float shake = lwc1(L18_ADDR(0x00161fe4));
    if (0.0f < shake) {
        swc1(L18_ADDR(0x001677e0), shake);
        sw(L18_ADDR(0x001677e8), static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161fe8)))));
    }
}

// The arc started again (once, while the level's word L18:0x161fec says so): its points along the line from the ball to
// the far end, the offsets cleared, the branches' timers and lengths, the waves' phases.
// orig: L18:0x002eacd8
void vf_ball_arc_reset_l18(uint32_t m) {
    if (lwu(L18_ADDR(0x00161fec)) == 0) return;
    const Frame f(160);
    const uint32_t sp = f.sp, line = sp, step = sp + 0x10, pv = lwu(m + 0x78);
    const int32_t frames = lw(L18_ADDR(0x00162080));
    sw(L18_ADDR(0x00161fec), 0);
    sw(L18_ADDR(0x001620dc), 0);
    sw(L18_ADDR(0x001620d8), static_cast<uint32_t>(frame_scale_i(frames)));
    sw(L18_ADDR(0x00162054), 0x7f2020);
    vec3_sub(V(line), V(pv + 0x10), V(m + 0x10));
    vec3_set_length(V(step), V(line), fpu_div(lwc1(L18_ADDR(0x00161ff0)), 20.0f));
    lq_sq(main_point(0), m + 0x10);
    for (uint32_t i = 0; i < POINTS - 1; ++i) {
        vec3_add(V(main_point(i + 1)), V(main_point(i)), V(step));
        sw(height(i + 1), 0);
        vec_clear(V(side_offset(i + 1)));
        sw(drawn_height(i + 1), 0);
    }
    for (uint32_t j = 0; j < 4; ++j) sh(branch_timer(3 - j), 0xffff);
    sh(branch_length(2), 8);
    sh(branch_length(3), 4);
    sh(branch_ref(3), 2);
    sh(branch_length(0), 8);
    sh(branch_ref(0), 4);
    sh(branch_length(1), 4);
    sh(branch_ref(1), 2);
    sh(branch_ref(2), 4);
    sw(L18_ADDR(0x00162008), 0);
    sw(L18_ADDR(0x00162010), 0);
    sw(L18_ADDR(0x00162024), 0);
}

// The arc's points stepped: the main arc from the ball to the far end (+0x10) wobbling, four branches (two from each end)
// flickering, three sparks.
// orig: L18:0x002eaea0
void vf_ball_arc_l18(uint32_t m) {
    const Frame f(496);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t dir = sp, rnd = sp + 0x10, bend = sp + 0x20, drift = sp + 0x30, side = sp + 0x40, to_end = sp + 0x50;
    const uint32_t across = sp + 0x60, up = sp + 0x70, base = sp + 0x80, to_cam = sp + 0x90, jolt = sp + 0xa0;
    const uint32_t from = m + 0x10, to = pv + 0x10;
    const uint32_t bright = lbu(0x0013e533);
    swc1(L18_ADDR(0x00162008), angle_add(lwc1(L18_ADDR(0x00162008)), lwc1(L18_ADDR(0x0016200c))));
    swc1(L18_ADDR(0x00162010), angle_sub(lwc1(L18_ADDR(0x00162010)), lwc1(L18_ADDR(0x00162014))));
    swc1(L18_ADDR(0x00162024), angle_add(lwc1(L18_ADDR(0x00162024)), lwc1(L18_ADDR(0x00162028))));
    float seg = fpu_mul(vec3_distance(V(from), V(to)), 0.05f);  // (a twentieth of the arc)
    sw(L18_ADDR(0x00162054), bright != 0 ? 0x7f2040u : 0x7f2020u);
    vec3_sub(V(dir), V(to), V(from));
    vec3_set_length(V(dir), V(dir), 1.0f);
    vec3_cross_ba(V(rnd), V(dir), V(UP));
    vec_rotate_axis(V(dir), V(dir), V(rnd), u2f(0x3e32b8c2u));  // (10 degrees)
    vec3_scale(V(drift), V(dir), seg);
    lq_sq(main_point(0), from);
    lq_sq(drawn_point(0), from);
    // the main arc: each point a step from the last along a drifting direction, its height waving, toward the end
    for (int32_t i = 1; i < POINTS; ++i) {
        const uint32_t k = static_cast<uint32_t>(i);
        const uint32_t point = main_point(k), last = point - 0x10, drawn = drawn_point(k);
        const float fi = static_cast<float>(i);
        const float share = fpu_mul(int_to_float(i), 0.05f);
        const float lo = lwc1(L18_ADDR(0x00162000));
        const float span = fpu_sub(lwc1(L18_ADDR(0x00162004)), lo);
        const float phase = fpu_add(lwc1(L18_ADDR(0x00162008)), fpu_mul(fi, lwc1(L18_ADDR(0x00161ffc))));
        float wave = fpu_mul(fpu_add(lo, fpu_mul(span, share)), sin_vu(angle_wrap_mod(phase)));
        wave = fpu_add(wave, rand_range_signed_f(lwc1(L18_ADDR(0x00162040)), lwc1(L18_ADDR(0x00162044))));
        const float z = fpu_sub(lwc1(point + 8), lwc1(height(k)));
        swc1(height(k), wave);
        swc1(point + 8, z);
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(V(rnd), seg, a, b);
        swc1(rnd + 8, fpu_mul(lwc1(rnd + 8), 0.5f));
        vec3_lerp(V(bend), V(drift), V(rnd), lwc1(L18_ADDR(0x0016208c)));
        const float len = vec3_length(V(bend));
        if (len == 0.0f) {
            lq_sq(point, last);
        } else {
            vec3_scale(V(bend), V(bend), fpu_div(seg, len));
            vec3_add(V(point), V(last), V(bend));
            lq_sq(drift, bend);
        }
        swc1(point + 8, fpu_add(lwc1(point + 8), wave));
        vec3_sub(V(rnd), V(point), V(last));
        vec3_sub(V(to_end), V(to), V(point));
        const float left = vec3_length(V(to_end));
        if (left != 0.0f) {  // the drift turned toward the end, harder near it
            float pull = 0.1f;
            if (i == 19) {
                pull = 0.5f;
                seg = left;
            } else if (!(i < 16)) {
                pull = 0.5f;
                seg = fpu_div(left, int_to_float(0x13 - i));
            }
            vec3_scale(V(to_end), V(to_end), fpu_div(seg, left));
            vec3_lerp(V(drift), V(drift), V(to_end), pull);
        }
        if (i < 10) {  // the drawn point: waved up and sideways
            const float turn = angle_wrap_mod(fpu_add(lwc1(L18_ADDR(0x00162010)), fpu_mul(fi, lwc1(L18_ADDR(0x00162018)))));
            const float amp = fpu_mul(lwc1(L18_ADDR(0x0016201c)),
                                      sin_vu(angle_wrap_mod(fpu_add(lwc1(L18_ADDR(0x00162024)), fpu_mul(fi, lwc1(L18_ADDR(0x0016202c)))))));
            lq_sq(drawn, point);
            const float s = sin_vu(turn);
            const float jitter = rand_range_signed_f(0.0f, 0.1f);
            const float lift = fpu_add(fpu_mul(amp, s), jitter);
            swc1(drawn_height(k), lift);
            swc1(drawn + 8, fpu_add(lwc1(drawn + 8), lift));
            const float yaw = angle_add(atan2_f(lwc1(rnd), lwc1(rnd + 4)), u2f(0x3fc90fdbu));
            const float width = fpu_mul(lwc1(L18_ADDR(0x00162020)), cos_vu(turn));
            swc1(side, fpu_mul(cos_vu(yaw), width));
            const float sy = fpu_mul(sin_vu(yaw), width);
            swc1(side + 8, 0.0f);
            swc1(side + 4, sy);
            lq_sq(side_offset(k), side);
            vec3_add(V(drawn), V(drawn), V(side));
        } else {  // mirrored: the offsets of point 21 - i
            const uint32_t twin = 0x15 - k;
            lq_sq(drawn, point);
            swc1(drawn + 8, fpu_add(lwc1(drawn + 8), lwc1(drawn_height(twin))));
            vec3_add(V(drawn), V(drawn), V(side_offset(twin)));
            lq_sq(side, side_offset(k));
        }
    }
    // the four branches: two from the ball, two from the far end; each re-forms when its timer runs out, else flickers,
    // its fade from its timers
    vec3_cross_ba(V(across), V(dir), V(UP));
    vec3_set_length(V(across), V(across), 1.0f);
    vec3_set_length(V(up), V(UP), 1.0f);
    for (uint32_t j = 0; j < 4; ++j) {
        float reach;
        if (j < 2) {
            lq_sq(base, from);
            reach = 0.4f;
        } else {
            lq_sq(base, to);
            reach = 0.5f;
        }
        const uint32_t timer = branch_timer(j);
        const uint32_t p0 = branch(j);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(timer))) != 0) {
            sh(timer, static_cast<uint32_t>(frame_scale_i(lh(branch_length(j)))));
            sh(branch_fade(j), 0);
            const float pitch = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
            const float yaw = rand_range_f(u2f(0xbf860a92u), u2f(0x3e32b8c2u));
            vec3_scale(V(to_end), V(dir), reach);
            vec_rotate_axis(V(to_end), V(to_end), V(across), yaw);
            vec_rotate_axis(V(to_end), V(to_end), V(up), pitch);
            lq_sq(p0, base);
            vec3_add(V(p0 + 0x10), V(base), V(to_end));
            float sign = 1.0f;
            for (uint32_t q = 0; q < 3; ++q) {
                const uint32_t a = p0 + 0x10 + q * 0x10;
                vec3_sub(V(to_cam), V(a), V(L18_ADDR(0x001677c0)));  // (the camera)
                const float t = fpu_mul(rand_range_f(u2f(0x3e32b8c2u), u2f(0x3f75be0bu)), sign);
                sign = -sign;
                vec_rotate_axis(V(to_end), V(to_end), V(to_cam), t);
                vec3_add(V(a + 0x10), V(a), V(to_end));
            }
        } else {
            for (uint32_t q = 0; q < 4; ++q) {
                const uint32_t d = jolt + 0x10 + q * 0x10;
                vec3_sub(V(d), V(p0 + 0x10 + q * 0x10), V(p0 + q * 0x10));
                swc1(d, fpu_add(lwc1(d), rand_range_signed_f(0.0f, 0.1f)));
                swc1(d + 4, fpu_add(lwc1(d + 4), rand_range_signed_f(0.0f, 0.1f)));
                swc1(d + 8, fpu_add(lwc1(d + 8), rand_range_signed_f(0.0f, 0.1f)));
            }
            lq_sq(p0, base);
            for (uint32_t q = 0; q < 4; ++q) vec3_add(V(p0 + 0x10 + q * 0x10), V(p0 + q * 0x10), V(jolt + 0x10 + q * 0x10));
            const int32_t a = lh(branch_ref(j)), b = lh(timer);
            const int32_t gap = a < b ? b - a : a - b;
            const float fade = fpu_div(int_to_float(gap), int_to_float(frame_scale_i(0xf)));
            sh(branch_fade(j), static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, fade), 32.0f))));
        }
    }
    // three sparks: two at the ball along a turned direction, one at the far end
    const float pitch = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
    const float yaw = rand_range_f(u2f(0xbf060a92u), u2f(0x3f860a92u));
    vec3_scale(V(to_end), V(dir), fpu_mul(lwc1(L18_ADDR(0x00162084)), lwc1(0x0015ed60)));
    vec_rotate_axis(V(to_end), V(to_end), V(across), yaw);
    vec_rotate_axis(V(to_end), V(to_end), V(up), pitch);
    int32_t spin = rand_range(1, 8);
    if (rand_mod(2) != 0) spin = -spin;
    float size = rand_range_f(0.2f, 0.6f);
    spawn_particle_35(from, static_cast<uint32_t>(frame_scale_i(0xf)), lwu(L18_ADDR(0x00162054)) | 0x7f000000u, 0, static_cast<uint32_t>(spin),
                      to_end, fpu_mul(size, 0.3f), size, 0.0f);
    spin = -spin;
    spawn_particle_35(from, static_cast<uint32_t>(frame_scale_i(0xf)), 0x307f7f7fu, 0, static_cast<uint32_t>(spin), to_end,
                      fpu_mul(size, 0.15f), fpu_mul(size, 0.5f), 0.0f);
    spin = rand_range(1, 8);
    if (rand_mod(2) != 0) spin = -spin;
    size = rand_range_f(0.15f, 3.0f);
    spawn_particle_35(to, static_cast<uint32_t>(frame_scale_i(0xc)), lwu(L18_ADDR(0x00162054)) | 0x7f000000u, 0, static_cast<uint32_t>(spin),
                      L18_ADDR(0x0015f580), fpu_mul(size, 0.1f), size, 0.0f);
}

// orig: L18:0x002e9e70
void vf_class983_update_l18(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    const float deg = u2f(0x3c8efa35u);  // (degrees to radians)
    swc1(pv + 0x2c, angle_add(lwc1(pv + 0x2c), fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161ed0)), deg), dt())));
    swc1(pv + 0x30, angle_add(lwc1(pv + 0x30), fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161ed4)), deg), dt())));
    bool gone = false;
    switch (lbu(m + 0x20)) {
    case 0:  // the wait, then gone (0x002e9f04)
        gone = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x28))) != 0;
        break;
    case 1: {  // flying (0x002e9f1c)
        const uint32_t way = sp, below = sp + 0x10;
        const float step = fpu_mul(lwc1(L18_ADDR(0x00161ee0)), dt());
        vec3_sub(V(way), V(pv), V(pos));
        const float dist = vec3_length(V(way));
        vf_ball_fizz_l18(m);
        const float reach = fpu_mul(lwc1(L18_ADDR(0x00161ed8)), lwc1(pv + 0x24));
        rand_sphere_point(pv + 0x10, reach, reach);
        vec3_add(V(pv + 0x10), V(pv + 0x10), V(pos));
        vf_ball_arc_reset_l18(m);
        vf_ball_arc_l18(m);
        draw_callback_add(L18_ADDR(0x002eb9f8), m);
        if (dist < step) {  // there: the blast
            vf_ball_blast_l18(m);
            gone = true;
            break;
        }
        vec3_set_length(V(way), V(way), step);
        vec3_add(V(pos), V(pos), V(way));
        lq_sq(below, pos);
        swc1(below + 8, floor_probe_v(0.5f, V(below), 0));
        moby_sphere_contact(lwu(pv + 0x20), pos, 0x10001, 0, 1, 0, 2.0f, 5.0f, 1.0f);
        moby_sphere_contact(lwu(pv + 0x20), below, 0x10001, 0, 1, 0, 2.0f, 5.0f, 1.0f);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x34))) != 0) {  // its life over: it shrinks away
            approach_f(pv + 0x24, 0.0f, 0.2f);
            gone = lwc1(pv + 0x24) == 0.0f;
        }
        break;
    }
    default:
        break;
    }
    if (gone) moby_retire(m);
    else sub_001f93e0(L18_ADDR(0x002ea1f8), m);  // (its glow on the level's draw list)
}

}  // namespace rac1
