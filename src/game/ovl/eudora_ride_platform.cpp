// Eudora's class 584: a platform moved by others (it does not move itself) that records each frame's move in its ride
// frame (pvars +0x60) so that what stands on it moves with it; its place of the frame before at +0xa0. Set up (state 0),
// its hit record cleared (health +0x20 0, +0x24 0, +0x28 4), +0x3e 0xd, the hero's colour copied; then state 1. Header
// eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`).
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// orig: L4:0x002d3580
void eud_class584_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, moved = sp;  // (the move: an EE frame local)
    vec3_sub(V(moved), V(pos), V(pv + 0xa0));
    attach_frame_set(pv + 0x60, moved, m + 0x40, m + 0x40);
    lq_sq(pv + 0xa0, pos);
    if (lbu(m + 0x20) == 0) {
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 0xd);
        moby_copy_hero_color(m);
        sb(m + 0x20, 1);
    }
    ee_stack_free(0x60);
}

}  // namespace rac1
