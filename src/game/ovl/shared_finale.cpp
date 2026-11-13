// Two families of Veldin's finale's own (level 18): a mesh of textured quads drawn for a moby (its points, its quads' corner
// indexes and texture coordinates in the level's tables, one set of tables per copy; the texture scrolled along s by a
// level word, the mesh lifted by the moby's height over its pvar +0), and a class hook that stops a moby. Each family's
// copies are the same code, apart from each draw's tables and quad count and each hook's state. Level words are L18_ADDR.
// Header shared_finale.h.
#include <game/ovl/shared_finale.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

namespace {
// a mesh's tables: its points (16 bytes), its quads (four corners of two halves: point, coordinate), its coordinates (s, t)
struct QuadSet {
    uint32_t points, quads, coords;
    int32_t count;
};
QuadSet quad_set(int32_t set) {
    switch (set) {
    case 0: return {L18_ADDR(0x001da3c0), L18_ADDR(0x001da610), L18_ADDR(0x001da7a0), 25};
    case 1: return {L18_ADDR(0x001da8d0), L18_ADDR(0x001dac20), L18_ADDR(0x001dae80), 38};
    default: return {L18_ADDR(0x001db030), L18_ADDR(0x001db240), L18_ADDR(0x001db3a0), 22};
    }
}

uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
}  // namespace

// orig: L18:0x002f2620 L18:0x002f27c8 L18:0x002f2970
void finale_quads_draw(int32_t set, uint32_t m) {
    const QuadSet q = quad_set(set);
    const uint32_t pv = lwu(m + 0x78);
    // (the frame's slots as the original lays them out: the quad's descriptor +0, its matrix +0x90)
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t desc = sp, matrix = sp + 0x90;
    const uint64_t tex = effect_tex0(lw(L18_ADDR(0x00162324)));
    const uint64_t alpha = sx(lwu(L18_ADDR(0x00162310))) | (sx(lwu(L18_ADDR(0x00162314))) << 2) | (sx(lwu(L18_ADDR(0x00162318))) << 4) |
                           (sx(lwu(L18_ADDR(0x0016231c))) << 6) | (sx(lwu(L18_ADDR(0x00162320))) << 32);
    sd(desc + 0x78, tex);
    sd(desc + 0x88, alpha);
    sd(desc + 0x80, (0xff90ull << 32) | 0x260u);
    sd(desc + 0x70, 0);
    mat4_identity(M(matrix));
    const uint32_t colour = lwu(L18_ADDR(0x00162328));
    for (uint32_t k = 0; k < 4; ++k) sw(desc + 0x40 + k * 4, colour);
    swc1(matrix + 0x38, fpu_sub(lwc1(m + 0x18), lwc1(pv)));  // (lifted by the moby's height over its base)
    for (int32_t i = 0; i < q.count; ++i) {
        const float scroll = lwc1(L18_ADDR(0x00162308));
        const uint32_t quad = q.quads + static_cast<uint32_t>(i) * 0x10;
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t point = lh(quad + k * 4);
            const int32_t coord = lh(quad + k * 4 + 2);
            V(desc + k * 0x10) = V(q.points + (static_cast<uint32_t>(point) << 4));
            const uint32_t st = q.coords + (static_cast<uint32_t>(coord) << 3);
            swc1(desc + 0x50 + k * 8, fpu_add(lwc1(st), scroll));
            swc1(desc + 0x54 + k * 8, lwc1(st + 4));
        }
        quad_draw(desc, matrix);
    }
    ee_stack_free(0xd0);
}

// orig: L18:0x002d6280 (state 5) L18:0x002fcd00 (state 8)
void finale_stop_hook(uint32_t m, uint32_t state) {
    sb(m + 0x20, state);
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
    sb(m + 0x31, 0);
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 1u) & 0xefffu);
}

}  // namespace rac1
