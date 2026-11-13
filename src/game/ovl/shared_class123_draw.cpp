// Class 123's draw callback: one port of the copies Quartu and Drek's fleet carry (the same code relinked). Each level keeps
// its own corners' texture coordinates (per level below); the camera is the engine's (OV 0x166dc0). Header
// shared_class123_draw.h.
//
// The pvars: +0x10 the beam's far end.
#include <game/ovl/shared_class123_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the level's four corners' (s, t)
uint32_t beam_uvs() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001e63c0) : L15_ADDR(0x001e27f0); }
}  // namespace

// The beam from 0.5 along the way from the moby to its far end (pvar +0x10), 0.1 wide at each end across the beam as
// the camera sees it, in the effect texture 14, a pale blue, blended additively.
// orig: L15:0x002a7628 L17:0x002acf00
void class123_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1a0);
    const uint32_t half = sp, side_near = sp + 0x10, quad = sp + 0x20, regs = sp + 0x90;
    const uint32_t side_far = sp + 0xb0, along = sp + 0xc0, start = sp + 0xd0, corner = sp + 0xe0;
    const uint32_t far = pv + 0x10, cam = OV(0x00166dc0);
    vec3_sub(V(side_near), V(far), V(m + 0x10));
    lq_sq(half, side_near);
    vec3_set_length(V(half), V(half), 0.5f);
    lq_sq(start, m + 0x10);
    vec3_add_inplace(V(start), V(half));
    vec3_sub(V(along), V(start), V(far));
    vec3_sub(V(side_near), V(start), V(cam));
    vec3_sub(V(side_far), V(far), V(cam));
    vec3_cross_ba(V(side_near), V(side_near), V(along));
    vec3_set_length(V(side_near), V(side_near), 0.1f);
    vec3_cross_ba(V(side_far), V(side_far), V(along));
    vec3_set_length(V(side_far), V(side_far), 0.1f);
    const uint64_t tex0 = effect_tex0(0xe);
    sd(regs + 8, tex0);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 0x18, 0x8000000048ull);
    for (uint32_t k = 0; k < 4; ++k) sw(quad + 0x40 + k * 4, 0x80243278u);
    sd(regs, 0);
    const uint32_t uv = beam_uvs();
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(quad + 0x50 + k * 8, lwc1(uv + k * 8));
        swc1(quad + 0x54 + k * 8, lwc1(uv + k * 8 + 4));
    }
    vec3_sub(V(corner), V(start), V(side_near));
    lq_sq(quad, corner);
    vec3_add(V(corner), V(start), V(side_near));
    lq_sq(quad + 0x10, corner);
    vec3_sub(V(corner), V(far), V(side_far));
    lq_sq(quad + 0x20, corner);
    vec3_add(V(corner), V(far), V(side_far));
    lq_sq(quad + 0x30, corner);
    quad_draw(quad, 0);
    ee_stack_free(0x1a0);
}

}  // namespace rac1
