// Class 934 of Quartu (level 15) and Drek's fleet (17): a spinning shot. One port of both levels' copies, the same code
// relinked: its addresses are the engine's (OV: the moby the last collision met 0x173e58, the frame counter 0x15f5cc) or the
// base ELF's (bare: the frame time 0x15ed6c); the trailing spark is the shared chain_spark_spawn (the levels' copies
// L15:0x262d68 / L17:0x26cf30). Header shared_class934.h.
//
// The pvars: +0 the velocity, +0x38 the frames it flies (a timer, i32), +0x3c the moby it does not burst on (its shooter).
// The moby's +0x40 its spin. State 0 flying; any other state nothing.
#include <game/ovl/shared_class934.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_level_helpers.h>

namespace rac1 {

namespace {
constexpr int32_t BUMPER_CLASS = 0x4e;  // (78: met, it takes no contact push)
}

// orig: L15:0x002e4380 L17:0x002e1c20
void class934_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {
        moby_retire(m);
        return;
    }
    if (lbu(m + 0x20) != 0) return;
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t next = sp, rec = sp + 0x10;
    const uint32_t pos = m + 0x10;
    // a turn a second
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu))));
    vec3_add(V(next), V(pos), V(pv));
    sub_00259888(2.0f, rec, static_cast<int32_t>(m), 1, pv);
    sh(rec + 0x1a, lhu(m + 0xa6));
    uint32_t record[9];
    for (uint32_t k = 0; k < 9; ++k) record[k] = lwu(rec + 4 * k);
    const bool met = segment_collide_v(V(pos), V(next), 0, m, record) != 0 || sphere_collide(pos, 0.3f, 0, m) != 0 ||
                     timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x38))) != 0;
    const uint32_t hit = lwu(OV(0x00173e58));
    if (met && hit != 0 && hit != lwu(pv + 0x3c)) {
        if (lh(hit + 0xa6) != BUMPER_CLASS) moby_sphere_contact(m, pos, 1, 0, 1, 0, 0.5f, 2.0f, 0.0f);
        explosion_burst(m, pos, -1, 0.5f, 13.0f);
        moby_sound_start(0, 0, m);
        moby_retire(m);
        ee_stack_free(0xa0);
        return;
    }
    lq_sq(pos, next);
    if (ee_rem(lw(OV(0x0015f5cc)), 3) == 0) chain_spark_spawn(m, 0, 30000.0f, 0.1f);
    ee_stack_free(0xa0);
}

}  // namespace rac1
