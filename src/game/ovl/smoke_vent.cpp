// Veldin's class 760 (update 0x2d6f28): a smoke vent over a level box, its puffs (kind 0x17
// particles) and the wisps of its effect slots (tables 0x1cb7e0..).
#include <cmath>

#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

// Takes n slots of the wisp tables (count 0x161390): the first one's index.
// orig: 0x002a4dc8
int32_t wisp_slots_alloc(int32_t n) {
    const int32_t base = lw(OV(0x00161390));
    sw(OV(0x00161390), static_cast<uint32_t>(base + n));
    return base;
}

// A kind 0x17 particle (a smoke puff) at `pos` jittered by up to `jitter`, drifting along
// `vel`, colour `color` (+4 with 0x20000000), size `size` growing by lo..hi, spin `spin`
// (one way or the other, by the frame rate); 10 frames. Returns it (0 when none was free).
// orig: 0x0026d000
uint32_t spawn_particle_17_v(const Vec4& pos, int32_t spin, const Vec4& vel, uint32_t color, float jitter, float lo, float hi, float size) {
    const uint32_t p = alloc_particle(0x17);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    sw(p + 4, color | 0x20000000u);
    const int32_t s = float_to_int(4.0f);
    sb(p + 9, static_cast<uint32_t>(s + 0x70));
    sb(p + 3, 0x48);
    swc1(p + 0xc, size);
    sb(p + 1, 0);
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20dc))));
    swc1(p + 0x10, lwc1(p + 0x10) + rand_range_signed_f(0.0f, jitter));
    swc1(p + 0x14, lwc1(p + 0x14) + rand_range_signed_f(0.0f, jitter));
    swc1(p + 0x18, lwc1(p + 0x18) + rand_range_signed_f(0.0f, jitter));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(10)));
    const uint32_t q = p + 0x20;
    sw(q + 4, 0);
    sw(p + 0x20, color);
    float r;
    if (rand_mod(2) == 0) {
        r = static_cast<float>(-spin) * lwc1(0x0015ed60);
    } else {
        r = lwc1(OV(0x00166c00u - 0x7ea0)) * static_cast<float>(spin);
    }
    sh(q + 8, static_cast<uint32_t>(float_to_int(r)));
    swc1(q + 0xc, rand_range_f(lo, hi));
    V(q + 0x10) = vel;
    return p;
}
// orig: 0x0026d000 (EE-address form)
uint32_t spawn_particle_17(uint32_t pos, int32_t spin, uint32_t vel, uint32_t color, float jitter, float lo, float hi, float size) {
    return spawn_particle_17_v(V(pos), spin, V(vel), color, jitter, lo, hi, size);
}

namespace {
// a wisp's random direction (each axis -1..1) in 0x1cb7e0[i]
// orig: (helper) repeated in 0x002d7bf0 / 0x002d7d58
void wisp_dir(int32_t i) {
    const uint32_t d = OV(0x001cb7e0) + static_cast<uint32_t>(i) * 0x10;
    swc1(d, rand_range_signed_f(0.0f, 1.0f));
    swc1(d + 4, rand_range_signed_f(0.0f, 1.0f));
    swc1(d + 8, rand_range_signed_f(0.0f, 1.0f));
}
}  // namespace

// A vent's wisp i (rising kind): its direction, speeds from the vent's pvar ranges (+0x10..+0x14
// 0x1d02e0, +0x18..+0x1c 0x1d15a0, +8..+0xc 0x1d4480), its timer 0x1d5740 (0x161380), phase
// 0x1d6a00 staggered by the slot (0x16137c apart), flag 0x1d2d10 0.
// orig: 0x002d7bf0
void wisp_init_rising(uint32_t m, int32_t i) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t k4 = static_cast<uint32_t>(i) * 4, k2 = static_cast<uint32_t>(i) * 2;
    wisp_dir(i);
    swc1(OV(0x001d02e0) + k4, rand_range_f(lwc1(pv + 0x10), lwc1(pv + 0x14)));
    swc1(OV(0x001d15a0) + k4, rand_range_f(lwc1(pv + 0x18), lwc1(pv + 0x1c)));
    swc1(OV(0x001d31c0) + k4, 0.0f);
    swc1(OV(0x001d4480) + k4, rand_range_f(lwc1(pv + 8), lwc1(pv + 0xc)));
    sh(OV(0x001d5740) + k2, lhu(OV(0x00161380)));
    sh(OV(0x001d60a0) + k2, 0);
    sb(OV(0x001d2d10) + static_cast<uint32_t>(i), 0);
    sh(OV(0x001d6a00) + k2, static_cast<uint32_t>((i - lh(pv) + 1) * lw(OV(0x0016137c))));
}

// A vent's wisp i (hovering kind): its direction, speeds around the vent's +0x44 (+2) and +0x4c
// (+4), drift 0.003..0.005, flag 0x1d2d10 1.
// orig: 0x002d7d58
void wisp_init_hover(uint32_t m, int32_t i) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t k4 = static_cast<uint32_t>(i) * 4;
    wisp_dir(i);
    const float a = lwc1(pv + 0x44), b = lwc1(pv + 0x4c);
    swc1(OV(0x001d02e0) + k4, rand_range_f(a, a + 2.0f));
    swc1(OV(0x001d15a0) + k4, rand_range_f(b, b + 4.0f));
    swc1(OV(0x001d31c0) + k4, 0.0f);
    swc1(OV(0x001d4480) + k4, rand_range_f(u2f(0x3b449ba6u), u2f(0x3ba3d70au)));
    sb(OV(0x001d2d10) + static_cast<uint32_t>(i), 1);
}

// Class 760, a smoke vent over level box +4 (pvars, as shorts: +2 rising wisps, +0x40 hovering
// ones, +0x42 puffs on): set up once (its box's sizes, wisp slots +0 from 0x2a4dc8, the
// colour +0x38 from the bytes +0x3c..+0x3e; no box: removed); then while its box top is on
// screen, near the camera (128) it puffs *(gp-0x5244) smoke particles a frame from random
// points of the box's base, and its wisps are drawn (0x2d73e8).
// orig: 0x002d6f28
void smoke_vent_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        if (lw(pv + 4) == -1) {
            moby_retire(m);
            return;
        }
        swc1(pv + 0x48, vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 4)) + 0x10)));
        const float b = vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 4))));
        if (lwc1(pv + 0x48) < b) swc1(pv + 0x48, b);
        swc1(pv + 0x44, lh(pv + 0x40) != 0 ? lwc1(pv + 0x48) : 3.0f);
        swc1(pv + 0x50, vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 4)) + 0x20)) * 0.5f);
        const int32_t base = static_cast<int16_t>(wisp_slots_alloc(lh(pv + 2) + lh(pv + 0x40)));
        sh(pv, static_cast<uint32_t>(base));
        const int32_t end = base + lh(pv + 2);
        for (int32_t i = base; i < end; ++i) {
            const int32_t r = rand_mod(2);
            swc1(OV(0x001cb7ec) + static_cast<uint32_t>(i) * 0x10, 1.0f);
            sb(OV(0x001d2860) + static_cast<uint32_t>(i), static_cast<uint32_t>(r));
            wisp_init_rising(m, i);
            sh(OV(0x001d5740) + static_cast<uint32_t>(i) * 2, 0);
        }
        const int32_t end2 = end + lh(pv + 0x40);
        for (int32_t i = end; i < end2; ++i) {
            const int32_t r = rand_mod(2);
            swc1(OV(0x001cb7ec) + static_cast<uint32_t>(i) * 0x10, 1.0f);
            sb(OV(0x001d2860) + static_cast<uint32_t>(i), static_cast<uint32_t>(r));
            wisp_init_hover(m, i);
        }
        swc1(lvd::box(lvd::BOXES, lw(pv + 4)) + 0x3c, 1.0f);
        sw(pv + 0x38, (lbu(pv + 0x3e) << 16) | (lbu(pv + 0x3d) << 8) | lbu(pv + 0x3c));
        return;
    }
    if (st != 1) return;
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 4));
    Vec4 top = V(box + 0x30);
    top.z = top.z + 9.0f;
    top.w = 10.0f;
    if (point_on_screen_v(top, lwc1(OV(0x00166c00u - 0x5220))) < 0) return;
    if (lh(pv + 0x42) != 0 && vec2_distance(V(OV(0x00166dc0)), V(m + 0x10)) < 128.0f) {
        for (int32_t i = 0; i < lw(OV(0x00166c00u - 0x5244)); ++i) {
            Vec4 v;
            v.x = rand_range_signed_f(0.0f, 1.0f);
            v.y = rand_range_signed_f(0.0f, 1.0f);
            v.z = 1.0f;
            v.w = 1.0f;
            Vec4 pos;
            mat4_transform(pos, v, M(box));
            pos.z = pos.z + 0.5f;
            v.x = rand_range_signed_f(0.0f, lwc1(0x0015ed6c) * 0.25f);
            v.y = rand_range_signed_f(0.0f, lwc1(0x0015ed6c) * 0.25f);
            v.z = lwc1(pv + 0x20) * lwc1(0x0015ed6c);
            const float size = rand_range_f(lwc1(pv + 0x24), lwc1(pv + 0x28));
            const uint32_t color = (static_cast<uint32_t>(rand_range(lh(pv + 0x2c), lh(pv + 0x2e))) << 24) | lwu(OV(0x00166c00u - 0x5238));
            const int32_t spin = rand_range(lw(OV(0x00166c00u - 0x5240)), lw(OV(0x00166c00u - 0x523c)));
            const uint32_t p = spawn_particle_17_v(pos, spin, v, color, lwc1(OV(0x00166c00u - 0x524c)), 1.0f, lwc1(OV(0x00166c00u - 0x5248)), size);
            if (p == 0) continue;
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(rand_range(lw(pv + 0x30), lw(pv + 0x34)))));
            sw(p + 0x24, 3);
            sb(p + 0x2a, color >> 24);
            sb(p + 0x2b, lbu(p + 0xa));
            if (rand_mod(2) != 0) sb(p + 3, 0x44);
        }
    }
    draw_callback_add(OVF(0x002d73e8), m);
}

}  // namespace rac1
