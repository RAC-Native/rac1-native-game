// Oltanis (level 14), class 309: a platform that tilts under the hero. While the hero stands on it (his ground moby
// +0x2fc, or +0x4f8 in hero state 3) its up row (+0xe0) springs toward the hero's side, at most 0x161b00 degrees off the
// vertical (+0xbc = 1); otherwise back to upright (+0xbc = 0). A creak (the class's sound +0x28) at most once a second
// while it moves. Its rotation rows are rebuilt from the up row, its attach frame (pvar +0x20) from its angles.
// Pvars: +0x20 the attach frame, +0x5c 1, +0x60 .. +0x68 the up row's spring speeds, +0x70 the creak's timer.
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;

// orig: L14:0x002de2b8 -- the up row toward the hero standing on the platform (or upright), the creak, the rows rebuilt
void class309_tilt(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 upright{0.0f, 0.0f, 1.0f, 1.0f};
    Vec4 goal{};
    const bool on = (lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0) || (lwu(HERO + 0x4f8) == m && lw(HERO + 0x208c) == 3);
    if (!on) {
        goal = upright;
        sb(m + 0xbc, 0);
    } else {
        sb(m + 0xbc, 1);
        if (abs_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10))) < 0.1f && abs_f(fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14))) < 0.1f) {
            goal = upright;  // (the hero on its centre)
        } else {
            Vec4 to{};
            vec3_sub(to, V(HERO + 0x80), V(m + 0x10));
            to.z = fpu_add(to.z, lwc1(L14_ADDR(0x00161b04)));
            const float len = vec3_length(to);
            if (len == 0.0f) {
                goal = upright;
            } else {
                float tilt = fpu_sub(u2f(0x3fc90fdbu), asin_f(fpu_div(vec3_dot(upright, to), len)));
                const float limit = fpu_mul(lwc1(L14_ADDR(0x00161b00)), u2f(0x3c8efa35u));
                if (limit < tilt) tilt = limit;
                Vec4 axis{};
                vec3_cross_ba(axis, to, upright);
                vec_rotate_axis(goal, upright, axis, tilt);
                vec3_set_length(goal, goal, 1.0f);
            }
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x70))) != 0) {
        if (0.1f < abs_f(fpu_sub(lwc1(m + 0xe0), goal.x)) || 0.1f < abs_f(fpu_sub(lwc1(m + 0xe4), goal.y)) ||
            0.1f < abs_f(fpu_sub(lwc1(m + 0xe8), goal.z))) {
            sw(pv + 0x70, static_cast<uint32_t>(frame_scale_i(60)));
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 2.0f));
            sound_start(lwu(lwu(m + 0x24) + 0x28), 8, m, m + 0x10, 0x400);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 2.0f));
        }
    }
    spring_to(m + 0xe0, pv + 0x60, goal.x, u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0xe4, pv + 0x64, goal.y, u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0xe8, pv + 0x68, goal.z, u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f);
    vec3_set_length(V(m + 0xe0), V(m + 0xe0), 1.0f);
    const Vec4 side{1.0f, 0.0f, 0.0f, 1.0f};
    vec3_cross_ba(V(m + 0xd0), side, V(m + 0xe0));
    vec3_set_length(V(m + 0xd0), V(m + 0xd0), 1.0f);
    vec3_cross_ba(V(m + 0xc0), V(m + 0xe0), V(m + 0xd0));
    swc1(m + 0xec, 0.0f);
    swc1(m + 0xcc, 0.0f);
    swc1(m + 0xdc, 0.0f);
}

}  // namespace

// orig: L14:0x002de1f8
void olt_class309_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t angles = ee_stack_alloc(0x10);  // (the angles before the set-up clears them: attach_frame_set takes them by address)
    V(angles) = V(m + 0x40);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x100u));
        sb(m + 0xbc, 0);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), u2f(0x3eb33333u)));
        sw(pv + 0x5c, 1);
        sw(pv + 0x70, 0);
        V(pv + 0x60) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        V(m + 0x40) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    }
    class309_tilt(m);  // (the original passes 0x161afc and 8.0 in f12 / f13: the helper never reads them)
    attach_frame_set(pv + 0x20, L14_ADDR(0x0015f580), angles, m + 0x40);
    ee_stack_free(0x10);
}

}  // namespace rac1
