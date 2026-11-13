// Class 1378 and its draw callback: one port of the copies Orxon (L10) and Drek's fleet (L17) carry, the same code relinked
// (the normalised texts compared). Its addresses are the engine's (OV: the frame counter 0x15f3f8) or the base ELF's (bare:
// the hero's position 0x13f3d0 and centre 0x13f420, his flag 0x14161b); the level's own data is one block of the draw's
// tables, one block of its small words (per level below) and the address of the draw callback the update queues.
// Header shared_class1378.h.
//
// The pvars: the quads' scale in x +0 and in z +4.
#include <game/ovl/shared_class1378.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// The level's tables (its own data, one block): a colour a layer +0, two ripple periods a layer +0x20 (seconds, floats),
// whether each ripple is a cosine +0x60, the four corners' texture coordinates +0xa0, the corners of the second quad +0xc0 and
// of the first +0x100.
uint32_t tables(uint32_t o) {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001d98a0) + o;
    return L10_ADDR(0x001da250) + o;
}
// The level's words (its own small data): the layer count +0, and the blend's selectors, two of each (even and odd layers):
// +8, +0x10, +0x18, +0x20.
uint32_t words(uint32_t o) {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161fb0) + o;
    return L10_ADDR(0x00161f40) + o;
}
// the level's copy of the draw callback (the address the update queues)
uint32_t draw_addr() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x002e9798);
    return L10_ADDR(0x002e91b8);
}
}  // namespace

// State 0: solid (+0x34 bit 0), hidden (+0x31), 1. State 1: with the hero within 8 units, his centre in the moby's
// coordinates: inside the box (-5 .. 5 across, 0 .. 8 along z) the flag 0x14161b says whether he is below 0.5 in y; outside it,
// the flag is cleared when he is in front of it (y > 0). Its draw is queued each frame.
// orig: L10:0x002e9028 L17:0x002e9608
void class1378_update(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        return;
    }
    if (state != 1) return;
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t local = sp, undo = sp + 0x10;
    if (vec3_distance(V(m + 0x10), V(0x0013f3d0)) < 8.0f) {
        vec3_sub(V(local), V(0x0013f420), V(m + 0x10));
        mat3_transpose(M(undo), M(m + 0xc0));
        mat4_transform(V(local), V(local), M(undo));
        const float x = lwc1(local), y = lwc1(local + 4), z = lwc1(local + 8);
        if (-5.0f < x && x < 5.0f && 0.0f < z && z < 8.0f) sb(0x0014161b, y < 0.5f ? state : 0u);
        else if (0.0f < y) sb(0x0014161b, 0);
    }
    sub_001f93e0(draw_addr(), m);
    ee_stack_free(0xb0);
}

// The level's layers (+0 of its words), each two quads of the level's corners (scaled by the pvars in x and z), through the
// moby's matrix, with effect texture 0x2c and the layer's colour (the second quad's on its first and third corners), their
// texture coordinates the level's rippled by two waves of the frame counter (each its period, a sine or a cosine of an
// eighth), the second's shifted by an eighth; each layer 0.1 above the last.
// orig: L10:0x002e91b8 L17:0x002e9798
void class1378_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x240);
    const uint32_t qa = sp, qb = sp + 0x90, matrix = sp + 0x120;
    const uint32_t pv = lwu(m + 0x78);
    mat3_copy_affine(M(matrix), M(m + 0xc0));
    lq_sq(matrix + 0x30, m + 0x10);
    swc1(matrix + 0x3c, 1.0f);
    const uint64_t tex1 = 0x0000ff9000000260ull;
    sd(qa + 0x78, effect_tex0(0x2c));
    sd(qa + 0x80, tex1);
    sd(qa + 0x70, 0);
    sd(qb + 0x78, effect_tex0(0x2c));
    sd(qb + 0x80, tex1);
    sd(qb + 0x70, 0);
    sw(qb + 0x4c, 0);
    sw(qb + 0x44, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t a = qa + k * 0x10, b = qb + k * 0x10;
        lq_sq(a, tables(0x100) + k * 0x10);
        lq_sq(b, tables(0xc0) + k * 0x10);
        swc1(a, fpu_mul(lwc1(a), lwc1(pv)));
        swc1(b, fpu_mul(lwc1(b), lwc1(pv)));
        swc1(a + 8, fpu_mul(lwc1(a + 8), lwc1(pv + 4)));
        swc1(b + 8, fpu_mul(lwc1(b + 8), lwc1(pv + 4)));
    }
    // (the selectors loaded sign-extended and shifted as doublewords, as the original does)
    auto word = [](uint32_t o) { return static_cast<uint64_t>(static_cast<int64_t>(lw(words(o)))); };
    for (int32_t layer = 0; layer < lw(words(0)); ++layer) {
        const uint32_t odd = static_cast<uint32_t>(layer & 1) << 2;
        const uint64_t blend = word(8 + odd) | word(0x10 + odd) << 2 | word(0x18 + odd) << 4 | word(0x20 + odd) << 6 | (0x8000ull << 24);
        sd(qa + 0x88, blend);
        sd(qb + 0x88, blend);
        const uint32_t colour = lwu(tables(0) + static_cast<uint32_t>(layer) * 4);
        for (uint32_t k = 0; k < 4; ++k) sw(qa + 0x40 + k * 4, colour);
        sw(qb + 0x40, colour);
        sw(qb + 0x48, colour);
        const uint32_t ripple = static_cast<uint32_t>(layer) << 3;
        for (uint32_t k = 0; k < 4; ++k) {
            for (uint32_t j = 0; j < 2; ++j) {
                const uint32_t period = tables(0x20) + ripple + j * 4;
                const int32_t frames = float_to_int(fpu_mul(lwc1(period), 360.0f));
                const float at = fpu_sub(fpu_div(int_to_float(ee_rem(lw(OV(0x0015f3f8)), frames)), lwc1(period)), 180.0f);
                const float wave = lwu(tables(0x60) + ripple + j * 4) != 0 ? cos_vu(fpu_mul(at, u2f(0x3c8efa35u))) : sin_vu(fpu_mul(at, u2f(0x3c8efa35u)));
                swc1(qa + 0x50 + k * 8 + j * 4, fpu_add(lwc1(tables(0xa0) + k * 8 + j * 4), fpu_mul(wave, 0.125f)));
            }
        }
        for (uint32_t j = 0; j < 2; ++j) {
            const float shift = fpu_mul(int_to_float(static_cast<int32_t>(j)), 0.125f);
            swc1(qb + 0x50 + j * 4, lwc1(qa + 0x58 + j * 4));
            swc1(qb + 0x58 + j * 4, fpu_add(lwc1(qa + 0x58 + j * 4), shift));
            swc1(qb + 0x60 + j * 4, lwc1(qa + 0x68 + j * 4));
            swc1(qb + 0x68 + j * 4, fpu_add(lwc1(qa + 0x68 + j * 4), shift));
        }
        quad_draw(qa, matrix);
        quad_draw(qb, matrix);
        for (uint32_t k = 0; k < 4; ++k) {
            swc1(qa + k * 0x10 + 4, fpu_add(lwc1(qa + k * 0x10 + 4), u2f(0x3dcccccdu)));
            swc1(qb + k * 0x10 + 4, fpu_add(lwc1(qb + k * 0x10 + 4), u2f(0x3dcccccdu)));
        }
    }
    ee_stack_free(0x240);
}

}  // namespace rac1
