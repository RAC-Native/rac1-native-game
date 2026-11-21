// Aridia's own moby classes: the Constructorbot and its hits, hand-ported from Aridia's overlay (`orig: L2:`). Aridia-only
// data is L2_ADDR; the engine's lists go through OV() and the level-data facade; the base ELF's (the hero 0x13f350, the
// frame time 0x15ed6c) are bare. The update and its helper are declared in aridia_enemies.h, the update registered in
// aridia_mobys.cpp.
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flame_spray.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)

float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // (its square: the accelerations' scale)

// the heading from the moby to the moby t, as the original computes it
float heading_to(uint32_t m, uint32_t t) {
    return atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14)));
}

// the animation set `set` from `frame` unless it is playing, blended over `frames` (the value given as it is)
void anim_raw(uint32_t m, uint32_t set, uint32_t frame, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, static_cast<int32_t>(frame), frames);
}

// the state `state` and the animation set `set` (blended over `frames` 60 Hz frames)
void go(uint32_t m, uint32_t state, uint32_t set, int32_t frames) {
    sb(m + 0x20, state);
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the centre (+0x30) of the level box the bot works at (pvar +0x188)
uint32_t work_place(uint32_t pv) { return lvd::box(lvd::BOXES, lw(pv + 0x188)) + 0x30; }
}  // namespace

// The Constructorbot's hits (mask 0x330000; a hit by a class-0x264 moby is not one): unless it is stunned (the hit's
// state 1) or dying (0x11), the health +0x20 down and a pulse (+0x60: red 0x64 for less than 1, else 0x78 and knocked
// back, state 0x10, the move record +0x70 set 0xb; at 0: 0xfa, thrown, state 0x11, set 0xe); then its target (+0x220):
// kept while it is a live moby of class 0x10e, 0xcb or 0 and the bot is in state 0xc, else what target_select_any finds
// within 14, else the hero.
// orig: L2:0x002d85b8
void constructorbot_hits_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t found = sp, hit_state = sp + 0x50, hit_dmg = sp + 0x54;
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    const uint32_t counted = (hit != 0 && lh(lwu(hit + 0x20) + 0xa6) != 0x264) ? hit : 0;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, counted, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x11) {
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg)));
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(u2f(0x4419999au))));
        swc1(pv + 0x98, u2f(0x3f19999au));
        // the knock: its speeds (the level's 0x161a64 ..), its yaw away from the hero, the move set
        auto knock = [m, pv](uint32_t speed, uint32_t up, uint32_t frames, uint32_t set) {
            swc1(pv + 0x80, fpu_mul(lwc1(L2_ADDR(0x00161a84)), dt2()));
            swc1(pv + 0x84, fpu_mul(lwc1(up), dt2()));
            swc1(pv + 0x88, fpu_mul(lwc1(speed), dt()));
            swc1(pv + 0x8c, fpu_mul(lwc1(speed + 4), dt()));
            sw(pv + 0x94, frames);
            swc1(pv + 0xbc, fpu_mul(lwc1(L2_ADDR(0x00161a9c)), dt()));
            sb(pv + 0xad, 0);
            const float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
            move_state_start(m, pv + 0x70, set, 1, 0, yaw);
            swc1(pv + 0xc0, 6.5f);
        };
        if (0.0f < lwc1(pv + 0x20)) {
            if (lwc1(hit_dmg) < 1.0f) {
                sb(pv + 0x67, 0x64);
            } else {
                sb(pv + 0x67, 0x78);
                swc1(pv + 0xa0, 0.5f);
                knock(L2_ADDR(0x00161a6c), L2_ADDR(0x00161a78), 0x19, 0xb);
                swc1(pv + 0xc4, 11.0f);
                sb(m + 0x20, 0x10);
            }
        } else {
            knock(L2_ADDR(0x00161a64), L2_ADDR(0x00161a74), 9, 0xe);
            swc1(pv + 0xc4, 13.0f);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0xfa);
            sb(m + 0x20, 0x11);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    bool keep = false;
    const uint32_t t = lwu(pv + 0x220);
    if (t != 0 && static_cast<int8_t>(lbu(t + 0x20)) >= 0) {
        const int32_t cls = lh(t + 0xa6);
        if ((cls == 0x10e || cls == 0xcb || cls == 0) && lbu(m + 0x20) == 0xc) keep = true;
    }
    if (!keep) {
        target_select_any(m, found, 14.0f);
        sw(pv + 0x220, lwu(found + 0x40));
    }
    if (lwu(pv + 0x220) == 0) sw(pv + 0x220, lwu(0x001413d0));
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0xc0);
}

// The Constructorbot: a builder robot that works at its level box (pvar +0x188) and burns what comes near it with its
// flame (the spray record +0x1f0). After its hits and target (constructorbot_hits_l2), by state (the original's jump
// table at L2:0x1fa9c0, 18 rows): 0 set up (by its mode +0x194: 1 idle (0xa), 2 walk (8), 3 (9), else to its work 7),
// 2 / 3 rises to its work place, 4 lands, 5 / 6 walk (doomlet_259028, the walker record +0xd0), 7 at work (its target
// near: turn to it and get ready, 0xb; else wander about the place), 8 / 9 / 10 watch for the hero (in the box +0x18c,
// or within +0x184 of the place: up, 2), 11 aims, 12 flames, 13 after the flame, 16 knocked (landed: 7), 17 dies
// (the bolts, the explosion and five debris); 1, 14, 15 nothing. Its floor band every frame but the last.
// orig: L2:0x002d7748
void constructorbot_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    joint_node_scale(m, 1, pv + 0x1a0, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L2_ADDR(0x001673c0))) < 30.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }
    constructorbot_hits_l2(m);
    const uint32_t state = lbu(m + 0x20);
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t point = sp + 0x20, dir = sp + 0x30, burn = sp + 0x40;
    const float k_turn = u2f(0x3ca3d70au), k_damp = u2f(0x3e99999au), k_max = u2f(0x3dcccccdu);  // (0.02, 0.3, 0.1)
    auto run = [&]() -> bool {  // false: retired (no floor band)
        switch (state < 18 ? state : 1) {  // (the original's jump table at L2:0x1fa9c0)
        case 0: {  // (0x002d7808) set up
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            sb(pv + 0x58, 0xf);
            swc1(pv + 0x30, 1.0f);
            sb(pv + 0x5c, 1);
            sb(pv + 0x5a, 8);
            sb(pv + 0x29, 0);
            sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));
            walker_record_init_l1(pv + 0xd0);
            sb(pv + 0x28, 2);
            swc1(pv + 0x10c, u2f(0x3cf5c28fu));
            swc1(pv + 0x110, u2f(0x3e99999au));
            swc1(pv + 0x114, 0.25f);
            swc1(pv + 0xf4, fpu_mul(dt(), 4.0f));
            swc1(pv + 0xd8, 2.0f);
            swc1(pv + 0x20, 3.0f);
            swc1(pv + 0xdc, 2.0f);
            vec_clear(V(pv + 0x40));
            flame_spray_clear(pv + 0x1f0);
            switch (lw(pv + 0x194)) {
            case 1:
                anim_raw(m, 0, 0, 0);
                sb(m + 0x20, 0xa);
                break;
            case 2:
                anim_raw(m, 2, 0, 0);
                sb(m + 0x20, 8);
                break;
            case 3:
                anim_raw(m, 0xf, 0, 0);
                sb(m + 0x20, 9);
                break;
            default:
                anim_raw(m, 0, 0, 0);
                sb(m + 0x20, 7);
                break;
            }
            return true;
        }
        case 2:  // (0x002d79a4) its animation played out: up (3)
            if ((lbu(m + 0x70) & 2) == 0) return true;
            sb(m + 0x20, 3);
            anim_raw(m, 4, 0, 0);
            return true;
        case 3: {  // (0x002d79e0) to its work place (6 a frame at most), rising to the floor there: 4
            yaw_turn_damped(m, pv + 0x214, heading_to(m, lwu(pv + 0x220)), k_turn, k_damp, fpu_mul(dt(), u2f(0x3fc90fdbu)));
            vec3_sub(V(sp + 0x20), V(work_place(pv)), V(m + 0x10));
            sw(sp + 0x28, 0);
            vec3_clamp_length(sp + 0x20, fpu_mul(dt(), 6.0f));
            vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x20));
            const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            approach_f(pv + 0x48, fpu_mul(dt(), 40.0f), fpu_mul(lwc1(L2_ADDR(0x00161a84)), dt2()));
            approach_f(m + 0x18, floor, lwc1(pv + 0x48));
            if (!(lwc1(m + 0x18) == floor)) return true;
            sb(m + 0x20, 4);
            if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 4, frame_scale_i(0xa));
            return true;
        }
        case 4:  // (0x002d7b34) landed: walking (5)
            if ((lbu(m + 0x70) & 2) == 0) return true;
            go(m, 5, 6, 6);
            return true;
        case 5:  // (0x002d7b6c) walking to its work place: there, at work (7)
            if (doomlet_259028(m, pv + 0xd0, work_place(pv), pv + 0x40) != 4) return true;
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x1e));
            sb(m + 0x20, 7);
            return true;
        case 6: {  // (0x002d7bb4) walking to its spot (+0x1e0): there, at work (7); its target near the place: 5
            if ((doomlet_259028(m, pv + 0xd0, pv + 0x1e0, pv + 0x40) & 6) != 0) {
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
                sb(m + 0x20, 7);
                return true;
            }
            const uint32_t t = lwu(pv + 0x220);
            if (!(vec3_distance(V(t + 0x10), V(work_place(pv))) < lwc1(pv + 0x180))) return true;
            if (!(abs_f(fpu_sub(lwc1(lwu(pv + 0x220) + 0x18), lwc1(m + 0x18))) < 4.0f)) return true;
            go(m, 5, 6, 0xa);
            return true;
        }
        case 7: {  // (0x002d7c98) at work
            const uint32_t t = lwu(pv + 0x220);
            if (vec3_distance(V(t + 0x10), V(work_place(pv))) < lwc1(pv + 0x180) &&
                abs_f(fpu_sub(lwc1(lwu(pv + 0x220) + 0x18), lwc1(m + 0x18))) < 4.0f) {
                if (!(vec3_distance(V(m + 0x10), V(work_place(pv))) < 1.0f)) {  // off its place: back to it (5)
                    go(m, 5, 6, 0xa);
                    return true;
                }
                const float yaw = heading_to(m, lwu(pv + 0x220));
                yaw_turn_damped(m, pv + 0x214, yaw, k_turn, k_damp, k_max);
                if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3db2b8c2u)) go(m, 0xb, 8, 6);  // facing it: aim (0xb)
                return true;
            }
            if ((lbu(m + 0x70) & 2) == 0) return true;
            // a new spot 1 .. 2 from the place, a random way (6)
            const float a = rand_angle();
            const float c = cos_vu(a);
            swc1(pv + 0x1e0, fpu_mul(c, rand_range_f(1.0f, 2.0f)));
            const float s = sin_vu(a);
            const float r = rand_range_f(1.0f, 2.0f);
            sw(pv + 0x1e8, 0);
            swc1(pv + 0x1e4, fpu_mul(s, r));
            vec3_add(V(pv + 0x1e0), V(pv + 0x1e0), V(work_place(pv)));
            go(m, 6, 6, 0xa);
            return true;
        }
        case 8:
        case 9:  // (0x002d7ecc) watching: sparks from its joint 0 every fourth frame on screen
            if ((lw(L2_ADDR(0x0015f5cc)) & 3) == 0 && lbu(m + 0x31) != 0) {
                const float speed = fpu_mul(rand_range_f(0.0f, lwc1(L2_ADDR(0x00161a98))), dt());
                const float a = rand_angle();
                swc1(dir, fpu_mul(cos_vu(a), speed));
                swc1(dir + 8, 0.0f);
                swc1(dir + 4, fpu_mul(sin_vu(a), speed));
                moby_chain_point_v(m, 0, V(point));
                const int32_t lo = frame_scale_i(lw(L2_ADDR(0x00161a90)));
                const int32_t hi = frame_scale_i(lw(L2_ADDR(0x00161a94)));
                const int32_t n = rand_range(lo, hi);
                const uint32_t c = lwu(L2_ADDR(0x00161a88));
                spawn_particle_15_v(V(point), V(dir), c, c, static_cast<uint32_t>(n), 1, 40000.0f);
            }
            [[fallthrough]];
        case 10: {  // (0x002d7f8c) the hero near (in its box +0x18c, or within +0x184 of its place): 8 / 9 up (2), 10 walks (5)
            const int32_t box = lw(pv + 0x18c);
            bool near;
            if (box >= 0) near = point_in_box(HERO + 0x2a0, box) > 0;
            else near = vec3_distance(V(HERO + 0x80), V(work_place(pv))) < lwc1(pv + 0x184);
            if (!near) return true;
            const uint32_t s = lbu(m + 0x20);
            if (s == 8 || s == 9) {
                const uint32_t set = s == 8 ? 4u : 0x10u;
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0xa));
                sb(m + 0x20, 2);
                return true;
            }
            go(m, 5, 6, 6);
            return true;
        }
        case 11: {  // (0x002d8088) aiming at the target: flame (0xc) when the animation has played out
            swc1(m + 0x58, 2.0f);
            yaw_turn_damped(m, pv + 0x214, heading_to(m, lwu(pv + 0x220)), k_turn, k_damp, k_max);
            if ((lbu(m + 0x70) & 2) == 0) return true;
            swc1(m + 0x58, 1.0f);
            go(m, 0xc, 9, 6);
            return true;
        }
        case 12: {  // (0x002d812c) the flame from its joint 0 (the spray +0x1f0, its reach 0x161a80), burning what it meets
            swc1(m + 0x58, 0.5f);
            moby_chain_point_v(m, 0, V(point));
            const float yaw = atan2_f(fpu_sub(lwc1(point), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14)));
            swc1(dir, cos_vu(yaw));
            swc1(dir + 4, sin_vu(yaw));  // (the original works the same heading out a second time for it)
            swc1(dir + 8, u2f(0xbdcccccdu));
            flame_spray(pv + 0x1f0, point, dir, lwc1(L2_ADDR(0x00161a80)));
            swc1(dir + 0xc, u2f(0x45afdf66u));
            swc1(dir + 8, 1.0f);
            sub_00259888(1.0f, burn, static_cast<int32_t>(m), 0x10001, dir);
            sb(burn + 0x18, 5);
            sh(burn + 0x1a, lhu(m + 0xa6));
            sb(burn + 0x19, 1);
            flame_spray_burn(pv + 0x1f0, m, burn);
            if ((lbu(m + 0x70) & 2) == 0) return true;
            swc1(m + 0x58, 1.0f);
            sb(m + 0x20, 0xd);
            if ((rand_mod(0xff) & 1) != 0) {
                if (lbu(m + 0x53) != 0xa) moby_anim_set(m, 0xa, 0, frame_scale_i(6));
            } else {
                if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0, frame_scale_i(6));
            }
            return true;
        }
        case 13: {  // (0x002d82a0) after the flame: the target still near and level (3): again (0xc), else at work (7)
            yaw_turn_damped(m, pv + 0x214, heading_to(m, lwu(pv + 0x220)), k_turn, k_damp, k_max);
            if ((lbu(m + 0x70) & 2) == 0) return true;
            if (vec2_distance(V(m + 0x10), V(lwu(pv + 0x220) + 0x10)) < fpu_add(lwc1(pv + 0x180), 1.0f) &&
                abs_f(fpu_sub(lwc1(lwu(pv + 0x220) + 0x18), lwc1(m + 0x18))) < 3.0f) {
                go(m, 0xc, 9, 6);
                return true;
            }
            go(m, 7, 0, 6);
            return true;
        }
        case 16:  // (0x002d838c) knocked back (the move record +0x70): on the ground, at work (7)
            if ((enemy_physics(m, pv + 0x70) & 0x40) == 0) return true;
            go(m, 7, 0, 6);
            return true;
        case 17: {  // (0x002d83d8) thrown, dying: at the events 0x140 its bolts, the explosion, five debris, gone
            if ((enemy_physics(m, pv + 0x70) & 0x140) == 0) return true;
            lq_sq(point, m + 0x10);
            swc1(point + 8, fpu_add(lwc1(point + 8), 1.0f));
            moby_drop_bolts(m, 0, -1);
            explosion_effect(m, pv + 0x40, point, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
            const uint32_t z = L2_ADDR(0x0015f580);  // (a zero vector)
            for (const uint32_t cls : {0x6d7u, 0x6d8u, 0x6d9u, 0x6e9u, 0x782u}) debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
            moby_retire(m);
            return false;
        }
        default:  // 1, 14, 15 and past the table (0x002d857c): nothing
            return true;
        }
    };
    if (run()) moby_floor_band(m);
    ee_stack_free(0x100);
}

}  // namespace rac1
