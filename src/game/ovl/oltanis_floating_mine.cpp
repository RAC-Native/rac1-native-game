// Oltanis (level 14), class 643: a floating mine riding a looping path (pvar +0x60) one way or the other (+0x6c = -1 / 1)
// at 0x161c44 a second, springing toward the path's point (+0x70 the springs' speeds), tumbling (+0x80 .. +0x88 a turn
// per frame on each axis) and pulsing its colour (+0x7c the pulse's phase). It explodes when a hit of strength 2 or more
// from anything but class 0x283 reaches it, or when the hero touches it (hero +0x23c), a bigger blast for the latter.
// The set-up measures the path's segments into the points' w lanes.
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350u;
}  // namespace

// orig: L14:0x002ec810
void olt_class643_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {
        if (lw(pv + 0x60) == -1) {
            debug_printf_stub();  // (the original prints the uid: no path)
            moby_retire(m);
            return;
        }
        const uint32_t path = lvd::path(lw(pv + 0x60));
        if (lw(path) == 0) {
            debug_printf_stub();  // (a path without points)
            moby_retire(m);
            return;
        }
        for (int32_t k = 0; k < lw(path); ++k) {  // each segment's length into its first point's w lane (the path loops)
            const int32_t next = ee_rem(k + 1, lw(path));
            lvd::set_path_f32(path + 0x1cu + (static_cast<uint32_t>(k) << 4),
                 vec3_distance(V(path + 0x10u + (static_cast<uint32_t>(k) << 4)), V(path + 0x10u + (static_cast<uint32_t>(next) << 4))));
        }
        sw(pv + 0x64, 0);
        sw(pv + 0x68, 0);
        swc1(pv + 0x6c, rand_mod(2) == 0 ? -1.0f : 1.0f);
        V(m + 0x10) = V(path + 0x10);
        sb(m + 0x20, 1);
        swc1(pv + 0x80, rand_range_f(u2f(0x3d567750u), u2f(0x3dd67750u)));
        swc1(pv + 0x84, rand_range_f(u2f(0x3d0efa35u), u2f(0x3d8efa35u)));
        swc1(pv + 0x88, rand_range_f(u2f(0x3c8efa35u), u2f(0x3d0efa35u)));
        vec_clear(V(pv + 0x70));
        swc1(pv + 0x7c, rand_range_signed_f(0.0f, u2f(0x40490fdbu)));
        return;
    }
    if (st != 1) return;

    {  // along the path
        Vec4 at{};
        int32_t seg = lw(pv + 0x64);
        float frac = lwc1(pv + 0x68);
        const float step = fpu_mul(fpu_mul(lwc1(L14_ADDR(0x00161c44)), lwc1(0x0015ed6cu)), lwc1(pv + 0x6c));
        path_advance_v(lvd::path(lw(pv + 0x60)), at, &seg, &frac, 1, step);
        sw(pv + 0x64, static_cast<uint32_t>(seg));
        swc1(pv + 0x68, frac);
        spring_to(m + 0x10, pv + 0x70, at.x, u2f(0x3ca3d70au), u2f(0x3e4ccccdu), 0.0f);
        spring_to(m + 0x14, pv + 0x74, at.y, u2f(0x3ca3d70au), u2f(0x3e4ccccdu), 0.0f);
        spring_to(m + 0x18, pv + 0x78, at.z, u2f(0x3ca3d70au), u2f(0x3e4ccccdu), 0.0f);
    }
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000u), 0);
    int32_t strength = 0;
    moby_hit_react_r(m, hit, pv + 0x20, 0, strength, nullptr, nullptr, 4);
    const bool shot = !(strength < 2) && hit != 0 && lh(lwu(hit + 0x20) + 0xa6) != 0x283;
    if (shot || lwu(HERO + 0x23c) == m) {
        const uint32_t zero = ee_stack_alloc(0x10);
        vec_clear(V(zero));
        moby_sound_start(0, 0, m);
        if (lwu(HERO + 0x23c) == m) explosion_effect(m, zero, m + 0x10, 10, 20, 60, -1, 1, 1, -1, 0, 3.0f, 1.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
        else explosion_effect(m, zero, m + 0x10, 10, 20, 60, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
        ee_stack_free(0x10);
        moby_retire(m);
        return;
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x80)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x84)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x88)));
    swc1(pv + 0x7c, angle_add(lwc1(pv + 0x7c), lwc1(L14_ADDR(0x00161c48))));
    const float pulse = sin_vu(lwc1(pv + 0x7c));
    const uint32_t byte0 = static_cast<uint32_t>(float_to_int(fpu_mul(pulse, 48.0f)) + 0xcf);
    const uint32_t byte12 = static_cast<uint32_t>(float_to_int(fpu_mul(pulse, 32.0f)) + 0x30);
    sw(m + 0x90, 0xff000000u | (byte12 << 16) | (byte12 << 8) | byte0);
}

}  // namespace rac1
