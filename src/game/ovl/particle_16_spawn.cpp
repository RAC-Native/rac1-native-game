// The spawner of particle type 0x16 and the update of type 0x20 (header particle_16_spawn.h).
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>

namespace rac1 {

// One particle of type 0x16 (alloc_particle; none when the pool is full): at `pos` (+0x10) moving by `vel` (+0x20, its w
// replaced by the gravity: the frame scale *(0x15ed70) x 1.24), the start / end colours (+4 and +0x30, +0x34), `frames`
// (+0xa and +0x38), the size 0.2 x `size` (+0xc) growing to `size` over the frames (+0x3c), a random angle byte (+8), the
// sprite byte +9 = 0x74, the flags +3 = 0x44, the texture byte +2 from the level's table *(0x1b20d8).
// orig: 0x0026ced0
void particle_16_spawn(uint32_t pos, uint32_t vel, uint32_t c_start, uint32_t c_end, int32_t frames, float size) {
    const uint32_t p = alloc_particle(0x16);
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    sw(p + 4, c_start);
    const int32_t spr = float_to_int(4.0f) + 0x70;
    sb(p + 1, 0);
    const float start = size * u2f(0x3e4ccccdu);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(spr));
    swc1(p + 0xc, start);
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    const uint32_t tex = lbu(lwu(OV(0x001b20d8)));
    sh(p + 0xa, static_cast<uint32_t>(frames));
    sb(p + 2, tex);
    sh(q + 0x18, static_cast<uint32_t>(frames));
    const float n = int_to_float(frames);
    sw(q + 0x10, c_start);
    sw(q + 0x14, c_end);
    swc1(q + 0x1c, fpu_div(size - lwc1(p + 0xc), n));
    lq_sq(q, vel);
    lq_sq(p + 0x10, pos);
    swc1(q + 0xc, lwc1(0x0015ed70) * u2f(0x3f9eb852u));
}

// Type 0x20: its frames +0xa counted down (freed at the end); t = frames left x the rate +0x30: the colour the lerp of +0x2c to +0x28
// by t, the size (+0xc) 210000 x the lerp of +0x24 to +0x20 by t.
// orig: 0x00271fc0
void particle_20_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(ee_ref<int16_t>(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float t = int_to_float(lh(p + 0xa)) * lwc1(q + 0x10);
    sw(p + 4, rgba_lerp(lwu(q + 0xc), lwu(q + 8), t));
    const float a = lwc1(q + 4);
    swc1(p + 0xc, ((lwc1(q) - a) * t + a) * u2f(0x484d1400u));
}

}  // namespace rac1
