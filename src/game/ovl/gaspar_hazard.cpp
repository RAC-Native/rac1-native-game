// Gaspar's own moby class 1206, a line of heat that hurts what stands in it — hand-ported from Gaspar's own overlay. Header
// gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR.
//
// The pvars: the spheres' radius +0, the line's rise +4 (over the spheres), their count +8, the hit's damage +0xc and kind +0x10,
// its hum +0x14.
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// Class 1206. Its hum kept on (checked every eighth frame, by its slot). Its line: `count` spheres of the radius, the first a
// radius out along its turned z (each next one step further: the rise less two radii, over the count, along the turn), each
// hitting the mobys inside it (its hit record: the damage and kind of its pvars; the original's debug note on a hit left out).
// orig: L9:0x00305a28
void gas_class1206_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sw(pv + 0x14, 0xffffffffu);
    }
    if ((lwu(L9_ADDR(0x0015f5cc)) & 7) == ((m >> 8) & 7)) {
        const int32_t slot = lw(pv + 0x14);
        if (slot == -1 || (sub_0028d8c0(m, slot) == 0 && lw(pv + 0x14) == -1))
            sw(pv + 0x14, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t step = sp, at = sp + 0x10, first = sp + 0x20, rec = sp + 0x30;
    const float n = int_to_float(lw(pv + 8));
    const float r = lwc1(pv);
    V(step) = Vec4{};
    swc1(step + 8, fpu_div(fpu_sub(lwc1(pv + 4), fpu_add(r, r)), n));
    lq_sq(at, m + 0x10);
    mat3_transform(V(step), V(step), M(m + 0xc0));
    vec3_set_length(V(first), V(step), lwc1(pv));
    vec3_add(V(at), V(at), V(first));
    hit_rec_init(rec, m, lwu(pv + 0x10), lwc1(pv + 0xc));
    for (int32_t i = 0; i < lw(pv + 8); ++i) {
        mobys_in_sphere(at, lwc1(pv), 1, m, rec);
        vec3_add(V(at), V(at), V(step));
    }
    ee_stack_free(0x80);
}

}  // namespace rac1
