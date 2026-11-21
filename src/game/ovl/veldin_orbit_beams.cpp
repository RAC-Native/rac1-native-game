// Drek's fleet's (level 17) laser beams, ported by hand from the level's own code: class 99, a beam from the moby's
// emitter along its facing (13 units, cut where it meets the world), sliding to and fro between its start and a target
// (a box's centre or another moby's start: pvars +0x44 / +0x68) or standing still, shown while the hero is inside its
// box (pvars +0x40); where it is on screen it hurts what crosses it, throws sparks (particle 0x4f, L17:0x26fb60) along
// it and at its end, and queues its draw (L17:0x2a95b8: a camera-facing textured quad along the beam, quad_draw). The
// original's order of calls and float operations kept. Level 17 only: `orig: L17:`, level data through lvd::* and the collision facade, the level's own
// data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
#include <game/ovl/veldin_orbit_own.h>

#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_orbit_mobys.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

float dt() { return lwc1(0x0015ed6cu); }

// (x, 0, 0, 0) at `v` (the original's `sq zero` then `swc1`)
void quad_x(uint32_t v, float x) {
    sw(v + 0x4, 0);
    sw(v + 0x8, 0);
    sw(v + 0xc, 0);
    swc1(v, x);
}

constexpr uint32_t HERO_POS = 0x0013f3d0u;
}  // namespace

// Spawns a spark (particle 0x4f) at `pos` moving by `vel`: tied to moby `m` (its offset from the moby kept, a short
// life) or free (m = 0: a life four times as long).
// orig: L17:0x0026fb60
uint32_t vo_26fb60_l17(uint32_t m, uint32_t pos, uint32_t vel) {
    const uint32_t life = static_cast<uint32_t>(frame_scale_i(rand_mod(5) + 2)) & 0xffu;
    const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
    const uint32_t p = alloc_particle(0x4f);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    swc1(p + 0x30, lwc1(vel + 0x0));
    swc1(p + 0x34, lwc1(vel + 0x4));
    swc1(p + 0x38, lwc1(vel + 0x8));
    if (m != 0) {
        vec3_sub(V(p + 0x20), V(p + 0x10), V(m + 0x10));
        sh(p + 0xa, life);
    } else {
        sh(p + 0xa, life << 2);
    }
    sw(p + 0x3c, m);
    sw(p + 0x4, 0x604040ffu);
    sb(p + 0x9, static_cast<uint32_t>(float_to_int(1.0f) + 0x20));
    sb(p + 0x3, 0x48);
    swc1(p + 0xc, 42000.0f);
    sb(p + 0x8, frame);
    sb(p + 0x1, 0);
    sb(p + 0x2, lbu(lwu(L17_ADDR(0x001b2a54))));
    return p;
}

// Class 99, the laser beam. Pvars: +0x0 the start, +0x10 / +0x20 the beam's two ends (+0x60: the moby whose position
// follows the far end), +0x30 the beam (end minus emitter), +0x40 the box the hero must be in to see it (-1: always),
// +0x44 the target box / +0x68 the target moby (-1: none), +0x48 the speed, +0x4c the draw's width, +0x50 / +0x54 the
// place between the ends and its step, +0x58 / +0x5c the animation's first time and a 2 pi's length of it. States:
// 0 set up, 1 standing still, 2 sliding, 3 off.
// orig: L17:0x002a8da0
void vo_class99_update_l17(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 3) return;
    const Frame f(352);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, mat = m + 0xc0;
    const uint32_t axis = sp + 0x10, tip = sp + 0x20;
    bool shown = false;

    // the beam's far end along the moby's facing (13 units), cut at the world: the beam (pv + 0x30) from the emitter
    auto cast_beam = [&] {
        quad_x(axis, 13.0f);
        mat3_transform(V(axis), V(axis), M(mat));
        vec3_add(V(tip), V(pos), V(axis));
        lq_sq(sp, tip);
        if (segment_collide(pos, sp, 2, 0, 0) != 0) V(sp) = coll_hit_point();
        vec3_sub(V(tip), V(sp), V(pos));
        lq_sq(pv + 0x30, tip);
    };

    if (st == 0) {
        swc1(pv + 0x58, class_anim_first_time(lbu(m + 0x22), 0));
        const float last = class_anim_last_time(lbu(m + 0x22), 0);
        swc1(pv + 0x5c, fpu_div(fpu_sub(last, lwc1(pv + 0x58)), u2f(0x40c90fdbu)));
        const int32_t box = lw(pv + 0x44);
        const int32_t mate = lw(pv + 0x68);
        if (box != -1 || mate != -1) {
            lq_sq(pv, pos);
            const uint32_t target = box != -1 ? lvd::box(lvd::BOXES, box) + 0x30
                                              : lwu(L17_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate) << 8) + 0x10;
            const float dist = vec3_distance(V(pv), V(target));
            swc1(pv + 0x54, fpu_div(fpu_mul(lwc1(pv + 0x48), dt()), dist));
            sb(m + 0x20, 2);
        } else {
            sb(m + 0x20, 1);
        }
        sw(pv + 0x60, 0);
        swc1(pv + 0x4c, 0.1f);
        cast_beam();
    } else if (st == 1) {
        const int32_t box = lw(pv + 0x40);
        if (box == -1 || point_in_box(HERO_POS, box) != 0) shown = true;
    } else if (st == 2) {
        const float step = lwc1(pv + 0x54);
        const float t = fpu_add(lwc1(pv + 0x50), step);
        swc1(pv + 0x50, t);
        if (1.0f < t || t < 0.0f) {  // at an end: turn back
            swc1(pv + 0x50, 1.0f < t ? 1.0f : 0.0f);
            swc1(pv + 0x54, -step);
        }
        const int32_t mate = lw(pv + 0x68);
        if (mate != -1) {
            const uint32_t other = lwu(L17_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate) << 8);
            vec3_lerp(V(pos), V(pv), V(lwu(other + 0x78)), lwc1(pv + 0x50));
        } else {
            vec3_lerp(V(pos), V(pv), V(lvd::box(lvd::BOXES, lw(pv + 0x44)) + 0x30), lwc1(pv + 0x50));
        }
        const int32_t box = lw(pv + 0x40);
        if (box == -1 || point_in_box(HERO_POS, box) != 0) {
            cast_beam();
            shown = true;
        }
    }

    // the common part: the beam's two ends (12 units along the facing, plus the beam), the follower's place
    quad_x(axis, 12.0f);
    mat3_transform(V(axis), V(axis), M(mat));
    vec3_add(V(tip), V(pos), V(pv + 0x30));
    lq_sq(sp, tip);
    const uint32_t follower = lwu(pv + 0x60);
    if (follower != 0) {
        vec3_set_length(V(pv + 0x20), V(axis), -fpu_sub(lwc1(L17_ADDR(0x00161498)), 0.2f));
        vec3_set_length(V(lwu(pv + 0x60) + 0x10), V(axis), -lwc1(L17_ADDR(0x00161498)));
        vec3_add(V(tip), V(pv + 0x20), V(sp));
        const uint32_t other = lwu(pv + 0x60);
        lq_sq(pv + 0x20, tip);
        vec3_add(V(tip), V(other + 0x10), V(sp));
        lq_sq(other + 0x10, tip);
    } else {
        lq_sq(pv + 0x20, sp);
    }
    if (!shown) return;

    // shown: the draw's width pulses with the animation; on screen, the hurt and the sparks
    const float phase = fpu_div(fpu_sub(moby_anim_time(m), lwc1(pv + 0x58)), lwc1(pv + 0x5c));
    const float c = cos_vu(angle_add(u2f(0x40490fdbu), phase));
    swc1(pv + 0x4c, fpu_add(fpu_mul(c, u2f(0x3ca3d70au)), u2f(0x3db851ecu)));
    vec3_set_length(V(pv + 0x10), V(axis), 0.2f);
    vec3_sub(V(sp + 0x50), V(pos), V(pv + 0x10));
    lq_sq(pv + 0x10, sp + 0x50);
    float len = vec3_length(V(pv + 0x30));
    vec3_lerp(V(tip), V(pv + 0x10), V(pv + 0x20), 0.5f);
    swc1(tip + 0xc, fpu_mul(len, 0.5f));
    if (point_on_screen(tip, 32.0f) != -1) {
        // the hero's place across the beam: the hit record pushes along it
        const uint32_t across = sp + 0x30;
        vec3_sub(V(across), V(HERO_POS), V(pos));
        const float along = vec3_dot(V(mat), V(across));
        vec3_set_length(V(sp + 0x40), V(mat), along);
        vec3_sub(V(across), V(across), V(sp + 0x40));
        sub_00259888(1.0f, sp + 0x60, static_cast<int32_t>(m), 0x10001, across);
        segment_collide(pos, pv + 0x20, 9, m, sp + 0x60);
        len = vec3_length(V(pv + 0x30));
        // seven sparks along the beam from the emitter's end, eight back from the far end
        lq_sq(sp + 0x50, pv + 0x10);
        for (int i = 0; i < 7; ++i) {
            vec3_scale(V(sp), V(pv + 0x30), rand_range_f(0.0f, u2f(0x3f666666u)));
            vec3_add_inplace(V(sp), V(sp + 0x50));
            const float hi = fpu_div(len, 20.0f);
            const float lo = fpu_mul(dt(), 10.0f);
            vec3_set_length(V(axis), V(pv + 0x30), rand_range_f(lo, hi));
            vo_26fb60_l17(m, sp, axis);
        }
        len = -len;
        lq_sq(sp + 0x50, pv + 0x20);
        for (int i = 0; i < 8; ++i) {
            vec3_scale(V(sp), V(pv + 0x30), rand_range_f(0.0f, u2f(0x3f666666u)));
            vec3_sub(V(sp + 0x60), V(sp + 0x50), V(sp));
            const float hi = fpu_div(len, 20.0f);
            const float lo = fpu_mul(dt(), 10.0f);
            lq_sq(sp, sp + 0x60);
            vec3_set_length(V(axis), V(pv + 0x30), rand_range_f(lo, hi));
            vo_26fb60_l17(m, sp, axis);
        }
        // one free spark at the far end, thrown back along the beam with a little scatter
        const float back = rand_range_f(fpu_mul(dt(), 0.5f), dt());
        vec3_set_length(V(axis), V(pv + 0x30), -back);
        const float lo = u2f(0xbc23d70au), hi = u2f(0x3c23d70au);
        swc1(axis + 0x0, fpu_add(lwc1(axis + 0x0), rand_range_f(lo, hi)));
        swc1(axis + 0x4, fpu_add(lwc1(axis + 0x4), rand_range_f(lo, hi)));
        swc1(axis + 0x8, fpu_add(lwc1(axis + 0x8), rand_range_f(lo, hi)));
        vo_26fb60_l17(0, pv + 0x20, axis);
    }
    sub_001f93e0(L17_ADDR(0x002a95b8), m);
}

// Class 99's draw callback: the beam between its two ends (pvars +0x10 / +0x20) as one quad facing the camera
// (L17:0x1676c0), as wide as pvars +0x4c on either side, the effect texture 0xe, the colour 0x80243278 and the level's
// four (s, t) pairs L17:0x1e6320. Every local lives in the original's frame at its offsets (+0 the quad's descriptor for
// quad_draw: its points +0 .. +0x30, colours +0x40, (s, t) +0x50, GS words +0x70; +0x90 / +0xa0 the two sides, +0xb0 the
// beam, +0xc0 a corner). VU0: no VU0 instruction of its own; its callees' ports (vec3_*, quad_draw) keep no VU0 state.
// orig: L17:0x002a95b8
void vo_cb_2a95b8_l17(uint32_t m) {
    const Frame f(0x150);
    const uint32_t q = f.sp, side_a = f.sp + 0x90, side_b = f.sp + 0xa0, beam = f.sp + 0xb0, corner = f.sp + 0xc0;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t a = pv + 0x10, b = pv + 0x20, cam = L17_ADDR(0x001676c0);
    vec3_sub(V(beam), V(a), V(b));
    vec3_sub(V(side_a), V(a), V(cam));
    vec3_sub(V(side_b), V(b), V(cam));
    vec3_cross_ba(V(side_a), V(side_a), V(beam));
    vec3_set_length(V(side_a), V(side_a), lwc1(pv + 0x4c));
    vec3_cross_ba(V(side_b), V(side_b), V(beam));
    vec3_set_length(V(side_b), V(side_b), lwc1(pv + 0x4c));
    sd(q + 0x78, effect_tex0(0xe));
    sd(q + 0x80, (0xff90ull << 32) | 0x260ull);  // TEX1
    sd(q + 0x88, (0x80ull << 32) | 0x48ull);     // ALPHA
    sd(q + 0x70, 0);                             // CLAMP
    for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, 0x80243278u);
    for (uint32_t k = 0; k < 4 * 8; k += 8) {
        swc1(q + 0x50 + k, lwc1(L17_ADDR(0x001e6320) + k));
        swc1(q + 0x54 + k, lwc1(L17_ADDR(0x001e6320) + k + 4));
    }
    vec3_sub(V(corner), V(a), V(side_a));
    lq_sq(q, corner);
    vec3_add(V(corner), V(a), V(side_a));
    lq_sq(q + 0x10, corner);
    vec3_sub(V(corner), V(b), V(side_b));
    lq_sq(q + 0x20, corner);
    vec3_add(V(corner), V(b), V(side_b));
    lq_sq(q + 0x30, corner);
    quad_draw(q, 0);
}

}  // namespace rac1
