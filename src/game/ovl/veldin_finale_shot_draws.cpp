// Veldin's finale (level 18): the draw callbacks of the boss's shots, queued on the level's draw list (sub_001f93e0) by
// their classes' updates, hand-written: the shell's glow (class 564), the ring's and the burst's rings of quads (classes
// 624 / 628), the energy ball's glow (class 983). Each is called with the moby the class queued it for. Level 18 only:
// the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in
// veldin_finale_mobys.cpp.
//
// VU0: none of these callbacks has a VU0 instruction of its own; their vector work is in the engine's helpers (quad_draw,
// the vec3 / matrix helpers, euler_to_mat3_vu / euler_to_mat4_vu, sin_vu / cos_vu, angle_wrap / angle_add, rgba_lerp,
// atan2_f), which the generated code called the same way before (the notes of task P18.5 list the registers the
// originals leave).
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;  // (pi / 180)
constexpr float PI = 3.141590118408203f;     // (the game's pi, 0x40490fd0)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the GS ALPHA register of a quad: the four selectors (the level's words, sign-extended, at bits 0 / 2 / 4 / 6), fixed
// alpha 0x80
uint64_t blend_word(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    auto sx = [](uint32_t w) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w))); };
    return sx(lw(a)) | (sx(lw(b)) << 2) | (sx(lw(c)) << 4) | (sx(lw(d)) << 6) | (uint64_t{0x8000} << 24);
}

// the quad descriptor's fixed part: its texture, the GIF tag's register list, its blend, the word +0x70 clear
void quad_begin(uint32_t desc, uint64_t tex, uint64_t blend) {
    sd(desc + 0x78, tex);
    sd(desc + 0x70, 0);
    sd(desc + 0x80, (uint64_t{0xff90} << 32) | 0x260);
    sd(desc + 0x88, blend);
}

// What the ring (class 624) and the burst (class 628) draw alike: four quads at the moby's place lifted by `lift`, quad
// q's four corners at radius `radius_0 + radius_step * q`, the corners' angles -pi / -pi + `step` degrees turned by q
// quarter steps, their heights -/+ the phase's fraction x `height`; the phase `phase_0 + q / 4`; the colour between
// `colour_a` and `colour_b` by that fraction, faded twice (by the fraction x `colour_scale`, at most 1, then by `fade`,
// 0 .. 1) toward its own colour with alpha 0. The four quads are drawn round the full circle: 360 / `step` times, the
// matrix turned by `step` degrees about z each time. Texture 0xe, (s, t) the level's four pairs at `st`.
struct RingLook {
    uint32_t blend[4];
    uint32_t colour_a, colour_b, colour_scale, st, height, step, lift, radius_step;
};

void ring_draw(uint32_t m, const RingLook& look, float radius_0, float phase_0, float fade) {
    const Frame f(0x3c0);
    const uint32_t mat = f.sp + 0x240, angles = f.sp + 0x2b0, turn = f.sp + 0x280;
    mat4_identity(M(mat));
    lq_sq(mat + 0x30, m + 0x10);
    swc1(mat + 0x3c, 1.0f);
    swc1(mat + 0x38, fpu_add(lwc1(mat + 0x38), lwc1(look.lift)));
    if (fade > 1.0f) fade = 1.0f;
    else if (fade < 0.0f) fade = 0.0f;
    for (int32_t q = 0; q < 4; ++q) {
        const uint32_t desc = f.sp + static_cast<uint32_t>(q) * 0x90;
        const float qf = int_to_float(q);
        quad_begin(desc, effect_tex0(0xe), blend_word(look.blend[0], look.blend[1], look.blend[2], look.blend[3]));
        const float radius = fpu_add(radius_0, fpu_mul(lwc1(look.radius_step), qf));
        float phase = fpu_add(phase_0, fpu_mul(qf, 0.25f));
        phase = fpu_sub(phase, int_to_float(float_to_int(phase)));
        const uint32_t base = rgba_lerp(lwu(look.colour_a), lwu(look.colour_b), phase);
        float t = fpu_mul(phase, lwc1(look.colour_scale));
        if (t > 1.0f) t = 1.0f;
        uint32_t colour = rgba_lerp(base & 0xffffff, base, t);
        colour = rgba_lerp(colour & 0xffffff, colour, fade);
        for (int32_t k = 0; k < 4; ++k) {
            const uint32_t c = desc + static_cast<uint32_t>(k) * 0x10;
            swc1(desc + 0x50 + k * 8, lwc1(look.st + k * 8));
            swc1(desc + 0x54 + k * 8, lwc1(look.st + k * 8 + 4));
            swc1(c + 8, (k & 1) != 0 ? fpu_mul(phase, lwc1(look.height)) : fpu_mul(-phase, lwc1(look.height)));
            const float step = lwc1(look.step);
            const float a = angle_add(fpu_sub(fpu_mul(int_to_float(k >> 1), fpu_mul(step, DEG)), PI),
                                      fpu_mul(fpu_mul(fpu_mul(step, 0.25f), qf), DEG));
            swc1(c + 0, fpu_mul(cos_vu(a), radius));
            swc1(c + 4, fpu_mul(sin_vu(a), radius));
            swc1(c + 0xc, 1.0f);
            sw(desc + 0x40 + k * 4, colour);
        }
    }
    vec_clear(V(angles));
    swc1(angles + 8, fpu_mul(lwc1(look.step), DEG));
    euler_to_mat3_vu(M(turn), V(angles));
    for (int32_t n = 0; int_to_float(n) < fpu_div(360.0f, lwc1(look.step)); ++n) {
        for (uint32_t q = 0; q < 4; ++q) quad_draw(f.sp + q * 0x90, mat);
        mat3_mul(M(mat), M(mat), M(turn));
    }
}
}  // namespace

// The shell's glow (class 564): one quad, the level's four corners L18:0x1d3630 about the shell's point (its pvars +0x10),
// its colour between L18:0x161980 and 0x161984 by a sine of the low four bits of its pvars +0x2c (sixteen steps round),
// its blend L18:0x161970 .. 0x16197c, texture 0xd, (s, t) the level's four pairs at L18:0x1d3670.
// orig: L18:0x002d5768
void vf_cb_2d5768_l18(uint32_t m) {
    const Frame f(0x140);
    const uint32_t desc = f.sp, pv = lwu(m + 0x78);
    const float turn = fpu_mul(fpu_mul(int_to_float(static_cast<int32_t>(lwu(pv + 0x2c) & 0xf)), 0.0625f), 6.283180236816406f);
    const float t = fpu_mul(fpu_add(sin_vu(angle_wrap(turn)), 1.0f), 0.5f);
    const uint32_t colour = rgba_lerp(lwu(L18_ADDR(0x00161980)), lwu(L18_ADDR(0x00161984)), t);
    quad_begin(desc, effect_tex0(0xd), blend_word(L18_ADDR(0x00161970), L18_ADDR(0x00161974), L18_ADDR(0x00161978), L18_ADDR(0x0016197c)));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t c = desc + k * 0x10;
        vec3_add(V(c), V(L18_ADDR(0x001d3630) + k * 0x10), V(pv + 0x10));
        swc1(c + 0xc, 1.0f);
        sw(desc + 0x40 + k * 4, colour);
        swc1(desc + 0x50 + k * 8, lwc1(L18_ADDR(0x001d3670) + k * 8));
        swc1(desc + 0x54 + k * 8, lwc1(L18_ADDR(0x001d3670) + k * 8 + 4));
    }
    quad_draw(desc, 0);
}

// The ring (class 624): ring_draw with its size (pvars +0x24) as the base radius and, x L18:0x161c74, the phase; the fade
// (+0x28 - +0x24) / (+0x20 x frame_scale_f(L18:0x161c7c)): it fades in as it grows toward +0x28.
// orig: L18:0x002dba20
void vf_cb_2dba20_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const RingLook look{{L18_ADDR(0x00161c48), L18_ADDR(0x00161c4c), L18_ADDR(0x00161c50), L18_ADDR(0x00161c54)},
                        L18_ADDR(0x00161c5c), L18_ADDR(0x00161c60), L18_ADDR(0x00161c78), L18_ADDR(0x001d4310),
                        L18_ADDR(0x00161c6c), L18_ADDR(0x00161c64), L18_ADDR(0x00161c68), L18_ADDR(0x00161c70)};
    const float size = lwc1(pv + 0x24);
    const float fade = fpu_div(fpu_sub(lwc1(pv + 0x28), size), fpu_mul(lwc1(pv + 0x20), frame_scale_f(lwc1(L18_ADDR(0x00161c7c)))));
    ring_draw(m, look, size, fpu_mul(size, lwc1(L18_ADDR(0x00161c74))), fade);
}

// The burst (class 628): ring_draw with its pvars +0 as the base radius and +4 as the phase; the fade its frames left
// (+8) over frame_scale_i(L18:0x161c9c).
// orig: L18:0x002dc4b8
void vf_cb_2dc4b8_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const RingLook look{{L18_ADDR(0x00161ca4), L18_ADDR(0x00161ca8), L18_ADDR(0x00161cac), L18_ADDR(0x00161cb0)},
                        L18_ADDR(0x00161cb8), L18_ADDR(0x00161cbc), L18_ADDR(0x00161cd4), L18_ADDR(0x001d4330),
                        L18_ADDR(0x00161cc8), L18_ADDR(0x00161cc0), L18_ADDR(0x00161cc4), L18_ADDR(0x00161ccc)};
    const float fade = fpu_div(int_to_float(lw(pv + 8)), int_to_float(frame_scale_i(lw(L18_ADDR(0x00161c9c)))));
    ring_draw(m, look, lwc1(pv + 0), lwc1(pv + 4), fade);
}

// The energy ball's glow (class 983, queued by its update every frame): four quads about the ball, each the level's
// corners (L18:0x1d9b70, 0x40 bytes a quad) through its own matrix turned to face the camera (z toward the camera's
// bearing, y down by the camera's elevation): the first as it is, the second with its y / z rows scaled by the ball's
// size (pvars +0x24), the third and fourth the same turned about x by the ball's glow angles (+0x2c / +0x30). Quad q's
// texture the level's L18:0x161f08[q], its colour L18:0x161ef8[q] (the first's from alpha 0 to the colour by the size);
// its blend L18:0x161ee4 .. 0x161ef0, (s, t) the four pairs at L18:0x1d9b50.
// orig: L18:0x002ea1f8
void vf_cb_2ea1f8_l18(uint32_t m) {
    const Frame f(0x420);
    const uint32_t angles = f.sp + 0x240, pos = m + 0x10, camera = L18_ADDR(0x001677c0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t mats[4] = {f.sp + 0x250, f.sp + 0x290, f.sp + 0x2d0, f.sp + 0x310};
    vec_clear(V(angles));
    swc1(angles + 8, atan2_f(fpu_sub(lwc1(camera + 0), lwc1(m + 0x10)), fpu_sub(lwc1(camera + 4), lwc1(m + 0x14))));
    swc1(angles + 4, -atan2_f(vec2_distance(V(pos), V(camera)), fpu_sub(lwc1(camera + 8), lwc1(m + 0x18))));
    for (uint32_t i = 0; i < 4; ++i) {
        if (i == 2) swc1(angles, lwc1(pv + 0x2c));
        if (i == 3) swc1(angles, lwc1(pv + 0x30));
        euler_to_mat4_vu(M(mats[i]), V(angles));
        if (i != 0) {
            vec3_scale(V(mats[i] + 0x10), V(mats[i] + 0x10), lwc1(pv + 0x24));
            vec3_scale(V(mats[i] + 0x20), V(mats[i] + 0x20), lwc1(pv + 0x24));
        }
        lq_sq(mats[i] + 0x30, pos);
    }
    for (uint32_t q = 0; q < 4; ++q) {
        const uint32_t desc = f.sp + q * 0x90;
        quad_begin(desc, effect_tex0(lw(L18_ADDR(0x00161f08) + q * 4)),
                   blend_word(L18_ADDR(0x00161ee4), L18_ADDR(0x00161ee8), L18_ADDR(0x00161eec), L18_ADDR(0x00161ef0)));
        for (uint32_t k = 0; k < 4; ++k) {
            swc1(desc + 0x50 + k * 8, lwc1(L18_ADDR(0x001d9b50) + k * 8));
            swc1(desc + 0x54 + k * 8, lwc1(L18_ADDR(0x001d9b50) + k * 8 + 4));
            if (q == 0) {
                const uint32_t c = lwu(L18_ADDR(0x00161ef8));
                sw(desc + 0x40 + k * 4, rgba_lerp(c & 0xffffff, c, lwc1(pv + 0x24)));
            } else {
                sw(desc + 0x40 + k * 4, lwu(L18_ADDR(0x00161ef8) + q * 4));
            }
            lq_sq(desc + k * 0x10, L18_ADDR(0x001d9b70) + q * 0x40 + k * 0x10);
        }
    }
    for (uint32_t q = 0; q < 4; ++q) quad_draw(f.sp + q * 0x90, mats[q]);
}

}  // namespace rac1
