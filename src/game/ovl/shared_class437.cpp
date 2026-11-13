// Class 437 of Novalis (level 1), Aridia (2) and Eudora (4): an older build of the Sandmouse (class 1818,
// shared_sandmouse.cpp). One port of the three levels' copies, the same code relinked (compared word by word: only
// relocated words differ). Level-own words: the class's tunings (per level below; the Sandmouse's layout) and the walker
// record, the walker's step, the burrow's dent, the dust and the floor's light, which the Sandmouse's levels dispatch
// (shared_sandmouse.h); the engine's (OV): the moby list 0x15ffd8, the update list 0x15ffe4, the collision's hit 0x173e40
// (+0x18 the moby hit, +0x28 the floor's height), the collected bytes 0x1bb230, the game mode 0x15f5c4; the base ELF's
// (bare): the hero 0x13f350, the frame scales 0x15ed60 / 0x15ed64 / 0x15ed6c / 0x15ed70. Header shared_class437.h.
//
// Its pvars: +0 the goal, +0x10 the step, +0x20 / +0x30 its home's place, +0x40 the zone (a box index) the hero must be in,
// +0x48 a byte set when the hero lets a held moby go, +0x4a the frames it has been stuck, +0x4c its timer, +0x4e the shot
// timer, +0x50 its house (a moby index; -1 none), +0x60 the rest timer, +0x64 the shots fired, +0x68 its target. The
// tunings: +0 the shot's interval, +4 the target range, +8 / +0xc the aim's yaw and pitch reach (degrees), +0x10 the shot's
// start ahead, +0x14 a target's height (with no pvars), +0x18 the rest, +0x1c the shots it fires, +0x20 the height above
// the floor it walks at, +0x24 the out time, +0x28 / +0x2c the dust's count, +0x30 the burrow's size, +0x34 / +0x38 the
// dust's size, +0x3c its alpha, +0x40 its spread, +0x44 .. +0x4c the turn toward a target.
#include <game/ovl/shared_class437.h>

#include <game/math/vecmath.h>
#include <game/lib/libc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its place, +0x1130 the moby it holds, +0x1158 its mode, +0x2080 its moby)

// the class's tunings (the level's own data)
uint32_t tunings() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x00161900);
    case ARIDIA: return L2_ADDR(0x00161898);
    default: return L4_ADDR(0x00161908);
    }
}

float scale() { return lwc1(0x0015ed60); }
float frame_dt() { return lwc1(0x0015ed6c); }
float frame_sq() { return lwc1(0x0015ed70); }

void zero_quad(uint32_t a) {
    for (uint32_t k = 0; k < 0x10; k += 4) sw(a + k, 0);
}
// the moby of index `idx` in the moby list
uint32_t moby_at(uint32_t idx) { return lwu(OV(0x0015ffd8)) + (idx << 8); }
// the heading from the moby's place to the point `p`
float heading_to(uint32_t m, uint32_t p) { return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14))); }
// the turn toward `yaw` through the walker record's yaw rate (11.519173 and pi a second's acceleration and damping, 660
// degrees a second at most)
void turn_toward(uint32_t m, float yaw) {
    const float k = u2f(0x41384e88u);  // (11.519173)
    yaw_turn_damped(m, sandmouse_walker_rec() + 0x14, yaw, fpu_mul(frame_sq(), k), fpu_mul(frame_sq(), u2f(0x40490fdbu)),
                    fpu_mul(frame_dt(), k));
}
// one of its idle animations (0 .. 2) at random over `frames` frames, unless the one drawn is the one it shows
void idle_anim(uint32_t m, int32_t frames) {
    const uint32_t shown = lbu(m + 0x53);
    if (shown == static_cast<uint32_t>(ee_rem(libc_rand(), 3))) return;
    moby_anim_set(m, static_cast<uint32_t>(ee_rem(libc_rand(), 3)), 0, frame_scale_i(frames));
}
}  // namespace

// orig: L1:0x002e3208 L2:0x002cbef0 L4:0x002c7110
uint32_t class437_sink(uint32_t m, uint32_t pv) {
    const float slow = fpu_add(fpu_mul(scale(), u2f(0xbd4cccd0u)), 1.0f);  // (-0.05)
    const float drop = fpu_mul(frame_sq(), u2f(0x41ed999au));                 // (29.7)
    swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), drop));
    swc1(pv + 0x10, fpu_mul(lwc1(pv + 0x10), slow));
    swc1(pv + 0x14, fpu_mul(lwc1(pv + 0x14), slow));
    const uint32_t rec = sandmouse_walker_rec();
    return walker_move_fx(m, pv + 0x10, lw(rec), 0, u2f(0x3e99999au), lwc1(rec + 0x30));  // (0.3)
}

// orig: L1:0x002e3110 L2:0x002cbdf8 L4:0x002c7018
void class437_goal_by_hero(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t ahead = sp, side = sp + 0x10;
    vec3_set_length(V(ahead), V(lwu(HERO + 0x2080) + 0xd0), u2f(0x3f8ccccdu));  // (1.1)
    vec3_set_length(V(side), V(lwu(HERO + 0x2080) + 0xe0), 1.0f);
    vec3_add(V(pv), V(ahead), V(side));
    vec3_add(V(pv), V(pv), V(HERO + 0x80));
    if (lwu(pv + 0x68) != 0) {
        swc1(pv + 8, fpu_add(lwc1(pv + 8), 1.0f));
    } else if (walker_probe(lwc1(sandmouse_walker_rec() + 8), 2.0f, pv, m, 0) != 0) {
        swc1(pv + 8, lwc1(OV(0x00173e68)));
    }
    ee_stack_free(0x80);
}

// orig: L1:0x00263a28 L2:0x002512b8 L4:0x00241980
uint32_t moby_collision_point(uint32_t m, uint32_t joint, uint32_t kind, uint32_t point, uint32_t size, uint32_t size2) {
    const uint32_t coll = lwu(m + 0x94);
    if (coll == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t turn = sp, a = sp + 0x40, b = sp + 0x50;
    const uint32_t e = coll + (joint << 5) + 0x10;  // (the primitive: +0 its kind, +4 .. its fields, +0x10 its point)
    sw(kind, lbu(e));
    const float k = fpu_mul(lwc1(m + 0x2c), u2f(0x3a800000u));  // (the moby's scale / 1024)
    euler_to_mat4_vu(M(turn), V(m + 0x40));
    // the point (scaled, turned, placed) and its size
    auto place = [m, point, turn, k](uint32_t from) {
        lq_sq(point, from);
        vec3_scale(V(point), V(point), k);
        mat4_transform(V(point), V(point), M(turn));
        vec3_add(V(point), V(point), V(m + 0x10));
    };
    auto pose = [m] {
        const int32_t n = lh(lwu(m + 0x94) + 2);
        if (n != 0) moby_pose_matrices(m, n, 0);
    };
    uint32_t found = 1;
    const uint32_t what = lwu(kind);
    if (what < 2 || what == 3) {  // (a point of the moby: a sphere, or a capsule of its own length)
        place(e + 0x10);
        swc1(size, fpu_mul(lwc1(point + 0xc), k));
        if (lwu(kind) < 2) sw(size2, 0);
        else swc1(size2, fpu_mul(lwc1(e + 4), k));
    } else if (what == 2) {  // (at a joint of the pose, on the scratchpad)
        pose();
        place(0x70000000u + (lwu(e + 4) << 4));
        swc1(size, fpu_mul(lwc1(e + 0xc), k));
        sw(size2, 0);
    } else if (what == 4) {  // (between two joints: the lower one, the length between them)
        pose();
        vec4_scale(V(a), V(0x70000000u + (static_cast<uint32_t>(lh(e + 4)) << 4)), k);
        vec4_scale(V(b), V(0x70000000u + (static_cast<uint32_t>(lh(e + 6)) << 4)), k);
        lq_sq(point, lwc1(a + 8) < lwc1(b + 8) ? a : b);
        mat4_transform(V(point), V(point), M(turn));
        vec3_add(V(point), V(point), V(m + 0x10));
        swc1(size, fpu_mul(lwc1(e + 0xc), k));
        vec3_sub(V(b), V(b), V(a));
        swc1(size2, vec3_length(V(b)));
    } else {
        found = 0;
    }
    ee_stack_free(0x100);
    return found;
}

// orig: L1:0x002e32a8 L2:0x002cbf90 L4:0x002c71b0
uint32_t class437_find_target(uint32_t m, uint32_t from, uint32_t angles, float yaw_reach, float pitch_reach, float range,
                              float near, float near_yaw, float near_pitch) {
    uint32_t best = 0;
    float best_score = u2f(0x4e6e6b28u);  // (1e9)
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t aim = sp;
    for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        if (lh(o + 0x32) == 0) continue;
        const uint32_t word = moby_pvar_word0(o);
        if ((lhu(o + 0x34) & 0x1000) == 0) continue;
        const uint32_t cls = lwu(o + 0x24);
        if (cls == 0) continue;
        const int32_t kind = lh(cls + 0x46);
        if (kind != 5 || word == 0) continue;
        if (!(0.0f < lwc1(word))) continue;
        const float d = vec3_distance(V(from), V(o + 0x10));
        if (!(d < range)) continue;
        // the mark 0.4 above it: within the yaw reach, or near and within the near reach; the same for the pitch
        lq_sq(aim, o + 0x10);
        swc1(aim + 8, fpu_add(lwc1(aim + 8), u2f(0x3ecccccdu)));  // (0.4)
        const float yaw = atan2_f(fpu_sub(lwc1(aim), lwc1(from)), fpu_sub(lwc1(aim + 4), lwc1(from + 4)));
        const float dy = angle_dist(lwc1(angles + 8), yaw);
        float score = fpu_mul(dy, dy);
        if (!(score < fpu_mul(yaw_reach, yaw_reach))) {
            if (!(d < near) || !(score < fpu_mul(near_yaw, near_yaw))) continue;
        }
        const float pitch = atan2_f(d, fpu_sub(lwc1(aim + 8), lwc1(from + 8)));
        const float dp = angle_dist(lwc1(angles + 4), pitch);
        const float dp2 = fpu_mul(dp, dp);
        if (!(dp2 < fpu_mul(pitch_reach, pitch_reach))) {
            if (!(d < near) || !(dp2 < fpu_mul(near_pitch, near_pitch))) continue;
        }
        score = fpu_mul(score, dp2);
        score = fpu_mul(score, sqrt_f(vec3_distance(V(from), V(aim))));
        if (!(score < best_score)) continue;
        // seen: nothing between, or what is between is of its kind (not the hero)
        if (segment_collide_v(V(from), V(aim), 0, m, nullptr) != 0) {
            const uint32_t hit = lwu(OV(0x00173e40) + 0x18);
            if (hit == 0 || hit == lwu(HERO + 0x2080)) continue;
            const uint32_t hcls = lwu(hit + 0x24);
            if (hcls == 0 || lh(hcls + 0x46) != kind) continue;
        }
        best_score = score;
        best = o;
    }
    ee_stack_free(0x100);
    return best;
}

// orig: L1:0x002e35a8 L2:0x002cc290 L4:0x002c74b0
void class437_shoot(uint32_t m, uint32_t pv) {
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x4e))) == 0) return;
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t from = sp, at = sp + 0x10, ahead = sp + 0x20;
    const uint32_t t = tunings();
    zero_quad(from);
    vec3_add(V(from), V(from), V(m + 0x10));
    swc1(from + 8, fpu_add(lwc1(from + 8), lwc1(t + 0x14)));
    const float degree = u2f(0x3c8efa35u);
    const float pi = u2f(0x40490fdbu);
    const uint32_t target = class437_find_target(m, from, m + 0x40, fpu_mul(lwc1(t + 8), degree), fpu_mul(lwc1(t + 0xc), degree),
                                                 lwc1(t + 4), 2.0f, pi, pi);
    if (target == 0) {
        sw(pv + 0x68, 0);
    } else {
        sw(pv + 0x68, target);
        // it shoots at a target above the hero's feet, in front of it, but in the mode 2
        if (fpu_add(lwc1(HERO + 0x88), u2f(0x3f8ccccdu)) < lwc1(m + 0x18) &&  // (1.1)
            angle_dist(lwc1(m + 0x48), heading_to(m, target + 0x10)) < u2f(0x3f060a92u) &&  // (30 degrees)
            lw(OV(0x0015f5c4)) != 2) {
            lq_sq(at, target + 0x10);
            const uint32_t word = moby_pvar_word0(target);
            const float lift = word != 0 ? lwc1(word + 0x10) : lwc1(t + 0x14);
            swc1(at + 8, fpu_add(lwc1(at + 8), lift));
            vec3_sub(V(ahead), V(at), V(from));
            vec3_set_length(V(ahead), V(ahead), lwc1(t + 0x10));
            vec3_add(V(from), V(from), V(ahead));
            auto yaw = [from, at] { return atan2_f(fpu_sub(lwc1(at), lwc1(from)), fpu_sub(lwc1(at + 4), lwc1(from + 4))); };
            auto pitch = [from, at] { return -atan2_f(vec2_distance(V(from), V(at)), fpu_sub(lwc1(at + 8), lwc1(from + 8))); };
            const float aim8 = yaw(), aimc = pitch(), yaw2 = yaw(), pitch2 = pitch();
            class1633_spawn(m, from, aim8, aimc, yaw2, pitch2);
            sh(pv + 0x4e, static_cast<uint32_t>(frame_scale_i(lw(t))));
            sw(pv + 0x64, lwu(pv + 0x64) + 1);
        }
    }
    ee_stack_free(0xa0);
}

// orig: L1:0x002e3830 L2:0x002cc518 L4:0x002c7738
void class437_move(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t step = sp;
    const uint32_t rec = sandmouse_walker_rec(), t = tunings();
    const uint32_t pos = m + 0x10, vel = pv + 0x10;
    vec3_sub(V(step), V(pv), V(pos));
    // the distance from its home (while going home, state 6) or from the hero (the original also measures the one to its
    // goal and drops it)
    const float away = vec2_distance(V(pos), V(lbu(m + 0x20) == 6 ? pv + 0x30 : HERO + 0x80));
    const float far = u2f(0x3fa66666u);  // (1.3)
    const uint32_t mode = lbu(m + 0xbc);  // (0: turning toward the goal, 2 / 3: walking)
    if (mode == 0) {
        turn_toward(m, heading_to(m, pv));
        if ((lbu(m + 0x70) & 2) != 0) {
            swc1(m + 0x58, 1.0f);
            idle_anim(m, 0x14);
        }
        if (far < away) {
            if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(0xa));
            sb(m + 0xbc, 2);
        }
        ee_stack_free(0xa0);
        return;
    }
    if (mode != 2 && mode != 3) {
        sb(m + 0xbc, far < away ? 2 : 0);
        ee_stack_free(0xa0);
        return;
    }
    const uint32_t target = lwu(pv + 0x68);
    if (target == 0) {
        turn_toward(m, heading_to(m, pv));
    } else {
        const float degree = u2f(0x3c8efa35u);
        yaw_turn_damped(m, rec + 0x14, heading_to(m, target + 0x10), fpu_mul(fpu_mul(lwc1(t + 0x44), degree), frame_sq()),
                        fpu_mul(fpu_mul(lwc1(t + 0x48), degree), frame_sq()), fpu_mul(fpu_mul(lwc1(t + 0x4c), degree), frame_dt()));
    }
    // the step: the way to the goal (0.04 of it a frame), less the last step, eased (0.13 far, 0.25 near), the fall slowed
    vec3_scale(V(step), V(step), fpu_mul(scale(), u2f(0x3d23d70au)));  // (0.04)
    vec3_sub(V(step), V(step), V(vel));
    const float ease = far < away ? u2f(0x3e051eb8u) : 0.25f;  // (0.13)
    vec3_scale(V(step), V(step), fpu_mul(lwc1(0x0015ed64), ease));
    swc1(step + 8, fpu_mul(lwc1(step + 8), fpu_add(fpu_mul(scale(), u2f(0xbdccccd0u)), 1.0f)));  // (-0.1)
    vec3_add(V(vel), V(vel), V(step));
    if (lbu(m + 0x20) == 6) vec3_clamp_length_vu(V(vel), V(vel), fpu_mul(frame_dt(), u2f(0x402ccccdu)));  // (2.7)
    const uint32_t moved = walker_move_fx(m, vel, lw(rec), 0, 1.0f, lwc1(rec + 0x30));
    const float speed = vec3_length(V(vel));
    const float slow = u2f(0x3ba3d70au);  // (0.005)
    // its animation: walking (5), stopped short at a wall or above the floor (3), else idle
    auto walk_anim = [m] {
        if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(7));
    };
    auto stop_anim = [m] {
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(0xb));
    };
    if (moved != 0) {
        // stuck (moving, slower than 0.005) for 20 frames: put at its goal (but while drawn)
        if (speed < slow) {
            const uint32_t n = lhu(pv + 0x4a) + 1;
            sh(pv + 0x4a, n);
            if (frame_scale_i(0x14) < static_cast<int16_t>(n) && lbu(m + 0x31) == 0) {
                lq_sq(pos, pv);
                sh(pv + 0x4a, 0);
            }
        }
        const float floor = floor_probe_v(0.5f, V(pos), 0);
        if ((moved & 2) == 0 && !(fpu_sub(lwc1(m + 0x18), floor) < lwc1(t + 0x20))) {
            walk_anim();
        } else if (!(speed < slow)) {
            stop_anim();
        } else if (!(lbu(m + 0x53) < 3)) {
            idle_anim(m, 0xa);
        }
    } else {
        const float floor = floor_probe_v(0.5f, V(pos), 0);
        if (fpu_sub(lwc1(m + 0x18), floor) < lwc1(t + 0x20)) {
            stop_anim();
        } else {
            walk_anim();
            sh(pv + 0x4a, 0);
        }
    }
    // pushed away from the hero within 1.1 (but while going home)
    const float close = u2f(0x3f8ccccdu);
    if (lbu(m + 0x20) != 6 && away < close) {
        const float push = fpu_sub(close, away);
        const float a = atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14)));
        const float k = u2f(0x3dcccccdu);  // (0.1)
        swc1(vel, fpu_sub(lwc1(vel), fpu_mul(fpu_mul(push, cos_vu(a)), fpu_mul(lwc1(0x0015ed64), k))));
        swc1(vel + 4, fpu_sub(lwc1(vel + 4), fpu_mul(fpu_mul(push, sin_vu(a)), fpu_mul(lwc1(0x0015ed64), k))));
    }
    ee_stack_free(0xa0);
}

// The update: 0 set up at its house (or gone if collected), 1 hidden, waiting for the hero in its zone, 2 coming out (its
// house opening), 3 running to the hero, 4 the hero let go, 5 running about shooting at targets, 6 burrowing back, 19 its
// fall into its house, 20 home again; the floor's light, and its house's collision, every frame.
// orig: L1:0x002e3f20 L2:0x002ccc08 L4:0x002c7e28
void class437_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t rec = sandmouse_walker_rec(), t = tunings();
    auto house = [pv] { return moby_at(lwu(pv + 0x50)); };
    auto has_house = [pv] { return lw(pv + 0x50) != -1; };
    // the house turned toward the hero (while it waits) or toward it
    if (has_house()) {
        const uint32_t h = house();
        const uint32_t to = lbu(m + 0x20) < 5 ? HERO + 0x80 : m + 0x10;
        swc1(lwu(h + 0x78), atan2_f(fpu_sub(lwc1(to), lwc1(h + 0x10)), fpu_sub(lwc1(to + 4), lwc1(h + 0x14))));
        if (has_house()) sw(house() + 0x94, 0);
    }
    const uint32_t sp = ee_stack_alloc(0xa0);
    enum class Exit { Out, Shown, Gone };
    auto run = [&]() -> Exit {
        switch (lbu(m + 0x20)) {  // (the level's jump table of 22 states)
        case 0: {  // (L2:0x002ccd14) set up at its house: gone if no house or once collected
            zero_quad(pv);
            zero_quad(pv + 0x10);
            sh(pv + 0x4a, 0);
            sh(pv + 0x4c, 0);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
            if (!has_house()) {
                debug_printf_stub();  // (the original's message: no house)
                return Exit::Gone;
            }
            lq_sq(m + 0x10, house() + 0x10);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), u2f(0x3ca3d70au)));  // (0.02)
            const int32_t idx = lh(m + 0xb2);
            const bool collected =
                lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0 ||
                ((lwu(0x0014c190 + (lwu(0x0015ed84) << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx & 0x1f)) & 1u) != 0;
            if (collected) {
                debug_printf_stub();  // (the original's message: collected)
                return Exit::Gone;
            }
            lq_sq(pv + 0x20, m + 0x10);
            lq_sq(pv + 0x30, m + 0x10);
            walker_record_init_l1(rec);
            // the step's limit from the size of its collision's first primitive (its kind, point and sizes on the stack)
            moby_collision_point(m, 0, sp + 0x20, sp, sp + 0x24, sp + 0x28);
            const float pi = u2f(0x40490fdbu), k = u2f(0x41384e88u);  // (pi, 11.519173)
            swc1(rec + 0x14, fpu_mul(scale(), u2f(0x3ecccccdu)));   // (0.4)
            swc1(rec + 0xc, u2f(0x3e19999au));                      // (0.15)
            swc1(rec + 0x24, fpu_mul(frame_dt(), u2f(0x402ccccdu)));  // (2.7)
            swc1(rec + 0x1c, fpu_mul(scale(), u2f(0x3c23d70au)));   // (0.01)
            swc1(rec + 8, u2f(0x3e19999au));
            swc1(rec + 0x10, fpu_mul(scale(), u2f(0x3c23d70au)));
            const int32_t limit = float_to_int(fpu_mul(lwc1(sp + 0x24), 1024.0f));
            sw(rec, static_cast<uint32_t>(limit + 1));
            sw(rec + 0x38, lwu(rec + 0x38) | 8);
            swc1(rec + 4, u2f(0x3e99999au));    // (0.3)
            swc1(rec + 0x30, u2f(0x3ba3d70au));  // (0.005)
            swc1(rec + 0x3c, fpu_mul(frame_sq(), k));
            swc1(rec + 0x40, fpu_mul(frame_sq(), pi));
            swc1(rec + 0x44, fpu_mul(frame_dt(), k));
            sw(rec + 0x48, static_cast<uint32_t>(frame_scale_i(4)));
            swc1(rec + 0x28, pi);
            sb(m + 0x20, 1);
            return Exit::Shown;
        }
        case 1: {  // (L2:0x002ccf74) hidden: the hero in its zone, on foot (mode 5), seen: it takes the hero
            if (lbu(0x0013d4c5) == 0) return Exit::Out;
            if (lw(HERO + 0x1158) != 5) return Exit::Out;
            sw(pv + 0x64, 0);
            sw(pv + 0x60, static_cast<uint32_t>(frame_scale_i(lw(t + 0x18))));
            if (lbu(m + 0x31) != 0 && (lbu(m + 0x70) & 2) != 0) idle_anim(m, 0xa);
            if (has_house()) sb(house() + 0xbc, point_in_box(HERO + 0x80, lw(pv + 0x40)) != 0 ? 3 : 1);
            if (lwu(HERO + 0x1130) == 0 || lw(HERO + 0x1158) != 5) return Exit::Out;
            turn_toward(m, heading_to(m, HERO + 0x80));
            const uint32_t from = sp, to = sp + 0x10;
            lq_sq(from, house() + 0x10);
            swc1(from + 8, fpu_add(lwc1(from + 8), 0.5f));
            lq_sq(to, HERO + 0x80);
            swc1(to + 8, fpu_add(lwc1(to + 8), u2f(0x3e4ccccdu)));  // (0.2)
            if (point_in_box(HERO + 0x80, lw(pv + 0x40)) == 0) return Exit::Out;
            if (segment_collide_v(V(from), V(to), 0, m, nullptr) != 0 && lwu(OV(0x00173e58)) != lwu(HERO + 0x2080)) return Exit::Out;
            point_ahead_of_hero(m, pv);
            const uint32_t held = lwu(HERO + 0x1130);
            if (held != 0 && lh(held + 0xa6) == 0x1b1) {
                const uint32_t st = lbu(held + 0x20);
                if (st != 0xfe && st != 0xfd) sb(held + 0x20, 1);
            }
            sw(rec + 0x50, m);
            sandmouse_hero_state(0x1f, 0);
            hero_anim_set(0, 0, static_cast<float>(frame_scale_i(6)));
            if (has_house()) {
                sb(house() + 0xbc, 5);
                sb(m + 0x20, 2);
            } else {
                sh(m + 0x32, 0xff);
                sb(m + 0x20, 3);
            }
            return Exit::Out;
        }
        case 2:  // (L2:0x002cd22c) coming out once the house is open (its state 5)
            if (has_house()) {
                if (lbu(house() + 0x20) != 5) return Exit::Out;
                sb(m + 0x20, 3);
                sh(pv + 0x4c, 0);
                sh(m + 0x32, 0xff);
            } else {
                sh(pv + 0x4c, 0);
                sh(m + 0x32, 0xff);
                sb(m + 0x20, 3);
            }
            return Exit::Out;
        case 3: {  // (L2:0x002cd280) running to the point ahead of the hero; there (or at a wall or drop): the hero let go
            point_ahead_of_hero(m, pv);
            turn_toward(m, heading_to(m, pv));
            const uint32_t blocked = sandmouse_walker_step(m, rec, pv, pv + 0x10);
            if ((blocked & 6) != 0 || vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 1.0f) {
                zero_quad(pv + 0x10);
                sb(m + 0x20, 4);
                sh(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(0x78)));
            }
            if (!(u2f(0x3ecccccdu) < vec2_distance(V(m + 0x10), V(pv + 0x30)))) return Exit::Out;  // (0.4)
            if (lh(pv + 0x4c) != 0) return Exit::Out;
            swc1(rec + 4, u2f(0x3f333333u));  // (0.7)
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(3));
            return Exit::Out;
        }
        case 4: {  // (L2:0x002cd3d8) the hero let go, shown again; a moby the hero holds dropped
            sandmouse_hero_state(0, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            if (has_house()) sh(house() + 0x34, lhu(house() + 0x34) & 0xffbe);
            sb(m + 0x20, 5);
            const uint32_t held = lwu(HERO + 0x1130);
            if (held == 0 || lh(held + 0xa6) != 0x1b1) return Exit::Out;
            const uint32_t st = lbu(held + 0x20);
            if (st == 0xfe || st == 0xfd) return Exit::Out;
            sb(pv + 0x48, 0xff);
            sb(lwu(HERO + 0x1130) + 0x20, 2);
            return Exit::Out;
        }
        case 5: {  // (L2:0x002cd464) out: shooting while it may; back home when its time or its shots run out or the hero leaves
            bool stay = false;
            if (lwu(HERO + 0x1130) != 0 && lw(HERO + 0x1158) == 5) {
                const int32_t hs = lw(HERO + 0x208c);
                if (hs != 0x16 && hs != 0x14 && lbu(HERO + 0x20a4) == 0 &&
                    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x60))) == 0)
                    stay = lw(pv + 0x64) < lw(t + 0x1c);
            }
            if (stay) {
                class437_goal_by_hero(m, pv);
                class437_move(m, pv);
            } else {
                if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(0xa));
                zero_quad(pv + 0x10);
                sh(pv + 0x4c, lhu(t + 0x24));
                sb(m + 0x20, 6);
                if (has_house()) sb(house() + 0xbc, 5);
            }
            class437_shoot(m, pv);
            return Exit::Out;
        }
        case 6: {  // (L2:0x002cd560) burrowing back: dust, spinning, fading; at the end its fall into its house (19)
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x4c))) != 0) {
                sb(m + 0x23, 0x80);
                lq_sq(m + 0x10, pv + 0x30);
                lq_sq(pv, pv + 0x30);
                zero_quad(pv + 0x10);
                sb(m + 0x20, 0x13);
                sh(m + 0x34, lhu(m + 0x34) & 0xfdff);
                return Exit::Out;
            }
            const uint32_t at = sp, still = sp + 0x10;
            zero_quad(still);
            lq_sq(at, m + 0x10);
            sandmouse_dent(at, lwc1(t + 0x30));
            const uint32_t red = static_cast<uint32_t>(libc_rand() + 0x30) & 0x3f;
            const uint32_t green = static_cast<uint32_t>(libc_rand() + 0x20) & 0x3f;
            const uint32_t blue = static_cast<uint32_t>(libc_rand()) & 0x2f;
            const uint32_t colour = red | (green << 8) | (blue << 16) | (lwu(t + 0x3c) << 24);
            const int32_t coin = libc_rand();
            int32_t spin = coin % 2;
            if ((lhu(pv + 0x4c) & 1) != 0) spin = -spin;
            const int32_t n = rand_range(lw(t + 0x28), lw(t + 0x2c));
            const float size = rand_range_f(lwc1(t + 0x34), lwc1(t + 0x38));
            sandmouse_dust(at, n, colour, spin, still, size, u2f(0x3a83126fu), fpu_mul(lwc1(t + 0x40), frame_sq()));  // (0.001)
            const int32_t out = lw(t + 0x24);
            sh(m + 0x34, lhu(m + 0x34) | 0x200);
            sb(m + 0x23, lbu(m + 0x23) - static_cast<uint32_t>(ee_div(0x80, out)));
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(frame_dt(), u2f(0x41c90fdbu))));  // (8 pi)
            return Exit::Out;
        }
        case 0x13: {  // (L2:0x002cd6e4) falling into its house, slowed; on the floor: at its house's place (20)
            const float slow = fpu_add(fpu_mul(scale(), u2f(0xbd4cccd0u)), 1.0f);  // (-0.05)
            swc1(pv + 0x14, fpu_mul(lwc1(pv + 0x14), slow));
            swc1(pv + 0x10, fpu_mul(lwc1(pv + 0x10), slow));
            if ((class437_sink(m, pv) & 2) == 0) return Exit::Out;
            zero_quad(pv + 0x10);
            lq_sq(pv, house() + 0x10);
            sb(m + 0x20, 0x14);
            return Exit::Out;
        }
        case 0x14: {  // (L2:0x002cd768) home again, turned to face its house; down a drop: hidden (1)
            sw(rec + 0x50, 0);
            turn_toward(m, heading_to(m, pv));
            const float z = lwc1(m + 0x18);
            const uint32_t r = sandmouse_walker_step(m, rec, pv, pv + 0x10);
            swc1(m + 0x18, z);  // (its height kept)
            if ((r & 4) == 0) return Exit::Out;
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(6));
            sb(house() + 0xbc, 1);
            sb(m + 0x20, 1);
            return Exit::Shown;
        }
        default:  // (L2:0x002cd844: 7 .. 18, 21 and past the table)
            return Exit::Out;
        }
    };
    const Exit exit = run();
    ee_stack_free(0xa0);
    if (exit == Exit::Gone) {
        moby_retire(m);
        return;
    }
    if (exit == Exit::Shown) sh(m + 0x32, 0);
    sandmouse_floor_light(m, u2f(0x3e99999au));  // (0.3)
    if (has_house()) sw(house() + 0x94, lwu(lwu(house() + 0x24) + 0x10));
}

}  // namespace rac1
