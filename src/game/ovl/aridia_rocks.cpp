// Aridia's own moby classes: the rocks that block a way until they are blasted (hand-ported from Aridia's overlay,
// `orig: L2:`). Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

// The rocks: state 0 gone at once when they were blasted before (the story byte 0x13d39d), else waiting (1); in state 1
// a hit of kind 0x800000 blasts them: the byte set, a sound, 200 dust particles (particle 0x16) and 50 rock shards (class
// picked from the level's three at 0x161cd0; rock_spawn), and they are gone.
// orig: L2:0x002e0720
void rocks_update_l2(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x800000), 0);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (lbu(0x0013d39d) != 0) moby_retire(m);
        else sb(m + 0x20, 1);
        return;
    }
    if (state != 1 || hit == 0) return;
    sb(0x0013d39d, 1);
    moby_sound_start(0, 0, m);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t dir = sp, pos = sp + 0x10, off = sp + 0x20, off2 = sp + 0x30;
    const float dt = lwc1(0x0015ed6c);
    // a random unit-cube direction at `out` (w 0), then an offset ahead along the facing (+-2 across, +-1 sideways, z up
    // by z_lo .. 4) at `at`, moved to the rocks' place
    auto scatter = [m](uint32_t out, uint32_t at, float z_lo) {
        V(out) = Vec4{};
        swc1(out, rand_range_f(-1.0f, 1.0f));
        swc1(out + 4, rand_range_f(-1.0f, 1.0f));
        swc1(out + 8, rand_range_f(-1.0f, 1.0f));
        V(at) = Vec4{};
        const float a = rand_range_f(-2.0f, 2.0f);
        swc1(at, fpu_mul(a, cos_vu(lwc1(m + 0x48))));
        const float b = rand_range_f(-1.0f, 1.0f);
        swc1(at + 4, fpu_mul(b, sin_vu(lwc1(m + 0x48))));
        swc1(at + 8, rand_range_f(z_lo, 4.0f));
    };
    for (int k = 0; k < 200; ++k) {  // the dust
        scatter(pos, off, 0.0f);
        lq_sq(dir, pos);
        lq_sq(pos, off);
        vec3_add(V(pos), V(pos), V(m + 0x10));
        vec3_set_length(V(dir), V(dir), fpu_mul(rand_range_f(1.5f, 3.5f), dt));
        const float size = fpu_mul(rand_range_f(1.5f, 3.5f), 210000.0f);
        const int32_t life = frame_scale_i(rand_range(0x78, 0xf0));
        particle_16_spawn(pos, dir, lwu(L2_ADDR(0x00161cc4)), lwu(L2_ADDR(0x00161cc8)), life, size);
    }
    for (int k = 0; k < 50; ++k) {  // the shards
        // (the original copies the level's three classes 0x161cd0 to its stack first and picks one there)
        scatter(off, off2, 1.0f);
        lq_sq(pos, off);
        lq_sq(off, off2);
        vec3_add(V(off), V(off), V(m + 0x10));
        vec3_set_length(V(pos), V(pos), fpu_mul(rand_range_f(1.0f, 5.0f), dt));
        const uint32_t cls = lwu(L2_ADDR(0x00161cd0) + (static_cast<uint32_t>(rand_mod(3)) << 2));
        const float scale = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3e19999au));
        const int32_t life = rand_range(0x3c, 0xb4);
        rock_spawn(off, pos, cls, life, 0, scale, 1.0f, 1.0f, 0.75f);
    }
    ee_stack_free(0xf0);
    moby_retire(m);
}

}  // namespace rac1
