// The homing missiles of Batalia (level 8) and Hoven (12), class 0x1ca (458): the launch (a puff cloud and sparks at the
// muzzle, the missile's heading from its velocity) and the missile's flight (a smoke trail, a turn, homing on a target of
// class 0x1b3 with lead, an explosion where it hits). The two levels' copies are the same code, relinked: the class's tuning
// words (level data, a per-level base below) are the one level word; the collision's point 0x173e60 and the frame counter
// 0x15f5cc go through OV(), the base ELF's are bare (the frame time 0x15ed6c, the game's level number 0x15ed84). Header
// shared_homing_missile.h.
#include <game/ovl/shared_homing_missile.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// The class's tuning words: the life +0 (frames), the trail's puffs a frame +4 and the launch's +8 (floats), the turn rate +0xc
// (degrees a second), the colours of the trail +0x10, of the launch's puffs +0x18 and sparks +0x20, the scatter of the trail
// +0x28 and of the launch +0x2c, the puffs' frame counts +0x30 / +0x34 / +0x38, the w words of the launch's +0x3c / +0x40 and
// of the trail's +0x44 / +0x48 second vectors.
uint32_t missile_tuning() { return on_level(HOVEN) ? L12_ADDR(0x00161a58) : L8_ADDR(0x00161cd8); }

constexpr uint32_t kMissileClass = 0x1ca;
constexpr int32_t kTargetClass = 0x1b3;

float dt() { return lwc1(0x0015ed6c); }

// a frame count +20 % (`more` 1.2) or +50 % (1.5) at random, in the frame rate's frames
int32_t frames(uint32_t word, float more) {
    const float f = int_to_float(lw(word));
    return float_to_int(frame_scale_f(rand_range_f(f, fpu_mul(f, more))));
}

// one puff (particle 2) at `at` with the velocities at `vel0` / `vel1` (EE vectors), the colours at `colours`
void puff(uint32_t tune, uint32_t at, uint32_t vel0, uint32_t vel1, uint32_t colours) {
    const int32_t life = frames(tune + 0x30, 1.2f);
    const int32_t grow = frames(tune + 0x34, 1.2f);
    const int32_t fade = frames(tune + 0x38, 1.5f);
    spawn_particle_02(at, vel0, vel1, lwu(colours), lwu(colours + 4), static_cast<uint32_t>(life), static_cast<uint32_t>(grow),
                      static_cast<uint32_t>(fade), -1);
}

// `out` (an EE vector) <- a random sideways push of length rand(lo, hi) * dt, in the plane of the rows `row0` / `row1`
void scatter(uint32_t out, uint32_t tmp, uint32_t row0, uint32_t row1, float lo, float hi) {
    const float angle = rand_angle();
    const float len = fpu_mul(rand_range_f(lo, hi), dt());
    vec3_set_length(V(out), V(row0), fpu_mul(len, cos_vu(angle)));
    vec3_set_length(V(tmp), V(row1), fpu_mul(len, sin_vu(angle)));
    vec3_add(V(out), V(out), V(tmp));
}
}  // namespace

// The launch: a missile of class 0x1ca from `src` at `pos` with the velocity `vel` (EE vectors), homing on `target` (or 0);
// returns it (0 when none could be spawned).
// orig: L8:0x002e7eb0 L12:0x002ef2e8
uint32_t homing_missile_launch(uint32_t src, uint32_t pos, uint32_t vel, uint32_t target) {
    const uint32_t tune = missile_tuning();
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t at = sp + 0x10, speed = sp + 0x20, frame = sp + 0x30, puff_vel0 = sp + 0x70, puff_vel1 = sp + 0x80;
    V(at) = V(pos);
    V(speed) = V(vel);
    const uint32_t m = moby_spawn(kMissileClass);
    if (m != 0) {
        const uint32_t pv = lwu(m + 0x78);
        sw(pv + 0x14, src);
        sw(pv + 0x10, target);
        moby_set_color_hi(m, 0xa0, 0xa0, 0xa0);
        sw(m + 0x40, 0);
        sh(m + 0x32, 0xff);
        sb(m + 0x30, 0xff);
        sb(m + 0x31, 1);
        swc1(m + 0x44, -atan2_f(vec2_length(V(speed)), lwc1(speed + 8)));
        swc1(m + 0x48, atan2_f(lwc1(speed), lwc1(speed + 4)));
        V(m + 0x10) = V(at);
        V(pv) = V(speed);
        sw(pv + 0x1c, (lwu(OV(0x0015f5cc)) & 1u) != 0 ? 1u : 0xffffffffu);  // (the way it spins)
        euler_to_mat4_vu(M(frame), V(m + 0x40));
        for (int32_t i = 0; int_to_float(i) < lwc1(tune + 8); ++i) {
            const float spread = lwc1(tune + 0x2c);
            scatter(puff_vel0, puff_vel1, frame + 0x20, frame + 0x10, spread, fpu_add(spread, spread));
            V(puff_vel1) = V(puff_vel0);
            swc1(puff_vel0 + 0xc, lwc1(tune + 0x3c));
            swc1(puff_vel1 + 0xc, lwc1(tune + 0x40));
            const int32_t lo = frame_scale_i(10);
            const int32_t hi = frame_scale_i(20);
            const int32_t sparks = rand_range(lo, hi);
            spawn_particle_15_v(V(m + 0x10), V(puff_vel0), lwu(tune + 0x20), lwu(tune + 0x24), static_cast<uint32_t>(sparks), 1, 20000.0f);
            puff(tune, m + 0x10, puff_vel0, puff_vel1, tune + 0x18);
        }
        if (lw(0x0015ed84) == 12) sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(0x78)));  // (Hoven: two seconds)
        else sw(pv + 0x18, static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(tune)))));
        moby_update_transform(m);
    }
    ee_stack_free(0x160);
    return m;
}

// The missile (pvars: its velocity +0, its target +0x10, its source +0x14, its life +0x18, its spin +0x1c). Out of the
// level's box it is gone; it leaves a trail of puffs back along this frame's way, turns, homes on a live target of class
// 0x1b3 (aiming where the target will be), and explodes where it hits something (or just goes when its life is over).
// orig: L8:0x002e8210 L12:0x002ef648
void homing_missile_update(uint32_t m) {
    const uint32_t tune = missile_tuning();
    const uint32_t pv = lwu(m + 0x78);
    // (the frame's slots as the original lays them out: the collision reads its record from +0x30 .. +0x50)
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t from = sp + 0x20, rec = sp + 0x30, step = sp + 0x40, push0 = sp + 0x50, push1 = sp + 0x60;
    V(from) = V(m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
    const bool inside = !(x < 10.0f) && !(y < 10.0f) && !(z < 10.0f) && !(500.0f < x) && !(500.0f < y) && !(500.0f < z);
    if (!inside) {
        moby_retire(m);
        ee_stack_free(0x140);
        return;
    }
    // the trail: puffs back along the way it came this frame
    vec3_scale(V(step), V(pv), fpu_div(-1.0f, lwc1(tune + 4)));
    V(rec) = V(m + 0x10);
    for (int32_t i = 0; int_to_float(i) < lwc1(tune + 4); ++i) {
        vec3_add(V(rec), V(rec), V(step));
        scatter(push0, push1, m + 0xe0, m + 0xd0, 0.0f, lwc1(tune + 0x28));
        swc1(push1 + 0xc, lwc1(tune + 0x48));
        swc1(push0 + 0xc, lwc1(tune + 0x44));
        puff(tune, rec, push0, push1, tune + 0x10);
    }
    // the spin
    const float turn = fpu_mul(fpu_mul(fpu_mul(lwc1(tune + 0xc), u2f(0x3c8efa35u)), dt()), int_to_float(lw(pv + 0x1c)));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), turn));
    // homing on the target
    const uint32_t target = lwu(pv + 0x10);
    if (target != 0) {
        const uint32_t state = lbu(target + 0x20);
        if (state == 0xfe || state == 0xfd) {
            sw(pv + 0x10, 0);
        } else if (lh(target + 0xa6) != kTargetClass) {
            sw(pv + 0x10, 0);
        } else {
            const uint32_t tpv = lwu(target + 0x78);
            const float speed = vec3_length(V(pv));
            const float time = fpu_div(vec3_distance(V(m + 0x10), V(lwu(pv + 0x10) + 0x10)), speed);
            vec3_scale(V(rec), V(tpv + 0x20), time);  // (where the target will be)
            vec3_add(V(rec), V(rec), V(lwu(pv + 0x10) + 0x10));
            vec3_sub(V(step), V(rec), V(m + 0x10));
            vec3_set_length(V(step), V(step), speed);
            vec3_sub(V(push0), V(step), V(pv));
            vec3_clamp_length(push0, fpu_mul(dt(), 3.7f));
            vec3_add(V(pv), V(pv), V(push0));
            vec3_set_length(V(pv), V(pv), speed);
        }
    }
    // the collision's record (what a hit leaves in the hit table): the velocity, the missile, its kind +0x14 (0x50001), +0x18 what
    // the frame's slot holds there, its damage +0x1c (2.0), +0x20 1
    sw(step + 4, 0x50001);
    sw(step, m);
    swc1(step + 0xc, 2.0f);
    sw(push0, 1);
    V(rec) = V(pv);
    int32_t life = lw(pv + 0x18);
    const int32_t ticked = timer_tick_i32(life);
    sw(pv + 0x18, static_cast<uint32_t>(life));
    if (ticked != 0) {  // (its life is over)
        moby_retire(m);
        ee_stack_free(0x140);
        return;
    }
    uint32_t record[9];
    for (uint32_t i = 0; i < 9; ++i) record[i] = lwu(rec + 4 * i);
    if (segment_collide_v(V(from), V(m + 0x10), 0, m, record) == 0) {
        ee_stack_free(0x140);
        return;
    }
    V(m + 0x10) = V(OV(0x00173e60));  // (the hit point)
    const float hx = lwc1(m + 0x10), hy = lwc1(m + 0x14), hz = lwc1(m + 0x18);
    if (15.0f < hx && 15.0f < hy && 15.0f < hz) {
        float size = 24.0f;  // (at most 24, and no deeper than the level's floor allows)
        if (fpu_sub(hx, 2.0f) < size) size = fpu_sub(hx, 2.0f);
        if (fpu_sub(hy, 2.0f) < size) size = fpu_sub(hy, 2.0f);
        if (fpu_sub(hz, 2.0f) < size) size = fpu_sub(hz, 2.0f);
        explosion_effect(m, pv, m + 0x10, 0, 10, 20, -1, 1, 1, -1, 0, 3.0f, 3.0f, 5.0f, 3.0f, 9.0f, 1.0f, size);
    }
    moby_retire(m);
    ee_stack_free(0x140);
}

}  // namespace rac1
