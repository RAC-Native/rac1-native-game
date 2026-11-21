// Small helpers several levels carry: one port of each family of copies (the same code relinked). The spark's colour table
// is the engine's (OV 0x1b2174), the camera too (OV 0x166dc0); the held moby 0x13e030 and the frame scale 0x15ed60 are the
// base ELF's. Header shared_level_helpers.h.
#include <game/ovl/shared_level_helpers.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// The particle's words: +0x20 the moby, +0x24 0, +0x28 a random brightness (0 .. 1, also its +8 byte as 0 .. 255), +0x2c a
// random entry (0 .. 15) of the colour table (its +2 byte), +0x2e the joint, +0x30 the fade (0.015 scaled), +0x34 the pull;
// +4 grey, +3 0x48, +9 0x44, +0xc the size, +0xa 80 frames (scaled); placed at the joint pulled towards the camera.
// orig: L7:0x0029b8d8 L15:0x00262d68 L17:0x0026cf30
uint32_t chain_spark_spawn(uint32_t m, int32_t joint, float size, float pull) {
    const uint32_t p = alloc_particle(0x3d);
    if (p == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t w = p + 0x20;
    sw(w, m);
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x50)));
    sw(w + 4, 0);
    swc1(w + 8, rand_range_f(lwc1(w + 4), 1.0f));
    const int32_t r1 = libc_rand();
    swc1(w + 0x14, pull);
    sh(w + 0xc, static_cast<uint32_t>(r1 & 0xf));
    const int32_t r2 = libc_rand();
    sb(p + 8, static_cast<uint32_t>(r2));
    sw(p + 4, 0x7f7f7f);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    swc1(p + 0xc, size);
    sb(p + 2, lbu(lwu(OV(0x001b2174)) + static_cast<uint32_t>(lh(w + 0xc))));
    sh(w + 0xe, static_cast<uint32_t>(joint));
    swc1(w + 0x10, fpu_mul(lwc1(0x0015ed60), u2f(0x3c75c28fu)));  // (0.015)
    sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(w + 8), 255.0f))));
    moby_chain_point_v(lwu(w), static_cast<uint32_t>(lh(w + 0xe)), V(sp));
    vec3_sub(V(sp + 0x10), V(OV(0x00166dc0)), V(sp));
    vec3_set_length(V(sp + 0x10), V(sp + 0x10), lwc1(w + 0x14));
    vec3_add(V(p + 0x10), V(sp), V(sp + 0x10));
    ee_stack_free(0x80);
    return p;
}

// orig: L1:0x002a2480 L14:0x00294d38
void held_moby_show() {
    const uint32_t held = lwu(0x0013e030);
    if (held == 0) return;
    sh(held + 0x34, lhu(held + 0x34) & 0xfffcu);
    const uint32_t m = lwu(0x0013e030);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
}

}  // namespace rac1
