// Drek's fleet's (level 17) class 1379, ported by hand from the level's own code: Ratchet's ship for the space fight above
// the fleet. It waits on its pad (the triangle boards it, carrying 1.02 Gameplay, fix 10, always on the PAL 2.00 disc),
// fades in, flies (L17:0x2ec360: the stick turns it, the arena's box pulls it back (0x2edf10, the box +0x12c), the camera
// behind it, hits on the world and on mobys), fires (0x2ec000 its guns, 0x2ec150 its missiles), trails its exhaust, and
// lands or is lost. The original's order of calls and float operations kept; its tuning words are the level's own data
// (L17:0x161fe0 .. 0x162184). Level 17 only: `orig: L17:`, level data through lvd::* and the collision facade, the level's
// own data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
//
// The moby: +0x20 the state (below), +0xb0 its checkpoint (0xff none; collected: the moby goes), +0xbc nonzero: it does not
// move. The pvars: +0x00 its velocity, +0x10 its heading, +0x20 / +0x24 / +0x28 the camera's angles (+0xa0.. their speeds),
// +0x30 / +0x34 / +0x38 its roll, pitch and yaw, +0x40 / +0x50 its pad's place and angles, +0x60 the camera's offset (0 /
// 1), +0x61 the gun that fires next, +0x62 the countdown of its loss, +0x64 its speed, +0x68 the frames since the flight's
// start, +0x6a its missile's launcher, +0x6c / +0x70 the stick's yaw / pitch (+0x90 / +0x94 their speeds), +0x80 / +0x84
// the guns' / missiles' cooldowns, +0xb0 the field of view (+0xc0 its speed), +0xb4.. the camera's offset (+0xc8.. its
// speeds), +0xc4 the thrust shown, +0xdc the hits' cooldown, +0xe0 / +0xe4 the aim's screen point, +0xe8 / +0xf0 / +0xf4 /
// +0xf8 the HUD's, +0xec the missiles' target, +0xfc / +0x100 / +0x120 / +0x12c boxes (the landing, the won fight's
// landing, the boarding, the arena; -1 none), +0x104 its palette's texture, +0x108 / +0x10c its engine's and boost's sounds
// (-1 none), +0x110 the camera's arena box (-1: the point L17:0x1621f0), +0x11c a moby group, +0x124 the HUD's timer,
// +0x130 the screen's fade.
//
// States (the table L17:0x1e6e70): 0 set up, 1 on the pad (the prompt), 2 the fade in (boards), 3 nothing, 4 flies, 5 lands
// (back on its pad), 6 nothing, 7 the loss's countdown (the hero lost), 8 the fight won (to the landing box), 9 the fade
// out, then the moby goes, 10 nothing.
#include <game/ovl/veldin_orbit_own.h>

#include <game/collision/collision.h>
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ammo_pickup.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hydro.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_ship_exhaust.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/visibomb.h>
#include <game/ovl/visibomb_ball.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
float tune(uint32_t at) { return lwc1(at); }

constexpr uint32_t P = 0x0013f350;  // the hero (+0x2080 his moby, +0x15f0 .. the HUD's ship record: +0x15fc its health)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// a quadword of the EE stack zeroed (the original's sq of $zero)
void sq_zero(uint32_t a) {
    for (uint32_t k = 0; k < 16; k += 4) sw(a + k, 0);
}

// the sound slot `slot` (0x13e550 + 0x70 slot) still plays for `m`
bool sound_playing(int32_t slot, uint32_t m) {
    const uint32_t rec = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    return lwu(rec + 0x88) == m && lbu(rec + 0x74) != 0;
}

// the moby's sound in the pvar word `at` stopped (when it plays for the moby), the word -1
void sound_stop(uint32_t at, uint32_t m) {
    const int32_t slot = lw(at);
    if (slot != -1 && sound_playing(slot, m)) sub_0028d918(slot);
    sw(at, 0xffffffffu);
}

// the moby group +0x11c's mobys shown / solid or not (a group of 0 or less: none)
void ship_group(uint32_t pv, int32_t v) {
    if (lw(pv + 0x11c) > 0) group_set_flags(lw(pv + 0x11c), v, v, v);
}

// the screen's fade +0x130 toward `to`, 4 a second, the level's fade word set from it
void fade_toward(uint32_t pv, float to) {
    approach_f(pv + 0x130, to, fpu_mul(dt(), 4.0f));
    swc1(L17_ADDR(0x0015f3fc), lwc1(pv + 0x130));
}

// its checkpoint collected (the moby goes)
bool checkpoint_taken(uint32_t m) {
    const uint32_t cp = lbu(m + 0xb0);
    return cp != 0xff && lbu(0x0014c050u + cp + (lwu(0x0015ed84) << 4)) == 0xff;
}

// the hero off the ship: his moby's vehicle word, his state 0, the hydro display
void hero_off() {
    const uint32_t hm = lwu(P + 0x2080);
    sb(P + 0x20a5, 0);
    sw(hm + 0x98, 0);
    hero_set_state(0, 1);
    hydro_2eac18(0);
}

// a value clamped to [0, 1] the original's way (above 1 first)
float unit_clamp(float k) {
    if (1.0f < k) return 1.0f;
    if (k < 0.0f) return 0.0f;
    return k;
}
}  // namespace

// The mobys of class `cls` (all of them, those retiring aside): `hidden` (+0x34 bit 2: 0 set, other clear), `solid` (+0x31 /
// +0x34 bit 1: 0 not solid, other solid), `glow` (+0x94: 0 none, other the class's +0x10); -1 leaves each.
// orig: L17:0x00253a90
void vo_253a90_l17(int32_t cls, int32_t hidden, int32_t solid, int32_t glow) {
    if (lwu(L17_ADDR(0x0015ffe0)) < lwu(L17_ADDR(0x0015ffd8))) return;
    for (uint32_t m = lwu(L17_ADDR(0x0015ffd8)); !(lwu(L17_ADDR(0x0015ffe0)) < m); m += 0x100) {
        if (lh(m + 0xa6) != cls || static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;
        if (hidden != -1) sh(m + 0x34, hidden == 0 ? (lhu(m + 0x34) | 2u) : (lhu(m + 0x34) & 0xfffdu));
        if (solid != -1) {
            sb(m + 0x31, solid == 0 ? 0u : 1u);
            sh(m + 0x34, solid == 0 ? (lhu(m + 0x34) | 1u) : (lhu(m + 0x34) & 0xfffeu));
        }
        if (glow != -1) sw(m + 0x94, glow == 0 ? 0u : lwu(lwu(m + 0x24) + 0x10));
    }
}

// The guns: the aim (L17:0x16206c x 0x1620a0 a frame along `yaw`, `pitch`, plus its velocity); with the fire buttons
// (0x13caf0 & 0x84) and the cooldown (+0x80) run out, a class 1009 shot from gun 3 or 4 in turn, the cooldown
// L17:0x162070; the aim's point 23 ahead put on the screen (+0xe0 / +0xe4, the HUD's sight).
// orig: L17:0x002ec000
void vo_2ec000_l17(uint32_t m, uint32_t pv, float yaw, float pitch) {
    const Frame f(144);
    const uint32_t sp = f.sp, aim = sp, muzzle = sp + 0x10, lead = sp + 0x20, at = sp + 0x30;
    vec_from_spherical_v(V(aim), fpu_mul(tune(L17_ADDR(0x0016206c)), tune(L17_ADDR(0x001620a0))), yaw, pitch);
    vec3_add(V(aim), V(aim), V(pv));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x80))) != 0 && (lwu(0x0013caf0) & 0x84u) != 0) {
        moby_sound_start(3, 0, m);
        vec3_set_length(V(lead), V(aim), u2f(0x400ccccdu));
        moby_chain_point_v(m, (lbu(pv + 0x61) & 1u) + 3, V(muzzle));
        vec3_add(V(muzzle), V(muzzle), V(lead));
        class1009_launch(m, aim, muzzle, 200.0f, -1.0f);
        sw(pv + 0x80, lwu(L17_ADDR(0x00162070)));
        sb(pv + 0x61, lbu(pv + 0x61) ^ 1u);
    }
    vec3_set_length(V(aim), V(aim), 23.0f);
    vec3_add(V(at), V(aim), V(m + 0x10));
    hud_screen_point(at, pv + 0xe0, pv + 0xe4, 0);
}

// The missiles: with the buttons (0x13caf0 & 0x28), the cooldown (+0x84) run out and missiles left (0x140946), a class
// 295 rocket from its launchers 7 .. 10 in turn at the target +0xec, for 300 frames at L17:0x162074 x 0x1620a0 a frame;
// the cooldown L17:0x162078.
// orig: L17:0x002ec150
void vo_2ec150_l17(uint32_t m, uint32_t pv) {
    const Frame f(144);
    const uint32_t sp = f.sp, from = sp, ahead = sp + 0x10;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x84))) == 0 || (lwu(0x0013caf0) & 0x28u) == 0 ||
        lbu(P + 0x15f6) == 0)
        return;
    const uint32_t k = (lhu(pv + 0x6a) + 1u) & 3u;
    sh(pv + 0x6a, k);
    moby_chain_point_v(m, k + 7, V(from));
    vec3_set_length(V(ahead), V(pv), u2f(0x3e99999au));
    vec3_add(V(from), V(from), V(ahead));
    const float speed = fpu_mul(tune(L17_ADDR(0x00162074)), tune(L17_ADDR(0x001620a0)));
    if (class295_launch(m, from, lwu(pv + 0xec), m + 0x40, frame_scale_i(0x12c), speed) != 0) {
        moby_sound_start(2, 0, m);
        sb(P + 0x15f6, lbu(P + 0x15f6) - 1u);
        sw(pv + 0x84, lwu(L17_ADDR(0x00162078)));
    }
}

// The arena's edge (the box +0x12c): where the ship would be in 60 frames; within 10 of the box's horizontal reach the
// stick's yaw (*yaw_in) is pulled toward turning back to its centre, within 10 of its top or bottom the stick's pitch
// (*pitch_in) toward levelling out, the more the nearer (and the steeper it climbs or dives); either sets the HUD's
// warning bit 2 of the hero's +0x160f.
// orig: L17:0x002edf10
void vo_2edf10_l17(uint32_t m, uint32_t yaw_in, uint32_t pitch_in) {
    const Frame f(144);
    const uint32_t sp = f.sp, ahead = sp;
    const uint32_t pv = lwu(m + 0x78);
    const float frames = static_cast<float>(frame_scale_i(0x3c));
    vec3_scale(V(ahead), V(pv + 0x10), fpu_mul(lwc1(pv + 0x64), frames));
    vec3_add(V(ahead), V(ahead), V(m + 0x10));
    const float z = lwc1(ahead + 8);
    vec3_sub(V(ahead), V(ahead), V(lvd::box(lvd::BOXES, lw(pv + 0x12c)) + 0x30));
    const float off = vec2_length(V(ahead));
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x12c));
    const float reach = vec2_length(V(box));
    const float half = lwc1(box + 0x28), mid = lwc1(box + 0x38);
    const float top = fpu_add(half, mid), bottom = fpu_add(-half, mid);
    const float back = angle_add(atan2_f(lwc1(ahead + 0), lwc1(ahead + 4)), u2f(0x40490fd0u));
    if (fpu_sub(reach, 10.0f) < off) {
        const float k = unit_clamp(fpu_div(fpu_add(fpu_sub(off, reach), 10.0f), 10.0f));
        const float turn = angle_sub(back, lwc1(pv + 0x38));
        const float y = lwc1(yaw_in);
        const float goal = 0.0f < turn ? fpu_sub(-1.0f, y) : fpu_sub(1.0f, y);
        swc1(yaw_in, fpu_add(fpu_mul(goal, k), y));
        sb(P + 0x160f, lbu(P + 0x160f) | 2u);
    }
    const float lim = u2f(0x3f060a92u);  // (30 degrees)
    float k, goal;
    if (fpu_sub(top, 10.0f) < z) {
        const float q = fpu_div(fpu_sub(lim, lwc1(pv + 0x34)), lim);
        k = fpu_mul(unit_clamp(fpu_div(fpu_add(fpu_sub(z, top), 10.0f), 10.0f)), unit_clamp(q));
        goal = -1.0f;
    } else if (z < fpu_add(bottom, 10.0f)) {
        const float q = fpu_div(fpu_add(lwc1(pv + 0x34), lim), lim);
        k = fpu_mul(unit_clamp(fpu_div(fpu_add(fpu_sub(bottom, z), 10.0f), 10.0f)), unit_clamp(q));
        goal = 1.0f;
    } else {
        return;
    }
    const float p = lwc1(pitch_in);
    swc1(pitch_in, fpu_add(fpu_mul(fpu_sub(goal, p), k), p));
    sb(P + 0x160f, lbu(P + 0x160f) | 2u);
}

// The flight: the speed (+0x64: the boost button 0x40 to L17:0x162008, else back to 0x162004; eased up over the flight's
// first frames, 0x162000), the stick's turns sprung (R1, 0x200, for the slow turn; the arena's edge, 0x2edf10, pulling
// it), the roll, pitch and yaw, the pull back toward the camera's arena (its box +0x110, or the point L17:0x1621f0), the
// move, the camera behind (+0x60's offset, its angles and field of view sprung), a hit on the world or a moby (a part of
// the HUD's health off, a blast; shot down at 0), the HUD's callback 0x2eb9a8 queued, the missiles and the guns. (The
// original returns a register its callers never read.)
// orig: L17:0x002ec360
void vo_2ec360_l17(uint32_t m, uint32_t pv) {
    const Frame f(480);
    const uint32_t sp = f.sp;
    const uint32_t vel = sp + 0x40, push = sp + 0x30, goal = sp + 0x80, ext = sp + 0x90;
    const float scale = tune(L17_ADDR(0x001620a0));
    sq_zero(sp + 0x40);
    sq_zero(sp + 0x60);
    const float lift = fpu_mul(fpu_mul(dt(), 8.0f), scale);
    sh(m + 0x34, lhu(m + 0x34) & 0xfeffu);
    swc1(sp + 0x68, 1.0f);
    swc1(sp + 0x48, lift);
    lq_sq(push, sp + 0x40);

    // the speed
    if (lbu(m + 0xbc) != 0) {
        swc1(pv + 0x64, u2f(0x38d1b717u));
    } else if (lw(L17_ADDR(0x00162000)) < lh(pv + 0x68)) {
        if ((lwu(0x0013caf4) & 0x40u) != 0) {  // the boost pressed: its sound
            sound_stop(pv + 0x10c, m);
            sw(pv + 0x10c, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        } else if ((lwu(0x0013caf8) & 0x40u) != 0) {  // released
            sound_stop(pv + 0x10c, m);
        }
        if ((lwu(0x0013caf0) & 0x40u) != 0) {  // held: up to the top speed
            const float up = fpu_add(lwc1(pv + 0x64), fpu_mul(tune(L17_ADDR(0x0016200c)), scale));
            const float top = fpu_mul(tune(L17_ADDR(0x00162008)), scale);
            swc1(pv + 0x64, top < up ? top : up);
        } else {  // back to the cruise
            const float cruise = fpu_mul(tune(L17_ADDR(0x00162004)), scale);
            const float a = fpu_mul(tune(L17_ADDR(0x00161ffc)), scale), v = lwc1(pv + 0x64);
            if (v < fpu_sub(cruise, a)) swc1(pv + 0x64, fpu_add(v, a));
            else if (cruise < v) swc1(pv + 0x64, fpu_add(v, fpu_mul(fpu_sub(cruise, v), tune(L17_ADDR(0x00162010)))));
            else swc1(pv + 0x64, cruise);
        }
    } else {
        sh(pv + 0x68, lhu(pv + 0x68) + 1u);
        swc1(pv + 0x64, fpu_add(lwc1(pv + 0x64), fpu_mul(tune(L17_ADDR(0x00161ff8)), scale)));
    }

    // the stick
    if (lw(L17_ADDR(0x00162014)) < lh(pv + 0x68)) {
        const uint32_t yaw_in = sp + 0x110, pitch_in = sp + 0x114;
        float lim_y = tune(L17_ADDR(0x001620b4)), lim_p = tune(L17_ADDR(0x001620c4));
        const float sy = lwc1(0x00141070), sp_ = lwc1(0x00141074);
        swc1(yaw_in, sy);
        swc1(pitch_in, sp_);
        if ((lwu(0x0013cae0) & 0x200u) != 0) {  // R1: the slow turn
            const float y = fpu_div(sy, tune(L17_ADDR(0x00162028)));
            const float p = fpu_div(sp_, tune(L17_ADDR(0x00162024)));
            lim_y = tune(L17_ADDR(0x001620b8));
            lim_p = tune(L17_ADDR(0x001620c8));
            swc1(yaw_in, fpu_mul(y, tune(L17_ADDR(0x00162030))));
            swc1(pitch_in, fpu_mul(p, tune(L17_ADDR(0x0016202c))));
        }
        vo_2edf10_l17(m, yaw_in, pitch_in);
        spring_to(pv + 0x6c, pv + 0x90, lwc1(yaw_in), tune(L17_ADDR(0x001620ac)), tune(L17_ADDR(0x001620b0)), lim_y);
        spring_to(pv + 0x70, pv + 0x94, lwc1(pitch_in), tune(L17_ADDR(0x001620bc)), tune(L17_ADDR(0x001620c0)), lim_p);
        spring_angle(pv + 0x30, pv + 0x98, 0, fpu_mul(lwc1(pv + 0x6c), tune(L17_ADDR(0x0016208c))), tune(L17_ADDR(0x001620cc)),
                     tune(L17_ADDR(0x001620d0)), tune(L17_ADDR(0x001620d4)));
        swc1(pv + 0x34, angle_sub(lwc1(pv + 0x34), fpu_mul(lwc1(pv + 0x70), tune(L17_ADDR(0x00162024)))));
        swc1(pv + 0x38, angle_sub(lwc1(pv + 0x38), fpu_mul(lwc1(pv + 0x6c), tune(L17_ADDR(0x00162028)))));
        const float lim = u2f(0x3fb2b8c2u);  // (80 degrees)
        if (lim < lwc1(pv + 0x34)) swc1(pv + 0x34, lim);
        else if (lwc1(pv + 0x34) < -lim) swc1(pv + 0x34, -lim);
        if (lim < lwc1(m + 0x44)) swc1(m + 0x44, lim);
        else if (lwc1(m + 0x44) < -lim) swc1(m + 0x44, -lim);
    }

    // the camera's arena: its box (+0x110: its centre, its extents) or the point L17:0x1621f0 (310 / 500)
    float reach_z = 310.0f, reach_xy = 500.0f;
    if (lw(pv + 0x110) != -1) {
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x110));
        lq_sq(goal, box + 0x30);
        reach_z = vec3_length(V(box + 0x20));
        vec3_add(V(ext), V(box), V(box + 0x10));
        reach_xy = vec3_length(V(ext));
    } else {
        lq_sq(goal, L17_ADDR(0x001621f0));
    }
    {
        const float d = vec2_distance(V(m + 0x10), V(L17_ADDR(0x001621f0)));
        const float to = atan2_f(fpu_sub(lwc1(goal + 0), lwc1(m + 0x10)), fpu_sub(lwc1(goal + 4), lwc1(m + 0x14)));
        swc1(pv + 0x38, ease_between(1, 5, d, lwc1(pv + 0x38), to, 50.0f, reach_xy));
        const float to2 = atan2_f(fpu_sub(lwc1(goal + 0), lwc1(m + 0x10)), fpu_sub(lwc1(goal + 4), lwc1(m + 0x14)));
        swc1(m + 0x48, ease_between(1, 5, d, lwc1(m + 0x48), to2, 50.0f, reach_xy));
    }
    for (uint32_t at : {pv + 0x34, m + 0x44}) {
        const float dz = abs_f(fpu_sub(lwc1(goal + 8), lwc1(m + 0x18)));
        const float pitch = -atan2_f(vec2_distance(V(m + 0x10), V(goal)), fpu_sub(lwc1(goal + 8), lwc1(m + 0x18)));
        swc1(at, ease_between(1, 5, dz, lwc1(at), pitch, 50.0f, reach_z));
    }

    // the move
    vec_from_spherical_v(V(vel), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
    vec3_set_length(V(pv + 0x10), V(vel), 1.0f);
    lq_sq(sp + 0x20, m + 0x10);
    if (lbu(m + 0xbc) == 0) vec3_add(V(m + 0x10), V(m + 0x10), V(vel));

    // the thrust shown (+0xc4): the speed between the cruise and the top
    float thrust;
    {
        const float cruise = tune(L17_ADDR(0x00162004));
        const float over = fpu_sub(lwc1(pv + 0x64), cruise);
        const float span = fpu_sub(tune(L17_ADDR(0x00162008)), cruise);
        thrust = over < 0.0f ? fpu_div(0.0f, span) : fpu_div(over, span);
    }
    // the camera's offsets (two entries, +0x60 picks): behind and above by the stick and the thrust shown
    {
        const uint32_t a = sp + 0xc0, b = sp + 0xd0;
        const float pitch = lwc1(pv + 0x70), k = tune(L17_ADDR(0x0016209c));
        sq_zero(a);
        sq_zero(b);
        const float back = fpu_sub(fpu_sub(tune(L17_ADDR(0x00162034)), fpu_mul(pitch, tune(L17_ADDR(0x00162138)))),
                                   fpu_mul(thrust, tune(L17_ADDR(0x00162140))));
        const float side = fpu_mul(lwc1(pv + 0x6c), tune(L17_ADDR(0x00162090)));
        const float up = fpu_add(tune(L17_ADDR(0x00162038)), fpu_mul(pitch, tune(L17_ADDR(0x0016213c))));
        swc1(a + 4, fpu_mul(side, k));
        swc1(a + 8, fpu_mul(up, k));
        swc1(b + 0, fpu_mul(tune(L17_ADDR(0x0016203c)), k));
        swc1(b + 8, fpu_mul(tune(L17_ADDR(0x00162040)), k));
        swc1(a + 0, fpu_mul(back, k));
        lq_sq(sp + 0xb0, b);
        lq_sq(sp + 0xa0, a);
    }
    swc1(pv + 0xc4, thrust);
    auto offs = [&] { return sp + 0xa0 + (static_cast<uint32_t>(lbu(pv + 0x60)) << 4); };
    spring_to(pv + 0xb4, pv + 0xc8, lwc1(offs() + 0), tune(L17_ADDR(0x00162114)), tune(L17_ADDR(0x00162118)), tune(L17_ADDR(0x0016211c)));
    spring_to(pv + 0xb8, pv + 0xcc, lwc1(offs() + 4), tune(L17_ADDR(0x00162120)), tune(L17_ADDR(0x00162124)), tune(L17_ADDR(0x00162128)));
    spring_to(pv + 0xbc, pv + 0xd0, lwc1(offs() + 8), tune(L17_ADDR(0x0016212c)), tune(L17_ADDR(0x00162130)), tune(L17_ADDR(0x00162134)));
    {
        const uint32_t mat = sp + 0xe0, cam = sp + 0xc0;
        swc1(ext + 0, lwc1(pv + 0xb4));
        swc1(ext + 4, lwc1(pv + 0xb8));
        swc1(ext + 8, lwc1(pv + 0xbc));
        sw(ext + 0xc, 0);
        euler_to_mat3_vu(M(mat), V(pv + 0x30));
        mat3_transform(V(goal), V(ext), M(mat));
        vec3_add(V(cam), V(m + 0x10), V(goal));
        uid5_set_pvar_80(cam);
        const uint32_t want = sp + 0xd0;
        swc1(want + 0, angle_add(fpu_mul(lwc1(pv + 0x30), tune(L17_ADDR(0x00162048))), fpu_mul(lwc1(pv + 0x6c), tune(L17_ADDR(0x00162044)))));
        const float tilt = angle_add(lwc1(pv + 0x34), tune(L17_ADDR(0x0016204c)));
        swc1(want + 8, lwc1(pv + 0x38));
        swc1(want + 4, angle_sub(tilt, fpu_mul(lwc1(pv + 0x70), tune(L17_ADDR(0x00162024)))));
        spring_angle(pv + 0x20, pv + 0xa0, 0, lwc1(want + 0), tune(L17_ADDR(0x001620f0)), tune(L17_ADDR(0x001620f4)), tune(L17_ADDR(0x001620f8)));
        spring_angle(pv + 0x24, pv + 0xa4, 0, lwc1(want + 4), tune(L17_ADDR(0x001620fc)), tune(L17_ADDR(0x00162100)), tune(L17_ADDR(0x00162104)));
        spring_angle(pv + 0x28, pv + 0xa8, 0, lwc1(want + 8), tune(L17_ADDR(0x00162108)), tune(L17_ADDR(0x0016210c)), tune(L17_ADDR(0x00162110)));
        const uint32_t rot = sp + 0x70;
        swc1(rot + 0, lwc1(pv + 0x20));
        swc1(rot + 8, lwc1(pv + 0x28));
        swc1(rot + 4, lwc1(pv + 0x24));
        uid5_set_pvar_90(rot);
    }
    {  // the field of view, by the thrust shown
        const float lo = tune(L17_ADDR(0x00162144));
        const float want = fpu_add(lo, fpu_mul(fpu_sub(tune(L17_ADDR(0x00162148)), lo), thrust));
        spring_to(pv + 0xb0, pv + 0xc0, want, tune(L17_ADDR(0x0016214c)), tune(L17_ADDR(0x00162150)), tune(L17_ADDR(0x00162154)));
        const float h = fpu_mul(lwc1(pv + 0xb0), 0.5f);
        const float s = sin_vu(h);
        swc1(L17_ADDR(0x0016d3f0), fpu_div(s, cos_vu(h)));
        projection_setup();
    }
    swc1(m + 0x40, angle_add(lwc1(pv + 0x30), fpu_mul(lwc1(pv + 0x6c), tune(L17_ADDR(0x00162160)))));
    swc1(m + 0x44, angle_add(lwc1(pv + 0x34), fpu_mul(-lwc1(pv + 0x70), tune(L17_ADDR(0x00162164)))));
    swc1(m + 0x48, angle_add(lwc1(pv + 0x38), fpu_mul(-lwc1(pv + 0x6c), tune(L17_ADDR(0x00162168)))));

    // a hit on the world or a moby (not of the classes 0x192, 0x4c2, 0x4c3, 0x52, 0x53, nor class 0)
    if (sphere_collide(m + 0x10, u2f(0x3f99999au), 0, m) != 0) {
        const uint32_t hm = coll_hit_moby();
        const int32_t cls = hm != 0 ? lh(hm + 0xa6) : -1;
        if (!(hm != 0 && (cls == 0 || cls == 0x192 || cls == 0x4c2 || cls == 0x4c3 || cls == 0x52 || cls == 0x53))) {
            V(m + 0x10) = coll_hit_pushed();
            const Vec4 n = coll_hit_normal();
            vec3_reflect(V(vel), V(vel), n);
            swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), fpu_mul(angle_sub(atan2_f(lwc1(vel + 0), lwc1(vel + 4)), lwc1(pv + 0x38)), tune(L17_ADDR(0x00162018)))));
            const float climb = -atan2_f(vec2_length(V(vel)), lwc1(vel + 8));
            swc1(pv + 0x34, angle_add(lwc1(pv + 0x34), fpu_mul(angle_sub(climb, lwc1(pv + 0x34)), tune(L17_ADDR(0x0016201c)))));
            swc1(pv + 0x64, fpu_mul(lwc1(pv + 0x64), tune(L17_ADDR(0x0016205c))));
            const uint32_t met = coll_hit_moby();
            if (met != 0 && lh(met + 0xa6) != 0x15b) {  // a moby (not a hull cannon): the hits' cooldown
                if (lw(pv + 0xdc) == 0) {
                    const float left = fpu_sub(lwc1(P + 0x15fc), tune(L17_ADDR(0x00162060)));
                    swc1(P + 0x15fc, left);
                    if (left < 0.0f) {  // shot down by the crash
                        // (the original also sets the triangle's edge and the normal to length 1 in its frame, which nothing reads)
                        const uint32_t bounce = sp + 0x30;
                        vec3_reflect(V(bounce), V(vel), n);
                        vec3_set_length(V(bounce), V(bounce), fpu_add(dt(), dt()));
                        explosion_effect(m, bounce, 0, 0x1e, 0xa, 0x18, -1, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
                        sb(m + 0x20, 7);
                    }
                    sw(pv + 0xdc, lwu(L17_ADDR(0x0016215c)));
                }
            } else {  // the world or a hull cannon: shot down at once
                sw(P + 0x15fc, 0);
                explosion_effect(m, push, 0, 0x1e, 0xa, 0x18, -1, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
                sh(pv + 0x62, lhu(L17_ADDR(0x0016217c)));
                sb(m + 0x20, 7);
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            }
        }
    }

    sub_001f93e0(L17_ADDR(0x002eb9a8), m);  // (the HUD's draw callback)
    vo_2ec150_l17(m, pv);
    const float yaw = angle_add(lwc1(pv + 0x28), fpu_div(-lwc1(pv + 0x6c), 7.0f));
    const float pitch = angle_add(lwc1(pv + 0x24), fpu_sub(-fpu_div(lwc1(pv + 0x70), 7.0f), u2f(0x3dcccccdu)));
    vo_2ec000_l17(m, pv, yaw, -pitch);
}

// orig: L17:0x002ed018
void vo_class1379_update_l17(uint32_t m) {
    if (m == 0) return;
    const Frame f(208);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const bool fix10 = gameplay102_on(10) || region_core_moved();  // (1.02 Gameplay, fix 10: the PAL 2.00 disc's own code)
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xdc)));

    // a hit while it flies: the HUD's health (+0x15fc) down by the damage, a push and a turn; or shot down
    {
        const uint32_t st = lbu(m + 0x20);
        if (!(st == 7 || st == 1 || st == 2)) {
            const uint32_t hit = sub_0025a420(m, 0x30001u, 0);
            if (hit != 0 && lbu(m + 0x20) == 4) {
                const float health = lwc1(P + 0x15fc), dmg = lwc1(hit + 0x2c);
                const uint32_t dir = sp + 0x20;
                if (dmg < health) {
                    swc1(P + 0x15fc, fpu_sub(health, dmg));
                    if ((lwu(hit + 0x30) & 1u) != 0) {
                        swc1(L17_ADDR(0x001676e0), 1.0f);
                        sw(L17_ADDR(0x001676e8), static_cast<uint32_t>(frame_scale_i(4)));
                        const float w = fpu_mul(lwc1(hit + 0x2c), tune(L17_ADDR(0x00162088)));
                        vec_from_spherical_v(V(dir), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                        vec3_clamp_length_vu(V(hit + 0x10), V(hit + 0x10), fpu_mul(dt(), 3.0f));
                        vec3_add(V(m + 0x10), V(m + 0x10), V(hit + 0x10));
                        vec3_reflect(V(dir), V(dir), V(hit + 0x10));
                        const float turn = fpu_mul(angle_sub(atan2_f(lwc1(dir + 0), lwc1(dir + 4)), lwc1(pv + 0x38)), tune(L17_ADDR(0x00162018)));
                        swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), fpu_mul(turn, w)));
                        const float up = atan2_f(vec2_length(V(dir)), lwc1(dir + 8));
                        const float tilt = fpu_mul(angle_sub(up, lwc1(pv + 0x34)), tune(L17_ADDR(0x0016201c)));
                        swc1(pv + 0x34, angle_add(lwc1(pv + 0x34), fpu_mul(tilt, w)));
                    }
                } else {  // shot down: Ratchet thrown off, the HUD's ship gone
                    vec_from_spherical_v(V(dir), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                    swc1(L17_ADDR(0x001676e0), u2f(0x3e4ccccdu));
                    sw(L17_ADDR(0x001676e8), static_cast<uint32_t>(frame_scale_i(5)));
                    sw(P + 0x15fc, 0);
                    sb(m + 0x20, 7);
                    sh(pv + 0x62, static_cast<uint32_t>(frame_scale_i(0x1e)));
                    const uint32_t hm = lwu(P + 0x2080);
                    swc1(P + 0x88, fpu_add(lwc1(P + 0x88), u2f(0x3e4ccccdu)));
                    sw(hm + 0x98, 0);
                    hero_set_state(0, 1);
                    sh(P + 0x15f4, 0xffffu);
                    sw(P + 0x15f0, 0);
                    explosion_effect(m, dir, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
                    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                }
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if (lwu(P + 0x15f0) == m)  // the HUD's bar: the health in 200ths of the full health L17:0x162084
        swc1(P + 0x1608, eer::cvt_w(fpu_mul(fpu_div(lwc1(P + 0x15fc), tune(L17_ADDR(0x00162084))), 200.0f)));

    switch (lbu(m + 0x20)) {  // (the table L17:0x1e6e70)
    case 0: {  // (0x002ed310): set up
        sb(m + 0x20, 1);
        sb(pv + 0x60, 0);
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        swc1(pv + 0x20, lwc1(pv + 0x30));
        swc1(pv + 0x24, lwc1(pv + 0x34));
        const float h = atan2_f(1.0f, lwc1(L17_ADDR(0x0016d3f0)));
        const float fov = fpu_add(h, h);
        sw(pv + 0xc0, 0);
        swc1(pv + 0xb0, fov);
        swc1(pv + 0xac, fov);
        lq_sq(pv + 0x30, m + 0x40);
        lq_sq(pv + 0x50, m + 0x40);
        lq_sq(pv + 0x40, m + 0x10);
        const uint32_t pal = lwu(L17_ADDR(0x0015f420)) +
                             (static_cast<uint32_t>(lhu(L17_ADDR(0x0016da80) + ((lwu(pv + 0x104) + 0x28u) << 4) + 0xa)) << 4);
        const int32_t copied = lw(L17_ADDR(0x00162184));
        sw(L17_ADDR(0x00162180), pal);
        if (copied == 0) {  // (its palette kept once: 0x400 bytes)
            const uint32_t dst = L17_ADDR(0x001d9b68);
            for (uint32_t k = 0; k < 0x400u; ++k) sb(dst + k, lbu(pal + k));
        }
        sw(L17_ADDR(0x00162184), 1);
        sw(pv + 0xe8, 1);
        sw(pv + 0xf8, 1);
        if (checkpoint_taken(m)) sb(m + 0x20, 9);
        ship_group(pv, 0);
        vo_253a90_l17(0x325, 1, 1, -1);
        sw(pv + 0x124, 0xffffffffu);
        break;
    }
    case 1: {  // (0x002ed4e0): on its pad, the prompt; the triangle boards it
        bool go = false;
        if (moby_sphere_gap(m, lwu(P + 0x2080)) < tune(L17_ADDR(0x00161fe0)) && !gameplay102_talk_blocked()) {  // (1.02 Gameplay, fix 10)
            if (hud_message_request(7, TXT(0x53e4)) != 0) go = (lwu(0x0013cae4) & 0x10u) != 0;
        }
        if (go) {
            hud_message_release(7);
            if (fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: 0x32 at the press)
            sb(m + 0x20, 2);
            sw(pv + 0x124, 0xffffffffu);
        } else if (lwc1(pv + 0x130) != 0.0f) {
            fade_toward(pv, 0.0f);
        }
        floor_light_add(m, fpu_mul(tune(L17_ADDR(0x00162054)), tune(L17_ADDR(0x0016209c))));
        break;
    }
    case 2: {  // (0x002ed5a4): the fade in; then Ratchet aboard, the flight starts
        fade_toward(pv, 1.0f);
        if (lwc1(pv + 0x130) != 1.0f) break;
        sw(P + 0x15f0, m);
        sb(P + 0x15f6, lbu(L17_ADDR(0x00162064)));
        sb(P + 0x15f8, lbu(L17_ADDR(0x00162068)));
        sb(P + 0x160e, 1);
        sb(P + 0x15f7, lbu(L17_ADDR(0x00162068)));
        swc1(P + 0x15fc, tune(L17_ADDR(0x00162058)));
        sh(P + 0x15f4, lhu(m + 0xa6));
        sw(pv + 0xf0, 0);
        sw(pv + 0xf4, 0xff);
        swc1(P + 0x1600, tune(L17_ADDR(0x00162084)));
        swc1(P + 0x1604, 100.0f);
        sb(P + 0x15fb, 3);
        sb(P + 0x15f9, 3);
        sb(P + 0x15fa, 3);
        sb(P + 0x160f, 0);
        music_track_request(2, 4);
        if (lw(pv + 0x120) != -1) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x120));
            hero_place(box + 0x30, box + 0x70, 0x32, 1);
            const uint32_t box2 = lvd::box(lvd::BOXES, lw(pv + 0x120));
            lq_sq(m + 0x10, box2 + 0x30);
            lq_sq(m + 0x40, box2 + 0x70);
        }
        hud_message_force(6, 0);
        if (!fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: skipped when set at the press)
        camera_scripted_set(m + 0x10, m + 0x40, 1, 0, 0);
        const float k = tune(L17_ADDR(0x0016209c));
        sh(pv + 0x68, 0);
        swc1(pv + 0x64, tune(L17_ADDR(0x00161fe4)));
        sw(pv + 0x6c, 0);
        sw(pv + 0x70, 0);
        sw(pv + 0x84, 0);
        sw(pv + 0x80, 0);
        sb(m + 0x30, 0xff);
        const float yaw = lwc1(m + 0x48);
        swc1(pv + 0x38, yaw);
        swc1(pv + 0x28, yaw);
        swc1(pv + 0xb4, fpu_mul(tune(L17_ADDR(0x00162034)), k));
        swc1(pv + 0xbc, fpu_mul(tune(L17_ADDR(0x00162038)), k));
        sw(pv + 0xb8, 0);
        sh(m + 0x32, 0xff);
        sw(lwu(P + 0x2080) + 0x98, 0xffffffffu);
        vec_from_spherical_v(V(pv), tune(L17_ADDR(0x00161fe4)), lwc1(pv + 0x38), -lwc1(pv + 0x34));
        ship_group(pv, 1);
        vo_253a90_l17(0x325, 0, 0, -1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 3.0f));
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), tune(L17_ADDR(0x0016209c))));
        sb(P + 0x20a5, 1);
        if (lw(pv + 0x108) == -1) sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        visibomb_203b18();
        sw(L17_ADDR(0x0015f608), 2);
        vo_2ec360_l17(m, pv);
        sb(m + 0x20, 4);
        break;
    }
    case 4: {  // (0x002ed7fc): flies
        hud_message_force(6, 0);
        sw(L17_ADDR(0x0015f608), 2);
        if (!sound_playing(lw(pv + 0x108), m)) {  // (its engine's sound again when it stopped)
            sw(pv + 0x108, 0xffffffffu);
            sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
        vo_2ec360_l17(m, pv);
        if (lw(pv + 0xe8) <= 0) {  // (the fight won)
            sw(pv + 0xf8, lwu(L17_ADDR(0x00162178)));
            sb(m + 0x20, 8);
        }
        if (lwc1(pv + 0x130) != 0.0f) fade_toward(pv, 0.0f);
        if ((lbu(P + 0x160f) & 1u) != 0) sb(m + 0x20, 5);
        break;
    }
    case 5: {  // (0x002ed8e8): back on its pad, Ratchet off at the landing box
        hero_off();
        sub_002359f8(static_cast<uint32_t>(lh(P + 0x160c)), 0);
        lq_sq(m + 0x10, pv + 0x40);
        lq_sq(m + 0x40, pv + 0x50);
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        moby_update_transform_kept(m);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0xfc));
        hero_place(box + 0x30, box + 0x70, 0, 1);
        music_track_request(0, 5);
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        const float f99 = u2f(0x3f7d70a4u);
        sb(m + 0x20, 1);
        swc1(pv + 0x130, f99);
        swc1(L17_ADDR(0x0015f3fc), f99);
        break;
    }
    case 7: {  // (0x002eda2c): the loss's countdown, then the hero lost
        hud_message_force(6, 0);
        sw(L17_ADDR(0x0015f608), 2);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x62))) == 0) break;
        visibomb_ball_203b78();
        sw(lwu(P + 0x2080) + 0x98, 0);
        hero_lost();
        hydro_2eac18(0);
        music_track_request(0, 5);
        lq_sq(m + 0x10, pv + 0x40);
        lq_sq(m + 0x40, pv + 0x50);
        moby_update_transform_kept(m);
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        sb(m + 0x20, 0);
        break;
    }
    case 8: {  // (0x002edb38): the fight won: flies on until its timer runs out and the screen fades, then lands
        hud_message_force(6, 0);
        sw(L17_ADDR(0x0015f608), 2);
        vo_2ec360_l17(m, pv);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xf8))) != 0) fade_toward(pv, 1.0f);
        if (!(1.0f <= lwc1(pv + 0x130))) break;
        hero_off();
        music_track_request(0, 5);
        if (lw(pv + 0x100) == -1) {
            debug_printf_stub();  // (the format L17:0x1e6e00)
            lq_sq(m + 0x10, pv + 0x40);
            lq_sq(m + 0x40, pv + 0x50);
        } else {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x100));
            lq_sq(m + 0x10, box + 0x30);
            lq_sq(m + 0x40, box + 0x70);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), u2f(0x3f12bbedu)));
        }
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        moby_update_transform_kept(m);
        if (lw(pv + 0xfc) == -1) {
            debug_printf_stub();  // (the format L17:0x1e6e38)
        } else {
            ship_group(pv, 0);
            vo_253a90_l17(0x325, 1, 1, -1);
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0xfc));
            hero_place(box + 0x30, box + 0x70, 0, 1);
        }
        if (lbu(m + 0xb0) != 0xff) checkpoint_mark_collected(lbu(m + 0xb0));
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        checkpoint_save(0x0013f3d0, 0x0013f3e0);
        sb(m + 0x20, 9);
        break;
    }
    case 9: {  // (0x002eddb4): the fade out, then the moby goes
        if (lwc1(pv + 0x130) == 0.0f) {
            moby_retire(m);
            return;
        }
        fade_toward(pv, 0.0f);
        break;
    }
    default:  // (3, 6, 10 and beyond: nothing)
        break;
    }

    // within the space's bounds (15 .. 1008 on each axis); flying: the hero's place and angles its; the exhaust
    for (uint32_t o = 0x10; o <= 0x18; o += 4) {
        if (lwc1(m + o) < 15.0f) swc1(m + o, 15.0f);
        if (1008.0f < lwc1(m + o)) swc1(m + o, 1008.0f);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 4) {
        lq_sq(0x0013f3d0, m + 0x10);
        lq_sq(0x0013f3e0, pv + 0x30);
    }
    if (st - 3u < 2u) ship_exhaust_spawn(m, pv);
}

}  // namespace rac1
