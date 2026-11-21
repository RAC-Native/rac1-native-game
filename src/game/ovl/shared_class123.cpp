// Class 123's update: one port of the copies Quartu and Drek's fleet carry (the same code relinked). Each level keeps its
// own sparks' tuning (per level below: +0 their count, +4 their size, +8 their life) and its own copy of the draw callback the
// update queues (shared_class123_draw.cpp is its port); the rest is the engine's (OV: the collision's hit point 0x173e60) or
// the base ELF's (bare: the frame time 0x15ed6c, libc's rand). Header shared_class123.h.
//
// The pvars: +0 its post, +0x10 the beam's far end, +0x20 its motion (0 circles, 1 / 2 slide, 3 / 4 swing; 4 and 2 turned a
// quarter), +0x24 the radius, +0x28 the phase, +0x2c the plane's heading, +0x30 the period (seconds).
#include <game/ovl/shared_class123.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;     // (0x3c8efa35)
constexpr float HALF_PI = 1.5707963705062866f;  // (0x3fc90fdb)
constexpr float PI = 3.1415927410125732f;       // (0x40490fdb)
constexpr float TWO_PI = 6.2831854820251465f;   // (0x40c90fdb)

uint32_t sparks() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001614f0) : L15_ADDR(0x00161558); }
// the level's copy of the draw callback (the queued function's address)
uint32_t draw_fn() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002acf00) : L15_ADDR(0x002a7628); }

float dt() { return lwc1(0x0015ed6c); }

// the phase's step this frame: a turn in the period (+0x30) at the radius's speed
float phase_step(uint32_t pv) {
    const float per_turn = fpu_div(fpu_mul(lwc1(pv + 0x24), TWO_PI), lwc1(pv + 0x30));
    return fpu_mul(fpu_mul(fpu_div(360.0f, per_turn), DEG), dt());
}

// a swing's angle at the phase (a quarter turn each way of cos, offset a quarter for the motion 4)
float swing_at(uint32_t pv, float phase) {
    const float a = angle_wrap(fpu_mul(cos_vu(phase), HALF_PI));
    return angle_add(a, lw(pv + 0x20) == 4 ? HALF_PI : 0.0f);
}

// the moby's place on its circle or swing: the point (0, 0, r) turned by `angle` about the plane's horizontal axis, from the
// post; its frame turned to face along it (pitch `pitch`, heading the plane's plus a quarter)
void place_on_arc(uint32_t m, uint32_t pv, uint32_t f, float r, float angle, float pitch) {
    const uint32_t arm = f + 0x10, out = f + 0x20, axis = f + 0x30, angles = f + 0x40, mtx = f + 0x50, at = f + 0x90;
    V(axis) = Vec4{};
    V(angles) = Vec4{};
    swc1(axis + 8, r);
    lq_sq(out, axis);
    swc1(axis, cos_vu(lwc1(pv + 0x2c)));
    swc1(axis + 4, sin_vu(lwc1(pv + 0x2c)));
    sw(axis + 8, 0);
    vec_rotate_axis(V(arm), V(out), V(axis), angle);
    vec3_add(V(at), V(pv), V(arm));
    lq_sq(m + 0x10, at);
    swc1(angles + 8, angle_add(lwc1(pv + 0x2c), HALF_PI));
    swc1(angles + 4, pitch);
    euler_to_mat4_vu(M(mtx), V(angles));
    mat3_copy(M(m + 0xc0), M(mtx));
    mat3_normalize_columns(m + 0xc0);
    sh(m + 0x34, lhu(m + 0x34) | 0x100u);
}
}  // namespace

// State 0 sets it up (its post where it is, the phase in degrees made radians, the animation's speed from the period, the motion's
// state), 1 circles about the post, 2 swings, 3 slides to and fro (the animation's speed following the motion). Then the beam:
// 12 along its frame's z, tested against the mobys (a hit on them: kinds 0x10001) and the world, its far end where it stops;
// sparks along it (towards the far end or back, the level's count, size and life); its draw queued.
// orig: L15:0x002a6e68 L17:0x002ac740
void class123_update(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x170);
    const uint32_t was = f, far = f + 0x10, beam = f + 0x20, rec = f + 0x40, back = f + 0x70;
    const uint32_t pv = lwu(m + 0x78), pos = m + 0x10;
    lq_sq(was, pos);
    switch (lbu(m + 0x20)) {
    case 0: {
        lq_sq(pv, pos);
        swc1(m + 0x58, fpu_div(lwc1(pv + 0x30), 1.5f));
        swc1(pv + 0x28, fpu_mul(lwc1(pv + 0x28), DEG));
        const int32_t motion = lw(pv + 0x20);
        if (motion == 0) {
            sb(m + 0x20, 1);
            swc1(pv + 0x2c, lwc1(m + 0x48));
        } else if (motion < 3) {
            sb(m + 0x20, 3);
        } else {
            sb(m + 0x20, 2);
            swc1(pv + 0x2c, lwc1(m + 0x48));
        }
        break;
    }
    case 1: {  // (circling)
        const float r = lwc1(pv + 0x24);
        swc1(pv + 0x28, angle_add(lwc1(pv + 0x28), phase_step(pv)));
        swc1(m + 0x58, fpu_div(lwc1(pv + 0x30), 1.5f));
        place_on_arc(m, pv, f, r, lwc1(pv + 0x28), angle_add(-lwc1(pv + 0x28), PI));
        break;
    }
    case 2: {  // (swinging)
        const float r = -lwc1(pv + 0x24);
        const float before = swing_at(pv, lwc1(pv + 0x28));
        swc1(pv + 0x28, angle_add(lwc1(pv + 0x28), phase_step(pv)));
        const float now = swing_at(pv, lwc1(pv + 0x28));
        const float moved = angle_sub(now, before);
        swc1(m + 0x58, fpu_div(fpu_mul(lwc1(pv + 0x24), moved), fpu_mul(dt(), 1.5f)));
        place_on_arc(m, pv, f, r, now, angle_add(-now, 0.0f));
        break;
    }
    case 3: {  // (sliding: along its heading, or up for the motion 2)
        const float before = fpu_mul(cos_vu(lwc1(pv + 0x28)), lwc1(pv + 0x24));
        swc1(pv + 0x28, angle_add(lwc1(pv + 0x28), phase_step(pv)));
        const float now = fpu_mul(cos_vu(lwc1(pv + 0x28)), lwc1(pv + 0x24));
        swc1(m + 0x58, fpu_div(fpu_sub(now, before), fpu_mul(dt(), 1.5f)));
        const bool up = lw(pv + 0x20) == 2;
        vec_from_spherical_v(V(far), now, up ? 0.0f : lwc1(m + 0x48), up ? HALF_PI : 0.0f);
        vec3_add(V(beam), V(pv), V(far));
        lq_sq(pos, beam);
        break;
    }
    default:
        break;
    }

    // the beam and the hit it gives
    V(beam) = Vec4{};
    swc1(beam + 8, 12.0f);
    mat3_transform(V(beam), V(beam), M(m + 0xc0));
    vec3_add(V(rec), V(pos), V(beam));
    lq_sq(far, rec);
    sw(rec + 0x14, 0x10001);
    sw(rec + 0x10, m);
    swc1(rec + 0x1c, 1.0f);
    sw(rec + 0x20, 1);
    vec3_set_length(V(rec), V(beam), 1.0f);
    sh(rec + 0x1a, lhu(m + 0xa6));
    swc1(rec + 8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));  // (5627.92)
    sb(rec + 0x19, 1);
    sb(rec + 0x18, 1);
    vec3_sub(V(back), V(pos), V(was));
    lq_sq(f + 0x30, back);
    segment_collide(pos, far, 9, m, rec);
    if (segment_collide(pos, far, 2, 0, 0) != 0) lq_sq(pv + 0x10, OV(0x00173e60));
    else lq_sq(pv + 0x10, far);

    // the sparks along it
    const uint32_t tune = sparks();
    for (int32_t i = 0; i < lw(tune);) {
        const float side = (libc_rand() & 1) != 0 ? 1.0f : -1.0f;
        const float most = lwc1(tune + 8);
        const int32_t life = float_to_int(frame_scale_f(rand_range_f(fpu_mul(most, 0.5f), most)));
        vec3_sub(V(back), V(pv + 0x10), V(pos));
        lq_sq(beam, back);
        vec3_scale(V(beam), V(beam), side);
        const float len = vec3_length(V(beam));
        lq_sq(far, side < 0.0f ? pv + 0x10 : pos);
        ++i;
        vec3_scale(V(beam), V(beam), rand_range_f(0.0f, 0.9f));
        vec3_add_inplace(V(far), V(beam));
        const float speed = rand_range_f(fpu_mul(dt(), 10.0f), fpu_div(len, int_to_float(life << 2)));
        vec3_set_length(V(beam), V(beam), speed);
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
        spawn_particle_3c(far, beam, 0x604040ffu, static_cast<uint32_t>(life) & 0xffu, frame, 0, lwc1(tune + 4));
    }
    sub_001f93e0(draw_fn(), m);
    ee_stack_free(0x170);
}

}  // namespace rac1
