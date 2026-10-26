// Novalis's own moby classes, part 3: the enemies. Header novalis_mobys.h. Addresses are Novalis's (`orig: L1:`);
// Novalis-only data is L1_ADDR, the globals the
// engine shares with Veldin are the level's own (OV).
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/walker.h>
#include <game/port_stubs.h>

namespace rac1 {

// Sets byte +0xbc of every moby of group `idx` (the level's table 0x1ab840 / Novalis 0x1abcc0 of lists of moby
// indices, the last one with bit 15 set) to `value`.
// orig: L1:0x0026e090
void moby_group_set_bc_l1(uint32_t idx, uint32_t value) {
    uint32_t p = lwu(OV(0x001ab840) + idx * 4);
    if (p == 0) return;
    for (;; p += 2) {
        sb(lwu(OV(0x0015ffd8)) + ((lhu(p) & 0x7fffu) << 8) + 0xbc, value);
        if (lh(p) < 0) break;
    }
}

// The walker record's defaults (speed 0, acceleration 0.3, drag 0.3, top speed 2.7, the stop distance 0.1, ...).
// orig: L1:0x0026d930
void walker_record_init_l1(uint32_t rec) {
    sw(rec, 0x280);
    swc1(rec + 0x2c, u2f(0x3dcccccdu));
    swc1(rec + 4, 1.0f);
    swc1(rec + 0xc, 0.5f);
    swc1(rec + 0x24, u2f(0x402ccccdu));
    swc1(rec + 0x1c, u2f(0x3e99999au));
    swc1(rec + 0x28, u2f(0x3fc8f5c3u));
    swc1(rec + 8, 0.5f);
    sw(rec + 0x14, 0);
    sw(rec + 0x10, 0);
    sw(rec + 0x18, 0);
    swc1(rec + 0x20, u2f(0x3e99999au));
}

// Claims a free slot of a table of `n` frame stamps (a slot whose stamp is before this frame's counter
// 0x15f5cc, from the last one down): the stamp becomes the frame counter + `delay`; the slot's index (-1:
// none).
// orig: L1:0x002efbf8
int32_t cooldown_slot_claim_l1(uint32_t arr, int32_t n, int32_t delay) {
    int32_t i = n - 1;
    if (i < 0) return i;
    const int32_t now = lw(OV(0x0015f5cc));
    if (lw(arr + static_cast<uint32_t>(i) * 4) < now) {
        sw(arr + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(now + delay));
        return i;
    }
    for (--i; i >= 0; --i) {
        if (lw(arr + static_cast<uint32_t>(i) * 4) < now) {
            sw(arr + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(now + delay));
            return i;
        }
    }
    return i;
}

// The next point of a path (count +0, points 0x10 apart from +0x10) for `m` to head for to get to
// `target`: the point nearest to the moby and the one nearest to the target among those nearer to it
// than the moby is; the moby's own point when they are the same, the next one toward the target's, or
// the target itself (-1) when no point is nearer the target. The point goes to `out`; the index is
// returned (the neighbour's, or -1).
// orig: L1:0x0028b5e0
int32_t path_next_point_l1(uint32_t m, uint32_t path, uint32_t target, uint32_t out) {
    int32_t near_moby = -1, near_target = -1;
    const float d_moby = vec3_distance(V(m + 0x10), V(target));
    float best_moby = u2f(0x461c4000u), best_target = best_moby;
    uint32_t p = path + 0x10;
    for (int32_t i = 0; i < lw(path); ++i, p += 0x10) {
        const float to_target = vec3_distance(V(p), V(target));
        const float to_moby = vec3_distance(V(p), V(m + 0x10));
        if (to_moby < best_moby) {
            best_moby = to_moby;
            near_moby = i;
        }
        if (to_target < d_moby && to_target < best_target) {
            best_target = to_target;
            near_target = i;
        }
    }
    if (near_target == -1) {
        lq_sq(out, target);
        return -1;
    }
    if (near_moby == near_target) {
        lq_sq(out, path + static_cast<uint32_t>(near_moby) * 16 + 0x10);
        return near_moby;
    }
    if (near_moby < near_target) {
        lq_sq(out, path + static_cast<uint32_t>(near_moby) * 16 + 0x20);
        return near_moby + 1;
    }
    lq_sq(out, path + static_cast<uint32_t>(near_moby) * 16);
    return near_moby - 1;
}

// Class 577, the Peckbot: a bird-legged robot that wanders about its home (pvar +0x1d0), turns to and
// walks at whatever target_select_any picks within the alert range +0x1fc (24, or 12 while the alert
// timer +0x214 runs), and pecks it (set 4..6); the hero's weapons hit it (mask 0x330000): health +0x20, state
// 0x63 when it dies (its bolts dropped, three debris pieces 0x6d3..5, an explosion), 6 when it is knocked
// back. pvars: the walker record +0x180, the move record +0x120 (enemy_physics / move_state_start), the
// colour pulse +0x110, the turn offset +0x1f4 / its size +0x1f8, the timers +0x208 / +0x20c / +0x210 / +0x214 /
// +0x228, the path index +0x21c, the follow distance +0x216 for the moby +0x218. States: 0 set up, 1 stands,
// 2 turns away, 3 walks, 4 pecks, 5 goes home, 6 / 0xb knocked back / thrown, 7 carried, 8 steps up, 0xa
// returns home to charge, 0xc walks to its path, 0xd shoved by other mobys, 0xe loiters, 0xf wanders, 0x10 /
// 0x11 rises from / sinks into the ground, 0x63 dies. m+0xbc: 1 while its group is alerted (set by
// moby_group_set_bc_l1).
// orig: L1:0x002efc60
void peckbot_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t pos0 = sp + 0x10, tgt = sp + 0x20, va = sp + 0x70, wp = sp + 0x80, diff = sp + 0x90;
    auto t16 = [](uint32_t a) -> int32_t { return timer_tick_i16(ee_ref<int16_t>(a)); };
    auto t32 = [](uint32_t a) -> int32_t { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); };
    joint_node_scale(m, 0, pv + 0x230, 1.5f);
    if (lw(OV(0x0015f5c4)) == 2) {
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        ee_stack_free(0x160);
        return;
    }
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    target_select_any(m, tgt, lwc1(pv + 0x1fc) + 4.0f);
    const uint32_t stale = lwu(sp + 0x60);  // (a stack word the original reads and never wrote)
    lq_sq(pos0, m + 0x10);
    if (lw(pv + 0x38) != 0) sh(pv + 0x214, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    t16(pv + 0x208);
    swc1(pv + 0x1fc, t16(pv + 0x214) == 0 ? u2f(0x41c00000u) : u2f(0x41400000u));
    const uint32_t hit = sub_0025a420(m, 0x330000, 0);
    int32_t hit_state = 0;
    float dmg = 0.0f;
    moby_hit_react_r(m, hit, pv + 0x20, 0, hit_state, &dmg, nullptr, 4);
    t16(pv + 0x26);
    const int32_t recent = lh(pv + 0x26);
    if (recent < frame_scale_i(0x2d) && hit != 0 && lbu(m + 0x20) != 0x63) {
        const uint32_t src = lwu(hit + 0x20);
        if (lhu(src + 0xa6) - 0xb0u < 2u && lh(pv + 0x26) != 0) swc1(hit + 0x2c, 0.0f);
        if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
        if (lwc1(hit + 0x2c) != 0.0f) {
            const float yaw = atan2_f(lwc1(m + 0x10) - lwc1(tgt), lwc1(m + 0x14) - lwc1(tgt + 4));
            swc1(sp + 0xa8, yaw);
            const float hp = lwc1(pv + 0x20) - lwc1(hit + 0x2c);
            const float dt = lwc1(0x0015ed6c);
            swc1(pv + 0x130, u2f(0x3c03126fu));
            swc1(pv + 0x134, u2f(0x3a03126fu));
            swc1(pv + 0x138, lwc1(L1_ADDR(0x00161a64)) * dt);
            swc1(pv + 0x13c, lwc1(L1_ADDR(0x00161a60)) * dt);
            sw(pv + 0x144, 9);
            swc1(pv + 0x20, hp);
            sb(pv + 0x15d, 0);
            if (hp <= 0.0f) {
                const float k = lwc1(0x0015ed6c) * u2f(0x41300000u);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                swc1(pv + 0x13c, k);
                swc1(pv + 0x138, k);
                lq_sq(va, hit + 0x10);
                vec_yaw_marked(va, sp + 0xa8, pv + 0x138, pv + 0x13c);
                move_state_start(m, pv + 0x120, 7, 1, 0, lwc1(sp + 0xa8));
                swc1(pv + 0x170, u2f(0x41300000u));
                swc1(pv + 0x174, u2f(0x41900000u));
                sb(m + 0x20, 0x63);
                sb(pv + 0x117, 0x78);
                color_pulse_start(m, pv + 0x110);
                moby_drop_bolts(m, 0, -1);
            } else {
                lq_sq(va, hit + 0x10);
                vec_yaw_marked(va, sp + 0xa8, pv + 0x138, pv + 0x13c);
                move_state_start(m, pv + 0x120, 6, 1, 0, lwc1(sp + 0xa8));
                swc1(pv + 0x170, u2f(0x40a00000u));
                swc1(pv + 0x174, u2f(0x41200000u));
                sb(m + 0x20, 6);
                sb(pv + 0x117, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x110);
            }
        }
    }
    const uint32_t state = lbu(m + 0x20);
    sb(m + 0xa4, 0xff);
    // the animation set `set` unless it is playing, blended over 3 frames
    auto anim = [&](uint32_t set) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(3));
    };
    // a new turn: maybe the other way, an offset of 30..90 frames
    auto turn_pick = [&] {
        if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
        swc1(pv + 0x1f4, lwc1(pv + 0x1f8) * u2f(0x3c8efa35u));
        sw(pv + 0x210, static_cast<uint32_t>(frame_scale_i(float_to_int(rand_range_f(u2f(0x41f00000u), u2f(0x42b40000u))))));
    };
    // the heading turned damped toward (x, y)
    auto face = [&](float x, float y, float accel, float damp, float vmax) {
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(x - lwc1(m + 0x10), y - lwc1(m + 0x14)), accel, damp, vmax);
    };
    // the vector 2 units along its heading (the walker's "target")
    auto ahead = [&] {
        swc1(va, cos_vu(lwc1(m + 0x48)) + cos_vu(lwc1(m + 0x48)));
        swc1(va + 4, sin_vu(lwc1(m + 0x48)) + sin_vu(lwc1(m + 0x48)));
        sw(va + 8, 0);
    };
    // the point it heads for: the next one of its path toward `goal`, or `goal` itself
    auto waypoint = [&](uint32_t goal) {
        if (lw(pv + 0x21c) != -1) path_next_point_l1(m, lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv + 0x21c)) * 4), goal, wp);
        else lq_sq(wp, goal);
    };
    // the ground check every walking state ends with: the surface kinds 0 / 8 (water) send it to 0x10
    auto water_end = [&] {
        floor_probe_v(0.5f, V(m + 0x10), 0);
        if (sub_001f35d8() != 0 && sub_001f35d8() != 8) {
            sb(m + 0xbc, 0);
            return;
        }
        sb(m + 0x20, 0x10);
        anim(0);
        sb(m + 0xbc, 0);
    };
    const float near_range = lwc1(pv + 0x1fc);
    switch (state < 0x64 ? state : 0xffu) {
    case 0:
        lq_sq(pv + 0x1d0, m + 0x10);
        sb(pv + 0x29, 0);
        if (lh(pv + 0x20a) == 0) {
            sb(m + 0x20, 0xe);
            swc1(m + 0x18, lwc1(m + 0x18) + u2f(0x40c00000u));
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        } else {
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, 0);
        }
        sh(pv + 0x214, 0);
        sh(pv + 0x208, 0);
        swc1(pv + 0x20, 1.0f);
        sh(pv + 0x24, 1);
        sb(pv + 0x58, 8);
        sb(pv + 0x5a, 4);
        walker_record_init_l1(pv + 0x180);
        sw(pv + 0x180, 0x38d);
        swc1(pv + 0x188, 2.0f);
        swc1(pv + 0x18c, 2.0f);
        swc1(pv + 0x1a4, lwc1(pv + 0x204) * lwc1(0x0015ed6c));
        if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sw(pv + 0xd0, L1_ADDR(0x00161a70));
        break;
    case 1: {
        vec3_sub(V(va), V(tgt), V(m + 0x10));
        const float len = vec3_length(V(va));
        if (len < near_range + 4.0f && abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 8.0f && rand_mod(0x13) == 0) {
            sb(m + 0x20, 2);
            anim(5);
            break;
        }
        if (lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) {
            turn_pick();
            sb(m + 0x20, 3);
            anim(2);
            sb(m + 0xbc, 0);
            break;
        }
        if (near_range + u2f(0x40c00000u) < vec3_length(V(va)) && lh(pv + 0x20a) == 0) {
            sb(m + 0x20, 0x10);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
        }
        break;
    }
    case 2: {
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(m + 0x10) - lwc1(tgt), lwc1(m + 0x14) - lwc1(tgt + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au),
                        u2f(0x3dcccccdu));
        vec3_sub(V(va), V(tgt), V(m + 0x10));
        const float len = vec3_length(V(va));
        bool turn = false;
        if (len < near_range && abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 8.0f && rand_mod(0x13) == 0) {
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
            turn = true;
        }
        if (!turn && lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) turn = true;
        if (turn) {
            turn_pick();
            sb(m + 0x20, 3);
            anim(2);
            sb(m + 0xbc, 0);
        } else if (near_range + u2f(0x40c00000u) < vec3_length(V(va)) && lh(pv + 0x20a) == 0) {
            sb(m + 0x20, 0x10);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
        }
        if (lbu(m + 0x53) == 5 && (lbu(m + 0x70) & 2u) != 0) moby_anim_set(m, 4, 0, frame_scale_i(3));
        break;
    }
    case 3: {
        const float ratio = fpu_div(abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)), vec2_distance(V(m + 0x10), V(tgt)));
        waypoint(tgt);
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(wp) - lwc1(m + 0x10), lwc1(wp + 4) - lwc1(m + 0x14)) + lwc1(pv + 0x1f4), u2f(0x3d4ccccdu),
                        u2f(0x3e99999au), u2f(0x3e4ccccdu));
        ahead();
        const uint32_t flags = walker_record_step(1.0f, m, pv + 0x180, va, wp);
        if (t32(pv + 0x210) != 0) {
            const float f = rand_range_f(u2f(0x41f00000u), u2f(0x42b40000u));
            sw(pv + 0x210, static_cast<uint32_t>(frame_scale_i(float_to_int(f))));
            swc1(pv + 0x1f4, -lwc1(pv + 0x1f4));
        }
        vec3_sub(V(diff), V(tgt), V(pv + 0x1d0));
        if (vec2_distance(V(m + 0x10), V(tgt)) < u2f(0x3fc00000u) && (flags & 2u) == 0) {
            sb(m + 0x20, 4);
            anim(3);
        } else if (1.0f < ratio || (flags & 2u) != 0) {
            swc1(m + 0x10, lwc1(pos0));
            swc1(m + 0x14, lwc1(pos0 + 4));
            sb(m + 0x20, 0xc);
            anim(5);
        } else if (vec3_length(V(diff)) < near_range + 4.0f && abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 8.0f) {
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
        } else if (lbu(m + 0xbc) != 1) {
            sb(m + 0x20, 5);
        }
        water_end();
        break;
    }
    case 4: {
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(tgt) - lwc1(m + 0x10), lwc1(tgt + 4) - lwc1(m + 0x14)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        u2f(0x3e4ccccdu));
        bool shoot = lbu(m + 0x52) == lbu(m + 0x53) && moby_anim_time(m) == u2f(0x41500000u) && abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 0.5f &&
                     vec2_distance(V(m + 0x10), V(tgt)) < 2.0f;
        if (shoot) shoot = angle_dist(atan2_f(lwc1(tgt) - lwc1(m + 0x10), lwc1(tgt + 4) - lwc1(m + 0x14)), lwc1(m + 0x48)) < u2f(0x3e860a92u);
        if (shoot) {
            swc1(va, cos_vu(lwc1(m + 0x48)) * u2f(0x3e4ccccdu));
            swc1(va + 4, sin_vu(lwc1(m + 0x48)) * u2f(0x3e4ccccdu));
            sw(va + 8, 0);
            lq_sq(wp, tgt);
            swc1(wp + 8, lwc1(wp + 8) + u2f(0x3f400000u));
            if (stale != 0) sub_00259bc8_v(stale, m, 1, wp, V(va), 1.0f);
        }
        if ((lbu(m + 0x70) & 2u) != 0 && u2f(0x3fc00000u) < vec2_distance(V(m + 0x10), V(tgt))) {
            sb(m + 0x20, 3);
            anim(2);
        }
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const float vz = lwc1(pv + 0x198) - lwc1(0x0015ed70) * u2f(0x411ccccdu);
        swc1(pv + 0x198, vz);
        const float z = lwc1(m + 0x18) + vz;
        swc1(m + 0x18, z);
        if (z < floor) {
            sw(pv + 0x198, 0);
            swc1(m + 0x18, floor);
        }
        break;
    }
    case 5: {
        const uint32_t home = pv + 0x1d0;
        waypoint(home);
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(wp) - lwc1(m + 0x10), lwc1(wp + 4) - lwc1(m + 0x14)), u2f(0x3ca3d70au), u2f(0x3e99999au),
                        u2f(0x3dcccccdu));
        ahead();
        const uint32_t flags = walker_record_step(1.0f, m, pv + 0x180, va, wp);
        if (vec2_distance(V(m + 0x10), V(home)) < 2.0f) {
            sb(m + 0x20, 1);
            anim(4);
        }
        vec3_sub(V(diff), V(tgt), V(home));
        bool go3;
        if (vec3_length(V(diff)) < lwc1(pv + 0x1fc)) {
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
            go3 = true;
        } else {
            go3 = lbu(m + 0xbc) == 1;
        }
        if (go3) {
            sb(m + 0x20, 3);
            anim(2);
        } else if ((flags & 2u) != 0) {
            swc1(m + 0x10, lwc1(pos0));
            swc1(m + 0x14, lwc1(pos0 + 4));
            sb(m + 0x20, 0xc);
            anim(5);
        }
        water_end();
        break;
    }
    case 6:
    case 0xb: {
        if ((enemy_physics(m, pv + 0x120) & 1u) != 0) {
            sb(m + 0x20, 3);
            if (state == 0xb) {
                if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
                swc1(pv + 0x1f4, lwc1(pv + 0x1f8) * u2f(0x3c8efa35u));
                sw(pv + 0x210, static_cast<uint32_t>(frame_scale_i(float_to_int(rand_range_f(u2f(0x41f00000u), u2f(0x42b40000u))))));
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 8, frame_scale_i(3));
            } else if (lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, frame_scale_i(3));
            }
            ee_stack_free(0x160);
            return;
        }
        if (lwc1(m + 0x18) < 5.0f) {
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
            ee_stack_free(0x160);
            return;
        }
        break;
    }
    case 7:
        if (carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, 0xe);
            sw(pv + 0x228, static_cast<uint32_t>(frame_scale_i(0xb4)));
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            sh(pv + 0xc8, 0);
        }
        break;
    case 8: {
        vec3_sub(V(va), V(tgt), V(m + 0x10));
        if (vec3_length(V(va)) < near_range - 4.0f) {
            if (abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 8.0f && rand_mod(0x13) == 0) {
                swc1(m + 0x18, lwc1(m + 0x18) + 1.0f);
                sb(m + 0x20, 2);
                anim(5);
            }
        }
        break;
    }
    case 0xa: {
        const uint32_t home = pv + 0x1d0;
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(home) - lwc1(m + 0x10), lwc1(home + 4) - lwc1(m + 0x14)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        u2f(0x3e4ccccdu));
        vec3_sub(V(va), V(tgt), V(home));
        bool go;
        if (vec3_length(V(va)) < near_range) go = abs_f(lwc1(pv + 0x1d8) - lwc1(tgt + 8)) < 8.0f || lbu(m + 0xbc) == 2;
        else go = lbu(m + 0xbc) == 2;
        if (!go) break;
        if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 2);
        if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, frame_scale_i(3));
        const float dt = lwc1(0x0015ed6c);
        swc1(pv + 0x134, 0.0f);
        const float b = lwc1(L1_ADDR(0x00161a6c));
        swc1(pv + 0x130, b);
        swc1(pv + 0x138, lwc1(L1_ADDR(0x00161a68)) * dt);
        float t_out = 0.0f;
        swc1(pv + 0x13c, sub_0025abf0(m + 0x10, home, &t_out, lwc1(L1_ADDR(0x00161a68)) * dt, -b));
        sw(pv + 0x144, 0xd);
        sb(pv + 0x15d, 0);
        move_state_start(m, pv + 0x120, 6, 1, 0, lwc1(m + 0x48));
        swc1(pv + 0x170, u2f(0x40a00000u));
        swc1(pv + 0x174, u2f(0x41200000u));
        sb(m + 0x20, 0xb);
        break;
    }
    case 0xc: {
        const float ratio = fpu_div(abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)), vec2_distance(V(m + 0x10), V(tgt)));
        const uint32_t home = pv + 0x1d0;
        waypoint(home);
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(wp) - lwc1(m + 0x10), lwc1(wp + 4) - lwc1(m + 0x14)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        u2f(0x3e4ccccdu));
        ahead();
        const uint32_t flags = walker_record_step(1.0f, m, pv + 0x180, va, wp);
        vec3_sub(V(diff), V(tgt), V(home));
        if (vec2_distance(V(m + 0x10), V(tgt)) < u2f(0x3fc00000u) && ratio <= 1.0f) {
            sb(m + 0x20, 4);
            anim(3);
        } else if (ratio <= 1.0f && (flags & 2u) == 0) {
            sb(m + 0x20, 5);
            anim(2);
        } else if (near_range + u2f(0x40c00000u) < vec2_distance(V(m + 0x10), V(tgt))) {
            sb(m + 0x20, 0x10);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
        } else if ((lbu(m + 0x70) & 2u) != 0) {
            rand_mod(4);
            const uint32_t set = rand_mod(4) != 0 ? 4u : 5u;
            moby_anim_set(m, set, 0, frame_scale_i(3));
        }
        water_end();
        break;
    }
    case 0xd: {
        if (lbu(m + 0x52) != 2 && lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(3));
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(va, c * (lwc1(pv + 0x204) * lwc1(0x0015ed6c)));
        const float sn = sin_vu(lwc1(m + 0x48));
        sw(va + 8, 0);
        swc1(va + 4, sn * (lwc1(pv + 0x204) * lwc1(0x0015ed6c)));
        vec3_add(V(wp), V(m + 0x10), V(va));
        for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
            if (o == m) continue;
            if (static_cast<int8_t>(lbu(o + 0x20)) < 0) continue;
            const int16_t cls = lh(o + 0xa6);
            if (cls != 0 && cls != 0x241) continue;
            if (!(vec2_distance(V(wp), V(o + 0x10)) < 2.0f)) continue;
            const float z = lwc1(m + 0x18);
            const float a = angle_sub(lwc1(m + 0x48), atan2_f(lwc1(o + 0x10) - lwc1(m + 0x10), lwc1(o + 0x14) - lwc1(m + 0x14)));
            float turn = lwc1(0x0015ed6c) * u2f(0x3fc90fdbu);
            if (!(0.0f < a)) turn = -turn;
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), turn));
            vec3_sub(V(wp), V(m + 0x10), V(o + 0x10));
            vec3_set_length(V(wp), V(wp), 2.0f);
            vec3_add(V(wp), V(wp), V(o + 0x10));
            swc1(wp + 8, z);
        }
        lq_sq(m + 0x10, wp);
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const float vz = lwc1(pv + 0x198) - lwc1(0x0015ed70) * u2f(0x411ccccdu);
        swc1(pv + 0x198, vz);
        const float z = lwc1(m + 0x18) + vz;
        swc1(m + 0x18, z);
        if (z < floor) {
            sw(pv + 0x198, 0);
            swc1(m + 0x18, floor);
        }
        const uint32_t follow = lwu(pv + 0x218);
        if (follow != 0) {
            const float d = vec2_distance(V(m + 0x10), V(follow + 0x10));
            if (!(int_to_float(lh(pv + 0x216)) < d)) {
                sb(m + 0xbc, 0);
                break;
            }
        }
        sb(m + 0x20, 5);
        anim(2);
        sb(m + 0xbc, 0);
        break;
    }
    case 0xe: {
        vec3_sub(V(va), V(tgt), V(m + 0x10));
        bool gone = false;
        if (t32(pv + 0x228) != 0) {
            if (vec3_length(V(va)) < near_range + 4.0f && abs_f(lwc1(m + 0x18) - lwc1(tgt + 8)) < 8.0f && rand_mod(0x13) == 0) {
                sb(m + 0x20, 0x11);
                gone = true;
            }
        }
        if (gone) break;
        if (lw(pv + 0x228) == 0 && lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) {
            turn_pick();
            sb(m + 0x20, 0x11);
            vec_clear(V(pv + 0x40));
            sb(m + 0xbc, 0);
            break;
        }
        if (t32(pv + 0x20c) == 0) break;
        sw(pv + 0x20c, static_cast<uint32_t>(frame_scale_i(0x78)));
        const float r = rand_range_f(0.0f, u2f(0x40400000u));
        const float a = rand_angle();
        swc1(wp, cos_vu(a) * r);
        swc1(wp + 4, sin_vu(a) * r);
        swc1(wp + 8, 0.0f);
        swc1(wp + 8, rand_range_f(u2f(0x40a00000u), u2f(0x40e00000u)));
        vec3_add(V(pv + 0x1e0), V(wp), V(pv + 0x1d0));
        sb(m + 0x20, 0xf);
        break;
    }
    case 0xf: {
        yaw_turn_damped(m, pv + 0x1f0, atan2_f(lwc1(pv + 0x1e0) - lwc1(m + 0x10), lwc1(pv + 0x1e4) - lwc1(m + 0x14)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        u2f(0x3e4ccccdu));
        vec3_sub(V(pv + 0x40), V(pv + 0x1e0), V(m + 0x10));
        vec3_clamp_length(pv + 0x40, lwc1(0x0015ed6c) * u2f(0x40800000u));
        vec3_add(V(va), V(pv + 0x40), V(m + 0x10));
        if (sphere_collide_v(V(va), u2f(0x3ecccccdu), 0, m) != 0) {
            vec_clear(V(pv + 0x40));
            sb(m + 0x20, 0xe);
        } else {
            lq_sq(m + 0x10, va);
        }
        if (vec3_length(V(pv + 0x40)) == 0.0f) sb(m + 0x20, 0xe);
        break;
    }
    case 0x10: {
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        approach_f(pv + 0x48, lwc1(0x0015ed6c) * u2f(0x41000000u), lwc1(0x0015ed70) * u2f(0x412ccccdu));
        const float z = lwc1(m + 0x18) + lwc1(pv + 0x48);
        swc1(m + 0x18, z);
        if (floor + u2f(0x40c00000u) <= z) {
            swc1(m + 0x18, floor + u2f(0x40c00000u));
            sb(m + 0x20, 0xe);
            sw(pv + 0x228, static_cast<uint32_t>(frame_scale_i(0xb4)));
            vec_clear(V(pv + 0x40));
        }
        break;
    }
    case 0x11: {
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        approach_f(pv + 0x48, -(lwc1(0x0015ed6c) * u2f(0x41000000u)), lwc1(0x0015ed70) * u2f(0x412ccccdu));
        swc1(m + 0x18, lwc1(m + 0x18) + lwc1(pv + 0x48));
        if (lbu(m + 0x53) != 1) {
            const float rise = lwc1(m + 0x18) - floor;
            const float lim = lwc1(0x0015ed6c) * u2f(0x41000000u) * frame_scale_f(u2f(0x42340000u));
            if (rise <= lim && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
        }
        if (floor < lwc1(m + 0x18)) break;
        swc1(m + 0x18, floor);
        if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
        swc1(pv + 0x1f4, lwc1(pv + 0x1f8) * u2f(0x3c8efa35u));
        sw(pv + 0x210, static_cast<uint32_t>(frame_scale_i(float_to_int(rand_range_f(u2f(0x41f00000u), u2f(0x42b40000u))))));
        sb(m + 0x20, 3);
        vec_clear(V(pv + 0x40));
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(3));
        break;
    }
    case 0x63: {
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        const uint32_t r = enemy_physics(m, pv + 0x120);
        if ((r & 0x140u) != 0) {
            const int32_t slot = cooldown_slot_claim_l1(L1_ADDR(0x00161a80), 3, float_to_int(int_to_float(frame_scale_i(1)) * u2f(0x42700000u)));
            if (slot >= 0) explosion_burst(m, m + 0x10, 4, 0.5f, u2f(0x41500000u));
        } else if (!(lwc1(m + 0x18) < 0.0f)) {
            break;
        }
        const uint32_t z = OV(0x0015f580);
        debris_spawn(m, 0x6d3, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        debris_spawn(m, 0x6d4, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        debris_spawn(m, 0x6d5, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        moby_retire(m);
        ee_stack_free(0x160);
        return;
    }
    default:
        break;
    }
    (void)face;
    color_pulse_update(m, pv + 0x110);
    moby_floor_band(m);
    ee_stack_free(0x160);
}

}  // namespace rac1
