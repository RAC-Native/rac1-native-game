// Class 1135's draw callback, the teleporter's ring of light: one port of the copies eight levels carry (Novalis, Umbris,
// Batalia, Hoven, Gemlik, Kalebo III, Drek's fleet, the finale's Veldin). The copies are the same code relinked (compared
// with the relocated fields masked); each level has its own copy of the ring's two tables (its own data, per level below:
// the blend's four selectors, the texture, the colour and the ring's height; the four corners' texture coordinates). The
// rest is the base ELF's (bare: the hero's moby 0x1413d0). Header shared_class1135.h.
#include <game/ovl/shared_class1135.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
// the level's ring words (seven: the blend's selectors +0..+0xc, the texture +0x10, the colour +0x14, the height +0x18)
uint32_t ring_words() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00161fb8);
    if (on_level(UMBRIS)) return L7_ADDR(0x00161c68);
    if (on_level(HOVEN)) return L12_ADDR(0x00161d88);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161de0);
    if (on_level(KALEBO)) return L16_ADDR(0x00161d20);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161ee8);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001621b8);
    return L8_ADDR(0x00162298);
}
// the level's corners' texture coordinates (four pairs of floats)
uint32_t ring_uvs() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00201630);
    if (on_level(UMBRIS)) return L7_ADDR(0x0020e890);
    if (on_level(HOVEN)) return L12_ADDR(0x001fb4f0);
    if (on_level(GEMLIK)) return L13_ADDR(0x001d9b50);
    if (on_level(KALEBO)) return L16_ADDR(0x001d9660);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001d9840);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001da2d0);
    return L8_ADDR(0x001ef770);
}
}  // namespace

// 32 quads round a ring of the radius pvars +0x34 about the moby (its matrix, its position), each spanning a 32nd of a
// turn and the ring's height +- a half thickness: 0.1, or (with a radius under 1.15) 0.1 more per unit the radius falls
// short of 1.15, doubled and kept within 0..1. Out of states 7 / 8, that small a radius also sets the hero moby's byte
// +0x25 to whether the radius is under 0.65. The quads' descriptor: the corners +0, their colours +0x40, their texture
// coordinates +0x50, the GS registers +0x70 (the texture's TEX0, the CLAMP-like word 0xff9000000260, the blend).
// orig: L1:0x003094f0 L7:0x0031ce40 L8:0x00304bc0 L12:0x00301600 L13:0x003051a0 L16:0x002e0a38 L17:0x002e5848 L18:0x002ee490
void class1135_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x180);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, matrix = sp + 0x90;
    const uint32_t pv = lwu(m + 0x78);
    mat3_copy_affine(M(matrix), M(m + 0xc0));
    lq_sq(matrix + 0x30, m + 0x10);
    const uint32_t words = ring_words();
    const uint64_t tex0 = effect_tex0(lw(words + 0x10));
    // (the selectors loaded sign-extended and shifted as doublewords, as the original does)
    auto word = [words](uint32_t o) { return static_cast<uint64_t>(static_cast<int64_t>(lw(words + o))); };
    const uint64_t blend = word(0) | word(4) << 2 | word(8) << 4 | word(0xc) << 6 | 0x8000000000ull;
    sd(regs + 8, tex0);
    sd(regs + 0x18, blend);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs, 0);
    const uint32_t uv = ring_uvs();
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(uvs + k * 8, lwc1(uv + k * 8));
        swc1(uvs + k * 8 + 4, lwc1(uv + k * 8 + 4));
        sw(colours + k * 4, lwu(words + 0x14));
    }
    float half = 0.1f;
    const float radius = lwc1(pv + 0x34);
    if (radius < u2f(0x3f933333u)) {
        float t = fpu_sub(u2f(0x3f933333u), radius);
        t = fpu_add(t, t);
        if (1.0f < t) t = 1.0f;
        else if (t < 0.0f) t = 0.0f;
        half = fpu_add(t, 0.1f);
        if (lbu(m + 0x20) - 7u >= 2u) sb(0x001413f5, radius < u2f(0x3f266666u) ? 1u : 0u);
    }
    for (int32_t seg = 0; seg < 32; ++seg) {
        for (int32_t k = 0; k < 4; ++k) {
            const uint32_t v = quad + static_cast<uint32_t>(k) * 0x10;
            const float a = fpu_sub(fpu_mul(fpu_mul(int_to_float(seg + k / 2), u2f(0x40c90fd0u)), 0.03125f), u2f(0x40490fd0u));
            swc1(v, fpu_mul(cos_vu(a), lwc1(pv + 0x34)));
            sw(v + 8, 0);
            swc1(v + 4, fpu_mul(sin_vu(a), lwc1(pv + 0x34)));
            const float h = lwc1(words + 0x18);
            swc1(v + 8, fpu_add((k & 1) == 0 ? fpu_sub(h, half) : fpu_add(h, half), 0.0f));
            swc1(v + 0xc, 1.0f);
        }
        quad_draw(quad, matrix);
    }
    ee_stack_free(0x180);
}

}  // namespace rac1
