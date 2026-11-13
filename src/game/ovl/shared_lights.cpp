// The flickering lights of Novalis (level 1) and Blarg (6), class 1504: a light (one of the level's light slots, Veldin
// 0x1802c0, 32 bytes each) whose colour wanders from one of its colours to another at random intervals, while the camera is
// near (and in its box), faded out over the last quarter of its range. The two levels' copies are the same code, relinked:
// no level word; the camera 0x166dc0, the moby list 0x15ffd8 and the light slots go through OV(). Header shared_lights.h.
#include <game/ovl/shared_lights.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// the light's slot, when it has one
void light_release(uint32_t pv) {
    const int32_t slot = lw(pv + 0x68);
    if (slot == -1) return;
    region_clear(static_cast<uint32_t>(slot));
    sw(pv + 0x68, 0xffffffffu);
}
}  // namespace

// pvars: its colours +0 (rows of 16 bytes, given 0..255, scaled to 0..1 at the start), the colour now +0x40 (+0x4c: the
// light's strength), the change's frames +0x50 .. +0x54, the colours' count +0x58, its range +0x5c, its radius +0x60, its slot
// +0x68, the change's timer +0x6c / its length +0x70, the colours it goes from +0x74 / to +0x78, the way it eases +0x7c, the
// camera's box +0x80 (-1: none), the moby it dies with +0x84 (-1: none).
// orig: L1:0x0030b618 L6:0x00308868
void flicker_light_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        const int32_t colours = lw(pv + 0x58);
        if (colours < 2) {
            moby_retire(m);
            return;
        }
        const float scale = u2f(0x3b800000u);  // (1 / 256)
        for (int32_t i = 0; i < lw(pv + 0x58); ++i) {
            const uint32_t row = pv + static_cast<uint32_t>(i) * 0x10;
            for (uint32_t k = 0; k < 0x10; k += 4) swc1(row + k, fpu_mul(lwc1(row + k), scale));
        }
        sw(pv + 0x68, 0xffffffffu);
        if (static_cast<int32_t>(lbu(m + 0x30)) < float_to_int(lwc1(pv + 0x5c)) + 4)  // (drawn out to its range)
            sb(m + 0x30, static_cast<uint32_t>(float_to_int(lwc1(pv + 0x5c)) + 4));
        sb(m + 0x20, 1);
        return;
    }
    case 1:
        break;
    default:
        return;
    }
    const uint32_t camera = OV(0x00166dc0);
    const float dist = vec2_distance(V(m + 0x10), V(camera));
    bool on = dist < lwc1(pv + 0x5c);
    if (on && lw(pv + 0x80) != -1 && point_in_box(camera, lw(pv + 0x80)) == 0) on = false;
    const int32_t owner = lw(pv + 0x84);
    const uint32_t list = lwu(OV(0x0015ffd8));
    if (owner != -1 && static_cast<int8_t>(lbu(list + (static_cast<uint32_t>(owner) << 8) + 0x20)) < 0) {  // (its moby is dead)
        if (lw(pv + 0x68) != -1) region_clear(lwu(pv + 0x68));
        moby_retire(m);
        return;
    }
    if (!on) {
        light_release(pv);
        return;
    }
    int32_t timer = lw(pv + 0x6c);
    const int32_t ticked = timer_tick_i32(timer);
    sw(pv + 0x6c, static_cast<uint32_t>(timer));
    if (ticked != 0) {  // (the next change: from the colour it reached to another one)
        const int32_t frames = rand_range(lw(pv + 0x50), lw(pv + 0x54));
        sw(pv + 0x74, lwu(pv + 0x78));
        sw(pv + 0x6c, static_cast<uint32_t>(frames));
        sw(pv + 0x70, static_cast<uint32_t>(frames));
        const int32_t step = rand_range(1, lw(pv + 0x58) - 1);
        const int32_t next = ee_rem(lw(pv + 0x78) + step, lw(pv + 0x58));
        sw(pv + 0x7c, lw(pv + 0x7c) == 0 ? 1u : 0u);
        sw(pv + 0x78, static_cast<uint32_t>(next));
    }
    const float done = fpu_mul(fpu_div(int_to_float(lw(pv + 0x70) - lw(pv + 0x6c)), int_to_float(lw(pv + 0x70))), u2f(0x40490fdbu));
    float angle = fpu_mul(done, 0.5f);
    if (lw(pv + 0x7c) != 0) angle = fpu_add(angle, u2f(0x3fc90fdbu));
    const float t = sin_vu(angle);
    // the ease: sin from 0 to 1 (0x7c clear: from +0x74 to +0x78) or from 1 down to 0 (set: from +0x78 back up to +0x74)
    const bool down = lw(pv + 0x7c) != 0;
    const Vec4 a = V(pv + (lwu(pv + (down ? 0x78u : 0x74u)) << 4));
    const Vec4 b = V(pv + (lwu(pv + (down ? 0x74u : 0x78u)) << 4));
    vec4_lerp_v(V(pv + 0x40), a, b, t);
    const float range = lwc1(pv + 0x5c);
    const float fade_from = fpu_mul(range, 0.75f);
    if (fade_from < dist) {
        const Vec4 colour = V(pv + 0x40);
        vec4_lerp_v(V(pv + 0x40), colour, Vec4{}, fpu_div(fpu_sub(dist, fade_from), fpu_sub(range, fade_from)));
    }
    const int32_t slot = lw(pv + 0x68);
    if (slot == -1) {
        sw(pv + 0x68, static_cast<uint32_t>(light_add(m + 0x10, lwc1(pv + 0x60), lwc1(pv + 0x4c), lwc1(pv + 0x40), lwc1(pv + 0x44), lwc1(pv + 0x48))));
        return;
    }
    const uint32_t entry = OV(0x001802c0) + (static_cast<uint32_t>(slot) << 5);
    V(entry + 0x10) = V(m + 0x10);
    swc1(entry + 0x1c, lwc1(pv + 0x60));
    V(entry) = V(pv + 0x40);
}

}  // namespace rac1
