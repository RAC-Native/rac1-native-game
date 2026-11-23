// Umbris's own moby class 1059, the walking enemy that keeps to its area (a polygon path), wanders, turns from walls,
// chases the hero when he is in its area and attacks him, and is knocked about or killed by his hits — hand-ported from
// Umbris's own overlay with its thirteen helpers (0x30e720 .. 0x30f540). Header umbris_mobys.h, registered in
// umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are bare (the frame
// time 0x15ed6c and its square 0x15ed70, the speed 0x15ed60, the hero 0x13f3d0 and his moby 0x1413d0); the hit record the
// collision facades'.
//
// Its pvars: its hit state +0x20 (health +0x20), its colour pulse +0x60, its move record +0x70 (a knock-back's), its
// velocity +0xd0 (+0xd8 vertical), its home +0xe0, its target place +0xf0, its area's path +0x110 (its points +0x128), the
// floor moby +0x118 (its height +0x12c, the floor under it +0x130, the height it keeps +0x138), its place +0x114, its
// timer +0x134, its heading +0x13c, its turn step +0x140, its chase path +0x144 (+0x148), its shove +0x14c, its turn
// rates +0x120 / +0x124, its joint node +0x150.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the knock-back's move record at `rec` (+0x10 .. +0x54: the move engine's rates) started from the push `push`:
// `set` its animation, `turn` its last word (0x400 / 0x599), `bounce` / `lo` / `hi` its +0x28 / +0x50 / +0x54
void knock_start(uint32_t m, uint32_t rec, uint32_t push, uint32_t turn, float bounce, float lo, float hi, uint32_t set) {
    const float dt = lwc1(0x0015ed6c);
    const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x41eb3334u));
    swc1(rec + 0x10, g);
    swc1(rec + 0x14, u2f(0x3a03126fu));
    swc1(rec + 0x18, fpu_mul(dt, 9.0f));
    swc1(rec + 0x1c, fpu_mul(dt, 7.0f));
    sw(rec + 0x20, turn);
    swc1(rec + 0x28, bounce);
    swc1(rec + 0x30, u2f(0x3e75c28fu));
    swc1(rec + 0x34, 0.5f);
    swc1(rec + 0x38, u2f(0x3f266666u));
    swc1(rec + 0x48, u2f(0x3e4ccccdu));
    swc1(rec + 0x4c, u2f(0x3c23d70au));
    swc1(rec + 0x50, lo);
    swc1(rec + 0x54, hi);
    sw(rec + 0x24, 0xd);
    sb(rec + 0x3d, 0);
    // (the yaw out and the two rates in / out: the original's stack and the record, by address)
    const uint32_t sp = ee_stack_alloc(0x20);
    V(sp) = V(push);
    vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x10)), *static_cast<float*>(ee_ram_ptr(rec + 0x18)),
                     *static_cast<float*>(ee_ram_ptr(rec + 0x1c)));
    const float yaw = lwc1(sp + 0x10);
    ee_stack_free(0x20);
    move_state_start(m, rec, set, 1, 0, yaw);
}
}  // namespace

// The knock-back of a kill (animation 0xc).
// orig: L7:0x0030e720
void umb_30e720_l7(uint32_t m, uint32_t rec, uint32_t push) {
    knock_start(m, rec, push, 0x400, 1.0f, 3.0f, 9.0f, 0xc);
}

// The knock-back of a hard hit (animation 6).
// orig: L7:0x0030e858
void umb_30e858_l7(uint32_t m, uint32_t rec, uint32_t push) {
    knock_start(m, rec, push, 0x599, u2f(0x3fb33333u), 8.0f, 19.0f, 6);
}

// Its hits (none while it is being killed, +0x20 0xe): only when it is awake (+0xbc); the reaction (the table L7:0x2116f0:
// 1, 2 its health's hit reset; 3 .. 8 a hard hit: knocked back, state 0xc; 9, 10 a stun: animation 4, state 0xd), then
// the damage off its health: none left, killed (its draw flag 0x1000 off, knocked back, bolts, state 0xe).
// orig: L7:0x0030e998
void umb_30e998_l7(uint32_t m, uint32_t pv, uint32_t hs) {
    if (lbu(m + 0x20) == 0xe) return;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000), 0);
    int32_t state = 0;
    const int32_t r = moby_hit_react_r(m, hit, hs, 0, state, nullptr, nullptr, 4);
    if (lbu(m + 0xbc) == 0) {
        sb(m + 0xa4, 0xff);
        return;
    }
    if (r == 1 || r == 2) {
        sw(hs, 0);
    } else if (r >= 3 && r <= 8) {
        umb_30e858_l7(m, pv + 0x70, hit + 0x10);
        sb(m + 0x20, 0xc);
    } else if (r == 9 || r == 10) {
        if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 4, 4);
        sb(m + 0x20, 0xd);
    }
    if (state >= 2) {
        const float health = lwc1(hs), dmg = lwc1(hit + 0x2c);
        if (health <= dmg) {
            sw(hs, 0);
            sh(m + 0x34, lhu(m + 0x34) & 0xefff);
            umb_30e720_l7(m, pv + 0x70, hit + 0x10);
            moby_drop_bolts(m, 0, -1);
            sb(pv + 0x67, 0x78);
            color_pulse_start(m, pv + 0x60);
            sb(m + 0x20, 0xe);
        } else {
            swc1(hs, fpu_sub(health, dmg));
            sb(pv + 0x67, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(60)));
            color_pulse_start(m, pv + 0x60);
        }
    }
    sb(m + 0xa4, 0xff);
}

// Its footing: the floor under it (+0x130), the height it keeps (+0x138, half way up to the floor moby's height +0x12c);
// 2 when that height is under 0.41 above the floor, 0 over 0.68, else 1 (the animations' sets by footing).
// orig: L7:0x0030ebc8
int32_t umb_30ebc8_l7(uint32_t m, uint32_t pv) {
    const float floor = floor_probe_20(m + 0x10);
    swc1(pv + 0x130, floor);
    const float up = fpu_sub(lwc1(lwu(pv + 0x12c)), floor);
    swc1(pv + 0x138, fpu_add(floor, fpu_mul(up, 0.5f)));
    if (up < u2f(0x3ed1eb85u)) return 2;
    return u2f(0x3f2e147bu) < up ? 0 : 1;
}

// Its rest animation and state by its footing +0xbc (0: animation 0, state 1; 1: 3, 7; 2: 9, 15).
// orig: L7:0x0030ec58
void umb_30ec58_l7(uint32_t m) {
    switch (lbu(m + 0xbc)) {
        case 0:
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0xa);
            sb(m + 0x20, 1);
            break;
        case 1:
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, 0xa);
            sb(m + 0x20, 7);
            break;
        case 2:
            if (lbu(m + 0x53) != 9) moby_anim_set(m, 9, 0, 0xa);
            sb(m + 0x20, 0xf);
            break;
        default:
            break;
    }
}

// A closed path's segment lengths (each point's +0xc; none for no path).
// orig: L7:0x0030eb38
void umb_30eb38_l7(uint32_t path) {
    if (path == 0) return;
    for (int32_t i = 0; i < lw(path); ++i) {
        const int32_t next = ee_rem(i + 1, lw(path));
        swc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c,
             vec3_distance(V(path + (static_cast<uint32_t>(i) << 4) + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10)));
    }
}

// Its set-up: its two paths, its floor moby, its footing, its place (the level's places 0x1600ec +0x30), its home, its rest
// state, a first wait of 0 .. 119 frames and a turn step of 20 degrees either way. 1 (with the debug message) when one of
// them is missing.
// orig: L7:0x0030ed30
int32_t umb_30ed30_l7(uint32_t m, uint32_t pv) {
    if (lw(pv + 0x110) == -1) {
        debug_printf_stub();  // (no path: L7:0x211720)
        return 1;
    }
    const uint32_t area = lvd::path(lw(pv + 0x110));
    sw(pv + 0x128, area);
    umb_30eb38_l7(area);
    if (lw(pv + 0x144) == -1) {
        debug_printf_stub();  // (L7:0x211758)
        return 1;
    }
    const uint32_t chase = lvd::path(lw(pv + 0x144));
    sw(pv + 0x148, chase);
    umb_30eb38_l7(chase);
    if (lw(pv + 0x118) == -1) {
        debug_printf_stub();  // (L7:0x211790)
        return 1;
    }
    sw(pv + 0x12c, lwu(L7_ADDR(0x0015ffd8)) + (lwu(pv + 0x118) << 8) + 0x18);
    sb(m + 0xbc, static_cast<uint32_t>(umb_30ebc8_l7(m, pv)));
    if (lw(pv + 0x114) == -1) {
        debug_printf_stub();  // (L7:0x2117c0)
        return 1;
    }
    V(pv + 0xf0) = V(lwu(L7_ADDR(0x001600ec)) + (lwu(pv + 0x114) << 7) + 0x30);
    V(pv + 0xe0) = V(m + 0x10);
    umb_30ec58_l7(m);
    sw(pv + 0x134, static_cast<uint32_t>(ee_rem(libc_rand(), 0x78)));
    swc1(pv + 0x140, (libc_rand() & 1) == 0 ? u2f(0xbeb2b8c2u) : u2f(0x3eb2b8c2u));
    sw(pv + 0x14c, 0);
    sb(pv + 0x29, 1);
    return 0;
}

namespace {
// the point 5.2 ahead of the moby along `yaw` (its z the moby's) at `at`; whether it is in its area and free (no wall
// between, nothing within 1.5)
bool ahead_free(uint32_t m, uint32_t pv, float yaw, uint32_t at) {
    swc1(at + 0x0, fpu_mul(cos_vu(yaw), u2f(0x40a66666u)));
    const float s = fpu_mul(sin_vu(yaw), u2f(0x40a66666u));
    sw(at + 0x8, 0);
    swc1(at + 0x4, s);
    vec3_add(V(at), V(at), V(m + 0x10));
    const uint32_t area = lwu(pv + 0x128);
    if (point_in_polygon(at, area + 0x10, lw(area)) == 0) return false;
    if (segment_collide_v(V(m + 0x10), V(at), 0x24, m, nullptr) != 0) return false;
    return sphere_collide(at, 1.5f, 0x24, m) == 0;
}
}  // namespace

// Its heading: while its timer runs, straight on when the way ahead is free, else the first free way turning by its
// step (a whole turn at most); none free, or its timer out: toward its target place +0xf0.
// orig: L7:0x0030eee8
void umb_30eee8_l7(uint32_t m, uint32_t pv) {
    if (lw(pv + 0x134) != 0) {
        // (the probe's point: the original's stack; its w lane is the stack's, read by nothing)
        const uint32_t sp = ee_stack_alloc(0x10);
        if (ahead_free(m, pv, lwc1(m + 0x48), sp)) {
            ee_stack_free(0x10);
            return;
        }
        float turned = 0.0f;
        const float step = abs_f(lwc1(pv + 0x140));
        float yaw = lwc1(m + 0x48);
        float by = lwc1(pv + 0x140);
        for (;;) {
            yaw = angle_add(yaw, by);
            if (ahead_free(m, pv, yaw, sp)) {
                swc1(pv + 0x13c, yaw);
                ee_stack_free(0x10);
                return;
            }
            turned = fpu_add(turned, step);
            if (!(turned < u2f(0x40c90fdbu))) break;
            by = lwc1(pv + 0x140);
        }
        ee_stack_free(0x10);
    }
    swc1(pv + 0x13c, atan2_f(fpu_sub(lwc1(pv + 0xf0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0xf4), lwc1(m + 0x14))));
}

// Its vertical move: falling (9.8) while above its floor moby's height less 0.3, its height eased toward +0x138 (rates
// `turn`, `accel`, `vmax`), then the walker's move (half the velocity kept after a bump).
// orig: L7:0x0030f0f8
void umb_30f0f8_l7(uint32_t m, uint32_t pv, float turn, float accel, float vmax) {
    if (fpu_sub(lwc1(lwu(pv + 0x12c)), u2f(0x3e99999au)) < lwc1(m + 0x18))
        swc1(pv + 0xd8, fpu_sub(lwc1(pv + 0xd8), fpu_mul(lwc1(0x0015ed6c), u2f(0x411ccccdu))));
    // (the eased height works on a copy of its place: the original's stack)
    const uint32_t sp = ee_stack_alloc(0x10);
    V(sp) = V(m + 0x10);
    seek_1d(sp + 0x8, pv + 0xd8, lwc1(pv + 0x138), turn, accel, vmax);
    ee_stack_free(0x10);
    if (walker_move_fx(m, pv + 0xd0, 0x400, 0x10, 1.0f, 0.0f) != 0) vec3_scale(V(pv + 0xd0), V(pv + 0xd0), 0.5f);
}

// Its speed along its heading: up by `accel` within `near` of the heading `yaw` (1), down by `decel` past `far`, kept
// between `lo` and `hi`; the velocity +0xd0 along its facing +0x48.
// orig: L7:0x0030f1e0
int32_t umb_30f1e0_l7(uint32_t m, uint32_t pv, float yaw, float near, float accel, float far, float decel, float hi, float lo) {
    float speed = vec2_length(V(pv + 0xd0));
    int32_t facing = 0;
    const float d = angle_dist(lwc1(m + 0x48), yaw);
    if (d < near) {
        speed = fpu_add(speed, accel);
        facing = 1;
    } else if (far < d) {
        speed = fpu_sub(speed, decel);
    }
    if (hi < speed) speed = hi;
    else if (speed < lo) speed = lo;
    swc1(pv + 0xd0, fpu_mul(cos_vu(lwc1(m + 0x48)), speed));
    swc1(pv + 0xd4, fpu_mul(sin_vu(lwc1(m + 0x48)), speed));
    return facing;
}

// Its attack: in state `state` with animation `anim`, between the animation's times `t0` and `t1`, a push of `len` along
// its velocity (a hit of `strength` made), and `target` within `radius` of its chain point 0: bounced back (its velocity
// turned, a hop of 0.05 of the speed), animation `anim2` (`frame2`, 5 frames), state `next`.
// orig: L7:0x0030f308
void umb_30f308_l7(uint32_t m, uint32_t pv, uint32_t state, uint32_t anim, uint32_t anim2, int32_t frame2, uint32_t next,
                   uint32_t target, float t0, float t1, float len, float strength, float radius) {
    const float t = moby_anim_time(m);
    if (lbu(m + 0x20) != state || lbu(m + 0x52) != anim) return;
    if (!(t0 < t) || !(t < t1)) return;
    // (the push and the hit record: the original's stack, by address)
    const uint32_t sp = ee_stack_alloc(0x50);
    vec3_set_length(V(sp), V(pv + 0xd0), len);
    swc1(sp + 0x8, 1.0f);
    sub_00259888(strength, sp + 0x10, static_cast<int32_t>(m), 1, sp);
    moby_chain_point_v(m, 0, V(sp + 0x40));
    const int32_t n = mobys_in_sphere(sp + 0x40, radius, 0x21, m, sp + 0x10);
    ee_stack_free(0x50);
    if (n == 0 || coll_hit_moby() != target) return;
    vec3_scale(V(pv + 0xd0), V(pv + 0xd0), -1.0f);
    swc1(pv + 0xd8, -fpu_mul(lwc1(0x0015ed60), u2f(0x3d4ccccdu)));
    if (lbu(m + 0x53) != anim2) moby_anim_set(m, anim2, frame2, 5);
    sb(m + 0x20, next);
}

// Whether `target` is within `radius` of its chain point 0 (the first moby the sphere meets).
// orig: L7:0x0030f4d0
bool umb_30f4d0_l7(uint32_t m, uint32_t target, float radius) {
    const uint32_t sp = ee_stack_alloc(0x10);
    moby_chain_point_v(m, 0, V(sp));
    const bool hit = sphere_collide(sp, radius, 0x21, m) != 0 && coll_hit_moby() == target;
    ee_stack_free(0x10);
    return hit;
}

// Whether another member of its group (+0x21) is in a state of `lo` .. `hi`.
// orig: L7:0x0030f540
bool umb_30f540_l7(uint32_t m, int32_t lo, int32_t hi) {
    const uint32_t sp = ee_stack_alloc(0x10);  // (the group's iterator: the original's stack)
    sw(sp, 0);
    bool found = false;
    if (group_first(sp, lbu(m + 0x21), 0, 0) == 0) {
        for (uint32_t o = lwu(sp); o != 0; o = lwu(sp)) {
            if (o != m) {
                const int32_t s = static_cast<int32_t>(lbu(o + 0x20));
                if (!(s < lo) && !(hi < s)) {
                    found = true;
                    break;
                }
            }
            if (group_next(sp, o, 0, 0) != 0) break;
        }
    }
    ee_stack_free(0x10);
    return found;
}

namespace {
const float SLOW = u2f(0xbd4cccd0u);  // (-0.05: the velocity's damping per unit of 0x15ed60)
float dt() { return lwc1(0x0015ed6c); }
float dt2() { return lwc1(0x0015ed70); }
uint32_t hero_moby() { return lwu(0x001413d0); }
void anim(uint32_t m, uint32_t set, int32_t frame, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frames);
}
// a new wait of `base` + 0 .. `span`-1 frames
void wait_rand(uint32_t pv, int32_t span, int32_t base) { sw(pv + 0x134, static_cast<uint32_t>(ee_rem(libc_rand(), span) + base)); }
// its velocity damped (`k` a unit of 0x15ed60)
void damp(uint32_t pv, float k) { vec3_scale(V(pv + 0xd0), V(pv + 0xd0), fpu_add(fpu_mul(lwc1(0x0015ed60), k), 1.0f)); }
// falling (9.8) and the walker's move; half the velocity kept after a bump when `halve`
void fall_move(uint32_t m, uint32_t pv, bool halve) {
    swc1(pv + 0xd8, fpu_sub(lwc1(pv + 0xd8), fpu_mul(dt(), u2f(0x411ccccdu))));
    if (walker_move_fx(m, pv + 0xd0, 0x400, 0x10, 1.0f, 0.0f) != 0 && halve) vec3_scale(V(pv + 0xd0), V(pv + 0xd0), 0.5f);
}
// the turn toward `yaw` and the pitch toward `pitch` (+0x44), at the rates `k` (accel, damp) and `vmax` of the frame's time
void turn(uint32_t m, uint32_t pv, float yaw, float ka, float kd, float kv) {
    yaw_turn_damped(m, pv + 0x120, yaw, fpu_mul(dt2(), ka), fpu_mul(dt2(), kd), fpu_mul(dt(), kv));
}
float pitch_seek(uint32_t m, uint32_t pv, float target, float ka, float kd, float kv) {
    return angle_seek_rate(pv + 0x124, lwc1(m + 0x44), target, fpu_mul(dt2(), ka), fpu_mul(dt2(), kd), fpu_mul(dt(), kv));
}
}  // namespace

// Class 1059 (+0x20, the table L7:0x211860): 0 set up (0x30ed30; none: retired); 1 resting / wandering in its area, 2
// roaming toward its heading, 3 nothing, 4 chasing the hero in its chase area, 5 grabbing him (pulled to its chain point
// 0 over 1/0.028 frames: then he is held, 6), 6 carrying him, 7 / 8 / 9 / 10 the low footing's rest, roam and attack
// (10: its bite, 0x30f308), 11 .. 17 knocked back, stunned, killed (14: the explosion, retired) and their ends. Every
// frame: its hits (0x30e998), the footing's change (the tables 0x161b40 / 0x161b50: the new state and animation), the
// splashes and spray of the water it walks in (the level's water grid 0x1612d0), the hero held at its place once the grab
// is done (+0x14c), out of the level's box: retired.
// orig: L7:0x0030f5f0
void umb_class1059_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    joint_node_scale(m, 0, pv + 0x150, u2f(0x400ccccdu));
    const uint32_t pos = m + 0x10;
    if (lbu(m + 0x20) == 0) {
        if (umb_30ed30_l7(m, pv) != 0) {
            debug_printf_stub();  // (L7:0x2117f8)
            moby_retire(m);
            return;
        }
    } else {
        // (the original's frame: the target record +0x20 .. +0x67, the chain point +0x70, the grab's vectors +0x80 .. +0xbf)
        const uint32_t F = ee_stack_alloc(0xc0);
        const uint32_t tgt = F + 0x20, chain = F + 0x70;
        target_select_any(m, tgt, 30.0f);
        umb_30e998_l7(m, pv, pv + 0x20);
        color_pulse_update(m, pv + 0x60);
        const bool timer_out = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x134))) != 0;
        bool in_chase;  // the target (or the hero) in its chase area
        if (lw(pv + 0x110) != 0 && !(lbu(m + 0x20) < 7)) {
            const uint32_t ch = lwu(pv + 0x148);
            in_chase = point_in_polygon(tgt, ch + 0x10, lw(ch)) != 0;
        } else {
            const uint32_t ch = lwu(pv + 0x148);
            in_chase = point_in_polygon(0x0013f3d0, ch + 0x10, lw(ch)) != 0;
            if (vec2_distance(V(pos), V(0x0013f3d0)) < 30.0f) {  // the hero as its target
                sw(F + 0x60, hero_moby());
                V(F + 0x20) = V(0x0013f3d0);
                V(F + 0x30) = V(0x0013f3e0);
                V(F + 0x40) = V(0x0013f410);
                V(F + 0x50) = V(0x0013f420);
                sw(F + 0x64, 0);
            } else {
                sw(F + 0x60, 0);
                for (uint32_t o = 0x20; o < 0x60; o += 0x10) V(F + o) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
                sw(F + 0x64, 2);
            }
        }
        if (lw(pv + 0x110) != 0) {
            const uint32_t area = lwu(pv + 0x128);
            point_in_polygon(pos, area + 0x10, lw(area));  // (its answer unused)
        }
        // the footing: a change gives its new state and animation
        const uint32_t was = lbu(m + 0xbc);
        uint32_t next_state = 0xff, next_anim = 0xff;
        const int32_t foot = umb_30ebc8_l7(m, pv);
        sb(m + 0xbc, static_cast<uint32_t>(foot));
        if (was != (static_cast<uint32_t>(foot) & 0xff)) {
            const uint32_t k = (static_cast<uint32_t>(foot) & 0xff) + was * 3;
            next_anim = lbu(L7_ADDR(0x00161b50) + k);
            next_state = lbu(L7_ADDR(0x00161b40) + k);
        }
        // in water: its splash on the grid (one frame in 45, by its slot +0xb2, one in two) and its spray (one frame in 20 .. 27)
        moby_chain_point_v(m, 0, V(chain));
        if (sphere_collide(chain, u2f(0x3f4ccccdu), 1, m) != 0 && sub_001f35d8() == 0) {
            const float half = fpu_mul(fpu_sub(lwc1(lwu(pv + 0x12c)), lwc1(pv + 0x130)), 0.5f);
            float depth;
            if (half < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(chain + 0x8))))
                depth = fpu_mul(fpu_sub(lwc1(lwu(pv + 0x12c)), lwc1(pv + 0x130)), 0.5f);
            else
                depth = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(chain + 0x8)));
            if (lwc1(pv + 0xd8) < 0.0f) depth = -depth;
            if (lbu(m + 0x20) == 0xe || lbu(m + 0x20) == 0xc) depth = fpu_add(depth, depth);
            const Vec4 hitp = coll_hit_point();
            float radius = 1.0f;
            if (0.5f < vec2_distance(V(pos), hitp)) {
                const float d = vec2_distance(V(pos), coll_hit_point());
                radius = fpu_add(d, d);
            }
            if (ee_rem(lw(L7_ADDR(0x0015f5cc)), 0x2d) == lh(m + 0xb2) && rand_mod(2) == 0)
                region_height_splash(lwu(L7_ADDR(0x001612d0)), lw(L7_ADDR(0x001612d8)), 1, lwc1(m + 0x10), lwc1(m + 0x14), radius, depth);
            const int32_t every = (libc_rand() & 7) + 0x14;
            if (ee_rem(lw(L7_ADDR(0x0015f5cc)), every) == 0) {
                const float size = rand_range_f(1.5f, u2f(0x4019999au));
                const float dx = rand_range_f(u2f(0xbe19999au), u2f(0x3e19999au));
                swc1(chain + 0x0, fpu_add(lwc1(chain + 0x0), dx));
                const float dy = rand_range_f(u2f(0xbe19999au), u2f(0x3e19999au));
                swc1(chain + 0x4, fpu_add(lwc1(chain + 0x4), dy));
                swc1(chain + 0x8, surface_height(coll_hit_point_addr(), 0));
                spawn_particle_2d(chain, 0, 0xffffffffu, size, 5250.0f);
            }
        }
        // the hero held at its place once the grab is done
        if (1.0f <= lwc1(pv + 0x14c)) V(0x0013f3d0) = V(pos);
        const uint32_t target = lwu(F + 0x60);
        const bool to_hero = target != 0 && target == hero_moby();
        bool tail = false;  // (the footing's new state and animation, when it changed)
        switch (lbu(m + 0x20)) {
            case 0:  // L7:0x30fac4
                debug_printf_stub();  // (L7:0x211838)
                break;
            case 1: {  // L7:0x30fad8: resting
                if ((lbu(m + 0x70) & 2) != 0) anim(m, 0, 0, 0xa);
                bool grabbed = false;
                if (in_chase && target != 0) {
                    if (target == hero_moby() && !umb_30f540_l7(m, 4, 6)) {
                        anim(m, 1, 0, 0xa);
                        sb(m + 0x20, 4);
                    }
                }
                if (in_chase && target == 0) {
                    // (no target: straight to its timer)
                } else if (to_hero && umb_30f4d0_l7(m, target, 1.5f)) {
                    anim(m, 2, 0, 2);
                    sb(m + 0x20, 5);
                    grabbed = true;
                }
                if (!grabbed && timer_out) {
                    anim(m, 1, 0, 0xa);
                    wait_rand(pv, 200, 300);
                    swc1(pv + 0x13c, rand_angle());
                    sb(m + 0x20, 2);
                }
                damp(pv, SLOW);
                umb_30f0f8_l7(m, pv, fpu_mul(dt2(), 15.0f), fpu_mul(dt2(), 50.0f), fpu_mul(dt(), 8.0f));
                tail = true;
                break;
            }
            case 2: {  // L7:0x30fc90: roaming
                if ((lbu(m + 0x70) & 2) != 0) anim(m, 1, 0, 0xa);
                bool turn_done = false;
                if (in_chase && to_hero) {
                    if (umb_30f540_l7(m, 4, 6)) {
                        turn_done = true;  // (another of its group on him: no turn)
                    } else {
                        anim(m, 1, 0, 0xa);
                        sb(m + 0x20, 4);
                        turn_done = true;
                    }
                }
                if (!turn_done) {
                    turn(m, pv, lwc1(pv + 0x13c), u2f(0x3fdf66f3u), u2f(0x4091361eu), u2f(0x3fdf66f3u));
                    swc1(m + 0x44, pitch_seek(m, pv, 0.0f, u2f(0x3fdf66f3u), u2f(0x4091361eu), u2f(0x3fdf66f3u)));
                    if (timer_out && point_in_box(pos, lw(pv + 0x114)) != 0) {
                        anim(m, 0, 0, 0xa);
                        wait_rand(pv, 0x78, 0x3c);
                        sb(m + 0x20, 1);
                    }
                }
                umb_30eee8_l7(m, pv);
                if (umb_30f1e0_l7(m, pv, lwc1(pv + 0x13c), u2f(0x3e567750u), fpu_mul(dt2(), 6.0f), u2f(0x3f060a92u),
                                  fpu_mul(dt2(), 4.0f), fpu_mul(dt(), 7.0f), fpu_mul(dt(), 3.0f)) != 0 && timer_out) {
                    anim(m, 0, 0, 0xa);
                    wait_rand(pv, 0x78, 0x3c);
                    sb(m + 0x20, 1);
                }
                if (to_hero && umb_30f4d0_l7(m, target, 1.5f)) {
                    anim(m, 2, 0, 2);
                    sb(m + 0x20, 5);
                }
                umb_30f0f8_l7(m, pv, fpu_mul(dt2(), 15.0f), fpu_mul(dt2(), 50.0f), fpu_mul(dt(), 8.0f));
                tail = true;
                break;
            }
            case 4: {  // L7:0x30ff58: chasing
                const float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x4), lwc1(m + 0x14)));
                const float four_pi = u2f(0x41490fdbu);
                turn(m, pv, yaw, four_pi, 0.0f, four_pi);
                const float pitch = atan2_f(vec2_distance(V(pos), V(tgt)), fpu_sub(lwc1(tgt + 0x8), lwc1(m + 0x18)));
                swc1(m + 0x44, pitch_seek(m, pv, -pitch, four_pi, 0.0f, four_pi));
                const bool close = vec2_distance(V(pos), V(0x0013f3d0)) < 3.0f;
                if (close) damp(pv, u2f(0xbdccccd0u));
                if (umb_30f1e0_l7(m, pv, yaw, u2f(0x3e567750u), fpu_mul(dt2(), 20.0f), u2f(0x3f060a92u), fpu_mul(dt2(), 3.0f),
                                  fpu_mul(dt(), 20.0f), fpu_mul(dt(), 5.0f)) != 0 && close) {
                    anim(m, 2, 4, 2);
                    sw(m + 0x94, 0);
                    sb(m + 0x20, 5);
                    swc1(pv + 0x14c, 0.0f);
                    V(pv + 0x100) = V(pos);
                }
                swc1(pv + 0x138, fpu_sub(lwc1(tgt + 0x8), 0.25f));
                umb_30f0f8_l7(m, pv, fpu_mul(dt2(), 38.0f), fpu_mul(dt2(), 0.0f), fpu_mul(dt(), 38.0f));
                if (!(in_chase && lh(0x0013f65c) != 0)) {
                    anim(m, 1, 0, 0xa);
                    sb(m + 0x20, 2);
                }
                tail = true;
                break;
            }
            case 5: {  // L7:0x31019c: grabbing
                moby_chain_point_v(m, 0, V(chain));
                vec3_sub(V(F + 0x80), V(chain), V(pos));
                const float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(chain + 0x0)), fpu_sub(lwc1(tgt + 0x4), lwc1(chain + 0x4)));
                const float four_pi = u2f(0x41490fdbu);
                turn(m, pv, yaw, four_pi, 0.0f, four_pi);
                swc1(m + 0x44, fpu_mul(lwc1(m + 0x44), fpu_mul(lwc1(0x0015ed60), u2f(0x3f333333u))));
                swc1(pv + 0x14c, fpu_add(lwc1(pv + 0x14c), u2f(0x3ce56042u)));
                V(F + 0xb0) = V(pos);
                vec3_sub(V(F + 0x90), V(tgt), V(F + 0x80));
                vec3_sub(V(F + 0xa0), V(F + 0x90), V(pv + 0x100));
                const float k = lwc1(pv + 0x14c) < 1.0f ? lwc1(pv + 0x14c) : 1.0f;
                vec3_scale(V(F + 0xa0), V(F + 0xa0), k);
                vec3_add(V(pos), V(F + 0xa0), V(pv + 0x100));
                debug_stub_1f6c28();
                debug_stub_1f6c28();
                vec3_sub(V(pv + 0xd0), V(pos), V(F + 0xb0));
                if (1.0f <= lwc1(pv + 0x14c)) {  // held: the hero's state 0x3d
                    sw(0x001415f8, 0);
                    hero_set_state(0x3d, 1);
                    sw(hero_moby() + 0x98, 0xffffffffu);
                    sb(0x001413f5, 1);
                    sb(m + 0x20, 6);
                } else {
                    swc1(pv + 0x13c, yaw);
                }
                if (!in_chase && lh(0x0013f65c) == 0) {
                    sw(pv + 0x14c, 0);
                    anim(m, 1, 0, 0xa);
                    sb(m + 0x20, 2);
                }
                break;
            }
            case 6: {  // L7:0x31038c: carrying
                umb_30eee8_l7(m, pv);
                umb_30f1e0_l7(m, pv, lwc1(pv + 0x13c), u2f(0x3e567750u), fpu_mul(dt2(), 6.0f), u2f(0x3f060a92u), fpu_mul(dt2(), 4.0f),
                              fpu_mul(dt(), 7.0f), fpu_mul(dt(), 3.0f));
                umb_30f0f8_l7(m, pv, fpu_mul(dt2(), 12.0f), fpu_mul(dt2(), 50.0f), fpu_mul(dt(), 7.0f));
                const float half_pi = u2f(0x3fc90fdbu), pi = u2f(0x40490fdbu);
                turn(m, pv, lwc1(pv + 0x13c), half_pi, pi, pi);
                swc1(m + 0x44, pitch_seek(m, pv, 0.0f, half_pi, pi, pi));
                if ((lbu(m + 0x70) & 2) != 0) anim(m, 1, 0, 0xa);
                tail = true;
                break;
            }
            case 7:  // L7:0x3104ec: low footing, resting
                if (in_chase) {
                    anim(m, 4, 0, 0xa);
                    sb(m + 0x20, 9);
                } else if (timer_out) {
                    anim(m, 4, 0, 0xa);
                    wait_rand(pv, 200, 300);
                    swc1(pv + 0x13c, rand_angle());
                    sb(m + 0x20, 8);
                }
                damp(pv, SLOW);
                fall_move(m, pv, true);
                tail = true;
                break;
            case 8: {  // L7:0x310610: low footing, roaming
                if (in_chase) {
                    anim(m, 4, 0, 0xa);
                    sb(m + 0x20, 9);
                } else {
                    const float q = u2f(0x3f490fdbu), h = u2f(0x3fc90fdbu);
                    turn(m, pv, lwc1(pv + 0x13c), q, h, q);
                    swc1(m + 0x44, pitch_seek(m, pv, 0.0f, q, h, q));
                    if (timer_out && point_in_box(pos, lw(pv + 0x114)) != 0) {
                        anim(m, 3, 0, 0xa);
                        wait_rand(pv, 0x78, 0x3c);
                        sb(m + 0x20, 7);
                    }
                }
                umb_30eee8_l7(m, pv);
                if (umb_30f1e0_l7(m, pv, lwc1(pv + 0x13c), u2f(0x3eb2b8c2u), fpu_mul(dt2(), 3.0f), u2f(0x3f860a92u),
                                  fpu_mul(dt2(), 9.0f), dt(), dt()) != 0 && timer_out) {
                    anim(m, 3, 0, 0xa);
                    wait_rand(pv, 0x78, 0x3c);
                    sb(m + 0x20, 7);
                }
                fall_move(m, pv, true);
                tail = true;
                break;
            }
            case 9:
            case 10: {  // L7:0x310840: low footing, attacking (10: the bite)
                const float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x4), lwc1(m + 0x14)));
                const float q = u2f(0x3f490fdbu), h = u2f(0x3fc90fdbu);
                turn(m, pv, yaw, q, h, q);
                if (umb_30f1e0_l7(m, pv, yaw, u2f(0x3f060a92u), fpu_mul(dt2(), 4.0f), h, fpu_mul(dt2(), 9.0f), fpu_mul(dt(), 1.5f), dt()) != 0 &&
                    vec2_distance(V(pos), V(tgt)) < u2f(0x3ff33333u) && lbu(m + 0x20) != 0xa) {
                    anim(m, 5, 0, 2);
                    sb(m + 0x20, 0xa);
                }
                fall_move(m, pv, true);
                sb(pv + 0x58, static_cast<uint32_t>(float_to_int(8.0f)));
                sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x4119999au))));
                if (!in_chase) {
                    anim(m, 4, 0, 0xa);
                    sb(m + 0x20, 8);
                }
                umb_30f308_l7(m, pv, 0xa, 5, 4, 4, 0xb, target, 17.0f, 20.0f, 1.0f, 1.0f, 1.0f);
                if ((lbu(m + 0x70) & 2) != 0) {
                    anim(m, 4, 0, 0xa);
                    wait_rand(pv, 200, 300);
                    sb(m + 0x20, 8);
                }
                tail = true;
                break;
            }
            case 11:  // L7:0x310abc: the bite's end
                if ((lbu(m + 0x70) & 2) != 0) sb(m + 0x20, 7);
                break;
            case 12:  // L7:0x310ad4: knocked back
                if ((enemy_physics(m, pv + 0x70) & 1) != 0) umb_30ec58_l7(m);
                break;
            case 13:  // L7:0x310af4: stunned
                if ((lbu(m + 0x70) & 2) != 0) umb_30ec58_l7(m);
                break;
            case 14:  // L7:0x310b0c: killed
                if ((enemy_physics(m, pv + 0x70) & lbu(m + 0x70) & 2) != 0) {
                    explosion_effect(m, pv + 0xd0, 0, 3, 3, 5, -1, 0, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
                    ee_stack_free(0xc0);
                    moby_retire(m);
                    return;
                }
                break;
            case 15:  // L7:0x310c30
                damp(pv, SLOW);
                fall_move(m, pv, false);
                tail = true;
                break;
            case 16:  // L7:0x310b9c
                damp(pv, SLOW);
                fall_move(m, pv, false);
                if ((lbu(m + 0x70) & 2) != 0) umb_30ec58_l7(m);
                break;
            case 17:  // L7:0x310cd4
                damp(pv, SLOW);
                if ((lbu(m + 0x70) & 2) != 0) umb_30ec58_l7(m);
                break;
            default:  // 3 (L7:0x310d1c) and past the table
                break;
        }
        if (tail && next_state != 0xff) {
            if (lbu(m + 0x53) != next_anim) moby_anim_set(m, next_anim, 0, 6);
            sb(m + 0x20, next_state);
        }
        ee_stack_free(0xc0);
    }
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(pos + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            return;
        }
    }
}

}  // namespace rac1
