// Aridia's own moby classes: the Sand Shark (class 580), its class hook +0x14 and its helpers, hand-ported from Aridia's
// overlay (`orig: L2:`). Aridia-only data is L2_ADDR; the engine's lists go through OV() and the level-data facade; the
// base ELF's (the hero 0x13f350, the frame time 0x15ed6c) are bare. The update, the hook and the helpers are declared in
// aridia_enemies.h; the update and the hook are registered in aridia_mobys.cpp.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/walker.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (0x13f350 + 0x80)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }    // (its square: the accelerations' scale)

// the heading from the moby to (x, y), as the original computes it
float heading_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the animation set `set` unless it is playing, blended over `frames` (60 Hz) frames
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the state `state` and the animation set `set` (blended over `frames`)
void go(uint32_t m, uint32_t state, uint32_t set, int32_t frames) {
    sb(m + 0x20, state);
    anim_to(m, set, frames);
}

// the shark's lair moby (pvar +0x288: a moby index, -1 none) counts the sharks out of it at its pvar +0x14c
void lair_count_add(uint32_t pv, int32_t d) {
    const int32_t i = lw(pv + 0x288);
    if (i < 0) return;
    const uint32_t lpv = lwu(lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8) + 0x78);
    sw(lpv + 0x14c, static_cast<uint32_t>(lw(lpv + 0x14c) + d));
}

// One sand particle (type 2) about the vector at frame + 0x10, frame being the caller's EE stack frame (the particle's
// position at +0x20, its velocity at +0x30, its acceleration at +0x40): the position jittered by z_lo..z_hi up and
// +-xy across; a random heading, its speeds, colours, sizes and life from the level's 0x44-byte record `rec` (speed
// across +0x10 / +0x14, up +0 / +4, its acceleration +8 / +0xc, the velocity's and acceleration's w +0x18 / +0x1c, three
// integer ranges +0x20 .. +0x34, the colours +0x38 / +0x3c, the texture +0x40).
void sand_particle(uint32_t frame, uint32_t rec, float z_lo, float z_hi, float xy) {
    const uint32_t at = frame + 0x20, vel = frame + 0x30, acc = frame + 0x40;
    const float yaw = rand_angle();
    lq_sq(at, frame + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(z_lo, z_hi)));
    swc1(at, fpu_add(lwc1(at), rand_range_f(-xy, xy)));
    swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_f(-xy, xy)));
    vec_clear(V(vel));
    const float c = cos_vu(yaw);
    const float cs = fpu_mul(rand_range_f(lwc1(rec + 0x10), lwc1(rec + 0x14)), dt());
    swc1(acc, fpu_mul(c, cs));
    const float s = sin_vu(yaw);
    const float ss = fpu_mul(rand_range_f(lwc1(rec + 0x10), lwc1(rec + 0x14)), dt());
    sw(acc + 8, 0);
    swc1(acc + 4, fpu_mul(s, ss));
    swc1(vel + 8, fpu_mul(rand_range_f(lwc1(rec), lwc1(rec + 4)), dt()));
    const float up = fpu_mul(rand_range_f(lwc1(rec + 8), lwc1(rec + 0xc)), dt());
    swc1(vel + 0xc, lwc1(rec + 0x18));
    swc1(acc + 0xc, lwc1(rec + 0x1c));
    swc1(acc + 8, up);
    auto pick = [rec](uint32_t k) {
        return float_to_int(rand_range_f(static_cast<float>(lw(rec + k)), static_cast<float>(lw(rec + k + 4))));
    };
    const int32_t n = pick(0x20);
    const int32_t h32 = pick(0x28);
    const int32_t h34 = pick(0x30);
    spawn_particle_02(at, vel, acc, lwu(rec + 0x38), lwu(rec + 0x3c), static_cast<uint32_t>(n), static_cast<uint32_t>(h32),
                      static_cast<uint32_t>(h34), lw(rec + 0x40));
}
}  // namespace

// A line of `count` sand grains (particle 0x17) from `from` to `to` (EE vectors), each a step further along: grey,
// half to fully opaque, a little drift and a rise up to the level's height 0x1619d4, all of the size
// size_lo * 210000 + (size_hi - size_lo) / count (the original's sum: the step is not multiplied by the grain's index);
// half of them additive (+3: 0x44); their life +0xa and fade from the level's 0x1619e0.
// orig: L2:0x002d3a50
void shark_sand_line_l2(uint32_t from, uint32_t to, int32_t count, float size_lo, float size_hi) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t step = sp, at = sp + 0x10, vel = sp + 0x20;
    const float n = static_cast<float>(count);
    vec3_sub(V(step), V(to), V(from));
    vec3_scale(V(step), V(step), fpu_div(1.0f, n));
    const float size_step = fpu_div(fpu_sub(size_hi, size_lo), n);
    V(vel) = Vec4{};
    swc1(vel + 8, u2f(0x3c23d70au));
    swc1(vel + 0xc, 1.0f);
    const float spread = u2f(0x3b23d70au);
    for (int32_t i = 0; i < count; ++i) {
        vec3_scale(V(at), V(step), static_cast<float>(i));
        vec3_add(V(at), V(from), V(at));
        const uint32_t alpha = static_cast<uint32_t>(rand_range(0x40, 0x70));
        const uint32_t grey = static_cast<uint32_t>(rand_range(0x40, 0x7f));
        const uint32_t rgb = grey | (grey << 16) | (grey << 8);
        const float hi = rand_range_f(1.0f, u2f(0x3f828f5cu));
        const int32_t spin = rand_range(-2, 2);
        swc1(vel, rand_range_signed_f(0.0f, spread));
        swc1(vel + 4, rand_range_signed_f(0.0f, spread));
        const float rise = lwc1(L2_ADDR(0x001619d4));
        swc1(vel + 8, rand_range_f(fpu_mul(rise, u2f(0x3dcccccdu)), rise));
        const uint32_t p = spawn_particle_17(at, spin, vel, (alpha << 24) | rgb, lwc1(L2_ADDR(0x001619dc)), 1.0f, hi,
                                             fpu_add(size_step, fpu_mul(size_lo, 210000.0f)));
        if (p == 0) continue;
        if (rand_mod(2) != 0) sb(p + 3, 0x44);
        sh(p + 0xa, lhu(L2_ADDR(0x001619e0)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, alpha);
        sb(p + 0x2b, lbu(L2_ADDR(0x001619e0)));
    }
    ee_stack_free(0xe0);
}

// The line of sand between the shark's points i and j (the level's table 0x1d3230 of 16-byte points, w a size, about
// the centre 0x1619c0, turned and placed with the moby; the sizes scaled by 0x1619d0).
// orig: L2:0x002d3c98
void shark_sand_edge_l2(uint32_t m, uint32_t i, uint32_t j, int32_t count) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t a = L2_ADDR(0x001d3230) + (i << 4), b = L2_ADDR(0x001d3230) + (j << 4);
    auto place = [m](uint32_t out, uint32_t point) {
        lq_sq(out, point);
        swc1(out + 0xc, 1.0f);
        vec3_sub(V(out), V(out), V(L2_ADDR(0x001619c0)));
        mat3_transform(V(out), V(out), M(m + 0xc0));
        vec3_add(V(out), V(out), V(m + 0x10));
    };
    place(sp, a);
    place(sp + 0x10, b);
    const float k = lwc1(L2_ADDR(0x001619d0));
    shark_sand_line_l2(sp, sp + 0x10, count, fpu_mul(lwc1(a + 0xc), k), fpu_mul(lwc1(b + 0xc), k));
    ee_stack_free(0xc0);
}

// The shark's outline in sand (its five edges, the grain counts from the level's 0x1619ec .. 0x1619f8): its dive.
// orig: L2:0x002d3dd0
void shark_sand_burst_l2(uint32_t m) {
    shark_sand_edge_l2(m, 0, 1, lw(L2_ADDR(0x001619f0)));
    shark_sand_edge_l2(m, 2, 3, lw(L2_ADDR(0x001619ec)));
    shark_sand_edge_l2(m, 4, 5, lw(L2_ADDR(0x001619f4)));
    shark_sand_edge_l2(m, 6, 7, lw(L2_ADDR(0x001619f4)));
    shark_sand_edge_l2(m, 8, 9, lw(L2_ADDR(0x001619f8)));
}

// A moby of the shark's group (+0x21; 0xff none) of class 0x29c (668: a shark's lair) still up (state below 0x7f):
// the first one, then each later one by a coin toss (rand_mod). 0 when there is none.
// orig: L2:0x002d5658
uint32_t shark_group_mate_l2(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return 0;
    uint32_t pick = 0;
    for (uint32_t e = lvd::group(static_cast<int32_t>(group));; e += 2) {
        const uint32_t mate = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(mate + 0xa6) == 0x29c && lbu(mate + 0x20) < 0x7f) {
            if (pick == 0 || (rand_mod(0xff) & 1) != 0) pick = mate;
        }
        if (lh(e) < 0) break;
    }
    return pick;
}

// The sharks (class 0x244, 580) of the shark's group alerted: each one's alert timer (pvar +0x25e) raised to 240 .. 300
// frames (a random draw for each).
// orig: L2:0x002d5728
void shark_group_alert_l2(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return;
    for (uint32_t e = lvd::group(static_cast<int32_t>(group));; e += 2) {
        const uint32_t mate = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(mate + 0xa6) == 0x244) {
            const uint32_t mpv = lwu(mate + 0x78);
            const float t = frame_scale_f(rand_range_f(240.0f, 300.0f));
            if (static_cast<float>(lh(mpv + 0x25e)) < t) sh(mpv + 0x25e, static_cast<uint32_t>(float_to_int(t)));
        }
        if (lh(e) < 0) break;
    }
}

// `count` sand particles about `pos` (an EE vector; the level's record 0x161920), fewer while the particles are many
// (0x160178: from 1025 a half, from 1501 a quarter).
// orig: L2:0x002d57f8
void shark_sand_spray_l2(uint32_t pos, int32_t count) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    lq_sq(sp + 0x10, pos);
    const int32_t live = lw(L2_ADDR(0x00160178));
    int32_t n = count;
    if (live >= 1501) n = count / 4;
    else if (live >= 1025) n = count / 2;
    for (int32_t i = 0; i < n; ++i) sand_particle(sp, L2_ADDR(0x00161920), -0.25f, -0.75f, 0.25f);
    ee_stack_free(0xe0);
}

// One sand particle about `pos` (an EE vector; the level's record 0x161964), unless there are 1701 particles or more.
// orig: L2:0x002d5a68
void shark_sand_puff_l2(uint32_t pos) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    lq_sq(sp + 0x10, pos);
    if (lw(L2_ADDR(0x00160178)) < 1701) sand_particle(sp, L2_ADDR(0x00161964), 0.125f, 0.0f, 0.125f);
    ee_stack_free(0xc0);
}

// The shark's frame before its state's own code: its last state (+0x280); out of a lair it surfaced from (+0x2e 2) at
// a group mate's lair (none: retired); its alert timer (+0x25e) rearmed while alerted (+0x38), or when the hero comes
// within 10 while the wait +0x25c has run out; its reach (+0x250: +0x24c, 5 more while alerted); the hits (mask
// 0x330000; unless it is knocked or dying: the group alerted, the health +0x20 down; at 0, knocked out (state 0x18:
// the move record +0x120 set 6 off the hit's direction, raised off the floor, its lair's count down)); its colour
// pulse; its target (+0x1d0 / +0x210, the selection's result +0x214: 2 none, or too far from home +0x220 or off its
// height by more than 3), the hero when it has none.
// orig: L2:0x002d5c88
void shark_frame_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x280, lbu(m + 0x20));
    if (lbu(m + 0x20) == 0) return;
    if (lbu(pv + 0x2e) == 2) {
        sb(pv + 0x2e, 1);
        lair_count_add(pv, -1);
        const uint32_t mate = shark_group_mate_l2(m);
        sw(pv + 0x27c, mate);
        if (mate == 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 0x1a);
        lq_sq(m + 0x10, pv + 0x220);
        sw(m + 0x94, 0);
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 10.0f));
    }
    bool rearm = true;
    if (lw(pv + 0x38) == 0) {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x25c))) != 0) rearm = false;
        else rearm = vec2_distance(V(m + 0x10), V(HERO_POS)) < 10.0f;
    }
    if (rearm) sh(pv + 0x25e, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
    sw(pv + 0x38, 0);
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x25e))) != 0) swc1(pv + 0x250, lwc1(pv + 0x24c));
    else swc1(pv + 0x250, fpu_add(lwc1(pv + 0x24c), 5.0f));

    // ---- the hit
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t probe = sp, dir = sp + 0x10, hit_state = sp + 0x20, hit_dmg = sp + 0x24, hit_yaw = sp + 0x28;
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x18) {
        shark_group_alert_l2(m);
        sw(pv + 0x29c, 0);
        const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) {  // knocked out: thrown off the hit (the move record +0x120, set 6), then dying in state 0x18
            sb(pv + 0x15d, 0);
            swc1(pv + 0x20, fpu_sub(hp, lwc1(hit_dmg)));
            sw(pv + 0x140, 0x200);
            swc1(pv + 0x130, fpu_mul(lwc1(L2_ADDR(0x00161914)), dt2()));
            swc1(pv + 0x134, fpu_mul(lwc1(L2_ADDR(0x00161918)), dt2()));
            swc1(pv + 0x138, fpu_mul(lwc1(L2_ADDR(0x0016190c)), dt()));
            swc1(pv + 0x13c, fpu_mul(lwc1(L2_ADDR(0x00161908)), dt()));
            sw(pv + 0x144, 9);
            swc1(pv + 0x16c, fpu_add(dt(), dt()));
            swc1(pv + 0x148, 0.5f);
            lq_sq(probe, m + 0x10);
            swc1(probe + 8, fpu_add(lwc1(probe + 8), 2.0f));
            const float floor = floor_probe_v(0.5f, V(probe), 0);
            if (lwc1(m + 0x18) < floor) swc1(m + 0x18, floor);
            if (lw(pv + 0x288) >= 0) {
                lair_count_add(pv, -1);
                sb(pv + 0x15d, 0);
            }  // (else the original raises the probe's height by 0.01 at its stack: not used again)
            sw(pv + 0x144, lwu(pv + 0x144) | 0x20);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            lq_sq(dir, hit + 0x10);
            vec_yaw_marked_v(V(dir), *static_cast<float*>(ee_ram_ptr(hit_yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
            move_state_start(m, pv + 0x120, 6, 1, 0, lwc1(hit_yaw));
            swc1(pv + 0x148, u2f(0x3f59999au));
            swc1(pv + 0x170, 7.0f);
            swc1(pv + 0x174, 14.0f);
            sb(m + 0x20, 0x18);
            sw(m + 0x94, 0);
            sb(pv + 0x117, 0x78);
            color_pulse_start(m, pv + 0x110);
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x110);

    // ---- the target: what it sees along its path's polygon within its reach
    const uint32_t path = lvd::path(lw(pv + 0x290));
    if (target_select(m, pv + 0x1d0, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x250)) != 2) {
        if (lwc1(pv + 0x250) < vec2_distance(V(pv + 0x220), V(pv + 0x1d0))) sw(pv + 0x214, 2);
        else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x1d8)))) sw(pv + 0x214, 2);
    }
    if (lw(pv + 0x210) == 0) {
        sw(pv + 0x210, lwu(0x001413d0));
        lq_sq(pv + 0x1d0, HERO_POS);
    }
    ee_stack_free(0xa0);
}

// Outside its path's polygon (pvar +0x290's points): back to state 0x12 (the animation set 0, over 0x14 frames) and the
// flag +0x298; the polygon test's result otherwise (1 when out).
// orig: L2:0x002d63c0
uint32_t shark_path_check_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0x290));
    const int32_t in = point_in_polygon(m + 0x10, path + 0x10, lw(path));
    if (in != 0) return static_cast<uint32_t>(in);
    sb(m + 0x20, 0x12);
    anim_to(m, 0, 0x14);
    sw(pv + 0x298, 1);
    return 1;
}

// Class 580, the Sand Shark: a fin swimming in the sand of its path's polygon (pvar +0x290), that surfaces and bites
// what comes near. Nothing while the level holds the sharks (0x1619a8). After the frame's common part (shark_frame_l2:
// hits, alert, target), by state (the original's jump table at L2:0x1fa8f0, 30 rows):
//   0 set up (home +0x220 / +0x230, the walker record +0x180; in a lair (+0x26c): under the sand, state 0xc; with a
//     box (+0x278): state 3; with a trigger moby (+0x258): state 0x1d; else state 1)
//   1 to state 0x12;  2 (0x12 in the original's numbering is 18: below) wanders about home, its heading wobbling
//   (+0x244 every 60 frames), now and then off along a new heading (state 0x11);  3 waits for its target in its box
//   5 / 22 (0x16) swim toward the target (22 under the sand), 6 bites (chain point at the animation's frame 13), 7 / 23
//   back home, 8 knocked (the move record +0x120: landed, state 0xb), 9 carried (carry_update), 12 (0xc) waits in its
//   lair for the hero in its boxes (+0x26c / +0x270), then 13 (0xd) leaps out of the lair toward home, 14 / 15 chase,
//   17 / 18 surface, 19 .. 21 dive, 24 (0x18) dying, 25 (0x19) fades out, 26 (0x1a) back from a lair, 27 (0x1b) a leap
//   with its spray, 29 (0x1d) waits for its trigger moby; 4, 10, 11, 16, 28 nothing.
// The killed shark (state 0x18, the events 0x140) carries "1.02 Fixes" fix 18: after its bolts, the 1.02 build's death
// burst (fixes102_sand_shark_burst; off by default).
// orig: L2:0x002d3e50
void sand_shark_update_l2(uint32_t m) {
    if (lw(L2_ADDR(0x001619a8)) != 0) return;
    // (the original starts the EE's timer 0 here and reads it at the end: a profiling count, nothing of the game's)
    const uint32_t pv = lwu(m + 0x78);
    joint_node_scale(m, 0, pv + 0x2a0, u2f(0x40066666u));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L2_ADDR(0x001673c0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    shark_frame_l2(m);
    const uint32_t state = lbu(m + 0x20);
    if (state >= 30) return;

    const uint32_t sp = ee_stack_alloc(0x100);
    const float sink_off = lwc1(L2_ADDR(0x0016191c));  // (the level's height the shark is raised by for its probes)
    auto rise = [m, sink_off] { swc1(m + 0x18, fpu_add(lwc1(m + 0x18), sink_off)); };
    auto sink = [m, sink_off] { swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), sink_off)); };
    // a puff of sand where it swims (half of the frames, by a coin toss)
    auto puff = [m, sp](uint32_t at) {
        if ((rand_mod(0xff) & 1) == 0) return;
        lq_sq(sp + at, m + 0x10);
        shark_sand_puff_l2(sp + at);
    };
    // a walker step toward the point 2 ahead along its heading (the walker record +0x180), into `out`
    auto step_ahead = [m, pv, sp](uint32_t out) {
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(sp, fpu_add(c, c));
        const float s = sin_vu(lwc1(m + 0x48));
        sw(sp + 8, 0);
        swc1(sp + 4, fpu_add(s, s));
        vec3_add(V(sp), V(sp), V(m + 0x10));
        walker_record_step(1.0f, m, pv + 0x180, sp, out);
    };
    // without a target: with the alert running, now and then (1 in 0x13) it dives with a sound (state 0x15)
    auto dive_check = [m, pv] {
        if (lh(pv + 0x25e) == 0) return;
        if (rand_mod(0x13) != 0) return;
        sb(m + 0x20, 0x15);
        moby_sound_start(4, 0, m);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        sw(pv + 0x274, 0xffffffffu);
    };
    // the end of a chase: at the target (1.5) it bites (state 6), else back to swimming (5) unless it has none
    auto chase_end = [m, pv](uint32_t target) {
        if (vec2_distance(V(m + 0x10), V(target)) < 1.5f) {
            go(m, 6, 1, 0xa);
            return;
        }
        if (lw(pv + 0x214) == 2) return;
        go(m, 5, 0, 0xf);
    };
    const float k_turn = u2f(0x3d4ccccdu), k_damp = u2f(0x3e99999au), k_max = u2f(0x3e4ccccdu);  // (0.05, 0.3, 0.2)
    const float deg = u2f(0x3c8efa35u);                                                           // (pi / 180)

    auto run = [&] {
        switch (state) {  // (the original's jump table at L2:0x1fa8f0)
        case 0: {  // (0x002d3f40) set up
            const int32_t path_i = lw(pv + 0x290);
            if (path_i == -1 || lw(lvd::path(path_i)) == 0) {
                debug_printf_stub();  // (the original prints that the moby has no path / an empty one)
                moby_retire(m);
                return;
            }
            lair_count_add(pv, 1);
            lq_sq(pv + 0x220, m + 0x10);
            lq_sq(pv + 0x230, m + 0x10);
            sh(pv + 0x24, 1);
            swc1(pv + 0x20, 1.0f);
            sb(pv + 0x5c, 1);
            sb(pv + 0x2e, 1);
            sb(pv + 0x29, 1);
            sh(pv + 0x25e, 0);
            sb(pv + 0x58, lbu(L2_ADDR(0x00161900)));
            walker_record_init_l1(pv + 0x180);
            swc1(pv + 0x188, 2.0f);
            sw(pv + 0x180, 0x200);
            swc1(pv + 0x184, 0.5f);
            swc1(pv + 0x1a4, fpu_mul(lwc1(pv + 0x254), dt()));
            swc1(pv + 0x18c, 2.0f);
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
            sw(pv + 0xd0, L2_ADDR(0x001619b0));
            if (lw(pv + 0x26c) >= 0) {  // in a lair: under the sand
                if (lw(pv + 0x28c) == -1) {
                    moby_retire(m);
                    return;
                }
                swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 10.0f));
                sb(m + 0x20, 0xc);
                sw(m + 0x94, 0);
                sh(m + 0x34, (lhu(m + 0x34) | 1u) & 0xefffu);
                anim_to(m, 0, 3);
                return;
            }
            if (lw(pv + 0x278) >= 0) {  // a box to wait in
                swc1(pv + 0x18c, u2f(0x3e4ccccdu));
                go(m, 3, 2, 3);
                return;
            }
            if (lw(pv + 0x258) >= 0) {  // a trigger moby to wait for
                sb(m + 0x20, 0x1d);
                sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
                sw(m + 0x94, 0);
                return;
            }
            go(m, 1, 2, 3);
            return;
        }
        case 29: {  // (0x002d4160) its trigger moby (+0x258) past state 4 and the hero not in state 0x72: it starts
            const uint32_t trigger = lwu(OV(0x0015ffd8)) + (lwu(pv + 0x258) << 8);
            if (lbu(trigger + 0x20) < 5) return;
            if (lw(0x001413d4) == 0x72) return;
            sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sw(pv + 0x258, 0xffffffffu);
            sh(pv + 0x25c, static_cast<uint32_t>(frame_scale_i(0x1518)));
            sb(m + 0x20, 0);
            sink();
            return;
        }
        case 1:  // (0x002d41d4) (the original's vector to the hero, at its stack, is not used)
            go(m, 0x12, 0, 3);
            return;
        case 2: {  // (0x002d4210) wandering about home
            rise();
            lq_sq(sp, pv + 0x220);
            const float yaw = heading_to(m, lwc1(sp), lwc1(sp + 4));
            yaw_turn_damped(m, pv + 0x240, angle_add(yaw, lwc1(pv + 0x244)), k_turn, k_damp, u2f(0x3d23d70au));
            swc1(m + 0x10, fpu_add(lwc1(m + 0x10), fpu_mul(cos_vu(lwc1(m + 0x48)), dt())));
            swc1(m + 0x14, fpu_add(lwc1(m + 0x14), fpu_mul(sin_vu(lwc1(m + 0x48)), dt())));
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x260))) != 0) {
                const int32_t frames = frame_scale_i(0x3c);
                const float k = fpu_mul(lwc1(pv + 0x248), deg);
                sw(pv + 0x260, static_cast<uint32_t>(frames));
                swc1(pv + 0x244, rand_range_f(-k, k));
            }
            if (lbu(m + 0x31) != 0) puff(0x10);
            sink();
            if (lw(pv + 0x214) == 2) {
                dive_check();
                return;
            }
            const int32_t box = lw(pv + 0x274);
            if (box >= 0) {
                if (point_in_box(pv + 0x1d0, box) != 0 && lh(pv + 0x25e) == 0) return;
                if (rand_mod(9) != 0) return;
                sw(pv + 0x274, 0xffffffffu);
                return;
            }
            if (rand_mod(4) != 0) return;
            const float k = fpu_mul(lwc1(pv + 0x248), deg);
            swc1(pv + 0x244, k);
            if (lw(pv + 0x284) != 0) swc1(pv + 0x244, fpu_mul(k, static_cast<float>(lw(pv + 0x284))));
            else if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x244, -lwc1(pv + 0x244));
            sw(pv + 0x260, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(f2u(eer::cvt_w(rand_range_f(30.0f, 90.0f)))))));
            swc1(pv + 0x1a4, fpu_mul(dt(), 4.0f));
            go(m, 0x11, 0xc, 3);
            return;
        }
        case 3:  // (0x002d4458) waiting in its box for the target
            if (lw(pv + 0x214) == 2) return;
            if (point_in_box(pv + 0x1d0, lw(pv + 0x278)) == 0) return;
            go(m, 5, 0, 3);
            return;
        case 14: {  // (0x002d44a0) chasing along the ground; across its path's edge near the target: state 0xf
            lq_sq(sp + 0x10, pv + 0x1d0);
            const float yaw = heading_to(m, lwc1(sp + 0x10), lwc1(sp + 0x14));
            yaw_turn_damped(m, pv + 0x240, fpu_add(yaw, lwc1(pv + 0x244)), k_turn, k_damp, k_max);
            step_ahead(sp + 0x20);
            shark_path_check_l2(m);
            bool at_edge = false;
            if (path_crossing(lw(pv + 0x290), m + 0x10, sp + 0x10, sp + 0x30) != 0 &&
                vec3_distance(V(m + 0x10), V(sp + 0x30)) < lwc1(pv + 0x294))
                at_edge = true;
            if (lh(pv + 0x25e) == 0) {
                sb(m + 0x20, 0x12);
                return;
            }
            if (at_edge) {
                go(m, 0xf, 5, 0xa);
                return;
            }
            chase_end(sp + 0x10);
            return;
        }
        case 15: {  // (0x002d4668) turning toward the target
            const float yaw = heading_to(m, lwc1(pv + 0x1d0), lwc1(pv + 0x1d4));
            const float two_pi = u2f(0x40c90fdbu);
            const float rate = fpu_mul(dt2(), two_pi);
            yaw_seek(m + 0x48, pv + 0x240, yaw, rate, rate, fpu_mul(dt(), two_pi));
            if (lh(pv + 0x25e) == 0) {
                sb(m + 0x20, 0x12);
                return;
            }
            chase_end(pv + 0x1d0);
            return;
        }
        case 17:
        case 18: {  // (0x002d474c) surfacing (to its home's height at least), then 0x16 / 0x17; sand while it rises
            rise();
            float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            if (floor < lwc1(pv + 0x228)) floor = lwc1(pv + 0x228);
            lq_sq(sp, m + 0x10);
            swc1(sp + 8, floor);
            approach_f(m + 0x18, floor, fpu_mul(dt(), 4.0f));
            if (floor == lwc1(m + 0x18)) {
                if (lbu(m + 0x53) != 0xc) moby_anim_set(m, 0xc, 0, frame_scale_i(5));
                sb(m + 0x20, lbu(m + 0x20) == 0x11 ? 0x16u : 0x17u);
            }
            sink();
            if (frame_scale_i(0xa) < lw(L2_ADDR(0x0015f5cc))) shark_sand_spray_l2(sp, 6);
            return;
        }
        case 19:
        case 20:
        case 21: {  // (0x002d4848) diving to the floor: then 5 (from 0x13), 7 (0x14) or 0xe (0x15, a dive distance +0x294)
            rise();
            const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            lq_sq(sp, m + 0x10);
            swc1(sp + 8, floor);
            if (approach_f(m + 0x18, fpu_add(floor, lwc1(L2_ADDR(0x0016191c))), fpu_mul(dt(), 4.0f)) == 0.0f) {
                const uint32_t s = lbu(m + 0x20);
                if (s == 0x13 || s == 0x14) {
                    go(m, s == 0x13 ? 5u : 7u, 0, 3);
                } else {
                    swc1(pv + 0x294, rand_range_f(2.0f, 3.5f));
                    swc1(pv + 0x1a4, fpu_mul(dt(), 4.0f));
                    go(m, 0xe, 0, 3);
                }
            }
            sink();
            shark_sand_spray_l2(sp, 6);
            return;
        }
        case 5:
        case 22: {  // (0x002d4984) swimming toward the target (22: under the sand); near it (12) it surfaces (0x13)
            if (lbu(m + 0x20) == 0x16) {
                rise();
                puff(0x40);
            }
            lq_sq(sp + 0x30, pv + 0x1d0);
            const float yaw = heading_to(m, lwc1(sp + 0x30), lwc1(sp + 0x34));
            yaw_turn_damped(m, pv + 0x240, fpu_add(yaw, lwc1(pv + 0x244)), k_turn, k_damp, k_max);
            step_ahead(sp + 0x10);
            shark_path_check_l2(m);
            if (lbu(m + 0x20) == 0x16) sink();
            if (lw(pv + 0x214) == 2) {  // no target: home (7) with a box, else 0x12 / 0xe by the alert
                if (lw(pv + 0x278) >= 0) sb(m + 0x20, 7);
                else sb(m + 0x20, lh(pv + 0x25e) == 0 ? 0x12u : 0xeu);
                return;
            }
            const int32_t box = lw(pv + 0x278);
            if (box >= 0 && point_in_box(pv + 0x1d0, box) == 0) {
                sb(m + 0x20, 7);
                return;
            }
            // (the original's vector from home to the target, at its stack, is not used)
            if (vec2_distance(V(m + 0x10), V(sp + 0x30)) < 12.0f && lbu(m + 0x20) == 0x16) {
                sb(m + 0x20, 0x13);
                moby_sound_start(4, 0, m);
                anim_to(m, 0, 3);
                return;
            }
            if (!(vec2_distance(V(m + 0x10), V(sp + 0x30)) < 1.5f)) return;
            if (lbu(m + 0x20) != 5) return;
            go(m, 6, 1, 3);
            return;
        }
        case 6: {  // (0x002d4bec) the bite: at the animation's frame 13 its jaws (0.75 ahead, 0.5 up) touch what is there
            const float yaw = heading_to(m, lwc1(pv + 0x1d0), lwc1(pv + 0x1d4));
            yaw_turn_damped(m, pv + 0x240, yaw, k_turn, k_damp, k_max);
            if (lbu(m + 0x52) == lbu(m + 0x53) && anim_crossed(m, 13.0f) != 0) {
                vec3_set_length(V(sp), V(m + 0xc0), 0.75f);
                vec3_add(V(sp), V(sp), V(m + 0x10));
                swc1(sp + 8, fpu_add(lwc1(sp + 8), 0.5f));
                moby_sphere_contact(m, sp, 1, 0, 1, 0, 0.5f, 1.0f, 1.0f);
            }
            if ((lbu(m + 0x70) & 2) != 0 && 1.5f < vec2_distance(V(m + 0x10), V(pv + 0x1d0))) go(m, 5, 0, 3);
            const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            const float vz = fpu_sub(lwc1(pv + 0x198), fpu_mul(dt2(), u2f(0x411ccccdu)));
            swc1(pv + 0x198, vz);
            const float z = fpu_add(lwc1(m + 0x18), vz);
            swc1(m + 0x18, z);
            if (z < floor) {
                sw(pv + 0x198, 0);
                swc1(m + 0x18, floor);
            }
            return;
        }
        case 7:
        case 23: {  // (0x002d4d80) back home (23: under the sand); there: 3 / 0x12 (from 7) or 2 (from 0x17)
            if (lbu(m + 0x20) == 0x17) {
                rise();
                puff(0x40);
            }
            lq_sq(sp + 0x30, pv + 0x220);
            const float yaw = heading_to(m, lwc1(sp + 0x30), lwc1(sp + 0x34));
            yaw_turn_damped(m, pv + 0x240, yaw, u2f(0x3ca3d70au), k_damp, u2f(0x3dcccccdu));
            step_ahead(sp + 0x10);
            if (lbu(m + 0x20) == 0x17) sink();
            if (vec2_distance(V(m + 0x10), V(pv + 0x220)) < 0.5f) {
                sw(pv + 0x298, 0);
                if (lbu(m + 0x20) == 7) {
                    if (lw(pv + 0x278) >= 0) {
                        go(m, 3, 2, 3);
                    } else {
                        sb(m + 0x20, 0x12);
                        swc1(pv + 0x1a4, fpu_mul(dt(), 1.5f));
                        anim_to(m, 0, 3);
                    }
                } else {
                    sb(m + 0x20, 2);
                    swc1(pv + 0x1a4, fpu_mul(dt(), 1.5f));
                    anim_to(m, 0xc, 5);
                }
            }
            if (lw(pv + 0x298) != 0) {  // out of its polygon: pushed on along its heading
                vec3_set_length(V(sp + 0x40), V(m + 0xc0), fpu_mul(dt(), 0.5f));
                vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x40));
                return;
            }
            if (lw(pv + 0x214) == 2) {
                dive_check();
                return;
            }
            vec3_sub(V(sp + 0x20), V(pv + 0x1d0), V(pv + 0x220));
            const int32_t box = lw(pv + 0x278);
            if (box >= 0) {  // the target in its box: after it (5)
                if (point_in_box(pv + 0x1d0, box) == 0) return;
                sb(m + 0x20, 5);
                swc1(pv + 0x1a4, fpu_mul(lwc1(pv + 0x254), dt()));
                return;
            }
            if (!(vec3_length(V(sp + 0x20)) < lwc1(pv + 0x24c))) return;  // the target near home: after it (5 / 0x16)
            sb(m + 0x20, lbu(m + 0x20) == 7 ? 5u : 0x16u);
            swc1(pv + 0x1a4, fpu_mul(lwc1(pv + 0x254), dt()));
            anim_to(m, 0, 3);
            return;
        }
        case 8: {  // (0x002d50fc) knocked: landed, state 0xb; below the ground, gone in sand
            if ((enemy_physics(m, pv + 0x120) & 1) != 0) {
                sb(m + 0x20, 0xb);
                if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 8, frame_scale_i(3));
                return;
            }
            if (lwc1(m + 0x18) < 0.0f) {
                shark_sand_burst_l2(m);
                moby_retire(m);
            }
            return;
        }
        case 9:  // (0x002d5174) carried
            if (carry_update(m, pv + 0x120) != 0) {
                sb(m + 0x20, 1);
                sh(pv + 0xc8, 0);
            }
            return;
        case 12: {  // (0x002d5194) in its lair (+0x28c) until the hero is in one of its boxes: a leap toward home (0xd)
            const bool seen = point_in_box(HERO_POS, lw(pv + 0x26c)) != 0 || point_in_box(HERO_POS, lw(pv + 0x270)) > 0;
            if (!seen) return;
            const uint32_t lair = lwu(OV(0x0015ffd8)) + (lwu(pv + 0x28c) << 8);
            const float g = u2f(0x412ccccdu);  // (10.8)
            const float drop = fpu_sub(lwc1(lair + 0x18), lwc1(pv + 0x228));
            const float up = rand_range_f(6.0f, 10.0f);
            const float g2 = fpu_mul(dt2(), g);
            const float vz = fpu_mul(up, dt());
            const float root = sqrt_f(fpu_mul(fpu_add(g2, g2), drop));
            const float frames = fpu_div(fpu_add(root, fpu_add(vz, vz)), fpu_mul(dt2(), g));
            const float speed = fpu_div(vec2_distance(V(pv + 0x220), V(lair + 0x10)), frames);
            vec3_sub(V(pv + 0x40), V(pv + 0x220), V(lair + 0x10));
            sw(pv + 0x48, 0);
            vec3_set_length(V(pv + 0x40), V(pv + 0x40), speed);
            swc1(pv + 0x48, vz);
            lq_sq(m + 0x10, lair + 0x10);
            sh(m + 0x34, (lhu(m + 0x34) & 0xfffeu) | 0x1000u);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            if (lbu(m + 0x53) != 0xb) moby_anim_set(m, 0xb, 0, 0);
            sb(m + 0x20, 0xd);
            return;
        }
        case 27: {  // (0x002d52d8) the leap's spray (particle 4), then as 0xd
            rand_sphere_point(sp + 0x10, 0.0f, u2f(0x3e4ccccdu));
            // (the original also adds the moby's position to it at its stack: not used)
            vec3_scale(V(sp + 0x10), V(sp + 0x10), fpu_mul(dt(), 5.0f));
            const int32_t life = frame_scale_i(rand_range(0x14, 0x28));
            const int32_t fade = rand_range(0x78, 0x96);
            spawn_particle_4(m + 0x10, sp + 0x10, lwu(L2_ADDR(0x001619e4)), lwu(L2_ADDR(0x001619e8)), static_cast<uint32_t>(life),
                             0x64, static_cast<uint32_t>(fade), 0);
            [[fallthrough]];
        }
        case 13: {  // (0x002d5370) the leap: spinning, falling (10.8); below home's height under the sand: 0x17
            swc1(m + 0x44, angle_sub(lwc1(m + 0x44), fpu_mul(dt(), u2f(0x41490fdbu))));
            vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x40));
            swc1(pv + 0x48, fpu_sub(lwc1(pv + 0x48), fpu_mul(dt2(), u2f(0x412ccccdu))));
            const float home_z = lwc1(pv + 0x228);
            const float z = lwc1(m + 0x18);
            if (z < fpu_sub(home_z, sink_off)) {
                sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
                sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
                sb(m + 0x20, 0x17);
                sw(m + 0x44, 0);
                swc1(m + 0x18, fpu_sub(lwc1(pv + 0x228), sink_off));
                anim_to(m, 0xc, 1);
                return;
            }
            if (!(z < fpu_add(home_z, u2f(0x3f19999au)))) return;
            lq_sq(sp, m + 0x10);
            shark_sand_spray_l2(sp, 6);
            return;
        }
        case 24: {  // (0x002d5484) dying (the move record +0x120): sand on landing; at the events 0x140 its bolts, then
                    // back at a group mate's lair (0x1a) or gone
            const uint32_t r = enemy_physics(m, pv + 0x120);
            if ((r & 1) != 0 && lw(pv + 0x29c) == 0) {
                sw(pv + 0x29c, 1);
                lq_sq(sp, m + 0x10);
                shark_sand_spray_l2(sp, 0x1b);
            }
            if ((r & 0x140) != 0) {
                moby_drop_bolts(m, 0, -1);
                fixes102_sand_shark_burst(m);  // enhancement: 1.02 Fixes, fix 18 (the sand shark's burst; fixes102.h)
                const uint32_t mate = shark_group_mate_l2(m);
                sw(pv + 0x27c, mate);
                shark_sand_burst_l2(m);
                if (mate == 0) {
                    moby_retire(m);
                    return;
                }
                sb(m + 0x20, 0x1a);
                sw(m + 0x58, 0);
                lq_sq(m + 0x10, pv + 0x220);
                sw(m + 0x94, 0);
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 10.0f));
                return;
            }
            if (lwc1(m + 0x18) < 0.0f) {
                shark_sand_burst_l2(m);
                moby_retire(m);
            }
            return;
        }
        case 25: {  // (0x002d5578) fading out (+0x23 down by 8), then back at a group mate's lair (0x1a) or gone
            const uint32_t fade = lbu(m + 0x23);
            if (fade >= 8) {
                sb(m + 0x23, fade - 8);
                return;
            }
            const uint32_t mate = shark_group_mate_l2(m);
            sw(pv + 0x27c, mate);
            shark_sand_burst_l2(m);
            if (mate == 0) {
                moby_retire(m);
                return;
            }
            sb(m + 0x23, 0x80);
            sb(m + 0x20, 0x1a);
            lq_sq(m + 0x10, pv + 0x220);
            sw(m + 0x94, 0);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 10.0f));
            return;
        }
        case 26: {  // (0x002d55f4) at a lair: gone when its group has none left
            const uint32_t mate = shark_group_mate_l2(m);
            sw(pv + 0x27c, mate);
            if (mate == 0) moby_retire(m);
            return;
        }
        default:  // 4, 10, 11, 16, 28 (0x002d5614): nothing
            return;
        }
    };
    run();
    ee_stack_free(0x100);
}

// The Sand Shark's class hook +0x14 (class +0x2c table): back at a group mate's lair (state 0x1a, under the sand at
// home), or gone in sand when there is none. The original returns what its last load left in $v0; its callers drop it.
// orig: L2:0x002d6340
uint32_t shark_hook_14_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t mate = shark_group_mate_l2(m);
    sw(pv + 0x27c, mate);
    if (mate == 0) {
        shark_sand_burst_l2(m);
        moby_retire(m);
        return 0;
    }
    sb(m + 0x20, 0x1a);
    sw(m + 0x58, 0);
    lq_sq(m + 0x10, pv + 0x220);
    sw(m + 0x94, 0);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 10.0f));
    return 0;
}

// The Sand Shark's class hook +0x10 (class +0x2c table): its carry record, pvar + 0x60 (0 for no moby or one without
// pvars). (Veldin's class 749 has the same code with its record at pvar + 0xd0: e749_hook_10.)
// orig: L2:0x002d6310
uint32_t shark_hook_10_l2(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

// Class 755, the Sand Shark's egg (nothing on Aridia spawns it): state 1 falls (enemy_physics on its pvars) puffing sand
// (three particles 4 a frame about it); landed, state 2; state 2 hatches its shark (pvar +0x60: put at its place in state
// 0x11, its animation set 5) and is gone.
// orig: L2:0x002e0280
void shark_egg_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 1) {
        const uint32_t sp = ee_stack_alloc(0x80);
        const uint32_t still = sp, at = sp + 0x10;
        vec_clear(V(still));
        if ((enemy_physics(m, pv) & 1) != 0) sb(m + 0x20, 2);
        const float xy = u2f(0x3e19999au);  // (0.15)
        for (int k = 0; k < 3; ++k) {
            lq_sq(at, m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(0.0f, 0.5f)));
            swc1(at, fpu_add(lwc1(at), rand_range_f(-xy, xy)));
            swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_f(-xy, xy)));
            const int32_t life = frame_scale_i(rand_range(0x14, 0x28));
            const int32_t fade = rand_range(0x14, 0x28);
            spawn_particle_4(at, still, lwu(L2_ADDR(0x00161c9c)), lwu(L2_ADDR(0x00161ca0)), static_cast<uint32_t>(life), 0x28,
                             static_cast<uint32_t>(fade), 1);
        }
        ee_stack_free(0x80);
        return;
    }
    if (state != 2) return;
    const uint32_t shark = lwu(pv + 0x60);
    lq_sq(shark + 0x10, m + 0x10);
    sb(shark + 0x20, 0x11);
    moby_anim_set(lwu(pv + 0x60), 5, 0, 1);
    const uint32_t s = lwu(pv + 0x60);
    sw(s + 0x94, lwu(lwu(s + 0x24) + 0x10));
    swc1(lwu(pv + 0x60) + 0x58, 1.0f);
    moby_update_transform(lwu(pv + 0x60));
    moby_retire(m);
}

}  // namespace rac1
