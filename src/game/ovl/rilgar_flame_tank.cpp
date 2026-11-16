// Rilgar's (level 5) flame tank (class 625): a tracked machine (its two treads are mobys of classes 0x368 / 0x369 that
// move with it) that drives back and forth along its path, turns its turret at the hero and, when he is in range, in
// its area and in sight, sprays fire at him; it blows up in three blasts and eight pieces. Ported by hand from Rilgar's
// own code (L5:0x304320, 0x305178, 0x305898), the original's order of calls and float operations kept.
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;      // the hero's block (+0x80 his position, +0xd0 his centre, +0x1158 his mode)
constexpr uint32_t kHeroMoby = 0x001413d0;  // (the word holding the hero's moby)
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float deg() { return u2f(0x3c8efa35u); }    // (one degree in radians)
}  // namespace

// The tank's hit: the largest of the hits on it and on its two treads (mask 0x210000), none from the hero himself
// (class 0) or from class 0x47; its health +0x20 down by the damage: still alive, a hit sound, +0x67 200 and the colour
// pulse; out of health (or a hit of no damage), the death: the sound, its bolts, three blasts (at it 3 up, 2 ahead, 2
// behind), the turret's timer +0x19e, state 3 and eight pieces (0x6dd, 0x6df, 0x6e0, 0x6fe x2, 0x6ff x3). Then the
// treads' hit slots cleared, the alert (+0x38: 4 seconds of 20 range, +0x178; seen from behind, the patrol turns
// round), the range +0x174 (14 / 20), the treads moved with it (their animation speed +0x58 from its speed, the turn
// +0x84 added on one, taken off the other) and the colour pulse.
// orig: L5:0x00305178
void ril_305178_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t hit_state = sp + 0x40, hit_dmg = sp + 0x44, at = sp + 0x20;
    sw(hit_dmg, 0);
    const uint32_t h0 = sub_0025a420(m, NOT_ADDR(0x210000u), 0);
    const uint32_t t0 = lwu(pv + 0x184);
    const uint32_t h1 = t0 != 0 ? sub_0025a420(t0, NOT_ADDR(0x210000u), 0) : 0;
    const uint32_t t1 = lwu(pv + 0x188);
    const uint32_t h2 = t1 != 0 ? sub_0025a420(t1, NOT_ADDR(0x210000u), 0) : 0;
    uint32_t hit = moby_largest(h0, h1, h2, 0);
    if (hit != 0) {
        hit = lwu(hit + 0x20) != m ? hit : 0;  // (its own hit on itself: none)
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0) {
            const int32_t cls = lh(src + 0xa6);
            if (cls == 0x47 || cls == 0) hit = 0;
        }
    }
    moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 3) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (0.0f < health && 0.0f < lwc1(hit_dmg)) {
            moby_sound_start(2, 0, m);
            sb(pv + 0x67, 0xc8);
            color_pulse_start(m, pv + 0x60);
        } else {  // the death
            moby_sound_start(3, 0, m);
            moby_drop_bolts(m, 0, -1);
            auto blast = [&] { explosion_effect(m, pv + 0x40, at, 0x14, 8, 0x14, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f); };
            lq_sq(at, m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), 3.0f));
            blast();
            for (int32_t side = 0; side < 2; ++side) {  // 2 ahead, then 2 behind
                lq_sq(at, m + 0x10);
                const float yaw = side == 0 ? lwc1(m + 0x48) : angle_add(lwc1(m + 0x48), u2f(0x40490fdbu));
                const float c = cos_vu(yaw);
                swc1(at, fpu_add(lwc1(at), fpu_add(c, c)));
                const float s = sin_vu(side == 0 ? lwc1(m + 0x48) : angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
                swc1(at + 4, fpu_add(lwc1(at + 4), fpu_add(s, s)));
                blast();
            }
            sh(pv + 0x19e, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x00161b74u)))));
            sb(m + 0x20, 3);
            vec_clear(V(sp + 0x30));
            swc1(sp + 0x38, lwc1(pv + 0x194));
            const uint32_t zero = L5_ADDR(0x0015f580u);  // (a zero vector)
            for (const uint32_t cls : {0x6ddu, 0x6dfu, 0x6e0u, 0x6feu, 0x6feu, 0x6ffu, 0x6ffu, 0x6ffu})
                debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            ee_stack_free(0x130);
            return;
        }
    }
    sb(m + 0xa4, 0xff);
    if (lwu(pv + 0x184) != 0) sb(lwu(pv + 0x184) + 0xa4, 0xff);
    if (lwu(pv + 0x188) != 0) sb(lwu(pv + 0x188) + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0 && lw(pv + 0x178) == 0) {  // alerted: 4 seconds of more range; from behind, the patrol turns
        sw(pv + 0x178, static_cast<uint32_t>(frame_scale_i(0xf0)));
        sw(pv + 0x38, 0);
        const float a = atan2_f(fpu_sub(lwc1(kHero + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(kHero + 0x84), lwc1(m + 0x14)));
        if (u2f(0x3fc90fdbu) < angle_dist(lwc1(m + 0x48), a)) sb(pv + 0xc4, static_cast<uint32_t>(-static_cast<int8_t>(lbu(pv + 0xc4))));
    }
    swc1(pv + 0x174, timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x178))) == 0 ? 14.0f : 20.0f);
    for (int32_t k = 0; k < 2; ++k) {  // the treads: their animation speed from the tank's, its turn added / taken off
        const uint32_t slot = pv + (k == 0 ? 0x184 : 0x188);
        if (lwu(slot) == 0) continue;
        const float speed = vec3_length(V(pv + 0x40));
        const float per = fpu_mul(lwc1(L5_ADDR(0x00161af0u)), dt());
        const float turn = lwc1(L5_ADDR(0x00161af4u));
        swc1(lwu(slot) + 0x58, fpu_div(speed, per));
        const float t = fpu_mul(lwc1(pv + 0x84), turn);
        swc1(lwu(slot) + 0x58, k == 0 ? fpu_add(lwc1(lwu(slot) + 0x58), t) : fpu_sub(lwc1(lwu(slot) + 0x58), t));
        lq_sq(lwu(slot) + 0x40, m + 0x40);
        lq_sq(lwu(slot) + 0x10, m + 0x10);
    }
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x130);
}

// The fire's damage: the drops it keeps (8 slots from +0x1a0: a particle 0x02 still alive, its +0x32) are kept, the
// others let go; unless the hero is in a vehicle (mode 6), each kept drop burns what is within 0.75 of it (a damage
// record of the tank's class, pushing 1.5 away from the tank, up 1).
// orig: L5:0x00305898
void ril_305898_l5(uint32_t m) {
    const uint32_t slots = lwu(m + 0x78) + 0x1a0;
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t s = slots + i * 4;
        const uint32_t p = lwu(s);
        if (p == 0) continue;
        if (lbu(p) != 2 || lh(p + 0x32) == 0) sw(s, 0);
    }
    if (lw(kHero + 0x1158) == 6) return;
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t rec = sp;  // (the damage record, 0x24 bytes; its +0x20 or'd with 1 as the original's stack word is)
    sw(rec + 0x20, lwu(rec + 0x20) | 1u);
    sw(rec + 0x10, m);
    sw(rec + 0x14, 1);
    swc1(rec + 0x1c, 1.0f);
    vec3_sub(V(rec), V(kHero + 0x80), V(m + 0x10));
    sw(rec + 8, 0);
    vec2_set_length(V(rec), V(rec), 1.5f);
    swc1(rec + 8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));  // (5627.92)
    sb(rec + 0x19, 1);
    sh(rec + 0x1a, lhu(m + 0xa6));
    sb(rec + 0x18, 0);
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t p = lwu(slots + i * 4);
        if (p != 0) mobys_in_sphere(p + 0x10, 0.75f, 0, m, rec);
    }
    ee_stack_free(0x90);
}

// Class 625, the flame tank. pvars: health +0x20, the move record +0x40 / +0x70 (its turn +0x84), the hit's colour
// pulse +0x60, the patrol (+0xc0: the point, its direction +0xc4, the path +0xd0; the path's index +0xd4), the turret's
// joints +0x100 / +0x140 (yaw +0x194, pitch +0x198), its range +0x174 and alert +0x178, the treads +0x184 / +0x188, the
// area the hero must be in +0x190 (a path's polygon; -1: anywhere), the fire's drops +0x1a0 (8). States: 0 set up (the
// treads spawned); 1 patrols (turret on the hero within 16); 2 sprays fire while he is in range, within 4 in height,
// in its area and in sight; 3 dead (+0x2e = 2: gone with its treads).
// orig: L5:0x00304320
void ril_class625_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    ril_305178_l5(m);
    ril_305898_l5(m);
    auto retire_all = [&] {
        if (lwu(pv + 0x184) != 0) moby_retire(lwu(pv + 0x184));
        if (lwu(pv + 0x188) != 0) moby_retire(lwu(pv + 0x188));
        moby_retire(m);
    };
    if (lbu(pv + 0x2e) == 2) {
        retire_all();
        return;
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L5_ADDR(0x001671c0u))) < 27.0f) {  // near the camera
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t pos = m + 0x10;
    // the patrol's step: toward the next point of the path (wrapping), past the end of an open path it turns round
    auto patrol = [&] {
        const uint32_t rec = pv + 0xc0;
        const uint32_t path = lwu(rec + 0x10);
        const int32_t n = lw(path);
        const int32_t dir = static_cast<int8_t>(lbu(rec + 4));
        const int32_t end = dir < 0 ? 0 : n - 1;
        const int32_t next = ee_rem(lw(rec) + n + dir, n);
        const uint32_t off = 0x10 + (static_cast<uint32_t>(next) << 4);
        doomlet_259028(m, pv + 0x70, path + off, pv + 0x40);
        if (!(vec2_distance(V(pos), V(lwu(rec + 0x10) + off)) < 1.0f)) return;
        if (next == end) {
            const uint32_t p = lwu(rec + 0x10);
            if (5.0f < vec3_distance(V(p + (lwu(p) << 4)), V(p + 0x10)))  // (an open path: its ends apart)
                sb(rec + 4, static_cast<uint32_t>(-static_cast<int8_t>(lbu(rec + 4))));
        }
        sw(rec, static_cast<uint32_t>(next));
    };
    // the hero in its range (+0x174), within 4 in height, in its area (+0x190) and in sight from `eye`
    // (firing: the original's tests read the other way round, `range < distance` / `4 <= height`, equal values keep it)
    auto hero_targeted = [&](uint32_t eye, bool firing) {
        const float d = vec2_distance(V(pos), V(kHero + 0x80));
        if (firing ? lwc1(pv + 0x174) < d : !(d < lwc1(pv + 0x174))) return false;
        const float dz = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(kHero + 0x88)));
        if (firing ? 4.0f <= dz : !(dz < 4.0f)) return false;
        const int32_t area = lw(pv + 0x190);
        if (area != -1) {
            const uint32_t p = lvd::path(area);
            if (point_in_polygon(kHero + 0x80, p + 0x10, lw(p)) == 0) return false;
        }
        lq_sq(eye, pos);
        swc1(eye + 8, fpu_add(lwc1(eye + 8), 3.0f));
        return segment_collide(eye, kHero + 0xd0, 2, m, 0) == 0;
    };
    // the turret: turned to `yaw` / `pitch` (90 degrees a second), its two joints set
    auto turret = [&](float yaw, float pitch) {
        const float rate = fpu_mul(dt(), u2f(0x3fc90fdbu));
        turn_towards(pv + 0x194, 0, yaw, rate);
        turn_towards(pv + 0x198, 0, pitch, fpu_mul(dt(), u2f(0x3fc90fdbu)));
        quat_from_axis_angle(V(pv + 0x140), 1, lwc1(pv + 0x198));
        quat_from_axis_angle(V(pv + 0x100), 2, angle_sub(lwc1(pv + 0x194), lwc1(m + 0x48)));
    };
    auto hero_yaw = [&] { return atan2_f(fpu_sub(lwc1(kHero + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(kHero + 0x84), lwc1(m + 0x14))); };
    const float tilt = u2f(0x3db2b8c2u);  // (5 degrees)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x00304444) set up: the treads, the turret's joints, the walker, the patrol from the path's first point
        for (int32_t k = 0; k < 2; ++k) {
            const uint32_t slot = pv + (k == 0 ? 0x184 : 0x188);
            if (lwu(slot) != 0) continue;
            sw(slot, moby_spawn(k == 0 ? 0x368 : 0x369));
            sb(lwu(slot) + 0x30, 0x40);
            sh(lwu(slot) + 0x32, 0x40);
            sb(lwu(slot) + 0x31, 1);
            sd(lwu(slot) + 0x38, ld(lwu(kHeroMoby) + 0x38));
            sh(lwu(slot) + 0x34, 0x4000);
            lq_sq(lwu(slot) + 0x10, pos);
            sw(lwu(slot) + 0x94, 0);
            moby_update_transform(lwu(slot));
        }
        sub_0024f590(m, 0, pv + 0xf0);
        sub_0024f590(m, 1, pv + 0x130);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sd(m + 0x38, ld(lwu(kHeroMoby) + 0x38));
        walker_record_init_l1(pv + 0x70);
        sw(pv + 0x70, 0x38d);
        swc1(pv + 0x7c, 2.0f);
        swc1(pv + 0x78, 4.0f);
        swc1(pv + 0xac, u2f(0x3c23d70au));  // (0.01)
        swc1(pv + 0xb0, u2f(0x3e99999au));  // (0.3)
        swc1(pv + 0xb4, u2f(0x3dcccccdu));  // (0.1)
        const uint32_t path = lvd::path(lw(pv + 0xd4));
        sw(pv + 0xc0, 1);
        sw(pv + 0xd0, path);
        lq_sq(pos, path + 0x10);
        sb(m + 0x20, 1);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        swc1(pv + 0x94, fpu_mul(lwc1(L5_ADDR(0x00161af8u)), dt()));
        vec_clear(V(pv + 0x40));
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 0);
        sh(pv + 0x24, 6);
        sb(pv + 0x28, 3);
        swc1(pv + 0x20, 6.0f);
        sh(pv + 0x3e, lhu(pv + 0x3e) | 2u);
        sw(pv + 0x180, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(pv + 0x198, 0);
        swc1(pv + 0x194, lwc1(m + 0x48));
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(lwc1(L5_ADDR(0x00161b78u)))));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(L5_ADDR(0x00161b7cu)), 8.0f))));
        break;
    }
    case 1:  // (0x0030469c) patrols; the hero targeted: fires (2)
        patrol();
        if (hero_targeted(sp + 0x10, false)) {
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 3);
            sb(m + 0x20, 2);
        }
        if (vec2_distance(V(pos), V(kHero + 0x80)) < 16.0f) turret(hero_yaw(), tilt);
        else turret(lwc1(m + 0x48), 0.0f);
        break;
    case 2: {  // (0x00304968) patrols and sprays fire from the nozzle (chain point 2)
        patrol();
        const uint32_t nozzle = sp + 0x10, vel = sp + 0x20, end = sp + 0x30, drop = sp + 0x40;
        swc1(vel + 0xc, lwc1(L5_ADDR(0x00161b30u)));  // (the two ends' sizes)
        swc1(end + 0xc, lwc1(L5_ADDR(0x00161b34u)));
        moby_chain_point(m, 2, nozzle);
        uint32_t taken = 0;
        for (int32_t k = 0; k < 2; ++k) {  // the flame: two drops of the first colours, the first one each 8 frames kept
            lq_sq(drop, nozzle);
            const float jit = u2f(0x3d851eb8u);  // (0.065)
            swc1(drop, fpu_add(lwc1(drop), rand_range_f(-jit, jit)));
            swc1(drop + 4, fpu_add(lwc1(drop + 4), rand_range_f(-jit, jit)));
            swc1(drop + 8, fpu_add(lwc1(drop + 8), rand_range_f(-jit, jit)));
            swc1(vel, fpu_mul(cos_vu(lwc1(pv + 0x194)), fpu_mul(lwc1(L5_ADDR(0x00161b20u)), dt())));
            const float s1 = sin_vu(lwc1(pv + 0x194));
            const float up1 = fpu_mul(lwc1(L5_ADDR(0x00161b24u)), dt());
            sw(vel + 8, 0);
            swc1(vel + 4, fpu_mul(s1, fpu_mul(lwc1(L5_ADDR(0x00161b20u)), dt())));
            swc1(vel + 8, rand_range_f(fpu_mul(up1, u2f(0x3f666666u)), fpu_mul(up1, u2f(0x3f8ccccdu))));  // (0.9 .. 1.1)
            const float sp2 = lwc1(L5_ADDR(0x00161b38u));
            const float a = fpu_mul(rand_range_f(-sp2, sp2), deg());
            swc1(end, fpu_mul(cos_vu(angle_add(lwc1(pv + 0x194), a)), fpu_mul(lwc1(L5_ADDR(0x00161b28u)), dt())));
            const float s2 = sin_vu(angle_add(lwc1(pv + 0x194), a));
            const float up2 = fpu_mul(lwc1(L5_ADDR(0x00161b2cu)), dt());
            sw(end + 8, 0);
            swc1(end + 4, fpu_mul(s2, fpu_mul(lwc1(L5_ADDR(0x00161b28u)), dt())));
            swc1(end + 8, rand_range_f(fpu_mul(up2, u2f(0x3f666666u)), fpu_mul(up2, u2f(0x3f8ccccdu))));
            const float n0 = lwc1(L5_ADDR(0x00161b14u));
            const uint32_t n = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(n0, fpu_add(n0, n0)))));
            const float h0 = lwc1(L5_ADDR(0x00161b18u));
            const uint32_t h = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(h0, fpu_mul(h0, u2f(0x3f99999au))))));
            const float l0 = lwc1(L5_ADDR(0x00161b1cu));
            const uint32_t life = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(l0, fpu_mul(l0, u2f(0x3f99999au))))));
            const uint32_t p = spawn_particle_02(drop, vel, end, lwu(L5_ADDR(0x00161b0cu)), lwu(L5_ADDR(0x00161b10u)), n, h, life, -1);
            if ((lwu(L5_ADDR(0x0015f5ccu)) & 7u) == 0 && p != 0 && taken == 0) {  // kept in the first free of the 8 slots
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(pv + 0x1a0 + i * 4) != 0) continue;
                    sw(pv + 0x1a0 + i * 4, p);
                    break;
                }
            }
            ++taken;
        }
        swc1(vel + 0xc, lwc1(L5_ADDR(0x00161b60u)));
        swc1(end + 0xc, lwc1(L5_ADDR(0x00161b64u)));
        for (int32_t k = 0; k < 2; ++k) {  // the smoke: two drops of the second colours, wider and slower
            lq_sq(drop, nozzle);
            const float jit = u2f(0x3e4ccccdu);  // (0.2)
            swc1(drop, fpu_add(lwc1(drop), rand_range_f(-jit, jit)));
            swc1(drop + 4, fpu_add(lwc1(drop + 4), rand_range_f(-jit, jit)));
            swc1(drop + 8, fpu_add(lwc1(drop + 8), rand_range_f(-jit, jit)));
            const float sp1 = lwc1(L5_ADDR(0x00161b6cu));
            const float a1 = fpu_mul(rand_range_f(-sp1, sp1), deg());
            swc1(vel, fpu_mul(cos_vu(angle_add(lwc1(pv + 0x194), a1)), fpu_mul(lwc1(L5_ADDR(0x00161b50u)), dt())));
            const float s1 = sin_vu(angle_add(lwc1(pv + 0x194), a1));
            const float up1 = fpu_mul(lwc1(L5_ADDR(0x00161b54u)), dt());
            sw(vel + 8, 0);
            swc1(vel + 4, fpu_mul(s1, fpu_mul(lwc1(L5_ADDR(0x00161b50u)), dt())));
            swc1(vel + 8, rand_range_f(fpu_mul(up1, u2f(0x3f333333u)), fpu_mul(up1, 1.5f)));  // (0.7 .. 1.5)
            const float sp2 = lwc1(L5_ADDR(0x00161b68u));
            const float a2 = fpu_mul(rand_range_f(-sp2, sp2), deg());
            swc1(end, fpu_mul(cos_vu(angle_add(lwc1(pv + 0x194), a2)), fpu_mul(lwc1(L5_ADDR(0x00161b58u)), dt())));
            const float s2 = sin_vu(angle_add(lwc1(pv + 0x194), a2));
            const float up2 = fpu_mul(lwc1(L5_ADDR(0x00161b5cu)), dt());
            sw(end + 8, 0);
            swc1(end + 4, fpu_mul(s2, fpu_mul(lwc1(L5_ADDR(0x00161b58u)), dt())));
            swc1(end + 8, rand_range_f(fpu_mul(up2, u2f(0x3f333333u)), fpu_mul(up2, 1.5f)));
            const float n0 = lwc1(L5_ADDR(0x00161b44u));
            const uint32_t n = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(n0, fpu_add(n0, n0)))));
            const float h0 = lwc1(L5_ADDR(0x00161b48u));
            const uint32_t h = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(h0, fpu_mul(h0, u2f(0x3f99999au))))));
            const float l0 = lwc1(L5_ADDR(0x00161b4cu));
            const uint32_t life = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(l0, fpu_mul(l0, u2f(0x3f99999au))))));
            spawn_particle_02(drop, vel, end, lwu(L5_ADDR(0x00161b3cu)), lwu(L5_ADDR(0x00161b40u)), n, h, life, -1);
        }
        if (!hero_targeted(drop, true)) {  // out of range / height / area / sight: back to patrolling
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 3);
            sb(m + 0x20, 1);
        }
        turret(hero_yaw(), tilt);
        break;
    }
    case 3:  // (0x003050fc) dead: gone with its treads
        retire_all();
        break;
    default:
        break;
    }
    ee_stack_free(0x150);
}

}  // namespace rac1
