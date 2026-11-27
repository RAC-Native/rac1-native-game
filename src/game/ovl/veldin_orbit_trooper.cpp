// Drek's fleet's (level 17) class 1382, ported by hand from the level's own code: a trooper that stands at its post (its
// home, pvars +0x1f0), sees the hero, turns to him, walks up to him inside its area and strikes at close range, then walks
// home; the Suck Cannon carries it (state 12, carry_update), any hit that counts knocks it down for good (state 15: it falls,
// bursts into debris, drops its bolts and goes). Its head looks at the hero (0x2f00a0), its three lights pulse with its
// mood (0x2f0210, which queues the shared draw class1382_glows_draw, L17:0x2f0448). The original's order of calls and float
// operations kept. Level 17 only: `orig: L17:`, level data through lvd::*, the level's own data L17_ADDR, base ELF bare.
// Registered in veldin_orbit_mobys.cpp; its five class hooks are carry_drop_hook and veldin_orbit_hooks / carry_hooks_levels.
//
// The moby: +0x20 the state (below), +0x31 / +0x34 bit 0 shown or hidden (the camera outside its boxes), +0x53 the
// animation, +0x58 the animation's speed, +0x70 bit 1 the animation's end, +0x7f the floor band (0x17 near the camera),
// +0x90 its colour, +0xa4 its hit slot. The pvars: +0x20 its health and hit record (moby_hit_react_r), +0x38 nonzero: its
// chase timer to be started, +0x110 / +0x120 its colour pulse and move record (enemy_physics, carry_update), +0x180 its
// target record (target_select: +0x180 the point, +0x1b0 the aim point, +0x1c0 the moby), +0x1d0 its area (a level path
// used as a polygon), +0x1d4 the chase timer (frames), +0x1d8 / +0x1dc its walking speed and turn rate, +0x1e0 / +0x1e2
// two countdowns, +0x1e4 its sight (0 not seen, 1 seen, 2 seen ahead), +0x1ec its walking speed toward the target, +0x1f0
// its home, +0x200 .. +0x220 its three lights, +0x230 its head's joint (+0x294 / +0x298 the pitch / yaw it looks at),
// +0x2b0 / +0x2b4 the camera's boxes where it is active, else +0x2b8 / +0x2bc the boxes where it is not (-1 none).
//
// States (the table L17:0x1e6ed0, 16): 0 set up, 1 at rest (sees the hero: 2), 2 alerted (then 5, or 6 / 7 when seen
// ahead), 3 back to rest, 4 falling (it lands: 5), 5 turns to its target, 6 / 7 on guard (one animation each), 8 turns to
// the target's point, 9 walks to it, 10 strikes, 11 walks home, 12 carried, 13 / 14 nothing, 15 knocked down.
#include <game/ovl/veldin_orbit_own.h>

#include <game/level/level_data.h>
#include <game/level/level_read.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_orbit_mobys.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his place, +0xd0 his point, +0x20a4 his vehicle mode)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the yaw from the moby toward the point `at`
float yaw_to(uint32_t m, uint32_t at) {
    return atan2_f(fpu_sub(lwc1(at + 0), lwc1(m + 0x10)), fpu_sub(lwc1(at + 4), lwc1(m + 0x14)));
}

// the hero inside the trooper's area (the level's path +0x1d0 taken as a polygon)
bool hero_in_area(uint32_t pv) {
    const uint32_t area = lvd::path(lw(pv + 0x1d0));
    return point_in_polygon(P + 0x80, area + 0x10, lvd::path_n(area)) != 0;
}
}  // namespace

// Its hits and its sight: the chase timer started (+0x38) and ticked; a hit that counts (any but the "no hit" reaction,
// not while knocked down) takes its damage, pushes it away (from a class-99 beam: away from its home) and knocks it down
// (state 15); then, unless `idle` (the camera outside its boxes), the target searched within 128, and the sight noted:
// the target within 64 and the line from it (1 up) to the hero's point free: 1, and 2 when also within 60 degrees ahead.
// orig: L17:0x002efa48
void vo_2efa48_l17(uint32_t m, int32_t idle) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    if (lw(pv + 0x38) != 0) {
        const float frames = frame_scale_f(rand_range_f(180.0f, 240.0f));
        sw(pv + 0x38, 0);
        swc1(pv + 0x1d4, static_cast<float>(float_to_int(frames)));
    }
    float_timer_tick(pv + 0x1d4);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t reaction = 0;
    float dmg = 0.0f;
    moby_hit_react_r(m, hit, pv + 0x20, 0, reaction, &dmg, nullptr, 4);
    if (reaction != 1 && lbu(m + 0x20) != 0xf) {
        sb(pv + 0x15d, 0);
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), dmg));
        swc1(pv + 0x130, fpu_mul(dt2(), 42.0f));
        swc1(pv + 0x134, fpu_mul(dt2(), 24.0f));
        sw(pv + 0x144, 9);
        swc1(pv + 0x13c, fpu_mul(dt(), u2f(0x417b3333u)));  // 15.7
        swc1(pv + 0x138, fpu_mul(dt(), 19.0f));
        swc1(pv + 0x174, -1.0f);
        swc1(pv + 0x170, -1.0f);
        const Vec4 push = V(hit + 0x10);
        float yaw = 0.0f;
        vec_yaw_marked_v(push, yaw, *static_cast<float*>(ee_ram_ptr(pv + 0x138)), *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
        if (hit != 0) {
            const uint32_t from = lwu(hit + 0x20);
            if (from != 0 && lh(from + 0xa6) == 0x63)  // a laser beam: thrown away from its home
                yaw = atan2_f(fpu_sub(lwc1(pv + 0x1f0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x1f4), lwc1(m + 0x14)));
        }
        move_state_start(m, pv + 0x120, 0xb, 1, 0, yaw);
        sb(m + 0x20, 0xf);
        sb(pv + 0x117, 0xf0);
        color_pulse_start(m, pv + 0x110);
    }
    sb(m + 0xa4, 0xff);
    if (idle != 0) return;
    if (target_select(m, pv + 0x180, 0, 0, 0, 0, 128.0f) == 2) return;
    if (!(vec3_distance(V(m + 0x10), V(pv + 0x1b0)) < 64.0f)) return;
    const float off = angle_dist(lwc1(m + 0x48), yaw_to(m, pv + 0x180));
    Vec4 eye = V(m + 0x10);
    eye.z = fpu_add(eye.z, 1.0f);
    const uint32_t seen = segment_collide_v(eye, V(P + 0xd0), 2, 0, nullptr) == 0 ? 1u : 0u;
    sb(pv + 0x1e4, off < u2f(0x3f860a92u) ? seen << 1 : seen);  // (60 degrees)
}

// A walking step toward `goal`: the yaw turned toward it, the speed (+0x1d8) eased to `speed`, the move along the yaw
// with a drop of half a frame (walker_move); returns 1 within 0.2 of the goal.
// orig: L17:0x002efd48
int32_t vo_2efd48_l17(uint32_t m, uint32_t goal, float speed) {
    const Frame f(112);
    const uint32_t step = f.sp;  // (its w lane the frame's)
    const uint32_t pv = lwu(m + 0x78);
    const float acc = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
    yaw_seek(m + 0x48, pv + 0x1dc, yaw_to(m, goal), acc, acc, fpu_mul(dt(), u2f(0x40ea927fu)));  // (7 pi / 3)
    approach_f(pv + 0x1d8, speed, fpu_mul(dt2(), 13.0f));
    swc1(step + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x1d8)));
    swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x1d8)));
    swc1(step + 8, -fpu_mul(dt(), 0.5f));
    walker_move(m, step, 0, 0.5f, 0.5f, 0.0f);
    return vec2_distance(V(m + 0x10), V(goal)) < u2f(0x3e4ccccdu) ? 1 : 0;  // (0.2)
}

// Its head looking at the hero's point from half a unit above its place: the yaw within 60 degrees of its facing, the pitch
// within 30, set on its head's joint (+0x294 / +0x298) and the joint's spring run.
// orig: L17:0x002f00a0
void vo_2f00a0_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 from = V(m + 0x10);
    from.z = fpu_add(from.z, 0.5f);
    Vec4 d;
    vec3_sub(d, V(P + 0xd0), from);
    float yaw = angle_sub(atan2_f(d.x, d.y), lwc1(m + 0x48));
    float pitch = -atan2_f(vec2_length(d), d.z);
    const float yaw_max = u2f(0x3f860a92u), pitch_max = u2f(0x3f060a92u);  // (60 and 30 degrees)
    if (yaw_max < yaw) yaw = yaw_max;
    else if (yaw < -yaw_max) yaw = -yaw_max;
    if (pitch_max < pitch) pitch = pitch_max;
    else if (pitch < -pitch_max) pitch = -pitch_max;
    const float step = lwc1(0x0015ed64);
    swc1(pv + 0x294, pitch);
    swc1(pv + 0x298, yaw);
    joint_node_spring(m, pv + 0x230, 0, fpu_mul(u2f(0x3ca3d70au), step), fpu_mul(u2f(0x3e99999au), step));  // (0.02, 0.3)
}

// Its three lights: their colour pulsing with the level's frame counter (a period of 170 frames at rest, 90 on guard or
// turning, 50 when moving or striking; nothing while carried), the moby's colour eased toward it, the lights' points
// (chain points 1 .. 3 into +0x200 .. +0x220) and the draw queued.
// orig: L17:0x002f0210
void vo_2f0210_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t period = frame_scale_i(0xaa);
    int32_t mood = 0;  // 0 at rest (states 0 .. 3), 1 states 4 .. 6, 2 the others, -1 carried
    const uint32_t st = lbu(m + 0x20);
    if (st - 4 < 12) {
        // (the table L17:0x1e6f10, indexed by the state less 4)
        switch (st - 4) {
        case 0: case 1: case 2:  // (0x002f0284)
            mood = 1;
            break;
        case 8:  // (0x002f0294)
            mood = -1;
            break;
        default:  // (0x002f028c)
            mood = 2;
            break;
        }
    }
    if (mood < 0) return;
    if (mood == 2) period = frame_scale_i(0x32);
    else if (mood == 1) period = frame_scale_i(0x5a);
    const float span = int_to_float(period);
    const int32_t phase = ee_rem(lw(L17_ADDR(0x0015f5cc)), period);
    float x = fpu_div(static_cast<float>(phase), span);
    x = fpu_add(x, x);
    x = fpu_mul(x, u2f(0x40490fdbu));
    x = fpu_add(x, u2f(0xc0490fdbu));
    const float s = sin_vu(x);
    int32_t r = float_to_int(fpu_mul(s, 20.0f)) + 0x32;
    int32_t g = float_to_int(fpu_mul(s, 70.0f)) + 0xb4;
    const int32_t b = float_to_int(fpu_mul(s, 10.0f)) + 0x14;
    if (g >= 256) g = 0xff;
    if (r >= 256) r = 0xff;
    uint32_t colour = 0x80000000u | (static_cast<uint32_t>(b) << 16);
    if (mood == 0) colour |= (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
    else colour |= (static_cast<uint32_t>(r / 2) << 8) | static_cast<uint32_t>(g);
    sw(m + 0x90, rgba_lerp(lwu(m + 0x90), colour, u2f(0x3dcccccdu)));  // (0.1)
    for (uint32_t k = 0; k < 3; ++k) moby_chain_point_v(m, k + 1, V(pv + 0x200 + k * 0x10));
    sub_001f93e0(L17_ADDR(0x002f0448), m);  // (class1382_glows_draw)
}

// orig: L17:0x002eeb68
void vo_class1382_update_l17(uint32_t m) {
    const Frame f(160);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam = L17_ADDR(0x001676c0);  // the camera's place

    // idle: the camera outside the boxes where it is active (+0x2b0 / +0x2b4), or inside one where it is not (+0x2b8; the
    // original tests box -1 there, +0x2bc)
    int32_t idle = 0;
    if (lbu(m + 0x20) != 0xc) {
        if (lw(pv + 0x2b0) != -1) {
            idle = point_in_box(cam, lw(pv + 0x2b0)) == 0 ? 1 : 0;
            const int32_t b1 = lw(pv + 0x2b4);
            if (b1 != -1 && point_in_box(cam, b1) != 0) idle = 0;
        } else if (lw(pv + 0x2b8) != -1) {
            idle = point_in_box(cam, -1);
            const int32_t b3 = lw(pv + 0x2bc);
            if (b3 != -1) idle |= point_in_box(cam, b3);
        }
    }
    vo_2efa48_l17(m, idle);
    if (idle != 0 && lbu(m + 0x20) != 0xf) {  // hidden (but a knocked-down one still falls)
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        return;
    }
    sb(m + 0x31, 1);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffe);
    vo_2f00a0_l17(m);
    vo_2f0210_l17(m);
    const uint32_t tgt = lwu(pv + 0x1c0);
    moby_set_70(pv + 0x230, u2f(0x40066666u));  // (2.1)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(cam)) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }

    auto anim_to = [m](uint32_t set) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(10));
    };
    auto go = [m, anim_to](uint32_t state, uint32_t set) {
        sb(m + 0x20, state);
        anim_to(set);
    };
    // the turn toward its target (state 5: its turning animation, the two countdowns)
    auto turn = [m, pv, anim_to](uint32_t state) {
        sb(m + 0x20, state);
        anim_to(3);
        sh(pv + 0x1e2, static_cast<uint32_t>(frame_scale_i(0x78)));
        sh(pv + 0x1e0, static_cast<uint32_t>(frame_scale_i(0x78)));
    };
    // on guard: one animation (state 6) in a vehicle mode 3, the other (state 7) otherwise
    auto guard = [go] {
        if (lbu(P + 0x20a4) == 3) go(6, 7);
        else go(7, 8);
    };
    // at rest: an idle animation from a random frame of its first `less` .. (the model's animation count +0x10)
    auto rest = [m](uint32_t state, int32_t less) {
        sb(m + 0x20, state);
        if (lbu(m + 0x53) == 0) return;
        const int32_t n = static_cast<int32_t>(lbu(lwu(lwu(m + 0x24) + 0x48) + 0x10)) - less;
        moby_anim_set(m, 0, rand_mod(n), frame_scale_i(10));
    };

    // (the table L17:0x1e6ed0)
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002eecf0): set up
        sh(m + 0x34, lhu(m + 0x34) | 0x1000);
        lq_sq(pv + 0x1f0, m + 0x10);
        sb(pv + 0x58, 8);
        sb(pv + 0x5a, 7);
        sb(pv + 0x29, 0);
        sh(pv + 0x24, 1);
        swc1(pv + 0x20, 1.0f);
        sw(pv + 0xd0, L17_ADDR(0x00162200));
        swc1(pv + 0x1ec, rand_range_f(fpu_mul(dt(), u2f(0x40e66666u)), fpu_mul(dt(), u2f(0x410b3333u))));  // (7.2 .. 8.7)
        sb(m + 0x20, 1);
        moby_anim_set(m, 0, rand_mod(lbu(lwu(lwu(m + 0x24) + 0x48) + 0x10)), 0);
        if (lw(pv + 0x1e8) == -1) sw(pv + 0x1e8, lwu(pv + 0x1d0));
        break;
    case 1:  // (0x002eedd0): at rest; seeing the hero, now and then alerted
        if (lbu(pv + 0x1e4) == 0) break;
        if (rand_mod(0x1f) != 0) break;
        go(2, 1);
        break;
    case 2:  // (0x002eee18): alerted, at its animation's end
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (lbu(pv + 0x1e4) == 2) {
            if (rand_mod(3) == 0) guard();
            else anim_to(2);
        } else {
            turn(5);
        }
        break;
    case 3:  // (0x002ef18c): back to rest at its animation's end
        if ((lbu(m + 0x70) & 2) != 0) rest(1, 1);
        break;
    case 4:  // (0x002ef868): falling (dropped); landed: it turns; below height 2: gone
        if ((enemy_physics(m, pv + 0x120) & 0x60) != 0) {
            turn(5);  // (the original also clears a stack vector it never reads)
        } else if (lwc1(m + 0x18) < 2.0f) {
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
        break;
    case 5: {  // (0x002eef14): turning toward its target
        if (tgt == 0) {
            if (rand_mod(0x1f) == 0) rest(3, 0);
            break;
        }
        const float acc = fpu_mul(dt2(), u2f(0x4096cbe4u));                                     // (3 pi / 2)
        yaw_seek(m + 0x48, pv + 0x1dc, yaw_to(m, tgt + 0x10), acc, acc, fpu_mul(dt(), u2f(0x405f66f3u)));  // (10 pi / 9)
        if (fpu_mul(dt(), u2f(0x3db2b8c2u)) < abs_f(lwc1(pv + 0x1dc))) anim_to(lwc1(pv + 0x1dc) < 0.0f ? 4 : 5);  // (5 degrees)
        else anim_to(3);
        swc1(m + 0x58, u2f(0x3fd9999au));  // (1.7)
        if (lbu(pv + 0x1e4) == 2 && (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1e2))) != 0 || lbu(P + 0x20a4) != 3)) {
            if (rand_mod(3) == 0) anim_to(2);
            else guard();
            break;
        }
        // (the original's 64-bit test of +0x1e0: its halfword +0x1e0 and its byte +0x1e4)
        if (lhu(pv + 0x1e0) == 0 && lbu(pv + 0x1e4) == 0) rest(3, 0);
        break;
    }
    case 6: case 7:  // (0x002ef1e8): on guard
        if (lbu(pv + 0x1e4) != 2) {
            if (rand_mod(0x14) == 0) turn(5);
            break;
        }
        if (lwc1(pv + 0x1d4) != 0.0f || hero_in_area(pv)) {
            if (rand_mod(0x14) == 0) go(8, 1);
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (rand_mod(3) == 0) anim_to(2);
        else if (rand_mod(7) == 0) anim_to(8);
        else anim_to(7);
        break;
    case 8:  // (0x002ef32c): turning toward the target's point; at its animation's end it walks (9) or goes home (11)
        yaw_turn_damped(m, pv + 0x1dc, yaw_to(m, pv + 0x180), u2f(0x3cf5c28fu), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40fb53d1u)));  // (0.03, 0.3, 5 pi / 2)
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (lwc1(pv + 0x1d4) == 0.0f && !hero_in_area(pv)) go(0xb, 9);
        else go(9, 9);
        break;
    case 9: {  // (0x002ef43c): walking to the target's point
        const uint32_t goal = pv + 0x180;
        vo_2efd48_l17(m, goal, lwc1(pv + 0x1ec));
        swc1(m + 0x58, u2f(0x3fe66666u));  // (1.8)
        if (!hero_in_area(pv) && lwc1(pv + 0x1d4) == 0.0f) {
            sb(m + 0x20, 0xb);
            break;
        }
        if (vec2_distance(V(m + 0x10), V(goal)) < u2f(0x3fe66666u)) {  // close: it strikes
            sb(m + 0x20, 0xa);
            if (lbu(m + 0x53) != 0xa) moby_anim_set(m, 0xa, 5, frame_scale_i(10));
            break;
        }
        if (32.0f < vec3_distance(V(goal), V(pv + 0x1f0)) || 3.0f < abs_f(fpu_sub(lwc1(pv + 0x188), lwc1(m + 0x18))))
            sb(m + 0x20, 0xb);  // too far from home, or the target too far above or below
        break;
    }
    case 10: {  // (0x002ef56c): the strike, at its animation's frame 17.5
        if ((lbu(m + 0x70) & 2) != 0) {
            go(0xb, 9);
            break;
        }
        const uint32_t goal = pv + 0x180;
        if (!(abs_f(fpu_sub(moby_anim_time(m), 17.5f)) <= 0.25f)) break;
        if (!(abs_f(fpu_sub(lwc1(pv + 0x188), lwc1(m + 0x18))) < 1.0f)) break;
        if (!(angle_dist(yaw_to(m, goal), lwc1(m + 0x48)) < u2f(0x3e860a92u))) break;  // (15 degrees)
        const uint32_t victim = lwu(pv + 0x1c0);
        if (victim == 0) break;
        if (!(vec2_distance(V(m + 0x10), V(goal)) < 2.75f)) break;
        const uint32_t dir = sp + 0x10, at = sp + 0x20;  // (the original's: the dir's w lane the frame's)
        const float k = u2f(0x3e4ccccdu);  // (0.2)
        swc1(dir + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), k));
        swc1(dir + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), k));
        sw(dir + 8, 0);
        lq_sq(at, goal);
        swc1(at + 8, fpu_add(lwc1(at + 8), 0.75f));
        sub_00259bc8(victim, m, 1, at, dir, 1.0f);
        break;
    }
    case 11: {  // (0x002ef6ec): walking home; there (or the target near enough inside its area) it turns
        const uint32_t home = pv + 0x1f0;
        vo_2efd48_l17(m, home, fpu_mul(dt(), 5.0f));
        swc1(m + 0x58, u2f(0x3faccccdu));  // (1.35)
        const float d = vec2_distance(V(m + 0x10), V(home));
        if ((d < u2f(0x3fb33333u) && (lbu(m + 0x70) & 2) != 0) || d < u2f(0x3e99999au)) {  // (1.4, 0.3)
            turn(5);
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        const float to_goal = vec2_distance(V(m + 0x10), V(pv + 0x180));
        const float far = frame_scale_f(40.0f);
        const float near = frame_scale_f(10.0f);
        const float v = fpu_mul(lwc1(pv + 0x1d8), dt());
        const float reach = fpu_add(fpu_add(fpu_mul(fpu_mul(v, far), 0.5f), fpu_mul(v, near)), 1.5f);
        if (!(to_goal < reach)) break;
        if (!hero_in_area(pv)) break;
        go(0xa, 0xa);
        break;
    }
    case 12:  // (0x002eedb0): carried by the Suck Cannon; let go: set up again
        if (carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, 0);
            sh(pv + 0xc8, 0);
        }
        break;
    case 15:  // (0x002ef8f8): knocked down: it falls; landed: it bursts; below height 2: gone
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        if ((enemy_physics(m, pv + 0x120) & 0x60) != 0) {
            const uint32_t still = L17_ADDR(0x0015f580);  // (a zero vector: the debris' velocity, spin and size)
            explosion_burst(m, m + 0x10, -1, 0.5f, 13.0f);
            debris_spawn(m, 0x6ce, m + 0x10, m + 0x40, 0, 0, still, still, still, 0.0f);
            debris_spawn(m, 0x6cf, m + 0x10, m + 0x40, 0, 0, still, still, still, 0.0f);
            debris_burst(m, 0x77d, 1, 0x77d, 1, 3, 2);
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        } else if (lwc1(m + 0x18) < 2.0f) {
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
        break;
    default:  // 13, 14 (0x002efa20) and beyond the table: nothing
        break;
    }
}

}  // namespace rac1
