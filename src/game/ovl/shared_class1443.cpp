// Class 1443's update, its group helpers and its draw callback: one port of the copies Kalebo III and Veldin carry (the same
// code relinked; Veldin's class is 1890). Each level keeps its own tuning (per level below: +0 the phase's rate, +4 the colour
// cycle's, +8 .. +0x14 the four rings' spins (degrees a second), +0x18 / +0x1c the two colours, +0x20 the rings' colour, +0x28 ..
// +0x34 the blend's four fields, +0x38 / +0x3c the two effect textures, +0x40 the count of the cycling quads), the two quads'
// (s, t) and corners (per level below) and its own copy of the draw callback the update queues; the rest is the engine's (OV:
// the moby groups 0x1ab840, the mobys 0x15ffd8) or the base ELF's (bare: the hero 0x13f3d0, the frame time 0x15ed6c). Header
// shared_class1443.h.
//
// The pvars: +8 the phase, +0xc the colour cycle (0 .. 1), +0x10 .. +0x1c the rings' angles, +0x20 its group (-1: none), +0x24
// its sound.
#include <game/ovl/shared_class1443.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;    // (0x3c8efa35)
constexpr float TWO_PI = 6.2831854820251465f;  // (0x40c90fdb)
constexpr int32_t CLASS_LEFT_OUT = 0x5a3;

uint32_t tuning() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x00162588) : L16_ADDR(0x00161e00); }
// the quads' data: +0 the cycling quad's (s, t), +0x20 the rings' (s, t), +0x40 the cycling quad's corners, +0x80 the rings'
uint32_t quads() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x001efc10) : L16_ADDR(0x001d9780); }
// the level's copy of the draw callback (the queued function's address)
uint32_t draw_fn() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x002fb018) : L16_ADDR(0x002e58d8); }

float dt() { return lwc1(0x0015ed6c); }

uint32_t group_list(int32_t group) { return lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2)); }
uint32_t member(uint32_t e) { return lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8); }

// a 64-bit word from a level's tuning word (sign-extended, as the original's lw)
uint64_t sx(uint32_t a) { return static_cast<uint64_t>(static_cast<int64_t>(lw(a))); }
}  // namespace

// orig: L16:0x002e5cd0 L18:0x002fb410
int32_t class1443_group_all_hit(int32_t group) {
    uint32_t e = group_list(group);
    if (e == 0) return 0;
    for (;; e += 2) {
        if (lbu(member(e) + 0xbc) == 0) return 0;
        if (lh(e) < 0) return 1;
    }
}

// orig: L16:0x002e5d30 L18:0x002fb470
void class1443_group_sound(int32_t group) {
    uint32_t e = group_list(group);
    uint32_t nearest = 0;
    if (e == 0) return;
    float best = 50000.0f;
    for (;; e += 2) {
        const uint32_t mm = member(e);
        if (lh(mm + 0xa6) != CLASS_LEFT_OUT) {
            const float d = vec3_distance(V(0x0013f3d0), V(mm + 0x10));
            if (d < best) {
                best = d;
                nearest = mm;
            }
        }
        if (lh(e) < 0) break;
    }
    if (nearest != 0) moby_sound_start(0, 0, nearest);
}

// State 0 shows it (state 1). State 1 cycles its colour, hums (its sound kept up), turns its phase and its rings, queues its draw
// while shown, and goes quiet (state 2) once every moby of its group is hit; state 2 waits for them to recover (back to 1, drawn
// again).
// orig: L16:0x002e5708 L18:0x002fae48
void class1443_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    case 1: {
        const uint32_t tune = tuning();
        const float cycle = fpu_add(lwc1(pv + 0xc), fpu_mul(lwc1(tune + 4), dt()));
        swc1(pv + 0xc, cycle);
        if (1.0f < cycle) swc1(pv + 0xc, fpu_sub(cycle, 1.0f));
        if (sub_0028d8c0(m, lw(pv + 0x24)) == 0) sw(pv + 0x24, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        swc1(pv + 8, angle_add(lwc1(pv + 8), fpu_mul(fpu_mul(lwc1(tune), DEG), dt())));
        for (uint32_t k = 0; k < 4; ++k)
            swc1(pv + 0x10 + k * 4, angle_add(lwc1(pv + 0x10 + k * 4), fpu_mul(fpu_mul(lwc1(tune + 8 + k * 4), DEG), dt())));
        if (lbu(m + 0x31) != 0) sub_001f93e0(draw_fn(), m);
        const int32_t group = lw(pv + 0x20);
        if (group != -1 && class1443_group_all_hit(group) != 0) {
            sw(m + 0x94, 0);
            sb(m + 0x20, 2);
            class1443_group_sound(lw(pv + 0x20));
        }
        break;
    }
    case 2:
        if (class1443_group_all_hit(lw(pv + 0x20)) == 0) {
            sb(m + 0x20, 1);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
        break;
    default:
        break;
    }
}

// The cycling quads (the tuning's count, spread round the phase, scaled with the moby, their colour between the two by the
// cycle, each a third further on) in the first effect texture, then the four rings at their angles in the second, blended by
// the tuning's fields.
// orig: L16:0x002e58d8 L18:0x002fb018
void class1443_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1c0);
    const uint32_t quad = sp, colours = sp + 0x40, st = sp + 0x50, regs = sp + 0x70, mtx = sp + 0x90, at = sp + 0xc0, angles = sp + 0xd0;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning(), data = quads();
    const float scale = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
    lq_sq(angles, m + 0x40);
    lq_sq(at, m + 0x10);
    sd(regs + 8, effect_tex0(lw(tune + 0x38)));
    sd(regs + 0x10, (uint64_t{0xff90} << 32) | 0x260);
    sd(regs + 0x18, sx(tune + 0x28) | (sx(tune + 0x2c) << 2) | (sx(tune + 0x30) << 4) | (sx(tune + 0x34) << 6) | (uint64_t{0x8000} << 24));
    sd(regs, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(st + k * 8, lwc1(data + k * 8));
        swc1(st + k * 8 + 4, lwc1(data + k * 8 + 4));
        sw(colours + k * 4, lwu(tune + 0x1c));
        lq_sq(quad + k * 0x10, data + 0x40 + k * 0x10);
        vec3_scale(V(quad + k * 0x10), V(quad + k * 0x10), scale);
    }
    const float spread = fpu_div(TWO_PI, int_to_float(lw(tune + 0x40)));
    const float third = fpu_div(3.0f, int_to_float(lw(tune + 0x40)));
    float cycle = lwc1(pv + 0xc);
    while (cycle < 0.0f) cycle = fpu_add(cycle, 1.0f);
    while (1.0f < cycle) cycle = fpu_sub(cycle, 1.0f);
    for (int32_t i = 0; i < lw(tune + 0x40); ++i) {
        swc1(angles, angle_add(lwc1(pv + 8), fpu_mul(static_cast<float>(i), spread)));
        euler_to_mat3_vu(M(mtx), V(angles));
        const float t = 0.5f < cycle ? fpu_add(fpu_add(-cycle, 1.0f), fpu_add(-cycle, 1.0f)) : fpu_add(cycle, cycle);
        const uint32_t c = rgba_lerp(lwu(tune + 0x18), lwu(tune + 0x1c), t);
        const float next = fpu_add(cycle, third);
        sw(colours + 4, c);
        sw(colours, c);
        cycle = 1.0f < next ? fpu_sub(next, 1.0f) : next;
        quad_draw(quad, mtx);
    }
    sd(regs + 8, effect_tex0(lw(tune + 0x3c)));
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(st + k * 8, lwc1(data + 0x20 + k * 8));
        swc1(st + k * 8 + 4, lwc1(data + 0x20 + k * 8 + 4));
        sw(colours + k * 4, lwu(tune + 0x20));
        lq_sq(quad + k * 0x10, data + 0x80 + k * 0x10);
    }
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(angles, lwc1(pv + 0x10 + k * 4));
        euler_to_mat3_vu(M(mtx), V(angles));
        quad_draw(quad, mtx);
    }
    ee_stack_free(0x1c0);
}

}  // namespace rac1
