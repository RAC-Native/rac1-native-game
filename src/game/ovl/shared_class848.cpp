// Class 848, a helicopter's gunfire, and its bullet: one port of the copies Kerwan, Umbris and Quartu carry (the same code relinked). Each level
// keeps one word of its own (the turning rate: per level below); the rest is the engine's (OV: the collision's hit point
// 0x173e60, normal 0x173e80 and moby 0x173e58) or the base ELF's (bare: the sound slots 0x13e550, the hero's moby 0x1413d0,
// the frame time 0x15ed6c). Header shared_class848.h.
//
// The pvars: +0 the velocity, +0xc the height it keeps to, +0x10 its owner (the helicopter), +0x14 the timer, +0x18 the gun
// that fired last (0 / 1).
#include <game/ovl/shared_class848.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
// the level's turning rate (degrees a frame)
uint32_t turn_rate() {
    if (on_level(UMBRIS)) return L7_ADDR(0x00161ab8);
    if (on_level(QUARTU)) return L15_ADDR(0x00161e40);
    return L3_ADDR(0x00161b30);
}

constexpr float HALF_PI = 1.5707963705062866f;  // (0x3fc90fdb)

// The guns' sound: the sound in the slot `slot` (a word of the owner's pvars) stopped when it is still the owner's
void sound_end(uint32_t slot, uint32_t owner) {
    const int32_t id = lw(slot);
    if (id != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(id) * 0x70;
        if (lwu(e + 0x88) == owner && lbu(e + 0x74) != 0) sub_0028d918(id);
    }
    sw(slot, 0xffffffffu);
}

// what the last collision met ends the burst: the hero's moby or a moby of class 0x1df
bool hit_ends(uint32_t m) {
    const uint32_t hit = lwu(OV(0x00173e58));
    if (hit == 0) return false;
    if (hit == lwu(0x001413d0) || lh(hit + 0xa6) == 0x1df) {
        sb(m + 0x20, 2);
        return true;
    }
    return false;
}

// a random direction in a cube (each axis -1 .. 1)
void rand_cube(uint32_t out) {
    V(out) = Vec4{};
    swc1(out, rand_range_f(-1.0f, 1.0f));
    swc1(out + 4, rand_range_f(-1.0f, 1.0f));
    swc1(out + 8, rand_range_f(-1.0f, 1.0f));
}
}  // namespace

// orig: L3:0x00260130 L7:0x0029a018 L15:0x00261560
void class848_bullet(uint32_t owner, uint32_t to, uint32_t from, uint32_t vel, int32_t life) {
    const uint32_t p = alloc_particle(0x31);
    if (p == 0) return;
    sw(p + 0xc, 0x0fffffff);
    sw(p + 4, 0x7fffffff);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x70));
    sb(p + 1, 2);
    lq_sq(p + 0x10, from);
    lq_sq(p + 0x20, to);
    swc1(p + 0x30, lwc1(vel));
    swc1(p + 0x34, lwc1(vel + 4));
    sw(p + 0x3c, owner);
    swc1(p + 0x38, lwc1(vel + 8));
    sh(p + 0xa, static_cast<uint32_t>(life));
}

// State 1 runs: it keeps to its height (at most 8 a frame), turns towards the target (the nearest within 20) while it is
// ahead, moves, keeps the guns' sound going while the target is below the owner's line of sight (the owner's +0x250 /
// +0x1b8 slot, for its classes 631 / 233), and every sixth frame then fires a bullet and sparks from one of the owner's
// guns (its joints 3 / 2 by turns), more sparks flying off the scenery where the line of fire meets it; it ends (state 2)
// when the fire meets the hero or a moby of class 0x1df, its timer runs out or its owner is gone. State 2 stops the sound
// and goes. An owner of class 233 in its states 0x40 / 1 ends it at once.
// orig: L3:0x002d47c0 L7:0x00304ff0 L15:0x002de430
void class848_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x210);
    const uint32_t target = sp, rec = sp + 0x60, way = sp + 0x90, gun = sp + 0xa0, out = sp + 0xb0, far = sp + 0xc0;
    const uint32_t side = sp + 0xd0, beside = sp + 0xe0, spark = sp + 0xf0, dir = sp + 0x100, push = sp + 0x110;
    const uint32_t back = sp + 0x120, heading = sp + 0x130;
    target_select_any(m, target, 20.0f);
    uint32_t slot = 0;  // (the guns' sound slot in the owner's pvars)
    {
        const uint32_t owner = lwu(pv + 0x10);
        const int32_t cls = lh(owner + 0xa6);
        if (cls == 0x277) {
            slot = lwu(owner + 0x78) + 0x250;
        } else if (cls == 0xe9) {
            slot = lwu(owner + 0x78) + 0x1b8;
            const uint32_t st = lbu(owner + 0x20);
            if (st == 0x40 || st == 1) sb(m + 0x20, 2);
        }
    }
    const uint32_t state = lbu(m + 0x20);
    if (state == 2) {
        if (slot != 0) sound_end(slot, lwu(pv + 0x10));
        moby_retire(m);
        ee_stack_free(0x210);
        return;
    }
    if (state != 1) {
        ee_stack_free(0x210);
        return;
    }
    vec2_set_length(V(way), V(pv), 1.0f);
    swc1(pv + 8, 1.0f);
    sub_00259888(2.0f, rec, static_cast<int32_t>(m), 0x10003, way);
    vec2_set_length(V(rec), V(rec), 1.0f);
    // (the hit record for the sweeps: +0 the push, +8 / +0xc, +0x18 / +0x19 the state, +0x1a the class)
    swc1(rec + 8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));  // (5627.92)
    sb(rec + 0x19, state);
    sh(rec + 0x1a, lhu(m + 0xa6));
    sb(rec + 0x18, state);
    lq_sq(sp + 0x50, m + 0x10);
    const uint32_t pos = m + 0x10;
    // the climb: towards its height, a tenth of the way a frame, at most 8 (scaled)
    {
        const float most = fpu_mul(lwc1(0x0015ed6c), 8.0f);
        float climb = fpu_div(-fpu_sub(lwc1(m + 0x18), lwc1(pv + 0xc)), 10.0f);
        if (most < climb) climb = most;
        else if (climb < -most) climb = -most;
        swc1(pv + 8, climb);
    }
    swc1(heading, atan2_f(lwc1(pv), lwc1(pv + 4)));
    const float bearing = atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
    if (angle_dist(lwc1(heading), bearing) < HALF_PI) {
        const float to = atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
        turn_towards(heading, 0, to, fpu_mul(fpu_mul(lwc1(turn_rate()), u2f(0x3c8efa35u)), lwc1(0x0015ed6c)));
        swc1(pv, fpu_mul(cos_vu(lwc1(heading)), vec2_length(V(pv))));
        swc1(pv + 4, fpu_mul(sin_vu(lwc1(heading)), vec2_length(V(pv))));
    }
    vec3_add(V(pos), V(pos), V(pv));
    const float reach = vec2_distance(V(pos), V(lwu(pv + 0x10) + 0x10));
    // the guns' sound while the target is below the owner's line of sight
    if (slot != 0) {
        const uint32_t owner = lwu(pv + 0x10);
        if (fpu_sub(lwc1(owner + 0x18), lwc1(target + 8)) < reach) {
            if (sub_0028d8c0(owner, lw(slot)) == 0) sw(slot, static_cast<uint32_t>(moby_sound_start(0, 4, lwu(pv + 0x10))));
        } else {
            sound_end(slot, owner);
        }
    }
    if (fpu_sub(lwc1(lwu(pv + 0x10) + 0x18), lwc1(target + 8)) < reach && ee_rem(lw(pv + 0x14), frame_scale_i(6)) == 0) {
        // the guns by turns: the aim a quarter to that side of the heading
        sw(pv + 0x18, lwu(pv + 0x18) ^ 1u);
        swc1(side, fpu_mul(cos_vu(angle_add(lwc1(heading), lw(pv + 0x18) != 0 ? HALF_PI : -HALF_PI)), 0.25f));
        swc1(side + 4, fpu_mul(sin_vu(angle_add(lwc1(heading), lw(pv + 0x18) != 0 ? HALF_PI : -HALF_PI)), 0.25f));
        sw(side + 8, 0);
        vec3_add(V(beside), V(pos), V(side));
        moby_chain_point_v(lwu(pv + 0x10), 3u - lwu(pv + 0x18), V(gun));
        vec3_sub(V(out), V(beside), V(gun));
        vec3_scale(V(far), V(out), 2.0f);
        vec3_add(V(far), V(far), V(gun));
        // five sparks from the gun along the line of fire, spread
        for (int32_t i = 0; i < 5; ++i) {
            rand_cube(dir);
            lq_sq(spark, dir);
            vec3_set_length(V(spark), V(spark), fpu_mul(vec3_length(V(out)), u2f(0x3f266666u)));  // (0.65)
            vec3_add(V(spark), V(out), V(spark));
            const float ft = lwc1(0x0015ed6c);
            vec3_set_length(V(spark), V(spark), rand_range_f(fpu_mul(ft, 5.0f), fpu_mul(ft, 15.0f)));
            const int32_t lo = frame_scale_i(5);
            const int32_t n = rand_range(lo, frame_scale_i(10));
            spawn_particle_1b(gun, spark, 0x5f2f4f6f, static_cast<uint32_t>(n), 50000.0f);
        }
        vec3_set_length(V(out), V(out), fpu_mul(lwc1(0x0015ed6c), 0.5f));
        {
            const int32_t lo = frame_scale_i(4);
            const int32_t n = rand_range(lo, frame_scale_i(7));
            spawn_particle_1b(gun, out, 0x2f4f7f7f, static_cast<uint32_t>(n), 400000.0f);
        }
        // the bullet: from the gun towards six frames ahead of the aim (less the side)
        vec3_scale(V(dir), V(pv), frame_scale_f(6.0f));
        vec3_add(V(dir), V(dir), V(pos));
        vec3_sub(V(dir), V(dir), V(side));
        vec3_sub(V(push), V(dir), V(gun));
        vec3_set_length(V(dir), V(push), 2.0f);
        vec3_sub(V(back), V(gun), V(dir));
        lq_sq(spark, back);
        vec3_add(V(dir), V(dir), V(spark));
        vec3_set_length(V(push), V(push), fpu_mul(lwc1(0x0015ed6c), 100.0f));
        class848_bullet(lwu(pv + 0x10), spark, dir, push, frame_scale_i(0x2d));
        if (segment_collide_v(V(gun), V(far), 0, lwu(pv + 0x10), nullptr) != 0) {
            // ten sparks off the scenery where the line of fire meets it
            lq_sq(gun, OV(0x00173e60));
            vec3_set_length(V(out), V(out), fpu_mul(lwc1(0x0015ed6c), 10.0f));
            for (int32_t i = 0; i < 10; ++i) {
                rand_cube(dir);
                lq_sq(spark, dir);
                vec3_reflect(V(dir), V(out), V(OV(0x00173e80)));
                vec3_set_length(V(spark), V(spark), fpu_mul(vec3_length(V(dir)), 0.5f));
                vec3_add(V(dir), V(dir), V(spark));
                const float ft = lwc1(0x0015ed6c);
                vec3_set_length(V(dir), V(dir), rand_range_f(fpu_mul(ft, 3.0f), fpu_mul(ft, 6.0f)));
                const int32_t lo = frame_scale_i(0xf);
                const int32_t n = rand_range(lo, frame_scale_i(0x14));
                spawn_particle_1b(gun, dir, 0x7f2f4f6f, static_cast<uint32_t>(n), 30000.0f);
            }
            // the hit there: a short sweep back along the line, one up, and the aim's own sphere
            vec3_set_length(V(out), V(out), -1.0f);
            vec3_add(V(far), V(gun), V(out));
            const uint32_t* record = static_cast<const uint32_t*>(ee_ram_ptr(rec));
            if (segment_collide_v(V(gun), V(far), 0x10, lwu(pv + 0x10), record) != 0) hit_ends(m);
            lq_sq(far, gun);
            swc1(far + 8, fpu_add(lwc1(far + 8), 2.0f));
            if (segment_collide_v(V(gun), V(far), 0x10, lwu(pv + 0x10), record) != 0) hit_ends(m);
            if (mobys_in_sphere(pos, 0.5f, 0x10, m, rec) != 0) hit_ends(m);
        }
    }
    // the timer, the owner gone or dying: the end
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) {
        sb(m + 0x20, 2);
    } else {
        const uint32_t owner = lwu(pv + 0x10);
        if (owner == 0 || lbu(owner + 0x20) == 0xfe || lbu(owner + 0x20) == 0xfd) sb(m + 0x20, 2);
    }
    ee_stack_free(0x210);
}

}  // namespace rac1
