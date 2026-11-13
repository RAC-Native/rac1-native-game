// Novalis's own moby classes, part 2g: the Birdbot (class 459) and its helpers. Header novalis_mobys2.h. Addresses are
// Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR, the base ELF's are bare. Registered in novalis_mobys2.cpp.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)
const float kFiveDegrees = u2f(0x3db2b8c2u);
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);

float dt() { return lwc1(0x0015ed6c); }        // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }     // (its square: the accelerations' scale)

// the heading from the moby to (x, y), as the original computes it
float heading_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the animation set `set` unless it is playing, blended over `frames` (60 Hz) frames
void anim_to(uint32_t m, uint32_t set, int32_t frames, int32_t frame = 0) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frame_scale_i(frames));
}
}  // namespace

// The moby's animation position in frames (frame times are s16 1/16ths): the current set's (+0x52, 0xff: the one
// +0x6c, else +0x68) plus the blend +0x54; while it blends from the set +0x53 into +0x52 with the frame +0x51 not past
// +0x50, the two sets' frame times mixed by the blend.
// orig: L1:0x00263920
float moby_anim_scalar_l1(uint32_t m) {
    const uint32_t set = lbu(m + 0x52) == 0xff ? lwu(m + 0x6c) : lwu(m + 0x68);
    const float sixteenth = u2f(0x3d800000u);
    if (lwc1(m + 0x54) == 0.0f) return fpu_mul(int_to_float(lh(set + 4)), sixteenth);
    if (lbu(m + 0x53) != lbu(m + 0x52) || lbu(m + 0x51) < lbu(m + 0x50))
        return fpu_add(fpu_mul(int_to_float(lh(set + 4)), sixteenth), lwc1(m + 0x54));
    const float a = fpu_mul(int_to_float(lh(set + 4)), fpu_sub(1.0f, lwc1(m + 0x54)));
    const float b = fpu_mul(int_to_float(lh(lwu(m + 0x6c) + 4)), lwc1(m + 0x54));
    return fpu_mul(fpu_add(a, b), sixteenth);
}

// The moby's vertical extent around `z` (+0x84 / +0x88: z - 0.2 .. z + 0.2).
// orig: L1:0x0026eff8
void moby_extent_set_l1(uint32_t m, float z) {
    const float k = u2f(0x3e4ccccdu);
    swc1(m + 0x88, fpu_add(z, k));
    swc1(m + 0x84, fpu_sub(z, k));
}

// The Birdbot's shot (class 0x2d2) at `pos` heading `dir`, fired by `m`, for `life` frames: half size, grey, a random
// spin, its two glows (particle 0x1a). The shot (0: none).
// orig: L1:0x002fa068
uint32_t bird_shot_spawn_l1(uint32_t m, uint32_t pos, uint32_t dir, uint32_t life) {
    const Vec4 at = V(pos), heading = V(dir);
    const uint32_t shot = moby_spawn(0x2d2);
    if (shot == 0) return 0;
    const uint32_t spv = lwu(shot + 0x78);
    sw(spv + 0x10, m);
    sb(shot + 0x31, 1);
    sh(shot + 0x32, 0x7f);
    swc1(shot + 0x2c, fpu_mul(lwc1(shot + 0x2c), 0.5f));
    sb(shot + 0x30, 0x7f);
    moby_set_color_hi(shot, 0x7f, 0x7f, 0x7f);
    swc1(shot + 0x40, rand_angle());
    swc1(shot + 0x44, rand_angle());
    swc1(shot + 0x48, rand_angle());
    V(shot + 0x10) = at;
    V(spv) = heading;
    sw(spv + 0x14, life);
    spawn_particle_1a(shot, 0x2f00007fu, life, 0xffffffffu, u2f(0x484d1400u));
    spawn_particle_1a(shot, 0x4f004f7fu, lwu(spv + 0x14), 0xffffffffu, u2f(0x47e19600u));
    moby_update_transform(shot);
    return shot;
}

// Three sparks (particle 0x16) from the Birdbot's joint 1, falling and drifting by how far the moby moved since `from`
// (its position at the start of the frame); their colours picked from the level's two tables of six.
// orig: L1:0x002e6518
void bird_sparks_l1(uint32_t m, uint32_t from) {
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t start = sp, vel = sp + 0x10, at = sp + 0x20, moved = sp + 0x30, c1 = sp + 0x40, c2 = sp + 0x60;
    lq_sq(start, from);
    for (int32_t k = 0; k < 3; ++k) {
        V(at) = Vec4{};
        swc1(at, fpu_mul(rand_range_f(1.0f, -1.0f), fpu_mul(dt(), u2f(0x3e4ccccdu))));
        swc1(at + 4, fpu_mul(rand_range_f(1.0f, -1.0f), fpu_mul(dt(), u2f(0x3e4ccccdu))));
        swc1(at + 8, fpu_mul(rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)), fpu_mul(dt(), -7.0f)));
        lq_sq(vel, at);
        for (uint32_t i = 0; i < 0x18; i += 4) {  // (the colour tables copied to the stack, as the original does)
            sw(c1 + i, lwu(L1_ADDR(0x0020af50) + i));
            sw(c2 + i, lwu(L1_ADDR(0x0020af68) + i));
        }
        moby_chain_point_v(m, 1, V(at));
        mat3_transform(V(vel), V(vel), M(m + 0xc0));
        vec3_sub(V(moved), V(m + 0x10), V(start));
        vec3_add(V(vel), V(vel), V(moved));
        const uint32_t start_colour = c1 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t end_colour = c2 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const float size = rand_range_f(u2f(0x47cd1400u), u2f(0x4819cf00u));
        const int32_t frames = frame_scale_i(rand_range(5, 0xf));
        particle_16_spawn(at, vel, lwu(start_colour), lwu(end_colour), frames, size);
    }
    ee_stack_free(0x150);
}

// The Birdbot's next point on its path (the follower pvar +0x150: index +0, direction +4, path +0x10; `idx` the
// index it is given): on a path flown point by point (pvar +0x1ac) the next point from `idx` that ends a leg (w 1);
// else the best of the points before, at and after the nearest one (only the legs' ends when +0x1ae is set) - while
// alerted (pvar +0x38, or the alert timer +0x1e2) the one nearest the target, else the one whose heading is furthest
// from the target's (within 90 degrees), or the nearest one beyond 12 units. `idx` gets the pick; 1 when it is not the
// nearest point (or on a point-by-point path).
// orig: L1:0x002e6790
uint32_t bird_path_pick_l1(uint32_t m, uint32_t idx) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t fol = pv + 0x150;
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t target = sp;
    const bool alert = lw(pv + 0x38) != 0 || lh(pv + 0x1e2) != 0;
    const float range = alert ? fpu_add(lwc1(pv + 0x1a0), lwc1(pv + 0x1a0)) : lwc1(pv + 0x1a0);
    target_select_any(m, target, range);
    auto path = [&] { return lwu(fol + 0x10); };
    auto count = [&] { return lw(path()); };
    auto point = [&](int32_t i) { return path() + static_cast<uint32_t>(i) * 16 + 0x10; };
    auto step = [&](int32_t i) {  // the index after `i` in the follower's direction, wrapped
        return ee_rem(i + count() + static_cast<int8_t>(lbu(fol + 4)), count());
    };
    if (lh(pv + 0x1ac) != 0) {
        for (;;) {
            if (lw(idx) < count() - 1) sw(idx, static_cast<uint32_t>(step(lw(idx))));
            if (lw(idx) == count() - 1) break;
            if (lwc1(point(lw(idx)) + 0xc) == 1.0f) break;
        }
        ee_stack_free(0x130);
        return 1;
    }
    // the nearest point (in the plane), the point scanned before it and the one scanned after it
    int32_t nearest = 0, prev_of_nearest = 0, after_nearest = 0, prev = 0;
    float best = u2f(0x461c4000u);
    for (int32_t i = 0; i < count(); ++i) {
        if (lh(pv + 0x1ae) != 0 && !(lwc1(point(i) + 0xc) == 1.0f)) continue;
        const float d = vec2_distance(V(point(i)), V(m + 0x10));
        if (d < best) {
            prev_of_nearest = prev;
            best = d;
            nearest = i;
        } else if (prev == nearest) {
            after_nearest = i;
        }
        prev = i;
    }
    if (nearest == count() - 1) after_nearest = 0;
    else if (nearest == 0) prev_of_nearest = count() - 1;
    int32_t pick = 0;
    best = alert ? u2f(0x461c4000u) : 0.0f;
    float best_turn = 0.0f;
    sw(idx, 0);
    for (;;) {
        const int32_t i = lw(idx);
        if (i == prev_of_nearest || i == nearest || i == after_nearest) {
            const Vec4 p = V(point(i));
            const bool usable = lh(pv + 0x1ae) == 0 || p.w == 1.0f;
            if (alert) {
                const float d = vec3_distance(V(target), p);
                if (usable && d < best) {
                    pick = i;
                    best = d;
                }
            } else {
                const float turn = angle_dist(atan2_f(fpu_sub(p.x, lwc1(m + 0x10)), fpu_sub(p.y, lwc1(m + 0x14))),
                                              atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14))));
                const float d = vec3_distance(V(m + 0x10), p);
                if (usable) {
                    if (turn < u2f(0x3fc90fdbu)) {
                        if (best_turn < turn) {
                            pick = i;
                            best = d;
                            best_turn = turn;
                        }
                    } else if ((12.0f <= d && (d < best || best < 12.0f)) || (d < 12.0f && best < d)) {
                        pick = i;
                        best = d;
                        best_turn = turn;
                    }
                }
            }
        }
        const int32_t next = step(i);
        sw(idx, static_cast<uint32_t>(next));
        if (next == 0) break;
    }
    sw(idx, static_cast<uint32_t>(pick));
    ee_stack_free(0x130);
    return pick != nearest ? 1u : 0u;
}

// Class 459, the Birdbot: a walking bird robot that patrols its path (the follower pvar +0x150), turns to what
// target_select_any finds within its range +0x1a0 (doubled while alerted: pvar +0x38 or the alert timer +0x1e2), shoots
// at it (class 0x2d2) when it faces it with a clear line, flies off to the next point of its path, and lands. The hero's
// weapons hit it (mask 0x210000): health +0x20; knocked back (state 0xc) or killed (0xe), exploding when it lands or
// leaves the level's box. States: 0 set up, 1 aims, 2 waits for the hero (a scripted camera), 3 walks its path or to
// the camera point, 6 fires, 7 after the shot, 9 takes off, 0xa flies, 0xb lands, 0xc knocked back, 0xe dies.
// pvars: the move record +0x70, the colour pulse +0x60, the walker record +0xd0, the turn rate +0x194, the fire timer
// +0x190, the shots fired +0x1a4, the flight speed +0x1b8 / its pitch rate +0x1bc, the scripted camera +0x1c4 / its
// timer +0x1c8, the camera point +0x1e4 with the velocity +0x1d0, the animation position of the last frame +0x1cc.
// orig: L1:0x002e6bf0
void birdbot_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x180);
    const uint32_t pos0 = sp + 0x20, tgt = sp + 0x30, va = sp + 0x80, vb = sp + 0x90, vc = sp + 0xa0;
    const uint32_t pick = sp + 0xb0, hit_state = sp + 0xb4, hit_dmg = sp + 0xb8, hit_yaw = sp + 0xbc;
    const uint32_t pos = m + 0x10;
    sw(pick, lwu(pv + 0x150));
    const float frame = moby_anim_scalar_l1(m);
    const bool alert = lw(pv + 0x38) != 0 || lh(pv + 0x1e2) != 0;
    joint_node_scale(m, 2, pv + 0x200, 2.5f);
    lq_sq(pos0, pos);
    if (lw(pv + 0x1c8) > 0) sw(pv + 0x1c8, static_cast<uint32_t>(lw(pv + 0x1c8) - 1));
    uint32_t moved_on = 0;  // the path pick is another point than the nearest
    if (lbu(m + 0x20) != 0) moved_on = bird_path_pick_l1(m, pick);
    const float range = alert ? fpu_add(lwc1(pv + 0x1a0), lwc1(pv + 0x1a0)) : lwc1(pv + 0x1a0);
    target_select_any(m, tgt, range);

    // ---- the hit
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x210000), 0);
    int32_t reaction;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x26)));
    if (hit != 0 && lh(pv + 0x26) == 0) {
        const uint32_t src = lwu(hit + 0x20);
        const uint32_t from = (src == 0 || src == lwu(0x001403e0)) ? HERO + 0x80 : src + 0x10;
        swc1(hit_yaw, atan2_f(fpu_sub(lwc1(pos), lwc1(from)), fpu_sub(lwc1(pos + 4), lwc1(from + 4))));
        sh(pv + 0x1e2, static_cast<uint32_t>(frame_scale_i(0x258)));
        const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) reaction = 1;
        // the knock: its speeds and arc, its yaw from the hit's direction, the move set
        auto knock = [&](float spin, float vmax, float up, uint32_t set, int32_t frames) {
            swc1(pv + 0xc0, spin);
            swc1(pv + 0x8c, fpu_mul(dt(), up));
            swc1(pv + 0xc4, vmax);
            sw(pv + 0x94, 1);
            sb(pv + 0xad, 0);
            lq_sq(va, hit + 0x10);
            vec_yaw_marked_v(V(va), *static_cast<float*>(ee_ram_ptr(hit_yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, set, frames, 0, lwc1(hit_yaw));
        };
        switch (reaction) {  // (the hit's jump table L1:0x0020afd0, 12 entries, by its index)
        case 1:
        case 2: {  // (0x002e6dfc) killed: thrown up and away
            const float g = dt_sq();
            const float h = fpu_mul(g, 10.0f);
            swc1(pv + 0x98, u2f(0x3e99999au));
            swc1(pv + 0x84, h);
            swc1(pv + 0x80, fpu_mul(g, 40.0f));
            swc1(pv + 0x88, sqrt_f(fpu_mul(fpu_add(h, h), 10.0f)));
            knock(5.0f, 10.0f, 15.0f, 9, frame_scale_i(2));
            sb(pv + 0x67, 0xb4);
            sb(m + 0x20, 0xe);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            color_pulse_start(m, pv + 0x60);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            break;
        }
        case 3:
        case 6:  // (0x002e6ef8) knocked back
            sb(pv + 0x67, 0x78);
            sw(pv + 0x90, 0x200);
            swc1(pv + 0x80, u2f(0x3c03126fu));
            swc1(pv + 0x98, 0.5f);
            swc1(pv + 0x88, sqrt_f(fpu_mul(fpu_add(lwc1(pv + 0x84), lwc1(pv + 0x84)), 1.5f)));
            knock(5.0f, 10.0f, 8.5f, 0xc, 6);
            anim_to(m, 0xb, 6);
            sb(m + 0x20, 0xc);
            color_pulse_start(m, pv + 0x60);
            break;
        case 4:
        case 5:  // (0x002e6ffc) knocked back hard
            sw(pv + 0x90, 0x200);
            sb(pv + 0x67, 0xfa);
            swc1(pv + 0x80, u2f(0x3c03126fu));
            swc1(pv + 0x88, sqrt_f(fpu_mul(fpu_add(lwc1(pv + 0x84), lwc1(pv + 0x84)), 1.5f)));
            knock(7.0f, 13.5f, 5.0f, 0xe, 6);
            sb(m + 0x20, 0xc);
            color_pulse_start(m, pv + 0x60);
            break;
        default:
            break;
        }
    }
    sb(m + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0) sh(pv + 0x1e2, static_cast<uint32_t>(frame_scale_i(0x258)));
    sw(pv + 0x38, 0);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1e2)));

    // ---- the pieces the states share
    auto path_point = [&](uint32_t fol, int32_t i) { return lwu(fol + 0x10) + static_cast<uint32_t>(i) * 16 + 0x10; };
    auto target_in_range = [&] {
        const float d = vec2_distance(V(pos), V(tgt));
        return alert ? d < fpu_add(lwc1(pv + 0x1a0), lwc1(pv + 0x1a0)) : d < lwc1(pv + 0x1a0);
    };
    auto target_dz = [&] { return abs_f(fpu_sub(lwc1(pos + 8), lwc1(tgt + 8))); };
    enum class Aim { NotReady, Fire, Blocked };
    // facing the target (within 5 degrees) with a clear line from its gun (joint 0, at the gun's height): it fires
    // (state 6, animation 4), else the fire timer waits a second
    auto try_aim = [&] {
        if (!(target_in_range() && target_dz() < 2.0f && lbu(m + 0x31) != 0 && (!alert || moved_on == 0) && lw(pv + 0x190) == 0))
            return Aim::NotReady;
        if (!(angle_dist(lwc1(m + 0x48), heading_to(m, lwc1(tgt), lwc1(tgt + 4))) < kFiveDegrees)) return Aim::NotReady;
        moby_chain_point_v(m, 0, V(va));
        lq_sq(vb, pos);
        swc1(vb + 8, lwc1(va + 8));
        if (segment_collide_v(V(vb), V(va), 0, m, nullptr) != 0) {
            sw(pv + 0x190, static_cast<uint32_t>(frame_scale_i(0x3c)));
            return Aim::Blocked;
        }
        sh(pv + 0x1a4, 0);
        swc1(pv + 0x198, 4.0f);
        sb(m + 0x20, 6);
        anim_to(m, 4, 6);
        sw(pv + 0x190, 0);
        return Aim::Fire;
    };
    // the picked point far enough (a quarter unit): it takes off for it (state 9)
    auto take_off = [&] {
        anim_to(m, 8, 0xa);
        sw(pv + 0x150, lwu(pick));
        swc1(m + 0x58, lwc1(L1_ADDR(0x0016195c)));
        sb(m + 0x20, 9);
    };
    // back to aiming (state 1): the animation 0 blended over `frames`
    auto to_aiming = [&](int32_t frames) {
        anim_to(m, 0, frames);
        sb(m + 0x20, 1);
    };
    auto explode = [&] {
        explosion_effect(m, pv + 0x40, pos, 5, 2, 4, 4, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
    };
    auto debris = [&] {
        const uint32_t z = L1_ADDR(0x0015f580);
        for (const uint32_t cls : {0x6c8u, 0x6c9u, 0x6cau, 0x6eau}) debris_spawn(m, cls, pos, m + 0x40, 0, 0, z, z, z, 0.0f);
    };
    auto out_of_level = [&] {
        const float lo = 2.0f, hi = u2f(0x447f4000u);  // 1021
        for (uint32_t k = 0; k < 12; k += 4) {
            const float c = lwc1(pos + k);
            if (c < lo || hi < c) return true;
        }
        return false;
    };

    // ---- the states (each ends with the sparks, with the common tail, or returns)
    enum class Tail { Common, Sparks, Gone };
    Tail tail = Tail::Common;
    switch (lbu(m + 0x20)) {  // (the state's jump table L1:0x0020b000, 15 entries, by its index; 4, 5, 8, 0xd and above: 0x002e89a8)
    case 0: {  // (0x002e7114) set up: on its path (or the path +0x134 it walks first), the walker record, its health
        const int32_t path_idx = lw(pv + 0x164);
        if (path_idx == -1) {
            debug_printf_stub();
            moby_retire(m);
            tail = Tail::Gone;
            break;
        }
        const uint32_t paths = L1_ADDR(0x001b0930);
        sw(pv + 0x150, 0);
        sw(pv + 0x160, lwu(paths + static_cast<uint32_t>(path_idx) * 4));
        const int32_t walk = lw(pv + 0x134);
        if (walk >= 0) {
            const uint32_t p = lwu(paths + static_cast<uint32_t>(walk) * 4);
            sw(pv + 0x120, 0);
            sw(pv + 0x130, p);
            lq_sq(pos, p + 0x10);
        } else {
            lq_sq(pos, lwu(pv + 0x160) + 0x10);
            if (lw(pv + 0x1b4) == 0) {
                sw(pv + 0x130, 0);
                swc1(pos + 8, fpu_add(lwc1(pos + 8), 20.0f));
            }
        }
        sh(pv + 0x1ae, 0);
        for (int32_t i = 0; i < lw(lwu(pv + 0x160)); ++i)
            if (lwc1(lwu(pv + 0x160) + static_cast<uint32_t>(i) * 16 + 0x1c) == 1.0f) sh(pv + 0x1ae, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sb(pv + 0x29, 0);
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));
        sb(pv + 0x5a, 8);
        sb(pv + 0x58, 8);
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0x10c, u2f(0x3cf5c28fu));
        swc1(pv + 0x110, u2f(0x3e99999au));
        swc1(pv + 0x114, 0.25f);
        swc1(pv + 0xec, fpu_mul(dt_sq(), 5.0f));
        swc1(pv + 0xf4, fpu_mul(lwc1(L1_ADDR(0x00161954)), dt()));
        swc1(pv + 0xdc, 2.0f);
        swc1(pv + 0xd8, 2.0f);
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, static_cast<uint32_t>(float_to_int(2.0f)));
        sb(pv + 0x29, 0);
        sb(pv + 0x28, 1);
        vec_clear(V(pv + 0x40));
        sw(pv + 0x1c8, 0xffffffffu);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(5));
        sh(pv + 0x1e0, lhu(m + 0x32));
        const float floor = floor_probe_v(0.5f, V(pos), 0);
        if (0.5f < fpu_sub(lwc1(pos + 8), floor)) {  // in the air: hidden until it lands
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        }
        sb(m + 0x20, 2);
        break;
    }
    case 1: {  // (0x002e735c) aims at the target, fires when it can; else walks off to the picked point
        yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(tgt), lwc1(tgt + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x190)));
        if (try_aim() == Aim::NotReady) {
            bool go = true;
            if (!alert) {
                go = vec2_distance(V(pos), V(tgt)) < 4.0f && target_dz() < 6.0f;
            }
            if (go && moved_on != 0 && 0.25f < vec3_distance(V(pos), V(path_point(pv + 0x150, lw(pick))))) {
                const uint32_t fol = pv + 0x150;
                if (lh(pv + 0x1ac) == 0 || lw(fol) < lw(lwu(fol + 0x10)) - 1) take_off();
            }
        }
        if (lw(pv + 0x1c8) == 0 && lw(pv + 0x1c4) != -1) {  // the scripted camera's time is up: the hero is let go
            hydro_2eac18(0);
            hero_set_state_l1(0, 1);
            sw(L1_ADDR(0x0015f404), 0);
            sw(pv + 0x1c4, 0xffffffffu);
        }
        break;
    }
    case 2:  // (0x002e766c) waits for the hero (near, or in its box +0x1c0, or +0x1e8 set): the scripted camera +0x1c4, then walks
        if (vec2_distance(V(pos), V(tgt)) < lwc1(pv + 0x1a8) || point_in_box(HERO + 0x80, lw(pv + 0x1c0)) != 0 || lw(pv + 0x1e8) != 0) {
            if (lw(pv + 0x1c4) != -1) {
                hero_set_state_l1(0x1f, 0);
                const uint32_t cam = lwu(L1_ADDR(0x001600ec)) + (lwu(pv + 0x1c4) << 7);
                camera_scripted_set(cam + 0x30, cam + 0x70, 0, 0, 0);
                sw(L1_ADDR(0x0015f404), 1);
                sw(pv + 0x1c8, static_cast<uint32_t>(frame_scale_i(0x168)));
            }
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            anim_to(m, 8, 0xa);
            sb(m + 0x20, 3);
        }
        break;
    case 3: {  // (0x002e7754) walks the path +0x120 to its end, or flies to the camera point +0x1e4, or drops to the floor
        tail = Tail::Sparks;
        bool arrived;
        if (lw(pv + 0x130) != 0) {
            const uint32_t fol = pv + 0x120;
            const int32_t n = lw(lwu(fol + 0x10));
            const int32_t next = ee_rem(lw(fol) + n + static_cast<int8_t>(lbu(fol + 4)), n);
            const uint32_t np = lwu(fol + 0x10) + static_cast<uint32_t>(next) * 16;
            yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(np + 0x10), lwc1(np + 0x14)), u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
            lq_sq(vb, path_point(fol, next));
            if (lw(fol) == lw(lwu(fol + 0x10)) - 1) {
                arrived = true;
            } else {
                vec3_sub(V(va), V(vb), V(pos));
                vec3_set_length(V(va), V(va), fpu_mul(dt(), 5.0f));
                vec3_add(V(pos), V(pos), V(va));
                if (vec3_distance(V(vb), V(pos)) < 0.5f) sw(fol, static_cast<uint32_t>(next));
                arrived = false;
            }
        } else if (lw(pv + 0x1e4) != -1) {
            const uint32_t vel = pv + 0x1d0;
            auto cam_point = [&] { return lwu(L1_ADDR(0x001600ec)) + (lwu(pv + 0x1e4) << 7) + 0x30; };
            yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(cam_point()), lwc1(cam_point() + 4)), u2f(0x3c23d70au), u2f(0x3e99999au),
                            u2f(0x3dcccccdu));
            const float speed = vec3_length(V(vel));
            const float vz = lwc1(pv + 0x1d8);
            vec3_set_length(V(vel), V(vel), fpu_mul(speed, u2f(0x3f733333u)));
            vec3_sub(V(vb), V(cam_point()), V(pos));
            vec3_set_length(V(vb), V(vb), fpu_mul(speed, u2f(0x3d4ccccdu)));
            vec3_add(V(vel), V(vel), V(vb));
            const float t = dt();
            if (fpu_mul(t, 10.0f) < speed) {  // braked, or sped up below half that
                vec3_set_length(V(vb), V(vel), -fpu_mul(dt_sq(), 10.0f));
                vec3_add(V(vel), V(vel), V(vb));
            } else if (speed < fpu_mul(t, 5.0f)) {
                vec3_set_length(V(vb), V(vel), fpu_mul(dt_sq(), 5.0f));
                vec3_add(V(vel), V(vel), V(vb));
            }
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1ec))) == 0) swc1(pv + 0x1d8, vz);
            vec3_add(V(pos), V(pos), V(vel));
            arrived = vec3_distance(V(pos), V(cam_point())) < fpu_mul(dt(), 10.0f);
        } else {
            vec_clear(V(va));
            const float floor = floor_probe_v(0.5f, V(pos), 0);
            const float limit = -fpu_mul(dt(), 10.0f);
            const float dz = fpu_sub(floor, lwc1(pos + 8));
            swc1(va + 8, dz < limit ? limit : dz);
            vec3_add(V(pos), V(pos), V(va));
            arrived = vec3_length(V(va)) < dt();
        }
        if (arrived) to_aiming(5);
        break;
    }
    case 6: {  // (0x002e7aec) fires: the shot as the animation passes frame 14, then aims again or stops after 4 shots
        if (lwc1(pv + 0x1cc) < 14.0f && 14.0f <= frame) {
            float yaw = lwc1(m + 0x48);
            moby_chain_point_v(m, 0, V(va));
            lq_sq(pv + 0x180, va);
            const float at_target = atan2_f(fpu_sub(lwc1(tgt), lwc1(va)), fpu_sub(lwc1(tgt + 4), lwc1(va + 4)));
            if (angle_dist(at_target, yaw) < kFiveDegrees) yaw = atan2_f(fpu_sub(lwc1(tgt), lwc1(va)), fpu_sub(lwc1(tgt + 4), lwc1(va + 4)));
            swc1(pv + 0x180, fpu_add(lwc1(pv + 0x180), fpu_mul(cos_vu(yaw), lwc1(pv + 0x198))));
            swc1(pv + 0x184, fpu_add(lwc1(pv + 0x184), fpu_mul(sin_vu(yaw), lwc1(pv + 0x198))));
            vec3_sub(V(vb), V(pv + 0x180), V(va));
            vec3_set_length(V(vb), V(vb), fpu_mul(lwc1(L1_ADDR(0x00161950)), dt()));
            const float dz = fpu_add(lwc1(vb + 8), 0.0f);
            lq_sq(vc, pos);
            swc1(vc + 8, lwc1(va + 8));
            swc1(vb + 8, dz);
            if (segment_collide_v(V(vc), V(va), 0, m, nullptr) != 0) {
                to_aiming(5);
                break;
            }
            // (the original also takes the shot's yaw and pitch into f12 / f13, which the spawn never reads)
            bird_shot_spawn_l1(m, va, vb, static_cast<uint32_t>(float_to_int(frame_scale_f(10.0f))));
            sh(pv + 0x1a4, lhu(pv + 0x1a4) + 1);
        }
        if (!(frame < 2.0f)) break;
        if (lh(pv + 0x1a4) < 4) {
            const float d = vec2_distance(V(pos), V(tgt));
            const float r = alert ? fpu_add(lwc1(pv + 0x1a0), lwc1(pv + 0x1a0)) : lwc1(pv + 0x1a0);
            if (!(r < d) && !(6.0f < target_dz())) break;
        }
        if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 5, frame_scale_i(0xa));
        sb(m + 0x20, 7);
        swc1(m + 0x58, 0.5f);
        break;
    }
    case 7:  // (0x002e7d70) after the shots: turns to the target; at the animation's end aims again, or takes off
        yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(tgt), lwc1(tgt + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
        if (lbu(m + 0x53) == 0) {
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x190))) == 0) break;
            if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(0xf));
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        swc1(m + 0x58, 1.0f);
        {
            bool go = true;
            if (!alert) go = vec2_distance(V(pos), V(tgt)) < 8.0f && target_dz() < 6.0f;
            if (go && moved_on != 0 && 0.25f < vec3_distance(V(pos), V(lwu(pv + 0x160) + (lwu(pick) << 4) + 0x10))) {
                sw(pv + 0x150, lwu(pick));
                anim_to(m, 8, 0xa);
                sb(m + 0x20, 9);
                swc1(m + 0x58, lwc1(L1_ADDR(0x0016195c)));
                break;
            }
        }
        if (try_aim() != Aim::Fire) to_aiming(5);
        break;
    case 9:  // (0x002e80a4) takes off
        swc1(m + 0x58, 1.0f);
        sw(pv + 0x1b8, 0);
        sw(pv + 0x1bc, 0);
        sb(m + 0x20, 0xa);
        bird_sparks_l1(m, pos0);
        break;
    case 0xa: {  // (0x002e80d4) flies to the picked point (the leg's end nearest it on a path of legs), 0.8 above it
        const uint32_t fol = pv + 0x150;
        uint32_t aim;
        if (lh(pv + 0x1ae) != 0) {
            int32_t nearest = 0;
            float best = u2f(0x51ba43b7u);
            for (int32_t i = 0; i < lw(lwu(fol + 0x10)); ++i) {
                const float d = vec3_distance(V(lwu(fol + 0x10) + static_cast<uint32_t>(i) * 16 + 0x10), V(pos));
                if (d < best) {
                    best = d;
                    nearest = i;
                }
            }
            const int32_t cur = lw(fol);
            aim = cur < nearest ? static_cast<uint32_t>(nearest - 1) : nearest < cur ? static_cast<uint32_t>(nearest + 1) : lhu(fol);
        } else {
            aim = lhu(pv + 0x150);
        }
        sh(pv + 0x1a6, aim);
        const float lift = u2f(0x3f4ccccdu);
        lq_sq(va, path_point(fol, lh(pv + 0x1a6)));
        swc1(va + 8, fpu_add(lwc1(va + 8), lift));
        yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(va), lwc1(va + 4)), fpu_mul(dt_sq(), kTwoPi), fpu_mul(dt_sq(), kPi),
                        fpu_mul(dt(), kTwoPi));
        if (angle_dist(lwc1(m + 0x48), heading_to(m, lwc1(va), lwc1(va + 4))) < u2f(0x3f490fdbu)) {
            const float accel = lwc1(L1_ADDR(0x00161954));
            if (lwc1(pv + 0x1b8) < fpu_mul(accel, dt())) swc1(pv + 0x1b8, fpu_add(lwc1(pv + 0x1b8), fpu_mul(accel, dt_sq())));
            const float pitch = atan2_f(vec2_distance(V(pos), V(va)), fpu_sub(lwc1(va + 8), lwc1(pos + 8)));
            vec_from_spherical_v(V(vb), lwc1(pv + 0x1b8), lwc1(m + 0x48), pitch);
            vec3_add(V(pos), V(pos), V(vb));
            const float t = dt();
            const float rate = fpu_mul(fpu_div(lwc1(pv + 0x1b8), fpu_mul(lwc1(L1_ADDR(0x00161954)), t)), u2f(0x3f060a92u));
            const float g = dt_sq();
            swc1(m + 0x44, angle_seek_rate(pv + 0x1bc, lwc1(m + 0x44), rate, fpu_mul(g, kTwoPi), fpu_mul(g, kPi), fpu_mul(t, u2f(0x4096cbe4u))));
        }
        lq_sq(va, path_point(fol, lw(fol)));
        swc1(va + 8, fpu_add(lwc1(va + 8), lift));
        if (vec3_distance(V(pos), V(va)) < 0.25f) sb(m + 0x20, 0xb);
        tail = Tail::Sparks;
        break;
    }
    case 0xb: {  // (0x002e837c) lands: slows down (unless +0x1ee holds it), turns to the target, folds its wings (animation 0xa)
        const float g_rate = u2f(0x3f060a92u);
        float rate;
        const float t = dt();
        float g;
        if (0.0f < lwc1(pv + 0x1b8) && lh(pv + 0x1ee) == 0) {
            swc1(pv + 0x1b8, fpu_sub(lwc1(pv + 0x1b8), fpu_mul(fpu_mul(lwc1(L1_ADDR(0x00161954)), 3.0f), dt_sq())));
            swc1(va, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x1b8)));
            swc1(va + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x1b8)));
            vec3_add(V(pos), V(pos), V(va));
            rate = fpu_div(lwc1(pv + 0x1b8), fpu_mul(lwc1(L1_ADDR(0x00161954)), t));
            g = dt_sq();
        } else {
            yaw_turn_damped(m, pv + 0x194, heading_to(m, lwc1(tgt), lwc1(tgt + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
            const float accel = fpu_mul(lwc1(L1_ADDR(0x00161954)), dt());
            sw(pv + 0x1b8, 0);
            rate = fpu_div(lwc1(pv + 0x1b8), accel);
            g = dt_sq();
        }
        swc1(m + 0x44, angle_seek_rate(pv + 0x1bc, lwc1(m + 0x44), fpu_mul(rate, g_rate), fpu_mul(g, kTwoPi), fpu_mul(g, kPi), fpu_mul(t, kTwoPi)));
        tail = Tail::Sparks;
        if (lbu(m + 0x53) != 0xa && lwc1(pv + 0x1b8) <= 0.0f &&
            angle_dist(lwc1(m + 0x48), heading_to(m, lwc1(tgt), lwc1(tgt + 4))) < u2f(0x3d0efa35u)) {
            swc1(pv + 0x1b8, 0.0f);
            anim_to(m, 0xa, 3);
            break;
        }
        if (lbu(m + 0x52) == 0xa && (lbu(m + 0x70) & 2) != 0) to_aiming(6);
        break;
    }
    case 0xc:  // (0x002e8604) knocked back: lands on its path (takes off again for the picked point) or blows up out of the level
        if ((enemy_physics(m, pv + 0x70) & 0x40) != 0) {
            if (0.25f < vec3_distance(V(pos), V(lwu(pv + 0x160) + (lwu(pick) << 4) + 0x10))) {
                anim_to(m, 8, 0xa);
                sw(pv + 0x150, lwu(pick));
                sb(m + 0x20, 9);
                swc1(m + 0x58, lwc1(L1_ADDR(0x0016195c)));
                tail = Tail::Gone;  // (no tail this frame)
                break;
            }
            to_aiming(6);
        }
        if (out_of_level()) {
            explode();
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
            tail = Tail::Gone;
        }
        break;
    case 0xe:  // (0x002e87b8) dies: blows up on landing (or out of the level), its pieces and bolts left
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        if ((enemy_physics(m, pv + 0x70) & 6) != 0 || out_of_level()) {
            explode();
            debris();
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
            tail = Tail::Gone;
        }
        break;
    default:
        break;
    }
    if (tail == Tail::Gone) {
        ee_stack_free(0x180);
        return;
    }
    if (tail == Tail::Sparks) bird_sparks_l1(m, pos0);

    // ---- the tail: the colour pulse, gravity in the walking states, the floor, blown up below it
    color_pulse_update(m, pv + 0x60);
    swc1(pv + 0x1cc, frame);
    const uint32_t state = lbu(m + 0x20);
    float floor;
    if (state == 1 || state == 5 || state == 6 || state == 7 || state == 0xb) {
        const float vz = fpu_add(lwc1(pv + 0xe8), fpu_mul(dt_sq(), u2f(0x411ccccdu)));
        swc1(pv + 0xe8, vz);
        swc1(pos + 8, fpu_sub(lwc1(pos + 8), fpu_sub(vz, 2.0f)));
        floor = floor_probe_v(0.5f, V(pos), 0);
        const float z = fpu_sub(lwc1(pos + 8), 2.0f);
        if (z < floor) {
            swc1(pos + 8, floor);
            sw(pv + 0xe8, 0);
        } else {
            swc1(pos + 8, z);
        }
    } else {
        floor = floor_probe_v(0.5f, V(pos), 0);
    }
    moby_extent_set_l1(m, floor);
    if (sub_001f35d8() == 0 && lwc1(pos + 8) < fpu_add(floor, 1.0f)) {
        explode();
        debris();
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
    }
    ee_stack_free(0x180);
}

}  // namespace rac1
