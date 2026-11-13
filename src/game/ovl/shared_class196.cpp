// The update of classes 196 / 197: one port of the copies Quartu and Drek's fleet carry (the same code relinked; each
// level's jump table of the states is its own, switched on here by the state). It reads the engine's moby list (OV 0x15ffd8)
// and the base ELF's hero (0x13f3d0) and frame time (0x15ed6c). Header shared_class196.h.
//
// The pvars: +0 the home place, +0x10 the moby it waits for (its index; -1 none), +0x14 the speed, +0x18 / +0x1c the boxes
// that send it out / back (-1 none).
#include <game/ovl/shared_class196.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// the most speed: three times the moby's scale over its class's size (+0x24 of the class's header)
float top_speed(uint32_t m) { return fpu_div(fpu_mul(lwc1(m + 0x2c), 3.0f), lwc1(lwu(m + 0x24) + 0x24)); }

// one step along the facing at the speed from home (pvar +0) plus the step: the moby's place
void slide(uint32_t m, uint32_t pv, uint32_t sp) {
    swc1(sp, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x14)));
    sw(sp + 8, 0);
    swc1(sp + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x14)));
    vec3_add(V(sp + 0x10), V(pv), V(sp));
    lq_sq(m + 0x10, sp + 0x10);
}
}  // namespace

// States: 0 takes its place as home (then 1 when it waits for a moby, else 4), 1 waits for that moby's state 4, 4 waits for
// the hero in the first box, 2 speeds up (backwards for classes 196 / 1958, which also play a sound on each start) until it
// is past its top speed (then 3, or 5 with no moby to wait for), 5 waits for the hero to leave the second box, 6 slows back
// to a stop (then 4); 3 rests. Gone when it waits for no moby and lacks a box.
// orig: L15:0x002bddb0 L17:0x002c0f00
void class196_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t cls = lh(m + 0xa6);
    const bool reverse = cls == 0xc4 || cls == 0x7a6;  // (classes 196 and 1958)
    const int32_t link = lw(pv + 0x10);
    if (link == -1 && (lw(pv + 0x18) == -1 || lw(pv + 0x1c) == -1)) {
        moby_retire(m);
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x60);
    switch (lbu(m + 0x20)) {  // (the level's jump table of seven states)
    case 0:  // (L15:0x002bde48)
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, lw(pv + 0x10) != 0 ? 1u : 4u);
        break;
    case 1:  // (L15:0x002bde64)
        if (lbu(lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(link) << 8) + 0x20) != 4) break;
        if (reverse) moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        break;
    case 2: {  // (L15:0x002bdea0)
        float step = fpu_mul(fpu_div(top_speed(m), 3.0f), lwc1(0x0015ed6c));
        if (reverse) step = -step;
        swc1(pv + 0x14, fpu_add(lwc1(pv + 0x14), step));
        slide(m, pv, sp);
        if (top_speed(m) < abs_f(lwc1(pv + 0x14))) sb(m + 0x20, lw(pv + 0x10) != -1 ? 3u : 5u);
        break;
    }
    case 4:  // (L15:0x002bdf6c)
        sw(pv + 0x14, 0);
        if (point_in_box(0x0013f3d0, lw(pv + 0x18)) == 0) break;
        if (reverse) moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        break;
    case 5:  // (L15:0x002bdfa4)
        if (point_in_box(0x0013f3d0, lw(pv + 0x1c)) != 0) break;
        if (reverse) moby_sound_start(0, 0, m);
        sb(m + 0x20, 6);
        break;
    case 6: {  // (L15:0x002bdfd8)
        const float brake = fpu_mul(fpu_div(top_speed(m), 3.0f), lwc1(0x0015ed6c));
        if (brake < abs_f(lwc1(pv + 0x14))) swc1(pv + 0x14, fpu_sub(lwc1(pv + 0x14), reverse ? -brake : brake));
        else sw(pv + 0x14, 0);
        slide(m, pv, sp);
        if (lwc1(pv + 0x14) == 0.0f) sb(m + 0x20, 4);
        break;
    }
    default:  // (3 and past the table: nothing)
        break;
    }
    ee_stack_free(0x60);
}

}  // namespace rac1
