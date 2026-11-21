// Class 28, a path-riding turret: one port of the copies Oltanis and Quartu carry (the same code relinked; their only
// differences are the relocated words). Each level keeps its own words (the turret's tunings, the shells' and rockets'
// throws, the two draw callbacks' addresses: per level below); the rest is the engine's (OV: the path table 0x1b04b0, the
// groups 0x1ab840, the moby list 0x15ffd8, the mode word 0x15f5c4, the level's paths 0x15f70c, the zero vector 0x15f580,
// the camera 0x166dc0, the collision's hit point 0x173e60) or the base ELF's (bare: the hero 0x13f3d0 and its moby
// 0x1413d0, the hero's height 0x13f62c, the difficulty byte 0x1413f4, the view's up axis 0x13f5e0, the frame scales
// 0x15ed60 / 0x15ed6c / 0x15ed70). Header shared_class28.h.
//
// The pvars: +0x20 the health, +0x38 a request to deal the range timer anew, +0x60 the hit's move record, +0xc0 the colour
// pulse, +0xd0 the target's point, +0x110 the target, +0x114 the mode (2: out of range), +0x120 / +0x160 two joints'
// records, +0x130 / +0x170 the head's and the body's turns, +0x1e0 .. +0x220 the glows' points, +0x234 the range,
// +0x238 its timer, +0x23c the yaw's rate, +0x240 the path (an index of the path table; -1 none), +0x244 the zone's path,
// +0x248 the wake timer, +0x24a whether it rides its path, +0x24b whether it may tilt down, +0x24c / +0x250 the path's
// point and fraction, +0x254 / +0x258 the drift, +0x25c, +0x260 the fire pause, +0x262 the firing timer, +0x264 the moby
// it calls, +0x268 / +0x26c the body's turn and its rate, +0x270 / +0x274 the head's, +0x278 the yaw it fires at,
// +0x27c the path's direction, +0x280 / +0x284 the tilt and its rate, +0x288 hidden for the difficulty.
#include <game/ovl/shared_class28.h>

#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby
constexpr uint32_t UP_AXIS = 0x0013f5e0;    // the view's up axis
constexpr uint32_t CLASS_TURRET = 0x1c;     // class 28

// the level's tunings: +0 the third glow's reach, +8 .. +0x10 the muzzle's offsets, +0x1c the range, +0x20 the knock-back's
// speed, +0x30 the path's speed, +0x34 the drift's drag, +0x38 the group's pause, +0x3c / +0x4c the muzzle's reach (fire,
// collision), +0x40 the firing arc, +0x44 the firing time, +0x48 the wake time
uint32_t tunings() { return on_level(QUARTU) ? L15_ADDR(0x00161388) : L14_ADDR(0x001614a8); }
// the level's throws: +0 / +4 a shell's speed range, +8 / +0xc its spin's; +0x28 / +0x2c a rocket's, +0x30 / +0x34 its spin's
uint32_t throws() { return on_level(QUARTU) ? L15_ADDR(0x00161a70) : L14_ADDR(0x00161b08); }
// the level's copies of the two draw callbacks (shared_class28_draw.cpp; the levels still register their copies)
uint32_t draw_spot_callback() { return on_level(QUARTU) ? L15_ADDR(0x00297808) : L14_ADDR(0x002b3a20); }
uint32_t draw_glows_callback() { return on_level(QUARTU) ? L15_ADDR(0x00296710) : L14_ADDR(0x002b2928); }

// the path of index `idx` in the path table
uint32_t path_at(int32_t idx) { return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(idx) << 2)); }
// the moby of index `idx` in the moby list
uint32_t moby_at(uint32_t idx) { return lwu(OV(0x0015ffd8)) + (idx << 8); }

// the animation `set` over `frames` (60 Hz) frames
void anim(uint32_t m, uint32_t set, int32_t frames) { moby_anim_set(m, set, 0, frame_scale_i(frames)); }
// whether the animation has played out (+0x70 bit 1) with no blend pending
bool anim_done(uint32_t m) { return (lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == lbu(m + 0x53); }

// the angle's turn toward `target` (yaw_seek: the frame's acceleration 4 pi, its top speed 2 pi a second)
void seek(uint32_t angle, uint32_t rate, float target) {
    const float accel = fpu_mul(lwc1(0x0015ed70), u2f(0x41490fdbu));  // (4 pi)
    yaw_seek(angle, rate, target, accel, accel, fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu)));  // (2 pi)
}
// `a` within the turn's limits (70 degrees either way)
float limited(float a) {
    const float limit = u2f(0x3f9c61aau);  // (1.2217305)
    if (limit < a) return limit;
    if (a < -limit) return -limit;
    return a;
}
// the heading from the moby to the target `t`
float yaw_to(uint32_t m, uint32_t t) {
    return atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14)));
}
// the bolts it leaves (more on the difficulty 2)
void drop_bolts(uint32_t m) { moby_drop_bolts(m, lbu(0x001413f4) == 2 ? 0x200u : 0u, -1); }
// the path's state reset (its direction forward, no drift; with `turns` the body's and head's turns too: the words are
// zeroed, the original copying the zeroed word's float into the drift's)
void path_reset(uint32_t pv, bool turns) {
    if (turns) {
        for (uint32_t k : {0x268u, 0x26cu, 0x270u, 0x274u}) sw(pv + k, 0);
    }
    swc1(pv + 0x27c, 1.0f);
    for (uint32_t k : {0x24cu, 0x250u, 0x254u, 0x258u, 0x25cu}) sw(pv + k, 0);
}
// a thrown moby's flight from `from`: away from the hero (flat), its speed one of `speeds` (scaled by the frame), turned up
// about the view's up axis by `tilt_lo` .. `tilt_hi` (either way) into `vel`; its spin's speed one of `spins` into `spin`,
// and three spin rates from +0 (signed ranges: 0 .. 0.5, 3 .. 8, 0 .. 1 degrees)
void thrown(uint32_t from, uint32_t sp, uint32_t speeds, uint32_t vel, float tilt_lo, float tilt_hi, uint32_t spins,
            uint32_t spin, uint32_t rates) {
    const uint32_t away = sp, side = sp + 0x10;
    vec3_sub(V(away), V(from), V(HERO_POS));
    {
        const float scale = lwc1(0x0015ed60);
        const float lo = fpu_mul(lwc1(speeds), scale), hi = fpu_mul(lwc1(speeds + 4), scale);
        swc1(away + 8, 0.0f);
        vec3_set_length(V(away), V(away), rand_range_f(lo, hi));
    }
    vec3_cross_ba(V(side), V(UP_AXIS), V(away));
    vec_rotate_axis(V(vel), V(away), V(UP_AXIS), rand_range_signed_f(tilt_lo, tilt_hi));
    {
        const float scale = lwc1(0x0015ed60);
        swc1(spin, rand_range_f(fpu_mul(lwc1(spins), scale), fpu_mul(lwc1(spins + 4), scale)));
    }
    swc1(rates, rand_range_signed_f(0.0f, u2f(0x3c0efa35u)));                 // (0.5 degrees)
    swc1(rates + 4, rand_range_signed_f(u2f(0x3d567750u), u2f(0x3e0efa35u)));  // (3 .. 8 degrees)
    swc1(rates + 8, rand_range_signed_f(0.0f, u2f(0x3c8efa35u)));             // (1 degree)
}
// a spawned moby at the spawner's place and heading, seen and kept updated
void spawned_at(uint32_t r, uint32_t m, uint32_t state) {
    sb(r + 0x20, state);
    sb(r + 0x30, 0xff);
    swc1(r + 0x48, lwc1(m + 0x48));
    sh(r + 0x32, 0xff);
    sb(r + 0x31, 1);
}
}  // namespace

// orig: L14:0x002b3318 L15:0x00297100
void class28_hide(uint32_t m) {
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
}

// orig: L14:0x002b3578 L15:0x00297360
void class28_drift(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float drag = fpu_add(fpu_mul(fpu_sub(lwc1(tunings() + 0x34), 1.0f), lwc1(0x0015ed60)), 1.0f);
    const float vy = fpu_mul(lwc1(pv + 0x258), drag), vx = fpu_mul(lwc1(pv + 0x254), drag);
    swc1(pv + 0x258, vy);
    swc1(pv + 0x254, vx);
    swc1(m + 0x10, fpu_add(lwc1(m + 0x10), vx));
    swc1(m + 0x14, fpu_add(lwc1(m + 0x14), lwc1(pv + 0x258)));
}

// orig: L14:0x002d9870 L15:0x002c7098
int32_t class28_call(uint32_t idx) {
    const uint32_t other = moby_at(idx);
    const uint32_t opv = lwu(other + 0x78);
    int32_t firing = 0;
    const uint32_t state = lbu(other + 0x20);
    if (state == 0) {
        sb(other + 0x20, 5);
        moby_set_anim(other, 1, 0);
    } else {
        firing = state == 6 ? 1 : 0;
    }
    sw(opv + 8, static_cast<uint32_t>(frame_scale_i(0x3c)));
    return firing;
}

// orig: L14:0x002b2ed8 L15:0x00296cc0
void class28_range(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x238, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x238)));
    float range = lwc1(tunings() + 0x1c);
    if (lw(pv + 0x238) != 0) range = fpu_add(range, 6.0f);
    swc1(pv + 0x234, range);
}

// The group's list (halfwords: a moby's index, the last with bit 15) walked: each class-28 moby given to `each`, which
// returns false to stop.
namespace {
template <typename F>
void each_turret(uint32_t list, F each) {
    for (uint32_t e = list;; e += 2) {
        const uint32_t other = moby_at(lhu(e) & 0x7fff);
        if (lh(other + 0xa6) == static_cast<int32_t>(CLASS_TURRET) && !each(other)) return;
        if (lh(e) < 0) return;
    }
}
uint32_t group_list(uint32_t group) { return lwu(OV(0x001ab840) + (group << 2)); }
}  // namespace

// orig: L14:0x002b3920 L15:0x00297708
int32_t class28_group_firing(uint32_t group) {
    if (group == 0xffffffffu) return 0;
    const uint32_t list = group_list(group);
    if (list == 0) return 0;
    int32_t n = 0;
    each_turret(list, [&n](uint32_t other) {
        if (((lbu(other + 0x20) - 5) & 0xff) <= 1) ++n;
        return true;
    });
    return n;
}

// orig: L14:0x002b39a0 L15:0x00297788
int32_t class28_group_settled(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return 1;
    const uint32_t list = group_list(group);
    if (list == 0) return 1;
    bool waking = false;
    each_turret(list, [&waking](uint32_t other) {
        waking = lbu(other + 0x20) == 2;
        return !waking;
    });
    return waking ? 0 : 1;
}

// orig: L14:0x002b3890 L15:0x00297678
void class28_group_pause(uint32_t group) {
    if (group == 0xffffffffu) return;
    const uint32_t list = group_list(group);
    if (list == 0) return;
    const uint32_t t = tunings();
    each_turret(list, [t](uint32_t other) {
        sh(lwu(other + 0x78) + 0x260, static_cast<uint32_t>(frame_scale_i(lw(t + 0x38))));
        return true;
    });
}

// orig: L14:0x002b3268 L15:0x00297050
void class28_pick_target(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t zone = path_at(lw(pv + 0x244));
    const uint32_t point = pv + 0xd0;
    if (target_select(m, point, 0, 0, zone + 0x10, lw(zone), lwc1(pv + 0x234)) != 2 &&
        lwc1(pv + 0x234) < vec2_distance(V(m + 0x10), V(point)))
        sw(pv + 0x114, 2);
    if (lwu(pv + 0x110) == 0) sw(pv + 0x110, lwu(HERO_MOBY));
}

// orig: L14:0x002b3358 L15:0x00297140
void class28_path_lengths(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x250, 0);
    const float zero = lwc1(pv + 0x250);
    sw(pv + 0x24c, 0);
    swc1(pv + 0x254, zero);
    swc1(pv + 0x25c, zero);
    swc1(pv + 0x258, zero);
    const int32_t idx = lw(pv + 0x240);
    if (idx == -1) return;
    const uint32_t path = path_at(idx);
    if (lw(path) <= 0) return;
    for (int32_t i = 0; i < lw(path); ++i) {
        const int32_t next = ee_rem(i + 1, lw(path));
        const uint32_t at = path + 0x10 + (static_cast<uint32_t>(i) << 4);
        swc1(at + 0xc, vec3_distance(V(at), V(path + 0x10 + (static_cast<uint32_t>(next) << 4))));
    }
}

// orig: L14:0x002b3418 L15:0x00297200
int32_t class28_path_step(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t to = sp, was = sp + 0x10, way = sp + 0x20;
    const float step = fpu_mul(fpu_mul(lwc1(tunings() + 0x30), lwc1(0x0015ed6c)), lwc1(pv + 0x27c));
    int32_t point = lw(pv + 0x24c);
    float frac = lwc1(pv + 0x250);
    const int32_t more = path_advance_v(path_at(lw(pv + 0x240)), V(to), &point, &frac, 0, step);
    sw(pv + 0x24c, static_cast<uint32_t>(point));
    swc1(pv + 0x250, frac);
    lq_sq(was, m + 0x10);
    spring_to(m + 0x10, pv + 0x254, lwc1(to), u2f(0x3c23d70au), u2f(0x3e4ccccdu), 0.0f);  // (0.01, 0.2)
    spring_to(m + 0x14, pv + 0x258, lwc1(to + 4), u2f(0x3c23d70au), u2f(0x3e4ccccdu), 0.0f);
    vec3_sub(V(way), V(to), V(was));
    seek(m + 0x48, pv + 0x23c, atan2_f(lwc1(way), lwc1(way + 4)));
    ee_stack_free(0xa0);
    return 0 < more ? 1 : 0;
}

// orig: L14:0x002b37b0 L15:0x00297598
void class28_head_turn(uint32_t m, float yaw) {
    const uint32_t pv = lwu(m + 0x78);
    const float body = lwc1(pv + 0x268);
    const float turn = limited(angle_add(-body, -angle_sub(yaw, lwc1(m + 0x48))));
    seek(pv + 0x270, pv + 0x274, turn);
    quat_from_axis_angle(V(pv + 0x130), 2, lwc1(pv + 0x270));
}

// orig: L14:0x002b35d8 L15:0x002973c0
float class28_aim(uint32_t m, int32_t pitch_too, float yaw) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t target = lwu(pv + 0x110);
    const float turn = limited(angle_sub(yaw, lwc1(m + 0x48)));
    seek(pv + 0x268, pv + 0x26c, turn);
    quat_from_axis_angle(V(pv + 0x170), 2, lwc1(pv + 0x268));
    if (pitch_too != 0) {
        const uint32_t sp = ee_stack_alloc(0xb0);
        const uint32_t at = sp, rel = sp + 0x10, tilt = sp + 0x20;
        lq_sq(at, target + 0x10);
        if (lwu(HERO_MOBY) == target) swc1(at + 8, fpu_sub(lwc1(at + 8), lwc1(0x0013f62c)));
        vec3_sub(V(rel), V(at), V(m + 0x10));
        swc1(rel + 8, fpu_add(lwc1(rel + 8), 1.5f));
        const float pitch = atan2_f(vec2_length(V(rel)), lwc1(rel + 8));
        seek(pv + 0x280, pv + 0x284, -pitch);
        quat_from_axis_angle(V(tilt), 1, lwc1(pv + 0x280));
        quat_mul(V(pv + 0x170), V(pv + 0x170), V(tilt));
        ee_stack_free(0xb0);
    }
    return angle_dist(turn, lwc1(pv + 0x268));
}

// orig: L14:0x002dea98 L15:0x002ca668
uint32_t class28_spawn_shell(uint32_t m, uint32_t pos) {
    const uint32_t r = moby_spawn(0x145);
    if (r == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t rpv = lwu(r + 0x78);
    sw(rpv + 0x24, m);
    lq_sq(r + 0x10, pos);
    spawned_at(r, m, 0);
    const uint32_t w = throws();
    thrown(r + 0x10, sp, w, rpv, u2f(0x3eb2b8c2u), u2f(0x3f860a92u), w + 8, rpv + 8, rpv + 0x10);  // (20 .. 60 degrees)
    sw(rpv + 0x20, static_cast<uint32_t>(frame_scale_i(0xc8)));
    moby_update_transform(r);
    ee_stack_free(0x80);
    return r;
}

// orig: L14:0x002e0170 L15:0x002cae58
uint32_t class28_spawn_rocket(uint32_t m, uint32_t speed, uint32_t state) {
    const uint32_t r = moby_spawn(0x193);
    if (r == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t rpv = lwu(r + 0x78);
    lq_sq(r + 0x10, m + 0x10);
    spawned_at(r, m, state);
    sw(rpv, m);
    if (lbu(r + 0x20) == 0) {
        // along the level's path of the turret's zone, from its point nearest the rocket
        sw(rpv + 4, static_cast<uint32_t>(frame_scale_i(0x5a)));
        swc1(rpv + 8, fpu_mul(lwc1(speed), lwc1(0x0015ed6c)));
        const uint32_t zone = lwu(lwu(m + 0x78) + 0x60);
        sw(rpv + 0x14, 0);
        sw(rpv + 0xc, zone);
        sw(rpv + 0x10, 0);
        const uint32_t path = lwu(lwu(OV(0x0015f70c)) + (zone << 5) + 0x10);
        int32_t idx = lw(rpv + 0x10);
        float dist = lwc1(rpv + 0x14);
        path_closest_point_v(path, V(r + 0x10), V(sp), &idx, &dist, 0, 20.0f, 5.0f, lwc1(rpv + 0x14));
        sw(rpv + 0x10, static_cast<uint32_t>(idx));
        swc1(rpv + 0x14, dist);
    } else {
        sw(rpv + 4, static_cast<uint32_t>(frame_scale_i(0xc8)));
        const uint32_t w = throws();
        thrown(r + 0x10, sp, w + 0x28, rpv + 0x30, u2f(0x3f137207u), u2f(0x3fb2b8c2u), w + 0x30, rpv + 0x38,
               rpv + 0x20);  // (33 .. 80 degrees)
    }
    moby_update_transform(r);
    ee_stack_free(0xa0);
    return r;
}

// orig: L14:0x002b2600 L15:0x002963e8
void class28_visuals(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    draw_hook_add_de80_l14(draw_spot_callback(), m);
    const uint32_t sp = ee_stack_alloc(0x170);
    const uint32_t muzzle = sp, back = sp + 0x10, dir = sp + 0x20, frame = sp + 0x30, reach = sp + 0x70, side = sp + 0x80,
                   up = sp + 0x90;
    const uint32_t t = tunings();
    // the first glow half a unit from the muzzle toward the camera
    moby_chain_point_v(m, 1, V(muzzle));
    vec3_sub(V(back), V(muzzle), V(OV(0x00166dc0)));
    vec3_set_length(V(back), V(back), -0.5f);
    vec3_add(V(pv + 0x1e0), V(muzzle), V(back));
    // the sparks: one drifting, three still (their life 2 frames, grey, the last sometimes long)
    rand_dir_scaled(dir, u2f(0x3ba3d70au), u2f(0x3cf5c28fu));  // (0.005 .. 0.03)
    {
        const uint32_t p = spawn_particle_45(muzzle, dir, 0x7f, m);
        if (p != 0) swc1(p + 0xc, rand_range_f(6000.0f, 32000.0f));
    }
    for (int32_t i = 0; i < 3; ++i) {
        const uint32_t p = spawn_particle_45(muzzle, OV(0x0015f580), 0x7f, m);
        if (p == 0) continue;
        if (i == 2 && rand_mod(8) == 0) swc1(p + 0xc, 180000.0f);
        else swc1(p + 0xc, rand_range_f(80000.0f, 120000.0f));
        const int32_t life = frame_scale_i(2);
        sh(p + 0xa, static_cast<uint32_t>(life));
        const float fade = fpu_div(1.0f, int_to_float(static_cast<int16_t>(life)));
        sh(p + 0x36, 3);
        sw(p + 0x38, 0x7f7f7f);
        swc1(p + 0x30, fade);
    }
    // the glows' points: chain point 6; the joint 7's place pushed along its x axis; the joint 2's place pushed along its
    // three axes (and back along the second twice)
    moby_chain_point_v(m, 6, V(pv + 0x200));
    moby_chain_world_one(m, 7, frame);
    vec3_scale(V(reach), V(frame), lwc1(t));
    vec3_add(V(pv + 0x1f0), V(frame + 0x30), V(reach));
    moby_chain_world_one(m, 2, frame);
    vec3_scale(V(reach), V(frame), lwc1(t + 8));
    vec3_scale(V(side), V(frame + 0x10), lwc1(t + 0xc));
    vec3_scale(V(up), V(frame + 0x20), lwc1(t + 0x10));
    vec3_add(V(pv + 0x210), V(frame + 0x30), V(reach));
    vec3_add(V(pv + 0x210), V(pv + 0x210), V(side));
    vec3_add(V(pv + 0x210), V(pv + 0x210), V(up));
    vec3_scale(V(side), V(side), -2.0f);
    vec3_add(V(pv + 0x220), V(pv + 0x210), V(side));
    draw_callback_add(draw_glows_callback(), m);
    ee_stack_free(0x170);
}

// orig: L14:0x002b2f60 L15:0x00296d48
void class28_hits(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) return;
    const uint32_t pulse = pv + 0xc0;
    if (state != 0xb) {
        const uint32_t sp = ee_stack_alloc(0x70);
        const uint32_t result = sp, damage = sp + 4;
        swc1(damage, 0.0f);
        // a hit (none from another turret or from class 880)
        uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        if (hit != 0 && lwu(hit + 0x20) != 0) {
            const int32_t from = lh(lwu(hit + 0x20) + 0xa6);
            if (from == lh(m + 0xa6) || from == 0x370) hit = 0;
        }
        int32_t reaction;
        {
            int32_t st = lw(result);
            float d = lwc1(damage);
            reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &d, nullptr, 4);
            sw(result, static_cast<uint32_t>(st));
            swc1(damage, d);
        }
        const bool start = lw(result) != 1 && lbu(m + 0x20) != 0xb;
        if (start) {
            const float health = fpu_sub(lwc1(pv + 0x20), lwc1(damage));
            swc1(pv + 0x20, health);
            if (health <= 0.0f) reaction = 1;
            const uint32_t move = pv + 0x60;
            switch (static_cast<uint32_t>(reaction)) {  // (the level's jump table of twelve reactions)
            case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: {  // (L14:0x002b3094) knocked back
                fx1471_release(m);
                sb(m + 0x20, 0xa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                const float dt = lwc1(0x0015ed6c);
                const float back = fpu_mul(lwc1(tunings() + 0x20), dt), up = fpu_mul(dt, 8.0f);
                swc1(pv + 0xb0, 11.0f);
                swc1(pv + 0x78, up);
                swc1(pv + 0xb4, 29.0f);
                swc1(pv + 0x7c, back);
                move_state_start(m, move, 7, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
                sb(pv + 0xc7, 0xfa);
                break;
            }
            case 1: case 2: {  // (L14:0x002b3124) dying: blown off its stand (or down, kinds 0x800000)
                sb(m + 0x20, (lwu(hit + 0x24) & 0x800000u) != 0 ? 0xc : 0xb);
                sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                const int32_t spin = float_to_int(fpu_mul(lwc1(pv + 0x88), 1024.0f));
                const float speed = fpu_mul(lwc1(0x0015ed6c), 10.0f);
                swc1(pv + 0x88, u2f(0x3e99999au));  // (0.3)
                swc1(pv + 0x7c, speed);
                swc1(pv + 0xb0, 9.0f);
                swc1(pv + 0x78, speed);
                sw(pv + 0x80, static_cast<uint32_t>(spin));
                swc1(pv + 0xb4, 15.0f);
                const float away = atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14));
                move_state_start(m, move, 8, frame_scale_i(5), 0, away);
                sb(pv + 0xc7, 0xfa);
                drop_bolts(m);
                sw(m + 0x94, 0);
                break;
            }
            default:  // (L14:0x002b3220: 0, 11 and past the table)
                break;
            }
            color_pulse_start(m, pulse);
        }
        ee_stack_free(0x70);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
}

// The update: hidden on the difficulty that leaves it out; else the hits, the target, the range, the mode; then the state:
// 0 set up on its path, 1 asleep until called, 2 waking, 3 / 8 / 9 riding its path, 4 aiming at the target, 5 / 6 firing,
// 7 back to aiming, 10 knocked back, 11 falling dead, 12 blown up, 13 waiting on its path. Then its body and head turn
// toward its heading (but while aiming or firing or asleep) and its glows and sparks.
// orig: L14:0x002b17d8 L15:0x002955c0
void class28_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    joint_node_scale(m, 2, pv + 0x120, 2.5f);
    if (lwu(pv + 0x288) != 0) {
        if (lbu(0x001413f4) != 2) {
            const uint32_t flags = lhu(m + 0x34) | 0x41;
            sw(m + 0x94, 0);
            sh(m + 0x34, flags);
            if (lbu(m + 0x53) == 5) {
                fx1471_release(m);
                moby_drop_bolts(m, 0, -1);
                moby_retire(m);
                return;
            }
            sh(m + 0x34, flags & 0xefff);
            sb(m + 0x30, 0xff);
            return;
        }
        sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
        if (lbu(m + 0x20) != 0xb) sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sb(m + 0x30, 0x40);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000);
    }
    uint32_t target = 0;
    if (lbu(m + 0x20) != 0) {
        class28_hits(m);
        class28_pick_target(m);
        class28_range(m);
        target = lwu(pv + 0x110);
    }
    if (lw(OV(0x0015f5c4)) == 2) {
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1);
    } else if (lbu(m + 0x20) != 1) {
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffe);
    }
    const uint32_t t = tunings();
    enum class Exit { Turn, Done, Retire };
    auto run = [&]() -> Exit {
        switch (lbu(m + 0x20)) {  // (the level's jump table of fourteen states)
        case 0: {  // (L14:0x002b1954) set up: checked, on its path, its joints and pvars
            const uint32_t rides = lbu(pv + 0x24a);
            if (rides != 0) {
                if (lw(pv + 0x240) == -1 || lw(path_at(lw(pv + 0x240))) == 0) {
                    debug_printf_stub();  // (the original's message: no path, or an empty one)
                    return Exit::Retire;
                }
            }
            if (lw(pv + 0x244) == -1 || lw(path_at(lw(pv + 0x244))) == 0) {
                debug_printf_stub();  // (no zone, or an empty one)
                return Exit::Retire;
            }
            if (rides != 0 && lw(pv + 0x264) < 0) {
                debug_printf_stub();  // (no moby to call)
                return Exit::Retire;
            }
            if (rand_mod(2) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
            class28_path_lengths(m);
            if (lw(pv + 0x240) != -1) lq_sq(m + 0x10, path_at(lw(pv + 0x240)) + 0x10);
            sb(pv + 0x28, 1);
            sb(pv + 0x29, 0);
            swc1(pv + 0x20, 2.0f);
            swc1(pv + 0x30, 1.25f);
            sh(pv + 0x24, 2);
            sb(pv + 0x58, static_cast<uint32_t>(float_to_int(15.0f)));
            sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(16.0f)));
            swc1(pv + 0x27c, 1.0f);
            sh(pv + 0x248, 0);
            sw(pv + 0x23c, 0);
            sh(pv + 0x260, 0);
            for (uint32_t k : {0x268u, 0x26cu, 0x280u, 0x284u, 0x270u, 0x274u}) sw(pv + k, 0);
            if (lbu(pv + 0x121) == 0) sub_0024f590(m, 2, pv + 0x120);
            if (lbu(pv + 0x161) == 0) sub_0024f590(m, 3, pv + 0x160);
            if (lbu(pv + 0x24a) != 0) {
                class28_hide(m);
                sb(m + 0x20, 1);
            } else {
                swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
                sb(m + 0x20, lw(pv + 0x240) == -1 ? 4 : 0xd);
            }
            return Exit::Done;
        }
        case 1: {  // (L14:0x002b1b90) asleep: called (by its moby, while in range, its group settled) it wakes
            if (lw(pv + 0x114) == 2) return Exit::Done;
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x248))) == 0) return Exit::Done;
            if (class28_call(lwu(pv + 0x264)) == 0) return Exit::Done;
            if (class28_group_settled(m) == 0) return Exit::Done;
            sh(pv + 0x248, static_cast<uint32_t>(frame_scale_i(lw(t + 0x48))));
            moby_show_default(m);
            sb(m + 0x20, 2);
            moby_set_anim(m, 1, 0);
            const uint32_t path = path_at(lw(pv + 0x240));
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)),
                                   fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
            path_reset(pv, true);
            return Exit::Done;
        }
        case 2:  // (L14:0x002b1c68) waking: on its path from the third frame of the animation
            if (3.0f <= moby_anim_time(m)) class28_path_step(m);
            if (anim_done(m)) {
                sb(m + 0x20, 3);
                anim(m, 3, 0xa);
                swc1(m + 0x58, 1.0f);
            }
            return Exit::Turn;
        case 3:  // (L14:0x002b1e84) riding its path to the end
            if (class28_path_step(m) != 0) {
                anim(m, 0, 0xa);
                sb(m + 0x20, 4);
            }
            return Exit::Turn;
        case 4: {  // (L14:0x002b1f20) aiming: out of range back along its path; else fire when its group lets it
            class28_drift(m);
            const float yaw = yaw_to(m, target);
            seek(m + 0x48, pv + 0x23c, yaw);
            const float left = class28_aim(m, 0, lwc1(m + 0x48));
            class28_head_turn(m, yaw);
            if (lw(pv + 0x114) == 2) {
                if (lw(pv + 0x240) == -1) return Exit::Turn;
                sb(m + 0x20, 8);
                if (lbu(m + 0x53) != 3) anim(m, 3, 0x14);
                const int32_t point = lw(pv + 0x24c) - 1;
                sw(pv + 0x24c, static_cast<uint32_t>(point));
                const float frac = lwc1(path_at(lw(pv + 0x240)) + (static_cast<uint32_t>(point) << 4) + 0x1c);
                swc1(pv + 0x254, 0.0f);
                swc1(pv + 0x250, frac);
                swc1(pv + 0x27c, -1.0f);
                swc1(pv + 0x25c, 0.0f);
                swc1(pv + 0x258, 0.0f);
                fx1471_release(m);
                return Exit::Turn;
            }
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x260))) == 0) return Exit::Turn;
            if (class28_group_firing(lbu(m + 0x21)) >= 2) return Exit::Turn;
            if (fx1471_claim(m, 1, 0) == 0) return Exit::Turn;
            if (rand_mod(4) != 0) return Exit::Turn;
            const float degree = u2f(0x3c8efa35u);  // (1 degree)
            if (!(angle_dist(lwc1(m + 0x48), yaw) < degree)) return Exit::Turn;
            if (!(left < degree)) return Exit::Turn;
            if (path_crossed(lwu(pv + 0x244), m + 0x10, target + 0x10) != 0) return Exit::Turn;
            class28_group_pause(lbu(m + 0x21));
            swc1(pv + 0x278, yaw);
            sb(m + 0x20, 5);
            anim(m, 4, 5);
            sh(pv + 0x262, static_cast<uint32_t>(frame_scale_i(lw(t + 0x44))));
            return Exit::Turn;
        }
        case 5: {  // (L14:0x002b212c) firing starts: turned to the arc's first edge
            const float yaw = yaw_to(m, target);
            class28_aim(m, 0, angle_sub(yaw, lwc1(t + 0x40)));
            class28_head_turn(m, yaw);
            if (anim_done(m)) {
                anim(m, 5, 0xa);
                sb(m + 0x20, 6);
            }
            return Exit::Turn;
        }
        case 6: {  // (L14:0x002b21bc) firing: sweeping the arc, the muzzle's line tested; then back to aiming
            int32_t down = 0;
            const float yaw = yaw_to(m, target);
            const float frac = fpu_div(int_to_float(lh(pv + 0x262)), int_to_float(frame_scale_i(lw(t + 0x44))));
            const float arc = lwc1(t + 0x40);
            const float sweep = angle_sub(lwc1(pv + 0x278), fpu_add(-arc, fpu_mul(fpu_add(arc, arc), frac)));
            if (lbu(pv + 0x24b) != 0 && fpu_sub(fpu_add(lwc1(target + 0x18), u2f(0x3dcccccdu)), lwc1(m + 0x18)) < 0.0f)
                down = 1;  // (0.1: the target's feet below the turret's)
            class28_aim(m, down, sweep);
            class28_head_turn(m, yaw);
            const uint32_t sp = ee_stack_alloc(0x150);
            const uint32_t frame = sp + 0x20, from = sp + 0x50, axis = sp + 0x60, to = sp + 0x70, rec = sp + 0x80;
            moby_chain_world_one(m, 0, frame);
            lq_sq(axis, frame + 0x10);
            if (down == 0) sw(axis + 8, 0);
            vec3_set_length(V(axis), V(axis), lwc1(t + 0x3c));
            vec3_add(V(to), V(from), V(axis));
            vec3_set_length(V(axis), V(axis), lwc1(t + 0x4c));
            vec3_add(V(from), V(from), V(axis));
            // the hit record: its direction the turret's heading (z 1), the turret, kinds 0x10001, damage 1, the class
            sw(rec + 0x10, m);
            sw(rec + 0x14, 0x10001);
            sw(rec + 0x20, 1);
            swc1(rec + 0x1c, 1.0f);
            swc1(rec, cos_vu(lwc1(m + 0x48)));
            swc1(rec + 4, sin_vu(lwc1(m + 0x48)));
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sb(rec + 0x19, 1);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, 0);
            if (segment_collide_v(V(from), V(to), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0)
                lq_sq(to, OV(0x00173e60));
            fx1471_set(m, frame, to);
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x262))) != 0) {
                anim(m, 6, 0xa);
                sb(m + 0x20, 7);
                fx1471_release(m);
            }
            ee_stack_free(0x150);
            return Exit::Turn;
        }
        case 7:  // (L14:0x002b23c0) firing ends
            if (anim_done(m)) {
                anim(m, 0, 0xa);
                sb(m + 0x20, 4);
            }
            return Exit::Turn;
        case 8:  // (L14:0x002b1eb0) riding its path back: calling its moby on the way; at the end asleep or waiting
            if (lbu(pv + 0x24a) != 0) class28_call(lwu(pv + 0x264));
            if (class28_path_step(m) == 0) return Exit::Turn;
            if (lbu(pv + 0x24a) != 0) {
                anim(m, 2, 0xa);
                sb(m + 0x20, 9);
            } else {
                sh(pv + 0x248, static_cast<uint32_t>(frame_scale_i(lw(t + 0x48))));
                sb(m + 0x20, 0xd);
            }
            return Exit::Turn;
        case 9: {  // (L14:0x002b1ce8) settling at its path's start, then asleep
            const uint32_t path = path_at(lw(pv + 0x240));
            spring_to(m + 0x10, pv + 0x254, lwc1(path + 0x10), u2f(0x3c23d70au), u2f(0x3e4ccccdu), 0.0f);
            spring_to(m + 0x14, pv + 0x258, lwc1(path + 0x14), u2f(0x3c23d70au), u2f(0x3e4ccccdu), 0.0f);
            if (!anim_done(m)) return Exit::Turn;
            anim(m, 0, 0xa);
            sb(m + 0x20, 1);
            swc1(m + 0x58, 1.0f);
            class28_hide(m);
            return Exit::Done;
        }
        case 10: {  // (L14:0x002b23fc) knocked back: when its animation (not 7) plays out, aiming again
            const uint32_t set = lbu(m + 0x52);
            if (anim_done(m) || (set != 7 && lbu(m + 0x53) != 7)) {
                moby_anim_set(m, 0, 0, 0xa);
                sb(m + 0x20, 4);
            }
            return Exit::Turn;
        }
        case 11: {  // (L14:0x002b246c) falling dead: on the ground it blows up
            if ((enemy_physics(m, pv + 0x60) & 0x160) == 0) return Exit::Done;
            fx1471_release(m);
            const uint32_t sp = ee_stack_alloc(0x150);
            const uint32_t at = sp + 0x20;
            moby_chain_point_v(m, 2, V(at));
            class28_spawn_shell(m, at);
            explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
            ee_stack_free(0x150);
            drop_bolts(m);
            return Exit::Retire;
        }
        case 12: {  // (L14:0x002b2548) blown up: a shell and a rocket thrown
            const uint32_t sp = ee_stack_alloc(0x150);
            const uint32_t at = sp + 0x20, speed = sp + 0xb0;
            moby_chain_point_v(m, 2, V(at));
            class28_spawn_shell(m, at);
            sw(speed, 0);
            class28_spawn_rocket(m, speed, 1);
            ee_stack_free(0x150);
            fx1471_release(m);
            return Exit::Retire;
        }
        case 13:  // (L14:0x002b1db4) waiting on its path, turned toward the target, until in range and its timer out
            seek(m + 0x48, pv + 0x23c, yaw_to(m, target));
            if (lw(pv + 0x114) == 2) return Exit::Turn;
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x248))) == 0) return Exit::Turn;
            sb(m + 0x20, 3);
            anim(m, 3, 0xa);
            swc1(m + 0x58, 1.0f);
            path_reset(pv, false);
            return Exit::Turn;
        default:
            return Exit::Turn;
        }
    };
    const Exit exit = run();
    if (exit == Exit::Retire) {
        moby_retire(m);
        return;
    }
    if (exit == Exit::Done) return;
    const uint32_t state = lbu(m + 0x20);
    if (!(state - 4 < 4) && state != 1) {
        class28_aim(m, 0, lwc1(m + 0x48));
        class28_head_turn(m, lwc1(m + 0x48));
    }
    class28_visuals(m);
}

}  // namespace rac1
