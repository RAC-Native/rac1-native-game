// Veldin's finale (level 18) class 1454: the troopers with rocket launchers (three placed at the arena's south side).
// Set up (state 0) it takes its health (6) and its hit box, keeps the path it walks (+0x100: the level's path table) from
// its +0x124 wait, and spawns two class 331 mobys (its two arms' pieces, +0x138 / +0x13c) that follow it; it then walks
// (state 1) from point to point of its path. Every frame (vf_trooper_frame_l18) it takes the hits on it (dead: its blast,
// nine pieces of debris, its bolts, its pieces gone with it) and looks for a target within 30 (the hero, when within 10;
// lost past 30), turning its aim (yaw +0x110 / pitch +0x118, clamped) toward it, or back ahead when it has none. With a
// target it aims (state 2) and fires a rocket (class 41, vf_rocket_spawn_l18: homing when the hero is within 10) from the
// muzzle of its arm 1 or 2 at the animation's frames 4 and 10, back to walking when the target is lost. Level 18 only:
// the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in
// veldin_finale_mobys.cpp; the helpers' declarations in veldin_finale_own.h.
//
// The pvars: +0x20 health, +0x24 / +0x28 the hit's kind and box, +0x40 the blast's velocity, +0x60 the colour pulse, +0x70
// the target's record (target_select_any's 0x48 bytes: +0x70 its point, +0xb0 its moby, +0xb4 the result: 2 = none),
// +0xc0 the hit node, +0xd0 the aim's rotation (a quaternion), +0x100 its path, +0x110 / +0x114 the aim's yaw and its
// speed, +0x118 / +0x11c its pitch and its speed, +0x120 the path point it walks to, +0x124 its wait, +0x128 its turn's
// speed, +0x12c its walking speed, +0x130 the aim's reach, +0x134 the aim's sway, +0x138 / +0x13c its two pieces.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position)
constexpr float TWO_PI = 6.2831854820251465f;
constexpr float AIM_TURN = 12.566370964050293f;  // (4 pi: the aim's acceleration, x the frame time's square)
constexpr float AIM_TOP = 9.42477798461914f;     // (3 pi: its top speed, x the frame time)

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

// a piece of the trooper (class 331) spawned at its place, its turn `turn` (+0x34), as the hero's moby draws (+0x38)
uint32_t piece_spawn(uint32_t m, uint32_t turn) {
    const uint32_t o = moby_spawn(0x14b);
    sb(o + 0x30, 0x40);
    sh(o + 0x32, 0x40);
    sb(o + 0x31, 1);
    sd(o + 0x38, ld(lwu(0x001413d0) + 0x38));
    sh(o + 0x34, turn);
    lq_sq(o + 0x10, m + 0x10);
    sw(o + 0x94, 0);
    moby_anim_set(o, 0, 0, 0);
    moby_update_transform(o);
    return o;
}

// the trooper killed: its blast, nine pieces of debris, its bolts, it and its two pieces gone
void die(uint32_t m, uint32_t pv, uint32_t sp) {
    const uint32_t at = sp + 0x20;
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
    explosion_effect(m, pv + 0x40, at, 0x14, 8, 0x14, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
    const uint32_t zero = L18_ADDR(0x0015f580);  // (the zero vector)
    static const uint32_t debris[] = {0x700, 0x701, 0x702, 0x704, 0x6fe, 0x6fe, 0x6ff, 0x6ff, 0x6ff};
    for (const uint32_t cls : debris) debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
    moby_drop_bolts(m, 0, -1);
    moby_retire(m);
    if (lwu(pv + 0x138) != 0) moby_retire(lwu(pv + 0x138));
    if (lwu(pv + 0x13c) != 0) moby_retire(lwu(pv + 0x13c));
}

// the aim's yaw and pitch sought toward `yaw` / `pitch`
void aim_seek(uint32_t pv, float yaw, float pitch) {
    yaw_seek(pv + 0x110, pv + 0x114, yaw, fpu_mul(dt_sq(), AIM_TURN), fpu_mul(dt_sq(), AIM_TURN), fpu_mul(dt(), AIM_TOP));
    yaw_seek(pv + 0x118, pv + 0x11c, pitch, fpu_mul(dt_sq(), AIM_TURN), fpu_mul(dt_sq(), AIM_TURN), fpu_mul(dt(), AIM_TOP));
}
}  // namespace

// The troopers' rocket (class 41) at the EE point `at` with the velocity `vel` (an EE vector), fired by `owner` (+0x14),
// homing on the hero when `homing` (+0x12); three times its class's size, as `owner` draws (+0x38), for frame_scale_i(240)
// frames (+0x10), turned along its velocity (yaw +0x48, pitch +0x44). The rocket (0: none could be spawned).
// orig: L18:0x002a75f0
uint32_t vf_rocket_spawn_l18(uint32_t owner, uint32_t at, uint32_t vel, uint32_t homing) {
    const uint32_t r = moby_spawn(0x29);
    if (r == 0) return 0;
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0xff);
    swc1(r + 0x2c, fpu_mul(lwc1(r + 0x2c), 3.0f));
    sb(r + 0x31, 1);
    sb(r + 0x20, 0);
    sd(r + 0x38, ld(owner + 0x38));
    lq_sq(r + 0x10, at);
    const uint32_t pv = lwu(r + 0x78);
    lq_sq(pv, vel);
    sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x14, owner);
    sh(pv + 0x12, homing);
    swc1(r + 0x48, atan2_f(lwc1(vel + 0), lwc1(vel + 4)));
    swc1(r + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    return r;
}

// The trooper's every frame before its state: the hits on it (mask 0x330000: its health less the damage, its colour
// pulsed; at 0 or below it dies), then, alive, its target (within 30; the hero's moby when he is within 10; lost (+0xb4 =
// 2) when it is past 30) and its aim turned toward it (the reach +0x130 and the sway +0x134 bending it; the pitch clamped
// to -45 .. 20 degrees) or back ahead, the aim's rotation +0xd0 rebuilt, its colour pulse stepped.
// orig: L18:0x002f8050
void vf_trooper_frame_l18(uint32_t m) {
    const Frame f(208);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t st = sp + 0x30, dmg = sp + 0x34;  // (the hit reaction's words; both read stale from the frame, as the original)
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    {
        int32_t react_st = lw(st);
        float react_dmg = lwc1(dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, react_st, &react_dmg, nullptr, 4);
        sw(st, static_cast<uint32_t>(react_st));
        swc1(dmg, react_dmg);
    }
    if (lw(st) != 1 && lbu(m + 0x20) != 0) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(dmg));
        swc1(pv + 0x20, health);
        if (!(0.0f < health)) {
            die(m, pv, sp);
            return;
        }
        sb(pv + 0x67, 0x78);
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    const uint32_t target = pv + 0x70;
    if (target_select_any(m, target, 30.0f) != 2) {
        if (vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 10.0f) {
            sw(pv + 0xb0, lwu(0x001413d0));
            lq_sq(target, lwu(0x001413d0) + 0x10);
        } else if (lwu(pv + 0xb0) != 0 && 30.0f < vec2_distance(V(m + 0x10), V(lwu(pv + 0xb0) + 0x10))) {
            sw(pv + 0xb4, 2);
        }
    }
    const uint32_t q = sp + 0x20;
    if (lw(pv + 0xb4) != 2) {  // (0x002f8414) toward the target, 3 below its point
        vec3_sub(V(q), V(target), V(m + 0x10));
        swc1(q + 8, fpu_sub(lwc1(q + 8), 3.0f));
        const float reach = lwc1(pv + 0x130);
        if (reach < vec3_length(V(q))) vec2_set_length(V(q), V(q), reach);
        const float sway = fpu_mul(sin_vu(lwc1(pv + 0x134)), 0.0872664600610733f);
        const float yaw = angle_add(atan2_f(lwc1(q + 0), lwc1(q + 4)), sway);
        float pitch = -atan2_f(vec2_length(V(q)), lwc1(q + 8));
        if (0.3490658402442932f < pitch) pitch = 0.3490658402442932f;
        else if (pitch < -0.7853981852531433f) pitch = -0.7853981852531433f;
        aim_seek(pv, yaw, pitch);
    } else {  // (0x002f856c) none: ahead, level
        aim_seek(pv, lwc1(m + 0x48), 0.0f);
    }
    quat_from_axis_angle(V(q), 1, lwc1(pv + 0x118));
    quat_from_axis_angle(V(pv + 0xd0), 2, angle_sub(lwc1(pv + 0x110), lwc1(m + 0x48)));
    quat_mul(V(pv + 0xd0), V(q), V(pv + 0xd0));
    color_pulse_update(m, pv + 0x60);
}

// The trooper's walk along its path: turned toward its point (+0x120 of the path +0x100), its speed +0x12c sought to
// L18:0x162478 (slowed to 1 x the frame time when it faces more than 15 degrees away), moved along its facing; within 1
// of the point, another of the path's points at random (never the same); its two pieces put where it is, their
// animation's speed by its own (0 .. 2).
// orig: L18:0x002f8658
void vf_trooper_walk_l18(uint32_t m) {
    const Frame f(160);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t point = sp, step = sp + 0x10;
    lq_sq(point, lvd::path(lw(pv + 0x100)) + 0x10 + (lwu(pv + 0x120) << 4));
    const float yaw = atan2_f(fpu_sub(lwc1(point + 0), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14)));
    yaw_seek(m + 0x48, pv + 0x128, yaw, fpu_mul(dt_sq(), 3.1415927410125732f), fpu_mul(dt_sq(), 3.1415927410125732f),
             fpu_mul(dt(), TWO_PI));
    float speed = fpu_mul(lwc1(L18_ADDR(0x00162478)), dt());
    if (0.2617993950843811f < angle_dist(yaw, lwc1(m + 0x48))) speed = dt();
    approach_f(pv + 0x12c, speed, dt_sq());
    vec3_set_length(V(step), V(m + 0xc0), lwc1(pv + 0x12c));
    walker_move(m, step, 0, 0.5f, 0.5f, 0.0f);
    if (vec3_distance(V(m + 0x10), V(point)) < 1.0f) {
        const int32_t n = lw(lvd::path(lw(pv + 0x100)));
        const int32_t r = rand_mod(n - 1);
        sw(pv + 0x120, static_cast<uint32_t>(ee_rem(lw(pv + 0x120) + r + 1, lw(lvd::path(lw(pv + 0x100))))));
    }
    float anim = fpu_div(lwc1(pv + 0x12c), fpu_mul(lwc1(L18_ADDR(0x00162480)), dt()));
    if (2.0f < anim) anim = 2.0f;
    else if (anim < 0.0f) anim = 0.0f;
    for (const uint32_t slot : {pv + 0x138, pv + 0x13c}) {
        const uint32_t o = lwu(slot);
        if (o == 0) continue;
        swc1(o + 0x58, anim);
        lq_sq(o + 0x40, m + 0x40);
        lq_sq(o + 0x10, m + 0x10);
    }
}

// orig: L18:0x002f7c40
void vf_class1454_update_l18(uint32_t m) {
    const Frame f(112);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    vf_trooper_frame_l18(m);
    switch (lbu(m + 0x20)) {
    case 0: {  // set up (0x002f7ca0)
        sub_0024f590(m, 0, pv + 0xc0);
        sb(pv + 0x28, 3);
        sh(pv + 0x24, 6);
        swc1(pv + 0x20, 6.0f);
        sw(pv + 0x124, static_cast<uint32_t>(frame_scale_i(0xf0)));
        sw(pv + 0x120, 0);
        if (lwu(pv + 0x138) == 0) sw(pv + 0x138, piece_spawn(m, 0x4000));
        if (lwu(pv + 0x13c) == 0) sw(pv + 0x13c, piece_spawn(m, 0x8000));
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // walking (0x002f7dfc)
        vf_trooper_walk_l18(m);
        if (lw(pv + 0xb4) == 2) break;
        swc1(pv + 0x130, 10.0f);
        sw(pv + 0x134, 0);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x14));
        sb(m + 0x20, 2);
        break;
    case 2: {  // aiming, firing (0x002f7e48)
        vf_trooper_walk_l18(m);
        approach_f(pv + 0x130, 30.0f, fpu_mul(dt(), 6.0f));
        swc1(pv + 0x134, angle_add(lwc1(pv + 0x134), fpu_mul(dt(), TWO_PI)));
        if (anim_crossed(m, 4.0f) != 0 || anim_crossed(m, 10.0f) != 0) {
            const uint32_t vel = sp, muzzle = sp + 0x10;
            moby_chain_point_v(m, moby_anim_time(m) < 7.0f ? 1 : 2, V(muzzle));
            const float speed = fpu_mul(lwc1(L18_ADDR(0x0016247c)), dt());
            swc1(vel + 0, fpu_mul(cos_vu(lwc1(pv + 0x110)), fpu_mul(cos_vu(lwc1(pv + 0x118)), speed)));
            swc1(vel + 4, fpu_mul(sin_vu(lwc1(pv + 0x110)), fpu_mul(cos_vu(lwc1(pv + 0x118)), speed)));
            sw(vel + 8, 0);
            swc1(vel + 8, fpu_mul(-sin_vu(lwc1(pv + 0x118)), speed));
            moby_sound_start(0, 0, m);
            spark_burst(m, muzzle, 0);
            const bool far = 10.0f < vec2_distance(V(m + 0x10), V(HERO + 0x80));
            vf_rocket_spawn_l18(m, muzzle, vel, far ? 0 : 1);
        }
        if (lw(pv + 0xb4) != 2) break;
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        sb(m + 0x20, 1);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
