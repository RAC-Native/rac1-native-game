// Batalia (level 8): the sea below the fort, hand-ported: class 327, its wave table, its draw and Ratchet's splash when he
// falls into it. Addresses are level 8's (`orig: L8:`); its own data and code are L8_ADDR, the base ELF's are bare (the
// hero 0x13f350: his position +0x80, his z +0x88, his z speed +0x108, his sound slot +0x22b2; his moby 0x1413d0; the level
// number 0x15ed84; the frame time 0x15ed6c).
#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
}  // namespace

// Class 327, the sea. No pvars, no states. The first one to run builds the sea's wave table once (the level's flag
// 0x161970; the sea's record 0x1d7060, the waves 2/3 of the level's table: ocean_wave_table_set_80); every frame it
// queues the sea's draw (0x2da0d0, the ocean draw several levels share) and, on Batalia (level 8) only, watches for
// Ratchet crossing the surface (the record's +8, his z now below it, a frame ago above it): then his sound in his slot is
// stopped, a ring (spawn_moby_307, scale 3, its fade byte 0x70) put out where he went in, 16 drops thrown up (particle
// 0x23) and 16 ripples (particle 0x2e, 30 to 60 frames) laid on the water around him.
// orig: L8:0x002da0f0
void bat_class327_update_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    if (lw(L8_ADDR(0x00161970)) == 0) {
        sw(L8_ADDR(0x00161970), 1);
        ocean_wave_table_set_80(L8_ADDR(0x001d7060), u2f(0x3f2aaaabu));  // (k = 2/3)
    }
    sub_001f93e0(L8_ADDR(0x002da0d0), m);
    if (lw(0x0015ed84) != 8) {
        ee_stack_free(0xb0);
        return;
    }
    const float hero_z = lwc1(0x0013f3d8), sea = lwc1(L8_ADDR(0x001d7068));
    if (!(hero_z < sea) || !(sea <= fpu_sub(hero_z, lwc1(0x0013f458)))) {
        ee_stack_free(0xb0);
        return;
    }

    // Ratchet went in: his sound stopped while the slot is still his
    if (sub_0028d8c0(lwu(0x001413d0), static_cast<int16_t>(lhu(0x00141602))) != 0) {
        sub_0028d918(static_cast<int16_t>(lhu(0x00141602)));
        sh(0x00141602, 0xffffu);
    }
    // the splash's place: his, on the surface
    lq_sq(sp, 0x0013f3d0);
    swc1(sp + 0x8, lwc1(L8_ADDR(0x001d7068)));
    const uint32_t ring = spawn_moby_307(sp, 3.0f);
    if (ring != 0) sb(ring + 0x23, 0x70);

    // 16 drops: out up to 3 a frame in any direction, 3 to 6.5 up
    const uint32_t v = sp + 0x10;
    for (int32_t k = 0; k < 16; ++k) {
        const float a = rand_angle();
        const float out = rand_range_f(fpu_mul(dt(), 0.0f), fpu_mul(dt(), 3.0f));
        swc1(v, fpu_mul(cos_vu(a), out));
        swc1(v + 0x4, fpu_mul(sin_vu(a), out));
        swc1(v + 0x8, rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.5f)));
        const int32_t life = rand_range(0x5a, 0x78);
        spawn_particle_23(sp, v, rand_mod(2), static_cast<uint32_t>(life));
    }
    // 16 ripples within 1 of the splash, just over the surface (the first one's last argument 2, the others' -2)
    for (int32_t k = 0; k < 16; ++k) {
        rand_sphere_point(v, 1.0f, 1.0f);
        vec3_add(V(v), V(v), V(sp));
        swc1(v + 0x8, fpu_add(lwc1(L8_ADDR(0x001d7068)), 0.05f));
        const float size = rand_range_f(0.7f, 1.0f);
        const uint32_t p = spawn_particle_2e(v, L8_ADDR(0x0015f580), L8_ADDR(0x001d7068), size, k == 0 ? 2.0f : -2.0f);
        if (p != 0) sh(p + 0xa, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(30.0f, 60.0f)))));
    }
    ee_stack_free(0xb0);
}

}  // namespace rac1
