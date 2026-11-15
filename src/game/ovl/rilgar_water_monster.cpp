// Rilgar's (level 5) water monster (class 35): the creature that swims round Blackwater City's deep water along a path
// and eats Ratchet when he swims too far out. Ported by hand from Rilgar's own code (its three functions, L5:0x2d1688,
// 0x2d1e30, 0x2d1fd8), the original's order of calls and float operations kept (the random sequence above all).
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x2f0 the height of the water he is in,
                                        // +0x2084 / +0x208c his state and his previous one: 0x12 swimming)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
// the height of the city's water (the level's word 0x1612e8: the rising water moves it)
float water_height() { return lwc1(L5_ADDR(0x001612e8u)); }
// how far the monster's origin sits below its swimming height (the level's word 0x161388, 1.0)
float depth() { return lwc1(L5_ADDR(0x00161388u)); }
}  // namespace

// The monster's wake while it swims: every 3 to 5 frames (its timer pvar +0x84) two ripples (particle 0x2e, riding the
// water's height 0x1612e8) beside it, spread along its side axis (+0xd0), 0.05 above the surface, turning opposite ways.
// orig: L5:0x002d1e30
void ril_2d1e30_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x84))) == 0) return;
    const uint32_t sp = ee_stack_alloc(0xa0);
    for (int32_t i = 0; i < 2; ++i) {
        // (the original turns its yaw half round and adds a random spread of +-45 degrees, then never uses the angle:
        // only the random draw is kept)
        rand_range_f(u2f(0xbf490fd8u), u2f(0x3f490fd8u));
        vec3_scale(V(sp), V(m + 0xd0), rand_range_f(-0.5f, 0.5f));
        vec3_add(V(sp), V(sp), V(m + 0x10));
        swc1(sp + 8, fpu_add(water_height(), u2f(0x3d4ccccdu)));  // (0.05)
        const float size = rand_range_f(u2f(0x3f333333u), 1.0f);  // (0.7 .. 1)
        const uint32_t p = spawn_particle_2e(sp, L5_ADDR(0x0015f580u), L5_ADDR(0x001612e8u), size, i != 0 ? -2.0f : 2.0f);
        if (p != 0) sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xf)));
        sw(pv + 0x84, static_cast<uint32_t>(float_to_int(rand_range_f(3.0f, 5.0f))));
    }
    ee_stack_free(0xa0);
}

// The splash at `pos` (the hero going under, the monster breaking the surface): a moby 307 (scale 3, its +0x23 0x70),
// 16 drops (particle 0x23 of kind 0 or 1, 90 to 120 frames) thrown out and up, and 16 ripples (particle 0x2e) around it
// on the surface, 30 to 60 frames.
// orig: L5:0x002d1fd8
void ril_2d1fd8_l5(uint32_t pos) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t splash = spawn_moby_307(pos, 3.0f);
    if (splash != 0) sb(splash + 0x23, 0x70);
    for (int32_t i = 0; i < 16; ++i) {
        const float a = rand_angle();
        const float r = rand_range_f(fpu_mul(dt(), 0.0f), fpu_mul(dt(), 3.0f));
        swc1(sp, fpu_mul(cos_vu(a), r));
        swc1(sp + 4, fpu_mul(sin_vu(a), r));
        swc1(sp + 8, rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.5f)));
        const uint32_t life = static_cast<uint32_t>(rand_range(0x5a, 0x78));
        const int32_t kind = rand_mod(2);
        spawn_particle_23(pos, sp, kind, life);
    }
    for (int32_t i = 0; i < 16; ++i) {
        rand_sphere_point(sp, 1.0f, 1.0f);
        vec3_add(V(sp), V(sp), V(pos));
        swc1(sp + 8, fpu_add(water_height(), u2f(0x3d4ccccdu)));  // (0.05)
        const float size = rand_range_f(u2f(0x3f333333u), 1.0f);  // (0.7 .. 1)
        const uint32_t p = spawn_particle_2e(sp, L5_ADDR(0x0015f580u), L5_ADDR(0x001612e8u), size, i != 0 ? -2.0f : 2.0f);
        if (p != 0) sh(p + 0xa, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(30.0f, 60.0f)))));
    }
    ee_stack_free(0x90);
}

// Class 35, the water monster. pvars: its path +0x60 (-1: none, the moby goes), the waiting monster +0x64 (non-zero: it
// waits under the hero's water instead of swimming), the path point it heads for +0x68 and the timer to the next one
// +0x6c, its yaw rate +0x70 (the pitch rate while it lunges), the surface it waits under +0x74, its rise speed +0x78,
// the wait before it may strike +0x7c, its swimming height +0x80, the wake's timer +0x84.
// States: 0 set up; 1 swims from point to point of its path (a random jump ahead every 10 seconds), deeper near a
// swimming hero; 2 waits for a hero swimming at its surface (0.33): then faces the camera's way under him and lunges
// (camera 0x167080 +0x140 / +0x150); 3 rises with the hero, swallows him (his state 0x72) and he is lost when it falls
// back 8 below its surface.
// orig: L5:0x002d1688
void ril_class35_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t state = lbu(m + 0x20);
    auto anim_to = [m](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    switch (state) {
    case 0: {  // (0x002d16f4) set up
        if (lw(pv + 0x64) != 0) {  // the waiting kind: hidden at its surface, its first wait one half second
            const float z = lwc1(m + 0x18);
            sb(m + 0x20, 2);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sb(m + 0x30, 0xff);
            swc1(pv + 0x74, z);
            sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(0x1e)));
            break;
        }
        if (lw(pv + 0x60) == -1) {
            moby_retire(m);
            break;
        }
        sw(pv + 0x68, static_cast<uint32_t>(rand_mod(lw(lvd::path(lw(pv + 0x60))))));
        sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(0x258)));
        anim_to(1, frame_scale_i(0xa));
        const float z = fpu_sub(lwc1(lvd::path(lw(pv + 0x60)) + 0x18), depth());  // (its path's first point, lowered)
        swc1(m + 0x18, z);
        swc1(pv + 0x80, z);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x002d17c4) swims along its path
        const uint32_t path = lvd::path(lw(pv + 0x60));
        lq_sq(sp, path + 0x10 + (lwu(pv + 0x68) << 4));
        const float pi = u2f(0x40490fdbu);
        const float turn = fpu_mul(dt2(), pi);
        yaw_seek(m + 0x48, pv + 0x70, atan2_f(fpu_sub(lwc1(sp), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 4), lwc1(m + 0x14))), turn, turn,
                 fpu_mul(dt(), pi));
        vec3_set_length(V(sp + 0x10), V(m + 0xc0), fpu_mul(dt(), 3.0f));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x10));
        // the next point: at the point, or every 10 seconds: a random number of points ahead
        if (vec3_distance(V(m + 0x10), V(sp)) < 3.0f || timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x6c))) != 0) {
            const int32_t ahead = rand_mod(lw(lvd::path(lw(pv + 0x60))));
            sw(pv + 0x68, static_cast<uint32_t>(ee_rem(lw(pv + 0x68) + ahead, lw(lvd::path(lw(pv + 0x60))))));
            sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(0x258)));
        }
        // its height: 2 below the surface while a swimming hero is within 8, else 0.5 below and its wake
        float target;
        if ((lw(kHero + 0x208c) == 0x12 || lw(kHero + 0x2084) == 0x12) && vec2_distance(V(m + 0x10), V(kHero + 0x80)) < 8.0f) {
            target = fpu_sub(fpu_sub(water_height(), depth()), 2.0f);
        } else {
            target = fpu_sub(fpu_sub(water_height(), depth()), 0.5f);
            ril_2d1e30_l5(m);
        }
        const float rate = fpu_add(dt2(), dt2());
        seek_1d(m + 0x18, pv + 0x78, target, rate, rate, fpu_add(dt(), dt()));
        break;
    }
    case 2: {  // (0x002d19c4) waits under the surface
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c)));
        if (abs_f(fpu_sub(lwc1(kHero + 0x2f0), lwc1(pv + 0x74))) < u2f(0x3ea8f5c3u) && lw(kHero + 0x208c) == 0x12) {  // (0.33)
            if (lw(pv + 0x7c) != 0) break;
            // the lunge: pitched up, facing the camera's yaw turned a quarter, right under the hero
            swc1(m + 0x44, u2f(0xbfc6d3f2u));  // (-1.5533)
            swc1(m + 0x48, angle_add(lwc1(L5_ADDR(0x001671d8u)), u2f(0x3fc90fdbu)));
            euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
            lq_sq(m + 0x10, kHero + 0x80);
            vec3_set_length(V(sp), V(m + 0xe0), depth());
            vec3_sub(V(m + 0x10), V(m + 0x10), V(sp));
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 2.0f));
            swc1(pv + 0x78, fpu_mul(dt(), 10.0f));
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            anim_to(2, frame_scale_i(5));
            sb(m + 0xbc, 0);
            sb(m + 0x20, 3);
            const uint32_t cam = L5_ADDR(0x00167080u);
            camera_scripted_set(cam + 0x140, cam + 0x150, 0, 0, 0);
            break;
        }
        if (lw(kHero + 0x2084) == 0x12) break;
        // the hero out of the water: the next wait 10 seconds while he is over the path's area, else 3
        const uint32_t path = lvd::path(lw(pv + 0x60));
        const int32_t inside = point_in_polygon(kHero + 0x80, path + 0x10, lw(path));
        sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(inside != 0 ? 0x258 : 0xb4)));
        break;
    }
    case 3: {  // (0x002d1b68) the lunge: rises, then falls back
        const float hero_water = lwc1(kHero + 0x2f0);
        if (hero_water < lwc1(m + 0x18) || lwc1(pv + 0x78) < 0.0f) {  // above the surface or falling: the hero in its mouth
            if (lw(kHero + 0x2084) != 0x72) {
                lq_sq(sp, kHero + 0x80);
                swc1(sp + 8, hero_water);
                ril_2d1fd8_l5(sp);
                hero_place(kHero + 0x80, kHero + 0x90, 0x72, 0);
            } else {
                lq_sq(kHero + 0x80, m + 0x10);
            }
            sb(kHero + 0x20a5, 1);
            swc1(kHero + 0x88, fpu_add(lwc1(kHero + 0x88), 20.0f));
        }
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x78)));
        const float speed = fpu_sub(lwc1(pv + 0x78), fpu_mul(dt2(), 10.0f));
        swc1(pv + 0x78, speed);
        if (speed < fpu_add(dt(), dt())) {  // near the top: pitches over, turning about its mouth
            vec3_set_length(V(sp), V(m + 0xe0), depth());
            yaw_seek(m + 0x44, pv + 0x70, u2f(0x3fc6d3f2u), fpu_mul(dt2(), u2f(0x41490fdbu)), fpu_mul(dt2(), u2f(0x41490fdbu)),
                     fpu_mul(dt(), u2f(0x41c90fdbu)));  // (1.5533; 4 pi, 8 pi)
            euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
            vec3_set_length(V(sp + 0x10), V(m + 0xe0), depth());
            vec3_sub(V(sp + 0x20), V(sp + 0x10), V(sp));
            vec3_sub(V(m + 0x10), V(m + 0x10), V(sp + 0x20));
        }
        if (lwc1(pv + 0x78) < 0.0f && lwc1(m + 0x18) < lwc1(kHero + 0x2f0) && lbu(m + 0xbc) == 0) {  // back under: the splash, once
            sb(m + 0xbc, 1);
            moby_sound_start(0, 0, m);
            vec3_set_length(V(sp), V(m + 0xe0), depth());
            vec3_add(V(sp), V(sp), V(m + 0x10));
            swc1(sp + 8, lwc1(kHero + 0x2f0));
            ril_2d1fd8_l5(sp);
        }
        if (lwc1(pv + 0x78) < 0.0f && lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x74), 8.0f)) hero_lost();
        if ((lbu(m + 0x70) & 2) != 0) anim_to(1, frame_scale_i(0xa));
        break;
    }
    default:
        break;
    }
    ee_stack_free(0xb0);
}

}  // namespace rac1
