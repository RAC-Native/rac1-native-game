// Class 252's draw callbacks: one port of Batalia's (L8) and Oltanis's (L14) copies (the same code relinked; compared with
// the relocated fields masked). Their level-own data: four colours and a height (the beam's, +0..+0xc / +0x1c), the
// glow's offset +0x20 and size +0x24, and the glow's four corners (per level below); the camera 0x166dc0 is OV. Header
// shared_class252.h. The quads' descriptor (quad_draw): the corners +0, their colours +0x40, their texture coordinates
// +0x50, the GS registers +0x70.
#include <game/ovl/shared_class252.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
// the beam's colours (+0..+0xc) and height (+0x1c)
uint32_t beam_words() { return on_level(OLTANIS) ? L14_ADDR(0x00161a68) : L8_ADDR(0x00161878); }
// the glow's offset toward the camera (+0) and size (+4)
uint32_t glow_words() { return on_level(OLTANIS) ? L14_ADDR(0x00161a88) : L8_ADDR(0x00161898); }
// the glow's four corners
uint32_t glow_corners() { return on_level(OLTANIS) ? L14_ADDR(0x001de580) : L8_ADDR(0x001d1d80); }

// the quads' GS registers: PRIM 5, the texture's TEX0, 0xff9000000260, the alpha blend 0x8000000048
void quad_regs(uint32_t regs, uint32_t tex) {
    sd(regs + 8, effect_tex0(static_cast<int32_t>(tex)));
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs, 5);
    sd(regs + 0x18, 0x8000000048ull);
}
}  // namespace

// A beam of three crossed pairs of quads from the moby's joint 2 toward the moby pvars +0x218 names (+0.25 high), at
// most 3 long, turned along it (euler_to_mat4_vu); its widths from pvars +0x1d0 / +0x1e0 (their x scaled by the length).
// orig: L8:0x002d3878 L14:0x002da678
void class252_draw_beam(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1b0);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, matrix = sp + 0x90;
    const uint32_t origin = sp + 0xd0, dir = sp + 0xe0, angles = sp + 0xf0;
    const uint32_t words = beam_words();
    const uint64_t tex0 = effect_tex0(0xe);
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, lwu(words + k * 4));
    sd(regs + 8, tex0);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs, 5);
    sd(regs + 0x18, 0x8000000048ull);
    for (uint32_t k = 0; k < 4; ++k) {  // (0, 0) (1, 0) (0, 1) (1, 1)
        swc1(uvs + k * 8, (k & 1) != 0 ? 1.0f : 0.0f);
        swc1(uvs + k * 8 + 4, k >= 2 ? 1.0f : 0.0f);
    }
    moby_chain_point_v(m, 2, V(origin));
    vec3_sub(V(dir), V(lwu(pv + 0x218) + 0x10), V(origin));
    vec3_clamp_length(dir, 3.0f);
    swc1(dir + 0xc, 1.0f);
    swc1(dir + 8, fpu_sub(fpu_add(lwc1(lwu(pv + 0x218) + 0x18), 0.25f), lwc1(origin + 8)));
    const float len = vec3_length(V(dir));
    vec_clear(V(angles));
    swc1(angles + 4, -atan2_f(vec2_length(V(dir)), lwc1(dir + 8)));
    swc1(angles + 8, atan2_f(lwc1(dir), lwc1(dir + 4)));
    euler_to_mat4_vu(M(matrix), V(angles));
    lq_sq(matrix + 0x30, origin);
    swc1(matrix + 0x3c, 1.0f);

    // corner `v` from the width vector `src`: its x scaled by the length, its z moved by `dz`
    auto from_width = [&](uint32_t v, uint32_t src, float dz) {
        lq_sq(v, src);
        const float x = fpu_mul(len, lwc1(src + 0xc));
        swc1(v + 0xc, 1.0f);
        swc1(v + 8, fpu_add(lwc1(v + 8), dz));
        swc1(v, x);
    };
    // the corners `first`, `first` + 2 ... moved: those before 2 by (-0.25, +0.25) in y / z, the others (+0.25, -0.25)
    auto shear = [&](uint32_t first, uint32_t step) {
        for (uint32_t k = first; k < 4; k += step) {
            const uint32_t v = quad + k * 0x10;
            const float y = lwc1(v + 4), z = lwc1(v + 8);
            swc1(v + 4, k < 2 ? fpu_add(y, -0.25f) : fpu_add(y, 0.25f));
            swc1(v + 8, k < 2 ? fpu_add(z, 0.25f) : fpu_add(z, -0.25f));
        }
    };
    const uint32_t w0 = pv + 0x1d0, w1 = pv + 0x1e0;
    // the first pair: from the joint (its height +0x1c) to the first width
    vec_clear(V(quad));
    swc1(quad + 0xc, 1.0f);
    swc1(quad + 8, lwc1(words + 0x1c));
    from_width(quad + 0x10, w0, -0.25f);
    vec_clear(V(quad + 0x20));
    sw(quad + 0x28, 0);
    swc1(quad + 0x2c, 1.0f);
    from_width(quad + 0x30, w0, 0.25f);
    quad_draw(quad, matrix);
    shear(1, 2);
    quad_draw(quad, matrix);
    // the second pair: between the two widths
    from_width(quad, w0, -0.25f);
    from_width(quad + 0x10, w1, -0.25f);
    from_width(quad + 0x20, w0, 0.25f);
    from_width(quad + 0x30, w1, 0.25f);
    quad_draw(quad, matrix);
    shear(0, 1);
    quad_draw(quad, matrix);
    // the third pair: from the second width out to three times the first, at the joint's height
    from_width(quad, w1, -0.25f);
    vec_clear(V(quad + 0x10));
    {
        const float x = fpu_mul(fpu_mul(len, lwc1(w0 + 0xc)), 3.0f);
        swc1(quad + 0x1c, 1.0f);
        swc1(quad + 0x18, fpu_add(lwc1(quad + 0x18), lwc1(words + 0x1c)));
        swc1(quad + 0x10, x);
    }
    from_width(quad + 0x20, w1, 0.25f);
    vec_clear(V(quad + 0x30));
    swc1(quad + 0x3c, 1.0f);
    sw(quad + 0x38, 0);
    swc1(quad + 0x30, fpu_mul(fpu_mul(len, lwc1(w0 + 0xc)), 3.0f));
    quad_draw(quad, matrix);
    shear(0, 2);
    quad_draw(quad, matrix);
    ee_stack_free(0x1b0);
}

// A quad of the level's four corners (scaled by its size +4) about pvars +0x220 moved toward the camera by its offset,
// turned to face the camera; its colour the moby's +0x90 at alpha 0x40.
// orig: L8:0x002d4108 L14:0x002daf08
void class252_draw_glow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x170);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, up = sp + 0x90;
    const uint32_t frame = sp + 0xa0, side = sp + 0xb0, rise = sp + 0xc0, centre = sp + 0xd0, toward = sp + 0xe0;
    V(up) = Vec4{};
    swc1(up + 8, 1.0f);
    swc1(up + 0xc, 1.0f);
    quad_regs(regs, 0xb);
    for (uint32_t k = 0; k < 4; ++k) {  // (0, 0) (0, 1) (1, 0) (1, 1)
        swc1(uvs + k * 8, k >= 2 ? 1.0f : 0.0f);
        swc1(uvs + k * 8 + 4, (k & 1) != 0 ? 1.0f : 0.0f);
    }
    lq_sq(centre, pv + 0x220);
    vec3_sub(V(frame), V(OV(0x00166dc0)), V(centre));
    vec3_set_length(V(frame), V(frame), 1.0f);
    vec3_cross_ba(V(side), V(frame), V(up));
    vec3_set_length(V(side), V(side), 1.0f);
    vec3_cross_ba(V(rise), V(side), V(frame));
    const uint32_t words = glow_words();
    vec3_scale(V(toward), V(frame), lwc1(words));
    vec3_add(V(centre), V(centre), V(toward));
    const uint32_t colour = (lwu(m + 0x90) & 0xffffffu) | 0x40000000u;
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, colour);
    const uint32_t corners = glow_corners();
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t v = quad + k * 0x10;
        vec3_scale(V(v), V(corners + k * 0x10), lwc1(words + 4));
        mat4_transform(V(v), V(v), M(frame));
    }
    quad_draw(quad, 0);
    ee_stack_free(0x170);
}

}  // namespace rac1
