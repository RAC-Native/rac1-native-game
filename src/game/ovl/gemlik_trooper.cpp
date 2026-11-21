// Gemlik Base's (level 13) class 63: the level's hovering trooper robot, which flies along its path or about its home,
// drops to the floor and walks after its target (the hero or a moby target_select_any picks), strikes it, and comes
// home; the level's wave spawners send it out (pvar +0x20a 2); 58 on the level. Level 13 only: the addresses are
// Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp; its five
// class hooks (L13:0x2b7980 .. 0x2b7b00) stay generated.
//
// The pvars: the hit record +0x20 (+0x20 the health, +0x24 / +0x29 / +0x58 / +0x5a its words, +0x26 the knock-back's
// timer), the timer flag +0x38, the velocity +0x40 (+0x48 its z), a table pointer +0xd0, the colour pulse +0x110
// (+0x117), the knock-back / jump record +0x120 (+0x128 its z speed, +0x130 .. +0x174), the walker record +0x180 (+0x198
// the fall's speed), home +0x1d0, the goal +0x1e0 (+0x1e8 its z), the turn's speed +0x1f0, the swerve +0x1f4 (its
// angle) / +0x1f8 (its degrees), the reach +0x1fc (+0x1dc / +0x200 its two values), the walk's speed +0x204, the timers
// +0x208 (16 bits) and +0x20c (32), the swerve's timer +0x210 (32 bits on the North American and PAL 1.00 builds, 16 on
// the 1.02 build), the wave flag +0x20a, the reach's timer +0x214 (16), a guard's radius +0x216 about the moby at
// +0x218, the walking path +0x21c, the flying path +0x220 and its point +0x224, the engines' particle +0x228, the guard
// box +0x22c, the joint's node +0x230.
//
// States (the table L13:0x1f42c0, switched on the index; 9, 19 .. 98 and past the table: nothing): 0 sets up (a wave's
// robot to 1, the others 6 up to 14); 1 guards (the target near: one frame in 19 the strike's run-up, 2, or with the
// alert flag +0xbc 1, the walk, 3; far and not a wave's: 16); 2 turns from the target and warns its group; 3 walks to
// it (a path point at a time when +0x21c names a path) and strikes, 4, within 1.5 (2.3 of a class-0x10e target); 4
// strikes (the push at the animation's frame 13); 5 walks home; 6 knocked back; 7 carried (carry_update); 8 waits for
// the target within its reach; 10 jumps at home (11 the jump); 12 walks after it from home; 13 walks pushed round class
// 0 / 63 mobys; 14 hovers (its flying path's points, 18, or a random goal about home, 15); 15 flies to its goal; 16
// rises to 6 above the floor; 17 drops to the floor; 18 flies along its path; 99 dying.
//
// PAL: the 1.02 build (the PAL disc 2.00, at L13:0x2b6288) changed three things. (1) A killed robot's bolts get
// moby_drop_bolts' flag 0x200 with Ratchet at the height 248 (within 4): 1.02 Gameplay fix 25 (with class 1885's).
// (2) The swerve's timer +0x210 is a halfword there (sh, timer_tick_i16), and state 13 sets the halfword +0x212 to 1
// each frame: no reader of +0x212 on any disc (orch/w2/p13_8/NOTES.md); that build's layout is kept on its own disc
// (region_core_moved(), no number). (3) At the end of the frame, a wave's robot not drawn this frame is retired one frame
// in 29 while either of the frame's load factors is above 0.98 and its group has 3 or more live members: 1.02 Gameplay
// fix 48. The PAL 1.00 build: relocations only; the four helpers: relocations only on both.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
constexpr float kDegToRad = 0.01745329238474369f;  // (0x3c8efa35)

// CVT.W.S's integer (truncated toward zero)
int32_t trunc_w(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }

// the animation `set` started over `frames` frames (scaled) unless it is the one playing
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the swerve's timer +0x210: a word on the North American and PAL 1.00 builds, a halfword on the 1.02 build (the PAL disc
// 2.00, region_core_moved(): its `sh` and timer_tick_i16), so that each disc's memory is its own
void swerve_timer_set(uint32_t pv, int32_t frames) {
    if (region_core_moved()) sh(pv + 0x210, static_cast<uint32_t>(frames));
    else sw(pv + 0x210, static_cast<uint32_t>(frames));
}
int32_t swerve_timer_tick(uint32_t pv) { return region_core_moved() ? tick16(pv + 0x210) : tick32(pv + 0x210); }

// a new swerve: its side flipped one time in two (rand_mod(256)'s low bit), its angle, its timer 30 .. 90 frames
void swerve_new(uint32_t pv) {
    if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
    swc1(pv + 0x1f4, fpu_mul(lwc1(pv + 0x1f8), kDegToRad));
    swerve_timer_set(pv, frame_scale_i(trunc_w(rand_range_f(30.0f, 90.0f))));
}

// the yaw from the moby to (x, y)
float yaw_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the step two long along the moby's yaw at `step` (its cosine and sine each added to itself; z 0)
void set_step_twice(uint32_t m, uint32_t step) {
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(step, fpu_add(c, c));
    const float s = sin_vu(lwc1(m + 0x48));
    sw(step + 8, 0);
    swc1(step + 4, fpu_add(s, s));
}

// the point `i` of the path record `path` (+0 its count, its points from +0x10)
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the flying path's next point when the moby is within `near` of the current one (the index wrapped at the count)
void path_point_next(uint32_t pv, uint32_t path) {
    sw(pv + 0x224, static_cast<uint32_t>(lw(pv + 0x224) + 1));
    if (!(lw(pv + 0x224) < lw(path))) sw(pv + 0x224, 0);
}

// a random goal about home (up to `radius` across, 5 .. 7 up) over a floor (higher than 1): ten tries at most, the
// offset at `off`
void goal_about_home(uint32_t pv, uint32_t off, float radius) {
    for (int32_t i = 0; i < 10; ++i) {
        const float r = rand_range_f(0.0f, radius);
        const float a = rand_angle();
        swc1(off, fpu_mul(cos_vu(a), r));
        const float s = fpu_mul(sin_vu(a), r);
        swc1(off + 8, 0.0f);
        swc1(off + 4, s);
        swc1(off + 8, rand_range_f(5.0f, 7.0f));
        vec3_add(V(pv + 0x1e0), V(off), V(pv + 0x1d0));
        if (1.0f < floor_probe_v(0.5f, V(pv + 0x1e0), 0x120)) return;
    }
}

// how a state's frame ends (the original's shared tails)
enum class Tail {
    Pulse,     // the colour pulse, the space's bounds, the 1.02 thinning (L13:0x2b789c)
    Particle,  // the engines' particle (0x2b4f28), then Pulse (0x2b6344)
    Floor,     // onto the floor (0x2b4ff0), then Pulse (0x2b7004)
    Retire,    // retired (0x2b73b8)
    Done,      // nothing more (0x2b793c)
};

// 1.02 Gameplay, fix 48 (the 1.02 build's L13:0x2b8af8 .. 0x2b8ba4, after the space's bounds): a wave's robot (+0x20a 2)
// while either load factor (0x15f5d4, then 0x15f5d0) is above 0.98, one frame in 29 (rand_mod(29)), not drawn this
// frame (+0x31 0): rand() % 3, or its group's live members (moby_group_count) for a group other than 0xff, 3 or more
// retire it
void load_thinning(uint32_t m, uint32_t pv) {
    if (lh(pv + 0x20a) != 2) return;
    const float limit = u2f(0x3f7ae148u);  // (0.98)
    if (!(limit < lwc1(OV(0x0015f5d4))) && !(limit < lwc1(OV(0x0015f5d0)))) return;
    if (rand_mod(0x1d) != 0) return;
    if (lbu(m + 0x31) != 0) return;
    int32_t n = ee_rem(libc_rand(), 3);
    if (lbu(m + 0x21) != 0xff) n = moby_group_count_l1(static_cast<int32_t>(lbu(m + 0x21)), -1);
    if (n >= 3) moby_retire(m);
}
}  // namespace

// The engines' particle (type 0x44) on the moby's chain point `joint`: its colour words `c0` / `c1`, its flags `flags`,
// its sizes `s0` / `s1` and its rate `rate`; the particle, or 0 when the pool is full.
// orig: L13:0x00280400
uint32_t gml_280400_l13(uint32_t m, uint32_t joint, uint32_t c0, uint32_t c1, uint32_t flags, float s0, float s1, float rate) {
    joint &= 0xff;
    flags &= 0xff;
    const uint32_t p = alloc_particle(0x44);
    if (p == 0) return 0;
    sw(p + 4, c0);
    sw(p + 0xc, c1);
    const int32_t k = float_to_int(u2f(0x3ecccccdu));  // (0.4)
    sb(p + 3, 0x48);
    sb(p + 1, 3);
    sb(p + 9, static_cast<uint32_t>(((static_cast<int32_t>(lh(m + 0x32)) >> 5) << 4) + k));
    sb(p + 2, lbu(lwu(L13_ADDR(0x001b2490))));
    moby_chain_point_v(m, joint, V(p + 0x10));
    lq_sq(p + 0x20, p + 0x10);
    swc1(p + 0x2c, s1);
    swc1(p + 0x1c, s0);
    sw(p + 0x30, m);
    sb(p + 0x35, flags);
    sh(p + 0x36, lhu(m + 0xa6));
    sb(p + 0x34, joint);
    swc1(p + 0x38, s0);
    swc1(p + 0x3c, rate);
    return p;
}

// The engines' particle kept while the moby is drawn (+0x31), freed while it is not.
// orig: L13:0x002b4f28
void gml_2b4f28_l13(uint32_t m, uint32_t pv) {
    if (lbu(m + 0x31) != 0) {
        if (lwu(pv + 0x228) == 0) {
            const float s = u2f(0x3e4ccccdu);  // (0.2)
            sw(pv + 0x228, gml_280400_l13(m, 0, 0x80808080u, 0x10808080u, 0x19, s, s, u2f(0x3f4ccccdu)));  // (0.8)
        }
    } else if (lwu(pv + 0x228) != 0) {
        free_particle(lwu(pv + 0x228));
        sw(pv + 0x228, 0);
    }
}

// The engines' particle freed.
// orig: L13:0x002b4fb8
void gml_2b4fb8_l13(uint32_t pv) {
    if (lwu(pv + 0x228) == 0) return;
    free_particle(lwu(pv + 0x228));
    sw(pv + 0x228, 0);
}

// Onto the floor: more than 0.3 above it the moby falls (gravity 9.8 on the record's z speed +0x128) down to it at most;
// within 0.3 of it, it rises 9.8 times the squared frame time while below it, and the record +0x120 is cleared.
// orig: L13:0x002b4ff0
void gml_2b4ff0_l13(uint32_t m, uint32_t pv) {
    const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
    const float z = lwc1(m + 0x18);
    if (floor < fpu_add(z, u2f(0x3e99999au))) {  // (0.3)
        const float vz = fpu_sub(lwc1(pv + 0x128), fpu_mul(dt_sq(), u2f(0x411ccccdu)));  // (9.8)
        swc1(pv + 0x128, vz);
        const float nz = fpu_add(lwc1(m + 0x18), vz);
        swc1(m + 0x18, nz);
        if (nz < floor) swc1(m + 0x18, floor);
        return;
    }
    if (z < floor) swc1(m + 0x18, fpu_add(z, fpu_mul(dt_sq(), u2f(0x411ccccdu))));
    sq_zero(pv + 0x120);
}

// The update (see the file's head). The original's frame (384 bytes): sp the moby's place at the frame's start (16),
// sp+0x10 the target's record (target_select_any writes 0x48 bytes: +0 its point, +0x40 the moby, at sp+0x50), sp+0x60,
// +0x70, +0x80 work vectors (16 each: walker_record_step reads sp+0x60 and its point sp+0x70, path_next_point writes
// sp+0x70, sub_00259bc8 reads sp+0x60 / +0x70), sp+0x90 / +0x94 the hit's state and damage words (moby_hit_react_r's, read
// back), sp+0x98 the knock-back's yaw (vec_yaw_marked_v's), sp+0x9c a speed (approach_f's), sp+0xa0 sub_0025abf0's time
// word, sp+0xa4 the flying path's record.
// orig: L13:0x002b50d8
void gml_class63_update_l13(uint32_t m) {
    const Frame f(0x180);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, home = pv + 0x1d0, goal = pv + 0x1e0, vel = pv + 0x40, pulse = pv + 0x110;
    const uint32_t target = sp + 0x10, target_moby = sp + 0x50;
    const uint32_t v60 = sp + 0x60, v70 = sp + 0x70, v80 = sp + 0x80;

    joint_node_scale(m, 0, pv + 0x230, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L13_ADDR(0x001670c0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    if (lw(L13_ADDR(0x0015f5c4)) == 2) {  // (a scripted scene: drawn and solid, nothing else)
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        return;
    }
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    target_select_any(m, target, fpu_add(lwc1(pv + 0x1fc), 4.0f));
    // the target moby: none, or out of the guard box while the reach's timer is off: no target (its point zero)
    uint32_t tm = lwu(target_moby);
    if (tm == 0 || (lh(pv + 0x214) == 0 && lw(pv + 0x22c) != -1 && point_in_box(target, lw(pv + 0x22c)) == 0)) {
        sw(target_moby, 0);
        tm = 0;
        sq_zero(target);
    }
    lq_sq(sp, pos);
    if (lw(pv + 0x38) != 0) sh(pv + 0x214, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    tick16(pv + 0x208);
    if (lbu(m + 0x20) != 0) swc1(pv + 0x1fc, tick16(pv + 0x214) == 0 ? lwc1(pv + 0x200) : lwc1(pv + 0x1dc));

    // the hit (the mask 0x330000): knocked back (6), or killed (0x63) when the damage takes the health to 0 or below
    const uint32_t hit_state = sp + 0x90, hit_dmg = sp + 0x94;
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000), 0);
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    tick16(pv + 0x26);
    if (lh(pv + 0x26) < frame_scale_i(0x2d) && hit != 0 && lbu(m + 0x20) != 0x63) {
        if (lhu(lwu(hit + 0x20) + 0xa6) - 0xb0u < 2u && lh(pv + 0x26) != 0) swc1(hit + 0x2c, 0.0f);  // (0xb0 / 0xb1 in the knock-back)
        if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
        if (lwc1(hit + 0x2c) != 0.0f) {
            const uint32_t yaw = sp + 0x98;
            swc1(yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(target)), fpu_sub(lwc1(m + 0x14), lwc1(target + 4))));
            swc1(pv + 0x148, 0.5f);
            sw(pv + 0x140, 0x202);
            const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit + 0x2c));
            const float t = dt();
            swc1(pv + 0x130, u2f(0x3bb43958u));  // (0.0055)
            swc1(pv + 0x134, u2f(0x3a902de0u));  // (0.0011)
            swc1(pv + 0x138, fpu_mul(lwc1(L13_ADDR(0x00161394)), t));
            swc1(pv + 0x13c, fpu_mul(lwc1(L13_ADDR(0x00161390)), t));
            sw(pv + 0x144, 9);
            swc1(pv + 0x20, health);
            sb(pv + 0x15d, 0);
            const bool killed = health <= 0.0f;
            if (killed) {
                swc1(pv + 0x138, fpu_mul(t, 10.0f));
                swc1(pv + 0x13c, fpu_mul(t, 5.5f));
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            }
            lq_sq(v60, hit + 0x10);
            vec_yaw_marked_v(V(v60), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
            const float ang = lwc1(yaw);
            move_state_start(m, pv + 0x120, static_cast<uint32_t>(libc_rand() & 1) + 9, 1, 0, ang);
            if (killed) {
                swc1(pv + 0x170, 11.0f);
                swc1(pv + 0x174, 18.0f);
                sb(m + 0x20, 0x63);
                gml_2b4fb8_l13(pv);
                sb(pv + 0x117, 0x78);
                color_pulse_start(m, pulse);
                // enhancement: 1.02 Gameplay, fix 25 (fixes102.h): with Ratchet at the height 248, the bolts' flag 0x200
                const bool to_hero = gameplay102_gemlik_bolts_flag() && abs_f(fpu_sub(lwc1(0x0013f3d8), 248.0f)) < 4.0f;
                moby_drop_bolts(m, to_hero ? 0x200u : 0u, -1);
            } else {
                swc1(pv + 0x170, 5.0f);
                swc1(pv + 0x174, 10.0f);
                sb(m + 0x20, 6);
                gml_2b4fb8_l13(pv);
                sb(pv + 0x117, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pulse);
            }
        }
    }
    sb(m + 0xa4, 0xff);

    // the target's height off the moby's (the walks' and strikes' tests) and the distance to it across
    auto target_dz = [&] { return abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))); };
    // the walking path's next point toward `to` at sp+0x70 (or `to` itself without a path)
    auto walk_point = [&](uint32_t to) {
        if (lw(pv + 0x21c) != -1) path_next_point_l1(m, lvd::path(lw(pv + 0x21c)), to, v70);
        else lq_sq(v70, to);
    };
    // the floor's fall (9.8 on +0x198) onto the floor below
    auto fall_to_floor = [&] {
        const float floor = floor_probe_v(0.5f, V(pos), 0x120);
        const float vz = fpu_sub(lwc1(pv + 0x198), fpu_mul(dt_sq(), u2f(0x411ccccdu)));  // (9.8)
        swc1(pv + 0x198, vz);
        const float z = fpu_add(lwc1(m + 0x18), vz);
        swc1(m + 0x18, z);
        if (z < floor) {
            sw(pv + 0x198, 0);
            swc1(m + 0x18, floor);
        }
    };

    auto run_state = [&]() -> Tail {
        switch (lbu(m + 0x20)) {
        case 0: {  // (0x2b5560) set up
            lq_sq(home, pos);
            swc1(pv + 0x1dc, lwc1(pv + 0x1fc));
            if (lh(pv + 0x20a) != 0) {
                sb(m + 0x20, 1);
                if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, 0);
            } else {
                sb(m + 0x20, 0xe);
                swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 6.0f));
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            }
            sh(pv + 0x214, 0);
            sh(pv + 0x208, 0);
            sb(pv + 0x29, 0);
            swc1(pv + 0x20, 1.0f);
            sh(pv + 0x24, 1);
            sb(pv + 0x58, 8);
            sb(pv + 0x5a, 4);
            walker_record_init_l1(pv + 0x180);
            sw(pv + 0x180, 0x38d);
            swc1(pv + 0x188, 2.0f);
            swc1(pv + 0x1a4, fpu_mul(lwc1(pv + 0x204), dt()));
            sw(pv + 0x1b8, lwu(pv + 0x1b8) | 0x30u);
            swc1(pv + 0x18c, 2.0f);
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
            sw(pv + 0xd0, L13_ADDR(0x001613a0));
            if (lw(pv + 0x220) != -1) sw(pv + 0x224, static_cast<uint32_t>(path_nearest_l1(pos, lvd::path(lw(pv + 0x220)), 0.0f)));
            return Tail::Pulse;
        }
        case 1: {  // (0x2b6354) guard
            vec3_sub(V(v60), V(target), V(pos));
            if (vec3_length(V(v60)) < fpu_add(lwc1(pv + 0x1fc), 4.0f) && target_dz() < 8.0f && rand_mod(0x13) == 0) {
                sb(m + 0x20, 2);
                if (lbu(m + 0x53) == 4) return Tail::Floor;
                moby_anim_set(m, 4, 0, frame_scale_i(3));
                return Tail::Floor;
            }
            if (lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) {
                swerve_new(pv);
                sb(m + 0x20, 3);
                anim_to(m, 5, 3);
                sb(m + 0xbc, 0);
                return Tail::Floor;
            }
            if (!(fpu_add(lwc1(pv + 0x1fc), 6.0f) < vec3_length(V(v60)))) return Tail::Floor;
            if (lh(pv + 0x20a) != 0) return Tail::Floor;
            sq_zero(vel);
            sb(m + 0x20, 0x10);
            if (lbu(m + 0x53) == 0) return Tail::Floor;
            moby_anim_set(m, 0, 0, frame_scale_i(3));
            return Tail::Floor;
        }
        case 2: {  // (0x2b68a0) turn away from the target; its group warned, then the walk (3)
            yaw_turn_damped(m, pv + 0x1f0, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(target)), fpu_sub(lwc1(m + 0x14), lwc1(target + 4))),
                            u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));  // (0.02, 0.3, 0.1)
            vec3_sub(V(v60), V(target), V(pos));
            bool walk = false;
            if (vec3_length(V(v60)) < lwc1(pv + 0x1fc) && target_dz() < 8.0f && rand_mod(0x13) == 0) {
                if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
                walk = true;
            } else if (lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) {
                walk = true;
            }
            if (walk) {
                swerve_new(pv);
                sb(m + 0x20, 3);
                anim_to(m, 5, 3);
                sb(m + 0xbc, 0);
            }
            if (lbu(m + 0x53) != 4) return Tail::Floor;
            if ((lbu(m + 0x70) & 2) == 0) return Tail::Floor;
            moby_anim_set(m, 3, 0, frame_scale_i(3));
            return Tail::Floor;
        }
        case 3: {  // (0x2b6a74) walk to the target (swerving), strike it near (4)
            const float slope = fpu_div(target_dz(), vec2_distance(V(pos), V(target)));
            walk_point(target);
            yaw_turn_damped(m, pv + 0x1f0, fpu_add(yaw_to(m, lwc1(v70), lwc1(v70 + 4)), lwc1(pv + 0x1f4)), u2f(0x3d4ccccdu),
                            u2f(0x3e99999au), u2f(0x3e4ccccdu));  // (0.05, 0.3, 0.2)
            set_step_twice(m, v60);
            const uint32_t step = walker_record_step(1.0f, m, pv + 0x180, v60, v70);
            if (swerve_timer_tick(pv) != 0) {
                swerve_timer_set(pv, frame_scale_i(trunc_w(rand_range_f(30.0f, 90.0f))));
                swc1(pv + 0x1f4, -lwc1(pv + 0x1f4));
            }
            vec3_sub(V(v80), V(target), V(home));
            const float d = vec2_distance(V(pos), V(target));
            bool strike = d < 1.5f;
            if (!strike) {
                const uint32_t tmb = lwu(target_moby);
                strike = tmb != 0 && lh(tmb + 0xa6) == 0x10e && lbu(tmb + 0x20) != 0xfe && lbu(tmb + 0x20) != 0xfd && d < u2f(0x40133333u);  // (2.3)
            }
            if (strike && (step & 2) == 0) {
                sb(m + 0x20, 4);
                if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, frame_scale_i(3));
                sb(m + 0xbc, 0);
                return Tail::Floor;
            }
            if (1.0f < slope || (step & 2) != 0) {  // (too steep, or the walker blocked: back where it was)
                swc1(m + 0x10, lwc1(sp));
                swc1(m + 0x14, lwc1(sp + 4));
            }
            if (vec3_length(V(v80)) < fpu_add(lwc1(pv + 0x1fc), 4.0f) && target_dz() < 8.0f) {
                if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
            } else if (lbu(m + 0xbc) != 1) {
                sb(m + 0x20, 5);
            }
            sb(m + 0xbc, 0);
            return Tail::Floor;
        }
        case 4: {  // (0x2b6d54) strike: the push on the target at the animation's frame 13, facing it
            const float rate = u2f(0x3e4ccccdu);  // (0.2)
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(target), lwc1(target + 4)), u2f(0x3d4ccccdu), u2f(0x3e99999au), rate);
            const bool settled = lbu(m + 0x52) == lbu(m + 0x53);
            const float t = moby_anim_time(m);
            const float dz = target_dz();
            const float d = vec2_distance(V(pos), V(target));
            const float off = angle_dist(yaw_to(m, lwc1(target), lwc1(target + 4)), lwc1(m + 0x48));
            if (settled && abs_f(fpu_sub(t, 13.0f)) < rate && dz < 1.5f) {
                bool reach = d < 2.0f;
                if (!reach) {
                    const uint32_t tmb = lwu(target_moby);
                    reach = tmb != 0 && lh(tmb + 0xa6) == 0x10e && lbu(tmb + 0x20) != 0xfe && lbu(tmb + 0x20) != 0xfd &&
                            d < u2f(0x40266666u);  // (2.6)
                }
                if (reach && off < u2f(0x3e860a92u)) {  // (15 degrees)
                    swc1(v60, fpu_mul(cos_vu(lwc1(m + 0x48)), rate));
                    const float s = fpu_mul(sin_vu(lwc1(m + 0x48)), rate);
                    sw(v60 + 8, 0);
                    swc1(v60 + 4, s);
                    lq_sq(v70, target);
                    swc1(v70 + 8, fpu_add(lwc1(v70 + 8), 0.75f));
                    if (tm != 0) sub_00259bc8(tm, m, 1, v70, v60, 1.0f);
                }
            }
            if ((lbu(m + 0x70) & 2) != 0 && 1.5f < vec2_distance(V(pos), V(target))) {
                sb(m + 0x20, 3);
                anim_to(m, 5, 3);
            }
            fall_to_floor();
            return Tail::Floor;
        }
        case 5: {  // (0x2b7014) walk home
            walk_point(home);
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(v70), lwc1(v70 + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
            set_step_twice(m, v60);
            const uint32_t step = walker_record_step(1.0f, m, pv + 0x180, v60, v70);
            if (vec2_distance(V(pos), V(home)) < 2.0f) {
                sb(m + 0x20, 1);
                anim_to(m, 3, 3);
            }
            vec3_sub(V(v80), V(target), V(home));
            bool walk = false;
            if (vec3_length(V(v80)) < lwc1(pv + 0x1fc)) {
                if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
                walk = true;
            } else if (lbu(m + 0xbc) == 1) {
                walk = true;
            }
            if (walk) {
                sb(m + 0x20, 3);
                anim_to(m, 5, 3);
            } else if ((step & 2) != 0) {  // (blocked: back where it was, the walk beside home)
                swc1(m + 0x10, lwc1(sp));
                swc1(m + 0x14, lwc1(sp + 4));
                sb(m + 0x20, 0xc);
                anim_to(m, 4, 3);
            }
            gml_2b4ff0_l13(m, pv);
            sb(m + 0xbc, 0);
            return Tail::Pulse;
        }
        case 6:    // (0x2b7244) knocked back: landed, the walk (3)
        case 11: {  // (0x2b67ac) the jump: landed, the walk (3)
            const bool jump = lbu(m + 0x20) == 11;
            if ((enemy_physics(m, pv + 0x120) & 1) != 0) {
                if (jump) gml_2b4fb8_l13(pv);
                sq_zero(pv + 0x120);
                sb(m + 0x20, 3);
                if (jump) {
                    swerve_new(pv);
                    if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 8, frame_scale_i(3));
                } else {
                    anim_to(m, 5, 3);
                }
                return Tail::Done;
            }
            if (lwc1(m + 0x18) < 5.0f) {  // (fallen off the level)
                moby_drop_bolts(m, 0, -1);
                return Tail::Retire;
            }
            return Tail::Pulse;
        }
        case 7: {  // (0x2b72cc) carried: let go, 14
            gml_2b4fb8_l13(pv);
            if (carry_update(m, pv + 0x120) == 0) return Tail::Pulse;
            sb(m + 0x20, 0xe);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            sh(pv + 0xc8, 0);
            return Tail::Pulse;
        }
        case 8: {  // (0x2b6528) wait for the target within its reach less 4 (one frame in 19): the run-up (2)
            vec3_sub(V(v60), V(target), V(pos));
            if (!(vec3_length(V(v60)) < fpu_sub(lwc1(pv + 0x1fc), 4.0f))) return Tail::Pulse;
            if (!(target_dz() < 8.0f)) return Tail::Pulse;
            if (rand_mod(0x13) != 0) return Tail::Pulse;
            sb(m + 0x20, 2);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
            anim_to(m, 4, 3);
            return Tail::Pulse;
        }
        case 10: {  // (0x2b65ec) turn to home and jump at it (11) when the target is in reach of home, or alerted (+0xbc 2)
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(home), lwc1(home + 4)), u2f(0x3d4ccccdu), u2f(0x3e99999au), u2f(0x3e4ccccdu));
            vec3_sub(V(v60), V(target), V(home));
            const bool in_reach = vec3_length(V(v60)) < lwc1(pv + 0x1fc) && abs_f(fpu_sub(lwc1(home + 8), lwc1(target + 8))) < 8.0f;
            if (!in_reach && lbu(m + 0xbc) != 2) return Tail::Pulse;
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 2);
            const uint32_t playing = lbu(m + 0x53);
            if (playing != static_cast<uint32_t>((libc_rand() & 1) + 9)) {
                const uint32_t set = static_cast<uint32_t>((libc_rand() & 1) + 9);
                moby_anim_set(m, set, 0, frame_scale_i(3));
            }
            const float speed = fpu_mul(lwc1(L13_ADDR(0x00161398)), dt());
            const float lift = lwc1(L13_ADDR(0x0016139c));
            sw(pv + 0x134, 0);
            swc1(pv + 0x130, lift);
            swc1(pv + 0x138, speed);
            float t = lwc1(sp + 0xa0);
            const float vz = sub_0025abf0(pos, home, &t, speed, -lift);
            swc1(sp + 0xa0, t);
            sb(pv + 0x15d, 0);
            swc1(pv + 0x13c, vz);
            sw(pv + 0x144, 0xd);
            move_state_start(m, pv + 0x120, static_cast<uint32_t>(libc_rand() & 1) + 9, 1, 0, lwc1(m + 0x48));
            swc1(pv + 0x170, 5.0f);
            swc1(pv + 0x174, 10.0f);
            gml_2b4f28_l13(m, pv);
            sb(m + 0x20, 0xb);
            return Tail::Pulse;
        }
        case 12: {  // (0x2b73c8) walk after the target from home, strike it near (4)
            const float slope = fpu_div(target_dz(), vec2_distance(V(pos), V(target)));
            walk_point(home);
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(v70), lwc1(v70 + 4)), u2f(0x3d4ccccdu), u2f(0x3e99999au), u2f(0x3e4ccccdu));
            set_step_twice(m, v60);
            const uint32_t step = walker_record_step(1.0f, m, pv + 0x180, v60, v70);
            vec3_sub(V(v80), V(target), V(home));
            if (vec2_distance(V(pos), V(target)) < 1.5f && slope <= 1.0f) {
                sb(m + 0x20, 4);
                if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, frame_scale_i(3));
                sb(m + 0xbc, 0);
                return Tail::Pulse;
            }
            if (slope <= 1.0f && (step & 2) == 0) {
                sb(m + 0x20, 5);
                if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(3));
                sb(m + 0xbc, 0);
                return Tail::Pulse;
            }
            if ((lbu(m + 0x70) & 2) != 0) {  // (the animation over: 3 or, one time in four, 4)
                rand_mod(4);
                const uint32_t set = rand_mod(4) != 0 ? 3u : 4u;
                moby_anim_set(m, set, 0, frame_scale_i(3));
            }
            if (rand_mod(0x63) != 0) {
                sb(m + 0xbc, 0);
                return Tail::Pulse;
            }
            sq_zero(vel);
            sb(m + 0x20, 0x10);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
            sb(m + 0xbc, 0);
            return Tail::Pulse;
        }
        case 13: {  // (0x2b7630) walk ahead pushed round the class-0 / 63 mobys in the way, back to 5 out of the guard's radius
            if (lbu(m + 0x52) != 5 && lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(3));
            if (region_core_moved()) sh(pv + 0x212, 1);  // (the 1.02 build's layout: NOTES.md, no reader)
            {
                const float c = cos_vu(lwc1(m + 0x48));
                swc1(v60, fpu_mul(c, fpu_mul(lwc1(pv + 0x204), dt())));
                const float s = sin_vu(lwc1(m + 0x48));
                sw(v60 + 8, 0);
                swc1(v60 + 4, fpu_mul(s, fpu_mul(lwc1(pv + 0x204), dt())));
            }
            vec3_add(V(v70), V(pos), V(v60));
            for (uint32_t o = lwu(L13_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
                if (o == m || static_cast<int8_t>(lbu(o + 0x20)) < 0) continue;
                const int32_t cls = lh(o + 0xa6);
                if (cls != 0 && cls != 0x3f) continue;
                if (!(vec2_distance(V(v70), V(o + 0x10)) < 2.0f)) continue;
                const float z = lwc1(m + 0x18);
                const float side = angle_sub(lwc1(m + 0x48), yaw_to(m, lwc1(o + 0x10), lwc1(o + 0x14)));
                float turn = fpu_mul(dt(), u2f(0x3fc90fdbu));  // (a quarter turn a second)
                if (!(0.0f < side)) turn = -turn;
                swc1(m + 0x48, angle_add(lwc1(m + 0x48), turn));
                vec3_sub(V(v70), V(pos), V(o + 0x10));
                vec3_set_length(V(v70), V(v70), 2.0f);
                vec3_add(V(v70), V(v70), V(o + 0x10));
                swc1(v70 + 8, z);
            }
            lq_sq(pos, v70);
            fall_to_floor();
            const uint32_t guard = lwu(pv + 0x218);
            if (guard != 0 && !(int_to_float(lh(pv + 0x216)) < vec2_distance(V(pos), V(guard + 0x10)))) {
                sb(m + 0xbc, 0);
                return Tail::Pulse;
            }
            sb(m + 0x20, 5);
            anim_to(m, 5, 3);
            sb(m + 0xbc, 0);
            return Tail::Pulse;
        }
        case 14: {  // (0x2b56ac) hover: the target near (one frame in 19: 17), alerted (one frame in 4: 17), its flying path (18), or
                    // now and then a random goal about home (15)
            gml_2b4f28_l13(m, pv);
            vec3_sub(V(v60), V(target), V(pos));
            const float floor = floor_probe_v(0.5f, V(pos), 0x120);
            if (1.0f < floor) {
                if (vec2_length(V(v60)) < fpu_add(lwc1(pv + 0x1fc), 4.0f) && target_dz() < 10.0f && rand_mod(0x13) == 0) {
                    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                    sb(m + 0x20, 0x11);
                    gml_2b4fb8_l13(pv);
                    return Tail::Pulse;
                }
                if (lbu(m + 0xbc) == 1 && rand_mod(4) == 0) {
                    swerve_new(pv);
                    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                    gml_2b4fb8_l13(pv);
                    sb(m + 0x20, 0x11);
                    vec_clear(V(vel));
                    sb(m + 0xbc, 0);
                    return Tail::Pulse;
                }
            }
            if (lw(pv + 0x220) != -1) {
                const uint32_t path = lvd::path(lw(pv + 0x220));
                if (vec3_distance(V(pos), V(path_point(path, lw(pv + 0x224)))) < 0.25f) path_point_next(pv, path);
                lq_sq(goal, path_point(path, lw(pv + 0x224)));
                sb(m + 0x20, 0x12);
                return Tail::Pulse;
            }
            if (tick32(pv + 0x20c) != 0) {
                sw(pv + 0x20c, static_cast<uint32_t>(frame_scale_i(rand_range(0x28, 0x78))));
                goal_about_home(pv, v70, 4.0f);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sb(m + 0x20, 0xf);
            }
            return Tail::Pulse;
        }
        case 15: {  // (0x2b5a30) fly to the goal, its speed eased (blocked or there: 14)
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(goal), lwc1(goal + 4)), u2f(0x3cf5c28fu), u2f(0x3f19999au), u2f(0x3db851ecu));  // (0.03, 0.6, 0.09)
            const uint32_t speed = sp + 0x9c;
            swc1(speed, vec3_length(V(vel)));
            swc1(m + 0x58, fpu_add(fpu_mul(lwc1(speed), 20.0f), 1.0f));
            vec3_sub(V(vel), V(goal), V(pos));
            if (1.0f < vec3_length(V(vel))) approach_f(speed, fpu_mul(dt(), 4.0f), fpu_mul(dt_sq(), 6.0f));
            else approach_f(speed, dt(), fpu_mul(dt_sq(), 8.0f));
            vec3_clamp_length(vel, lwc1(speed));
            vec3_add(V(v60), V(vel), V(pos));
            if (sphere_collide(v60, u2f(0x3f99999au), 0, m) != 0) {  // (1.2)
                vec_clear(V(vel));
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sb(m + 0x20, 0xe);
            } else {
                lq_sq(pos, v60);
            }
            if (vec3_distance(V(pos), V(goal)) < lwc1(speed) || lwc1(speed) == 0.0f) {
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sb(m + 0x20, 0xe);
            }
            return Tail::Particle;
        }
        case 16: {  // (0x2b6214) rise to 6 above the floor (then 14)
            const float floor = floor_probe_v(0.5f, V(pos), 0x120);
            if (fpu_sub(lwc1(m + 0x18), floor) < 3.0f) approach_f(pv + 0x48, fpu_mul(dt(), 8.0f), fpu_mul(dt_sq(), u2f(0x412ccccdu)));  // (10.8)
            else approach_f(pv + 0x48, dt(), fpu_mul(dt_sq(), u2f(0x412ccccdu)));
            walker_move(m, vel, 0, u2f(0x3f19999au), 0.5f, 0.0f);  // (0.6)
            const float z = lwc1(m + 0x18);
            const float top = fpu_add(floor, 6.0f);
            swc1(m + 0x58, fpu_sub(6.0f, fpu_mul(fpu_div(fpu_sub(z, floor), 6.0f), 5.0f)));
            if (top <= z) {
                swc1(m + 0x18, top);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sb(m + 0x20, 0xe);
                vec_clear(V(vel));
            }
            return Tail::Particle;
        }
        case 17: {  // (0x2b6078) drop to the floor (the landing animation near it), then the walk (3)
            const float floor = floor_probe_v(0.5f, V(pos), 0x120);
            approach_f(pv + 0x48, -fpu_mul(dt(), 8.0f), fpu_mul(dt_sq(), u2f(0x412ccccdu)));  // (10.8)
            walker_move(m, vel, 0, u2f(0x3f19999au), 0.5f, 0.0f);
            if (lbu(m + 0x53) != 2) {
                const float above = fpu_sub(lwc1(m + 0x18), floor);
                const float k = frame_scale_f(45.0f);
                if (above <= fpu_mul(fpu_mul(dt(), 8.0f), k) && lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(5));
            }
            if (!(lwc1(m + 0x18) <= floor)) return Tail::Pulse;
            swc1(m + 0x18, floor);
            swerve_new(pv);
            sb(m + 0x20, 3);
            vec_clear(V(vel));
            anim_to(m, 5, 3);
            return Tail::Pulse;
        }
        case 18: {  // (0x2b5c28) fly along the flying path (blocked: a random goal about home, 15), the target near or alerted: 17
            const uint32_t path = lvd::path(lw(pv + 0x220));
            sw(sp + 0xa4, path);
            yaw_turn_damped(m, pv + 0x1f0, yaw_to(m, lwc1(goal), lwc1(goal + 4)), u2f(0x3c9374bcu), u2f(0x3f4ccccdu), u2f(0x3db851ecu));  // (0.018, 0.8, 0.09)
            vec3_sub(V(vel), V(goal), V(pos));
            vec3_clamp_length(vel, fpu_mul(dt(), 4.0f));
            vec3_add(V(v60), V(vel), V(pos));
            swc1(m + 0x58, fpu_add(fpu_mul(vec3_length(V(vel)), 20.0f), 1.0f));
            if (sphere_collide(v60, u2f(0x3f99999au), 0, m) != 0) {  // (1.2)
                sw(pv + 0x20c, static_cast<uint32_t>(frame_scale_i(0x78)));
                goal_about_home(pv, v70, 3.0f);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sb(m + 0x20, 0xf);
            } else {
                lq_sq(pos, v60);
            }
            if (!(vec3_distance(V(pos), V(goal)) < vec3_length(V(vel)))) return Tail::Particle;
            path_point_next(pv, lwu(sp + 0xa4));
            lq_sq(goal, path_point(lwu(sp + 0xa4), lw(pv + 0x224)));
            swc1(pv + 0x1e8, fpu_add(lwc1(pv + 0x1e8), rand_range_f(u2f(0xbe4ccccdu), u2f(0x3e4ccccdu))));  // (-0.2 .. 0.2)
            const float floor = floor_probe_v(0.5f, V(pos), 0x120);
            if (!(1.0f < floor)) return Tail::Particle;
            // (the stack's sp+0x70: the random goal's offset when blocked this frame, else what the frame holds there)
            if (vec2_length(V(v70)) < fpu_add(lwc1(pv + 0x1fc), 4.0f) && target_dz() < 10.0f && rand_mod(0x13) == 0) {
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
                sq_zero(vel);
                sb(m + 0x20, 0x11);
                gml_2b4fb8_l13(pv);
                return Tail::Particle;
            }
            if (lbu(m + 0xbc) != 1) return Tail::Particle;
            if (rand_mod(4) != 0) return Tail::Particle;
            swerve_new(pv);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
            gml_2b4fb8_l13(pv);
            sq_zero(vel);
            sb(m + 0x20, 0x11);
            vec_clear(V(vel));
            sb(m + 0xbc, 0);
            return Tail::Particle;
        }
        case 99: {  // (0x2b7310) dying: on landing (or a wall) a burst (one in three slots of L13:0x1613b0 a second), gone; below 0, gone
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            if ((enemy_physics(m, pv + 0x120) & 0x140) != 0) {
                vec_clear(V(v60));
                const float frames = static_cast<float>(frame_scale_i(1));
                if (cooldown_slot_claim_l1(L13_ADDR(0x001613b0), 3, trunc_w(fpu_mul(frames, 60.0f))) >= 0)
                    explosion_burst(m, pos, 4, 0.5f, 13.0f);
                return Tail::Retire;
            }
            if (lwc1(m + 0x18) < 0.0f) return Tail::Retire;
            return Tail::Pulse;
        }
        default:  // (9, 19 .. 98 and past the table: 0x2b789c)
            return Tail::Pulse;
        }
    };

    const Tail tail = lbu(m + 0x20) < 100 ? run_state() : Tail::Pulse;
    switch (tail) {
    case Tail::Done:
        return;
    case Tail::Retire:
        moby_retire(m);
        return;
    case Tail::Particle:
        gml_2b4f28_l13(m, pv);
        break;
    case Tail::Floor:
        gml_2b4ff0_l13(m, pv);
        break;
    case Tail::Pulse:
        break;
    }
    color_pulse_update(m, pulse);
    const float lo = 2.0f, hi = 1021.0f;  // (the space's bounds)
    const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
    if (x < lo || hi < x || y < lo || hi < y || z < lo || hi < z) {
        moby_retire(m);
        return;
    }
    // enhancement: 1.02 Gameplay, fix 48 (fixes102.h): the thinning under a heavy frame load
    if (gameplay102_gemlik_load_thinning()) load_thinning(m, pv);
}

}  // namespace rac1
