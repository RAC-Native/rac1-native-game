// The space combat's shots, classes 1009 (homing, with its target search) and 1017 (straight): one port of the copies
// Pokitaru, Gemlik Base and Drek's fleet carry (the same code relinked). Class 1017's trail settings are the level's own
// (per level below); the rest is the engine's (OV: the moby table 0x15ffd8, the target list 0x15ffe4, the frame counter
// 0x15f5cc, the collision's normal 0x173e80) or the base ELF's (bare: the frame time 0x15ed6c). Header shared_ship_shots.h.
//
// The pvars: +0 the velocity, +0x10 the glow (class 1009: x 24 the moby's +0x23; it fades by 0.74 each time the timer runs
// out, the shot gone under 0.02), +0x18 the timer, +0x1c the moby its line of flight ignores (its owner), +0x20 (1009) where
// the target was last seen / (1017) the hit record's reach, +0x2c (1009) the target.
#include <game/ovl/shared_ship_shots.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr float CONE = 0.2617993950843811f;  // (15 degrees; 0x3e860a92)

// Class 1017's trail (the level's own, 0x14 bytes): +0 / +4 its sparks' colours (from, to), +8 their frames, +0xc their
// size (an integer), +0x10 their speed (a part of the shot's)
uint32_t trail() {
    if (on_level(GEMLIK)) return L13_ADDR(0x00161d20);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161db0);
    return L11_ADDR(0x00161dc0);
}

// Five sparks where a shot met something: its velocity `vel` reflected off the surface (the collision's normal), spread by
// half its length in a random direction, at 5 .. 9 a second, living 10 .. 15 frames (`spark` / `bounce` the stack's
// vectors)
void sparks(uint32_t pos, uint32_t vel, uint32_t spark, uint32_t bounce) {
    for (int32_t k = 0; k < 5; ++k) {
        V(bounce) = Vec4{};
        swc1(bounce, rand_range_f(-1.0f, 1.0f));
        swc1(bounce + 4, rand_range_f(-1.0f, 1.0f));
        swc1(bounce + 8, rand_range_f(-1.0f, 1.0f));
        lq_sq(spark, bounce);
        vec3_reflect(V(bounce), V(vel), V(OV(0x00173e80)));
        vec3_set_length(V(spark), V(spark), fpu_mul(vec3_length(V(bounce)), 0.5f));
        vec3_add(V(spark), V(bounce), V(spark));
        const float ft = lwc1(0x0015ed6c);
        vec3_set_length(V(spark), V(spark), rand_range_f(fpu_mul(ft, 5.0f), fpu_mul(ft, 9.0f)));
        const int32_t lo = frame_scale_i(0xa);
        const int32_t life = rand_range(lo, frame_scale_i(0xf));
        spawn_particle_1b(pos, spark, 0x7f2f4f6f, static_cast<uint32_t>(life), 90000.0f);
    }
}

// the timer run out: the glow faded by 0.74, the shot gone once it is under 0.02
void fade(uint32_t m, uint32_t pv) {
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) == 0) return;
    const float glow = fpu_mul(lwc1(pv + 0x10), u2f(0x3f3d70a4u));  // (0.74)
    swc1(pv + 0x10, glow);
    if (glow < 0.02f) moby_retire(m);
}

// the record's nine words as the collision reads them
void record_words(uint32_t rec, uint32_t* out) {
    for (uint32_t i = 0; i < 9; ++i) out[i] = lwu(rec + 4 * i);
}
}  // namespace

namespace {
// a shot of class `cls` spawned and set out (the launchers' common part): its owner, its turn along `vel`, its velocity
// (+0) at `speed` (or the velocity's own: the speed returned), at `pos`; the glow full
uint32_t shot_spawn(uint32_t cls, uint32_t owner, uint32_t vel, uint32_t pos, float range, float& speed, bool solid) {
    const uint32_t r = moby_spawn(cls);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    if (!solid) sw(r + 0x94, 0);
    sw(pv + 0x1c, owner);
    sb(r + 0x30, 0xff);
    sh(r + 0x32, static_cast<uint32_t>(float_to_int(range)));
    sw(r + 0x40, 0);
    sb(r + 0x31, 1);
    swc1(r + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    swc1(r + 0x48, atan2_f(lwc1(vel), lwc1(vel + 4)));
    swc1(pv + 0x10, 1.0f);
    if (0.0f < speed) {
        vec3_set_length(V(pv), V(vel), speed);
    } else {
        lq_sq(pv, vel);
        speed = vec3_length(V(vel));
    }
    lq_sq(r + 0x10, pos);
    return r;
}
}  // namespace

// orig: L11:0x00308848 L13:0x003025f8 L17:0x002e2128
uint32_t class1009_launch(uint32_t owner, uint32_t vel, uint32_t pos, float range, float speed) {
    const uint32_t r = shot_spawn(0x3f1, owner, vel, pos, range, speed, true);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    const int32_t frames = float_to_int(fpu_div(range, speed));
    sw(pv + 0x18, static_cast<uint32_t>(frames));
    sw(pv + 0x14, static_cast<uint32_t>(frames));
    sh(r + 0x34, 0x200);
    swc1(r + 0x2c, fpu_mul(lwc1(lwu(r + 0x24) + 0x24), fpu_div(lwc1(owner + 0x2c), lwc1(lwu(owner + 0x24) + 0x24))));
    sb(r + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x10), 24.0f))));
    moby_set_color_hi(r, 0xff, 0xff, 0xff);
    moby_update_transform(r);
    return r;
}

// orig: L11:0x00308f48 L13:0x00302cf8 L17:0x002e2828
uint32_t class1017_launch(uint32_t owner, uint32_t vel, uint32_t pos, float range, float speed, float reach) {
    const uint32_t r = shot_spawn(0x3f9, owner, vel, pos, range, speed, false);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    const int32_t frames = float_to_int(fpu_div(range, speed));
    sw(pv + 0x14, static_cast<uint32_t>(frames));
    swc1(pv + 0x20, reach);
    sw(pv + 0x18, static_cast<uint32_t>(frames - 0xd));
    sh(r + 0x34, 0x200);
    moby_update_transform(r);
    return r;
}

// orig: L11:0x00308670 L13:0x00302420 L17:0x002e1f50
uint32_t class1009_target(uint32_t pos, uint32_t rot, float yaw_cone, float pitch_cone, float range) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t aim = sp;
    uint32_t best = 0;
    float best_score = 10.0f;
    for (uint32_t t = lwu(OV(0x0015ffe4)); t != 0; t = lwu(t + 0x28)) {
        if ((lhu(t + 0x34) & 0x1000u) == 0) continue;
        const uint32_t cls = lwu(t + 0x24);
        if (cls == 0 || lh(cls + 0x46) != 5) continue;
        const float across = vec2_distance(V(pos), V(t + 0x10));
        if (!(across < range)) continue;
        lq_sq(aim, t + 0x10);
        swc1(aim + 8, fpu_add(lwc1(aim + 8), 0.4f));
        const float yaw_off = angle_dist(lwc1(rot + 8), atan2_f(fpu_sub(lwc1(aim), lwc1(pos)), fpu_sub(lwc1(aim + 4), lwc1(pos + 4))));
        const float yaw2 = fpu_mul(yaw_off, yaw_off);
        if (!(yaw2 < fpu_mul(yaw_cone, yaw_cone))) continue;
        const float pitch_off = angle_dist(lwc1(rot + 4), atan2_f(across, fpu_sub(lwc1(aim + 8), lwc1(pos + 8))));
        const float pitch2 = fpu_mul(pitch_off, pitch_off);
        if (!(pitch2 < fpu_mul(pitch_cone, pitch_cone))) continue;
        const float score = fpu_add(fpu_mul(fpu_mul(yaw2, pitch2), 30.0f), sqrt_f(vec3_distance(V(pos), V(aim))));
        if (score < best_score) {
            best_score = score;
            best = t;
        }
    }
    ee_stack_free(0xa0);
    return best;
}

// Each frame: it grows to its scale, shows its glow; every sixth frame (staggered by its slot) it drops its target, and
// one that is gone or more than 3 from where it was last seen is searched again (15 degrees each way, 60 across); held, it
// turns towards it at most 50 degrees a second each way (let go once it is more than 15 degrees off), its speed kept; it
// moves, and is gone outside 2 .. 1021 on an axis; where its line of flight meets something it throws five sparks off the
// surface and goes; else its glow fades each time its timer runs out.
// orig: L11:0x003089d0 L13:0x00302780 L17:0x002e22b0
void class1009_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t from = sp, seen = sp + 0x10, rec = sp + 0x20, spark = sp + 0x50, bounce = sp + 0x60;
    {
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(sc, fpu_mul(fpu_sub(lwc1(lwu(m + 0x24) + 0x24), sc), 0.1f)));
    }
    lq_sq(from, pos);
    {
        const int32_t slot = static_cast<int32_t>(m - lwu(OV(0x0015ffd8))) >> 8;
        if (ee_rem(lw(OV(0x0015f5cc)), 6) == ee_rem(slot, 6)) sw(pv + 0x2c, 0);
    }
    sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x10), 24.0f))));
    swc1(seen, lwc1(pv + 0x20));
    swc1(seen + 4, lwc1(pv + 0x24));
    swc1(seen + 8, lwc1(pv + 0x28));
    sw(seen + 0xc, 0);
    {
        const uint32_t t = lwu(pv + 0x2c);
        bool keep = t != 0;
        if (keep) {
            const uint32_t st = lbu(t + 0x20);
            keep = st != 0xfe && st != 0xfd && !(3.0f < vec3_distance(V(t + 0x10), V(seen)));
        }
        if (!keep) sw(pv + 0x2c, class1009_target(pos, m + 0x40, CONE, CONE, 60.0f));
    }
    if (const uint32_t t = lwu(pv + 0x2c); t != 0) {
        swc1(pv + 0x20, lwc1(t + 0x10));
        swc1(pv + 0x24, lwc1(t + 0x14));
        swc1(pv + 0x28, lwc1(t + 0x18));
        const float speed = vec3_length(V(pv));
        float yaw = angle_sub(atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14))), lwc1(m + 0x48));
        const float yaw_off = abs_f(yaw);
        const float across = vec2_distance(V(pos), V(t + 0x10));
        float pitch = angle_sub(-atan2_f(across, fpu_sub(lwc1(t + 0x18), lwc1(m + 0x18))), lwc1(m + 0x44));
        const float pitch_off = abs_f(pitch);
        if (!(yaw_off < CONE) || !(pitch_off < CONE)) {
            sw(pv + 0x2c, 0);
        } else {
            const float turn = u2f(0x3f5f66f3u);  // (50 degrees a second)
            const float most_yaw = fpu_mul(lwc1(0x0015ed6c), turn);
            if (most_yaw < yaw_off) yaw = fpu_mul(fpu_div(yaw, yaw_off), most_yaw);
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), yaw));
            const float most_pitch = fpu_mul(lwc1(0x0015ed6c), turn);
            if (most_pitch < pitch_off) pitch = fpu_mul(fpu_div(pitch, pitch_off), most_pitch);
            const float new_pitch = angle_add(lwc1(m + 0x44), pitch);
            swc1(m + 0x44, new_pitch);
            vec_from_spherical_v(V(pv), speed, lwc1(m + 0x48), -new_pitch);
            debug_stub_1f6c28();  // (a debug line from the shot to its target)
        }
    }
    vec3_add(V(pos), V(pos), V(pv));
    for (uint32_t axis = 0; axis < 0xc; axis += 4) {
        const float c = lwc1(pos + axis);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            ee_stack_free(0x130);
            return;
        }
    }
    hit_rec_init(rec, m, 0x10000, 1.0f);
    sh(rec + 0x1a, lhu(m + 0xa6));
    uint32_t record[9];
    record_words(rec, record);
    if (segment_collide_v(V(from), V(pos), 0, lwu(pv + 0x1c), record) == 0) {
        fade(m, pv);
    } else {
        sparks(pos, pv, spark, bounce);
        moby_retire(m);
    }
    ee_stack_free(0x130);
}

// Each frame: a spark of the trail's colours (a random mix) a little about the shot, moving at the trail's part of its
// velocity; the shot moves; where its line of flight (the record of reach +0x20 along its velocity) meets something it
// throws five sparks and goes; else its glow fades each time its timer runs out.
// orig: L11:0x00309098 L13:0x00302e48 L17:0x002e2978
void class1017_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t from = sp, at = sp + 0x10, vel = sp + 0x20, rec = sp + 0x30, spark = sp + 0x60, bounce = sp + 0x70;
    const uint32_t tr = trail();
    lq_sq(from, pos);
    const uint32_t colour = rgba_lerp(lwu(tr), lwu(tr + 4), rand_range_f(0.0f, 1.0f));
    rand_sphere_point(at, 0.0f, 0.1f);
    vec3_add(V(at), V(at), V(pos));
    vec3_scale(V(vel), V(pv), lwc1(tr + 0x10));
    const int32_t frames = frame_scale_i(lw(tr + 8));
    spawn_particle_1b(at, vel, colour, static_cast<uint32_t>(frames), int_to_float(lw(tr + 0xc)));
    vec3_add(V(pos), V(pos), V(pv));
    sub_00259888(lwc1(pv + 0x20), rec, static_cast<int32_t>(m), 0x10000, pv);
    uint32_t record[9];
    record_words(rec, record);
    if (segment_collide_v(V(from), V(pos), 0, lwu(pv + 0x1c), record) == 0) {
        fade(m, pv);
    } else {
        sparks(pos, pv, spark, bounce);
        moby_retire(m);
    }
    ee_stack_free(0x130);
}

}  // namespace rac1
