// The sea of Umbris and Gaspar, class 1139, and the puff of particle 0x34: one port each of the levels' copies (the same
// code relinked; compared word by word: only relocated words differ). Level-own words: the sea's wave record and draw
// callback, the puff's records (per level below); the engine's (OV): the zero vector 0x15f580; the base ELF's (bare): the
// hero's state 0x1413dc. Header shared_seas.h.
#include <game/ovl/shared_seas.h>

#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

// orig: L7:0x002f8058 L9:0x002ef7f8
void sea_update(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        ocean_wave_table_set(on_level(UMBRIS) ? L7_ADDR(0x001d3400) : L9_ADDR(0x001fc740), 0x80, 1.0f);
    } else if (state == 1) {
        draw_hook_add_dc80_l2(on_level(UMBRIS) ? L7_ADDR(0x002f8038) : L9_ADDR(0x002ef750), m);
    }
}

// orig: L5:0x0031abe0 L16:0x002e0f78
void class1139_update(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x30, 0x80);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x41);
        return;
    }
    if (state != 1) return;
    const uint32_t sp = ee_stack_alloc(0x50);
    lq_sq(sp, m + 0x10);
    swc1(sp + 0xc, 8.0f);
    if (point_on_screen(sp, 128.0f) != -1 && lw(0x001413dc) == 0x16) {
        for (int32_t k = 0; k < 3; ++k) {
            const float a = rand_angle();
            const float b = rand_angle();
            particle_43_spawn(m, OV(0x0015f580), frame_scale_i(0x28), 1, 0x7f, 0, a, b);
        }
    }
    ee_stack_free(0x50);
}

// A point within the record's reach about the moby (0.025 up), the particle's two sizes and its life dealt from the
// record's bounds.
uint32_t particle34_puff(uint32_t m, uint32_t rec) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const float a = rand_angle();
    const float r = rand_range_f(0.0f, lwc1(rec + 0x18));
    const float size = rand_range_f(0.0f, lwc1(rec + 8));
    const float grow = rand_range_f(lwc1(rec + 8), lwc1(rec + 0xc));
    const int32_t life = rand_range(lw(rec + 0x10), lw(rec + 0x14));
    swc1(sp, fpu_mul(cos_vu(a), r));
    swc1(sp + 8, 0.0f);
    swc1(sp + 4, fpu_mul(sin_vu(a), r));
    vec3_add(V(sp), V(sp), V(m + 0x10));
    swc1(sp + 8, fpu_add(lwc1(sp + 8), u2f(0x3ccccccdu)));  // (0.025)
    const uint32_t p = nov_287158_l1(sp, lwu(rec), lwu(rec + 4), life, size, grow);
    ee_stack_free(0x70);
    return p;
}

// orig: L10:0x002e3de8 L12:0x002e3420 L13:0x003072c0
uint32_t particle34_puff_1(uint32_t m) {
    uint32_t rec;
    switch (ovl_reloc_level()) {
    case ORXON: rec = L10_ADDR(0x00161e58); break;
    case HOVEN: rec = L12_ADDR(0x00161820); break;
    default: rec = L13_ADDR(0x00161e3c); break;
    }
    return particle34_puff(m, rec);
}

// orig: L13:0x0030c2b0
uint32_t particle34_puff_2(uint32_t m) { return particle34_puff(m, L13_ADDR(0x00161f6c)); }

}  // namespace rac1
