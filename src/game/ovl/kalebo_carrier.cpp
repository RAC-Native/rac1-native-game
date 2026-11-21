// Kalebo III (level 16), class 1401: the carrier of class 933 (Oltanis's class 923 in oltanis_carrier.cpp is its sibling;
// the carrier's helpers are shared: shared_class933.h). Kalebo III's build differs from Oltanis's in two places, so it is
// its own function: a hit on the moby it holds (pvars +0xf4) knocks it back too, and its flight's speed is the level's
// (0x161d90) with no own speed (+0xf8) and no level test. Its kind (pvars +0xfc): 0 brings a class 933 moby, 1 comes and
// stays, 2 just stands (state 1, turning toward the hero). Set up (0) on its two paths (+0xd8 in, +0xdc out) and its box
// (+0xe0) it waits hidden (2) until the hero enters the box; then (spawning the moby it holds, kind 0) it flies in along
// the first path (4), hovers at its end (3) and, its animation done, either stays (kind 1: state 1) or lets the moby go
// (its state 1) and flies out along the second path (5), to wait hidden again at the first path's start (2). Shot (a hit
// of kind 0x210000 on it or on its moby, or the hero's touch with its animation not yet 1) it is knocked back and falls
// (6), dropping bolts, until it lands (a burst) or falls below 5. Its sparks while it flies; its colour pulse. Level 16
// only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class1401,
// the P16.5 checks).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class933.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l16.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;

constexpr uint32_t HERO = 0x0013f350u;

// the springs' rate and damping, the level's (0x161d94 / 0x161d98)
float spring_k() { return lwc1(L16_ADDR(0x00161d94)); }
float spring_d() { return lwc1(L16_ADDR(0x00161d98)); }

// the carrier's place (+0x10 .. +0x18) sprung toward the point at sp (its springs +0xe4 .. +0xec)
void spring_toward(uint32_t m, uint32_t pv, uint32_t sp) {
    spring_to(m + 0x10, pv + 0xe4, lwc1(sp), spring_k(), spring_d(), 0.0f);
    spring_to(m + 0x14, pv + 0xe8, lwc1(sp + 4), spring_k(), spring_d(), 0.0f);
    spring_to(m + 0x18, pv + 0xec, lwc1(sp + 8), spring_k(), spring_d(), 0.0f);
}
}  // namespace

// Along its path (the second, +0xdc, in state 5; else the first, +0xd8; its place +0xd0 / +0xd4) at the level's speed
// (0x161d90 a second), springing toward the point, turning toward its way (+0xf0) unless the way is within 0.01 on x or y.
// 1 at the path's end.
// orig: L16:0x002e3fa0
int32_t kal_carrier_fly_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x80);
    const uint32_t sp = f.sp;
    const float step = fpu_mul(lwc1(L16_ADDR(0x00161d90)), lwc1(0x0015ed6cu));
    int32_t seg = lw(pv + 0xd0);
    float frac = lwc1(pv + 0xd4);
    const int32_t end = path_advance_v(lvd::path(lw(pv + (lbu(m + 0x20) == 5 ? 0xdcu : 0xd8u))), V(sp), &seg, &frac, 0, step);
    sw(pv + 0xd0, static_cast<uint32_t>(seg));
    swc1(pv + 0xd4, frac);
    spring_toward(m, pv, sp);
    vec3_sub(V(sp + 0x10), V(sp), V(m + 0x10));
    if (u2f(0x3c23d70au) < abs_f(lwc1(sp + 0x10)) && u2f(0x3c23d70au) < abs_f(lwc1(sp + 0x14)))
        yaw_turn_damped(m, pv + 0xf0, atan2_f(lwc1(sp + 0x10), lwc1(sp + 0x14)), spring_k(), spring_d(), 0.0f);
    return end;
}

// orig: L16:0x002e37a0
void kal_class1401_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x80);
    // its animation 1 done (the hold anew): animation 4
    if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == lbu(m + 0x53) && lbu(m + 0x52) == 1) moby_anim_set(m, 4, 0, frame_scale_i(5));
    // the hero touching it: animation 1 (a flinch), unless it is in it
    if (lwu(HERO + 0x23c) == m && lbu(m + 0x53) != 1 && lbu(m + 0x52) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
    // shot, or its moby shot (0x002e3864): knocked back
    bool hit = sub_0025a420(m, NOT_ADDR(0x00210000u), 0) != 0;
    if (!hit) {
        const uint32_t held = lwu(pv + 0xf4);
        hit = held != 0 && sub_0025a420(held, NOT_ADDR(0x00210000u), 0) != 0;
    }
    if (hit && lbu(m + 0x20) != 6) {  // (0x002e3894)
        const float dt = lwc1(0x0015ed6cu);
        swc1(pv + 0x80, u2f(0x3c03126fu));
        sw(pv + 0x94, 1);
        swc1(pv + 0x8c, fpu_mul(dt, 12.0f));
        swc1(pv + 0x84, u2f(0x3a03126fu));
        swc1(pv + 0x88, fpu_mul(dt, 24.0f));
        sb(pv + 0xad, 0);
        const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
        move_state_start(m, pv + 0x70, 1, 1, 0, away);
        sb(pv + 0x67, 0x78);
        moby_drop_bolts(m, 0, -1);
        sb(m + 0x20, 6);
    }
    const uint32_t st = lbu(m + 0x20);
    sb(m + 0xa4, 0xff);
    bool sparks = false;
    switch (st < 7 ? st : 7u) {  // (the table 0x1e87e0)
    case 0: {  // (0x002e3978) set up
        const int32_t kind = lw(pv + 0xfc);
        if (kind == 2) {  // just stands
            sb(m + 0x20, 1);
            sw(pv + 0xf0, 0);
            moby_set_anim(m, 4, 0);
            sw(pv + 0xf4, 0);
            break;
        }
        // (the original prints the uid where a path or the box is missing, or a path has no points)
        if (lw(pv + 0xd8) == -1 || lw(lvd::path(lw(pv + 0xd8))) == 0) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        if (kind != 1 && (lw(pv + 0xdc) == -1 || lw(lvd::path(lw(pv + 0xdc))) == 0)) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        if (lw(pv + 0xe0) == -1) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        class933_carrier_start(m);
        sb(m + 0x20, 2);
        sb(m + 0x30, 0x80);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        sw(m + 0x94, 0);
        sw(pv + 0xf4, 0);
        break;
    }
    case 1:  // (0x002e3aac) stands, turning toward the hero
        yaw_turn_damped(m, pv + 0xf0, atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14))),
                        u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f);
        sparks = true;
        break;
    case 2:  // (0x002e3b00) hidden until the hero enters its box
        if (point_in_box(0x0013f3d0u, lw(pv + 0xe0)) == 0) break;
        if (lw(pv + 0xfc) == 1) {
            moby_set_anim(m, 0, 0);
        } else {
            sw(pv + 0xf4, class933_spawn(m, m + 0x10));
            class933_carrier_hold(m);
            moby_set_anim(m, 2, 0);
        }
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sb(m + 0x20, 4);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    case 3: {  // (0x002e3bc8) at the first path's end
        const uint32_t path = lvd::path(lw(pv + 0xd8));
        if (lw(pv + 0xfc) == 0) class933_carrier_hold(m);
        lq_sq(f.sp, path + (static_cast<uint32_t>(lw(path)) << 4));  // (its last point)
        spring_toward(m, pv, f.sp);
        sparks = true;
        if ((lbu(m + 0x70) & 2) == 0 || lbu(m + 0x52) != lbu(m + 0x53)) break;
        if (lw(pv + 0xfc) == 1) {  // stays
            sb(m + 0x20, 1);
            swc1(pv + 0xf0, 0.0f);
            moby_set_anim(m, 4, 0);
            break;
        }
        if (lwu(pv + 0xf4) != 0) {  // its moby let go
            moby_state_hook(lwu(pv + 0xf4), 1);
            sw(pv + 0xf4, 0);
        }
        sb(m + 0x20, 5);
        moby_anim_set(m, 0, 0, 0x14);
        swc1(pv + 0xd4, 0.0f);
        sw(pv + 0xd0, 0);
        break;
    }
    case 4:  // (0x002e3b84) flying in
        if (lw(pv + 0xfc) == 0) class933_carrier_hold(m);
        if (kal_carrier_fly_l16(m) != 0) {
            sb(m + 0x20, 3);
            moby_anim_set(m, 3, 0, 5);
        }
        sparks = true;
        break;
    case 5:  // (0x002e3ce8) flying out: hidden again at the first path's start
        if (kal_carrier_fly_l16(m) == 0) {
            sparks = true;
            break;
        }
        sb(m + 0x20, 2);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        sw(pv + 0xd4, 0);
        sw(pv + 0xd0, 0);
        swc1(pv + 0xf0, 0.0f);
        swc1(pv + 0xec, 0.0f);
        swc1(pv + 0xe8, 0.0f);
        swc1(pv + 0xe4, 0.0f);
        {
            const uint32_t path = lvd::path(lw(pv + 0xd8));
            lq_sq(m + 0x10, path + 0x10);
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
        }
        break;
    case 6:  // (0x002e3d84) falling: a burst where it lands, or gone below 5
        if ((enemy_physics(m, pv + 0x70) & 3u) != 0) {
            explosion_burst(m, m + 0x10, -1, 1.0f, 13.0f);
            moby_retire(m);
            return;
        }
        if (lwc1(m + 0x18) < 5.0f) {
            moby_retire(m);
            return;
        }
        break;
    default:  // (7 and past: the table's bound, 0x002e3df0)
        break;
    }
    if (sparks) class933_carrier_sparks(m);
    color_pulse_update(m, pv + 0x60);
}

}  // namespace rac1
