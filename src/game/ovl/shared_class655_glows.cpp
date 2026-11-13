// Class 655's glows: one port of the copies Quartu and Drek's fleet carry (the same code relinked). Each level keeps its own
// words (the two glows' colours and sizes, the corners and their texture coordinates: per level below); the camera is the
// engine's (OV 0x166dc0), the view's up axis the base ELF's (0x13f5e0). Header shared_class655_glows.h.
#include <game/ovl/shared_class655_glows.h>

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
// the level's words: +0 the outer glow's colour, +0x4c the inner one's, +0x54 / +0x58 their sizes
uint32_t words() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161af0) : L15_ADDR(0x00161c60); }
// the level's four corners (points), then their (s, t)
uint32_t corners() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001d3970) : L15_ADDR(0x001d36f0); }
}  // namespace

// The point pvar +0xf0 pulled 0.3 towards the camera, a frame facing the camera there; two quads, the inner (its size
// +- 0.025) then the outer (+- 0.05), in the effect texture 11, blended additively.
// orig: L15:0x002d8e48 L17:0x002d74c0
void class655_glows(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x180);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, frame = sp + 0x90, at = sp + 0xc0;
    const uint32_t point = lwu(m + 0x78) + 0xf0;
    const uint32_t cam = OV(0x00166dc0);
    const uint64_t tex0 = effect_tex0(0xb);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 8, tex0);
    sd(regs + 0x18, 0x8000000048ull);
    sd(regs, 0);
    vec3_sub(V(at), V(cam), V(point));
    vec3_set_length(V(at), V(at), u2f(0x3e99999au));  // (0.3)
    vec3_add(V(at), V(at), V(point));
    swc1(at + 0xc, 1.0f);
    vec3_sub(V(frame), V(cam), V(at));
    vec3_set_length(V(frame), V(frame), 1.0f);
    vec3_cross_ba(V(frame + 0x10), V(frame), V(0x0013f5e0));
    vec3_set_length(V(frame + 0x10), V(frame + 0x10), -1.0f);
    vec3_cross_ba(V(frame + 0x20), V(frame + 0x10), V(frame));
    const uint32_t w = words(), tmpl = corners();
    for (int32_t glow = 0; glow < 2; ++glow) {
        const uint32_t colour = lwu(glow == 0 ? w + 0x4c : w);
        for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, colour);
        const float size = glow == 0 ? fpu_add(lwc1(w + 0x54), rand_range_signed_f(0.0f, u2f(0x3ccccccdu)))   // (0.025)
                                     : fpu_add(lwc1(w + 0x58), rand_range_signed_f(0.0f, u2f(0x3d4ccccdu)));  // (0.05)
        for (uint32_t k = 0; k < 4; ++k) {
            vec3_scale(V(quad + k * 0x10), V(tmpl + k * 0x10), size);
            mat4_transform(V(quad + k * 0x10), V(quad + k * 0x10), M(frame));
            swc1(uvs + k * 8, lwc1(tmpl + 0x40 + k * 8));
            swc1(uvs + k * 8 + 4, lwc1(tmpl + 0x44 + k * 8));
        }
        quad_draw(quad, 0);
    }
    ee_stack_free(0x180);
}

}  // namespace rac1
