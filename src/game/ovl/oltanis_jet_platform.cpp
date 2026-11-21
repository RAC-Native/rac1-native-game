// Oltanis (level 14), class 557: a flying craft of the aircraft base family (jet_update_base_l1, Kerwan's class 845)
// carrying a platform moby (pvar +0x150: its index; +0x158 = 12345 when it rides at the craft's chain +0x154, offset
// +0x160 in the craft's frame). Both are hidden while the hero is inside the box +0x21c. It hums (sound 0, its channel
// +0x15c) and spins its rotor (joint node +0x1c0, angle +0x200). While the hero rides the platform (hero state 0xd on it)
// it pulls him along with it, and its speed (+0xfc) eases from +0x214 down to 0.005; when he gets off (or drops 3 below
// the craft) it eases back to +0x214 again. Pvars: +0x74 the path (its first point the start), +0x180 the attach frame,
// +0x204 the hero on board, +0x20c / +0x210 the ease's timer and its inverse, +0x218 the speed when he got off.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;

uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }

// the ease of the speed by the timer +0x20c: from `from` toward `to` (the original's two forms)
void ease_tick(uint32_t pv) { timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x20c))); }
float ease_t(uint32_t pv) { return fpu_mul(int_to_float(lh(pv + 0x20c)), lwc1(pv + 0x210)); }

}  // namespace

// orig: L14:0x002e7bd8
void olt_class557_update_l14(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (lw(pv + 0x21c) >= 0 && point_in_box(HERO + 0x80, lw(pv + 0x21c)) != 0) {  // hidden, with its platform
        sw(m + 0x94, 0);
        sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x41u));
        if (lw(pv + 0x150) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0x150));
            sw(o + 0x94, 0);
            sh(o + 0x34, static_cast<uint16_t>((lhu(o + 0x34) | 0x41u) & 0xefffu));
        }
        return;
    }
    if (lw(m + 0x94) == 0) {  // shown again, with its platform
        sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xffbeu));
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (lw(pv + 0x150) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0x150));
            const uint16_t f = static_cast<uint16_t>(lhu(o + 0x34) & 0xffbeu);
            sh(o + 0x34, f);
            sh(o + 0x34, static_cast<uint16_t>(f | 0x1000u));
            sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10));
        }
    }
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t old_pos = frame, old_ang = frame + 0x10, moved = frame + 0x20;  // (attach_frame_set takes them by address)
    V(old_pos) = V(m + 0x10);
    V(old_ang) = V(m + 0x40);
    if (lbu(m + 0x20) == 0) {
        V(m + 0x10) = V(lvd::path(lw(pv + 0x74)) + 0x10);
        sw(pv + 0x200, 0);
        sw(pv + 0x15c, 0xffffffffu);
        sh(pv + 0x206, 0);
        sub_0024f590(m, 0, pv + 0x1c0);
        if (lw(pv + 0x150) == -1) {
            debug_printf_stub();  // (the original prints the uid: a craft with no platform)
            moby_retire(m);
            ee_stack_free(0x40);
            return;
        }
        sh(pv + 0x204, 0);
        swc1(pv + 0x214, lwc1(pv + 0xfc));
    }
    jet_update_base_l1(m);
    const uint32_t deck = level_moby(lw(pv + 0x150));
    if (lw(pv + 0x158) == 0x3039) {  // the platform held at the chain point (or the craft's place) plus its offset
        Vec4 at{};
        if (lw(pv + 0x154) != -1) moby_chain_point_v(m, lwu(pv + 0x154), at);
        else at = V(m + 0x10);
        Mat4 rot{};
        euler_to_mat4_vu(rot, V(m + 0x40));
        Vec4 off{};
        mat4_transform(off, V(pv + 0x160), rot);
        vec3_add(V(deck + 0x10), at, off);
    }
    {
        const int32_t ch = lw(pv + 0x15c);
        if (ch == -1 || (sub_0028d8c0(m, ch) == 0 && lw(pv + 0x15c) == -1)) sw(pv + 0x15c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
    swc1(pv + 0x200, angle_add(lwc1(pv + 0x200), u2f(0x3e32b8c2u)));
    quat_from_axis_angle(V(pv + 0x1d0), 0, lwc1(pv + 0x200));
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x180, moved, old_ang, m + 0x40);

    bool slow = false;  // the speed eased toward 0.005 (the hero on board) or back toward +0x214
    if (lw(HERO + 0x208c) == 0xd && lwu(HERO + 0x964) == deck) {
        if (lh(pv + 0x204) == 0) {
            sh(pv + 0x204, 1);
            const int32_t life = frame_scale_i(lw(L14_ADDR(0x00161b8c)));
            sh(pv + 0x20c, static_cast<uint16_t>(life));
            swc1(pv + 0x210, fpu_div(1.0f, int_to_float(static_cast<int16_t>(life))));
        }
        slow = true;
    } else if (lh(pv + 0x204) == 1) {
        if (lh(HERO + 0x30e) == 0 || lwc1(HERO + 0x88) <= fpu_sub(lwc1(m + 0x18), 3.0f)) {  // he got off
            sh(pv + 0x204, 0);
            const int32_t life = frame_scale_i(lw(L14_ADDR(0x00161b90)));
            sh(pv + 0x20c, static_cast<uint16_t>(life));
            swc1(pv + 0x218, lwc1(pv + 0xfc));
            swc1(pv + 0x210, fpu_div(1.0f, int_to_float(static_cast<int16_t>(life))));
        } else {  // in the air above it: pulled toward the platform, at most 0x161b88 a second
            Vec4 pull{};
            vec3_sub(pull, V(deck + 0x10), V(HERO + 0x80));
            pull.z = 0.0f;
            float len = vec3_length(pull);
            const float most = fpu_mul(lwc1(L14_ADDR(0x00161b88)), lwc1(0x0015ed6cu));
            if (most < len) len = most;
            vec3_set_length(V(HERO + 0xf0), pull, len);
        }
        slow = lh(pv + 0x204) != 0;
    } else {
        slow = lh(pv + 0x204) != 0;  // (a value the game never sets: the original takes the slow ease for any non-zero)
    }
    ease_tick(pv);
    if (slow) {
        swc1(pv + 0xfc, fpu_add(fpu_mul(fpu_sub(lwc1(pv + 0x214), u2f(0x3ba3d70au)), ease_t(pv)), u2f(0x3ba3d70au)));
    } else {
        const float from = lwc1(pv + 0x214);
        const float t = ease_t(pv);
        swc1(pv + 0xfc, fpu_add(from, fpu_mul(fpu_sub(lwc1(pv + 0x218), from), t)));
    }
    ee_stack_free(0x40);
}

}  // namespace rac1
