// Class 1629 of Batalia (level 8) and Oltanis (14): a shower of drops about a place. One port of both levels' copies, the
// same code relinked (compared word by word: only relocated words differ). Level-own words: the wind of class 1400's record
// (+0x20 / +0x24; per level below); the engine's (OV): the camera 0x166dc0; the base ELF's (bare): the frame time 0x15ed6c.
// The drops are class 1400's (class1400_drop, shared_class1400.cpp). Header shared_class1629.h.
#include <game/ovl/shared_class1629.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

// orig: L8:0x003085f0 L14:0x003088b0
void class1629_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam = OV(0x00166dc0);
    const uint32_t wind = on_level(OLTANIS) ? L14_ADDR(0x00162230) : L8_ADDR(0x00162360);  // (class 1400's record +0x20)
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t at = sp, centre = sp + 0x10, rel = sp + 0x20, vel = sp + 0x30;
    vec3_sub(V(at), V(cam), V(m + 0x10));
    sw(at + 8, 0);
    if (vec3_dot(V(at), V(at)) < fpu_mul(lwc1(pv + 4), lwc1(pv + 4))) {
        lq_sq(centre, m + 0x10);
        const float deg180 = 180.0f, pi = u2f(0x40490fdbu);
        const float a0 = fpu_mul(fpu_div(fpu_sub(lwc1(pv + 0x28), deg180), deg180), pi);
        const float a1 = fpu_mul(fpu_div(fpu_sub(lwc1(pv + 0x2c), deg180), deg180), pi);
        const float gust = fpu_mul(lwc1(pv + 0x24), lwc1(0x0015ed6c));
        for (int32_t i = 0; i < lw(pv + 0x14); ++i) {
            lq_sq(at, centre);
            const float a = rand_range_f(a0, a1);
            const float r = rand_range_f(lwc1(pv + 0xc), lwc1(pv + 8));
            swc1(at, fpu_add(lwc1(at), fpu_mul(cos_vu(a), r)));
            swc1(at + 4, fpu_add(lwc1(at + 4), fpu_mul(sin_vu(a), r)));
            vec3_sub(V(rel), V(at), V(cam));
            if (vec2_length(V(rel)) <= lwc1(pv + 0x10)) continue;
            swc1(vel, fpu_add(lwc1(wind), rand_range_f(-gust, gust)));
            swc1(vel + 4, fpu_add(lwc1(wind + 4), rand_range_f(-gust, gust)));
            const float fall = fpu_mul(rand_range_f(lwc1(pv + 0x18), lwc1(pv + 0x1c)), lwc1(0x0015ed6c));
            sw(vel + 0xc, 0);
            swc1(vel + 8, -fall);
            class1400_drop(at, lw(pv), vel, fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x20)));
        }
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
