// Class 857's draw callback of Blarg (L6) and Orxon (L10): one port of the two levels' copies, the same code but for the
// glow: Orxon's copy calls Veldin's glow_draw, Blarg's its own build of it (L6:0x272498, a variant, still transliterated).
// Header shared_class857.h.
#include <game/ovl/shared_class857.h>

#include <game/ovl/blarg_part2.h>
#include <game/ovl/glows.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

// Every moby of the moby's group (+0x21) of its class that is shown (+0x31) gets three glows, at its pvars' points
// +0x140 / +0x150 / +0x160 in its colour +0x290, the size doubled while 0x15edb3 is set.
// orig: L6:0x002f37d0 L10:0x002d7808
void class857_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const float size = lbu(0x0015edb3) != 0 ? u2f(0x3de978d5u) : u2f(0x3d6978d5u);
    const float toward = u2f(0x3da3d70au);
    group_first(sp, lbu(m + 0x21), 0, 0);
    while (lwu(sp) != 0) {
        const uint32_t o = lwu(sp);
        if (lbu(o + 0x31) != 0 && lh(o + 0xa6) == lh(m + 0xa6)) {
            const uint32_t pv = lwu(o + 0x78);
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t point = pv + 0x140 + k * 0x10;
                if (on_level(BLARG))
                    bl2_272498_l6(point, lwu(pv + 0x290), 0, 0, 0, 0, 0, 0, size, toward, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                else
                    glow_draw(point, lwu(pv + 0x290), size, toward);
            }
        }
        group_next(sp, lwu(sp), 0, 0);
    }
    ee_stack_free(0x70);
}

}  // namespace rac1
