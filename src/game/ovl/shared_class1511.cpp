// Class 1511's draw callback: one port of the copies Rilgar and Oltanis carry (the same code relinked; compared with the
// relocated fields masked, and normalised). Its level-own data: two distances (below) and the quad's four corners; the
// camera 0x166dc0 is the engine's (OV). Header shared_class1511.h.
//
// The moby's pvars: its alpha +0x60 (halved at random while +0x67 is set), its colour +0x68, its size +0x70.
#include <game/ovl/shared_class1511.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
// the glow's distance along the moby's y axis (+0) and toward the camera (+4)
uint32_t glow_words() { return on_level(RILGAR) ? L5_ADDR(0x00161fc8) : L14_ADDR(0x0016229c); }
// the quad's four corners
uint32_t glow_corners() { return on_level(RILGAR) ? L5_ADDR(0x00215c70) : L14_ADDR(0x001f6200); }
}  // namespace

// The glow's centre (along the moby's y axis, then toward the camera) and a frame facing the camera (up 0, 0, 1); its
// corners scaled by the size and placed in that frame; effect texture 11, blended; its colour with the alpha in the top
// byte (while +0x67 is set, the alpha is halved one frame in eight first).
// orig: L5:0x0031cf58 L14:0x00308150
void class1511_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t d = sp, up = sp + 0x90, mat = sp + 0xa0, ax = mat, ay = mat + 0x10, az = mat + 0x20, centre = mat + 0x30;
    const uint32_t along = sp + 0xe0, toward = sp + 0xf0;
    V(up) = Vec4{};
    swc1(up + 8, 1.0f);
    swc1(up + 0xc, 1.0f);
    const uint32_t pv = lwu(m + 0x78);
    const uint64_t tex = effect_tex0(0xb);
    const uint32_t w = glow_words();
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x78, tex);
    sd(d + 0x70, 5);
    sd(d + 0x88, 0x8000000048ull);
    sw(d + 0x50, 0);
    sw(d + 0x54, 0);
    sw(d + 0x58, 0);
    swc1(d + 0x5c, 1.0f);
    swc1(d + 0x60, 1.0f);
    sw(d + 0x64, 0);
    swc1(d + 0x68, 1.0f);
    swc1(d + 0x6c, 1.0f);
    vec3_scale(V(along), V(m + 0xe0), lwc1(w));
    vec3_add(V(centre), V(m + 0x10), V(along));
    vec3_sub(V(ax), V(OV(0x00166dc0)), V(centre));
    vec3_set_length(V(ax), V(ax), 1.0f);
    vec3_cross_ba(V(ay), V(ax), V(up));
    vec3_set_length(V(ay), V(ay), 1.0f);
    vec3_cross_ba(V(az), V(ay), V(ax));
    vec3_scale(V(toward), V(ax), lwc1(w + 4));
    vec3_add(V(centre), V(centre), V(toward));
    if (lbu(pv + 0x67) != 0 && rand_mod(8) == 0) sw(pv + 0x60, static_cast<uint32_t>(lw(pv + 0x60) >> 1));
    const uint32_t colour = (lwu(pv + 0x60) << 24) | lwu(pv + 0x68);
    sw(d + 0x40, colour);
    sw(d + 0x4c, colour);
    sw(d + 0x48, colour);
    sw(d + 0x44, colour);
    const uint32_t corners = glow_corners();
    for (uint32_t k = 0; k < 0x40; k += 0x10) {
        vec3_scale(V(d + k), V(corners + k), lwc1(pv + 0x70));
        mat4_transform(V(d + k), V(d + k), M(mat));
    }
    quad_draw(d, 0);
    ee_stack_free(0x100);
}

}  // namespace rac1
