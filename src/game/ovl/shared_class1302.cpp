// Class 1302's draw callback of Blarg (L6) and Orxon (L10): one port of the two levels' copies, which are the same code
// relinked (the normalised texts compared). The level's own data is one block of the mesh's tables (per level below); the
// camera's yaw 0x166dd8 is the engine's (OV). Header shared_class1302.h.
//
// The pvars: the texture's scroll +0x1c, the ring's growth +0x20 (0 .. 1), the size +0x24.
#include <game/ovl/shared_class1302.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// The level's mesh (its own data, one block): the points +0 (16 bytes each), their texture coordinates +0x90 (pairs of
// floats), their colours +0xd8 (a word each), the four quads' corners +0x100 (four point indices each).
uint32_t mesh(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x001da110) + o;
    return L6_ADDR(0x001fe8d0) + o;
}

// the rotation about z by the camera's yaw (into `matrix`, its angles at `angles`)
void facing(uint32_t matrix, uint32_t angles) {
    swc1(angles + 8, lwc1(OV(0x00166dd8)));
    sw(angles, 0);
    sw(angles + 4, 0);
    euler_to_mat3_vu(M(matrix), V(angles));
}

// a value that grows by 0.01 a frame (`wrap`: back by 1 past 1, else to 0)
float grow(uint32_t a, bool wrap) {
    const float v = fpu_add(lwc1(a), u2f(0x3c23d70au));
    swc1(a, v);
    if (1.0f < v) {
        if (wrap) swc1(a, fpu_sub(v, 1.0f));
        else sw(a, 0);
    }
    return lwc1(a);
}
}  // namespace

// The four quads of the level's mesh (the moby's size scaling x and y), turned to the camera's yaw about the moby, with the
// effect texture 0x18 scrolling down (+0x1c, a hundredth a frame); then a quad of the effect texture 0x1b over them, its
// corners spreading (1.2 x the growth, x the size) and rising (1.1 + 1.4 x the growth, the far edge 0.2 lower) as it grows,
// its alpha fading from 128 to 0.
// orig: L6:0x00307578 L10:0x002e7520
void class1302_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x270);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, turned = sp + 0x90;
    const uint32_t angles = sp + 0xd0, matrix = sp + 0xe0, corners = sp + 0x110, ring = sp + 0x150;
    const uint32_t pv = lwu(m + 0x78);
    facing(matrix, angles);
    grow(pv + 0x1c, true);
    sd(regs + 0x18, 0x0000004000000044ull);
    sd(regs + 0x10, 1);
    sd(regs, 0);
    sd(regs + 8, effect_tex0(0x18));
    const float px = lwc1(m + 0x10), py = lwc1(m + 0x14), pz = lwc1(m + 0x18);
    for (uint32_t q = 0; q < 4; ++q) {
        const uint32_t e = mesh(0x100) + q * 0x10;
        for (uint32_t k = 0; k < 4; ++k) mat3_transform(V(turned + k * 0x10), V(mesh(0) + (lwu(e + k * 4) << 4)), M(matrix));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t t = turned + k * 0x10, c = quad + k * 0x10;
            swc1(c, fpu_add(px, fpu_mul(lwc1(t), lwc1(pv + 0x24))));
            swc1(c + 4, fpu_add(py, fpu_mul(lwc1(t + 4), lwc1(pv + 0x24))));
            swc1(c + 8, fpu_add(pz, lwc1(t + 8)));
        }
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = lwu(e + k * 4);
            sw(colours + k * 4, lwu(mesh(0xd8) + (idx << 2)));
            swc1(uvs + k * 8, lwc1(mesh(0x90) + (idx << 3)));
            swc1(uvs + k * 8 + 4, fpu_add(lwc1(mesh(0x94) + (idx << 3)), lwc1(pv + 0x1c)));
        }
        quad_draw(quad, 0);
    }

    sd(regs + 8, effect_tex0(0x1b));
    grow(pv + 0x20, false);
    for (uint32_t k = 0; k < 4; ++k)
        for (uint32_t w = 0; w < 0x10; w += 4) sw(ring + k * 0x10 + w, 0);
    const float spread = u2f(0x3f99999au), rise = u2f(0x3fb33333u), base = u2f(0x3f8ccccdu), drop = u2f(0x3e4ccccdu);
    auto out = [pv, spread](bool negative) {  // (+-) the growth x 1.2 x the size
        const float g = negative ? -lwc1(pv + 0x20) : lwc1(pv + 0x20);
        return fpu_mul(fpu_mul(g, spread), lwc1(pv + 0x24));
    };
    auto up = [pv, rise, base, drop](bool far) {
        const float h = fpu_add(fpu_mul(lwc1(pv + 0x20), rise), base);
        return far ? fpu_sub(h, drop) : h;
    };
    swc1(ring, out(true));
    swc1(ring + 4, out(true));
    swc1(ring + 8, up(false));
    swc1(ring + 0x10, out(false));
    swc1(ring + 0x14, out(true));
    swc1(ring + 0x18, up(true));
    swc1(ring + 0x20, out(true));
    swc1(ring + 0x24, out(false));
    swc1(ring + 0x28, up(false));
    swc1(ring + 0x30, out(false));
    swc1(ring + 0x34, out(false));
    swc1(ring + 0x38, up(true));
    for (uint32_t k = 0; k < 4; ++k) lq_sq(corners + k * 0x10, ring + k * 0x10);
    facing(matrix, angles);
    for (uint32_t k = 0; k < 4; ++k) mat3_transform(V(turned + k * 0x10), V(corners + k * 0x10), M(matrix));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t t = turned + k * 0x10, c = quad + k * 0x10;
        swc1(c, fpu_add(px, lwc1(t)));
        swc1(c + 4, fpu_add(py, lwc1(t + 4)));
        swc1(c + 8, fpu_add(pz, lwc1(t + 8)));
    }
    const uint32_t colour = (static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, lwc1(pv + 0x20)), 128.0f))) << 24) | 0x808080u;
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, colour);
    static const float kUv[8] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    for (uint32_t k = 0; k < 8; ++k) swc1(uvs + k * 4, kUv[k]);
    quad_draw(quad, 0);
    ee_stack_free(0x270);
}

}  // namespace rac1
