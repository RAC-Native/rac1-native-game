// Eudora's class 617: a mover that runs along a level path as a driving moby's work goes (its pvars +0, 0 .. 1: a
// Hydrodisplacer pump's work done, shared_class280.cpp), and carries another moby with it. The driver's moby index is in
// its pvars +0, the carried moby's in +4, the path's index in +8: at work w it stands (1 - w) of the way along the path
// (between its points i and i + 1, or on point i exactly), and the carried moby's place is set to its own. It has no
// states. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data
// is L4_ADDR.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
// the level's moby of index `idx` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t idx) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
}  // namespace

// orig: L4:0x002d6f68
void eud_class617_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t from = sp, to = sp + 0x10;  // (EE frame locals: the two points' weighted parts)
    const uint32_t path = lvd::path(lw(pv + 8));  // (a count, then points of 16 bytes)
    const uint32_t driver_pv = lwu(moby_at(lw(pv)) + 0x78);
    const uint32_t carried = moby_at(lw(pv + 4));
    const float along = fpu_mul(fpu_sub(1.0f, lwc1(driver_pv)), int_to_float(lw(path) - 1));
    const float frac = float_fmod_vu(along, 1.0f);
    const int32_t point = float_to_int(along);
    const int32_t next = point + (frac == 0.0f ? 0 : 1);
    vec3_scale(V(from), V(path + 0x10 + (static_cast<uint32_t>(point) << 4)), fpu_sub(1.0f, frac));
    vec3_scale(V(to), V(path + 0x10 + (static_cast<uint32_t>(next) << 4)), frac);
    vec3_add(V(m + 0x10), V(from), V(to));
    lq_sq(carried + 0x10, m + 0x10);
    ee_stack_free(0xa0);
}

}  // namespace rac1
