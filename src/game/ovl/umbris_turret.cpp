// Umbris's own moby classes: class 1041, the turret that lobs shells at Ratchet (umbris.turret: it wakes after a
// while, turns, fires a shell from the barrel nearest its aim every 7 frames for 3 seconds, rests 2; shot down: bolts,
// an explosion, its two halves as debris) and 0x30ba90, the shell (class 882) it fires — hand-ported from Umbris's own
// overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); the frame time
// 0x15ed6c (its square 0x15ed70) is the base ELF's, the level's frame counter 0x15f5cc and the camera 0x166e40 Umbris's.
//
// Its pvars: its hit state +0x20 (health +0x20), its colour pulse +0x60, the barrel's turn +0x80 (a quaternion, its angle
// +0xc0), the node +0x70 its barrels hang from, the first wait +0xb0 (frames), when it next acts +0xb4 (frame counter),
// the fire timer +0xb8, the shells fired +0xba, its aim +0xbc, which barrel pair fires +0xc4 (bit 0).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
uint32_t frame_counter() { return lwu(L7_ADDR(0x0015f5cc)); }
}  // namespace

// The shell (class 882) at `pos` with velocity `vel`, ignoring moby `ignore` in its collisions, its kind `kind`: shown,
// a random orientation, spins of 100 .. 300 degrees a second either way. The shell (0: none spawned).
// orig: L7:0x0030ba90
uint32_t umb_30ba90_l7(uint32_t pos, uint32_t vel, uint32_t ignore, int16_t kind) {
    const uint32_t b = moby_spawn(0x372);
    if (b == 0) return 0;
    sh(b + 0x32, 0xff);
    sb(b + 0x30, 0xff);
    sb(b + 0x31, 1);
    const uint32_t bp = lwu(b + 0x78);
    swc1(b + 0x40, rand_angle());
    swc1(b + 0x44, rand_angle());
    swc1(b + 0x48, rand_angle());
    const float deg = u2f(0x3c8efa35u);
    const float s0 = fpu_mul(rand_range_signed_f(100.0f, 300.0f), deg);
    swc1(bp + 0x18, fpu_mul(s0, lwc1(0x0015ed6c)));
    const float s1 = fpu_mul(rand_range_signed_f(100.0f, 300.0f), deg);
    swc1(bp + 0x1c, fpu_mul(s1, lwc1(0x0015ed6c)));
    V(b + 0x10) = V(pos);
    V(bp) = V(vel);
    sw(bp + 0x14, static_cast<uint32_t>(static_cast<int32_t>(kind)));
    sw(bp + 0x10, ignore);
    moby_update_transform(b);
    return b;
}

// Class 1041 (+0x20: 0 the first wait set, 1 resting, 2 firing; past 0 the barrel turns at 1020 degrees a second).
// orig: L7:0x0030d4d8
void umb_class1041_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L7_ADDR(0x00166e40))) < 27.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    if (pv == 0) return;
    const uint32_t hs = pv + 0x20;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0);
    int32_t state = 0;
    const int32_t r = moby_hit_react_r(m, hit, hs, 0, state, nullptr, nullptr, 4);
    if (r == 1 || r == 2) sw(hs, 0);  // (the table L7:0x2116c0: 1, 2 -> L7:0x30d5c8, the rest 0x30d5cc)
    if (state >= 2) {
        // not its own shells (0x372): the damage off its health; none left: destroyed
        const uint32_t by = hit != 0 ? lwu(hit + 0x20) : 0;
        if (by == 0 || lh(by + 0xa6) != 0x372) {
            const float dmg = lwc1(hit + 0x2c);
            const float health = lwc1(hs);
            if (health <= dmg) {
                moby_drop_bolts(m, 0, -1);
                swc1(hs, 0.0f);
                const uint32_t sp = ee_stack_alloc(0x10);
                vec_clear(V(sp));
                explosion_effect(m, sp, 0, 10, 3, 0x10, 1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
                ee_stack_free(0x10);
                const uint32_t z = L7_ADDR(0x0015f580);
                debris_spawn(m, 0x621, pos, m + 0x40, 0, 0, z, z, z, 0.0f);
                debris_spawn(m, 0x622, pos, m + 0x40, 0, 0, z, z, z, 0.0f);
                moby_retire(m);
                return;
            }
            swc1(hs, fpu_sub(health, dmg));
            sb(pv + 0x67, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(60)));
            color_pulse_start(m, pv + 0x60);
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    // (the target's record, its shells' frames: the original's stack, passed by address. The record is target_select's,
    // 0x48 bytes: +0 the point, +0x40 the moby, +0x44 the result; the original keeps it at sp+0x30 of a 0x1e0 frame)
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t tgt = sp + 0x0, from = sp + 0x50, at = sp + 0x60, vel = sp + 0x70;
    target_select_any(m, tgt, 20.0f);
    const uint32_t st = lbu(m + 0x20);
    bool next_rest = false;
    if (st == 0) {
        const int32_t wait = frame_scale_i(lw(pv + 0xb0));
        const int32_t extra = frame_scale_i(rand_range(0, 10));
        sw(pv + 0xb4, frame_counter() + static_cast<uint32_t>(wait) + static_cast<uint32_t>(extra));
        sub_0024f590(m, 0, pv + 0x70);
        sw(pv + 0xc0, 0);
        moby_sound_start(0, 4, m);
        sb(m + 0x20, 1);
        sw(pv + 0xc4, 0);
    } else if (st == 1) {
        if (!(static_cast<int32_t>(frame_counter()) < lw(pv + 0xb4))) {
            const int32_t span = frame_scale_i(180);
            sw(pv + 0xb4, frame_counter() + static_cast<uint32_t>(span));
            sh(pv + 0xb8, 0);
            swc1(pv + 0xbc, rand_angle());
            sh(pv + 0xba, 0);
            sb(m + 0x20, 2);
        }
    } else if (st == 2) {
        next_rest = true;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xb8))) != 0) {
            sh(pv + 0xb8, static_cast<uint32_t>(frame_scale_i(7)));
            // fires when shown, or the camera within 40 (within 90 every other volley)
            bool fire = lbu(m + 0x31) != 0;
            if (!fire) {
                const uint32_t cam = L7_ADDR(0x00166e40);
                if (vec2_distance(V(pos), V(cam)) < 90.0f && (lw(pv + 0xc4) & 1) != 0) fire = true;
                else if (vec2_distance(V(pos), V(cam)) < 40.0f) fire = true;
            }
            if (fire) {
                sw(pv + 0xc4, lwu(pv + 0xc4) ^ 1);
                if (vec2_distance(V(pos), V(tgt)) < 20.0f)
                    swc1(pv + 0xbc, atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x4), lwc1(m + 0x14))));
                // the shell's aim: 3 .. 11 ahead, -2 .. 2 aside, turned by the aim; from the barrel (1 .. 3) nearest it
                Vec4 aim{0.0f, 0.0f, 0.0f, 0.0f};
                aim.x = rand_range_f(3.0f, 11.0f);
                aim.y = rand_range_f(-2.0f, 2.0f);
                Vec4 best{0.0f, 0.0f, 0.0f, -1.0f};
                float best_d = u2f(0x40490fdbu);
                for (uint32_t k = 1; k < 4; ++k) {
                    Vec4 p;
                    moby_chain_point_v(m, k, p);
                    const float a = atan2_f(fpu_sub(p.x, lwc1(m + 0x10)), fpu_sub(p.y, lwc1(m + 0x14)));
                    const float d = angle_dist(a, angle_sub(lwc1(pv + 0xbc), fpu_mul(lwc1(0x0015ed6c), u2f(0x418e6b3bu))));
                    if (d < best_d) {
                        best = p;
                        best_d = d;
                    }
                }
                V(from) = best;
                Mat4 rot;
                Vec4 ang{0.0f, 0.0f, lwc1(pv + 0xbc), 0.0f};
                euler_to_mat4_vu(rot, ang);
                mat4_transform(aim, aim, rot);
                Vec4 v;
                vec3_set_length(v, aim, fpu_mul(lwc1(0x0015ed6c), 7.0f));
                vec3_add(aim, aim, V(pos));
                V(at) = aim;
                V(vel) = v;
                const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));
                swc1(vel + 0x8, sub_0025abf0(from, at, nullptr, fpu_mul(lwc1(0x0015ed6c), 7.0f), -g));
                vec3_add(V(from), V(from), V(vel));
                umb_30ba90_l7(from, vel, m, static_cast<int16_t>(lhu(pv + 0xba)));
                sh(pv + 0xba, lhu(pv + 0xba) + 1);
            } else {
                next_rest = true;
            }
        }
    }
    ee_stack_free(0x80);
    if (next_rest && !(static_cast<int32_t>(frame_counter()) < lw(pv + 0xb4))) {
        const int32_t rest = frame_scale_i(120);
        sw(pv + 0xb4, frame_counter() + static_cast<uint32_t>(rest));
        sb(m + 0x20, 1);
    }
    if (lbu(m + 0x20) == 0) return;
    const float ang = angle_add(lwc1(pv + 0xc0), fpu_mul(lwc1(0x0015ed6c), u2f(0x418e6b3bu)));
    swc1(pv + 0xc0, ang);
    quat_from_axis_angle(V(pv + 0x80), 2, ang);
}

}  // namespace rac1
