// Gemlik Base's (level 13) wall turrets: class 29, the turret's base, which turns toward its target and fires class 1238's
// shots at it, and class 36, its top (the part the hero can destroy: a shield turns away hits from the front). Level 13
// only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Class 29's pvars: +0x40 its target (the hero's moby when nothing else), +0x50 a box the hero's +0xd0 must be in for it
// to wake (-1 none), +0x54 its rest yaw, +0x5c its top, +0x60 its yaw's speed, +0x64 the countdown of its break, +0x68
// its shots' speed, +0x6c how far (in degrees) it turns from its rest yaw, +0x70 its top's joint scale record. States
// (the table L13:0x1f4270): 0 set up (spawns its top), 1 idle, 2 nothing, 3 aim and fire, 4 break, 5 its top broken
// (counts down to 4). Its +0xbc, 4 or more, is a state to take (set by a trigger).
//
// Class 36's pvars: +0 its hit record (+0x20: +0 its health, +0x10 a value fix 38 changes: its use not traced), +0xc
// its colour pulse (+0x60), +0x70 its base. States: 0 set up, 1 alive (its animation follows the base's), 2 broken
// (explodes at its animation's end).
#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// `top` is a live moby of class 0x24 (the turret's top): not gone, not retiring
bool top_alive(uint32_t top) {
    if (top == 0 || lh(top + 0xa6) != 0x24) return false;
    const uint32_t st = lbu(top + 0x20);
    return st != 0xfe && st != 0xfd;
}

// the yaw from the moby to its target
float yaw_to(uint32_t m, uint32_t t) {
    return atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14)));
}

// the base turned toward `yaw` at its turning rate (4 pi a second, `vmax` its top speed)
void base_turn(uint32_t m, uint32_t pv, float yaw, float vmax) {
    const float acc = fpu_mul(dt_sq(), u2f(0x41490fdbu));
    yaw_seek(m + 0x48, pv + 0x60, yaw, acc, acc, vmax);
}

// the base's idle animation (set 0, from frame 0) at a random speed, when another set plays
void base_idle_anim(uint32_t m) {
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, rand_range(7, 0xd));
}
}  // namespace

// Class 1238's shot spawned at `pos` (an EE point) flying along `vel` (an EE vector: its length a frame's move) for
// `range`, fired by `owner`: its pitch and yaw from `vel`, its frames left (+0x2c) range / speed + 1. The shot, or 0.
// orig: L13:0x003061c8
uint32_t gml_3061c8_l13(uint32_t pos, uint32_t vel, uint32_t owner, float range) {
    const uint32_t s = moby_spawn(0x4d6);
    if (s == 0) return 0;
    sw(s + 0x40, 0);
    sh(s + 0x32, 0x7f);
    sb(s + 0x30, 0xff);
    sb(s + 0x31, 1);
    const uint32_t pv = lwu(s + 0x78);
    swc1(s + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    swc1(s + 0x48, atan2_f(lwc1(vel + 0), lwc1(vel + 4)));
    lq_sq(s + 0x10, pos);
    lq_sq(pv + 0x10, pos);
    lq_sq(pv + 0x00, vel);
    moby_anim_set(s, 1, 0, frame_scale_i(5));
    sh(s + 0x34, 0x208);
    sw(pv + 0x24, 1);
    sw(pv + 0x20, owner);
    swc1(pv + 0x28, range);
    sb(s + 0x23, 0x10);
    sw(pv + 0x2c, static_cast<uint32_t>(float_to_int(fpu_add(fpu_div(range, vec3_length(V(pv))), 1.0f))));
    moby_update_transform(s);
    return s;
}

// The base fires, once every L13:0x16138c frames: a shot from its muzzle (chain point 0) along its yaw spread at random
// (2.5 degrees either way at 20 from the target, 7.5 next to it), at L13:0x161388 a second, for its range +0x68.
// orig: L13:0x002b47a0
void gml_2b47a0_l13(uint32_t m) {
    const Frame f(112);
    const uint32_t sp = f.sp, muzzle = sp, vel = sp + 0x10;
    if (ee_rem(lw(L13_ADDR(0x0015f5cc)), lw(L13_ADDR(0x0016138c))) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    moby_chain_point_v(m, 0, V(muzzle));
    const float d = vec2_distance(V(m + 0x10), V(lwu(pv + 0x40) + 0x10));
    const float spread = fpu_add(fpu_mul(fpu_div(fpu_sub(20.0f, d), 20.0f), 5.0f), 2.5f);
    const float yaw = angle_add(fpu_mul(rand_range_f(-spread, spread), u2f(0x3c8efa35u)), lwc1(m + 0x48));
    swc1(vel + 0, fpu_mul(cos_vu(yaw), fpu_mul(lwc1(L13_ADDR(0x00161388)), dt())));
    swc1(vel + 4, fpu_mul(sin_vu(yaw), fpu_mul(lwc1(L13_ADDR(0x00161388)), dt())));
    sw(vel + 8, 0);
    vec3_add(V(muzzle), V(muzzle), V(vel));
    gml_3061c8_l13(muzzle, vel, m, lwc1(pv + 0x68));
}

// The base's target (target_select_any within 20, 22 while it fires; the hero's moby when none), and the state its +0xbc
// names when 4 or more. (The original returns whether +0xbc is under 4, which its caller never reads.)
// orig: L13:0x002b48d8
void gml_2b48d8_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    target_select_any(m, pv, lbu(m + 0x20) == 3 ? 22.0f : 20.0f);
    if (lwu(pv + 0x40) == 0) sw(pv + 0x40, lwu(0x001413d0));
    const uint32_t bc = lbu(m + 0xbc);
    if (bc >= 4) sb(m + 0x20, bc);
}

// The base `m`'s top: class 0x24 spawned on it, drawn with it, its hit record (health 1, +0x10 0.9; 0.7 with fix 38),
// its colour pulse record, its scale the base's relative to their classes'. The top, or 0.
// orig: L13:0x002b4958
uint32_t gml_2b4958_l13(uint32_t m) {
    const uint32_t t = moby_spawn(0x24);
    if (t == 0) return 0;
    const uint32_t pv = lwu(t + 0x78);
    sw(pv + 0x70, m);
    sw(pv + 0x00, pv + 0x20);
    sw(pv + 0x0c, pv + 0x60);
    sb(t + 0x30, lbu(m + 0x30));
    const uint32_t fl = lhu(t + 0x34) | 0x20u;
    sb(t + 0x31, 1);
    sh(t + 0x32, lhu(lwu(pv + 0x70) + 0x32));
    sh(t + 0x34, fl);
    sb(t + 0x20, 0);
    sb(t + 0xbc, 0);
    lq_sq(t + 0x10, lwu(pv + 0x70) + 0x10);
    swc1(t + 0x2c, fpu_div(fpu_mul(lwc1(lwu(t + 0x24) + 0x24), lwc1(m + 0x2c)), lwc1(lwu(m + 0x24) + 0x24)));
    swc1(pv + 0x30, fixes102_gemlik_turret_aim() ? u2f(0x3f333333u) : u2f(0x3f666666u));  // (1.02 Fixes, fix 38: 0.7)
    swc1(pv + 0x20, 1.0f);
    sb(pv + 0x29, 1);
    sb(pv + 0x2a, 3);
    sh(pv + 0x24, 1);
    sb(pv + 0x28, 0);
    moby_update_transform(t);
    moby_copy_hero_color(t);
    return t;
}

// orig: L13:0x002b41b8
void gml_class29_update_l13(uint32_t m) {
    const Frame f(128);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    gml_2b48d8_l13(m);
    const uint32_t tgt = lwu(pv + 0x40);
    {
        const uint32_t top = lwu(pv + 0x5c);
        if (top != 0 && lh(top + 0xa6) == 0x24 && static_cast<int8_t>(lbu(top + 0x20)) >= 0)
            joint_node_scale(top, 1, pv + 0x70, u2f(0x402ccccdu));
    }
    // the top broken (or gone): the base stops and breaks after a while
    auto top_check = [&]() -> bool {
        const uint32_t top = lwu(pv + 0x5c);
        if (top_alive(top) && lbu(top + 0x20) != 2) return false;
        base_idle_anim(m);
        swc1(pv + 0x64, 1.0f);
        sb(m + 0x20, 5);
        return true;
    };
    // (state 3) the target still in front (within 90 degrees of its rest yaw), within 22, the hero in its box
    auto engaged = [&] {
        if (u2f(0x3fc90fdbu) <= angle_dist(yaw_to(m, tgt), lwc1(pv + 0x54))) return false;
        if (22.0f < vec2_distance(V(m + 0x10), V(tgt + 0x10))) return false;
        const int32_t box = lw(pv + 0x50);
        return box == -1 || point_in_box(0x0013f420, box) != 0;
    };

    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4270)
    case 0: {  // (0x002b4240)
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 1.5f));
        sw(pv + 0x5c, gml_2b4958_l13(m));
        swc1(pv + 0x54, lwc1(m + 0x48));
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0xa);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x002b4290): idle; wakes when the target is in front, within 20 (the box)
        if (angle_dist(yaw_to(m, tgt), lwc1(pv + 0x54)) < u2f(0x3fc90fdbu) && vec2_distance(V(m + 0x10), V(tgt + 0x10)) < 20.0f) {
            const int32_t box = lw(pv + 0x50);
            if (box == -1 || point_in_box(0x0013f420, box) != 0) {
                swc1(pv + 0x68, 8.0f);
                if (lbu(m + 0x53) != 1) {
                    const int32_t frame = rand_range(0, 3);
                    moby_anim_set(m, 1, frame, rand_range(7, 0xd));
                }
                sb(m + 0x20, 3);
            }
        }
        base_turn(m, pv, lwc1(pv + 0x54), fpu_mul(dt(), u2f(0x41490fdbu)));
        top_check();
        break;
    }
    case 3: {  // (0x002b4414): aims (up to +0x6c degrees from its rest yaw) and fires
        if (!engaged()) {
            base_idle_anim(m);
            sb(m + 0x20, 1);
            break;
        }
        approach_f(pv + 0x68, 20.0f, fpu_mul(dt(), 6.0f));
        float turn = angle_sub(yaw_to(m, tgt), lwc1(pv + 0x54));
        const float lim = fpu_mul(lwc1(pv + 0x6c), u2f(0x3c8efa35u));
        if (lim < turn) turn = lim;
        else if (turn < -lim) turn = -lim;
        base_turn(m, pv, angle_add(turn, lwc1(pv + 0x54)), fpu_mul(dt(), u2f(0x41490fdbu)));
        gml_2b47a0_l13(m);
        top_check();
        break;
    }
    case 4: {  // (0x002b461c): breaks
        rand_sphere_point(sp + 0x20, 0.5f, 1.0f);
        moby_drop_bolts(m, 0, -1);
        explosion_effect(m, sp + 0x20, m + 0x10, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        return;
    }
    case 5: {  // (0x002b46b8): its top broken: counts down
        base_turn(m, pv, lwc1(pv + 0x54), fpu_mul(dt(), u2f(0x41c90fdbu)));
        const float left = fpu_sub(lwc1(pv + 0x64), 1.0f);
        swc1(pv + 0x64, left);
        if (left <= 0.0f) sb(m + 0x20, 4);
        break;
    }
    default:  // (2 and beyond: nothing)
        break;
    }

    // its top drawn with it: its angles and matrix
    const uint32_t top = lwu(pv + 0x5c);
    if (top_alive(top)) {
        lq_sq(top + 0x40, m + 0x40);
        lq_sq(top + 0xc0, m + 0xc0);
        lq_sq(top + 0xd0, m + 0xd0);
        lq_sq(top + 0xe0, m + 0xe0);
    } else {
        sw(pv + 0x5c, 0);
    }
}

// The top's hits: class 36's hit record +0x20; hits of kind 0x800000 from within 95 degrees of the base's rest yaw (or
// with no attacker) do no harm (its shield); a hit that takes its health breaks it and its base (state 4), drops its
// bolts and flashes. (The original returns 0xff, which its caller never reads.)
// orig: L13:0x002b4a58
void gml_2b4a58_l13(uint32_t m, uint32_t pv) {
    const uint32_t hs = pv + 0x20;
    if (lbu(m + 0x20) != 2) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        int32_t st = 0;
        const int32_t react = moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
        if (hit != 0 && (lwu(hit + 0x24) & 0x800000u) != 0) {
            const uint32_t att = lwu(hit + 0x20);
            if (att == 0) {
                sw(hit + 0x2c, 0);
                st = 1;
            } else if (angle_dist(yaw_to(m, att), lwc1(lwu(lwu(pv + 0x70) + 0x78) + 0x54)) < u2f(0x3fd43b67u)) {
                sw(hit + 0x2c, 0);
                st = 1;
            }
        }
        if (react == 1 || react == 2) sw(hs + 0, 0);  // (the table L13:0x1f4290: reactions 1 and 2 (0x002b4b68))
        if (st >= 2) {
            const float dmg = lwc1(hit + 0x2c), health = lwc1(hs + 0);
            if (health <= dmg) {
                sw(hs + 0, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                const uint32_t base = lwu(pv + 0x70);
                if (base != 0 && lh(base + 0xa6) == 0x1d) {
                    const uint32_t bst = lbu(base + 0x20);
                    if (bst != 0xfe && bst != 0xfd) sb(base + 0x20, 4);
                }
                moby_drop_bolts(m, 0, -1);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
                if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 1, 0);
                sb(m + 0x20, 2);
            } else {
                swc1(hs + 0, fpu_sub(health, dmg));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x60);
            }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// orig: L13:0x002b4c80
void gml_class36_update_l13(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(pv + 0x29, 1);
        sb(m + 0x20, 1);
        sb(pv + 0x58, 8);
        sb(pv + 0x5a, 6);
        return;
    case 1: {
        if (lbu(lwu(pv + 0x70) + 0x20) == 3) {  // the base fires: animation set 2
            if (lbu(m + 0x53) != 2) {
                const int32_t frame = rand_range(0, 3);
                moby_anim_set(m, 2, frame, rand_range(7, 0xc));
            }
        } else if (lbu(m + 0x53) >= 2 || (lbu(m + 0x70) & 2u) != 0) {  // idle: set 0, now and then 1, at a random speed
            const uint32_t set = lbu(m + 0x53);
            if (rand_mod(7) != 0) {
                if (set != 0) moby_anim_set(m, 0, 0, rand_range(7, 0xd));
            } else if (set != 1) {
                moby_anim_set(m, 1, 0, rand_range(7, 0xd));
            }
            swc1(m + 0x58, rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)));
        }
        gml_2b4a58_l13(m, pv);
        return;
    }
    case 2: {  // broken: explodes at its animation's end
        if ((lbu(m + 0x70) & 2u) == 0) return;
        vec3_sub(V(sp + 0x20), V(L13_ADDR(0x001670c0)), V(m + 0x10));
        moby_chain_point_v(m, 0, V(sp + 0x30));
        explosion_effect(m, sp + 0x20, sp + 0x30, 0x14, 9, 0x20, -1, 0, 1, -1, 0, 0.0f, 0.0f, 10.0f, 7.0f, 20.0f, 2.0f, 40.0f);
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
