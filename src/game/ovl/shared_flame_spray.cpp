// The flame spray of Aridia's (level 2) and Kalebo III's (16) flamethrowers: flames (particle 0xc) sprayed from a point
// along a direction as far as the walls let them, a few of them kept in the spray's record to burn the mobys they touch,
// and now and then a puff of smoke (particle 0x19). The two levels' copies are the same code, relinked: the spray's tuning
// words are the level's (the same address on both); the frame counter 0x15f5cc and the hit point 0x173e60 go through OV(), the
// frame time 0x15ed6c and its square 0x15ed70 are the base ELF's. Header shared_flame_spray.h.
#include <game/ovl/shared_flame_spray.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

namespace {
// The spray's tuning (level data): the flames' speed +0, the flames a frame (+4 near, 1 within 4 of a wall) and +8 more, the
// flames' scatter at the far end +0xc and at the near end +0x10.
uint32_t spray_tuning() { return on_level(KALEBO) ? L16_ADDR(0x001600d0) : L2_ADDR(0x001600d0); }

constexpr uint32_t kFlame = 0xc;  // (the flames' particle type)
}  // namespace

// orig: L2:0x00264e40 L16:0x0025ea88
void flame_spray_clear(uint32_t rec) {
    for (uint32_t i = 0; i < 7; ++i) sw(rec + i * 4, 0);
    sh(rec + 0x1c, 0);
}

// orig: L2:0x002651d0 L16:0x0025ee18
void flame_spray_burn(uint32_t rec, uint32_t ignore, uint32_t hit) {
    for (uint32_t i = 0; i < 7; ++i) {
        const uint32_t slot = rec + i * 4;
        const uint32_t p = lwu(slot);
        if (p == 0) continue;
        if (lbu(p) != kFlame || static_cast<int8_t>(lbu(p + 1)) < 0) {  // (the particle is something else now, or done)
            sw(slot, 0);
            continue;
        }
        mobys_in_sphere(p + 0x10, fpu_mul(fpu_div(lwc1(p + 0xc), 210000.0f), 0.25f), 0, ignore, hit);
        if (lh(p + 0xa) < 5) sw(slot, 0);  // (burnt out)
    }
}

// orig: L2:0x00264e70 L16:0x0025eab8
void flame_spray(uint32_t rec, uint32_t pos, uint32_t dir, float len) {
    const uint32_t tune = spray_tuning();
    // (the frame's slots as the original lays them out: seek_1d's place and speed +0x50 / +0x54 are the frame's leftovers)
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t from = sp, to = sp + 0x10, reach_vec = sp + 0x20, at = sp + 0x30, vel = sp + 0x40;
    const bool keep_one = (lwu(OV(0x0015f5cc)) & 3u) == 0;
    V(from) = V(pos);
    vec3_set_length(V(reach_vec), V(dir), fpu_add(len, 0.75f));
    vec3_add(V(to), V(reach_vec), V(from));
    float reach = len;
    if (segment_collide_v(V(from), V(to), 2, 0, nullptr) != 0) {  // (a wall: as far as the hit, 0.75 short)
        reach = fpu_sub(vec3_distance(V(from), V(OV(0x00173e60))), 0.75f);
        if (reach < 0.0f) reach = 0.0f;
    }
    seek_1d(sp + 0x50, sp + 0x54, reach, 1.0f, fpu_mul(lwc1(0x0015ed70), 48.0f), fpu_mul(lwc1(tune), lwc1(0x0015ed6c)));
    vec3_set_length(V(reach_vec), V(reach_vec), lwc1(sp + 0x54));
    const int32_t near = reach < 4.0f ? 1 : lw(tune + 4);
    const int32_t count = lw(tune + 8) + near;
    if (count > 0) {
        const float part = fpu_div(reach, len);
        for (int32_t i = 0; i < count; ++i) {
            const float wide = lwc1(tune + 0x10);
            const float scatter = fpu_mul(fpu_add(fpu_mul(fpu_sub(lwc1(tune + 0xc), wide), part), wide), lwc1(0x0015ed6c));
            rand_sphere_point(vel, 0.0f, scatter);
            vec3_add(V(vel), V(vel), V(reach_vec));
            vec3_scale(V(at), V(reach_vec), rand_range_f(0.0f, 1.0f));
            vec3_add(V(at), V(at), V(from));
            const uint32_t flame = spawn_particle_0c(at, vel, i < near ? 1u : 0u, reach);
            if (i == 2 && keep_one) {  // (the third flame of every fourth frame burns: into the first empty slot)
                for (uint32_t k = 0; k < 7; ++k) {
                    if (lwu(rec + k * 4) == 0) {
                        sw(rec + k * 4, flame);
                        break;
                    }
                }
            }
        }
    }
    int16_t smoke = static_cast<int16_t>(lh(rec + 0x1c));
    const int32_t ticked = timer_tick_i16(smoke);
    sh(rec + 0x1c, static_cast<uint32_t>(smoke));
    if (ticked != 0) {
        sh(rec + 0x1c, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(10.0f, 30.0f)))));
        rand_sphere_point(vel, 0.0f, fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c)));
        vec3_scale(V(at), V(reach_vec), rand_range_f(0.5f, 0.1f));  // (the original's bounds, high below low)
        vec3_add(V(at), V(at), V(vel));
        swc1(at + 0xc, 0.0f);
        spawn_particle_19(pos, at, 0);
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
