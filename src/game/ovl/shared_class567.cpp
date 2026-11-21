// Class 567, a falling fire bomb: one port of the copies five levels carry (Novalis, Batalia, Gaspar, Quartu, Drek's fleet).
// The copies are the same code relinked (compared with the relocated fields masked); their one level-own callee is each
// level's copy of Veldin's particle 4 spawn (0x269958, the overlay maps pair them), here the Veldin port. What they name
// is the engine's (OV: the collision's result 0x173e40..) or the base ELF's (bare: the hero 0x13f350, the frame scale's
// square 0x15ed70). Header shared_class567.h.
//
// The bomb's pvars: its velocity +0, its thrower +0x10, its life +0x14, its height above the floor +0x18.
#include <game/ovl/shared_class567.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// State 1 falls: it moves by its velocity, faces along it and trails two puffs (a bright one, additive, and a dark
// one); kept its height above the floor below it (its fall stopped), or stopped at the hero's height (+0x2a8) plus that
// height, else it falls faster. Its path tested against the world and the mobys (its thrower ignored): on a hit it
// hurts the moby it hit (damage 1, kinds 0x10001, from the bomb) and bursts (state 2), as it does when its life runs out.
// State 2 bursts (a burst of size 13, scale 0.25) and leaves.
// orig: L1:0x002eda00 L8:0x002efb60 L9:0x002f6678 L15:0x002d4748 L17:0x002d2dc0
void class567_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 2) {
        explosion_burst(m, m + 0x10, -1, 0.25f, 13.0f);
        moby_retire(m);
        return;
    }
    if (st != 1) return;
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t was = sp, trail = sp + 0x10, rec = sp + 0x20;
    const uint32_t pos = m + 0x10;
    lq_sq(was, pos);
    vec3_add(V(pos), V(pos), V(pv));
    swc1(m + 0x48, atan2_f(lwc1(pv), lwc1(pv + 4)));
    vec_clear(V(trail));
    {
        const int32_t life = frame_scale_i(rand_range(0xf, 0x16));
        spawn_particle_4(pos, trail, 0x6f00afffu, 0xff, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(rand_range(0x14, 0x23)), 1);
    }
    {
        const int32_t life = frame_scale_i(rand_range(0x1e, 0x3c));
        spawn_particle_4(pos, trail, 0x1fffffffu, 0x4f4f4fu, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(rand_range(0x32, 0x4b)), 0);
    }
    const float floor = floor_probe_v(0.5f, V(pos), 0);
    const float z = lwc1(m + 0x18);
    const float above = lwc1(pv + 0x18);
    const float over_floor = fpu_sub(z, floor);
    if (over_floor < above) {
        swc1(m + 0x18, fpu_add(z, fpu_sub(above, over_floor)));
        sw(pv + 8, 0);
    } else if (z < fpu_add(lwc1(0x0013f5f8), above)) {
        sw(pv + 8, 0);
    } else {
        swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(lwc1(0x0015ed70), u2f(0x412ccccdu))));  // (10.8)
    }
    if (segment_collide_v(V(pos), V(was), 0, lwu(pv + 0x10), nullptr) != 0) {
        if (lwu(OV(0x00173e58)) != 0) {
            sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, pv);
            vec2_set_length(V(rec), V(rec), 1.0f);
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sb(rec + 0x19, 1);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, 0);
            moby_hit_record(lwu(OV(0x00173e58)), rec);
        }
        sb(m + 0x20, 2);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) sb(m + 0x20, 2);
    ee_stack_free(0xc0);
}

}  // namespace rac1
