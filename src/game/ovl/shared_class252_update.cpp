// Class 252's update and its two helpers: one port of Batalia's (L8) and Oltanis's (L14) copies (the same code relinked;
// compared word by word: only relocated words differ). Kept apart from the draw callbacks (shared_class252.cpp), whose
// file another task edits; the header is shared_class252.h.
#include <game/ovl/shared_class252.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/weapons.h>

namespace rac1 {
// ---- class 252's update and its two helpers (task F11). Their level-own words: a record the update hands its pvars
// +0xd0, and the two draw callbacks' addresses (per level below); the engine's (OV): the camera 0x166dc0, the path table
// 0x1b04b0, the collision's point 0x173e70; the base ELF's (bare): the frame's time 0x15ed6c and its square 0x15ed70, the
// hero's moby 0x1413d0.
//
// The pvars: +0x20 the health, +0x40 the speed, +0x110 the carry record, +0x170 the target's point (+0x178 its height),
// +0x1b0 the moby it takes as its target, +0x1b4 2 when the target is out of reach, +0x1c0 its home (+0x1c8 the floor
// height), +0x1d4 .. +0x1ec the beam's widths, +0x1f0 the point it wanders to, +0x200 the height above the floor, +0x204 /
// +0x208 / +0x20c the bob's phase, size and rate, +0x210 the yaw's rate, +0x214 the sound, +0x216 whether it floats,
// +0x218 the moby its beam goes to, +0x21c its path (an index of the path table; -1: none, it goes), +0x220 the glow's
// point, +0x230 the joint's scale.
namespace {
uint32_t class252_record() { return on_level(OLTANIS) ? L14_ADDR(0x00161a78) : L8_ADDR(0x00161888); }
uint32_t class252_beam_callback() { return on_level(OLTANIS) ? L14_ADDR(0x002da678) : L8_ADDR(0x002d3878); }
uint32_t class252_glow_callback() { return on_level(OLTANIS) ? L14_ADDR(0x002daf08) : L8_ADDR(0x002d4108); }
}  // namespace

// orig: L8:0x002d3d78 L14:0x002dab78
void class252_move(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t vel = pv + 0x40, pos = m + 0x10, step = sp;
    // the speed eased by 6 per second squared, at most 3 a frame
    float speed = fpu_sub(vec3_length(V(vel)), fpu_mul(lwc1(0x0015ed70), 6.0f));
    const float most = fpu_mul(lwc1(0x0015ed6c), 3.0f);
    if (most < speed) speed = most;
    else if (speed < 0.0f) speed = 0.0f;
    vec3_set_length(V(vel), V(vel), speed);
    vec3_add(V(pos), V(pos), V(vel));
    // the bob: its height about the floor's, the floating height on top
    const float phase = angle_add(lwc1(pv + 0x204), lwc1(pv + 0x20c));
    swc1(pv + 0x204, phase);
    float height = fpu_add(lwc1(pv + 0x1c8), fpu_mul(lwc1(pv + 0x208), sin_vu(phase)));
    if (lh(pv + 0x216) != 0) height = fpu_add(height, lwc1(pv + 0x200));
    float rise = fpu_sub(height, lwc1(m + 0x18));
    const float cap = fpu_mul(lwc1(0x0015ed6c), 3.0f);
    if (cap < rise) rise = cap;
    else if (rise < -cap) rise = -cap;
    swc1(pv + 0x48, rise);
    // (a test the original makes through a stub that always answers 0: the push out of what it met never happens)
    lq_sq(step, pos);
    swc1(step + 8, fpu_add(lwc1(step + 8), 0.5f));
    if (zero_stub() != 0) {
        vec3_sub(V(step), V(OV(0x00173e70)), V(pos));
        const float lim = fpu_mul(lwc1(0x0015ed6c), 3.0f);
        const float z = lwc1(step + 8);
        if (lim < z) swc1(step + 8, lim);
        else if (z < -lim) swc1(step + 8, -lim);
        vec3_add(V(pos), V(pos), V(step));
    }
    ee_stack_free(0x60);
}

// orig: L8:0x002d36e0 L14:0x002da4e0
void class252_hit(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t st = 0;
    float dmg = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t r = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    if (st != 1 && lbu(m + 0x20) != 0x63) {
        const float health = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) r = 1;
        if (r < 12 && r >= 0) {
            moby_drop_bolts(m, 0, -1);
            sb(m + 0x20, 0x63);
        }
    }
    sb(m + 0xa4, 0xff);
    const uint32_t state = lbu(m + 0x20);
    if (state != 0 && state != 4) {
        // its target: one in its path's boxes within 14; too far from home or too far above or below it: out of reach
        const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x21c) << 2));
        const float range = 14.0f;
        if (target_select(m, pv + 0x170, 0, 0, path + 0x10, lw(path), range) != 2) {
            if (range < vec2_distance(V(pv + 0x1c0), V(pv + 0x170))) sw(pv + 0x1b4, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(pv + 0x1c8), lwc1(pv + 0x178)))) sw(pv + 0x1b4, 2);
        }
    }
    if (lwu(pv + 0x1b0) == 0) sw(pv + 0x1b0, lwu(0x001413d0));
}

// Hit, then by its state: 0 set up (no path: gone), 1 wait (the target out of reach: wander to a point about home, 7), 2
// go for the target (close: strike, 6; strayed: home, 3), 3 home, 4 the strike (the beam drawn over its frames 9 .. 14; at
// 14 a blow to the beam's moby when close), 5 carried, 6 the strike's sound, 7 wander, 0x63 dead (an explosion, gone).
// Then the move, and but in 5 or from 0x80 up its glow toward the state's colour and its draw.
// orig: L8:0x002d2af0 L14:0x002d98f0
void class252_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    joint_node_scale(m, 2, pv + 0x230, u2f(0x40666666u));  // (3.6)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(OV(0x00166dc0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    class252_hit(m);
    sw(pv + 0x218, lwu(pv + 0x1b0));
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t goal = sp + 0x20, offset = sp + 0x30, at = sp + 0x40;
    lq_sq(goal, pv + 0x170);
    const float quarter_turn = u2f(0x3f490fdbu);  // (pi / 4)
    auto anim_to = [m](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    // turned toward (x, y); facing it within pi / 4, pushed ahead by 12 a second squared
    auto turn_and_push = [m, pv, quarter_turn](float x, float y) {
        const float yaw = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
        yaw_turn_damped(m, pv + 0x210, yaw, u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3d4ccccdu));  // (0.01, 0.3, 0.05)
        if (angle_dist(lwc1(m + 0x48), yaw) < quarter_turn) {
            const float push = 12.0f;
            swc1(pv + 0x40, fpu_add(lwc1(pv + 0x40), fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed70), push))));
            swc1(pv + 0x44, fpu_add(lwc1(pv + 0x44), fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed70), push))));
        }
    };
    auto face = [m, pv](float x, float y) {
        const float yaw = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
        yaw_turn_damped(m, pv + 0x210, yaw, u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3d4ccccdu));
    };
    auto rise = [pv] {
        swc1(pv + 0x200, 4.0f);
        swc1(pv + 0x208, 0.5f);
    };
    switch (lbu(m + 0x20)) {
    case 0: {
        if (lw(pv + 0x21c) == -1) {
            moby_retire(m);
            ee_stack_free(0xd0);
            return;
        }
        sh(m + 0x34, lhu(m + 0x34) | 0x1000);
        swc1(m + 0x48, rand_angle());
        swc1(pv + 0x200, 4.0f);
        const float phase = rand_angle();
        swc1(pv + 0x208, 0.5f);
        swc1(pv + 0x204, phase);
        vec_clear(V(pv + 0x40));
        swc1(pv + 0x20c, fpu_mul(lwc1(0x0015ed6c), u2f(0x40060a92u)));  // (2 pi / 3 a second)
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(15.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x4119999au))));  // (9.6)
        sw(pv + 0xd0, class252_record());
        anim_to(0, 0);
        sb(m + 0x20, 1);
        lq_sq(pv + 0x1c0, pos);
        // home's floor: the floor below when it is within 10 under it (then it floats), else its own height
        const float top = lwc1(m + 0x18);
        if (fpu_sub(top, 10.0f) < floor_probe_v(0.5f, V(pos), 0)) {
            swc1(pv + 0x1c8, floor_probe_v(0.5f, V(pos), 0));
            sh(pv + 0x216, 1);
        } else {
            swc1(pv + 0x1c8, top);
            sh(pv + 0x216, 0);
        }
        lq_sq(pv + 0x1f0, pv + 0x1c0);
        sb(pv + 0x29, 0);
        break;
    }
    case 1: {
        if (lw(pv + 0x1b4) == 2) {
            if ((lbu(m + 0x70) & 2) == 0) break;
            // a point 0 .. 3 from home to wander to
            const float a = rand_angle();
            const float r = rand_range_f(0.0f, 3.0f);
            swc1(offset, fpu_mul(cos_vu(a), r));
            swc1(offset + 4, fpu_mul(sin_vu(a), r));
            swc1(offset + 8, 0.0f);
            vec3_add(V(pv + 0x1f0), V(pv + 0x1c0), V(offset));
            anim_to(2, 6);
            sb(m + 0x20, 7);
            break;
        }
        sw(pv + 0x200, 0);
        swc1(pv + 0x208, u2f(0x3dcccccdu));  // (0.1)
        anim_to(2, 6);
        sb(m + 0x20, 2);
        break;
    }
    case 2: {
        turn_and_push(lwc1(goal), lwc1(goal + 4));
        const uint32_t target = lwu(pv + 0x218);
        if (vec2_distance(V(pos), V(target + 0x10)) < 3.0f && abs_f(fpu_sub(lwc1(target + 0x18), lwc1(m + 0x18))) < 2.0f) {
            sh(pv + 0x214, static_cast<uint32_t>(moby_sound_start(0, 0, m)));
            sb(m + 0x20, 6);
            break;
        }
        if (!(15.0f < vec2_distance(V(goal), V(pv + 0x1c0))) && !(4.0f < abs_f(fpu_sub(lwc1(pv + 0x1c8), lwc1(goal + 8))))) break;
        rise();
        anim_to(2, 6);
        sb(m + 0x20, 3);
        break;
    }
    case 3: {
        const float x = lwc1(pv + 0x1c0), y = lwc1(pv + 0x1c4);
        const float yaw = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
        rise();
        yaw_turn_damped(m, pv + 0x210, yaw, u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3d4ccccdu));
        if (angle_dist(lwc1(m + 0x48), yaw) < quarter_turn) {
            const float push = 12.0f;
            swc1(pv + 0x40, fpu_add(lwc1(pv + 0x40), fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed70), push))));
            swc1(pv + 0x44, fpu_add(lwc1(pv + 0x44), fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed70), push))));
        }
        if (vec2_distance(V(pos), V(pv + 0x1c0)) < 1.0f && u2f(0x4079999au) <= abs_f(fpu_sub(lwc1(pv + 0x1c8), lwc1(m + 0x18)))) {  // (3.9)
            anim_to(0, 3);
            sb(m + 0x20, 1);
        }
        break;
    }
    case 4: {
        face(lwc1(goal), lwc1(goal + 4));
        const float t = moby_anim_time(m);
        if (lbu(m + 0x52) == lbu(m + 0x53) && 9.0f <= t && t <= 14.0f) {
            // the beam: its widths grow over the frames 9 .. 14, wobbling about a random angle
            const float k = fpu_div(fpu_sub(t, 9.0f), 5.0f);
            const float twice = fpu_add(k, k);
            swc1(pv + 0x1dc, fpu_div(k, 3.0f));
            swc1(pv + 0x1ec, fpu_div(twice, 3.0f));
            const float a = rand_angle();
            const float w = 0.25f;
            swc1(pv + 0x1d4, fpu_mul(fpu_mul(sin_vu(a), w), k));
            swc1(pv + 0x1d8, fpu_mul(fpu_mul(cos_vu(a), w), k));
            swc1(pv + 0x1e4, fpu_mul(fpu_mul(sin_vu(a), w), k));
            swc1(pv + 0x1e8, fpu_mul(fpu_mul(cos_vu(a), w), k));
            sub_001f93e0(class252_beam_callback(), m);
        }
        if (anim_crossed(m, 14.0f) == 0) break;
        if (vec2_distance(V(lwu(pv + 0x1b0) + 0x10), V(pos)) < 3.0f) {
            const uint32_t target = lwu(pv + 0x218);
            const float reach = 1.0f;
            if (abs_f(fpu_sub(lwc1(target + 0x18), lwc1(m + 0x18))) < reach) {
                // a blow from the goal (0.75 up) along its facing, 0.2
                const float s = u2f(0x3e4ccccdu);  // (0.2)
                swc1(offset, fpu_mul(cos_vu(lwc1(m + 0x48)), s));
                swc1(offset + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), s));
                sw(offset + 8, 0);
                lq_sq(at, goal);
                swc1(at + 8, fpu_add(lwc1(at + 8), 0.75f));
                sub_00259bc8(lwu(pv + 0x218), m, 1, at, offset, reach);
            }
        }
        rise();
        anim_to(2, 3);
        sb(m + 0x20, 3);
        break;
    }
    case 5:
        if (carry_update(m, pv + 0x110) != 0) {
            rise();
            anim_to(2, 3);
            sh(pv + 0xc8, 0);
        }
        break;
    case 6:
        face(lwc1(goal), lwc1(goal + 4));
        if (sub_0028d8c0(m, lh(pv + 0x214)) != 0) break;
        anim_to(3, 3);
        sb(m + 0x20, 4);
        break;
    case 7: {
        turn_and_push(lwc1(pv + 0x1f0), lwc1(pv + 0x1f4));
        if (vec2_distance(V(pos), V(pv + 0x1f0)) < 0.5f) {
            anim_to(0, 3);
            sb(m + 0x20, 1);
        }
        break;
    }
    case 0x63: {
        lq_sq(offset, pos);
        swc1(offset + 8, fpu_add(lwc1(offset + 8), 1.0f));
        moby_sound_start(1, 0, m);
        explosion_effect(m, pv + 0x40, offset, 5, 2, 4, -1, 0, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 0.0f);
        moby_retire(m);
        ee_stack_free(0xd0);
        return;
    }
    default:
        break;
    }
    class252_move(m);
    const uint32_t state = lbu(m + 0x20);
    if (state != 5 && (state & 0x80) == 0) {
        const uint32_t colour = state == 4 || state == 6 ? 0x7f2828a0u : 0x7f20ff20u;
        sw(m + 0x90, rgba_lerp(lwu(m + 0x90), colour, u2f(0x3dcccccdu)));  // (0.1)
        moby_chain_point_v(m, 3, V(pv + 0x220));
        draw_callback_add(class252_glow_callback(), m);
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
