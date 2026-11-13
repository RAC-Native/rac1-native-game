// Class 28's draw callbacks: one port of the copies Oltanis and Quartu carry (the same code relinked). Each level keeps its
// own words (the glows' sizes and colour, two corner templates: per level below); the rest is the engine's (OV: the camera
// 0x166dc0) or the base ELF's (bare: the view's up axis 0x13f5e0). Header shared_class28_draw.h.
#include <game/ovl/shared_class28_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>

namespace rac1 {

namespace {
// the level's words: +0 the third glow's pull towards the camera, +0x10 the last two's, +0x14 their size; +0x4c the spot's
// size, +0x50 its reach, +0x54 its pull back, +0x58 its colour
uint32_t words() { return on_level(QUARTU) ? L15_ADDR(0x0016138c) : L14_ADDR(0x001614ac); }
// the level's two corner templates (four points each: the glows', then the spot's)
uint32_t corners() { return on_level(QUARTU) ? L15_ADDR(0x001cbf60) : L14_ADDR(0x001d8850); }

// the quad's GS words: the effect texture 11, blended additively (`clamp` the first word)
void quad_regs(uint32_t regs, uint64_t clamp) {
    const uint64_t tex0 = effect_tex0(0xb);
    sd(regs + 8, tex0);
    sd(regs + 0x18, 0x8000000048ull);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs, clamp);
}

// the frame at `frame` whose first axis points from `pos` (its row 3) at the camera, the view's up axis making the other two
void face_camera(uint32_t frame, uint32_t pos) {
    vec3_sub(V(frame), V(OV(0x00166dc0)), V(pos));
    vec3_set_length(V(frame), V(frame), 1.0f);
    vec3_cross_ba(V(frame + 0x10), V(frame), V(0x0013f5e0));
    vec3_set_length(V(frame + 0x10), V(frame + 0x10), -1.0f);
    vec3_cross_ba(V(frame + 0x20), V(frame + 0x10), V(frame));
}

// the quad: the template's four corners scaled by `size` through the frame, in `colour`
void glow_quad(uint32_t quad, uint32_t frame, uint32_t tmpl, uint32_t colour, float size) {
    for (uint32_t k = 0; k < 4; ++k) sw(quad + 0x40 + k * 4, colour);
    for (uint32_t k = 0; k < 4; ++k) {
        vec3_scale(V(quad + k * 0x10), V(tmpl + k * 0x10), size);
        mat4_transform(V(quad + k * 0x10), V(quad + k * 0x10), M(frame));
    }
    quad_draw(quad, 0);
}
}  // namespace

// Five quads facing the camera: at pvar +0x1e0 (0.4, white), +0x200 (0.2, dim grey), +0x1f0 pulled towards the camera
// (0.2, green), +0x210 and +0x220 pulled further (the level's size, red).
// orig: L14:0x002b2928 L15:0x00296710
void class28_draw_glows(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1a0);
    const uint32_t quad = sp, uvs = sp + 0x50, regs = sp + 0x70, frame = sp + 0x90, pos = sp + 0xc0, pull = sp + 0xd0;
    const uint32_t w = words(), tmpl = corners();
    lq_sq(pos, pv + 0x1e0);
    swc1(pos + 0xc, 1.0f);
    face_camera(frame, pos);
    quad_regs(regs, 5);
    // the corners' (s, t): (0, 0), (0, 1), (1, 0), (1, 1)
    swc1(uvs + 0x1c, 1.0f);
    sw(uvs, 0);
    sw(uvs + 4, 0);
    sw(uvs + 8, 0);
    swc1(uvs + 0xc, 1.0f);
    swc1(uvs + 0x10, 1.0f);
    sw(uvs + 0x14, 0);
    swc1(uvs + 0x18, 1.0f);
    glow_quad(quad, frame, tmpl, 0x80ffffffu, u2f(0x3ecccccdu));  // (0.4)
    lq_sq(pos, pv + 0x200);
    face_camera(frame, pos);
    glow_quad(quad, frame, tmpl, 0x202020ffu, u2f(0x3e4ccccdu));  // (0.2)
    lq_sq(pos, pv + 0x1f0);
    face_camera(frame, pos);
    vec3_scale(V(pull), V(frame), lwc1(w));
    vec3_add(V(pos), V(pos), V(pull));
    glow_quad(quad, frame, tmpl, 0x4020ff20u, u2f(0x3e4ccccdu));
    for (uint32_t at : {0x210u, 0x220u}) {
        lq_sq(pos, pv + at);
        face_camera(frame, pos);
        vec3_scale(V(pull), V(frame), lwc1(w + 0x10));
        vec3_add(V(pos), V(pos), V(pull));
        glow_quad(quad, frame, tmpl, 0x40ff2020u, lwc1(w + 0x14));
    }
    ee_stack_free(0x1a0);
}

// One quad at the moby's place (0.01 up), moved towards the camera by its distance less the level's pull back (at most the
// level's reach, at least 0), the second template scaled by the level's size, in the level's colour.
// orig: L14:0x002b3a20 L15:0x00297808
void class28_draw_spot(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t pos = sp, quad = sp + 0x10, uvs = sp + 0x60, regs = sp + 0x80, to_eye = sp + 0xa0, move = sp + 0xb0;
    const uint32_t w = words();
    lq_sq(pos, m + 0x10);
    const float z = fpu_add(lwc1(pos + 8), u2f(0x3c23d70au));  // (0.01)
    swc1(pos + 0xc, 1.0f);
    swc1(pos + 8, z);
    quad_regs(regs, 0);
    sub_002a0ba8(0x4a, 0);
    sub_002a0ba8(0x47, 0x51001);
    vec3_sub(V(to_eye), V(OV(0x00166dc0)), V(pos));
    const float distance = vec3_length(V(to_eye));
    if (0.0f < distance) {
        float reach = lwc1(w + 0x50);
        const float near = fpu_sub(distance, lwc1(w + 0x54));
        if (near < reach) reach = near < 0.0f ? 0.0f : near;
        vec3_set_length(V(move), V(to_eye), reach);
        vec3_add(V(pos), V(pos), V(move));
    }
    const uint32_t colour = lwu(w + 0x58);
    // the corners' (s, t): (1, 1), (0, 1), (1, 0), (0, 0)
    swc1(uvs + 0x10, 1.0f);
    sw(quad + 0x40, colour);
    swc1(uvs, 1.0f);
    swc1(uvs + 4, 1.0f);
    sw(uvs + 8, 0);
    swc1(uvs + 0xc, 1.0f);
    sw(uvs + 0x14, 0);
    sw(uvs + 0x18, 0);
    sw(uvs + 0x1c, 0);
    sw(quad + 0x4c, colour);
    sw(quad + 0x48, colour);
    sw(quad + 0x44, colour);
    const uint32_t tmpl = corners() + 0x40;
    for (uint32_t k = 0; k < 4; ++k) {
        vec3_scale(V(quad + k * 0x10), V(tmpl + k * 0x10), lwc1(w + 0x4c));
        vec3_add(V(quad + k * 0x10), V(quad + k * 0x10), V(pos));
    }
    quad_draw(quad, 0);
    ee_stack_free(0x110);
}

}  // namespace rac1
