// Class 156: one port of the copies six levels carry (the same code relinked). Each level keeps its own words (the shot's
// tunings: per level below); the rest is the engine's (OV: the camera 0x166dc0, the collision's results 0x173e40 (+0x18
// the moby hit, +0x20 the hit point), the particles' texture words 0x1b20b8). Header shared_class156.h.
//
// The pvars: +0 the timer, +0xc the spin's rate, +0x10 a flag cleared at the end of its flight, +0x18 the moby it ignores,
// +0x20 the velocity.
#include <game/ovl/shared_class156.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
// the level's tunings: +0 the trail's spin, +8 the shot's radius
uint32_t tunings() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x00161494);
    case BLARG: return L6_ADDR(0x00161464);
    case BATALIA: return L8_ADDR(0x0016148c);
    case GASPAR: return L9_ADDR(0x001614f4);
    case QUARTU: return L15_ADDR(0x001615f4);
    default: return L17_ADDR(0x0016154c);
    }
}
// a direction: about the angle `pitch` (either way by up to pi in each angle), `len` long
void scattered(uint32_t out, float len, float pitch) {
    const float pi = u2f(0x40490fdbu);
    const float a = angle_add(rand_range_signed_f(0.0f, pi), pitch);
    const float b = angle_add(rand_range_signed_f(0.0f, pi), pitch);
    vec_from_spherical_v(V(out), len, b, a);
}
}  // namespace

// orig: L1:0x00280970 L6:0x0027a850 L8:0x00275bd8 L9:0x0028a428 L15:0x0025b050 L17:0x002653f8
uint32_t class156_trail(uint32_t spin, uint32_t m) {
    const uint32_t p = alloc_particle(0xe);
    if (p == 0) return 0;
    lq_sq(p + 0x10, m + 0x10);
    const float jitter = u2f(0x3cf5c28fu);  // (0.03)
    swc1(p + 0x10, fpu_add(lwc1(p + 0x10), rand_range_signed_f(0.0f, jitter)));
    swc1(p + 0x14, fpu_add(lwc1(p + 0x14), rand_range_signed_f(0.0f, jitter)));
    swc1(p + 0x18, fpu_add(lwc1(p + 0x18), rand_range_signed_f(0.0f, jitter)));
    sw(p + 4, 0xffffffffu);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    swc1(p + 0xc, 29400.0f);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x60));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    sb(p + 2, lbu(lwu(OV(0x001b20b8))));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(6)));
    sw(p + 0x28, m);
    sw(p + 0x24, 1);
    if (spin == 0) {
        sw(p + 0x20, 0);
        return p;
    }
    sw(p + 0x20, rand_mod(2) == 0 ? 0u - spin : spin);
    return p;
}

// orig: L1:0x002c72c8 L6:0x002be868 L8:0x002ba668 L9:0x002d0b20 L15:0x002aa950 L17:0x002af3a0
void class156_burst(uint32_t) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t dir = sp, at = sp + 0x10;
    const uint32_t hit = OV(0x00173e60);
    // about the angle from the hit point up to the camera
    vec3_sub(V(dir), V(OV(0x00166dc0)), V(hit));
    const float pitch = -atan2_f(vec2_length(V(dir)), lwc1(dir + 8));
    for (int32_t k = 0; k < 20; ++k) {  // (sparks: 1 .. 3 times 0.2 out, flying on)
        scattered(dir, u2f(0x3e4ccccdu), pitch);  // (0.2)
        vec3_scale(V(at), V(dir), rand_range_f(1.0f, 3.0f));
        vec3_add(V(at), V(at), V(hit));
        spawn_particle_13_v(V(at), V(dir));
    }
    const float spread = u2f(0x3dcccccdu);  // (0.1)
    for (int32_t k = 0; k < 10; ++k) {  // (smoke about the hit point, 10 .. 20 frames)
        swc1(at, fpu_add(lwc1(hit), rand_range_signed_f(0.0f, spread)));
        swc1(at + 4, fpu_add(lwc1(hit + 4), rand_range_signed_f(0.0f, spread)));
        swc1(at + 8, fpu_add(lwc1(hit + 8), rand_range_signed_f(0.0f, spread)));
        scattered(dir, u2f(0x3d4ccccdu), pitch);  // (0.05)
        const int32_t lo = frame_scale_i(0xa);
        const int32_t life = rand_range(lo, frame_scale_i(0x14));
        spawn_particle_15_v(V(at), V(dir), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(life), 1, 20000.0f);
    }
    ee_stack_free(0xa0);
}

// It faces the camera, spins and flies on; in flight (1) it trails its particle and tests its sphere against the mobys
// (but the one it ignores): on one it hits it and bursts; when its timer runs out it fades (2) and goes.
// orig: L1:0x002c7530 L6:0x002bead0 L8:0x002ba8d0 L9:0x002d0d88 L15:0x002aabb8 L17:0x002af608
void class156_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, vel = pv + 0x20, cam = OV(0x00166dc0);
    swc1(m + 0x44, -atan2_f(vec2_distance(V(cam), V(pos)), fpu_sub(lwc1(cam + 8), lwc1(m + 0x18))));
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(cam), lwc1(pos)), fpu_sub(lwc1(cam + 4), lwc1(pos + 4))));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0xc)));
    vec3_add(V(pos), V(pos), V(vel));
    const uint32_t state = lbu(m + 0x20);
    const uint32_t t = tunings();
    if (state == 1) {
        class156_trail(lwu(t), m);
        const uint32_t sp = ee_stack_alloc(0xb0);
        const uint32_t rec = sp, dir = sp + 0x30;
        vec3_set_length(V(dir), V(vel), 1.0f);
        sub_00259888(0.0f, rec, static_cast<int32_t>(m), 0, dir);
        bool burst = false;
        if (mobys_in_sphere(pos, lwc1(t + 8), 0x10, lwu(pv + 0x18), rec) != 0 && zero_stub() == 0) {
            const uint32_t hit = lwu(OV(0x00173e58));
            if (hit != 0) sub_00259bc8(hit, m, 0x10001, OV(0x00173e60), dir, 1.0f);
            burst = true;
        }
        ee_stack_free(0xb0);
        if (burst) {
            class156_burst(m);
            moby_retire(m);
            return;
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv))) == 0) return;
        sw(pv, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sb(m + 0x20, 2);
        if (lwu(pv + 0x10) != 0) sw(pv + 0x10, 0);
    } else if (state == 2) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv))) != 0) {
            moby_retire(m);
            return;
        }
        const float full = static_cast<float>(frame_scale_i(0x1e));
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(static_cast<float>(lw(pv)), fpu_div(127.0f, full)))));
    }
}

}  // namespace rac1
