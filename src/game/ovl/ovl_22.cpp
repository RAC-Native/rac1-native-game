// Ported overlay code 0x00220000..0x0022ffff.
#include <game/ovl/ovl_common.h>
#include <game/ovl/hero_l1.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

// orig: 0x00221b68 — push the hero off the edge point PLAYER+0x210: arm the push timer
// PLAYER+0x1ea when standing still at a ledge in a ground substate, then spring the push speed
// PLAYER+0x244 and set the extra velocity PLAYER+0xf0 away from the point.
void sub_00221b68() {
    const float k = lwc1(0x0015ed6c);
    bool arm = lbu(g::PLAYER + 0x257) != 0 && 0.0f < lwc1(g::PLAYER + 0x2dc) && lwc1(g::PLAYER + 0x160) < k * 3.0f;
    if (arm) arm = vec3_length(V(g::PLAYER + 0xe0)) < lwc1(0x0015ed6c) * 3.0f && lwc1(g::PLAYER + 0x229c) < u2f(0x3e4ccccdu);
    if (arm) {
        const int32_t s = lw(g::PLAYER + 0x208c);
        if (s == 0 || s == 2 || s == 4 || s == 0xa || s == 1) sh(g::PLAYER + 0x1ea, static_cast<uint32_t>(frame_scale_i(5)));
    }
    if (lh(g::PLAYER + 0x1ea) == 0) {
        sw(g::PLAYER + 0x244, 0);
        return;
    }
    const float d = vec2_distance(V(g::PLAYER + 0x80), V(g::PLAYER + 0x210));
    const float lim = lwc1(g::PLAYER + 0x234) * u2f(0x3f666666u);
    if (!(d < lim)) return;
    float dir = atan2_f(lwc1(g::PLAYER + 0x80) - lwc1(g::PLAYER + 0x210), lwc1(g::PLAYER + 0x84) - lwc1(g::PLAYER + 0x214));
    if (d < lwc1(g::PLAYER + 0x234) * u2f(0x3d4ccccdu)) dir = lwc1(g::PLAYER + 0x98);
    float x = d - lim;
    const float t = lwc1(0x0015ed64);
    spring_to_v(x, g::PLAYER + 0x244, 0.0f, t * u2f(0x3cf5c28fu), t * u2f(0x3e99999au), lwc1(0x0015ed6c) * u2f(0x40333333u));
    swc1(g::PLAYER + 0xf0, cos_vu(dir) * lwc1(g::PLAYER + 0x244));
    const float y = sin_vu(dir) * lwc1(g::PLAYER + 0x244);
    swc1(g::PLAYER + 0xf8, 0.0f);
    swc1(g::PLAYER + 0xf4, y);
}

// orig: 0x00227d40 — hero height smoothing: the bob offset PLAYER+0x690 (velocity +0x694)
// springs back to 0 (state 0x36) or decays, the ground height +0x69c springs towards the
// floor (+0x2f0, or +0x8a0 in substate 0x16), and the eye height +0x88 follows both.
void sub_00227d40() {
    const int32_t t = frame_scale_i(10);
    bool settle = t < lw(g::PLAYER + 0x198) || lwc1(g::PLAYER + 0x690) < 0.0f || lw(g::PLAYER + 0x2094) == 0x11;
    if (!settle) settle = lw(g::PLAYER + 0x20a0) == 0x11;
    if (settle) {
        const float k1 = lwc1(0x0015ed64);
        if (lw(g::HERO_STATE) == 0x36) {
            spring_to(g::PLAYER + 0x690, g::PLAYER + 0x8fc, 0.0f, k1 * u2f(0x3cf5c28fu), k1 * u2f(0x3e99999au),
                      lwc1(0x0015ed6c) * 1.5f);
            swc1(g::PLAYER + 0x694, 0.0f);
        } else {
            const float a = k1 * u2f(0x3ba3d70au);
            const float b = lwc1(0x0015ed60) * u2f(0x3d3851ecu);
            const float v = lwc1(g::PLAYER + 0x694);
            const float x = lwc1(g::PLAYER + 0x690);
            swc1(g::PLAYER + 0x694, v - (a * x + b * v));
            if (abs_f(x) < u2f(0x3a83126fu) && lwc1(g::PLAYER + 0x694) < u2f(0x38d1b717u)) sw(g::PLAYER + 0x694, 0);
        }
    }
    const float n = lwc1(g::PLAYER + 0x690) + lwc1(g::PLAYER + 0x694);
    const float v = lwc1(g::PLAYER + 0x694);
    swc1(g::PLAYER + 0x690, n);
    if (lw(g::HERO_STATE) == 0x36 && 0.0f < n) {
        if (0.0f < v) swc1(g::PLAYER + 0x694, 0.0f);
        approach_f(g::PLAYER + 0x690, 0.0f, lwc1(0x0015ed6c) * u2f(0x3f333333u));
    }
    if (lw(g::HERO_STATE) != 0x7f) {
        const float floor = lw(g::PLAYER + 0x208c) == 0x16 ? lwc1(g::PLAYER + 0x8a0) : lwc1(g::PLAYER + 0x2f0);
        const float k = lwc1(0x0015ed64);
        spring_to(g::PLAYER + 0x69c, g::PLAYER + 0x6a0, floor, k * u2f(0x3cdd2f1bu), k * u2f(0x3e99999au), 0.0f);
    }
    swc1(g::PLAYER + 0x88, (lwc1(g::PLAYER + 0x69c) + u2f(0xbdf5c28fu)) + lwc1(g::PLAYER + 0x690));
}

// orig: 0x00228fa8 — release the hero's held moby PLAYER+0x1614 and clear the 0x41 flags
// of the two attached mobies PLAYER+0x1180/0x1184.
void hero_release_attached() {
    if (lw(g::PLAYER + 0x1614) != 0) {
        moby_retire(lw(g::PLAYER + 0x1614));
        sw(g::PLAYER + 0x1614, 0);
    }
    for (uint32_t off = 0x1180; off <= 0x1184; off += 4) {
        const uint32_t m = lw(g::PLAYER + off);
        if (m != 0) sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
    }
}

// orig: 0x00221df8 — stick direction relative to the hero's facing as a quadrant: 3 = back,
// 1 = right (-pi/2), 0 = left (+pi/2), 2 = forward (within pi/4 of each).
int32_t hero_stick_quadrant() {
    hero_stick_move(0, 1.0f);
    const float d = angle_sub(lwc1(g::PLAYER + 0x180), lwc1(g::PLAYER + 0x98));
    const float q = u2f(0x3f490fdbu);
    if (angle_dist(d, u2f(0x40490fdbu)) < q) return 3;
    if (angle_dist(d, u2f(0xbfc90fdbu)) < q) return 1;
    if (angle_dist(d, u2f(0x3fc90fdbu)) < q) return 0;
    return 2;
}

// orig: 0x00221d98 — frames for the stick-direction turn: 12 (frame-scaled) unless the stick
// points forward (quadrant 2: 0).
int32_t hero_stick_turn_frames() {
    int32_t n = frame_scale_i(0xc);
    const int32_t q = hero_stick_quadrant();
    if (q == 2) return 0;
    if (q == 3) n = frame_scale_i(0xc);
    return n;
}


// orig: 0x00228c18 — the hero's gait (P+0x2088: 0 walk, 1 run, by speed +0x194 against the
// dt-scaled band table 0x17bdb8, after 4 frames on the ground) and its animation speed +0xa90;
// on a gait change the run cycle is re-synced (set 3 + gait, frame mapped across the cycles plus
// the 0x15f6c8 offset). Swimming / other modes (+0x20a4 3 / 1) only set the speed.
void hero_run_gait_update() {
    const uint32_t P = g::PLAYER;
    constexpr OvConst<0x0017bdb8u> BAND{};
    const uint32_t mode = lbu(P + 0x20a4);
    if (mode == 3 || mode == 1) {
        const float k = mode == 3 ? 54.0f : 90.0f, lo = mode == 3 ? u2f(0x3f333333u) : 0.5f;
        const float v = lwc1(P + 0x160) * k;
        swc1(P + 0xa90, v);
        if (v < lo) swc1(P + 0xa90, lo);
        return;
    }
    auto settled = [P] { return frame_scale_i(4) < lw(P + 0x1a0) && lh(P + 0x3bc) == 0; };
    if (settled()) {
        const float k = lwc1(0x0015ed6c);
        const float spd = lwc1(P + 0x194);
        const uint32_t g0 = lwu(P + 0x2088);
        const bool slower = spd < lwc1(BAND + g0 * 16) * k;
        if ((slower || lwc1(BAND + g0 * 16 + 4) * k < spd) && settled()) {
            const int32_t old = lw(P + 0x2088);
            const float k2 = lwc1(0x0015ed6c);
            const float s2 = lwc1(P + 0x194);
            if (slower) {
                if (s2 < lwc1(BAND + static_cast<uint32_t>(old) * 16) * k2) {
                    int32_t g = lw(P + 0x2088);
                    do {
                        g -= 1;
                        sw(P + 0x2088, static_cast<uint32_t>(g));
                    } while (s2 < lwc1(BAND + static_cast<uint32_t>(g) * 16) * k2);
                }
            } else if (lwc1(BAND + 4 + static_cast<uint32_t>(old) * 16) * k2 < s2) {
                int32_t g = old + 1;
                while (lwc1(BAND + 4 + static_cast<uint32_t>(g) * 16) * k2 < s2) ++g;
                sw(P + 0x2088, static_cast<uint32_t>(g));
            }
            if (lw(P + 0x2088) < 0) sw(P + 0x2088, 0);
            if (!(lw(P + 0x2088) < 2)) sw(P + 0x2088, 1);
            const int32_t now = lw(P + 0x2088);
            if (old != now) {
                const uint32_t hm = lwu(P + 0x2080);
                const uint32_t sets = lwu(hm + 0x24) + 0x48;
                const int32_t frame = lbu(hm + 0x51);
                const int32_t ncnt = lbu(lwu(sets + static_cast<uint32_t>(now + 3) * 4) + 0x10);
                const int32_t ocnt = lbu(lwu(sets + static_cast<uint32_t>(old + 3) * 4) + 0x10);
                const int32_t off = lw(OV(0x0015f6c8) + static_cast<uint32_t>(now) * 4 + static_cast<uint32_t>(old) * 8);
                const int32_t f = ee_rem(ee_div(frame * ncnt, ocnt) + off, ncnt);
                hero_anim_set(static_cast<uint32_t>(now + 3), static_cast<uint32_t>(f), int_to_float(frame_scale_i(8)));
            }
        }
    }
    const int32_t g = lw(P + 0x2088);
    if (g != 0 && g != 1) return;
    const float v = lwc1(P + 0x194) * (g == 0 ? 157.0f : 14.0f);
    swc1(P + 0xa90, v);
    if (v < u2f(0x3f19999au)) swc1(P + 0xa90, u2f(0x3f19999au));
    const float hi = g == 0 ? 4.0f : u2f(0x400ccccdu);
    if (hi < lwc1(P + 0xa90)) swc1(P + 0xa90, hi);
}

// orig: 0x00229910 — floor just ahead of the hero: only when its speed PLAYER+0x164 is at least
// radius / frame_scale_i(20) and the velocity heading (atan2 of +0x104, +0x100) is within 30
// degrees of the facing +0x98. Probes down from 0.47 above to 1 below the point 0.75 radii
// ahead (ignoring PLAYER+0x2080); 1 when the floor there is within (-1, 0.37) of the hero's z.
int32_t hero_floor_ahead() {
    const uint32_t P = g::PLAYER;
    const float min_speed = fpu_div(lwc1(P + 0x234), static_cast<float>(frame_scale_i(0x14)));
    if (lwc1(P + 0x164) < min_speed) return 0;
    const float d = angle_dist(atan2_f(lwc1(P + 0x100), lwc1(P + 0x104)), lwc1(P + 0x98));
    if (u2f(0x3f060a92u) < d) return 0;
    Vec4 a = V(P + 0x80);
    const float k = u2f(0x3f400000u);
    a.x = a.x + cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x234) * k;
    a.y = a.y + sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x234) * k;
    Vec4 b = a;
    a.z = a.z + u2f(0x3ef0a3d7u);
    b.z = b.z - 1.0f;
    if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) == 0) return 0;
    const float dz = lwc1(OV(0x00173e68)) - lwc1(P + 0x88);
    if (!(dz < u2f(0x3ebd70a4u))) return 0;
    return -1.0f < dz ? 1 : 0;
}

// Pick the best-scoring (target_score, lowest) moby of class 0x323 for the
// hero: active pvars (+0 set), not hidden (+0x34 bit 0), at most 3 below the hero, within the
// pvars range +0x20 (0: any; target range 16 then), +10 when +0x31 is 0, not when the hero is
// inside the pvars box +0x1c. The yaw/pitch limits are pi/4 (state 0x2d: 1.22 / 1.13; states
// 1 and 0x1e: 0.087). PLAYER+0x990 = the pick; with a clear line (flag 2) from 0.5 above the
// hero to it, PLAYER+0x998 = its score and PLAYER+0x99e = 1.
// orig: 0x00221ee0
void hero_pick_target_323() {
    const uint32_t P = g::PLAYER;
    uint32_t best = 0;
    float best_score = u2f(0x4cbebc20u);
    sh(P + 0x99e, 0);
    for (uint32_t m = lwu(OV(0x0015ffe4)); m != 0; m = lwu(m + 0x28)) {
        if (lh(m + 0xa6) != 0x323) continue;
        const uint32_t pv = lwu(m + 0x78);
        if (lw(pv) == 0) continue;
        if ((lhu(m + 0x34) & 1) != 0) continue;
        if (3.0f < lwc1(P + 0x88) - lwc1(m + 0x18)) continue;
        if (lwc1(pv + 0x20) != 0.0f && lwc1(pv + 0x20) < vec3_distance(V(P + 0x80), V(m + 0x10))) continue;
        const int32_t st = lw(P + 0x2084);
        float max_yaw = st == 0x2d ? u2f(0x3f9c61aau) : u2f(0x3f490fdbu);
        float max_pitch = st == 0x2d ? u2f(0x3f91361eu) : u2f(0x3f490fdbu);
        if (st == 1 || st == 0x1e) {
            max_pitch = u2f(0x3db2b8c2u);
            max_yaw = max_pitch;
        }
        const float range = lwc1(pv + 0x20) == 0.0f ? 16.0f : lwc1(pv + 0x20);
        int32_t rejected = 0;
        float score = target_score(m, &rejected, lwc1(P + 0x98), range, max_yaw, max_pitch);
        if (rejected != 0) continue;
        if (lbu(m + 0x31) == 0) score = score + 10.0f;
        const int32_t box = lw(pv + 0x1c);
        if (box >= 0 && point_in_box(P + 0x80, box) != 0) continue;
        if (score < best_score) {
            best_score = score;
            best = m;
        }
    }
    sw(P + 0x990, best);
    if (best == 0) return;
    Vec4 a = V(P + 0x80);
    a.z = a.z + 0.5f;
    if (segment_collide_v(a, V(best + 0x10), 2, 0, nullptr) != 0) return;
    swc1(P + 0x998, best_score);
    sh(P + 0x99e, 1);
}

// Like hero_pick_target_323 for class 0x2f6 with inactive pvars (+0 clear),
// no height limit, target range 30, limits 0.52 / -1 (states 1/0x1e: 0.087), yaw +0x57c in state
// 0xf, and not within 4 (xy) of the hero. State 0xd keeps the last result. PLAYER+0x968 (the
// return value) is 1 when the pick has a clear line; then PLAYER+0x964 = pick, +0x978 / +0x980
// = its pvars +8 / +4, +0x984 = score.
// orig: 0x00222158
int32_t hero_pick_target_2f6() {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0x208c) == 0xd) return lw(P + 0x968);
    uint32_t best = 0;
    float best_score = u2f(0x4cbebc20u);
    sw(P + 0x968, 0);
    for (uint32_t m = lwu(OV(0x0015ffe4)); m != 0; m = lwu(m + 0x28)) {
        if (lh(m + 0xa6) != 0x2f6) continue;
        if ((lhu(m + 0x34) & 1) != 0) continue;
        const uint32_t pv = lwu(m + 0x78);
        if (lw(pv) != 0) continue;
        if (lwc1(pv + 0x20) != 0.0f && lwc1(pv + 0x20) < vec3_distance(V(P + 0x80), V(m + 0x10))) continue;
        const int32_t st = lw(P + 0x2084);
        float max_yaw = u2f(0x3f060a92u);
        float max_pitch = -1.0f;
        if (st == 1 || st == 0x1e) {
            max_pitch = u2f(0x3db2b8c2u);
            max_yaw = max_pitch;
        }
        const float yaw = lw(P + 0x208c) == 0xf ? lwc1(P + 0x57c) : lwc1(P + 0x98);
        const float range = lwc1(pv + 0x20) == 0.0f ? 30.0f : lwc1(pv + 0x20);
        int32_t rejected = 0;
        float score = target_score(m, &rejected, yaw, range, max_yaw, max_pitch);
        if (rejected != 0) continue;
        if (vec2_distance(V(P + 0x80), V(m + 0x10)) < 4.0f) continue;
        if (lbu(m + 0x31) == 0) score = score + 10.0f;
        const int32_t box = lw(pv + 0x1c);
        if (box >= 0 && point_in_box(P + 0x80, box) != 0) continue;
        if (score < best_score) {
            best_score = score;
            best = m;
        }
    }
    if (best == 0) return lw(P + 0x968);
    sw(P + 0x964, best);
    Vec4 a = V(P + 0x80);
    a.z = a.z + 0.5f;
    const uint32_t pv = lwu(best + 0x78);
    sw(P + 0x978, lwu(pv + 8));
    swc1(P + 0x984, best_score);
    swc1(P + 0x980, lwc1(pv + 4));
    if (segment_collide_v(a, V(best + 0x10), 2, 0, nullptr) == 0) sw(P + 0x968, 1);
    return lw(P + 0x968);
}

// Random idle event: candidates are the 5 records (0x70 bytes) at 0x179890 that are not busy
// (+0x60), have a period +0x58 (seconds), match the slot-0 item (+0x50, -1: any) and, with
// +0x44, PLAYER+0x2fc clear; record 3 only on level 0xc, without item 0x17 and with no
// obstacle (flag 2, ignoring the hero moby) 8 straight up from PLAYER+0xd0; record 4 only with
// PLAYER+0x2094 == 1, +0x198 <= frame_scale_i(50) and +0x22a8 < 2. One is drawn with weight
// 1 / int(frame_scale_f(period) * 60) — or none, with the rest of 1 — into *out (returns 1).
// With PLAYER+0x22a8 == 1: *out = 4, and 1 one time in three when *(0x179ab0) is clear and
// PLAYER+0xa98 has bit 1. Nothing while PLAYER+0x1e8 is set.
// orig: 0x00229010
int32_t hero_idle_event_pick(int32_t* out) {
    const uint32_t P = g::PLAYER, TAB = OV(0x00179890);
    if (lh(P + 0x1e8) != 0) return 0;
    if (lw(P + 0x22a8) == 1) {
        *out = 4;
        if (lw(OV(0x00179ab0)) != 0) return 0;
        if ((lw(P + 0xa98) & 2) == 0) return 0;
        return rand_range_f(0.0f, 1.0f) < u2f(0x3ea8f5c3u) ? 1 : 0;
    }
    int32_t list[5];
    int32_t n = 0;
    for (int32_t i = 0; i < 5; ++i) {
        const uint32_t e = TAB + static_cast<uint32_t>(i) * 0x70;
        if (lw(e + 0x60) != 0) continue;
        if (lwc1(e + 0x58) == 0.0f) continue;
        const int32_t item = sub_0020d498(0);
        const int32_t want = lw(e + 0x50);
        if (want != -1 && want != item) continue;
        if (lw(e + 0x44) != 0 && lw(P + 0x2fc) != 0) continue;
        if (i == 3) {
            if (lw(0x0015ed84) != 0xc) continue;
            if (sub_0020d498(0) == 0x17) continue;
            const Vec4 a = V(P + 0xd0);
            Vec4 b = a;
            b.z = b.z + 8.0f;
            if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) != 0) continue;
        }
        if (i == 4) {
            if (lw(P + 0x2094) != 1) continue;
            if (frame_scale_i(0x32) < lw(P + 0x198)) continue;
            if (!(lw(P + 0x22a8) < 2)) continue;
        }
        list[n++] = i;
    }
    if (n == 0) return 0;
    auto weight = [TAB](int32_t i) {
        const float t = frame_scale_f(lwc1(TAB + static_cast<uint32_t>(i) * 0x70 + 0x58)) * 60.0f;
        return fpu_div(1.0f, static_cast<float>(float_to_int(t)));
    };
    float total = 0.0f;
    for (int32_t j = 0; j < n; ++j) total = total + weight(list[j]);
    const float r = rand_range_f(0.0f, 1.0f);
    if (!(r < total)) return 0;
    for (int32_t j = 0; j < n; ++j) {
        *out = list[j];
        total = total - weight(list[j]);
        if (total < r) return 1;
    }
    return 1;
}

// Per-frame hero motion and its slow probes: hero_move_integrate (unless +0x20ad); every 3rd
// frame (*(0x15f5cc)) a wall probe at height 0.7 out to 4 (distance +0x248, wall pitch +0x250,
// +0x255 = a moby wall, +0x254 = of class 500..540); every 5th, while airborne (+0x300), the
// drop ahead: a probe from 1.1 ahead / 1 up down to 20 below (not below z 0.5, flag 2) gives the
// depth +0x258 (20 when nothing; nothing done within 3), and the first free spot for the hero's
// sphere (flag 2) stepping 0.1 from 0.7 to 1.6 ahead sets +0x260 = 1 and +0x25c = radius + step.
// orig: 0x00221310
void hero_motion_update() {
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lbu(P + 0x20ad) == 0) hero_move_integrate();
    if (lw(OV(0x0015f5cc)) % 3 == 0) {
        swc1(P + 0x248, 4.0f);
        sb(P + 0x254, 0);
        sb(P + 0x255, 0);
        if (hero_wall_probe(P + 0x248, u2f(0x3f333333u), 4.0f) != 0) {
            swc1(P + 0x250, atan2_f(lwc1(OUTB + 0x48), vec2_length(V(OUTB + 0x40))));
            const uint32_t m = lwu(OUTB + 0x18);
            if (m != 0) {
                sb(P + 0x255, 1);
                if (moby_in_range_class_500(m) != 0) sb(P + 0x254, 1);
            }
        }
    }
    [&] {
        if (lw(OV(0x0015f5cc)) % 5 != 0) return;
        sw(P + 0x260, 0);
        sw(P + 0x258, 0);
        if (lw(P + 0x300) == 0) return;
        const float a0 = lwc1(P + 0x258);
        Vec4 top, bot;
        sub_00233660_v(top, u2f(0x3f8ccccdu), a0, 1.0f);
        sub_00233660_v(bot, u2f(0x3f8ccccdu), a0, -20.0f);
        if (bot.z < 0.5f) bot.z = 0.5f;
        float depth = 20.0f;
        const int32_t hit = segment_collide_v(top, bot, 2, lwu(P + 0x2080), nullptr);
        if (hit != 0) {
            depth = vec3_distance(top, V(OUTB + 0x20));
            if (!(3.0f < depth)) return;
        }
        for (float x = u2f(0xbecccccdu);;) {
            Vec4 s;
            sub_00233660_v(s, x + u2f(0x3f8ccccdu), a0, a0);
            if (sphere_collide_v(s, lwc1(P + 0x234), 2, 0) == 0) {
                sw(P + 0x260, 1);
                swc1(P + 0x258, hit == 0 ? 20.0f : depth);
                const float r = lwc1(P + 0x234) + x;
                swc1(P + 0x25c, r);
                if (r < a0) sw(P + 0x25c, 0);
                return;
            }
            x = x + u2f(0x3dcccccdu);
            if (!(x < 0.5f)) return;
        }
    }();
    if (hero_has_hoverboard()) hero_hoverboard_moby_sync();
}

}  // namespace rac1
