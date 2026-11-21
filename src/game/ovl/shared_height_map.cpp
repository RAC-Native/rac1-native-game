// The level's height map and the ground's dust puffs: Veldin's 0x263468 / 0x2634f8 / 0x268f10 and the copies Batalia (L8),
// Hoven (L12) and Oltanis (L14) carry (the same code relinked; the overlay maps pair them). What they name is the
// engine's: the height map 0x15fc98.. (its width +0, its bytes +0x10, the heights of byte 255 / byte 0 +8 / +0xc), the
// camera 0x166dc0, the collision's hit point 0x173e60, the particle textures' table 0x1b2084: OV. Header
// shared_height_map.h.
#include <game/ovl/shared_height_map.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// The byte at the point truncated to the grid, scaled between the two heights.
// orig: L0:0x00263468 L8:0x0026d3e0 L12:0x0027bb20 L14:0x0026b008
float ground_height_at(const Vec4& p) {
    const int32_t row = float_to_int(p.y);
    const int32_t col = float_to_int(p.x);
    const uint32_t at = lwu(OV(0x0015fca8)) + lwu(OV(0x0015fc98)) * static_cast<uint32_t>(row) + static_cast<uint32_t>(col);
    const float low = lwc1(OV(0x0015fca0));
    const float shade = fpu_div(int_to_float(static_cast<int32_t>(lbu(at))), 255.0f);
    return fpu_add(fpu_mul(fpu_sub(lwc1(OV(0x0015fca4)), low), fpu_sub(1.0f, shade)), low);
}

// orig: L0:0x00268f10 L8:0x00273080 L12:0x002817e8 L14:0x00270a78
void ground_puff_spawn(uint32_t pos, uint32_t alpha) {
    const uint32_t p = alloc_particle(1);
    if (p == 0) return;
    swc1(pos + 8, fpu_add(lwc1(pos + 8), 0.1f));
    lq_sq(p + 0x10, pos);
    sw(p + 4, alpha << 24 | 0x907070u);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    swc1(p + 0xc, u2f(0x46f61801u));
    sb(p + 2, lbu(lwu(OV(0x001b2084))));
    sb(p + 8, 0xa0);
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x1b)));
    sw(p + 0x20, alpha);
}

// The ground's height there plus a step (a 256th of the map's range); below the camera's height + 5, a segment from it
// down two steps is tested against the world (mask 0x12) and the puff goes where it hits, its alpha 64 + 191 * the
// hit's flat distance from the camera / 60 (a byte).
// orig: L0:0x002634f8 L8:0x0026d470 L12:0x0027bbb0 L14:0x0026b098
void ground_puff_at(uint32_t pos) {
    const float step = fpu_mul(fpu_sub(lwc1(OV(0x0015fca4)), lwc1(OV(0x0015fca0))), u2f(0x3b800000u));
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t top = sp, bottom = sp + 0x10, off = sp + 0x20;
    lq_sq(top, pos);
    const float h = fpu_add(ground_height_at(V(top)), step);
    const bool below = h < fpu_add(lwc1(OV(0x00166dc8)), 5.0f);
    swc1(top + 8, h);
    if (below) {
        lq_sq(bottom, top);
        swc1(bottom + 8, fpu_sub(lwc1(bottom + 8), fpu_add(step, step)));
        if (segment_collide_v(V(top), V(bottom), 0x12, 0, nullptr) != 0) {
            const uint32_t hit = OV(0x00173e60);
            vec3_sub(V(off), V(hit), V(OV(0x00166dc0)));
            const float shade = fpu_add(fpu_mul(fpu_div(vec2_length(V(off)), 60.0f), 191.0f), 64.0f);
            ground_puff_spawn(hit, static_cast<uint32_t>(float_to_int(shade)) & 0xffu);
        }
    }
    ee_stack_free(0x80);
}

}  // namespace rac1
