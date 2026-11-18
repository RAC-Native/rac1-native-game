// Hoven's (level 12) classes 326 (the gunship, both roaming and in the turret-defense waves, with its rotor moby of class
// 1371 and helpers 0x27ba70 / 0x2eae00 / 0x2eaf68 / 0x2eb220 / 0x2ecce8) and 409 (the rocket 0x199 the gunship fires).
// Level 12 only: `orig: L12:`, level data through lvd::* and collision hits through coll_hit_*, Hoven-only data L12_ADDR,
// base ELF bare. Registered in hoven_mobys.cpp.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

void stop_owned_sound(uint32_t m, int32_t ch) {
    if (ch == -1) return;
    const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
    if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
}
}  // namespace

// Stores the 3D segment lengths of path `path_idx` into each point's w lane (`+0x1c + i * 0x10`) for `i < count - 1`.
// orig: L12:0x0027ba70
void gunship_path_lengths(int32_t path_idx) {
    if (path_idx == -1) return;
    const uint32_t path = lvd::path(path_idx);
    if (lw(path) - 1 <= 0) return;
    for (int32_t i = 0; i < lw(path) - 1; ++i) {
        const int32_t n = lw(path);
        const int32_t next = ee_rem(i + 1, n);
        const float d = vec3_distance(V(path + 0x10 + (static_cast<uint32_t>(i) << 4)),
                                      V(path + 0x10 + (static_cast<uint32_t>(next) << 4)));
        swc1(path + 0x1c + (static_cast<uint32_t>(i) << 4), d);
    }
}

// Banks the gunship's pitch (`+0x44`) and roll (`+0x40`) from its velocity (`pv + 0x70`) and speed (`pv + 0x94`).
// orig: L12:0x002eae00
void gunship_tilt(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float rel = angle_sub(atan2_f(lwc1(pv + 0x70), lwc1(pv + 0x74)), lwc1(m + 0x48));
    const float c = cos_vu(rel);
    const float pitch_goal =
        fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x94), u2f(0x3e685696u)), c), fpu_mul(lwc1(0x0015ed6c), 10.0f));
    const float s = sin_vu(rel);
    const float dt = lwc1(0x0015ed6c), dt2 = lwc1(0x0015ed70);
    const float roll_goal =
        fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x94), u2f(0xbe685696u)), s), fpu_mul(dt, 10.0f));
    const float pitch =
        angle_seek_rate(pv + 0x9c, lwc1(m + 0x44), pitch_goal, fpu_mul(dt2, u2f(0x3f060a92u)),
                        fpu_mul(dt2, u2f(0x3f860a92u)), fpu_mul(dt, u2f(0x3f490fdbu)));
    const float dt2_b = lwc1(0x0015ed70), dt_b = lwc1(0x0015ed6c);
    swc1(m + 0x44, pitch);
    const float roll =
        angle_seek_rate(pv + 0x98, lwc1(m + 0x40), roll_goal, fpu_mul(dt2_b, u2f(0x3f860a92u)),
                        fpu_mul(dt2_b, u2f(0x40060a92u)), fpu_mul(dt_b, u2f(0x3f9c61aau)));
    swc1(m + 0x40, roll);
}

// Flies the gunship along path `path_idx`, slowing to a halt near the final point and returning 1 once stopped.
// orig: L12:0x002eaf68
uint32_t gunship_follow_path(uint32_t m, int32_t path_idx) {
    const Frame f(192);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(path_idx);
    const int32_t last = lw(path) - 1;
    const int32_t cur = lw(pv + 0xb4);
    float rem_len = 0.0f;
    for (int32_t k = cur; k < last; ++k) {
        rem_len = fpu_add(rem_len, lwc1(path + 0x1c + (static_cast<uint32_t>(k) << 4)));
    }
    if (5.0f < rem_len) {
        const uint32_t pt = path + (static_cast<uint32_t>(lw(pv + 0xb4)) << 4);
        const float yaw = atan2_f(fpu_sub(lwc1(pt + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(pt + 0x14), lwc1(m + 0x14)));
        const float new_yaw = angle_seek_rate(pv + 0x88, lwc1(m + 0x48), yaw, u2f(0x3b83126fu), u2f(0x3e99999au),
                                              fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu)));
        swc1(m + 0x48, new_yaw);
        approach_f(pv + 0x94, fpu_mul(lwc1(0x0015ed6c), 10.0f), fpu_mul(lwc1(0x0015ed70), 7.0f));
    } else {
        target_select_any(m, sp, 48.0f);
        const float yaw = atan2_f(fpu_sub(lwc1(sp + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x4), lwc1(m + 0x14)));
        const float new_yaw = angle_seek_rate(pv + 0x88, lwc1(m + 0x48), yaw, u2f(0x3b83126fu), u2f(0x3e99999au),
                                              fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu)));
        swc1(m + 0x48, new_yaw);
        approach_f(pv + 0x94, 0.0f, fpu_mul(lwc1(0x0015ed70), 9.0f));
        if (lwc1(pv + 0x94) == 0.0f) return 1;
    }
    const uint32_t pt = path + 0x10 + (static_cast<uint32_t>(lw(pv + 0xb4)) << 4);
    vec3_sub(V(sp), V(pt), V(m + 0x10));
    vec3_set_length(V(pv + 0x70), V(sp), lwc1(pv + 0x94));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x70));
    const uint32_t pt2 = path + 0x10 + (static_cast<uint32_t>(lw(pv + 0xb4)) << 4);
    if (vec3_distance(V(m + 0x10), V(pt2)) < 2.0f) {
        const int32_t next = lw(pv + 0xb4) + 1;
        sw(pv + 0xb4, static_cast<uint32_t>(next));
        const int32_t max_idx = lw(path) - 1;
        if (max_idx < next) sw(pv + 0xb4, static_cast<uint32_t>(max_idx));
    }
    gunship_tilt(m);
    return 0;
}

// Processes weapon hits on the gunship (mask 0x330000; turret homing missiles 0x1ca deal 10 damage, shots 0xb8 and own
// rockets 0x199 are ignored), starting knockback state 5 or death state 6.
// orig: L12:0x002eb220
void gunship_hits(uint32_t m) {
    const Frame f(128);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    swc1(sp + 4, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    int32_t st = lw(sp);
    float dmg = lwc1(sp + 4);
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(sp, static_cast<uint32_t>(st));
    swc1(sp + 4, dmg);
    if (lw(sp) != 1 && lbu(m + 0x20) != 6) {
        const int16_t src_cls = lh(lwu(hit + 0x20) + 0xa6);
        if (src_cls != 0xb8 && src_cls != 0x199) {
            if (src_cls == 0x1ca) swc1(sp + 4, 10.0f);
            const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(sp + 4));
            swc1(pv + 0x20, hp);
            if (react == 1 || hp <= 0.0f) {
                moby_sound_start(2, 0, m);
                sb(m + 0x20, 6);
            } else if (react != 0xb) {
                sb(pv + 0x67, 0xb4);
                color_pulse_start(m, pv + 0x60);
                if (lwc1(hit + 0x1c) == u2f(0x45afdf66u) && lbu(m + 0x20) != 5) {
                    const float rel = angle_sub(
                        atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(hit + 0x0)), fpu_sub(lwc1(m + 0x14), lwc1(hit + 0x4))),
                        lwc1(m + 0x48));
                    const float pitch_k = fpu_mul(cos_vu(rel), u2f(0x3db2b8c2u));
                    const float roll_k = fpu_mul(sin_vu(rel), u2f(0xbeb2b8c2u));
                    if (abs_f(lwc1(m + 0x40)) < abs_f(roll_k)) {
                        swc1(m + 0x40, roll_k);
                        swc1(pv + 0x98, 0.0f);
                    }
                    if (abs_f(lwc1(m + 0x44)) < abs_f(pitch_k)) {
                        swc1(m + 0x44, pitch_k);
                        swc1(pv + 0x9c, 0.0f);
                    }
                    swc1(pv + 0x94, fpu_mul(lwc1(0x0015ed6c), 4.0f));
                    sh(pv + 0x8e, static_cast<uint32_t>(frame_scale_i(0x3c)));
                    const uint32_t prev_st = lbu(m + 0x20);
                    sb(m + 0x20, 5);
                    sb(m + 0xbc, prev_st);
                    moby_sound_start(1, 0, m);
                }
            }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// Spawns a gunship rocket (class 409, 0x199) at `pos` with velocity `vel`.
// orig: L12:0x002ecce8
uint32_t gunship_rocket_spawn(uint32_t vel, uint32_t pos, uint32_t owner, int32_t life, float yaw, float speed, float f14) {
    const uint32_t r = moby_spawn(0x199);
    if (r == 0) return 0;
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0x7e);
    sb(r + 0x20, 1);
    const uint32_t rpv = lwu(r + 0x78);
    sb(r + 0x31, 1);
    lq_sq(r + 0x10, pos);
    lq_sq(rpv + 0x0, vel);
    swc1(r + 0x48, atan2_f(lwc1(rpv + 0x0), lwc1(rpv + 0x4)));
    swc1(r + 0x44, -atan2_f(vec2_length(V(rpv)), lwc1(rpv + 0x8)));
    const uint32_t scaled_life = static_cast<uint32_t>(frame_scale_i(life));
    sw(rpv + 0x10, owner);
    swc1(rpv + 0x18, f14);
    swc1(rpv + 0x1c, yaw);
    swc1(rpv + 0x24, speed);
    sw(rpv + 0x28, 0);
    sw(rpv + 0x14, scaled_life);
    moby_update_transform(r);
    return r;
}

// Class 326's update: states 0..6 (jump table L12:0x20c540: 0 set up, 1 waiting for trigger box or wave timer, 2 flying
// entry path, 3 hovering and firing rockets, 4 strafing between ends of path `pv + 0xa4`, 5 knocked back, 6 exploding and
// retiring or respawning in turret-wave mode `pv + 0xc0 != 0`).
// orig: L12:0x002e9f68
void hov_class326_update_l12(uint32_t m) {
    const Frame f(352);
    const uint32_t sp = f.sp;
    if (lw(L12_ADDR(0x0015f5c4)) == 2) return;
    const uint32_t pv = lwu(m + 0x78);
    target_select_any(m, sp + 0x20, 70.0f);
    const uint32_t rotor0 = lwu(pv + 0xac);
    if (rotor0 != 0 && static_cast<int8_t>(lbu(rotor0 + 0x20)) >= 0) {
        joint_node_scale(rotor0, 0, pv + 0xd0, u2f(0x402ccccdu));
    }
    gunship_hits(m);

    if (lbu(0x001413f4) == 2) {
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 0.5f));
        const uint32_t r = lwu(pv + 0xac);
        if (r != 0) swc1(r + 0x2c, fpu_mul(lwc1(lwu(r + 0x24) + 0x24), 0.5f));
    } else {
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        const uint32_t r = lwu(pv + 0xac);
        if (r != 0) swc1(r + 0x2c, lwc1(lwu(r + 0x24) + 0x24));
    }

    if (static_cast<uint32_t>(lbu(m + 0x20) - 2u) < 4u) {
        if (lw(pv + 0xb0) == -1) sw(pv + 0xb0, static_cast<uint32_t>(moby_sound_start(3, 4, m)));
    } else {
        stop_owned_sound(m, lw(pv + 0xb0));
        sw(pv + 0xb0, 0xffffffffu);
    }

    if (lw(0x0015ed84) == 12 && lw(L12_ADDR(0x00161f00)) != 0 && lwu(pv + 0xc0) == 0) {
        if (lwu(L12_ADDR(0x0016735c)) == m) sw(L12_ADDR(0x0016735c), 0);
        if (lbu(m + 0x31) == 0) {
            if (lwu(pv + 0xac) != 0) {
                moby_retire(lwu(pv + 0xac));
                sw(pv + 0xac, 0);
            }
            moby_retire(m);
            return;
        }
    }
    if (lwu(pv + 0xc0) != 0) {
        const uint32_t cp = 0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0);
        if (lbu(cp) == 0xff) {
            if (lwu(pv + 0xac) != 0) {
                moby_retire(lwu(pv + 0xac));
                sw(pv + 0xac, 0);
            }
            moby_retire(m);
            return;
        }
        if (lw(L12_ADDR(0x001619a0)) != 0) {
            if (lwu(pv + 0xac) != 0) {
                class1371_throw(m, lwu(pv + 0xac));
                sw(pv + 0xac, 0);
            }
            moby_retire(m);
            return;
        }
    }

    switch (lbu(m + 0x20)) {  // jump table L12:0x0020c540 (7 entries)
    case 0: {                 // (0x002ea1e4)
        sb(pv + 0x28, 2);
        swc1(pv + 0x20, 6.0f);
        sh(pv + 0x24, 6);
        sb(pv + 0x29, 0);
        sw(pv + 0x94, 0);
        sw(pv + 0x98, 0);
        sw(pv + 0x9c, 0);
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        gunship_path_lengths(lw(pv + 0xa0));
        gunship_path_lengths(lw(pv + 0xa0));
        gunship_path_lengths(lw(pv + 0xa4));
        if (lh(m + 0x32) < 128) sh(m + 0x32, 0x80);
        if (lwu(pv + 0xac) == 0) {
            const uint32_t r = class1371_spawn(m);
            sw(pv + 0xac, r);
            sb(r + 0x30, 0);
            const uint32_t r2 = lwu(pv + 0xac);
            sh(r2 + 0x34, lhu(r2 + 0x34) | 0x41u);
        }
        const uint32_t path0 = lvd::path(lw(pv + 0xa0));
        lq_sq(m + 0x10, path0 + 0x10);
        if (lwu(pv + 0xc0) != 0 && lwu(pv + 0xc4) == 0) {
            const int32_t lo_f = frame_scale_i(0xf0), hi_f = frame_scale_i(0x190);
            sw(pv + 0xc4, static_cast<uint32_t>(rand_range(lo_f, hi_f)));
            int32_t w = lw(L12_ADDR(0x00161f20)) - 1;
            if (!(w < 11)) w = 10;
            if (!(-1 < w)) w = 0;
            const int32_t extra = w * frame_scale_i(0x23);
            sw(pv + 0xc4, lwu(pv + 0xc4) + static_cast<uint32_t>(extra));
        }
        sw(pv + 0xb4, 0);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x002ea32c)
        int32_t timed_out = 0;
        if (lwu(pv + 0xc0) != 0 && lw(L12_ADDR(0x00161f00)) != 0) {
            timed_out = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc4))) > 0 ? 1 : 0;
        }
        const int32_t box = lw(pv + 0x80);
        if ((box != -1 && point_in_box(0x0013f3d0, box) != 0) || timed_out != 0) {
            sb(pv + 0x58, 0x16);
            sb(pv + 0x59, 1);
            sb(pv + 0x5a, 7);
            sb(m + 0xa4, 0xff);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            const uint32_t r = lwu(pv + 0xac);
            if (r != 0) sh(r + 0x34, lhu(r + 0x34) & 0xffbeu);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2: {  // (0x002ea3e0)
        if (lwu(pv + 0xc0) == 0 && lwu(L12_ADDR(0x0016735c)) == 0) {
            sw(L12_ADDR(0x00167360), 0);
            sw(L12_ADDR(0x0016735c), m);
        }
        if (lwu(pv + 0xc0) != 0) {
            int32_t w = lw(L12_ADDR(0x00161f20)) - 3;
            if (!(w < 6)) w = 5;
            if (!(-1 < w)) w = 0;
            if (lh(pv + 0x8c) < w * frame_scale_i(0xf)) {
                sh(pv + 0x8c, static_cast<uint32_t>(w * frame_scale_i(0xf)));
            }
        }
        if (gunship_follow_path(m, lw(pv + 0xa0)) != 0) sb(m + 0x20, 3);
        break;
    }
    case 3: {  // (0x002ea478)
        if (lw(0x0015ed84) == 12 && lbu(0x0013d4cb) == 0 && lhu(0x00141c50) == 0) {
            sub_00203908(TXT(0x2ee2), 0x5d);
        }
        const float hero_d = vec2_distance(V(m + 0x10), V(0x0013f3d0));
        if (lwu(pv + 0xc0) == 0) {
            if (hero_d < 21.0f && 1.0f < fpu_sub(lwc1(m + 0x18), lwc1(0x0013f3d8))) {
                if (lwu(L12_ADDR(0x0016735c)) == 0) {
                    sw(L12_ADDR(0x00167360), 0);
                    sw(L12_ADDR(0x0016735c), m);
                }
            } else if (lwu(L12_ADDR(0x0016735c)) == m) {
                sw(L12_ADDR(0x0016735c), 0);
            }
        }
        const bool in_range =
            (vec2_distance(V(m + 0x10), V(sp + 0x20)) < 40.0f && -2.0f < fpu_sub(lwc1(m + 0x18), lwc1(sp + 0x28))) ||
            lwu(pv + 0xc0) != 0;
        if (in_range) {
            const float yaw_goal =
                atan2_f(fpu_sub(lwc1(sp + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x24), lwc1(m + 0x14)));
            swc1(m + 0x48, angle_seek_rate(pv + 0x88, lwc1(m + 0x48), yaw_goal, u2f(0x3b83126fu), u2f(0x3e99999au),
                                           fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu))));
            const float d2d = vec2_distance(V(sp + 0x20), V(m + 0x10));
            const float pitch_goal = atan2_f(d2d, fpu_sub(lwc1(m + 0x18), lwc1(sp + 0x28)));
            swc1(m + 0x44, angle_seek_rate(pv + 0x9c, lwc1(m + 0x44), pitch_goal, u2f(0x3b83126fu), u2f(0x3e99999au),
                                           fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu))));
            vec_clear(V(pv + 0x70));
            gunship_tilt(m);
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x8c))) != 0) {
                sh(pv + 0xbc, lhu(pv + 0xbc) + 1u);
                const uint32_t muzzle = sp + 0x70, aim = sp + 0x90, vel = sp + 0x80, pvel = sp + 0xa0;
                moby_chain_point_v(m, lwu(pv + 0xb8), V(muzzle));
                sw(pv + 0xb8, lwu(pv + 0xb8) < 1u ? 1u : 0u);
                lq_sq(aim, sp + 0x40);
                if (lwu(pv + 0xc0) != 0) {
                    lq_sq(aim, L12_ADDR(0x00167240));
                    const float rz = rand_range_f(-0.5f, u2f(0x3dcccccd));
                    swc1(aim + 8, fpu_add(lwc1(aim + 8), rz));
                    const float rx = rand_range_signed_f(0.0f, u2f(0x3ecccccd));
                    swc1(aim + 0, fpu_add(lwc1(aim + 0), rx));
                }
                vec3_sub(V(vel), V(aim), V(muzzle));
                vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(0x0015ed6c), lwu(pv + 0xc0) != 0 ? 22.0f : 15.0f));
                const float shot_yaw = atan2_f(lwc1(vel + 0), lwc1(vel + 4));
                const int32_t shot_life = frame_scale_i(0x82);
                gunship_rocket_spawn(vel, muzzle, m, shot_life, shot_yaw, fpu_mul(lwc1(0x0015ed6c), 15.0f), 0.0f);
                moby_sound_start(0, 0, m);
                vec3_set_length(V(vel), V(vel), 0.5f);
                vec3_add(V(muzzle), V(muzzle), V(vel));
                for (int k = 0; k < 25; ++k) {
                    swc1(pvel + 0, rand_range_signed_f(0.0f, fpu_mul(lwc1(0x0015ed6c), u2f(0x3fa66666u))));
                    swc1(pvel + 4, rand_range_signed_f(0.0f, fpu_mul(lwc1(0x0015ed6c), u2f(0x3fa66666u))));
                    swc1(pvel + 8, rand_range_signed_f(0.0f, fpu_mul(lwc1(0x0015ed6c), u2f(0x3fa66666u))));
                    const uint32_t r_col = static_cast<uint32_t>(rand_range(0xaa, 0xf0));
                    const uint32_t g_col = static_cast<uint32_t>(rand_range(0x6e, 0xb4)) << 8;
                    const uint32_t col = g_col | (r_col | 0x7f000000u);
                    const uint32_t plife = static_cast<uint32_t>(frame_scale_i(rand_range(0x1e, 0x46)));
                    const uint32_t psize = static_cast<uint32_t>(rand_range(0x78, 0xaa));
                    spawn_particle_4(muzzle, pvel, col, 0xff, plife, 0x1e, psize, 1);
                }
                int32_t volley = 4;
                if (lwu(pv + 0xc0) != 0) {
                    // (original tests `< 11` then `< 7` with two movz instructions, so `>= 7` always leaves 3)
                    const int32_t w = lw(L12_ADDR(0x00161f20));
                    if (!(w < 11)) volley = 2;
                    if (!(w < 7)) volley = 3;
                }
                if (lh(pv + 0xbc) >= volley && lw(pv + 0xa4) != -1) {
                    sh(pv + 0xbc, 0);
                    sb(m + 0x20, 4);
                } else {
                    sh(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(0x28)));
                }
            }
        }
        break;
    }
    case 4: {  // (0x002ea928)
        const float yaw_goal =
            atan2_f(fpu_sub(lwc1(sp + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x24), lwc1(m + 0x14)));
        swc1(m + 0x48, angle_seek_rate(pv + 0x88, lwc1(m + 0x48), yaw_goal, u2f(0x3b83126fu), u2f(0x3e99999au),
                                       fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu))));
        const uint32_t path = lvd::path(lw(pv + 0xa4));
        const uint32_t goal_pt = lh(pv + 0xbe) != 0 ? (path + 0x10) : (path + (lwu(path) << 4));
        const uint32_t delta = sp + 0x70;
        vec3_sub(V(delta), V(goal_pt), V(m + 0x10));
        bool move = true;
        if (5.5f < vec3_length(V(delta))) {
            approach_f(pv + 0x94, fpu_mul(lwc1(0x0015ed6c), 10.0f), fpu_mul(lwc1(0x0015ed70), 7.0f));
        } else {
            approach_f(pv + 0x94, 0.0f, fpu_mul(lwc1(0x0015ed70), 9.0f));
            if (lwc1(pv + 0x94) == 0.0f) {
                sh(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(0x32)));
                sh(pv + 0xbe, static_cast<uint32_t>(lh(pv + 0xbe)) < 1u ? 1u : 0u);
                sb(m + 0x20, 3);
                move = false;
            }
        }
        if (move) {
            vec3_set_length(V(pv + 0x70), V(delta), lwc1(pv + 0x94));
            vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x70));
            gunship_tilt(m);
        }
        break;
    }
    case 5: {  // (0x002eaad0)
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x8e))) != 0) {
            sb(m + 0x20, lbu(m + 0xbc));
        } else {
            const float next_sp = fpu_sub(lwc1(pv + 0x94), fpu_mul(lwc1(0x0015ed70), 4.0f));
            swc1(pv + 0x94, next_sp < 0.0f ? 0.0f : next_sp);
            const float c = cos_vu(lwc1(m + 0x48));
            swc1(sp + 0x70, fpu_mul(c, -lwc1(pv + 0x94)));
            const float s = sin_vu(lwc1(m + 0x48));
            swc1(sp + 0x78, 0.0f);
            swc1(sp + 0x74, fpu_mul(s, -lwc1(pv + 0x94)));
            vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x70));
        }
        break;
    }
    case 6: {  // (0x002eab64)
        if (lwu(L12_ADDR(0x0016735c)) == m) sw(L12_ADDR(0x0016735c), 0);
        if (lwu(pv + 0xac) != 0) {
            class1371_throw(m, lwu(pv + 0xac));
            sw(pv + 0xac, 0);
        }
        sw(sp + 0x0, 1);
        sw(sp + 0x8, 0xffffffffu);
        sw(sp + 0x10, 0);
        explosion_effect(m, L12_ADDR(0x0015f580), m + 0x10, 0x14, 3, 4, 2, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f,
                         100000.0f, 3.0f, 15.0f);
        if (lwu(pv + 0xc0) == 0) moby_drop_bolts(m, 0, -1);
        stop_owned_sound(m, lw(pv + 0xb0));
        if (lwu(pv + 0xc0) == 0) {
            moby_retire(m);
            return;
        }
        sw(pv + 0xb0, 0xffffffffu);
        const int32_t lo_f = frame_scale_i(0xf0), hi_f = frame_scale_i(0x190);
        sw(pv + 0xc4, static_cast<uint32_t>(rand_range(lo_f, hi_f)));
        sw(m + 0x94, 0);
        sb(m + 0x20, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        break;
    }
    default:
        break;
    }

    const float bob = u2f(0x3e2e147bu);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(cos_vu(lwc1(pv + 0x90)), bob)));
    const float period = static_cast<float>(frame_scale_i(0x78));
    const float phase = angle_add(lwc1(pv + 0x90), fpu_div(u2f(0x40c90fdbu), period));
    swc1(pv + 0x90, phase);
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(cos_vu(phase), bob)));
    if (lwu(pv + 0xac) != 0) {
        lq_sq(lwu(pv + 0xac) + 0x10, m + 0x10);
        lq_sq(lwu(pv + 0xac) + 0x40, m + 0x40);
        moby_anim_advance(lwu(pv + 0xac));
        moby_update_transform(lwu(pv + 0xac));
        const uint32_t r = lwu(pv + 0xac);
        sh(r + 0x34, lhu(r + 0x34) | 6u);
    }
    if (lwu(pv + 0xc0) == 0 && lbu(m + 0x20) >= 2 && lbu(m + 0x31) != 0 &&
        vec3_distance(V(m + 0x10), V(L12_ADDR(0x00167240))) < 30.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }
}

// Class 409's update: the gunship's rocket (state 1 flying with smoke trail and segment/sphere collision, state 2 exploding
// and retiring).
// orig: L12:0x002ece00
void hov_class409_update_l12(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp;
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        const uint32_t pv = lwu(m + 0x78);
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000u), 0);
        if (hit != 0 && lwu(hit + 0x20) != lwu(pv + 0x10)) {
            sb(m + 0x20, 2);
            return;
        }
        if (lw(0x0015ed84) == 12 && lw(0x001413d4) == 0x32) {
            const uint32_t owner = lwu(pv + 0x10);
            if (owner != 0) {
                if (static_cast<int8_t>(lbu(owner + 0x20)) < 0) {
                    moby_retire(m);
                    return;
                }
                if (lbu(owner + 0x20) < 2) {
                    sb(m + 0x20, 2);
                    return;
                }
            }
        }
        const uint32_t prev = sp + 0x20, pos = m + 0x10, fwd = sp + 0x30, tail = sp + 0x40;
        lq_sq(prev, pos);
        vec3_add(V(pos), V(pos), V(pv));
        const float spin = fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu));
        swc1(m + 0x48, lwc1(pv + 0x1c));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), spin));
        vec3_set_length(V(fwd), V(pv), u2f(0x3e4ccccdu));
        vec3_set_length(V(tail), V(pv), u2f(0xbe4ccccdu));
        vec3_add_inplace(V(tail), V(pos));
        {
            const uint32_t life = static_cast<uint32_t>(frame_scale_i(rand_range(0xf, 0x16)));
            const uint32_t size = static_cast<uint32_t>(rand_range(0x14, 0x23));
            spawn_particle_4(tail, fwd, 0x6f00afffu, 0xff, life, 0x28, size, 1);
        }
        {
            const uint32_t life = static_cast<uint32_t>(frame_scale_i(rand_range(0x1e, 0x3c)));
            const uint32_t size = static_cast<uint32_t>(rand_range(0x32, 0x4b));
            spawn_particle_4(tail, fwd, 0x1fffffffu, 0x004f4f4fu, life, 0x28, size, 0);
        }
        if (segment_collide_v(V(pos), V(prev), 0, m, nullptr) != 0 ||
            sphere_collide(pos, u2f(0x3e4ccccdu), 0, m) != 0) {
            const uint32_t hit_m = coll_hit_moby();
            if (hit_m != 0 && hit_m != lwu(pv + 0x10)) {
                int32_t dmg_i = 1;
                if (lw(0x0015ed84) == 12 && lw(0x001413d4) == 0x32) {
                    dmg_i = lw(L12_ADDR(0x00161f20)) < 16 ? 3 : 2;
                }
                const uint32_t rec = sp + 0x60;
                sub_00259888(static_cast<float>(dmg_i), rec, static_cast<int32_t>(m), 0x10003, pv);
                swc1(rec + 0x0, cos_vu(lwc1(pv + 0x1c)));
                swc1(rec + 0x4, sin_vu(lwc1(pv + 0x1c)));
                const uint32_t cls_id = lhu(m + 0xa6);
                const uint32_t target = coll_hit_moby();
                swc1(rec + 0x8, 1.0f);
                swc1(rec + 0xc, u2f(0x45afdf66u));
                sb(rec + 0x19, 1);
                sh(rec + 0x1a, cls_id);
                sb(rec + 0x18, 1);
                moby_hit_record(target, rec);
            }
            sb(m + 0x20, 2);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) sb(m + 0x20, 2);
    } else if (st == 2) {
        vec3_sub(V(sp + 0x20), V(m + 0x10), V(L12_ADDR(0x00167240)));
        vec3_set_length(V(sp + 0x20), V(sp + 0x20), u2f(0x3ecccccd));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x20));
        if (lw(0x0015ed84) == 12 && lw(0x001413d4) == 0x32) {
            sw(sp + 0x8, 0xffffffffu);
            sw(sp + 0x0, 0);
            sw(sp + 0x10, 0);
            explosion_effect(m, L12_ADDR(0x0015f580), 0, 3, 3, 5, -1, 0, 0, -1, 0, 0.0f, 0.0f, u2f(0x3e99999au),
                             u2f(0x3e4ccccdu), 0.0f, u2f(0x3e19999au), 0.0f);
            swc1(L12_ADDR(0x00167260), u2f(0x3ccccccdu));
            sw(L12_ADDR(0x00167268), static_cast<uint32_t>(frame_scale_i(0x14)));
        } else {
            explosion_burst(m, m + 0x10, 0, 0.25f, 13.0f);
        }
        moby_retire(m);
    }
}

}  // namespace rac1
