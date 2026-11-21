// Gemlik Base's (level 13) class 69: Ratchet's ship for Qwark's fight in space. It waits on its pad (the triangle boards it,
// carrying 1.02 Gameplay, fix 10, always on the PAL 2.00 disc), fades in, flies (0x2ba178: the stick turns it, the
// arena pulls it back, the camera behind it, hits on walls and mobys), fires (0x2b9df8 its guns, 0x2b9f58 its missiles),
// trails particles from its engines (0x2b8590) and lands or is lost. Level 13 only: the addresses are Gemlik's
// (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// The moby: +0x20 the state (below), +0xb0 its checkpoint (0xff none; collected: the moby goes). The pvars: +0x00 its
// velocity, +0x10 its heading, +0x20 / +0x24 / +0x28 the camera's angles (+0xa0.. their speeds), +0x30 / +0x34 / +0x38
// its roll, pitch and yaw, +0x40 / +0x50 its pad's place and angles, +0x60 the camera's offset (0 / 1), +0x61 the gun
// that fires next, +0x62 the countdown of its loss, +0x64 its speed, +0x68 the frames since the fight's start, +0x6a its
// missile's launcher, +0x6c / +0x70 the stick's yaw / pitch (+0x90 / +0x94 their speeds), +0x7c the screen's fade,
// +0x80 / +0x84 the guns' / missiles' cooldowns, +0xb0 the field of view (+0xc0 its speed), +0xb4.. the camera's offset
// (+0xc8.. its speeds), +0xc4 the thrust shown, +0xdc the hits' cooldown, +0xe0 / +0xe4 the aim's screen point, +0xe8 /
// +0xf8 / +0xf4 / +0xf0 the HUD's, +0xec the missiles' target, +0xfc / +0x100 / +0x12c / +0x130 / +0x134 boxes (the
// landing, the loss, the exit, the boarding, a box that boards it), +0x104 its palette's texture, +0x108 / +0x10c its
// engine's and boost's sounds (-1 none), +0x110 the arena's box (-1: the point L13:0x161430), +0x11c / +0x13c two moby
// groups, +0x120 the class of the last hit's maker (Qwark's ships 0x52 / 0x53; +10000 when it took the last of the
// health), +0x128 / +0x138 the HUD's timers.
//
// States (the table L13:0x1f44e0): 0 set up, 1 on the pad (the prompt), 2 the fade in (boards), 3 nothing, 4 flies,
// 5 lands, 6 shot down (the sounds stop), 7 the loss's countdown (the hero lost), 8 the fight won (to the landing box),
// 9 the moby goes, 10 nothing.
#include <game/collision/collision.h>
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ammo_pickup.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_particle_4a.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/visibomb.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float step60() { return lwc1(0x0015ed60); }  // (the frame's step at 60 Hz: 1.0)
float spring_k() { return lwc1(0x0015ed64); }

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

// the level's two moby groups (pvars +0x11c, +0x13c) shown / solid or not
void ship_groups(uint32_t pv, int32_t v) {
    if (lw(pv + 0x11c) > 0) group_set_flags(lw(pv + 0x11c), v, v, v);
    if (lw(pv + 0x13c) > 0) group_set_flags(lw(pv + 0x13c), v, v, v);
}

// the space fight's scenery shown (0) or hidden (1): the mobys of the class lists L13:0x1cbde8 and 0x161440, class 0x102
void ship_scenery(int32_t v) {
    gml_2651c8_l13(L13_ADDR(0x001cbde8), static_cast<uint32_t>(v), static_cast<uint32_t>(v), 0xffffffffu);
    gml_2651c8_l13(L13_ADDR(0x00161440), static_cast<uint32_t>(v), 0xffffffffu, 0xffffffffu);
    gml_265170_l13(0x102, 0);
}

// its checkpoint collected (the moby goes)
bool checkpoint_taken(uint32_t m) {
    const uint32_t cp = lbu(m + 0xb0);
    return cp != 0xff && lbu(0x0014c050u + cp + (lwu(0x0015ed84) << 4)) == 0xff;
}

// the camera's field of view and the flight's angles set from the moby (states 0 and 1's boarding)
void ship_view_start(uint32_t m, uint32_t pv) {
    swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
    swc1(pv + 0x20, lwc1(pv + 0x30));
    swc1(pv + 0x24, lwc1(pv + 0x34));
    const float h = atan2_f(1.0f, lwc1(L13_ADDR(0x0016cdf0)));
    const float fov = fpu_add(h, h);
    sw(pv + 0xc0, 0);
    sw(pv + 0x120, 0);
    swc1(pv + 0xb0, fov);
    swc1(pv + 0xac, fov);
    lq_sq(pv + 0x30, m + 0x40);
}
}  // namespace

// The moby of class `cls` (all of them) given the state `state`, but those retiring.
// orig: L13:0x00265170
void gml_265170_l13(int32_t cls, uint32_t state) {
    if (lwu(L13_ADDR(0x0015ffe0)) < lwu(L13_ADDR(0x0015ffd8))) return;
    for (uint32_t m = lwu(L13_ADDR(0x0015ffd8)); !(lwu(L13_ADDR(0x0015ffe0)) < m); m += 0x100)
        if (lh(m + 0xa6) == cls && static_cast<int8_t>(lbu(m + 0x20)) >= 0) sb(m + 0x20, state);
}

// The mobys of the classes listed at `list` (halfwords, up to 224, ended by 0 or one of 2048 or more), but those
// retiring: `hidden` (+0x34 bit 2: 0 set, other clear), `solid` (+0x31 / +0x34 bit 1: 0 not solid, other solid), `glow`
// (+0x94: 0 none, other the class's +0x10); -1 leaves each.
// orig: L13:0x002651c8
void gml_2651c8_l13(uint32_t list, uint32_t hidden, uint32_t solid, uint32_t glow) {
    if (lwu(L13_ADDR(0x0015ffe0)) < lwu(L13_ADDR(0x0015ffd8))) return;
    for (uint32_t m = lwu(L13_ADDR(0x0015ffd8)); !(lwu(L13_ADDR(0x0015ffe0)) < m); m += 0x100) {
        int32_t c = lh(list);
        if (c == 0 || c >= 2048) continue;
        const int32_t cls = lh(m + 0xa6);
        uint32_t e = list;
        for (int32_t n = 0;;) {
            if (cls == c && static_cast<int8_t>(lbu(m + 0x20)) >= 0) {
                if (hidden != 0xffffffffu) sh(m + 0x34, hidden == 0 ? (lhu(m + 0x34) | 2u) : (lhu(m + 0x34) & 0xfffdu));
                if (solid != 0xffffffffu) {
                    sb(m + 0x31, solid == 0 ? 0u : 1u);
                    sh(m + 0x34, solid == 0 ? (lhu(m + 0x34) | 1u) : (lhu(m + 0x34) & 0xfffeu));
                }
                if (glow != 0xffffffffu) sw(m + 0x94, glow == 0 ? 0u : lwu(lwu(m + 0x24) + 0x10));
            }
            e += 2;
            c = lh(e);
            ++n;
            if (c == 0 || c >= 2048 || n >= 224) break;
        }
    }
}

// The engines' trails: from its chain points 1 and 2, three particles of type 0x4a (the records L13:0x1613d8 .. 0x1613f8)
// blown back at a speed by the thrust shown (+0xc4) and the speed (+0x64), and five more over 0.5 of thrust.
// orig: L13:0x002b8590
void gml_2b8590_l13(uint32_t m, uint32_t pv) {
    const Frame f(304);
    const uint32_t sp = f.sp, vel = sp, at0 = sp + 0x10, at1 = sp + 0x20, at2 = sp + 0x30, at3 = sp + 0x40;
    auto spawn = [&](uint32_t at, float k, uint32_t rec, float scale) {
        const float r = rand_range_f(6.0f, 7.0f);
        swc1(vel + 0, fpu_mul(-fpu_mul(fpu_add(r, k), dt()), scale));
        particle_4a_spawn(m, at, vel, rec, 1);
    };
    for (uint32_t point = 1; point < 3; ++point) {
        sq_zero(vel);
        float scale = fpu_div(lwc1(pv + 0x64), fpu_mul(dt(), 24.0f));
        if (1.0f < scale) scale = 1.0f;
        else if (scale < u2f(0x3eb33333u)) scale = u2f(0x3eb33333u);
        moby_chain_point_v(m, point, V(at0));
        sw(at0 + 0xc, 0);
        lq_sq(at1, at0);
        lq_sq(at2, at0);
        lq_sq(at3, at0);
        const float k1 = u2f(0x3ca3d70au), k2 = u2f(0x3c23d70au);
        metal_25f8e0(at1, fpu_add(fpu_mul(lwc1(pv + 0xc4), k1), k1));
        metal_25f8e0(at2, fpu_add(fpu_mul(lwc1(pv + 0xc4), k1), k1));
        metal_25f8e0(at3, fpu_add(fpu_mul(lwc1(pv + 0xc4), k2), k1));
        spawn(at1, fpu_add(lwc1(pv + 0xc4), lwc1(pv + 0xc4)), L13_ADDR(0x001613d8), scale);
        spawn(at2, fpu_mul(lwc1(pv + 0xc4), 3.0f), L13_ADDR(0x001613e8), scale);
        spawn(at3, fpu_mul(lwc1(pv + 0xc4), 4.0f), L13_ADDR(0x001613f8), scale);
        if (0.5f < lwc1(pv + 0xc4)) {
            metal_25f8e0(at1, fpu_add(fpu_mul(lwc1(pv + 0xc4), k1), k2));
            metal_25f8e0(at2, fpu_add(fpu_mul(lwc1(pv + 0xc4), k1), k2));
            metal_25f8e0(at3, fpu_add(fpu_mul(lwc1(pv + 0xc4), k2), k2));
            spawn(at1, fpu_add(lwc1(pv + 0xc4), lwc1(pv + 0xc4)), L13_ADDR(0x001613d8), scale);
            spawn(at2, fpu_mul(lwc1(pv + 0xc4), 3.0f), L13_ADDR(0x001613e8), scale);
            spawn(at3, fpu_mul(lwc1(pv + 0xc4), 4.0f), L13_ADDR(0x001613f8), scale);
            spawn(at1, fpu_add(lwc1(pv + 0xc4), lwc1(pv + 0xc4)), L13_ADDR(0x00161408), scale);
            spawn(at0, fpu_mul(lwc1(pv + 0xc4), 3.0f), L13_ADDR(0x00161418), scale);
        }
    }
}

// The guns: the aim 400 a second along (`yaw`, `pitch`) plus its velocity; with the fire buttons (0x13caf0 & 0x84) and
// the cooldown (+0x80) run out, a class 1009 shot from gun 3 or 4 in turn; the aim's point 23 ahead put on the screen
// (+0xe0 / +0xe4, the HUD's sight).
// orig: L13:0x002b9df8
void gml_2b9df8_l13(uint32_t m, uint32_t pv, float yaw, float pitch) {
    const Frame f(144);
    const uint32_t sp = f.sp, aim = sp, muzzle = sp + 0x10, lead = sp + 0x20, at = sp + 0x30;
    vec_from_spherical_v(V(aim), fpu_mul(dt(), 400.0f), yaw, pitch);
    vec3_add(V(aim), V(aim), V(pv));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x80))) != 0 && (lwu(0x0013caf0) & 0x84u) != 0) {
        moby_sound_start(3, 0, m);
        vec3_set_length(V(lead), V(aim), u2f(0x400ccccdu));
        moby_chain_point_v(m, (lbu(pv + 0x61) & 1u) + 3, V(muzzle));
        vec3_add(V(muzzle), V(muzzle), V(lead));
        class1009_launch(m, aim, muzzle, 200.0f, -1.0f);
        sw(pv + 0x80, static_cast<uint32_t>(frame_scale_i(4)));
        sb(pv + 0x61, lbu(pv + 0x61) ^ 1u);
    }
    vec3_set_length(V(aim), V(aim), 23.0f);
    vec3_add(V(at), V(aim), V(m + 0x10));
    hud_screen_point(at, pv + 0xe0, pv + 0xe4, 0);
}

// The missiles: with the buttons (0x13caf0 & 0x28), the cooldown (+0x84) run out and missiles left (0x140946), a class
// 295 rocket from its launchers 7 .. 10 in turn at the target +0xec, for 300 frames at 100 a second.
// orig: L13:0x002b9f58
void gml_2b9f58_l13(uint32_t m, uint32_t pv) {
    const Frame f(144);
    const uint32_t sp = f.sp, from = sp, ahead = sp + 0x10;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x84))) == 0 || (lwu(0x0013caf0) & 0x28u) == 0 || lbu(0x00140946) == 0) return;
    const uint32_t k = (lhu(pv + 0x6a) + 1u) & 3u;
    sh(pv + 0x6a, k);
    moby_chain_point_v(m, k + 7, V(from));
    vec3_set_length(V(ahead), V(pv), u2f(0x3e99999au));
    vec3_add(V(from), V(from), V(ahead));
    const float speed = fpu_mul(dt(), 100.0f);
    if (class295_launch(m, from, lwu(pv + 0xec), m + 0x40, frame_scale_i(0x12c), speed) != 0) {
        moby_sound_start(2, 0, m);
        sb(0x00140946, lbu(0x00140946) - 1u);
        sw(pv + 0x84, static_cast<uint32_t>(frame_scale_i(0x1e)));
    }
}

// The flight: the speed (+0x64: the boost button 0x40 to 30 a second, else back to 24; eased up over the fight's first
// 120 frames), the stick's turns sprung (the R1 button 0x200 for the slow turn), the roll, pitch and yaw, the pull back
// into the arena (its box +0x110, or 50 around L13:0x161430), the move, the camera behind (+0x60's offset, its angles and
// field of view sprung), a hit on the world or a moby (5 of health off the HUD's 0x14094c, a blast; shot down at 0), the
// HUD's callback queued, the guns and missiles. (The original returns a register its callers never read.)
// orig: L13:0x002ba178
void gml_2ba178_l13(uint32_t m, uint32_t pv) {
    const Frame f(480);
    const uint32_t sp = f.sp;
    const uint32_t vel = sp + 0x40, zero_v = sp + 0x30, goal = sp + 0x80, ext = sp + 0x90;
    sq_zero(sp + 0x40);
    sq_zero(sp + 0x60);
    swc1(sp + 0x48, fpu_mul(dt(), 8.0f));
    swc1(sp + 0x68, 1.0f);
    sh(m + 0x34, lhu(m + 0x34) & 0xfeffu);
    lq_sq(zero_v, sp + 0x40);

    // the speed
    if (lbu(m + 0xbc) != 0) {
        swc1(pv + 0x64, u2f(0x38d1b717u));
    } else if (frame_scale_i(0x78) < lh(pv + 0x68)) {
        if ((lwu(0x0013caf4) & 0x40u) != 0) {  // the boost pressed: its sound
            sound_stop(pv + 0x10c, m);
            sw(pv + 0x10c, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        } else if ((lwu(0x0013caf8) & 0x40u) != 0) {  // released
            sound_stop(pv + 0x10c, m);
        }
        if ((lwu(0x0013caf0) & 0x40u) != 0) {  // held: to 30 a second
            const float sp1 = fpu_add(lwc1(pv + 0x64), fpu_mul(step60(), u2f(0x3d0f5c29u)));
            const float top = fpu_mul(dt(), 30.0f);
            swc1(pv + 0x64, top < sp1 ? top : sp1);
        } else {  // back to 24 a second
            const float cruise = fpu_mul(dt(), 24.0f), s60 = step60();
            const float a = fpu_mul(s60, u2f(0x3ccccccdu)), v = lwc1(pv + 0x64);
            if (v < fpu_sub(cruise, a)) swc1(pv + 0x64, fpu_add(v, a));
            else if (cruise < v) swc1(pv + 0x64, fpu_add(v, fpu_mul(fpu_sub(cruise, v), fpu_mul(s60, u2f(0x3dcccccdu)))));
            else swc1(pv + 0x64, cruise);
        }
    } else {
        swc1(pv + 0x64, fpu_add(lwc1(pv + 0x64), fpu_mul(step60(), u2f(0x3b449ba6u))));
        sh(pv + 0x68, lhu(pv + 0x68) + 1u);
    }

    // the stick
    if (frame_scale_i(5) < lh(pv + 0x68)) {
        const float s60 = step60();
        float lim_v = fpu_mul(s60, u2f(0x3dcccccdu)), yaw_in = lwc1(0x00141070), pitch_in = lwc1(0x00141074),
              lim_p = fpu_mul(s60, 0.25f);
        if ((lwu(0x0013cae0) & 0x200u) != 0) {  // R1: the slow turn
            const float a = fpu_mul(s60, u2f(0x3c490fdbu)), b = fpu_mul(s60, u2f(0x3c80adfdu));
            lim_p = fpu_mul(s60, u2f(0x3da3d70au));
            pitch_in = fpu_div(pitch_in, a);
            yaw_in = fpu_div(yaw_in, a);
            lim_v = lim_p;
            pitch_in = fpu_mul(pitch_in, b);
            yaw_in = fpu_mul(yaw_in, b);
        }
        const float kd = u2f(0x3cf5c28fu), turn = u2f(0x3c490fdbu);
        spring_to(pv + 0x6c, pv + 0x90, yaw_in, fpu_mul(spring_k(), kd), spring_k(), lim_v);
        spring_to(pv + 0x70, pv + 0x94, pitch_in, fpu_mul(spring_k(), kd), spring_k(), lim_p);
        spring_angle(pv + 0x30, pv + 0x98, 0, lwc1(pv + 0x6c), fpu_mul(spring_k(), u2f(0x3d4ccccdu)), spring_k(), step60());
        swc1(pv + 0x34, angle_sub(lwc1(pv + 0x34), fpu_mul(lwc1(pv + 0x70), fpu_mul(step60(), turn))));
        swc1(pv + 0x38, angle_sub(lwc1(pv + 0x38), fpu_mul(lwc1(pv + 0x6c), fpu_mul(step60(), turn))));
        const float lim = u2f(0x3fb2b8c2u);
        if (lim < lwc1(pv + 0x34)) swc1(pv + 0x34, lim);
        else if (lwc1(pv + 0x34) < -lim) swc1(pv + 0x34, -lim);
        if (lim < lwc1(m + 0x44)) swc1(m + 0x44, lim);
        else if (lwc1(m + 0x44) < -lim) swc1(m + 0x44, -lim);
    }

    // the arena: its box (+0x110: its centre, its extents) or the point L13:0x161430 (310 / 500)
    float reach_z = 310.0f, reach_xy = 500.0f;
    if (lw(pv + 0x110) != -1) {
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x110));
        lq_sq(goal, box + 0x30);
        reach_z = vec3_length(V(box + 0x20));
        vec3_add(V(ext), V(box), V(box + 0x10));
        reach_xy = vec3_length(V(ext));
    } else {
        lq_sq(goal, L13_ADDR(0x00161430));
    }
    {
        const float d = vec2_distance(V(m + 0x10), V(L13_ADDR(0x00161430)));
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

    // the camera's offsets (two entries, +0x60 picks): behind and above by the stick and the thrust shown (+0xc4)
    float thrust;
    {
        const float cruise = fpu_mul(dt(), 24.0f);
        const float over = fpu_sub(lwc1(pv + 0x64), cruise);
        const float span = fpu_sub(fpu_mul(dt(), 30.0f), cruise);
        thrust = over < 0.0f ? fpu_div(0.0f, span) : fpu_div(over, span);
    }
    {
        const uint32_t a = sp + 0xc0, b = sp + 0xd0;
        const float pitch = lwc1(pv + 0x70);
        sq_zero(a);
        sq_zero(b);
        const float back = fpu_mul(fpu_sub(fpu_sub(-15.0f, pitch), fpu_mul(thrust, 4.0f)), 0.25f);
        swc1(b + 0, u2f(0xbfa33333u));
        swc1(b + 8, u2f(0x3f266666u));
        swc1(a + 8, fpu_mul(fpu_add(fpu_add(pitch, pitch), 4.0f), 0.25f));
        swc1(a + 4, fpu_mul(fpu_mul(lwc1(pv + 0x6c), -3.0f), 0.25f));
        swc1(a + 0, back);
        swc1(pv + 0xc4, thrust);
        lq_sq(sp + 0xa0, a);
        lq_sq(sp + 0xb0, b);
    }
    const uint32_t offs = sp + 0xa0 + (static_cast<uint32_t>(lbu(pv + 0x60)) << 4);
    if (lbu(m + 0x20) == 2) {
        swc1(pv + 0xb4, lwc1(offs + 0));
        swc1(pv + 0xb8, lwc1(offs + 4));
        swc1(pv + 0xbc, lwc1(offs + 8));
    } else {
        const float k1 = u2f(0x3e19999au), k2 = u2f(0x3f4ccccdu);
        spring_to(pv + 0xb4, pv + 0xc8, lwc1(offs + 0), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
        spring_to(pv + 0xb8, pv + 0xcc, lwc1(offs + 4), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
        spring_to(pv + 0xbc, pv + 0xd0, lwc1(offs + 8), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
    }
    {
        const uint32_t mat = sp + 0xe0, cam = sp + 0xc0;
        swc1(ext + 4, lwc1(pv + 0xb8));
        swc1(ext + 8, lwc1(pv + 0xbc));
        swc1(ext + 0, lwc1(pv + 0xb4));
        sw(ext + 0xc, 0);
        euler_to_mat3_vu(M(mat), V(pv + 0x30));
        mat3_transform(V(goal), V(ext), M(mat));
        vec3_add(V(cam), V(m + 0x10), V(goal));
        uid5_set_pvar_80(cam);
        const uint32_t want = sp + 0xd0;
        swc1(want + 0, angle_add(fpu_mul(lwc1(pv + 0x30), 0.5f), fpu_mul(lwc1(pv + 0x6c), 0.03125f)));
        swc1(want + 4, angle_sub(angle_add(lwc1(pv + 0x34), u2f(0x3dc90fdbu)), fpu_mul(lwc1(pv + 0x70), fpu_mul(step60(), u2f(0x3c490fdbu)))));
        swc1(want + 8, lwc1(pv + 0x38));
        float a0;
        if (lbu(m + 0x20) == 2) {
            lq_sq(pv + 0x20, want);
            a0 = lwc1(pv + 0x20);
        } else {
            const float k1 = u2f(0x3e19999au), k2 = u2f(0x3f4ccccdu);
            spring_angle(pv + 0x20, pv + 0xa0, 0, lwc1(want + 0), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
            spring_angle(pv + 0x24, pv + 0xa4, 0, lwc1(want + 4), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
            spring_angle(pv + 0x28, pv + 0xa8, 0, lwc1(want + 8), fpu_mul(spring_k(), k1), fpu_mul(spring_k(), k2), step60());
            a0 = lwc1(pv + 0x20);
        }
        const uint32_t rot = sp + 0x70;
        swc1(rot + 0, a0);
        swc1(rot + 8, lwc1(pv + 0x28));
        swc1(rot + 4, lwc1(pv + 0x24));
        uid5_set_pvar_90(rot);
        if (lh(lwu(L13_ADDR(0x00167100)) + 0x86) != 5) {
            debug_printf_stub();  // (the original prints a note with the format L13:0x1f4450)
            camera_scripted_set(cam, rot, 0, 0, 0);
        }
    }
    {  // the field of view, by the thrust shown
        spring_to(pv + 0xb0, pv + 0xc0, fpu_add(fpu_mul(thrust, u2f(0x3eb2b8c0u)), u2f(0x3f91361eu)), u2f(0x3d4ccccdu), 1.0f, 1.0f);
        const float h = fpu_mul(lwc1(pv + 0xb0), 0.5f);
        const float s = sin_vu(h);
        swc1(L13_ADDR(0x0016cdf0), fpu_div(s, cos_vu(h)));
        projection_setup();
    }
    swc1(m + 0x40, angle_add(lwc1(pv + 0x30), fpu_mul(lwc1(pv + 0x6c), 0.5f)));
    swc1(m + 0x44, angle_add(lwc1(pv + 0x34), fpu_mul(-lwc1(pv + 0x70), 0.5f)));
    swc1(m + 0x48, angle_add(lwc1(pv + 0x38), fpu_mul(-lwc1(pv + 0x6c), 0.0f)));

    // a hit on the world or a moby (not of the classes 0x192, 0x4c2, 0x4c3, Qwark's 0x52 / 0x53, nor class 0)
    if (sphere_collide(m + 0x10, u2f(0x3f99999au), 0, m) != 0) {
        const uint32_t hm = coll_hit_moby();
        const int32_t cls = hm != 0 ? lh(hm + 0xa6) : -1;
        if (!(hm != 0 && (cls == 0 || cls == 0x192 || cls == 0x4c2 || cls == 0x4c3 || cls == 0x52 || cls == 0x53))) {
            V(m + 0x10) = coll_hit_pushed();
            const Vec4 n = coll_hit_normal();
            vec3_reflect(V(vel), V(vel), n);
            const float k = u2f(0x3dcccccdu);
            swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), fpu_mul(angle_sub(atan2_f(lwc1(vel + 0), lwc1(vel + 4)), lwc1(pv + 0x38)), fpu_mul(step60(), k))));
            const float up = -atan2_f(vec2_length(V(vel)), lwc1(vel + 8));
            swc1(pv + 0x34, angle_add(lwc1(pv + 0x34), fpu_mul(angle_sub(up, lwc1(pv + 0x34)), fpu_mul(step60(), k))));
            swc1(pv + 0x64, fpu_mul(lwc1(pv + 0x64), u2f(0x3f333333u)));
            if (coll_hit_moby() != 0) {
                if (lw(pv + 0xdc) == 0) {
                    const float left = fpu_sub(lwc1(0x0014094c), 5.0f);
                    swc1(0x0014094c, left);
                    if (left < 0.0f) {  // shot down by the crash
                        // (the original also sets the triangle's edge and the normal to length 1 in its frame, which nothing reads)
                        const uint32_t bounce = sp + 0x30;
                        vec3_reflect(V(bounce), V(vel), n);
                        vec3_set_length(V(bounce), V(bounce), fpu_add(dt(), dt()));
                        explosion_effect(m, bounce, 0, 0x1e, 0xa, 0x18, 6, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
                        sh(pv + 0x62, static_cast<uint32_t>(frame_scale_i(0xf0)));
                        sb(m + 0x20, 6);
                    }
                    sw(pv + 0xdc, static_cast<uint32_t>(frame_scale_i(0xa)));
                }
            } else {  // the world: shot down at once
                swc1(0x0014094c, 0.0f);
                explosion_effect(m, zero_v, 0, 0x1e, 0xa, 0x18, 6, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                sh(pv + 0x62, static_cast<uint32_t>(frame_scale_i(0xf0)));
                sb(m + 0x20, 6);
            }
        }
    }

    sub_001f93e0(L13_ADDR(0x002b97f8), m);  // (the HUD's draw callback)
    gml_2b9f58_l13(m, pv);
    const float yaw = angle_add(lwc1(pv + 0x28), fpu_div(-lwc1(pv + 0x6c), 7.0f));
    const float pitch = angle_add(lwc1(pv + 0x24), fpu_sub(-fpu_div(lwc1(pv + 0x70), 7.0f), u2f(0x3dcccccdu)));
    gml_2b9df8_l13(m, pv, yaw, -pitch);
}

// orig: L13:0x002bb068
void gml_class69_update_l13(uint32_t m) {
    if (lw(0x001413d4) == 0x72) return;
    const Frame f(192);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const bool fix10 = gameplay102_on(10) || region_core_moved();  // (1.02 Gameplay, fix 10: the PAL 2.00 disc's own code)
    sw(pv + 0x120, 0);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xdc)));

    // a hit while it flies: the HUD's health (0x14094c) down by the damage; a push and a turn, or shot down
    {
        const uint32_t st = lbu(m + 0x20);
        if (!(st == 6 || st == 7 || st == 1 || st == 2)) {
            const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x30001), 0);
            if (hit != 0 && lbu(m + 0x20) == 4) {
                const float health = lwc1(0x0014094c), dmg = lwc1(hit + 0x2c);
                const uint32_t dir = sp + 0x20;
                if (dmg < health) {
                    swc1(0x0014094c, fpu_sub(health, dmg));
                    const uint32_t by = lwu(hit + 0x20);
                    if (by != 0 && lhu(by + 0xa6) - 0x52u < 2u) sw(pv + 0x120, static_cast<uint32_t>(lh(by + 0xa6)));
                    if ((lwu(hit + 0x30) & 1u) != 0) {
                        swc1(L13_ADDR(0x001670e0), 1.0f);
                        sw(L13_ADDR(0x001670e8), static_cast<uint32_t>(frame_scale_i(4)));
                        const float k = u2f(0x3dcccccdu);
                        const float w = fpu_mul(lwc1(hit + 0x2c), u2f(0x3d4ccccdu));
                        vec_from_spherical_v(V(dir), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                        vec3_clamp_length_vu(V(hit + 0x10), V(hit + 0x10), fpu_mul(dt(), 3.0f));
                        vec3_add(V(m + 0x10), V(m + 0x10), V(hit + 0x10));
                        vec3_reflect(V(dir), V(dir), V(hit + 0x10));
                        swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), fpu_mul(fpu_mul(angle_sub(atan2_f(lwc1(dir + 0), lwc1(dir + 4)), lwc1(pv + 0x38)), fpu_mul(step60(), k)), w)));
                        const float up = atan2_f(vec2_length(V(dir)), lwc1(dir + 8));
                        swc1(pv + 0x34, angle_add(lwc1(pv + 0x34), fpu_mul(fpu_mul(angle_sub(up, lwc1(pv + 0x34)), fpu_mul(step60(), k)), w)));
                    }
                } else {  // shot down
                    vec_from_spherical_v(V(dir), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                    const uint32_t by = lwu(hit + 0x20);
                    if (by != 0 && lhu(by + 0xa6) - 0x52u < 2u) sw(pv + 0x120, static_cast<uint32_t>(lh(by + 0xa6) + 0x2710));
                    swc1(L13_ADDR(0x001670e0), u2f(0x3e4ccccdu));
                    sw(L13_ADDR(0x001670e8), static_cast<uint32_t>(frame_scale_i(5)));
                    sw(P + 0x15fc, 0);
                    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                    explosion_effect(m, dir, 0, 0xa, 3, 0x10, 6, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
                    sw(m + 0x94, 0);
                    sh(pv + 0x62, static_cast<uint32_t>(frame_scale_i(0xf0)));
                    sb(m + 0x20, 6);
                }
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if (lwu(P + 0x15f0) == m)  // the HUD's bar: the health in 200ths
        swc1(P + 0x1608, eer::cvt_w(fpu_mul(fpu_mul(lwc1(P + 0x15fc), 0.00390625f), 200.0f)));

    switch (lbu(m + 0x20)) {  // (the table L13:0x1f44e0)
    case 0: {  // (0x002bb3dc): set up
        sb(m + 0x20, 1);
        sb(pv + 0x60, 0);
        ship_view_start(m, pv);
        lq_sq(pv + 0x50, m + 0x40);
        lq_sq(pv + 0x40, m + 0x10);
        sw(pv + 0x7c, 0);
        const uint32_t pal = lwu(L13_ADDR(0x0015f420)) +
                             (static_cast<uint32_t>(lhu(L13_ADDR(0x0016d480) + ((lwu(pv + 0x104) + 0x28u) << 4) + 0xa)) << 4);
        const int32_t copied = lw(L13_ADDR(0x001613c4));
        sw(L13_ADDR(0x001613c0), pal);
        if (copied == 0) {  // (its palette kept once: 0x400 bytes)
            const uint32_t dst = L13_ADDR(0x001cbe70);
            for (uint32_t k = 0; k < 0x400u; ++k) sb(dst + k, lbu(pal + k));
        }
        sw(L13_ADDR(0x001613c4), 1);
        sw(pv + 0xe8, 1);
        sw(pv + 0xf8, 1);
        if (checkpoint_taken(m)) sb(m + 0x20, 9);
        ship_groups(pv, 0);
        ship_scenery(1);
        sw(pv + 0x128, 0xffffffffu);
        sw(pv + 0x138, static_cast<uint32_t>(frame_scale_i(0x23f)));
        break;
    }
    case 1: {  // (0x002bb5fc): on its pad, the prompt; the triangle (or its box) boards it
        bool go = false;
        if (checkpoint_taken(m)) {
            sb(m + 0x20, 9);
        } else if (lw(pv + 0x134) != -1 && point_in_box(0x0013f3d0, lw(pv + 0x134)) != 0) {
            const float f99 = u2f(0x3f7d70a4u);
            go = true;
            swc1(pv + 0x7c, f99);
            swc1(L13_ADDR(0x0015f3fc), f99);
        } else if (moby_sphere_gap(m, lwu(P + 0x2080)) < 2.0f && !gameplay102_talk_blocked()) {  // (1.02 Gameplay, fix 10)
            const int32_t shown = hud_message_request(7, TXT(0x53e4));
            if ((lwu(0x0013cae4) & 0x10u) != 0) go = shown != 0;
        }
        if (go) {
            if (fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: 0x32 at the press)
            ship_view_start(m, pv);
            sw(pv + 0xe8, 1);
            sw(pv + 0xf8, 1);
            sb(m + 0x20, 2);
            sw(pv + 0x128, 0xffffffffu);
        } else if (lwc1(pv + 0x7c) != 0.0f) {
            approach_f(pv + 0x7c, 0.0f, fpu_mul(dt(), 4.0f));
            swc1(L13_ADDR(0x0015f3fc), lwc1(pv + 0x7c));
        }
        floor_light_add(m, 1.5f);
        break;
    }
    case 2: {  // (0x002bb798): the fade in; then Ratchet aboard, the flight starts
        approach_f(pv + 0x7c, 1.0f, fpu_mul(dt(), 4.0f));
        const float fade = lwc1(pv + 0x7c);
        swc1(L13_ADDR(0x0015f3fc), fade);
        if (fade != 1.0f) break;
        if (lw(pv + 0x130) != -1) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x130));
            hero_place(box + 0x30, box + 0x70, 0x32, 1);
            const uint32_t box2 = lvd::box(lvd::BOXES, lw(pv + 0x130));
            lq_sq(m + 0x10, box2 + 0x30);
            lq_sq(m + 0x40, box2 + 0x70);
        }
        if (!fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: skipped when set at the press)
        camera_scripted_set(m + 0x10, m + 0x40, 0, 0, 0);
        sh(pv + 0x68, 0);
        sw(pv + 0x6c, 0);
        sw(pv + 0x70, 0);
        sw(pv + 0x84, 0);
        swc1(pv + 0x64, fpu_mul(step60(), u2f(0x3dcccccdu)));
        sw(pv + 0x80, 0);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(lwu(P + 0x2080) + 0x98, 0xffffffffu);
        lq_sq(pv + 0x20, m + 0x40);
        for (uint32_t o : {0xa0u, 0xa4u, 0xa8u, 0xc8u, 0xccu, 0xd0u}) sw(pv + o, 0);
        sw(P + 0x15f0, m);
        swc1(P + 0x15fc, 255.0f);
        sb(P + 0x15f6, 0xa);
        sb(P + 0x15f8, 0x14);
        sb(P + 0x160e, 1);
        sb(P + 0x15f7, 0x14);
        sh(P + 0x15f4, lhu(m + 0xa6));
        sw(pv + 0xf4, 0xff);
        sw(pv + 0xf0, 0);
        sb(P + 0x15fb, 3);
        sb(P + 0x15f9, 3);
        sb(P + 0x15fa, 3);
        sb(P + 0x160f, 0);
        swc1(P + 0x1600, 256.0f);
        swc1(P + 0x1604, 100.0f);
        vec_from_spherical_v(V(pv), fpu_mul(step60(), u2f(0x3dcccccdu)), lwc1(pv + 0x38), -lwc1(pv + 0x34));
        ship_groups(pv, 1);
        ship_scenery(0);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 3.0f));
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 0.25f));
        sb(P + 0x20a5, 1);
        if (lw(pv + 0x108) == -1) sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        sw(L13_ADDR(0x0015f608), 2);
        gml_2ba178_l13(m, pv);
        visibomb_203b18();
        music_track_request(2, 4);
        sb(m + 0x20, 4);
        break;
    }
    case 4: {  // (0x002bba48): flies
        if (lwc1(pv + 0x7c) != 0.0f) {
            approach_f(pv + 0x7c, 0.0f, fpu_mul(dt(), 4.0f));
            swc1(L13_ADDR(0x0015f3fc), lwc1(pv + 0x7c));
        }
        hud_message_force(6, 0);
        sw(L13_ADDR(0x0015f608), 2);
        if (!sound_playing(lw(pv + 0x108), m)) {  // (its engine's sound again when it stopped)
            sw(pv + 0x108, 0xffffffffu);
            sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
        gml_2ba178_l13(m, pv);
        if ((lbu(P + 0x160f) & 1u) != 0) sb(m + 0x20, 5);
        if (lw(pv + 0xe8) <= 0) {
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x4b0)));
            sb(m + 0x20, 8);
        }
        break;
    }
    case 5: {  // (0x002bbb38): back on the pad (or its exit box), Ratchet off
        const uint32_t hm = lwu(P + 0x2080);
        sb(P + 0x20a5, 0);
        sw(hm + 0x98, 0);
        hero_set_state(0, 1);
        hydro_2eac18(0);
        sub_002359f8(static_cast<uint32_t>(lh(P + 0x160c)), 0);
        lq_sq(m + 0x10, pv + 0x40);
        lq_sq(m + 0x40, pv + 0x50);
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        moby_update_transform_kept(m);
        ship_groups(pv, 0);
        ship_scenery(1);
        if (lw(pv + 0x12c) != -1) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x12c));
            hero_place(box + 0x30, box + 0x70, 0, 1);
        } else {
            hero_place(pv + 0x40, lvd::box(lvd::BOXES, lw(pv + 0xfc)) + 0x70, 0, 1);
        }
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        music_track_request(0, 5);
        const float f99 = u2f(0x3f7d70a4u);
        swc1(pv + 0x7c, f99);
        swc1(L13_ADDR(0x0015f3fc), f99);
        sb(m + 0x20, 1);
        break;
    }
    case 6: {  // (0x002bbd1c): shot down: the sounds stop
        sw(L13_ADDR(0x0015f608), 2);
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        sb(m + 0x20, 7);
        break;
    }
    case 7: {  // (0x002bbdbc): the loss's countdown, then the hero lost (no help message or dialogue running)
        sw(L13_ADDR(0x0015f608), 2);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x62))) == 0) break;
        if (lw(0x00151720) != 0 || lw(0x001516ec) != -1) break;
        hero_lost();
        hydro_2eac18(0);
        lq_sq(m + 0x10, pv + 0x40);
        lq_sq(m + 0x40, pv + 0x50);
        moby_update_transform_kept(m);
        sound_stop(pv + 0x108, m);
        sound_stop(pv + 0x10c, m);
        sb(m + 0x20, 0);
        break;
    }
    case 8: {  // (0x002bbebc): the fight won: to the landing box, Ratchet off, the checkpoint
        const uint32_t hm = lwu(P + 0x2080);
        sb(P + 0x20a5, 0);
        sw(hm + 0x98, 0);
        hero_set_state(0, 1);
        hydro_2eac18(0);
        if (lw(pv + 0x100) == -1) {
            debug_printf_stub();  // (the format L13:0x1f4470)
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
            debug_printf_stub();  // (the format L13:0x1f44a8)
        } else {
            ship_groups(pv, 0);
            ship_scenery(1);
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
    case 9: {  // (0x002bc0f0)
        moby_retire(m);
        return;
    }
    default:  // (3, 10 and beyond: nothing)
        break;
    }

    // within the space's bounds (15 .. 1008 on each axis); flying: the hero's place and angles its; the trails
    for (uint32_t o = 0x10; o <= 0x18; o += 4) {
        if (lwc1(m + o) < 15.0f) swc1(m + o, 15.0f);
        if (1008.0f < lwc1(m + o)) swc1(m + o, 1008.0f);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 4) {
        lq_sq(0x0013f3d0, m + 0x10);
        lq_sq(0x0013f3e0, pv + 0x30);
    }
    if (st == 3 || st == 4 || st == 8) gml_2b8590_l13(m, pv);
}

}  // namespace rac1
