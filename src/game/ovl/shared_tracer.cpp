// The tracer rounds of Batalia (level 8) and Gaspar (9), class 1292: a round that flies on by its velocity with a trail of
// sparks (when the screen's two fade levels 0x15f5d0 / 0x15f5d4 are under 0.9), damages the moby it hits, and bursts when it
// hits, when its shooter is gone or when its life is over. The two levels' copies are the same code, relinked: no level
// word; the fade levels and the hit's moby and point 0x173e58 / 0x173e60 go through OV(). Header shared_tracer.h.
#include <game/ovl/shared_tracer.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// pvars: the velocity +0, the shooter +0x10 (not hit), the moby it belongs to +0x14, its life +0x18, a big burst +0x2c (1)
// orig: L8:0x00307298 L9:0x00308f08
void tracer_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 1: {
        const Vec4 from = V(m + 0x10);
        const uint32_t owner = lwu(pv + 0x14);
        if (owner == 0 || lbu(owner + 0x20) == 0xfe || lbu(owner + 0x20) == 0xfd) sb(m + 0x20, 2);
        if (lwc1(OV(0x0015f5d4)) < 0.9f && lwc1(OV(0x0015f5d0)) < 0.9f) {  // (the trail: a spark behind it)
            const uint32_t sp = ee_stack_alloc(0x40);
            const uint32_t vel = sp + 0x10, at = sp + 0x20;
            vec3_scale(V(vel), V(pv), rand_range_f(0.95f, u2f(0x3f7c28f6u)));
            vec3_set_length(V(at), V(pv), -0.1f);
            vec3_add_inplace(V(at), V(m + 0x10));
            const int32_t life = frame_scale_i(rand_range(0xf, 0x14));
            const int32_t fade = frame_scale_i(rand_range(7, 0xa));
            spawn_particle_4(at, vel, 0x6f00afffu, 0xff, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(fade), 1);
            ee_stack_free(0x40);
        }
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        if (segment_collide_v(V(m + 0x10), from, 0x10, lwu(pv + 0x10), nullptr) != 0) {  // (from where it is back to where it was)
            const uint32_t hit = lwu(OV(0x00173e58));
            if (hit != lwu(pv + 0x10)) {
                if (hit != 0) sub_00259bc8(hit, m, 0x10001, OV(0x00173e60), pv, 1.0f);
                sb(m + 0x20, 2);
            }
        }
        int32_t life = lw(pv + 0x18);
        const int32_t ticked = timer_tick_i32(life);
        sw(pv + 0x18, static_cast<uint32_t>(life));
        if (ticked != 0) sb(m + 0x20, 2);
        break;
    }
    case 2: {
        const bool dark = lwc1(OV(0x0015f5d4)) < 0.85f && lwc1(OV(0x0015f5d0)) < 0.85f;
        explosion_burst(m, m + 0x10, -1, 0.25f, dark || lw(pv + 0x2c) == 1 ? 13.0f : 0.0f);
        moby_retire(m);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
