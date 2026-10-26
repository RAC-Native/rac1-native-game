// Ported overlay code 0x00200000..0x0020ffff.
#include <game/enhance/advanced.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l3.h>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace rac1 {

// orig: 0x00203a28 — clear bit 31 of 148 words at 0x14196c (stride 8).
void sub_00203a28() {
    uint32_t p = 0x0014196c;
    int32_t n = 0x93;
    do {
        sw(p, lwu(p) & 0x7fffffffu);
        --n;
        p += 8;
    } while (n >= 0);
}

// orig: 0x002050b8
uint32_t sub_002050b8(uint32_t v) { return v; }

// orig: 0x00205110
uint32_t sub_00205110(int32_t kind) {
    if (kind == 0 || kind == 2 || kind == 3 || kind == 4) return lwu(g::HERO_MOBY);
    if (kind == 1) return lwu(0x001404d4);
    if (kind == 5) return lwu(0x001403e0);
    return 0;
}

// orig: 0x0020a1e0
void fx_params_set_0(float a, float b) { swc1(g::FX_PARAMS + 0x0a8, b); swc1(g::FX_PARAMS + 0x0a4, a); }
// orig: 0x0020a1f8
void fx_params_set_1(float a, float b) { swc1(g::FX_PARAMS + 0x158, b); swc1(g::FX_PARAMS + 0x154, a); }
// orig: 0x0020a210
void fx_params_set_2(float a, float b) { swc1(g::FX_PARAMS + 0x208, b); swc1(g::FX_PARAMS + 0x204, a); }
// orig: 0x0020a228
void fx_params_set_3(float a, float b) { swc1(g::FX_PARAMS + 0x2b8, b); swc1(g::FX_PARAMS + 0x2b4, a); }

// orig: 0x0020a240 — wall ahead of the hero: segment along the hero's local x at local height z,
// from just inside its radius (PLAYER+0x234 - 0.02) out to `reach`, against flag 2. On a hit
// *dist (when given) gets the xy distance from the hero to the contact; returns 1 on a hit.
int32_t hero_wall_probe(float* dist, float z, float reach) {
    Vec4 a{0.0f, 0.0f, 0.0f, 0.0f}, b{0.0f, 0.0f, 0.0f, 0.0f};
    sub_002335c8_v(a, lwc1(g::PLAYER + 0x234) - u2f(0x3ca3d70au), 0.0f, z);
    sub_002335c8_v(b, reach, 0.0f, z);
    if (segment_collide_v(a, b, 2, 0, nullptr) == 0) return 0;
    if (dist != nullptr) *dist = vec2_distance(V(g::PLAYER + 0x80), V(OV(0x00173e60)));
    return 1;
}
// orig: 0x0020a240 (EE-address form; dist = 0: none)
int32_t hero_wall_probe(uint32_t dist, float z, float reach) {
    return hero_wall_probe(dist != 0 ? static_cast<float*>(ee_ram_ptr(dist)) : nullptr, z, reach);
}

// Hero shadow: off (hero moby flag 0x400 clear) in substates 0x12/0x15/0x16 and state 0x7d.
// Otherwise the light pitch PLAYER+0xd20 eases (by at most 0.052 a frame) to 0.97 (1.5 with
// +0x30e or in substate 4); the light direction (yaw of the moby's palette colour 1, that
// pitch) goes to 0x1aeb90. Unless the hero is low (+0x2d8 < 1, and +0x2f0 / +0x2f4 too: then
// the range is cleared), four probes (flags 0x22) from the moby's bounding sphere offset by the
// table 0x17c300 (scaled by its radius, turned to the light yaw) run along the light down to
// 0.5 below the height; the receiving range hero+0x84 / +0x88 spans the hits (-0.12 / +0.24,
// at most 3 tall).
// orig: 0x00208b60
void hero_shadow_update() {
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    const int32_t sub = lw(P + 0x208c);
    if (sub == 0x12 || sub == 0x15 || lw(P + 0x2084) == 0x7d || sub == 0x16) {
        const uint32_t hm = lwu(P + 0x2080);
        sh(hm + 0x34, lhu(hm + 0x34) & 0xfbffu);
        return;
    }
    const uint32_t hm = lwu(P + 0x2080);
    sh(hm + 0x34, lhu(hm + 0x34) | 0x400u);
    sb(lwu(P + 0x2080) + 0xbd, 1);
    float target = u2f(0x3f7851ecu);
    if (lh(P + 0x30e) != 0 || lw(P + 0x208c) == 4) target = 1.5f;
    float d = angle_sub(target, lwc1(P + 0xd20));
    const float step = u2f(0x3d54fdf4u);
    if (step < d) {
        d = step;
    } else if (d < -step) {
        d = -step;
    }
    swc1(P + 0xd20, angle_add(lwc1(P + 0xd20), d));
    Vec4 light;
    moby_palette_colour_1_v(lwu(P + 0x2080), light);
    const float a = atan2_f(light.x, light.y);
    const float ca = cos_vu(a);
    const float sa = sin_vu(a);
    const float cp = cos_vu(lwc1(P + 0xd20));
    light.y = sa * cp;
    light.x = ca * cp;
    light.z = -sin_vu(lwc1(P + 0xd20));
    V(OV(0x001aeb90)) = light;
    float h = lwc1(P + 0x2d8);
    if (h < 1.0f) {
        h = lwc1(P + 0x2f0);
        if (!(1.0f <= h)) {
            h = lwc1(P + 0x2f4);
            if (!(1.0f < h)) {
                sw(lwu(P + 0x2080) + 0x84, 0);
                sw(lwu(P + 0x2080) + 0x88, 0);
                return;
            }
        }
    }
    float lo = h, hi = h;
    const float inv = u2f(0x3a800000u);
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t m = lwu(P + 0x2080);
        const float r = lwc1(m + 0xc) * inv;
        const uint32_t t = OV(0x0017c300) + 0x10 * i;
        Vec4 a0;
        vec3_scale(a0, V(m), inv);
        a0.z = a0.z + lwc1(t + 8) * r;
        const float len = fpu_div(a0.z - h + 0.5f, -light.z);
        a0.x = a0.x + ca * lwc1(t) * r;
        a0.y = a0.y + sa * lwc1(t + 4) * r;
        Vec4 b;
        vec3_set_length(b, light, len);
        vec3_add(b, a0, b);
        if (segment_collide_v(a0, b, 0x22, 0, nullptr) != 0) {
            lo = min_f(lo, lwc1(OUTB + 0x28));
            hi = max_f(hi, lwc1(OUTB + 0x28));
        }
    }
    const uint32_t m = lwu(P + 0x2080);
    swc1(m + 0x84, lo - u2f(0x3df5c28fu));
    swc1(m + 0x88, hi + u2f(0x3e75c28fu));
    const float cap = lwc1(m + 0x84) + 3.0f;
    if (cap < lwc1(m + 0x88)) swc1(m + 0x88, cap);
}

// Footstep timing: in state 2 nothing before frame 15 of PLAYER+0x198 and a left step at frame
// 22; walking (hero moby +0x53 == 3) steps on the animation crossing 49.5 (left) and 17 (right),
// running (4) on 12.5 and 1 — unless PLAYER+0xa9c. Sound on, level_sound_start's a = +0x12ed.
// orig: 0x00206080
void hero_footsteps_update() {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0x2084) == 2 && lw(P + 0x198) < frame_scale_i(0xf)) return;
    const int32_t a = static_cast<int32_t>(lbu(P + 0x12ed));
    if (lw(P + 0x2084) == 2 && lw(P + 0x198) == frame_scale_i(0x16)) hero_footstep(a, 0, 1);
    const uint32_t k = lbu(lwu(P + 0x2080) + 0x53);
    float left, right;
    if (k == 3) {
        left = u2f(0x42460000u);
        right = 17.0f;
    } else if (k == 4) {
        left = 12.5f;
        right = 1.0f;
    } else {
        return;
    }
    if (lw(P + 0xa9c) != 0) return;
    if (sub_00211870(left) != 0) hero_footstep(a, 0, 1);
    if (sub_00211870(right) != 0) hero_footstep(a, 1, 1);
}

// Ledge grab check from pos (facing PLAYER+0x98, compared with *ref_yaw). Off with +0x308 == 2.
// Four steps ahead (radius +0x234, then 0.25 x step each) look down from top + 0.15 to `high`
// for a flat (< 20 degrees) surface that is not of type 9 / 12 (flag 4, ignoring the hero moby);
// it must lie no higher than top above pos and at least `min` above the ground +0x2d8. Five
// level probes (flag 2) towards it from below its edge (drop .. 0) find the wall, whose outward
// yaw must be within 65 degrees of *ref_yaw; walking 0.07 steps along the wall normal (up to
// 2.4) must then leave the ledge top within 30 degrees of *ref_yaw. Returns 1 on a ledge. Sizes
// (top, high, step, drop, min): 1.5, 1.15, 0.3, -0.7, 1.8; with +0x20a4 == 1: 0.725, 0.7, 0.25,
// -0.4, 0.7.
// orig: 0x0020c758
int32_t hero_ledge_check_v(const Vec4& pos, const float* ref_yaw) {
    if (on_batalia()) return bat_ledge_check_call(pos, ref_yaw);  // (level 8's own: L8:0x0021e4c8)
    if (on_pokitaru()) return pok_ledge_check_call(pos, ref_yaw);  // (level 11's own: L11:0x0023d240)
    if (on_hoven()) return hov_ledge_check_call(pos, ref_yaw);  // (level 12's own: L12:0x0022e870)
    if (on_oltanis()) return olt_ledge_check_call(pos, ref_yaw);  // (level 14's own: L14:0x0021e770)
    if (on_quartu()) return qua_ledge_check_call(pos, ref_yaw);  // (level 15's own: L15:0x00205070)
    if (on_kalebo()) return kal_ledge_check_call(pos, ref_yaw);  // (level 16's own: L16:0x0020a2d8)
    if (on_veldin_finale()) return vf_ledge_check_call(pos, ref_yaw);  // (level 18's own: L18:0x00214700)
    if (on_l1_family() || on_rilgar()) return hero_ledge_check_v_l1(pos, ref_yaw);
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lh(P + 0x308) == 2) return 0;
    float drop = u2f(0xbf333333u), step = u2f(0x3e99999au), top = 1.5f, high = u2f(0x3f933333u), min = u2f(0x3fe66666u);
    if (lbu(P + 0x20a4) == 1) {
        min = u2f(0x3f333333u);
        drop = u2f(0xbecccccdu);
        step = 0.25f;
        high = min;
        top = u2f(0x3f39999au);
    }
    Vec4 p = pos;
    p.x = p.x + cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    p.y = p.y + sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    bool found = false;
    Vec4 a, b;
    for (int32_t i = 0; i < 4 && !found; ++i) {
        p.x = p.x + cos_vu(lwc1(P + 0x98)) * step * 0.25f;
        p.y = p.y + sin_vu(lwc1(P + 0x98)) * step * 0.25f;
        a = p;
        a.z = a.z + (top + u2f(0x3e19999au));
        b = p;
        b.z = b.z + high;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) continue;
        const int32_t t = sub_001f35d8();
        if (t == 9 || t == 0xc) continue;
        if (atan2_f(lwc1(OUTB + 0x48), vec2_length(V(OUTB + 0x40))) < u2f(0x3eb2b8c2u)) found = true;
    }
    if (!found) return 0;
    if (pos.z + top < lwc1(OUTB + 0x28)) return 0;
    const Vec4 c = V(OUTB + 0x20);
    const float dir = atan2_f(c.x - pos.x, c.y - pos.y);
    if (c.z - lwc1(P + 0x2d8) < min) return 0;
    bool wall = false;
    float wall_yaw = 0.0f;
    for (int32_t j = 0; j < 5; ++j) {
        a = pos;
        b = c;
        const float r = lwc1(P + 0x234) + lwc1(P + 0x234);
        b.x = b.x + cos_vu(dir) * r;
        b.y = b.y + sin_vu(dir) * r;
        const float z = b.z + drop * (1.0f - fpu_div(static_cast<float>(j), 5.0f));
        b.z = z;
        a.z = z;
        if (segment_collide_v(a, b, 2, 0, nullptr) != 0) {
            wall = true;
            wall_yaw = atan2_f(lwc1(OUTB + 0x40), lwc1(OUTB + 0x44));
            break;
        }
    }
    if (!wall) return 0;
    if (!(angle_dist(*ref_yaw, angle_add(wall_yaw, u2f(0x40490fdbu))) < u2f(0x3f91361eu))) return 0;
    for (float d = 0.0f;;) {
        a = c;
        a.x = a.x + cos_vu(wall_yaw) * d;
        a.y = a.y + sin_vu(wall_yaw) * d;
        b = a;
        a.z = a.z + 0.5f;
        b.z = b.z - 0.5f;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) break;
        d = d + u2f(0x3d8f5c29u);
        if (!(d < u2f(0x4019999au))) return 0;
    }
    const float d = angle_dist(angle_add(wall_yaw, u2f(0x40490fdbu)), *ref_yaw);
    return u2f(0x3f060a92u) < d ? 0 : 1;
}
// orig: 0x0020c758 (EE-address form)
int32_t hero_ledge_check(uint32_t pos, const float* ref_yaw) { return hero_ledge_check_v(V(pos), ref_yaw); }

// Ledge grab while jumping / falling (substate 4 rising slower than 3.5 x frame scale or with
// +0x41e, substate 2, states 8 / 0x1a / 0x1b — not state 0xe below 37 on +0xaa8; with +0x20a4
// == 1: 2.5, substate 2, states 0x4d..0x4f). Off with +0x308 == 2 or +0x1b0 (+0x4e8 = 0
// either way). As hero_ledge_check from the position 2 frames ahead (gravity +0x940, +0x4a0 in
// substate 4), but the top must be of a typed surface other than 9 / 12, a moby top must be
// grabbable (pvars word 0 +0x1e bit 0, or word 2 +0x3c bit 0), and the way from the hero to
// the edge (0.05 above it) must be clear. +0x4f8 = the top's moby, bit 0 of +0x4f4 = a 0.45 x
// 0.6 capsule on the edge collides. A grab (wall within 65 degrees of the facing, walking off
// the top along the wall normal in 0.03 steps within 2.4) sets the hang point +0x4d0 (where the
// walk left the top, 0.015 back, at the edge height; then 0.45 along the normal and -1.43 down; -0.71 / 0.32 with +0x20a4 == 1),
// the edge height +0x4e0, the wall yaw +0x4e4 and +0x4e8 = 1.
// orig: 0x0020bed0
void hero_ledge_grab_check() {
    if (on_batalia()) { bat_call(bat_hero_ledge_grab_check_u); return; }  // (level 8's own: L8:0x0021dd20)
    if (on_pokitaru()) { pok_call(pok_hero_ledge_grab_check_u); return; }  // (level 11's own: L11:0x0023ca98)
    if (on_hoven()) { hov_call(hov_hero_ledge_grab_check_u); return; }  // (level 12's own: L12:0x0022e0c8)
    if (on_oltanis()) { olt_call(olt_hero_ledge_grab_check_u); return; }  // (level 14's own: L14:0x0021dfc8)
    if (on_quartu()) { qua_call(qua_hero_ledge_grab_check_u); return; }  // (level 15's own: L15:0x002048c8)
    if (on_kalebo()) { kal_call(kal_hero_ledge_grab_check_u); return; }  // (level 16's own: L16:0x00209b30)
    if (on_veldin_finale()) { vf_call(vf_hero_ledge_grab_check_u); return; }  // (level 18's own: L18:0x00213f58)
    if (on_l1_family() || on_rilgar()) return hero_ledge_grab_check_l1();
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lh(P + 0x308) == 2) return;
    sw(P + 0x4e8, 0);
    if (lh(P + 0x1b0) != 0) return;
    bool ok = false;
    const float fs = lwc1(0x0015ed6c);
    if (lbu(P + 0x20a4) == 0) {
        const int32_t sub = lw(P + 0x208c);
        if (sub == 4 && (lh(P + 0x41e) != 0 || lwc1(P + 0xe8) < fs * 3.5f)) {
            ok = true;
        } else if (sub == 2) {
            ok = true;
        } else {
            const int32_t st = lw(P + 0x2084);
            if (st == 8 || st == 0x1a || st == 0x1b) ok = true;
        }
        if (lw(P + 0x2084) == 0xe && lwc1(P + 0xaa8) < 37.0f) ok = false;
    }
    if (lbu(P + 0x20a4) == 1) {
        const int32_t sub = lw(P + 0x208c);
        if (sub == 4 && (lh(P + 0x41e) != 0 || lwc1(P + 0xe8) < lwc1(0x0015ed6c) * 2.5f)) {
            ok = true;
        } else if (sub == 2) {
            ok = true;
        } else {
            const int32_t st = lw(P + 0x2084);
            if (st == 0x4f || st == 0x4d || st == 0x4e) ok = true;
        }
    }
    if (!ok) return;
    float drop = u2f(0xbf333333u), step = u2f(0x3e99999au), top = 1.5f, high = u2f(0x3f933333u), hang = u2f(0xbfb70a3du), min = u2f(0x3fe66666u), back = u2f(0x3ee66666u);
    if (lbu(P + 0x20a4) == 1) {
        high = u2f(0x3f333333u);
        drop = u2f(0xbecccccdu);
        step = 0.25f;
        min = high;
        top = u2f(0x3f39999au);
        hang = u2f(0xbf35c28fu);
        back = u2f(0x3ea3d70au);
    }
    const float g = lw(P + 0x208c) == 4 ? lwc1(P + 0x4a0) : lwc1(P + 0x940);
    Vec4 p;
    sub_00259508_v(p, V(P + 0x80), V(P + 0x110), frame_scale_i(2), g);
    p.x = p.x + cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    p.y = p.y + sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    bool found = false;
    Vec4 a, b;
    for (int32_t i = 0; i < 4 && !found; ++i) {
        p.x = p.x + cos_vu(lwc1(P + 0x98)) * step * 0.25f;
        p.y = p.y + sin_vu(lwc1(P + 0x98)) * step * 0.25f;
        a = p;
        a.z = a.z + (top + u2f(0x3e19999au));
        b = p;
        b.z = b.z + high;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) continue;
        const int32_t t = sub_001f35d8();
        if (t == 9 || t == 0xc || t == 0) continue;
        if (atan2_f(lwc1(OUTB + 0x48), vec2_length(V(OUTB + 0x40))) < u2f(0x3eb2b8c2u)) found = true;
    }
    if (!found) return;
    const uint32_t hit = lwu(OUTB + 0x18);
    if (hit != 0) {
        const uint32_t pw = moby_pvar_word0(hit);
        const uint32_t w2 = moby_pvar_word2(lwu(OUTB + 0x18));
        bool grab = false;
        if (pw != 0 && (lhu(pw + 0x1e) & 1) != 0) {
            grab = true;
        } else if (w2 != 0) {
            grab = (lw(w2 + 0x3c) & 1) != 0;
        }
        if (!grab) return;
    }
    sw(P + 0x4f8, lwu(OUTB + 0x18));
    if (lwc1(P + 0x88) + top < lwc1(OUTB + 0x28)) return;
    const Vec4 c = V(OUTB + 0x20);
    Vec4 c5 = c;
    c5.z = c5.z + u2f(0x3d4ccccdu);  // (sp+0x20)
    Vec4 h = V(P + 0x80);
    h.z = c5.z;
    if (segment_collide_v(h, c5, 4, lwu(P + 0x2080), nullptr) != 0) return;
    Vec4 cap = V(OUTB + 0x20);
    cap.z = cap.z + u2f(0x3f19999au);
    if (capsule_collide_v(cap, u2f(0x3ee66666u), u2f(0x3f19999au), 4, lwu(P + 0x2080)) != 0) {
        sw(P + 0x4f4, lwu(P + 0x4f4) | 1u);
    } else {
        sw(P + 0x4f4, lwu(P + 0x4f4) & 0xfffffffeu);
    }
    const float dir = atan2_f(c.x - lwc1(P + 0x80), c.y - lwc1(P + 0x84));
    swc1(P + 0x4e0, c.z);
    if (c.z - lwc1(P + 0x2d8) < min) return;
    bool wall = false;
    for (int32_t j = 0; j < 5; ++j) {
        a = V(P + 0x80);
        b = c;
        const float r = lwc1(P + 0x234) + lwc1(P + 0x234);
        b.x = b.x + cos_vu(dir) * r;
        b.y = b.y + sin_vu(dir) * r;
        const float z = b.z + drop * (1.0f - fpu_div(static_cast<float>(j), 5.0f));
        b.z = z;
        a.z = z;
        if (segment_collide_v(a, b, 2, 0, nullptr) != 0) {
            wall = true;
            swc1(P + 0x4e4, atan2_f(lwc1(OUTB + 0x40), lwc1(OUTB + 0x44)));
            break;
        }
    }
    if (!wall) return;
    if (!(angle_dist(lwc1(P + 0x98), angle_add(lwc1(P + 0x4e4), u2f(0x40490fdbu))) < u2f(0x3f91361eu))) return;
    for (float d = 0.0f;;) {
        a = c;
        a.x = a.x + cos_vu(lwc1(P + 0x4e4)) * d;
        a.y = a.y + sin_vu(lwc1(P + 0x4e4)) * d;
        b = a;
        a.z = a.z + 0.5f;
        b.z = b.z - 0.5f;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) break;
        d = d + u2f(0x3cf5c28fu);
        if (!(d < u2f(0x4019999au))) return;
    }
    Vec4 e = a;  // the last walk probe's start (sp+0x60)
    const float k = -(u2f(0x3cf5c28fu) * 0.5f);
    e.z = c.z;
    e.x = e.x + cos_vu(lwc1(P + 0x4e4)) * k;
    e.y = e.y + sin_vu(lwc1(P + 0x4e4)) * k;
    V(P + 0x4d0) = e;
    swc1(P + 0x4d0, lwc1(P + 0x4d0) + cos_vu(lwc1(P + 0x4e4)) * back);
    const float sy = sin_vu(lwc1(P + 0x4e4)) * back;
    sw(P + 0x4e8, 1);
    swc1(P + 0x4d4, lwc1(P + 0x4d4) + sy);
    swc1(P + 0x4d8, lwc1(P + 0x4d8) + hang);
}

// In the air (PLAYER+0x2dc >= 0.4, not falling faster than 20 x frame scale along the up axis,
// +0x4e8 clear — else +0x1b4 = 0): a ledge ahead of where the hero will be in 20 frames
// (velocity +0x100/+0x104/+0xe8, gravity +0x4a0) starts a grab (+0x1b4 = 0, +0x1f0 = frames +
// 2). Otherwise a near-vertical wall (not type 10 / 12) right in front (probe at local height
// 1.7, or 3 unless state 0x11 or +0x255 without +0x254, out to radius + 1.4) facing back within
// 50 degrees and closer than 0.72 (0.95 in state 0x11) is recorded (+0x460 normal, +0x46c its
// yaw) and, in substate 4 or with +0x2094 / +0x20a0 == 4, unless +0x22b4 and the yaw +0x47c is
// within 170 degrees of the old one, opens the wall-jump window +0x1b4 (6 frames, 7 in 0x11).
// orig: 0x0020b5b8
void hero_air_wall_check() {
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lwc1(P + 0x2dc) < u2f(0x3ecccccdu)) return;
    if (sub_00233a78(P + 0xe0) < lwc1(0x0015ed6c) * -20.0f) return;
    if (lw(P + 0x4e8) != 0) {
        sw(P + 0x1b4, 0);
        return;
    }
    Vec4 p = V(P + 0x80);
    const int32_t n = frame_scale_i(0x14);
    const float fn = static_cast<float>(n);
    const float drop = lwc1(P + 0x4a0) * fn * fn * 0.5f;
    p.x = p.x + lwc1(P + 0x100) * fn;
    p.y = p.y + lwc1(P + 0x104) * fn;
    p.z = p.z + lwc1(P + 0xe8) * fn - drop;
    if (hero_ledge_check_v(p, reinterpret_cast<const float*>(ee_ram_ptr(P + 0x98))) != 0) {
        sw(P + 0x1b4, 0);
        sh(P + 0x1f0, static_cast<uint32_t>(n + 2));
        return;
    }
    if (hero_wall_probe(nullptr, u2f(0x3e99999au), 1.0f) == 0) return;
    float h = u2f(0x3fd9999au);
    if (lw(P + 0x2084) != 0x11 && (lbu(P + 0x255) == 0 || lbu(P + 0x254) != 0)) h = 3.0f;
    Vec4 a = p, b{0.0f, 0.0f, 0.0f, 0.0f};
    sub_002335c8_v(a, 0.0f, 0.0f, h);
    sub_002335c8_v(b, lwc1(P + 0x234) + u2f(0x3fb33333u), 0.0f, h);
    if (segment_collide_v(a, b, 2, 0, nullptr) == 0) return;
    const int32_t t = sub_001f35d8();
    if (t == 10 || t == 12) return;
    if (sub_00233d30(OUTB + 0x40) < u2f(0x3fa78d36u)) return;
    Vec4 ln;
    mat4_transform(ln, V(OUTB + 0x40), M(P + 0x40));
    if (u2f(0x3f5f66f3u) < angle_dist(atan2_f(ln.x, ln.y), u2f(0x40490fdbu))) return;
    const float lim = lw(P + 0x2084) == 0x11 ? u2f(0x3f733333u) : u2f(0x3f3851ecu);
    if (lim < vec2_distance(V(OUTB + 0x20), a)) return;
    V(P + 0x460) = V(OUTB + 0x40);
    swc1(P + 0x46c, atan2_f(lwc1(OUTB + 0x40), lwc1(OUTB + 0x44)));
    if (lw(P + 0x208c) != 4 && lw(P + 0x2094) != 4 && lw(P + 0x20a0) != 4) return;
    if (lw(P + 0x22b4) != 0 && angle_dist(lwc1(P + 0x47c), lwc1(P + 0x46c)) < u2f(0x403de44eu)) return;
    sw(P + 0x1b4, static_cast<uint32_t>(frame_scale_i(lw(P + 0x2084) == 0x11 ? 7 : 6)));
}

// Head tracking: in substates 0, 1, 2, 4 and 7 (else the look target PLAYER+0x2274 is
// cleared), unless the hero moby's +0x53 is 0x54. Every 7th frame (*(0x15f5cc)) the look list
// 0x1aba00 (0-terminated) is scanned for the moby to look at: within its pvars range +0x38 (xy),
// at most 110 degrees off the facing and 60 degrees up or down; highest priority +0x39 first,
// then lowest distance x (1 + angle) (the current one gets 1.5 off). The target is dropped when
// a wall (flag 2) hides it from 1.2 above the hero. The head angles towards its point (+0x3a / 8
// above it) from the head (0.3 ahead, 0.85 up) are clamped (yaw +-59, pitch -25..+35 degrees;
// yaw +-17 and pitch 0 once PLAYER+0x1c0 exceeds frame_scale(77) - frame_scale(50)) and go to
// the FX parameter records 2 and 3 (pitch +0x274, yaw * 0.7 +0x278, +0x1c8 = 0.55 of that).
// orig: 0x0020ac98
void hero_look_at_update() {
    const uint32_t P = g::PLAYER, X = g::FX_PARAMS;
    const int32_t sub = lw(P + 0x208c);
    if (!(sub == 1 || sub == 4 || sub == 2 || sub == 7 || sub == 0)) {
        sw(P + 0x2274, 0);
        return;
    }
    if (lbu(lwu(P + 0x2080) + 0x53) == 0x54) return;
    if (lw(OV(0x0015f5cc)) % 7 == 0) {
        float best = u2f(0x4cbebc20u), prio = 0.0f;
        uint32_t pick = 0;
        for (uint32_t e = OV(0x001aba00); lwu(e) != 0; e += 4) {
            const uint32_t m = lwu(e);
            const uint32_t pw = moby_pvar_word0(m);
            if (pw == 0) continue;
            const float d = vec2_distance(V(P + 0x80), V(m + 0x10));
            if (static_cast<float>(lbu(pw + 0x38)) < d) continue;
            const float ad = angle_dist(atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84)), lwc1(P + 0x98));
            if (u2f(0x3ff5be0bu) < ad) continue;
            const float pitch = atan2_f(vec2_distance(V(m + 0x10), V(P + 0x80)), lwc1(P + 0x88) - lwc1(m + 0x18));
            if (u2f(0x3f860a92u) < abs_f(pitch)) continue;
            float score = ad * d + d;
            if (m == lwu(P + 0x2274)) {
                score = score - 1.5f;
                if (score < 0.0f) score = 0.0f;
            }
            const float pr = static_cast<float>(lbu(pw + 0x39));
            if (!(prio < pr) && !(score < best)) continue;
            if (prio < pr) prio = pr;
            best = score;
            pick = m;
        }
        sw(P + 0x2274, pick);
    }
    if (lwu(P + 0x2274) == 0) return;
    Vec4 head;
    sub_00233660_v(head, 0.0f, 0.0f, u2f(0x3f99999au));
    Vec4 tp = V(lwu(P + 0x2274) + 0x10);
    tp.z = tp.z + 0.5f;
    if (segment_collide_v(head, tp, 2, 0, nullptr) != 0) sw(P + 0x2274, 0);
    const uint32_t t = lwu(P + 0x2274);
    if (t == 0) return;
    Vec4 aim = V(t + 0x10);
    const uint32_t pw = moby_pvar_word0(t);
    if (pw != 0) aim.z = aim.z + static_cast<float>(lbu(pw + 0x3a)) * 0.125f;
    sub_00233660_v(tp, u2f(0x3e99999au), 0.0f, u2f(0x3f59999au));
    float yaw = angle_sub(atan2_f(aim.x - tp.x, aim.y - tp.y), lwc1(P + 0x98));
    float pitch = atan2_f(vec2_distance(aim, tp), tp.z - aim.z);
    const float ylim = u2f(0x3f83cea9u);
    if (ylim < yaw) {
        yaw = ylim;
    } else if (yaw < -ylim) {
        yaw = -ylim;
    }
    if (u2f(0x3f1c61aau) < pitch) pitch = u2f(0x3f1c61aau);
    if (pitch < u2f(0xbedf66f3u)) pitch = u2f(0xbedf66f3u);
    const int32_t a = frame_scale_i(0x4d);
    if (a - frame_scale_i(0x32) < lw(P + 0x1c0)) {
        const float l = u2f(0x3e97e9d8u);
        if (l < yaw) {
            yaw = l;
        } else if (yaw < -l) {
            yaw = -l;
        }
        pitch = 0.0f;
    }
    const float fs = lwc1(0x0015ed64);
    fx_params_set_3(fs * u2f(0x3cac0831u), fs * u2f(0x3e8a3d71u));
    const float fs2 = lwc1(0x0015ed64);
    swc1(X + 0x274, pitch);
    swc1(X + 0x278, yaw * u2f(0x3f333333u));
    fx_params_set_2(fs2 * u2f(0x3c656042u), fs2 * u2f(0x3e99999au));
    swc1(X + 0x1c8, lwc1(X + 0x278) * u2f(0x3f0ccccdu));
}

// Lean against a wall ahead (when PLAYER+0x20ab and the slot-0 item is not 0x1a): in state
// 0x81 the lean is reset to -0.35. hero_wall_probe at height 0.57 (0.7 with +0x20a8; then a
// class 500..540 moby hit is ignored) out to 1.3; on a hit the lean FX_PARAMS+0x694 grows
// from -0.52 to -1.22 as the wall comes within 1.3 of the hero's radius, the FX speeds +0x6d4 /
// +0x6d8 are 0.05 / 0.3 of the frame scale and PLAYER+0x22c8 = 1 (else 0).
// orig: 0x0020a318
void hero_wall_lean() {
    const uint32_t P = g::PLAYER, X = g::FX_PARAMS;
    sh(P + 0x22c8, 0);
    if (lbu(P + 0x20ab) == 0) return;
    if (sub_0020d498(0) == 0x1a) return;
    if (lw(P + 0x2084) == 0x81) {
        const float fs = lwc1(0x0015ed64);
        swc1(X + 0x694, u2f(0xbeb2b8c2u));
        swc1(X + 0x6d4, fs * u2f(0x3d4ccccdu));
        swc1(X + 0x6d8, fs * u2f(0x3e99999au));
    }
    const float reach = lh(P + 0x22c8) != 0 ? u2f(0x3fccccccu) : u2f(0x3fa66666u);
    const float z = lbu(P + 0x20a8) != 0 ? u2f(0x3f333333u) : u2f(0x3f11eb85u);
    if (hero_wall_probe(nullptr, z, reach) == 0) return;
    if (lbu(P + 0x20a8) != 0) {
        const uint32_t hit = lwu(OV(0x00173e58));
        if (hit != 0 && moby_in_range_class_500(hit) != 0) return;
    }
    const float fs = lwc1(0x0015ed64);
    swc1(X + 0x6d4, fs * u2f(0x3d4ccccdu));
    swc1(X + 0x6d8, fs * u2f(0x3e99999au));
    const float d = vec3_distance(V(P + 0x80), V(OV(0x00173e60))) - lwc1(P + 0x234);
    const float r = u2f(0x3fa66666u), lo = u2f(0xbf9c61aau), hi = u2f(0xbf060a92u);
    float lean = fpu_div(r - d, r) * 1.25f * lo;
    if (hi < lean) lean = hi;
    if (lean < lo) lean = lo;
    swc1(X + 0x694, lean);
    sh(P + 0x22c8, 1);
}

// orig: 0x0020a500
void sub_0020a500() {
    sw(g::PLAYER + 0x1004, 0);
    sw(g::PLAYER + 0x1008, 0);
    uint32_t p = g::PLAYER;
    for (int32_t n = 3; n >= 0; --n, p += 0x10) {
        sw(p + 0x1024, 0);
        sw(p + 0x1028, 0);
    }
}

// orig: 0x0020d460 — 0x50-byte slots at PLAYER+0x1090; value if the slot's state is 2.
uint32_t sub_0020d460(int32_t slot) {
    const uint32_t p = g::PLAYER + static_cast<uint32_t>(slot * 0x50);
    return lw(p + 0x10b4) == 2 ? lwu(p + 0x1090) : 0u;
}

// orig: 0x0020d498
int32_t sub_0020d498(int32_t slot) {
    const uint32_t p = g::PLAYER + static_cast<uint32_t>(slot * 0x50);
    return lw(p + 0x10b4) == 2 ? lw(p + 0x10b8) : -1;
}

// orig: 0x0020d568
int32_t sub_0020d568() {
    if (static_cast<uint32_t>(lw(g::HERO_SUBSTATE) - 0x11) < 2u) return 1;
    const int32_t s = lw(g::HERO_STATE);
    return (s == 0x6a || s == 0x82 || s == 0x76 || s == 0x75) ? 1 : 0;
}

// orig: 0x0020d5b8
uint32_t sub_0020d5b8(uint32_t moby, uint32_t v) { return v == 0xff ? lbu(moby + 0xa5) : v; }

// orig: 0x0020d658 — float from a 32-entry ring at PLAYER+0x2128 (write index at +0x21a8,
// fill count at +0x21ac), `back` entries before the newest (clamped).
float sub_0020d658(int32_t back) {
    int32_t a1 = back < 0x20 ? back : 0x1f;
    const int32_t count = lw(g::PLAYER + 0x21ac);
    if (count < a1) a1 = count;
    const int32_t v0 = lw(g::PLAYER + 0x21a8) - a1;
    const int32_t v1 = v0 + 0x1f;
    const int32_t t = (-1 < v1) ? v1 : v0 + 0x3e;
    const int32_t idx = v1 - ((t >> 5) << 5);
    return lwc1(g::PLAYER + 0x2128 + static_cast<uint32_t>(idx * 4));
}

// orig: 0x0020da10
int32_t sub_0020da10(int32_t v) {
    if (v == 2) return 0x77;
    if (v < 3) return v == 1 ? 0x79 : 0;
    return v == 3 ? 0x75 : 0;
}

// orig: 0x00200fd8 — append a 16-byte DMA tag (cnt 0x15, addr 0x152040) at the packet cursor 0x1611c0.
void sub_00200fd8() {
    constexpr OvConst<0x001611c0u> CUR{};
    if (lwu(CUR) == 0) return;
    sw(lwu(CUR) + 0x0, 0x30000015u);
    sw(lwu(CUR) + 0x4, 0x00152040u);
    sw(lwu(CUR) + 0x8, 0);
    sw(lwu(CUR) + 0xc, 0x50000015u);
    sw(CUR, lwu(CUR) + 0x10);
}

// orig: 0x00201040 — same with cnt 9 and addr (*(0x15eeb8) + 0xc0) & 0x0fffffff.
void sub_00201040() {
    constexpr OvConst<0x001611c0u> CUR{};
    sw(lwu(CUR) + 0x0, 0x30000009u);
    const uint32_t addr = (lwu(0x0015eeb8) + 0xc0u) & 0x0fffffffu;
    sw(lwu(CUR) + 0x4, addr);
    sw(lwu(CUR) + 0x8, 0);
    sw(lwu(CUR) + 0xc, 0x50000009u);
    sw(CUR, lwu(CUR) + 0x10);
}

// orig: 0x002039a0
void sub_002039a0() {
    constexpr OvConst<0x00179410u> base{};
    if (lw(base) == 0) return;
    const int32_t a = lw(lwu(OV(0x0015f660)) + static_cast<uint32_t>(lw(base + 0x20) << 4) + 8);
    if (a != -1) {
        constexpr uint32_t b = 0x001516d0;
        if (a == static_cast<int32_t>(lh(b + 0x54)) - 0x7530 &&
            !((static_cast<uint32_t>(lhu(b + 0x5a)) - 6u) < 2u))
            sh(b + 0x5a, 5);
    }
    sw(base, 0);
    sw(base + 0x04, 0);
    sw(base + 0x20, 0xffffffffu);
    sw(base + 0x24, 0xffffffffu);
}

// orig: 0x00203a68 — index of the 16-byte record (list *(0x15f660), count *(0x17943c))
// whose word +4 equals v; -1 if none.
int32_t sub_00203a68(int32_t v) {
    constexpr OvConst<0x00179410u> base{};
    if (lw(base + 0x2c) <= 0) return -1;
    const uint32_t list = lwu(OV(0x0015f660));
    if (lw(list + 4) == v) return 0;
    for (int32_t i = 1; i < lw(base + 0x2c); ++i)
        if (lw(list + static_cast<uint32_t>(i << 4) + 4) == v) return i;
    return -1;
}

// orig: 0x00204af8 — search the 150 (u16, u16) pairs at 0x179450 for `key` in
// field `field` (0 or 1); copy the other field to *out (if out). Index or -1.
int32_t sub_00204af8(int32_t key, int32_t field, uint32_t out) {
    constexpr OvConst<0x00179450u> T{};
    const int32_t k = static_cast<int16_t>(key);
    uint32_t p = T + static_cast<uint32_t>(field) * 2u;
    for (int32_t i = 0; i < 0x96; ++i, p += 4) {
        if (lh(p) == k) {
            const uint32_t off = field != 0 ? static_cast<uint32_t>(i) * 4u : 2u + static_cast<uint32_t>(i) * 4u;
            if (out != 0) sh(out, lhu(off + T));
            return i;
        }
    }
    return -1;
}

// orig: 0x002072c8 — copy the hero moby's colour (+0x38) to the two mobys of each of the 7 slots.
void sub_002072c8() {
    for (int32_t s = 0; s < 7; ++s) {
        const uint32_t p = g::PLAYER + static_cast<uint32_t>(s * 0x50);
        for (int32_t k = 0; k < 2; ++k) {
            const uint32_t m = lwu(p + (k == 1 ? 0x1094u : 0x1090u));
            if (m != 0) sd(m + 0x38, ld(lwu(g::PLAYER + 0x2080) + 0x38));
        }
    }
}

// orig: 0x00207d00 — once: queue 0x207d40 with the hero moby (sub_001f93e0).
void sub_00207d00() {
    if (lh(g::PLAYER + 0x1eb2) != 0) return;
    const uint32_t hero = lwu(g::PLAYER + 0x2080);
    sh(g::PLAYER + 0x1eb2, 1);
    sub_001f93e0(OVF(0x00207d40), hero);
}

// orig: 0x0020d4d0
int32_t sub_0020d4d0(int32_t slot) {
    const int32_t v = sub_0020d498(slot);
    return (v == 0x11 || v == 0xa || v == 0x14 || v == 0x19) ? 1 : 0;
}

// orig: 0x0020d518
int32_t sub_0020d518(int32_t slot) { return (static_cast<uint32_t>(sub_0020d498(slot)) - 5u) < 3u ? 1 : 0; }

// orig: 0x0020d540
int32_t sub_0020d540(int32_t slot) { return (static_cast<uint32_t>(sub_0020d498(slot)) - 0x1cu) < 2u ? 1 : 0; }

// orig: 0x0020d5d0 — per-kind parameter (jump table at 0x1e7c40). *out = 0, then:
// 0x37 → -0.9599 (1), 0x1c/0x1d/0x1f → -1.3090 when *(0x13fdf8) > 25 (1),
// 0x31/0x32/0x4a/0x4b/0x50/0x60/0x6d/0x6e → 1, anything else → 0.
int32_t sub_0020d5d0(int32_t kind, float* out) {
    if (on_rilgar()) {  // (Rilgar's own: L5:0x0023b378, writes the float through the EE address in $a1)
        const uint32_t b = ee_stack_alloc(16);
        const int32_t r = static_cast<int32_t>(ril_call(ril_sub_20d5d0_u, static_cast<uint32_t>(kind), b).v0);
        *out = ee_read<float>(b);
        ee_stack_free(16);
        return r;
    }
    if (on_kalebo()) {  // (level 16's own: L16:0x0020afa0, Rilgar's build: the float through the EE address in $a1)
        const uint32_t b = ee_stack_alloc(16);
        const int32_t r = static_cast<int32_t>(kal_call(kal_sub_0020d5d0_u, static_cast<uint32_t>(kind), b).v0);
        *out = ee_read<float>(b);
        ee_stack_free(16);
        return r;
    }
    *out = 0.0f;
    switch (kind) {
        case 0x37:
            *out = u2f(0xbf75be0bu);
            return 1;
        case 0x1c: case 0x1d: case 0x1f:
            if (25.0f < lwc1(0x0013fdf8)) *out = u2f(0xbfa78d36u);
            return 1;
        case 0x31: case 0x32: case 0x4a: case 0x4b: case 0x50: case 0x60: case 0x6d: case 0x6e:
            return 1;
        default:
            return 0;
    }
}

// orig: 0x0020fd80 — jump table at 0x1e7d90 (kinds 9..23).
void sub_0020fd80(int32_t kind) {
    switch (kind) {
        case 9: case 11: case 13: case 16: case 23:
            sh(g::PLAYER + 0x22ce, 3);
            sh(g::PLAYER + 0x1dc, static_cast<uint32_t>(frame_scale_i(0x46)));
            break;
        default:
            break;
    }
}

// orig: 0x0020fde0 — jump table at 0x1e7dd0 (kinds 9..26).
void sub_0020fde0(int32_t kind) {
    switch (kind) {
        case 9: case 11: case 13: case 14: case 15: case 16: case 18: case 19: case 22: case 23: case 26:
            sh(0x0013f52a, static_cast<uint32_t>(frame_scale_i(0x46)));
            break;
        default:
            break;
    }
}

// orig: 0x0020fe30
int32_t sub_0020fe30() {
    if (sub_00233f38() != 0) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    if (lbu(g::PLAYER + 0x20a4) != 0) return 0;
    const uint32_t s = lwu(g::PLAYER + 0x208c);
    if (s < 3u || s == 4u) return lbu(g::PLAYER + 0x12e4) == 0 ? 1 : 0;
    return 0;
}

// orig: 0x00203ad8 — the record for v (sub_00203a68), or the default record 0x1796a8.
uint32_t sub_00203ad8(int32_t v) {
    if (const uint32_t s = advanced_text(v)) return s;  // enhancement: the Advanced page's texts
    const int32_t i = sub_00203a68(v);
    if (i >= 0) return lwu(lwu(OV(0x0015f660)) + static_cast<uint32_t>(i << 4));
    {  // (host: testing) RAC1_TEXT_TRACE=1: a text id the table lacks
        static const bool trace = std::getenv("RAC1_TEXT_TRACE") != nullptr;
        if (trace) std::fprintf(stderr, "[text] id 0x%x missing (level %u, frame %d, count %d)\n", static_cast<uint32_t>(v), lwu(0x0015ed84), lw(0x0015eea4), lw(OV(0x0017943c)));
    }
    return OV(0x001796a8);
}

// orig: 0x00208038 — queue (vec, v, a, b) in the 8-entry arrays at PLAYER+0x1dd0.. (count +0x1eb0).
void sub_00208038_v(const Vec4& vec, uint32_t v, float a, float b) {
    const int32_t n = lh(g::PLAYER + 0x1eb0);
    V(g::PLAYER + 0x1dd0 + static_cast<uint32_t>(n << 4)) = vec;
    sw(g::PLAYER + static_cast<uint32_t>(n << 2) + 0x1e50, v);
    swc1(g::PLAYER + static_cast<uint32_t>(lh(g::PLAYER + 0x1eb0) << 2) + 0x1e70, a);
    swc1(g::PLAYER + static_cast<uint32_t>(lh(g::PLAYER + 0x1eb0) << 2) + 0x1e90, b);
    sub_00207d00();
    sh(g::PLAYER + 0x1eb0, lhu(g::PLAYER + 0x1eb0) + 1u);
}
// orig: 0x00208038 (EE-address form)
void sub_00208038(uint32_t vec, uint32_t v, float a, float b) { sub_00208038_v(V(vec), v, a, b); }

// orig: 0x0020d2f8
void sub_0020d2f8_v(Vec4& dst) { sub_00233660_v(dst, u2f(0x3e99999au), 0.0f, u2f(0x3fab851fu)); }
// orig: 0x0020d2f8 (EE-address form)
void sub_0020d2f8(uint32_t dst) { sub_0020d2f8_v(V(dst)); }

// orig: 0x0020f0b8 — sum of |a - b| over the 3x3 part of two matrices.
float mat3_abs_diff(uint32_t a, uint32_t b) {
    float sum = 0.0f;
    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t k = 0; k < 3; ++k)
            sum = sum + abs_f(lwc1(a + r * 16 + k * 4) - lwc1(b + r * 16 + k * 4));
    return sum;
}

// orig: 0x00205000 — kind of the current interaction (0 = none).
uint32_t sub_00205000(int32_t flag) {
    if (on_blarg()) return blg_call(blg_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 6's own: L6:0x0021d0a0)
    if (on_batalia()) return bat_call(bat_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 8's own: L8:0x00217b80)
    if (on_pokitaru()) return pok_call(pok_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 11's own: L11:0x00236968)
    if (on_hoven()) return hov_call(hov_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 12's own: L12:0x00227f10)
    if (on_oltanis()) return olt_call(olt_sub_00205000_u, static_cast<uint32_t>(flag)).v0;  // (level 14's own: L14:0x00218610)
    if (on_quartu()) return qua_call(qua_sub_00205000_u, static_cast<uint32_t>(flag)).v0;  // (level 15's own: L15:0x001fe778)
    if (on_kalebo()) return kal_call(kal_sub_00205000_u, static_cast<uint32_t>(flag)).v0;  // (level 16's own: L16:0x00203868)
    if (on_veldin_finale()) return vf_call(vf_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 18's own: L18:0x0020dde8)
    if (on_veldin_orbit()) return vo_call(vo_sub_205000_u, static_cast<uint32_t>(flag)).v0;  // (level 17's own: L17:0x002020a8)
    if (on_l1_family() || on_rilgar()) return sub_00205000_l1(flag);
    const uint32_t mode = lbu(g::PLAYER + 0x20a4);
    if (mode == 2) return 0;
    if (mode == (mode < 3 ? 1u : 3u)) return 0;
    if (lw(g::PLAYER + 0x22a8) == 1) return 0x54;
    if (lbu(g::PLAYER + 0x12e2) != 0) return 0x6e;
    if (flag == 0) return 0;
    if (lbu(g::PLAYER + 0x20a8) == 0) return 0;
    if (lbu(g::PLAYER + 0x20aa) == 0) return 0;
    if (lh(g::PLAYER + 0x22c8) != 0) return 0;
    const int32_t k = sub_0020d498(0);
    return lwu(OV(0x00179ac0) + static_cast<uint32_t>(k * 0x4c) + 0x24);
}

// Footstep of foot `foot` (0/1): with `sound`, level_sound_start(a, foot, c) where c says the
// hero holds a moby of class 0xad (PLAYER+0x10e0). On level 0xc (*(0x15ed84)), unless
// PLAYER+0x12ed or +0x2fc, a footprint (particle 0x46, colour 1 for foot 0) is spawned on the
// floor (floor_probe from 0.5 above, +0.04) under the foot joint 0x16 + foot, 0.15 ahead.
// orig: 0x00205f60
void hero_footstep(int32_t a, uint32_t foot, int32_t sound) {
    if (on_batalia()) { bat_call(bat_hero_footstep_u, static_cast<uint32_t>(a), foot, static_cast<uint32_t>(sound)); return; }  // (level 8's own: L8:0x00218ab8)
    if (on_pokitaru()) { pok_call(pok_hero_footstep_u, static_cast<uint32_t>(a), foot, static_cast<uint32_t>(sound)); return; }  // (level 11's own: L11:0x002378a0)
    if (on_quartu()) { qua_call(qua_hero_footstep_u, static_cast<uint32_t>(a), foot, static_cast<uint32_t>(sound)); return; }  // (level 15's own: L15:0x001ff6c0)
    if (on_kalebo()) { kal_call(kal_hero_footstep_u, static_cast<uint32_t>(a), foot, static_cast<uint32_t>(sound)); return; }  // (level 16's own: L16:0x002047a0)
    if (on_veldin_finale()) { vf_call(vf_hero_footstep_u, static_cast<uint32_t>(a), foot, static_cast<uint32_t>(sound)); return; }  // (level 18's own: L18:0x0020ed30)
    if (on_l1_family() || on_rilgar() || on_blarg()) return hero_footstep_l1(a, foot, sound);
    const uint32_t P = g::PLAYER;
    if (sound != 0) {
        const uint32_t held = lwu(P + 0x10e0);
        const int32_t c = held != 0 && lh(held + 0xa6) == 0xad ? 1 : 0;
        level_sound_start(a, static_cast<int32_t>(foot), c, 0, lwu(P + 0x2080));
    }
    if (lw(0x0015ed84) != 0xc) return;
    if (lbu(P + 0x12ed) != 0) return;
    if (lw(P + 0x2fc) != 0) return;
    const float k = u2f(0x3e19999au);
    Vec4 p;
    moby_chain_point_v(lwu(P + 0x2080), foot + 0x16, p);
    p.x = p.x + cos_vu(lwc1(P + 0x98)) * k;
    p.y = p.y + sin_vu(lwc1(P + 0x98)) * k;
    p.z = floor_probe_v(0.5f, p, 0) + u2f(0x3d23d70au);
    spawn_particle_46_v(p, foot == 0 ? 1u : 0u, lwc1(P + 0x98));
}

// orig: 0x002050c0 — use up n (1 when n >= 2) of PLAYER+0x22a8 (not below 0) and re-request
// camera 0x7535; nothing for n == 0.
void hero_counter_use(int32_t n) {
    if (n == 0) return;
    const int32_t d = n < 2 ? n : 1;
    const int32_t v = lw(g::PLAYER + 0x22a8) - d;
    sw(g::PLAYER + 0x22a8, static_cast<uint32_t>(v));
    if (v < 0) sw(g::PLAYER + 0x22a8, 0);
    camera_request_7535();
}

// orig: 0x00205ea8 — PLAYER+0x2048 = whether the timer +0x2044 fell below 20 with +0x2040 set
// (always 0 without *(0x13d4db)); then +0x2044 = 100000, +0x2040 = 0, and when set slot 7
// requests camera 0x753a (controllers 0x235dc0 / 0x23a218 / 0x23a2d0, args &PLAYER+0x2048, 1).
void hero_camera_7_update() {
    const uint32_t P = g::PLAYER;
    if (lbu(0x0013d4db) == 0) {
        sw(P + 0x2048, 0);
        return;
    }
    sw(P + 0x2048, lwc1(P + 0x2044) < 20.0f && lw(P + 0x2040) != 0 ? 1u : 0u);
    swc1(P + 0x2044, u2f(0x47c35000u));
    sw(P + 0x2040, 0);
    if (lw(P + 0x2048) != 0) camera_request(7, 0x753a, OVF(0x00235dc0), OVF(0x0023a218), OVF(0x0023a2d0), P + 0x2048, 1);
}

// orig: 0x00205538 — reset the 31 effect records at FX_PARAMS (stride 0xb0): detach
// active ones from their owner moby, clear three vectors, scale = 1.
void sub_00205538() {
    uint32_t r = g::FX_PARAMS;
    for (int32_t k = 0; k < 31; ++k, r += 0xb0) {
        if (lbu(r + 1) != 0) {
            const uint32_t m = sub_00205110(lh(r + 0xa2));
            if (m != 0) sub_0024f608(m, r);
        }
        vec_clear(V(r + 0x40));
        vec_clear(V(r + 0x60));
        vec_clear(V(r + 0x90));
        swc1(r + 0xac, 1.0f);
    }
}

// orig: 0x00207c48 — fade 0x15f3fc in to 1 (phase 0), hold one frame (1), fade out to 0 (2).
void sub_00207c48() {
    const uint32_t ACTIVE = OV(0x0015f680), PHASE = OV(0x0015f684), STEP = OV(0x0015f67c), V0 = OV(0x0015f3fc);
    if (lw(ACTIVE) == 0) return;
    const int32_t ph = lw(PHASE);
    if (ph == 0) {
        approach_f(V0, 1.0f, lwc1(STEP));
        if (lwc1(V0) == 1.0f) sw(PHASE, 1);
    } else if (ph == 1) {
        sw(PHASE, 2);
    } else if (ph == 2) {
        approach_f(V0, 0.0f, lwc1(STEP));
        if (lwc1(V0) == 0.0f) sw(ACTIVE, 0);
    }
}

// orig: 0x00209018 — sample a looping table of n floats at the position PLAYER+0xaa8,
// blending entries i and i-1 (or n-1 when bit 1 of PLAYER+0xa98 is set) with weight
// frac and (PLAYER+0xaac - frac).
float sub_00209018(uint32_t table, int32_t n) {
    const int32_t i = float_to_int(lwc1(g::PLAYER + 0xaa8));
    int32_t j = ee_rem(static_cast<int32_t>(static_cast<uint32_t>(i) + static_cast<uint32_t>(n) - 1u), n);
    if ((lw(g::PLAYER + 0xa98) & 2) != 0) j = n - 1;
    float ip;
    const float frac = split_int_frac(&ip, lwc1(g::PLAYER + 0xaa8));
    const float wj = lwc1(g::PLAYER + 0xaac) - frac;
    return frac * lwc1(table + static_cast<uint32_t>(i) * 4u) + wj * lwc1(table + static_cast<uint32_t>(j) * 4u);
}

// orig: 0x00205600 — schedule the next random event: first call arms it 30 frames after
// 0x15f5cc; when that time is reached, set PLAYER+0xff0 and draw the next time.
void sub_00205600() {
    constexpr OvConst<0x0015f5ccu> NOW{};
    if (lw(g::PLAYER + 0xff0) != 0) return;
    if (lw(g::PLAYER + 0xff8) == 0) {
        const int32_t d = frame_scale_i(0x1e);
        sw(g::PLAYER + 0xff4, lwu(NOW) + static_cast<uint32_t>(d));
        return;
    }
    if (!(lw(g::PLAYER + 0xff4) < lw(NOW))) return;
    sw(g::PLAYER + 0xff0, 1);
    const int32_t a = frame_scale_i(0x14);
    const int32_t b = rand_mod(lw(g::PLAYER + 0xff8));
    const int32_t c = rand_mod(lw(g::PLAYER + 0xff8));
    sw(g::PLAYER + 0xff4, lwu(NOW) + static_cast<uint32_t>(a) + static_cast<uint32_t>(b) + static_cast<uint32_t>(c));
}

// orig: 0x00206c08 — release the 8 hero-held slots (PLAYER+0x2218, owners at +0x2238;
// owner 0 = the hero moby): live ones go through sub_0028d918, all become -1.
void sub_00206c08() {
    for (uint32_t k = 0; k < 8; ++k) {
        const uint32_t slot_p = g::PLAYER + 0x2218 + k * 4;
        const uint32_t owner = lwu(g::PLAYER + 0x2238 + k * 4);
        const int32_t slot = lw(slot_p);
        if (slot != -1) {
            const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
            const uint32_t want = owner != 0 ? owner : lwu(g::PLAYER + 0x2080);
            if (lwu(e + 0x88) == want && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sw(slot_p, 0xffffffffu);
    }
}

// orig: 0x002079e8 — in state 0x7f, swing the hero moby (sub_00262b80 on PLAYER+0x6b0/+0x6b4).
void sub_002079e8() {
    if (on_quartu()) { qua_call(qua_sub_002079e8_u); return; }  // (level 15's own: L15:0x002008d0)
    if (lw(g::HERO_STATE) != 0x7f) return;
    const float k = lwc1(0x0015ed6c);
    sub_00262b80(lwu(g::PLAYER + 0x2080), g::PLAYER + 0x6b0, g::PLAYER + 0x6b4, u2f(0x3e32b8c2u),
                 k * u2f(0x3fc90fdbu), k * u2f(0x40060a92u));
}

// orig: 0x00204b60 — move the entry of `key` (sub_00204af8) to the end of the byte MRU list at
// 0x141e08 (count *(0x15ee30)), appending it when absent.
void sub_00204b60(int32_t key) {
    constexpr uint32_t LIST = 0x00141e08, COUNT = 0x0015ee30;
    const int32_t v = sub_00204af8(static_cast<int16_t>(key), 0, 0);
    if (v == -1) return;
    const uint32_t b = static_cast<uint32_t>(v);
    const int32_t n = lw(COUNT);
    int32_t i = 0;
    if (lbu(LIST) != b) {
        if (n <= 0) {
            const uint32_t c = lwu(COUNT);
            sb(LIST + c, b);
            sw(COUNT, c + 1);
            return;
        }
        i = 1;
        while (lbu(LIST + static_cast<uint32_t>(i)) != b && i < n) ++i;  // also looks at [n], as the original
    }
    if (i < n) {
        for (; i < n - 1; ++i) sb(LIST + static_cast<uint32_t>(i), lbu(LIST + static_cast<uint32_t>(i) + 1));
        sb(LIST + static_cast<uint32_t>(i), 0);
        sw(COUNT, lwu(COUNT) - 1u);
    }
    const uint32_t c = lwu(COUNT);
    sb(LIST + c, b);
    sw(COUNT, c + 1);
}

// orig: 0x00207330 — derive a grey colour (4/10 and 3/10 of the sum of bytes 4..6 of m's
// colour) for `target`, and refresh palette entries 12 and the light at 0x17fec0+0x300.
void sub_00207330(uint32_t target, uint32_t m) {
    const uint32_t s = lbu(m + 0x3c) + lbu(m + 0x3d) + lbu(m + 0x3e);
    const int32_t a = static_cast<int32_t>(s << 2) / 10;
    const int32_t b = static_cast<int32_t>(s * 3u) / 10;
    moby_set_color_packed(target, static_cast<int64_t>(a + (b << 8)), 0xc, 0xc, 0);
    moby_palette_colour_1(m, OV(0x001801d0));
    Vec4 c;
    moby_palette_colour_0_v(m, c);
    constexpr OvConst<0x0017fec0u> L{};
    const float sum = (c.x + c.y) + c.z;
    swc1(L + 0x30c, u2f(0xbe99999au));
    swc1(L + 0x308, sum * 0.0f);
    swc1(L + 0x300, sum * 0.5f);
    swc1(L + 0x304, sum * u2f(0x3ecccccdu));
}

// orig: 0x00208f20 — set the hero animation speed PLAYER+0xa90 so that the animation reaches
// position x (looping over the set length) — scaled by b / (a * frame rate); applied directly
// when c is -1, else approached with step c. Only when a > 0.
void sub_00208f20(float x, float a, float b, float c) {
    const float pos = lwc1(g::PLAYER + 0xaa8);
    float dist;
    if (pos < x) {
        dist = x - pos;
    } else {
        float rate = 2.0f;
        const uint32_t hero = lwu(g::PLAYER + 0x2080);
        if (lw(g::PLAYER + 0xa9c) == 0) rate = fpu_div(0.5f, lwc1(g::PLAYER + 0xa94));
        const uint32_t set = lwu(lwu(hero + 0x24) + lbu(hero + 0x53) * 4u + 0x48);
        dist = vuc(x + vuc(vuc(rate * static_cast<float>(static_cast<int32_t>(lbu(set + 0x10)))) - pos));  // (EE clamps: rate may be FLT_MAX)
    }
    if (!(0.0f < a)) return;
    float fr = lwc1(g::PLAYER + 0xa94);
    if (lw(g::PLAYER + 0xa9c) != 0) fr = lwc1(lwu(lwu(g::PLAYER + 0x2080) + 0x6c));
    const float v = fpu_div(vuc(b * dist), vuc(a * fr));
    if (c == -1.0f) swc1(g::PLAYER + 0xa90, v);
    else approach_f(g::PLAYER + 0xa90, v, c);
}

// orig: 0x00203908 — start dialogue/record `v` with option `opt` when nothing is active.
int32_t sub_00203908(int32_t v, int32_t opt) {
    constexpr OvConst<0x00179410u> base{};
    if (lw(base) != 0) return 0;
    const int32_t cur = lw(base + 0x24);
    if (cur != -1) return 0;
    constexpr uint32_t b = 0x001516d0;
    if (lw(b + 0x50) != 0) return 0;
    if (lw(b + 0x1c) != cur) return 0;
    if (lhu(0x00141968 + static_cast<uint32_t>(opt << 3)) == 0xffff) return 0;
    sw(base + 0x28, static_cast<uint32_t>(opt));
    sw(base + 0x24, static_cast<uint32_t>(v));
    sub_00204b60(v);
    return 1;
}

// orig: 0x00206cf8 — hint triggers: dialogue 0x4e2e (once the hero is in a "busy" state,
// when the flag 0x13d4c4 is set) and, every 128 ticks, dialogue 0x4e26 when 9+ of the 37
// collectibles (except #8 and #24) are owned but not yet placed.
void sub_00206cf8() {
    if (lhu(0x00141d28) == 0 && lbu(0x0013d4c4) != 0) {
        if (sub_0020d568() != 0) sub_00203908(0x4e2e, 0x78);
    }
    const int32_t t = lw(OV(0x0015f5cc));
    if (t <= 0) return;
    if ((t & 0x7f) != 0) return;
    if (lhu(0x00141be8) != 0) return;
    if (lhu(0x001418f0) != 0) return;
    int32_t n = 0;
    for (int32_t i = 0; i < 0x25; ++i) {
        if (lw(OV(0x00179ac0) + 8 + static_cast<uint32_t>(i * 0x4c)) != 0) continue;
        if (i == 8 || i == 0x18) continue;
        if (lbu(0x0013d4c0 + static_cast<uint32_t>(i)) != 0) ++n;
    }
    if (!(n < 9)) sub_00203908(0x4e26, 0x50);
}

// orig: 0x002090d0 — rotate the hero by Euler angles (x, y, z) about the point `pivot`
// (hero-relative): the hero position moves so that the pivot stays in place.
void sub_002090d0_v(const Vec4& pivot, float x, float y, float z) {
    constexpr uint32_t ROT = 0x0013f3e0, POS = 0x0013f3d0;
    const Vec4 p = pivot;
    Mat4 m;
    euler_to_mat4_vu(m, V(ROT));
    Vec4 before;
    mat4_transform(before, p, m);
    Mat4 r;
    const Vec4 ang{x, y, z, 0.0f};
    euler_to_mat4_vu(r, ang);
    mat4_mul(m, m, r);
    Vec4 after;
    mat4_transform(after, p, m);
    Vec4 d;
    vec3_sub(d, before, after);
    vec3_add(V(POS), V(POS), d);
    mat_to_euler_v(m, ROT);
}
// orig: 0x002090d0 (EE-address form)
void sub_002090d0(uint32_t pivot, float x, float y, float z) { sub_002090d0_v(V(pivot), x, y, z); }

// orig: 0x002078a8 — per-frame swing of the hero moby in states 0x7f/0x6b; in 0x6b also keep
// the attached moby *(PLAYER+0x86c) at the hero's hand offset (0, 0, 0.21) rotated by the hero
// matrix and the angles 0x17a6c0.
void sub_002078a8() {
    if (on_quartu()) { qua_call(qua_sub_002078a8_u); return; }  // (level 15's own: L15:0x002007d8)
    const float k = lwc1(0x0015ed6c);
    if (lw(g::HERO_STATE) == 0x7f)
        sub_00262b00(lwu(g::PLAYER + 0x2080), g::PLAYER + 0x6a8, g::PLAYER + 0x6ac, u2f(0x3e99999au), k * u2f(0x40df66f3u));
    if (lw(g::HERO_STATE) != 0x6b) return;
    if (lh(g::PLAYER + 0x8bc) != 0) return;
    sub_00262b00(lwu(g::PLAYER + 0x2080), g::PLAYER + 0x6a8, g::PLAYER + 0x6ac, u2f(0x3f8ccccdu),
                 lwc1(0x0015ed6c) * u2f(0x40a78d36u));
    const uint32_t p = lwu(g::PLAYER + 0x86c);
    if (p == 0) return;
    const uint32_t hero = lwu(g::PLAYER + 0x2080);
    lq_sq(p + 0x10, hero + 0x10);
    Vec4 off{0.0f, 0.0f, 0.0f, 0.0f};
    off.z = u2f(0x3e570a3du);
    mat3_transform(off, off, M(hero + 0xc0));
    Mat4 r;
    euler_to_mat4_vu(r, V(OV(0x0017a6c0)));
    mat4_transform(off, off, r);
    vec3_add(V(lwu(g::PLAYER + 0x86c) + 0x10), V(lwu(g::PLAYER + 0x86c) + 0x10), off);
}

// orig: 0x002091d8 — rotate the hero about the point 0.6 above its origin.
void sub_002091d8(float x, float y, float z) {
    Vec4 pivot;
    vec_clear(pivot);
    pivot.z = u2f(0x3f19999au);
    sub_002090d0_v(pivot, x, y, z);
}

// orig: 0x00209540 — every `limit` calls, spawn a puff (particle 0x2d) at the hero's feet
// PLAYER+0x80/+0x84/+0x2f0 jittered by +-0.05, then restart the counter at rand_mod(n).
// The puff position's w is an uninitialised stack lane in the original (port: 0).
void sub_00209540(int32_t n, int32_t limit) {
    const uint32_t c = lhu(g::PLAYER + 0x904) + 1u;
    sh(g::PLAYER + 0x904, c);
    if (!(limit < static_cast<int16_t>(c))) return;
    const float lo = u2f(0xbd4ccccdu), hi = u2f(0x3d4ccccdu);
    Vec4 p{0.0f, 0.0f, 0.0f, 0.0f};
    p.x = lwc1(g::PLAYER + 0x80) + rand_range_f(lo, hi);
    p.y = lwc1(g::PLAYER + 0x84) + rand_range_f(lo, hi);
    p.z = lwc1(g::PLAYER + 0x2f0);
    const float size = rand_range_f(u2f(0x3ecccccdu), u2f(0x3f19999au));
    spawn_particle_2d_v(p, g::PLAYER + 0x2f0, 0xffffffffu, size, u2f(0x45a41000u));
    sh(g::PLAYER + 0x904, static_cast<uint32_t>(rand_mod(n)));
}

// orig: 0x00207a58 — scale animation: step the scale curve 0x1797d8 (index PLAYER+0x22dc,
// -1 ends it), scale the hero's and the held items' matrices by it, and scale the items'
// offsets from the hero pivot 0x13f420 (in the move-mode frame) accordingly.
void sub_00207a58() {
    if (lh(g::PLAYER + 0x22dc) == 0) return;
    const uint32_t c = lhu(g::PLAYER + 0x22dc) + 1u;
    sh(g::PLAYER + 0x22dc, c);
    const float s = lwc1(OV(0x001797d8) + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(c)) * 4));
    if (s == -1.0f) {
        sh(g::PLAYER + 0x22dc, 0);
        return;
    }
    vec3_scale(V(lwu(g::PLAYER + 0x2080) + 0xc0), V(lwu(g::PLAYER + 0x2080) + 0xc0), s);
    vec3_scale(V(lwu(g::PLAYER + 0x2080) + 0xd0), V(lwu(g::PLAYER + 0x2080) + 0xd0), s);
    if (lbu(g::PLAYER + 0x20a4) != 0) return;
    for (int32_t slot = 0; slot < 7; ++slot) {
        const uint32_t p = g::PLAYER + static_cast<uint32_t>(slot * 0x50);
        for (int32_t k = 0; k < 3; ++k) {
            const uint32_t m = lwu(p + (k == 1 ? 0x1094u : 0x1090u));  // k = 2 repeats +0x1090, as the original
            float ms = s;
            if (slot == 0 && lbu(g::PLAYER + 0x20a4) == 0 && 1.0f < s) ms = 1.0f;
            if (m == 0) continue;
            vec3_scale(V(m + 0xc0), V(m + 0xc0), ms);
            vec3_scale(V(m + 0xd0), V(m + 0xd0), ms);
            Vec4 d;
            vec3_sub(d, V(m + 0x10), V(0x0013f420));
            sub_00233b20_v(d, d, 0.0f);
            vec3_sub(V(m + 0x10), V(m + 0x10), d);
            vec3_scale(d, d, s);
            vec3_add(V(m + 0x10), V(m + 0x10), d);
        }
    }
}

// orig: 0x00209848 — every `limit` calls, kick up dirt (particle 0x23) ahead of the hero's
// feet (velocity PLAYER+0x100 x8), then restart the counter at rand_mod(n). The w lanes of the
// stack position/velocity are uninitialised in the original (port: 0).
void sub_00209848(int32_t n, int32_t limit) {
    const uint32_t c = lhu(g::PLAYER + 0x906) + 1u;
    sh(g::PLAYER + 0x906, c);
    if (!(limit < static_cast<int16_t>(c))) return;
    const float lo = u2f(0xbe19999au), hi = u2f(0x3e19999au);
    Vec4 pos{0.0f, 0.0f, 0.0f, 0.0f}, vel{0.0f, 0.0f, 0.0f, 0.0f};
    const float r1 = rand_range_f(lo, hi);
    pos.x = (lwc1(g::PLAYER + 0x80) + r1) + lwc1(g::PLAYER + 0x100) * 8.0f;
    const float r2 = rand_range_f(lo, hi);
    pos.z = lwc1(g::PLAYER + 0x2f0) + u2f(0x3dcccccdu);
    pos.y = (lwc1(g::PLAYER + 0x84) + r2) + lwc1(g::PLAYER + 0x104) * 8.0f;
    const float a = rand_angle();
    const float k = lwc1(0x0015ed6c);
    const float spd = rand_range_f(k * 0.0f, k * 3.0f);
    vel.x = cos_vu(a) * spd + lwc1(g::PLAYER + 0x100) * 0.75f;
    vel.y = sin_vu(a) * spd + lwc1(g::PLAYER + 0x104) * 0.75f;
    const float k2 = lwc1(0x0015ed6c);
    vel.z = rand_range_f(k2 * 3.0f, k2 * 6.5f);
    const int32_t life = rand_range(0x5a, 0x78);
    spawn_particle_23_v(pos, vel, lwc1(g::PLAYER + 0x22a4) < 0.25f ? 3 : 1, static_cast<uint32_t>(life));
    sh(g::PLAYER + 0x906, static_cast<uint32_t>(rand_mod(n)));
}

// orig: 0x002076a8 — hero colour effects: fade the tint PLAYER+0x22f8 down by 7 per channel,
// blink blue in the damage states (0x80/0x82 for 30 frames, 0x76 for 20 frames on levels 15/17),
// and refresh the held item's colour.
void sub_002076a8() {
    if (on_batalia()) { bat_call(bat_sub_2076a8_u); return; }  // (level 8's own: L8:0x00219ab0)
    if (on_pokitaru()) { pok_call(pok_sub_2076a8_u); return; }  // (level 11's own: L11:0x00238890)
    if (on_hoven()) { hov_call(hov_sub_2076a8_u); return; }  // (level 12's own: L12:0x00229f20)
    if (on_oltanis()) { olt_call(olt_sub_002076a8_u); return; }  // (level 14's own: L14:0x0021a628)
    if (on_quartu()) { qua_call(qua_sub_002076a8_u); return; }  // (level 15's own: L15:0x00200788)
    if (on_kalebo()) { kal_call(kal_sub_002076a8_u); return; }  // (level 16's own: L16:0x00205798)
    if (on_veldin_finale()) { vf_call(vf_sub_2076a8_u); return; }  // (level 18's own: L18:0x0020fe00)
    if (on_l1_family()) return sub_002076a8_l1();
    if (lw(g::PLAYER + 0x22f4) != 0) {
        const uint32_t v = lwu(g::PLAYER + 0x22f8);
        int32_t r = static_cast<int32_t>(v & 0xff) - 7;
        int32_t gg = static_cast<int32_t>((static_cast<int32_t>(v) >> 8) & 0xff) - 7;
        int32_t b = static_cast<int32_t>((static_cast<int32_t>(v) >> 16) & 0xff) - 7;
        if (!(-1 < b)) b = 0;
        if (!(-1 < r)) r = 0;
        if (!(-1 < gg)) gg = 0;
        const uint32_t a = v >> 24;
        moby_set_color_hi(lwu(g::PLAYER + 0x2080), r, gg, b);
        sw(g::PLAYER + 0x22f8, (a << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(gg) << 8) | static_cast<uint32_t>(r));
    }
    int32_t blink = 0;
    const int32_t st = lw(g::HERO_STATE);
    if (st == 0x80 || st == 0x82) blink = lw(g::PLAYER + 0x198) < frame_scale_i(0x1e) ? 1 : 0;
    const int32_t level = lw(0x0015ed84);
    if ((level == 0xf || level == 0x11) && lw(g::HERO_STATE) == 0x76) {
        if (lw(g::PLAYER + 0x198) < frame_scale_i(0x14)) blink = 1;
    }
    if (blink != 0) {
        uint32_t c[3];
        moby_get_color_hi_v(lwu(g::PLAYER + 0x2080), &c[0], &c[1], &c[2]);
        const int32_t t = lw(g::PLAYER + 0x198);
        if (t - ((t < 0 ? t + 3 : t) >> 2 << 2) < 3) {
            c[0] = c[1] = c[2] = 0;
        } else {
            c[1] = 0x90;
            c[2] = 0xf0;
            c[0] = 0x90;
        }
        moby_set_color_hi(lwu(g::PLAYER + 0x2080), c[0], c[1], c[2]);
        sub_002072c8();
    }
    const int32_t s = lw(g::PLAYER + 0x10b8);
    if (s < 0) return;
    if (lbu(0x0013e520 + static_cast<uint32_t>(s)) == 0) return;
    const uint32_t m = lwu(g::PLAYER + 0x1090);
    if (m == 0) return;
    sub_00207330(m, lwu(g::PLAYER + 0x2080));
}

// orig: 0x00209638 — every `limit` calls, two exhaust puffs (particle 0x2e) behind the hero's
// feet at +-45 degrees, moving against its facing; the counter restarts at rand_mod(n) (0 when
// n is 0). The puffs' w lanes are uninitialised stack lanes in the original (port: 0).
void sub_00209638(int32_t n, int32_t limit) {
    const uint32_t c = lhu(g::PLAYER + 0x908) + 1u;
    sh(g::PLAYER + 0x908, c);
    if (!(limit < static_cast<int16_t>(c))) return;
    const float k06 = u2f(0x3f19999au), k18 = 18.0f, k15 = 1.5f, q = u2f(0x3f490fd8u);
    for (int32_t side = 0; side < 2; ++side) {
        const float ang = side == 0 ? angle_add(lwc1(g::PLAYER + 0x98), q) : angle_sub(lwc1(g::PLAYER + 0x98), q);
        Vec4 vel{0.0f, 0.0f, 0.0f, 0.0f}, pos{0.0f, 0.0f, 0.0f, 0.0f};
        vel.x = cos_vu(ang) * (lwc1(0x0015ed6c) * k06);
        const float vy = sin_vu(ang) * (lwc1(0x0015ed6c) * k06);
        pos.z = lwc1(g::PLAYER + 0x2f0);
        vel.z = 0.0f;
        vel.y = vy;
        pos.x = lwc1(g::PLAYER + 0x80) + vel.x * k18;
        pos.y = lwc1(g::PLAYER + 0x84) + vy * k18;
        vel.x = vel.x - cos_vu(lwc1(g::PLAYER + 0x98)) * (lwc1(0x0015ed6c) * k15);
        vel.y = vel.y - sin_vu(lwc1(g::PLAYER + 0x98)) * (lwc1(0x0015ed6c) * k15);
        const float size = rand_range_f(u2f(0x3ecccccdu), 0.5f);
        spawn_particle_2e_v(pos, vel, g::PLAYER + 0x2f0, size, side == 0 ? 2.0f : -2.0f);
        sh(g::PLAYER + 0x908, n == 0 ? 0u : static_cast<uint32_t>(rand_mod(n)));
    }
}

// orig: 0x0020d6b0 — targeting score of moby `t` (lower is better): distance x (1 + yaw
// difference to `yaw`), +7 for class 500..540; *rejected = 1 when beyond max_dist or outside
// the yaw/pitch limits (if > 0). In states 1/0x1e the angles are measured from the camera.
float target_score(uint32_t t, int32_t* rejected, float yaw, float max_dist, float max_yaw, float max_pitch) {
    *rejected = 0;
    Vec4 eye = V(0x0013f3d0);
    if (lw(g::HERO_STATE) == 0x14) eye.z = lwc1(g::PLAYER + 0x2d8);
    const uint32_t tp = t + 0x10;
    const float dist = vec3_distance(eye, V(tp));
    if (max_dist < dist) *rejected = 1;
    const float a = atan2_f(lwc1(t + 0x10) - eye.x, lwc1(t + 0x14) - eye.y);
    float dyaw = angle_dist(a, yaw);
    const float d2 = vec2_distance(eye, V(tp));
    float limit_yaw = dyaw;
    float dpitch = angle_dist(atan2_f(d2, lwc1(t + 0x18) - eye.z), 0.0f);
    const int32_t st = lw(g::HERO_STATE);
    if (st == 1 || st == 0x1e) {
        constexpr OvConst<0x00166c80u> CAM{};
        const float cy = angle_dist(atan2_f(lwc1(t + 0x10) - lwc1(CAM + 0x140), lwc1(t + 0x14) - lwc1(CAM + 0x144)),
                                    lwc1(CAM + 0x158));
        const float cd = vec2_distance(V(CAM + 0x140), V(tp));
        dpitch = angle_dist(atan2_f(cd, lwc1(t + 0x18) - lwc1(CAM + 0x148)), -lwc1(CAM + 0x154));
        limit_yaw = dpitch;
        dyaw = cy + dpitch;
        if (dpitch < cy) limit_yaw = cy;
    }
    if (0.0f < max_yaw && max_yaw < limit_yaw) *rejected = 1;
    if (0.0f < max_pitch && max_pitch < dpitch) *rejected = 1;
    float score = dist + dyaw * dist;
    if (moby_in_range_class_500(t) != 0) score = score + 7.0f;
    return score;
}

// orig: 0x0020a9e8 — idle fidgets (state 0, base animation): four timers PLAYER+0x1060..
// re-roll random head/body angles (PLAYER+0x1024.. pairs, stride 0x10; the 4th alternates its
// sign) and copy the angles into effect records 13..16 of FX_PARAMS.
void sub_0020a9e8() {
    if (lw(g::HERO_STATE) != 0) return;
    if (lbu(lwu(g::PLAYER + 0x2080) + 0x53) != 0) return;
    const uint32_t T0 = g::PLAYER + 0x1060;
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t s1 = g::PLAYER + k * 0x10;
        const uint32_t timer = T0 + k * 4;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) {
            if (k == 0) {
                swc1(g::PLAYER + 0x1028, rand_range_signed_f(u2f(0x3e32b8c2u), u2f(0x3f060a92u)));
                const int32_t lo = frame_scale_i(0x46);
                sw(0x001403b0, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x96))));
            } else if (k == 1) {
                swc1(T0 - 0x28, rand_range_signed_f(u2f(0x3e32b8c2u), u2f(0x3f060a92u)));
                const int32_t lo = frame_scale_i(0x28);
                sw(T0 + 4, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x5a))));
            } else if (k == 2) {
                swc1(T0 - 0x18, rand_range_signed_f(u2f(0x3e860a92u), u2f(0x3f5f66f3u)));
                const int32_t lo = frame_scale_i(0x28);
                sw(T0 + 8, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x5a))));
            } else {
                swc1(s1 + 0x1024, rand_range_f(u2f(0xbdb2b8c2u), u2f(0x3f060a92u)));
                const float old = lwc1(s1 + 0x1028);
                const float n = rand_range_signed_f(u2f(0x3eb2b8c2u), u2f(0x3f75be0bu));
                swc1(s1 + 0x1028, n);
                if ((old <= 0.0f && n <= 0.0f) || (0.0f <= old && 0.0f <= n)) swc1(s1 + 0x1028, -lwc1(s1 + 0x1028));
                const int32_t lo = frame_scale_i(0x23);
                sw(timer, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x46))));
            }
        }
        const uint32_t e = g::FX_PARAMS + (k + 0xd) * 0xb0;
        const float b = lwc1(s1 + 0x1028), a = lwc1(s1 + 0x1024);
        swc1(e + 0x64, a);
        swc1(e + 0x68, b);
    }
}

// orig: 0x0020d8f8 — best target among the moby list 0x1aba00 (null-terminated): mobys with a
// non-zero pvar float and flag 0x1000, not rejected by target_score, with the lowest score.
uint32_t find_best_target(float max_dist, float yaw, float max_yaw, float max_pitch) {
    uint32_t best = 0;
    float best_score = u2f(0x4cbebc20u);
    for (uint32_t p = OV(0x001aba00);; p += 4) {
        const uint32_t m = lwu(p);
        if (m == 0) break;
        const uint32_t pv = moby_pvar_word0(m);
        if (pv == 0) continue;
        if (lwc1(pv) == 0.0f) continue;
        if ((lhu(m + 0x34) & 0x1000) == 0) continue;
        int32_t rejected;
        const float s = target_score(m, &rejected, yaw, max_dist, max_yaw, max_pitch);
        if (rejected != 0) continue;
        if (s < best_score) {
            best_score = s;
            best = m;
        }
    }
    return best;
}

// orig: 0x00206e00 — cheat input detector (when enabled by 0x15eea0 / 0x15ee20): moves that
// just began (frame counter PLAYER+0x198 == 15 or 60 frames) push a move code into the ring
// PLAYER+0x21c4 (index +0x21d4); a ring tail matching one of the 12 codes at 0x179700 toggles
// that cheat (0x15edb0[i], 0x15edc0[i] = 1) and shows message 0x4fbe/0x4fbf.
void cheat_input() {
    if (lw(0x0015eea0) == 0 && lw(0x0015ee20) == 0) return;
    int32_t code = 0;
    if (lw(g::PLAYER + 0x198) == frame_scale_i(0xf)) {
        const int32_t sub = lw(g::PLAYER + 0x208c);
        const int32_t st = lw(g::HERO_STATE);
        if (sub == 4) {
            if (st == 0xb) {
                const int32_t x = lw(g::PLAYER + 0x450);
                if (x == 0) code = lbu(0x0015edb4) != 0 ? 2 : 1;
                else if (x == 1) code = lbu(0x0015edb4) != 0 ? 1 : 2;
                else code = x == 3 ? 3 : 0;
            } else if (st == 0x11) {
                code = 9;
            } else if (st == 0xa) {
                code = 10;
            } else {
                code = st == 0xe ? 5 : 0;
            }
        } else if (sub == 6) {
            const int32_t s6 = lw(g::HERO_STATE);
            if (s6 == 0x14) code = 6;
            else if (s6 == 0x13) code = lw(g::PLAYER + 0xa60) + 0xc;
            else code = s6 == 0x15 ? 4 : 0;
        } else {
            code = lw(g::HERO_STATE) == 0x22 ? 0xb : 0;
        }
    } else if (lw(g::PLAYER + 0x198) == frame_scale_i(0x3c)) {
        const int32_t st = lw(g::HERO_STATE);
        code = st == 4 ? 7 : (st == 8 ? 8 : 0);
    }
    if (code == 0) return;
    const uint32_t RING = g::PLAYER + 0x21c4, IDX = g::PLAYER + 0x21d4, CODES = OV(0x00179700);
    sb(RING + lwu(IDX), static_cast<uint32_t>(code));
    sw(IDX, lwu(IDX) + 1u);
    for (uint32_t c = 0; c < 0xc; ++c) {
        const uint32_t seq = CODES + c * 0x10;
        int32_t len = 0;
        if (lbu(seq) != 0) {
            uint32_t p = seq;
            do {
                ++p;
                ++len;
            } while (lbu(p) != 0);
        }
        if (len == 0) continue;
        int32_t r = lw(IDX) - len;
        if (!(-1 < r)) r += 0x10;
        int32_t k = 0;
        if (lbu(RING + static_cast<uint32_t>(r)) != lbu(seq)) {
            k = -1;
        } else {
            for (;;) {
                ++r;
                ++k;
                if (!(r < 0x10)) r -= 0x10;
                if (!(k < len)) break;
                if (lbu(RING + static_cast<uint32_t>(r)) != lbu(seq + static_cast<uint32_t>(k))) {
                    k = -1;
                    break;
                }
            }
        }
        if (k == len) {
            const uint32_t flag = 0x0015edb0 + c;
            const uint32_t on = lbu(flag) == 0 ? 1u : 0u;
            sb(0x0015edc0 + c, 1);
            sb(flag, on);
            sub_00263d40(on != 0 ? 0x4fbe : 0x4fbf, -1);
            break;
        }
    }
    if (!(lw(IDX) < 0x10)) sw(IDX, 0);
}

// orig: 0x002056a8 — hero head look: clamp the look angles (0x17a680+0x8a8 = yaw/4 within
// +-0.148, +0x8a4 = pitch/2.8 within -0.175..0.297; tighter in hero anims 1..2), then run the
// random-event timer (sub_00205600): while an event runs (PLAYER+0xff0, 12 steps) attach the
// 7 head-anim nodes PLAYER+0xd30.. with their tables 0x17c040/0c0b0/0c120 and set their blend
// from 0x17c190[step]; when it ends detach them.
void hero_head_look() {
    constexpr OvConst<0x0017a680u> B{};
    const float yaw = lwc1(B + 0x278) * 0.25f;
    swc1(B + 0x8a8, yaw);
    if (u2f(0x3e17e9d8u) < yaw) {
        swc1(B + 0x8a8, u2f(0x3e17e9d8u));
    } else if (yaw < u2f(0xbe17e9d8u)) {
        swc1(B + 0x8a8, u2f(0xbe17e9d8u));
    }
    const float pitch = fpu_div(lwc1(B + 0x274), u2f(0x40333333u));
    swc1(B + 0x8a4, pitch);
    if (u2f(0x3e97e9d8u) < pitch) swc1(B + 0x8a4, u2f(0x3e97e9d8u));
    if (lwc1(B + 0x8a4) < u2f(0xbe32b8c2u)) swc1(B + 0x8a4, u2f(0xbe32b8c2u));
    if (static_cast<uint32_t>(lbu(lw(g::PLAYER + 0x2080) + 0x53) - 1) < 2u) {
        if (u2f(0x3d0efa35u) < lwc1(B + 0x8a8)) swc1(B + 0x8a8, u2f(0x3d0efa35u));
        if (lwc1(B + 0x8a8) < u2f(0xbd0efa35u)) swc1(B + 0x8a8, u2f(0xbd0efa35u));
        if (u2f(0x3dfa35ddu) < lwc1(B + 0x8a4)) swc1(B + 0x8a4, u2f(0x3dfa35ddu));
        if (lwc1(B + 0x8a4) < u2f(0xbdfa35ddu)) swc1(B + 0x8a4, u2f(0xbdfa35ddu));
    }
    sub_00205600();
    const uint32_t P = g::PLAYER;
    if (lw(P + 0xff0) == 0) return;
    const int32_t step = lw(P + 0xff0) + 1;
    sw(P + 0xff0, static_cast<uint32_t>(step));
    if (!(step < 12)) sw(P + 0xff0, 0);
    if (lw(P + 0xff0) == 0) {
        for (uint32_t i = 0; i < 7; ++i) {
            if (lbu(P + 0xd31 + i * 0x40) != 0) sub_0024f608(lw(P + 0x2080), P + 0xd30 + i * 0x40);
        }
        return;
    }
    for (uint32_t i = 0; i < 7; ++i) {
        const uint32_t n = P + 0xd30 + i * 0x40;
        if (lbu(n + 1) != 0) continue;
        sub_0024f590(lw(P + 0x2080), lw(OV(0x0017c1c0) + i * 4), n);
        sb(n + 3, 1);
        lq_sq(n + 0x10, OV(0x0017c040) + i * 16);
        lq_sq(n + 0x20, OV(0x0017c0b0) + i * 16);
        lq_sq(n + 0x30, OV(0x0017c120) + i * 16);
    }
    for (uint32_t i = 0; i < 7; ++i) swc1(P + 0xd3c + i * 0x40, lwc1(OV(0x0017c190) + static_cast<uint32_t>(lw(P + 0xff0)) * 4));
}

// orig: 0x00205168 — step one bone-offset node n (anim node of the moby picked by kind
// n+0xa2): spring its angles +0x40 (vel +0x50) to the targets +0x60 (mirrored when 0x15edb5)
// and its offset +0x70 (vel +0x80) to +0x90 using stiffness/damping +0xa4/+0xa8, attach it to
// the moby, write its quaternion +0x10, offset +0x30 and scale +0x20 (= +0xac). A node at rest
// with zero targets is detached instead. Targets are then cleared and the scale reset to 1.
void bone_node_update(uint32_t n) {
    const uint32_t moby = sub_00205110(lh(n + 0xa2));
    if (moby == 0) return;
    if (lbu(0x001413f4) == 0) {
        const int32_t k = lh(n + 0xa2);
        if (k == 4 || k == 2 || k == 3) return;
    } else if (lh(n + 0xa2) == 0) {
        return;
    }
    bool rest = lwc1(n + 0x60) == 0.0f && lwc1(n + 0x64) == 0.0f && lwc1(n + 0x68) == 0.0f;
    if (rest) rest = lwc1(n + 0x90) == 0.0f && lwc1(n + 0x94) == 0.0f && lwc1(n + 0x98) == 0.0f;
    const float eo = u2f(0x3b449ba6u), ea = u2f(0x3ba3d70au);
    if (rest) rest = abs_f(lwc1(n + 0x70)) < eo && abs_f(lwc1(n + 0x74)) < eo && abs_f(lwc1(n + 0x78)) < eo;
    if (rest) rest = lwc1(n + 0xac) == 1.0f;
    if (rest) rest = abs_f(lwc1(n + 0x40)) < ea && abs_f(lwc1(n + 0x44)) < ea && abs_f(lwc1(n + 0x48)) < ea;
    if (rest) {
        if (lbu(n + 1) != 0) sub_0024f608(moby, n);
    } else {
        if (lbu(0x0015edb5) != 0) {
            swc1(n + 0x60, -lwc1(n + 0x60));
            swc1(n + 0x68, -lwc1(n + 0x68));
            swc1(n + 0x74, -lwc1(n + 0x74));
        }
        for (uint32_t i = 0; i < 3; ++i) {
            const float t = lwc1(0x0015ed64);
            spring_angle(n + 0x40 + 4 * i, n + 0x50 + 4 * i, 2, lwc1(n + 0x60 + 4 * i), lwc1(n + 0xa4) * t, lwc1(n + 0xa8) * t, 0.0f);
        }
        for (uint32_t i = 0; i < 3; ++i) {
            spring_to(n + 0x70 + 4 * i, n + 0x80 + 4 * i, lwc1(n + 0x90 + 4 * i), lwc1(n + 0xa4), lwc1(n + 0xa8), 0.0f);
        }
        if (lbu(n + 1) == 0) sub_0024f590(moby, static_cast<uint32_t>(lh(n + 0xa0)), n);
        quat_from_euler(n + 0x10, n + 0x40);
        lq_sq(n + 0x30, n + 0x70);
        const float s = lwc1(n + 0xac);
        swc1(n + 0x28, s);
        swc1(n + 0x20, s);
        swc1(n + 0x24, s);
    }
    vec_clear(V(n + 0x90));
    vec_clear(V(n + 0x60));
    swc1(n + 0xac, 1.0f);
}

// orig: 0x002054e8 — step all 31 bone-offset nodes at FX_PARAMS (stride 0xb0).
void bone_nodes_update() {
    for (uint32_t n = g::FX_PARAMS; n < g::FX_PARAMS + 0x1550; n += 0xb0) bone_node_update(n);
}

// orig: 0x0020cd08 — find a rail near pos: scan the rail table (*0x15f70c, count *0x15f710,
// 0x20 each: centre, radius +0xc, path +0x10, loop flag +0x14), optionally skipping path
// `skip` or only accepting `only`; for each rail in reach take the closest path point
// (path_closest_point, range 12, step 10) and accept it if within 0.9 (0.3 in sub-states 0/1;
// +0.5 with PLAYER+0x1ca) in xy and 1.5 (10 with PLAYER+0x1ca) in z. Returns 1 with the rail.
int32_t find_rail_v(const Vec4& pos, uint32_t* path_out, Vec4& point, int32_t* idx, float* dist, int32_t* loop_out, uint32_t skip, uint32_t only) {
    if (on_kerwan()) return find_rail_l3_v(pos, path_out, point, idx, dist, loop_out, skip, only);  // (Kerwan's own build, L3:0x00205830)
    const Vec4 p = pos;
    for (int32_t i = 0; i < lw(OV(0x0015f710)); ++i) {
        const uint32_t e = static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20;
        if (skip != 0 && static_cast<uint32_t>(lw(e + 0x10)) == skip) continue;
        if (only != 0 && static_cast<uint32_t>(lw(e + 0x10)) != only) continue;
        if (lw(lw(e + 0x10)) == 0) continue;
        if (lwc1(e + 0xc) < vec3_distance(V(e), p)) continue;
        int32_t k = 0;
        float d = 0.0f;
        const uint32_t e2 = static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20;
        if (path_closest_point_v(lw(e2 + 0x10), p, point, &k, &d, lw(e2 + 0x14), 12.0f, 10.0f, 0.0f) == 0) continue;
        float rxy = u2f(0x3f666666u);
        float rz = 1.5f;
        if (static_cast<uint32_t>(lw(g::PLAYER + 0x208c)) < 2u) rxy = u2f(0x3e99999au);
        if (lh(g::PLAYER + 0x1ca) != 0) {
            rz = 10.0f;
            rxy = rxy + 0.5f;
        }
        const float dxy = vec2_distance(p, point);
        const float dz = abs_f(p.z - point.z);
        if (!(dxy < rxy)) continue;
        if (!(dz < rz)) continue;
        const uint32_t e3 = static_cast<uint32_t>(lw(OV(0x00166c00u - 0x74f4))) + static_cast<uint32_t>(i) * 0x20;
        *path_out = static_cast<uint32_t>(lw(e3 + 0x10));
        *idx = k;
        *dist = d;
        *loop_out = lw(static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20 + 0x14);
        return 1;
    }
    return 0;
}
// Kerwan's build of the rail search (find_rail_v above): the same scan of the rail table, the same closest path point, but
// one pair of thresholds: accepted within 1.7 in xy and 1.5 in z, whatever the sub-state (+0x208c) and PLAYER+0x1ca (Veldin's
// build: 0.9, 0.3 in sub-states 0 / 1, +0.5 and 10 in z with +0x1ca). The z distance is measured only once the xy one passes
// (no side effect: both are pure). Its only caller is Kerwan's rail grab check (L3:0x00205a68, Veldin's 0x20d330).
// orig: L3:0x00205830
int32_t find_rail_l3_v(const Vec4& pos, uint32_t* path_out, Vec4& point, int32_t* idx, float* dist, int32_t* loop_out, uint32_t skip, uint32_t only) {
    const Vec4 p = pos;
    for (int32_t i = 0; i < lw(OV(0x0015f710)); ++i) {
        const uint32_t e = static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20;
        if (skip != 0 && static_cast<uint32_t>(lw(e + 0x10)) == skip) continue;
        if (only != 0 && static_cast<uint32_t>(lw(e + 0x10)) != only) continue;
        if (lw(lw(e + 0x10)) == 0) continue;
        if (lwc1(e + 0xc) < vec3_distance(V(e), p)) continue;
        int32_t k = 0;
        float d = 0.0f;
        const uint32_t e2 = static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20;
        if (path_closest_point_v(lw(e2 + 0x10), p, point, &k, &d, lw(e2 + 0x14), 12.0f, 10.0f, 0.0f) == 0) continue;
        if (!(vec2_distance(p, point) < u2f(0x3fd9999au))) continue;
        if (!(abs_f(p.z - point.z) < 1.5f)) continue;
        const uint32_t e3 = static_cast<uint32_t>(lw(OV(0x00166c00u - 0x74f4))) + static_cast<uint32_t>(i) * 0x20;
        *path_out = static_cast<uint32_t>(lw(e3 + 0x10));
        *idx = k;
        *dist = d;
        *loop_out = lw(static_cast<uint32_t>(lw(OV(0x0015f70c))) + static_cast<uint32_t>(i) * 0x20 + 0x14);
        return 1;
    }
    return 0;
}
// orig: 0x0020cd08 (EE-address form)
int32_t find_rail(uint32_t pos, uint32_t path_out, uint32_t point, uint32_t idx, uint32_t dist, uint32_t loop_out, uint32_t skip, uint32_t only) {
    return find_rail_v(V(pos), static_cast<uint32_t*>(ee_ram_ptr(path_out)), V(point), static_cast<int32_t*>(ee_ram_ptr(idx)), static_cast<float*>(ee_ram_ptr(dist)),
                       static_cast<int32_t*>(ee_ram_ptr(loop_out)), skip, only);
}

// orig: 0x0020f160 — smoothed orientation: push src (orthonormalised) into the 16-matrix ring
// PLAYER+0x12f0 (head +0x21b8, fill +0x21bc), ease the window +0x21c0 towards 0xe/5/0xb, average
// the window's newest matrices minus the 1-2 most different, Euler angles to obj+0x40. The
// difference sums start from an uninitialised stack array in the original (0 here).
void hero_rot_smooth(uint32_t obj, uint32_t src) {
    const uint32_t P = g::PLAYER;
    const uint32_t RING = P + 0x12f0;
    auto slot = [&](int32_t back) { return RING + static_cast<uint32_t>(ee_rem(lw(P + 0x21b8) - (back - 15), 16)) * 0x30; };
    mat3_copy(M(RING + static_cast<uint32_t>(lw(P + 0x21b8)) * 0x30), M(src));
    mat3_normalize_columns(RING + static_cast<uint32_t>(lw(P + 0x21b8)) * 0x30);
    const int32_t head = ee_rem(lw(P + 0x21b8) + 1, 16);
    const int32_t fill = lw(P + 0x21bc) + 1;
    sw(P + 0x21bc, static_cast<uint32_t>(fill));
    sw(P + 0x21b8, static_cast<uint32_t>(head));
    if (!(fill < 17)) sw(P + 0x21bc, 16);
    Mat4 acc;
    vec_clear(acc.r[0]);
    vec_clear(acc.r[1]);
    vec_clear(acc.r[2]);
    acc.r[3] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    int32_t target = 0xb;
    if (lw(P + 0x208c) == 0x16) {
        target = 0xe;
    } else if (lw(g::HERO_STATE) == 2) {
        target = 5;
    }
    sw(OV(0x00166c00u - 0x756c), static_cast<uint32_t>(target));
    const int32_t win = lw(P + 0x21c0);
    if (win < lw(OV(0x00166c00u - 0x756c))) {
        sw(P + 0x21c0, static_cast<uint32_t>(win + 1));
    } else if (lw(OV(0x00166c00u - 0x756c)) < win) {
        sw(P + 0x21c0, static_cast<uint32_t>(win - 1));
    }
    int32_t n = lw(P + 0x21c0);
    if (!(1 < n)) n = 2;
    if (lw(P + 0x21bc) < n) return;
    float diff[16] = {};
    for (int32_t i = 0; i < n; ++i) {
        for (int32_t j = 0; j < n; ++j) {
            if (i == j) continue;
            diff[i] = diff[i] + mat3_abs_diff(slot(i), slot(j));
        }
    }
    float mx = 0.0f;
    for (int32_t i = 0; i < n; ++i) {
        if (mx < diff[i]) mx = diff[i];
    }
    int32_t outliers = 0;
    const float thr = mx * u2f(0x3e4ccccdu);
    for (int32_t i = 0; i < n; ++i) {
        if (thr < diff[i]) ++outliers;
    }
    int32_t used = 0;
    const float cut = mx * u2f(0x3e4ccccdu);
    for (int32_t i = 0; i < n; ++i) {
        if (cut < diff[i] && outliers > 0 && outliers < 3) continue;
        ++used;
        const uint32_t m = slot(i);
        for (uint32_t r = 0; r < 3; ++r) vec3_add(acc.r[r], acc.r[r], V(m + r * 16));
    }
    const float cnt = static_cast<float>(used);
    for (uint32_t r = 0; r < 3; ++r) vec3_scale(acc.r[r], acc.r[r], fpu_div(1.0f, cnt));
    mat_to_euler_v(acc, obj + 0x40);
}

// orig: 0x00209ec8 — hero smoke effects. Timer PLAYER+0x49e: puff a particle 0x19 sideways
// from the hero (next in 2..3 frames). Timer +0x49c: burst 57 kind-4 particles from the hero
// at random spherical velocities, alpha fading in with the state time +0x198 over 40 frames
// (next burst in 3..7 frames).
void hero_smoke_fx() {
    const uint32_t P = g::PLAYER;
    Vec4 vel{0.0f, 0.0f, 0.0f, 0.0f};  // one stack slot: part B's w lane is part A's (else uninitialised)
    if (timer_tick_i16(ee_ref<int16_t>(P + 0x49e)) != 0) {
        const Vec4 pos = V(P + 0x80);
        const float yaw = angle_add(lwc1(P + 0x98), u2f(0x3fc90fdbu));
        const float vm = lwc1(0x0015ed6c);
        const float sp = rand_range_signed_f(vm * u2f(0x3e4ccccdu), vm * u2f(0x3f333333u));
        vel.x = cos_vu(yaw) * sp;
        vel.y = sin_vu(yaw) * sp;
        vel.z = 0.0f;
        vel.w = lwc1(0x0015ed70) * -8.0f;
        spawn_particle_19_v(pos, vel, 0);
        sh(P + 0x49e, static_cast<uint32_t>(rand_range(2, 3)));
    }
    if (timer_tick_i16(ee_ref<int16_t>(P + 0x49c)) == 0) return;
    const int32_t t40 = frame_scale_i(0x28);
    const float span = static_cast<float>(frame_scale_i(0x28));
    float f = fpu_div(static_cast<float>(t40 - lw(P + 0x198)), span);
    if (1.0f < f) f = 1.0f;
    if (f < 0.0f) f = 0.0f;
    int32_t alpha = float_to_int(f * 240.0f);
    if (!(alpha < 0xa1)) alpha = 0xa0;
    if (!(0x1f < alpha)) alpha = 0x20;
    const uint32_t colour = static_cast<uint32_t>(alpha) << 24 | 0xc0f0u;
    const uint32_t life = static_cast<uint32_t>(frame_scale_i(0x28));
    for (int32_t k = 0x38; k >= 0; --k) {
        const int32_t c = float_to_int(fpu_div(rand_range_f(4200.0f, 7350.0f), 1000.0f));
        const int32_t d = float_to_int(fpu_div(rand_range_f(21000.0f, u2f(0x46f61801u)), 1000.0f));
        const float vm = lwc1(0x0015ed6c);
        const float r = rand_range_f(vm * u2f(0x3e19999au), vm * u2f(0x3f11eb85u));
        const Vec4 pos = V(P + 0x80);
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(vel, r, a, b);
        spawn_particle_4_v(pos, vel, colour, 0x70f0, life, static_cast<uint32_t>(c), static_cast<uint32_t>(d), 1);
    }
    const int32_t lo = frame_scale_i(3);
    const int32_t hi = frame_scale_i(7);
    sh(P + 0x49c, static_cast<uint32_t>(rand_range(lo, hi)));
}

// orig: 0x0020d330 — rail grab check (sub-state 2, or 4 without PLAYER+0x41e): find a rail near
// the hand point (sub_0020d2f8) and, when the hand is between 0.7 (1.2 with 0x13cae4 bit 0x80)
// below and 0.4 above it, latch it: point +0x5e0, path +0x5f0, segment +0x5f4, distance +0x5f8,
// loop flag +0x60c, found flag +0x5fc (cleared first). Skipped while +0x1e4 is set.
void hero_rail_grab_check() {
    const uint32_t P = g::PLAYER;
    sw(P + 0x5fc, 0);
    if (lh(P + 0x1e4) != 0) return;
    Vec4 hand;
    sub_0020d2f8_v(hand);
    const int32_t sub = lw(P + 0x208c);
    if (sub == 4) {
        if (lh(P + 0x41e) != 0) return;
    } else if (sub != 2) {
        return;
    }
    Vec4 point{0.0f, 0.0f, 0.0f, 0.0f};  // uninitialised stack in the original
    uint32_t path = 0;
    int32_t idx = 0, loop = 0;
    float dist = 0.0f;
    if (find_rail_v(hand, &path, point, &idx, &dist, &loop, 0, 0) == 0) return;
    const float dz = hand.z - point.z;
    const float lo = (lw(0x0013cae4) & 0x80) != 0 ? u2f(0xbf99999au) : u2f(0xbf333333u);
    if (!(lo < dz)) return;
    if (!(dz < u2f(0x3ecccccdu))) return;
    V(P + 0x5e0) = point;
    sw(P + 0x5fc, 1);
    sw(P + 0x5f0, path);
    sw(P + 0x5f4, static_cast<uint32_t>(idx));
    swc1(P + 0x5f8, dist);
    sw(P + 0x60c, static_cast<uint32_t>(loop));
}

namespace {
static bool near_point(float x, float y, float z, float r) {
    Vec4 c{0.0f, 0.0f, 0.0f, 0.0f};
    c.x = x;
    c.y = y;
    c.z = z;
    return !(r < vec3_distance(V(g::PLAYER + 0x80), c));
}
}  // namespace

// orig: 0x0020cf58 — rail landing check (needs the grind boots 0x13d4dd): in levels 16/14/6
// only near their rail spots; in sub-states 0..2 (or 4 with +0x41e/+0x1ca, or states 0x29/0x2a
// while +0x570/+0x5c4) find a rail at the hero's feet within 0.5 below and 0.57 above (any
// height with +0x1ca) and latch it: point +0x500, path +0x560, segment +0x564, distance +0x568,
// loop +0x584, flag +0x56c. The rail being left (+0x560 in state 0x29) is not taken again.
void hero_rail_land_check() {
    const uint32_t P = g::PLAYER;
    if (lbu(0x0013d4dd) == 0) return;
    sw(P + 0x56c, 0);
    if (lh(P + 0x1c8) != 0) return;
    const int32_t level = lw(0x0015ed84);
    if (level == 0x10) {
        if (!near_point(115.0f, 264.0f, 133.0f, 12.0f) && !near_point(257.0f, 301.0f, 128.0f, 12.0f)) return;
    } else if (level == 0xe) {
        if (!near_point(157.0f, 265.0f, 51.0f, 12.0f) && !near_point(162.0f, 341.0f, 48.0f, 12.0f) && !near_point(308.0f, 104.0f, 69.0f, 12.0f)) return;
    } else if (level == 6) {
        if (!near_point(177.0f, 304.0f, 143.0f, 14.0f)) return;
    }
    const int32_t sub = lw(P + 0x208c);
    bool go = sub == 2 || sub == 0 || sub == 1;
    if (!go) {
        const uint32_t st = static_cast<uint32_t>(lw(g::HERO_STATE));
        if (st - 0x29 < 2u && (lw(P + 0x570) != 0 || lw(P + 0x5c4) != 0)) {
            go = true;
        } else {
            if (sub != 4) return;
            if (lh(P + 0x41e) == 0 && lh(P + 0x1ca) == 0) return;
        }
    }
    const Vec4 pos = V(P + 0x80);
    Vec4 point{0.0f, 0.0f, 0.0f, 0.0f};  // uninitialised stack in the original
    uint32_t path = 0;
    int32_t idx = 0, loop = 0;
    float dist = 0.0f;
    if (find_rail_v(pos, &path, point, &idx, &dist, &loop, 0, 0) == 0) return;
    if (lw(g::HERO_STATE) == 0x29 && lw(P + 0x5c4) != 0 && path == static_cast<uint32_t>(lw(P + 0x560))) return;
    const float dz = lwc1(P + 0x88) - point.z;
    if (!(-0.5f < dz)) return;
    if (!(dz < u2f(0x3f11eb85u)) && lh(P + 0x1ca) == 0) return;
    V(P + 0x500) = point;
    sw(P + 0x56c, 1);
    sw(P + 0x560, path);
    sw(P + 0x564, static_cast<uint32_t>(idx));
    swc1(P + 0x568, dist);
    sw(P + 0x584, static_cast<uint32_t>(loop));
}

// orig: 0x0020a540 — hero head motion: head-bob blend 0x15ee14 eased towards 1.57/0.92; while
// aiming at a ledge (PLAYER+0x260) tilt the head by the slope +0x258 / height +0x25c; standing
// idle (state 0, anims 1..2 or 0x3a) glance around at random times (+0x1010 timer, angles
// +0x1004/+0x1008, blend +0x1014/+0x1018). Results go to the bone offsets in FX_PARAMS.
void hero_head_motion() {
    const uint32_t B = g::FX_PARAMS;
    const uint32_t P = g::PLAYER;
    if (lbu(0x0015edb3) != 0 && lw(P + 0x1184) != 0) swc1(B + 0xd0c, lwc1(OV(0x00166c00u - 0x7de8)));
    swc1(B + 0xc5c, lwc1(0x0015ee14));
    approach_f(0x0015ee14, lbu(0x0015edb1) != 0 ? u2f(0x3fc8f5c3u) : u2f(0x3f6b851fu), u2f(0x3d4ccccdu));
    const float deg = u2f(0x3c8efa35u);
    if (lw(P + 0x260) != 0 && u2f(0x3fb33333u) < lwc1(P + 0x258) && 1.0f < lwc1(P + 0x248)) {
        const float t = lwc1(0x0015ed64);
        fx_params_set_3(t * u2f(0x3c75c28fu), t * u2f(0x3e99999au));
        float a = lwc1(P + 0x258) * 1.5f * deg + u2f(0x3e449809u);
        if (u2f(0x3ef14639u) < a) a = u2f(0x3ef14639u);
        const float b = (1.0f - lwc1(P + 0x25c)) * 35.0f * deg;
        if (b < a) a = b;
        const float t2 = lwc1(0x0015ed64);
        swc1(B + 0x274, a);
        fx_params_set_1(t2 * u2f(0x3c03126fu), t2 * u2f(0x3e99999au));
        swc1(B + 0x114, lwc1(B + 0x274) * u2f(0x3f333333u));
        return;
    }
    if (lw(g::HERO_STATE) != 0) return;
    const uint32_t anim = lbu(lw(P + 0x2080) + 0x53);
    if (!(anim < 3) && anim != 0x3a) return;
    if (lw(P + 0x2274) != 0) return;
    if (anim - 1 < 2u) {
        const int32_t lo = frame_scale_i(0x28);
        const int32_t hi = frame_scale_i(0x46);
        sw(P + 0x1010, static_cast<uint32_t>(rand_range(lo, hi)));
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x1010))) != 0) {
        swc1(P + 0x1004, -rand_range_f(u2f(0xbe20d97cu), u2f(0x3edf66f3u)));
        swc1(P + 0x1008, rand_range_signed_f(u2f(0x3eb2b8c2u), u2f(0x3f7eadaeu)));
        const float r = rand_range_f(0.0f, 1.0f);
        swc1(P + 0x1014, lerp_f(u2f(0x3c75c28fu), u2f(0x3be56042u), r));
        swc1(P + 0x1018, lerp_f(u2f(0x3e947ae1u), u2f(0x3e99999au), r));
        const int32_t lo = frame_scale_i(0x5a);
        const int32_t hi = frame_scale_i(0xc8);
        sw(P + 0x1010, static_cast<uint32_t>(rand_range(lo, hi)));
        const int32_t extra = float_to_int(static_cast<float>(frame_scale_i(0x50)) * r);
        sw(P + 0x1010, static_cast<uint32_t>(lw(P + 0x1010) + extra));
    }
    if (static_cast<uint32_t>(lbu(lw(P + 0x2080) + 0x53)) - 1 < 2u) {
        const float yaw = lwc1(P + 0x1008);
        if (u2f(0x3eb2b8c2u) < yaw) {
            swc1(P + 0x1008, u2f(0x3eb2b8c2u));
        } else if (yaw < u2f(0xbeb2b8c2u)) {
            swc1(P + 0x1008, u2f(0xbeb2b8c2u));
        }
        if (u2f(0x3e860a92u) < lwc1(P + 0x1004)) swc1(P + 0x1004, u2f(0x3e860a92u));
        if (lwc1(P + 0x1004) < 0.0f) swc1(P + 0x1004, 0.0f);
    }
    fx_params_set_3(lwc1(P + 0x1014), lwc1(P + 0x1018));
    const float t = lwc1(0x0015ed64);
    swc1(B + 0x274, lwc1(P + 0x1004));
    swc1(B + 0x278, lwc1(P + 0x1008));
    fx_params_set_1(t * u2f(0x3c03126fu), t * u2f(0x3e99999au));
    swc1(B + 0x118, lwc1(B + 0x278) * u2f(0x3f051eb8u));
    swc1(B + 0x114, lwc1(B + 0x274) * u2f(0x3f0ccccdu));
}

// orig: 0x002059d8 — Clank glow and blink: ease 0x15ee18 to 1.8/1.0; for the backpack moby (the
// hero moby in modes 1/2, else PLAYER+0x1184 unless +0x22d8) pulse its colour +0x90 with a
// 110-frame sine (red base by +0x22a8), shifted towards orange while +0x1ee counts; in mode 0
// run the blink: timer +0xffc arms a 22-step blink (+0xffe) that drives the 4 eye anim nodes
// PLAYER+0xef0.. (tables 0x17c1e0/0c220/0c260, blend 0x17c2a0[step]).
void clank_glow_blink() {
    const uint32_t P = g::PLAYER;
    approach_f(0x0015ee18, lbu(0x0015edb3) != 0 ? u2f(0x3fe66666u) : 1.0f, u2f(0x3d4ccccdu));
    uint32_t m;
    if (static_cast<uint32_t>(lbu(P + 0x20a4)) - 1 < 2u) {
        m = lw(P + 0x2080);
    } else {
        if (lh(P + 0x22d8) != 0) return;
        m = lw(P + 0x1184);
    }
    if (m == 0) return;
    sh(m + 0x34, lhu(m + 0x34) | 0x10);
    int32_t r = lw(P + 0x22a8) == 1 ? 0x88 : 0x38;
    const int32_t period = frame_scale_i(0x6e);
    const float fp = int_to_float(period);
    constexpr float PI = 3.14159274f;
    float a = fpu_div(static_cast<float>(ee_rem(lw(OV(0x0015f5cc)), period)), fp);
    a = (a + a) * PI + -PI;
    const float s = sin_vu(a);
    float k = s * 24.0f;
    const int32_t dr = float_to_int(k) + 0xc;
    r += dr;
    const int32_t g0 = float_to_int(s * 48.0f);
    const int32_t dg = g0 + 0x18;
    int32_t g = g0 + 0xa0;
    const int32_t b0 = float_to_int(k);
    const int32_t db = b0 + 0xc;
    int32_t b = b0 + 0x4c;
    if (lh(P + 0x1ee) != 0) {
        k = 1.0f;
        const int32_t in = frame_scale_i(5);
        const int32_t out = frame_scale_i(0x14);
        const int32_t cnt = lh(P + 0x1ee);
        if (frame_scale_i(0x2d) - in < cnt) k = fpu_div(int_to_float(frame_scale_i(0x2d) - lh(P + 0x1ee)), int_to_float(in));
        if (lh(P + 0x1ee) < out) k = fpu_div(int_to_float(lh(P + 0x1ee)), int_to_float(out));
        r -= float_to_int(static_cast<float>(dr) * k);
        g -= float_to_int(static_cast<float>(dg) * k);
        b -= float_to_int(static_cast<float>(db) * k);
        r += float_to_int(k * 112.0f);
        g -= float_to_int(k * 72.0f);
    }
    sw(m + 0x90, 0x80000000u | static_cast<uint32_t>(b) << 16 | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(r));
    if (lbu(P + 0x20a4) != 0) return;
    if (timer_tick_i16(ee_ref<int16_t>(P + 0xffc)) != 0 && lbu(m + 0x53) == 1) {
        const int32_t lo = frame_scale_i(0x32);
        const int32_t hi = frame_scale_i(0xc8);
        const int32_t t = rand_range(lo, hi);
        sh(P + 0xffe, 1);
        sh(P + 0xffc, static_cast<uint32_t>(t));
    }
    if (lh(P + 0xffe) == 0) return;
    const uint32_t step = lhu(P + 0xffe) + 1u;
    sh(P + 0xffe, step);
    if (!(static_cast<int16_t>(step) < 0x16)) sh(P + 0xffe, 0);
    if (lh(P + 0xffe) == 0) {
        for (uint32_t i = 0; i < 4; ++i) {
            if (lbu(P + 0xef1 + i * 0x40) != 0) sub_0024f608(m, P + 0xef0 + i * 0x40);
        }
        return;
    }
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t n = P + 0xef0 + i * 0x40;
        if (lbu(n + 1) != 0) continue;
        sub_0024f590(m, lw(OV(0x00166c00u - 0x7508) + i * 4), n);
        sb(n + 3, 1);
        lq_sq(n + 0x10, OV(0x0017c1e0) + i * 16);
        lq_sq(n + 0x20, OV(0x0017c220) + i * 16);
        lq_sq(n + 0x30, OV(0x0017c260) + i * 16);
        vec3_scale(V(n + 0x30), V(n + 0x30), lwc1(m + 0x2c));
    }
    for (uint32_t i = 0; i < 4; ++i) swc1(P + 0xefc + i * 0x40, lwc1(OV(0x0017c2a0) + static_cast<uint32_t>(lh(P + 0xffe)) * 4));
}

// orig: 0x00208a10 — push the hero state into the hero moby: flags (0x20a5 ? set : clear),
// sub_00232fe8, position +0x10 = PLAYER+0x80, angles +0x40 = PLAYER+0x90, rotation rows from
// the PLAYER matrix, then its transform; unless +0x1cc, aim its two look targets at points
// +0x224 and +0x234+0.05 up the hero (sub_0024eed8 with radius +0x234).
void hero_moby_sync() {
    const uint32_t P = g::PLAYER;
    if (lbu(P + 0x20a5) != 0) {
        hero_mobys_set_flag1();
    } else {
        hero_mobys_clear_flag1();
    }
    sub_00232fe8();
    sb(lw(P + 0x2080) + 0xa4, 0xff);
    const uint32_t hm = lw(P + 0x2080);
    lq_sq(hm + 0x10, P + 0x80);
    lq_sq(hm + 0x40, P + 0x90);
    mem_copy(hm + 0xc0, P, 0x30);
    moby_update_transform(lw(P + 0x2080));
    if (lw(P + 0x1cc) != 0) return;
    if (lw(lw(P + 0x2080) + 0x94) == 0) return;
    Vec4 t = V(P + 0x80);
    sub_002336e8_v(t, t, lwc1(P + 0x224));
    sub_0024eed8_v(lw(P + 0x2080), 0, t, lwc1(P + 0x234), lwc1(P + 0x220) - lwc1(P + 0x224));
    const float h = lwc1(P + 0x234) + u2f(0x3d4ccccdu);
    t = V(P + 0x80);
    sub_002336e8_v(t, t, h);
    sub_0024eed8_v(lw(P + 0x2080), 1, t, lwc1(P + 0x234), lwc1(P + 0x220) - h);
}

// orig: 0x00208820 — per-frame hero moby update: flags (0x20a5 ? sub_00232fe8 : sub_002330d0),
// position/angles/rotation from the hero state with the state swings (sub_002078a8/0x2079e8),
// its transform, the two look targets (at +0x224, and +0x234+0.05 (+0.15 in sub-state 4, or
// +0x224 in the sub_0020d568 states); radius >= 0.05), and the mirror flag from 0x15edb5.
void hero_moby_update() {
    const uint32_t P = g::PLAYER;
    sb(lw(P + 0x2080) + 0xa4, 0xff);
    if (lbu(P + 0x20a5) != 0) {
        sub_00232fe8();
    } else {
        sub_002330d0();
    }
    lq_sq(lw(P + 0x2080) + 0x10, P + 0x80);
    sub_002078a8();
    lq_sq(lw(P + 0x2080) + 0x40, P + 0x90);
    sub_002079e8();
    mem_copy(lw(P + 0x2080) + 0xc0, P, 0x30);
    moby_update_transform(lw(P + 0x2080));
    if (lw(P + 0x1cc) == 0 && lw(lw(P + 0x2080) + 0x94) != 0) {
        const float lo = u2f(0x3d4ccccdu);
        Vec4 t = V(P + 0x80);
        sub_002336e8_v(t, t, lwc1(P + 0x224));
        float b = lwc1(P + 0x220) - lwc1(P + 0x224);
        if (b < lo) b = lo;
        sub_0024eed8_v(lw(P + 0x2080), 0, t, lwc1(P + 0x234), b);
        float h = lwc1(P + 0x234) + lo;
        if (lw(P + 0x208c) == 4) h = h + u2f(0x3e19999au);
        if (sub_0020d568() != 0) h = lwc1(P + 0x224);
        t = V(P + 0x80);
        sub_002336e8_v(t, t, h);
        b = lwc1(P + 0x220) - h;
        if (b < lo) b = lo;
        sub_0024eed8_v(lw(P + 0x2080), 1, t, lwc1(P + 0x234), b);
    }
    const uint32_t hm = lw(P + 0x2080);
    if (lh(hm + 0xa6) != 0) return;
    const uint32_t f = lhu(hm + 0x34);
    sh(hm + 0x34, lbu(0x0015edb5) != 0 ? (f | 0x8000u) : (f & 0x7fffu));
}

// orig: 0x00209ca8 — hero landing effects: optionally a shock ring (moby 0x307 at the feet,
// scale 2.25), `puffs` dust particles (kind 0x2d) around the feet and `sparks` debris particles
// (kind 0x23) thrown out at random. As in the original the debris offset uses the x component
// for both x and y. The scratch vectors' w lanes carry over (uninitialised at first).
void hero_land_fx(int32_t puffs, int32_t sparks, int32_t ring) {
    const uint32_t P = g::PLAYER;
    Vec4 a{0.0f, 0.0f, 0.0f, 0.0f};
    Vec4 b{0.0f, 0.0f, 0.0f, 0.0f};
    if (ring != 0) {
        a = V(P + 0x80);
        a.z = lwc1(P + 0x2f0);
        spawn_moby_307_v(a, 2.25f);
    }
    const float lo = u2f(0xbe99999au), hi = u2f(0x3e99999au);
    for (int32_t n = puffs; n > 0; --n) {
        a.x = lwc1(P + 0x80) + rand_range_f(lo, hi);
        const float dy = rand_range_f(lo, hi);
        a.z = lwc1(P + 0x2f0);
        a.y = lwc1(P + 0x84) + dy;
        spawn_particle_2d_v(a, P + 0x2f0, 0xffffffffu, rand_range_f(hi, u2f(0x3f19999au)), u2f(0x45a41000u));
    }
    for (int32_t n = sparks; n > 0; --n) {
        const float ang = rand_angle();
        const float vm = lwc1(0x0015ed6c);
        const float sp = rand_range_f(vm * 0.0f, vm * 3.0f);
        a.x = cos_vu(ang) * sp;
        a.y = sin_vu(ang) * sp;
        const float vm2 = lwc1(0x0015ed6c);
        a.z = rand_range_f(vm2 * 3.0f, vm2 * 8.0f);
        const float off = a.x * 8.0f;
        b.y = lwc1(P + 0x84) + off;
        b.x = lwc1(P + 0x80) + off;
        b.z = lwc1(P + 0x2f0) - rand_range_f(0.0f, u2f(0x3e4ccccdu));
        const int32_t kind = rand_mod(2);
        const int32_t life = rand_range(0x5a, 0x78);
        spawn_particle_23_v(b, a, kind, static_cast<uint32_t>(life));
    }
}


// orig: 0x002092c8 — encode the current local pose of a moby (default: the hero moby 0x1413d0;
// moby_pose_eval) as a key frame at out for the joints of list (+4.., ended by a negative id):
// an identity key, the s16 quaternions plus pad identity keys, scale records (joints not at 1.0)
// and translation records (joints off the bind pose of cls +0x18), padded to 16 bytes.
void moby_pose_encode(uint32_t list, uint32_t out, uint32_t cls, int32_t pad, uint32_t moby) {
    constexpr uint32_t S = 0x70000000u;
    if (moby == 0) moby = lwu(0x001413d0);
    moby_pose_eval(moby);
    uint32_t p = out + 0x18;
    sd(out + 0x10, 0x7fff000000000000ull);
    uint32_t keys = 1;
    if (lw(list + 4) >= 0) {
        uint32_t e = list + 4;
        do {
            const uint32_t id = lwu(e);
            ++keys;
            e += 4;
            sd(p, vec4_to_s16x4(V(S + id * 64), 32768.0f));
            p += 8;
        } while (lw(e) >= 0);
    }
    if (pad > 0) {
        int32_t c = pad;
        do {
            sd(p, 0x7fff000000000000ull);
            p += 8;
            ++keys;
        } while (--c != 0);
    }
    const uint32_t key_bytes = keys * 8;
    constexpr uint64_t XYZ = 0x0000ffffffffffffull;
    uint32_t nscale = 0, idx = 1;
    if (lw(list + 4) >= 0) {
        uint32_t e = list + 4;
        do {
            const uint64_t v = vec4_to_s16x4(V(S + 0x10 + lwu(e) * 64), 4096.0f) & XYZ;
            e += 4;
            if (v != 0x0000100010001000ull) {
                sd(p, v);
                ++nscale;
                sh(p + 6, idx | 0x8000u);
                p += 8;
            }
            ++idx;
        } while (lw(e) >= 0);
    }
    const uint32_t rot_end = keys + nscale;
    uint32_t npos = 0;
    idx = 1;
    if (lw(list + 4) >= 0) {
        uint32_t e = list + 4;
        do {
            const uint64_t v = vec4_to_s16x4(V(S + 0x20 + lwu(e) * 64), 1.0f) & XYZ;
            const uint64_t b = vec4_to_s16x4(V(lwu(cls + 0x18) + idx * 16), 1.0f) & XYZ;
            e += 4;
            if (v != b) {
                sd(p, v);
                ++npos;
                sh(p + 6, idx);
                p += 8;
            }
            ++idx;
        } while (lw(e) >= 0);
    }
    if ((p & 0xf) != 0) {
        sd(p, 0);
        p += 8;
    }
    sh(out + 0xe, npos);
    sh(out + 0xa, nscale);
    sh(out + 8, key_bytes);
    sh(out + 6, static_cast<uint32_t>(static_cast<int32_t>(p - 0x10 - out) >> 4));
    sh(out + 0xc, rot_end * 8);
    sh(out, 0);
    sh(out + 2, 0);
    sh(out + 4, 0);
}


// orig: 0x0020dd20 — update the two animation layers of the hero (records at P+0xd00 / +0xd04):
// fade a layer out (+8 toward 0, released with sub_0024f0e8 at 0) when flagged off (+0x34) or
// not wanted (P+0x20ab / +0x20af), create it (sub_0024f028) when wanted, then follow the hero
// moby's animation (sets via sub_0020d5b8 / sub_0020d5d0, cross-fades +0x24) and step it.
void hero_layers_update() {
    const uint32_t P = g::PLAYER;
    const float k01 = u2f(0x3dcccccdu);
    auto frames_to = [](uint32_t r, uint32_t i) {
        for (uint32_t k = 0; k < 2; ++k) {
            if (lbu(r + 0x22 + k) < 0x17) {
                sb(r + 6 + k, 1);
                sw(r + 0x38 + k * 4, OV(0x0017ca80u) + i * 0x800 + k * 0x400);
            } else {
                sb(r + 6 + k, 0);
            }
        }
    };
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t L = 0x00140050u + i * 4;
        const uint32_t cur = lwu(L);
        if (cur != 0 && lw(cur + 0x34) != 0) {
            if (approach_f(cur + 8, 0.0f, lwc1(0x0015ed60) * k01) == 0.0f) sub_0024f0e8(lwu(P + 0x2080), L);
        } else {
            const uint32_t want_flag = lbu(P + 0x20ab);
            bool want = want_flag != 0 && (i == 0 || want_flag == 2);
            if (lbu(P + 0x20af) != 0) {
                want = false;
                if (lwu(L) != 0) sw(lwu(L) + 0x34, 1);
            }
            if (want) {
                if (lwu(L) == 0) {
                    const uint32_t r = sub_0024f028(lwu(P + 0x2080), lwu(OV(0x0015f6a0) + i * 4));
                    sw(L, r);
                    sw(r + 8, 0);
                    sw(lwu(L) + 0x18, lwu(OV(0x00197300) + static_cast<uint32_t>(lbu(OV(0x00197bc0) + i + 1)) * 4));
                }
            } else {
                if (lwu(L) == 0) continue;
                if (approach_f(lwu(L) + 8, 0.0f, lwc1(0x0015ed60) * k01) == 0.0f) sub_0024f0e8(lwu(P + 0x2080), L);
            }
        }
        const uint32_t r = lwu(L);
        if (r == 0) continue;
        uint32_t hm = lwu(P + 0x2080);
        const bool switching = lbu(hm + 0x52) != lbu(hm + 0x53);
        if (lwu(P + 0xd08) != 0 && lwu(P + 0xd14) == 0) {
            approach_f(r + 8, 0.0f, lwc1(0x0015ed60) * k01);
        } else if (lw(r + 0x34) == 0) {
            approach_f(r + 8, 1.0f, lwc1(0x0015ed60) * k01);
        }
        uint32_t mix = 0;
        if (switching && lwu(P + 0xa9c) != 0) {
            const uint32_t a = sub_002050b8(lbu(r + 0x23));
            if (a != sub_002050b8(lbu(lwu(P + 0x2080) + 0x53))) mix = lbu(r + 0x22) != lbu(r + 0x23) ? 1u : 0u;
        }
        sh(r + 0x30, mix);
        hm = lwu(P + 0x2080);
        float fade = 0.0f;
        const int32_t kind = sub_0020d5d0(lbu(hm + 0x53), &fade);
        if (kind != 0 || lbu(r + 0x32) != 0) {
            if (!(fade == 0.0f)) {
                constexpr OvConst<0x0017a680u> T{};
                const float base = lwc1(0x0015ed64);
                swc1(T + 0x5e4, fade);
                swc1(T + 0x694, fade);
                const float a = base * u2f(0x3d4ccccdu), b = base * u2f(0x3e99999au);
                swc1(T + 0x624, a);
                swc1(T + 0x628, b);
                swc1(T + 0x6d4, a);
                swc1(T + 0x6d8, b);
            }
            if (kind != 0) {
                if (!(lbu(r + 0x32) != 0 && lbu(r + 0x23) == 0)) {
                    const uint32_t to = lbu(r + 0x23);
                    if (lbu(r + 0x22) == to || u2f(0x3f000000u) < lwc1(r + 0x24)) {
                        sb(r + 0x22, to);
                        sb(r + 0x20, lbu(r + 0x21));
                    }
                    sw(r + 0x24, 0);
                    sb(r + 0x23, 0);
                    sb(r + 0x21, 0);
                    swc1(r + 0x28, 1.0f);
                    const float n = int_to_float(frame_scale_i(0xf));
                    sb(r + 0x32, 1);
                    sb(r + 0x33, 0);
                    swc1(r + 0x2c, fpu_div(1.0f, n));
                }
                swc1(r + 0x28, 1.0f);
            } else if (lbu(r + 0x33) == 0) {
                const uint32_t h = lwu(P + 0x2080);
                sb(r + 0x23, sub_0020d5b8(h, lbu(h + 0x53)));
                sb(r + 0x21, 0);
                sb(r + 0x33, 1);
                sw(r + 0x24, 0);
                sw(r + 0x28, 0);
            } else {
                float t = lwc1(r + 0x24) + lwc1(0x0015ed60) * k01;
                swc1(r + 0x24, t);
                if (1.0f < t) swc1(r + 0x24, 1.0f);
                if (lwc1(r + 0x24) == 1.0f) sb(r + 0x32, 0);
            }
            frames_to(r, i);
            moby_anim_step(r);
            continue;
        }
        if (lh(r + 0x30) == 0) {
            for (uint32_t k = 0; k < 2; ++k) {
                if (switching && k != 1) continue;
                const uint32_t h = lwu(P + 0x2080);
                sb(r + 0x22 + k, sub_0020d5b8(h, lbu(h + 0x52 + k)));
            }
        }
        if (lh(r + 0x30) != 0) {
            const float t = lwc1(r + 0x24) + lwc1(0x0015ed60) * u2f(0x3e19999au);
            swc1(r + 0x24, t);
            if (1.0f <= t) {
                sb(r + 0x22, lbu(r + 0x23));
                sb(r + 0x20, lbu(r + 0x21));
                sh(r + 0x30, 0);
            }
        } else {
            if (!switching) sb(r + 0x20, lbu(lwu(P + 0x2080) + 0x50));
            sb(r + 0x21, lbu(lwu(P + 0x2080) + 0x51));
            swc1(r + 0x24, lwc1(lwu(P + 0x2080) + 0x54));
        }
        frames_to(r, i);
        moby_anim_step(r);
    }
}


// orig: 0x00209240 — world matrices of the hero's (or moby's) nine tracked joint chains (ids in
// the table 0x1e7c10) into P+0xac0.
void hero_chain_world(uint32_t moby) {
    if (moby == 0) moby = lwu(0x001413d0);
    moby_chain_world(moby, 9, OV(0x001e7c10), 0x0013fe10);
}


// orig: 0x0020e360 — when the hero holds a moby (P+0x20a8 / +0x20aa, not riding) and asked to
// (a0 == 0, a1 != 0): create the hand animation layers P+0xd08 (and +0xd0c when +0x20ab is 2)
// playing the set of hero slot 0's entry (0x179ac0 table +0x28), fading in from 0.
void hero_layers_start(int32_t off, int32_t on) {
    const uint32_t P = g::PLAYER;
    if (lbu(P + 0x20a4) != 0 || lbu(P + 0x20a8) == 0 || lbu(P + 0x20aa) == 0 || off != 0 || on == 0) return;
    const uint32_t set = lwu(OV(0x00179ac0u) + static_cast<uint32_t>(sub_0020d498(0)) * 0x4c + 0x28);
    if (set == 0xffffffffu) return;
    for (uint32_t s = 0; s < 2; ++s) {
        if (lwu(P + 0xd08 + s * 4) != 0) continue;
        if (s == 1 && lbu(P + 0x20ab) != 2) continue;
        const uint32_t r = sub_0024f028(lwu(P + 0x2080), lwu(OV(0x0015f6a0) + s * 4));
        sw(P + 0xd08 + s * 4, r);
        if (r == 0) continue;
        sw(P + 0xd14, 0);
        sw(r + 8, 0);
        moby_anim_rec_start(lwu(P + 0x2080), r, set, 0, frame_scale_i(0xf), 1);
        swc1(r + 0x28, 1.0f);
    }
}


// orig: 0x0020da68 — the hero's extra animation layer P+0xd10: wanted (with its set and target
// weight) while aiming a target moby (+0x2274), with +0x1dc, in mode +0x22a8 == 1, in sub-state
// 0x11 (weight from +0x22a0) or holding in slot-0 kinds 0xf / 0x10 / 0x13; fades in / out
// (dt / 20 per frame) and restarts on a set change.
void hero_extra_layer_update() {
    const uint32_t P = g::PLAYER;
    uint32_t set = 0;
    bool want = false;
    float weight = 1.0f;
    const uint32_t tgt = lwu(P + 0x2274);
    if (tgt != 0) {
        want = true;
        const uint32_t pv = moby_pvar_word0(tgt);
        if (pv != 0) {
            set = 0x79;
            const uint32_t k = lbu(pv + 0x3c);
            if (k != 0) {
                set = static_cast<uint32_t>(sub_0020da10(static_cast<int32_t>(k)));
                if (lw(P + 0x208c) == 1) weight = u2f(0x3f333333u);
            }
        } else {
            set = 0x79;
        }
    }
    if (lh(P + 0x1dc) != 0) {
        want = true;
        set = static_cast<uint32_t>(sub_0020da10(lh(P + 0x22ce)));
    }
    if (lw(P + 0x22a8) == 1) {
        want = true;
        const uint32_t cur = lbu(lwu(P + 0x2080) + 0x53);
        if ((cur == 0x54 || cur == 0x82 || cur == 0x5a) && lwu(P + 0xa9c) == 0) want = false;
        weight = u2f(0x3f4ccccdu);
        set = 0x7b;
    }
    if (lw(P + 0x208c) == 0x11 && lbu(0x0013d4c6) == 0) {
        want = true;
        set = 0x78;
        const float f = 1.0f - fpu_div(int_to_float(lw(P + 0x22a0)), int_to_float(10000));
        weight = f + 0.25f;
        if (1.0f < weight) weight = 1.0f;
    }
    const int32_t kind = sub_0020d498(0);
    if ((static_cast<uint32_t>(kind - 0xf) < 2u || kind == 0x13) && lbu(P + 0x20a8) != 0) {
        weight = u2f(0x3f4ccccdu);
        want = true;
        set = 0x76;
    }
    const float step = lwc1(0x0015ed60) * u2f(0x3d4ccccdu);
    if (want) {
        if (lwu(P + 0xd10) == 0) {
            const uint32_t r = sub_0024f028(lwu(P + 0x2080), 0x18);
            sw(P + 0xd10, r);
            sw(r + 8, 0);
        }
    } else {
        if (lwu(P + 0xd10) == 0) return;
        if (approach_f(lwu(P + 0xd10) + 8, 0.0f, step) == 0.0f) sub_0024f0e8(lwu(P + 0x2080), P + 0xd10);
    }
    if (lwu(P + 0xd10) == 0 || !want) return;
    approach_f(lwu(P + 0xd10) + 8, weight, step);
    const uint32_t r = lwu(P + 0xd10);
    if (lbu(r + 0x23) != set) moby_anim_rec_start(lwu(P + 0x2080), r, set, 0, frame_scale_i(8), 1);
    else moby_anim_step(r);
}


// orig: 0x00209a40 — n smoke puffs (particle 0x22) from the hero: around its position (mode 0) or
// at its hand / foot joints (modes 1 / 2, alternating), drifting with its velocity (P+0x100,
// scaled 1.15 in state 0x34, else 0.7) plus a random spread.
void hero_smoke_puffs(int32_t n, int32_t mode) {
    const uint32_t P = g::PLAYER;
    const float k01 = u2f(0x3dcccccdu), k04 = u2f(0x3ecccccdu);
    Vec4 pos = {}, vel = {};
    for (int32_t i = 0; i < n; ++i) {
        if (mode == 0) {
            pos.x = lwc1(P + 0x80) + rand_range_f(-k01, k01);
            pos.y = lwc1(P + 0x84) + rand_range_f(-k01, k01);
            pos.z = lwc1(P + 0x88) + rand_range_f(-0.5f, k01);
        } else if (mode == 1) {
            moby_chain_point_v(lwu(P + 0x2080), (i & 1) != 0 ? 0xeu : 0u, pos);
        } else if (mode == 2) {
            moby_chain_point_v(lwu(P + 0x2080), (i & 1) != 0 ? 0x16u : 0x17u, pos);
        }
        vel = V(P + 0x100);
        vec3_scale(vel, vel, lw(P + 0x2084) == 0x34 ? u2f(0x3f933333u) : u2f(0x3f333333u));
        float k = lwc1(0x0015ed6c);
        vel.x = vel.x + rand_range_f(k * -k04, k * k04);
        k = lwc1(0x0015ed6c);
        vel.y = vel.y + rand_range_f(k * -k04, k * k04);
        k = lwc1(0x0015ed6c);
        vel.z = vel.z + rand_range_f(-k, k * 0.0f);
        spawn_particle_22_v(pos, vel, rand_range_f(u2f(0x45834000u), u2f(0x45e5b000u)), -1.0f);
    }
}


// orig: 0x0020e698 — drop the held moby: flag the hand layers out (P+0xd14) or, idle on the
// holding animation (+0x2294), return to the default set (sub_00205000(0), table blend 1);
// clears the holding flags +0x20a8 / +0x20aa.
void hero_hold_release() {
    const uint32_t P = g::PLAYER;
    if (lbu(P + 0x20a8) == 0) return;
    if (lwu(P + 0xd08) != 0) {
        sw(P + 0xd14, 1);
    } else if (lw(P + 0x2084) == 0 && static_cast<int32_t>(lbu(lwu(P + 0x2080) + 0x53)) == lw(P + 0x2294)) {
        hero_anim_set(sub_00205000(0), 0, -2.0f);
    }
    sb(P + 0x20aa, 0);
    sb(P + 0x20a8, 0);
}

// orig: 0x002061f0 — attach the rider moby P+0xa88 to the hero: at its chain-5 joint (position
// and orientation) on set 0x81 when the hero moby is shown (+0x34 bit 0 clear), else hide it.
void hero_rider_attach() {
    const uint32_t P = g::PLAYER;
    const uint32_t r = lwu(P + 0xa88);
    sb(r + 0x7f, 0);
    if ((lhu(lwu(P + 0x2080) + 0x34) & 1) != 0) {
        sh(r + 0x34, lhu(r + 0x34) | 0x41);
        return;
    }
    Mat4 j = {};
    moby_chain_world_one_v(lwu(P + 0x2080), 5, j);
    V(r + 0x10) = j.r[3];
    mat3_copy(M(r + 0xc0), j);
    mat_to_euler_r(j, V(r + 0x40));
    sh(r + 0x34, lhu(r + 0x34) & 0xffbe);
    moby_anim_set(r, 0x81, 0, frame_scale_i(0xa));
    moby_anim_advance(r);
    moby_update_transform(r);
}


// orig: 0x0020e4c8 — start holding with hero slot 0's entry k (0x179ac0 + k * 0x4c): flags
// +0x20a8 / +0x20aa, blend 0xa / 0xb frames; on foot and idle it switches the hero to the
// entry's set (+0x24), else it (re)creates the hand layers on set +0x28 (+0x2c in sub-state 0xc).
void hero_hold_start() {
    const uint32_t P = g::PLAYER;
    const int32_t k = sub_0020d498(0);
    if (static_cast<uint32_t>(k + 1) < 2u) return;
    const uint32_t e = OV(0x00179ac0u) + static_cast<uint32_t>(k) * 0x4c;
    sw(P + 0x2298, static_cast<uint32_t>(k));
    const uint32_t set = lwu(e + 0x24);
    uint32_t layer = lwu(e + 0x28);
    if (lw(P + 0x208c) == 0xc) layer = lwu(e + 0x2c);
    sb(P + 0x20a8, 1);
    sb(P + 0x20aa, lwu(e + 0x30));
    int32_t frames = frame_scale_i(0xa);
    if (lwu(e + 0x18) == 0) frames = frame_scale_i(0xb);
    if (lw(P + 0x208c) == 0 && lh(P + 0x22c8) == 0) {
        sw(P + 0x2294, set);
        hero_anim_set(set, 0, int_to_float(frames));
        return;
    }
    if (layer == 0xffffffffu) return;
    for (uint32_t s = 0; s < 2; ++s) {
        if (s == 1 && lbu(P + 0x20ab) != 2) return;
        const uint32_t slot = P + 0xd08 + s * 4;
        if (lwu(slot) != 0) sub_0024f0e8(lwu(P + 0x2080), slot);
        const uint32_t r = sub_0024f028(lwu(P + 0x2080), lwu(OV(0x0015f6a0) + s * 4));
        sw(slot, r);
        if (r == 0) continue;
        sw(P + 0x2294, layer);
        sw(P + 0xd14, 0);
        swc1(r + 8, 1.0f);
        moby_anim_rec_start(lwu(P + 0x2080), r, lwu(P + 0x2294), 0, frames, 1);
    }
}


// orig: 0x0020e728 — per-frame holding: release when the held moby is gone (sub_00233e98(-1));
// keep the hand layers P+0xd08 / +0xd0c on the entry's layer set and step them, fading them out
// (flag +0xd14: 2 fast, else slow; freed at 0) or in, ending the hold at the end of a one-shot
// layer; then the carry frame P+0x640 (hero rotation, or aimed in sub-state 0xf) and its
// forward axis P+0x670.
void hero_hold_update() {
    const uint32_t P = g::PLAYER;
    if (lbu(P + 0x20a8) != 0 && lbu(P + 0x20aa) != 0 && sub_00233e98(-1) == 0) hero_hold_release();
    for (uint32_t s = 0; s < 2; ++s) {
        const uint32_t slot = P + 0xd08 + s * 4;
        if (lwu(slot) == 0) continue;
        const int32_t k = sub_0020d498(0);
        if (k == lw(P + 0x2298)) {
            const uint32_t e = OV(0x00179ac0u) + static_cast<uint32_t>(k) * 0x4c;
            const uint32_t set = lw(P + 0x208c) == 0xc ? lwu(e + 0x2c) : lwu(e + 0x28);
            if (set != 0xffffffffu && lbu(lwu(slot) + 0x23) != set) {
                sw(P + 0x2294, set);
                moby_anim_rec_start(lwu(P + 0x2080), lwu(slot), lwu(P + 0x2294), 0, frame_scale_i(8), 1);
            }
        }
        moby_anim_step(lwu(slot));
        const int32_t fade = lw(P + 0xd14);
        if (fade != 0) {
            const float k2 = fade == 2 ? 0.25f : u2f(0x3d8f5c29u);
            approach_f(lwu(slot) + 8, 0.0f, lwc1(0x0015ed60) * k2);
            if (lwc1(lwu(slot) + 8) == 0.0f) {
                sub_0024f0e8(lwu(P + 0x2080), slot);
                return;
            }
        } else {
            approach_f(lwu(slot) + 8, 1.0f, lwc1(0x0015ed60) * u2f(0x3e4ccccdu));
            if ((lbu(lwu(slot) + 5) & 2) != 0 && lbu(P + 0x20aa) == 0) {
                sw(P + 0xd14, 1);
                sw(lwu(slot) + 0x28, 0);
                sb(P + 0x20a8, 0);
                return;
            }
        }
    }
    if (lw(P + 0x208c) != 0xf) {
        const uint32_t hm = lwu(P + 0x2080);
        lq_sq(P + 0x640, hm + 0xc0);
        lq_sq(P + 0x650, hm + 0xd0);
        lq_sq(P + 0x660, hm + 0xe0);
        vec_clear(V(P + 0x670));
        swc1(P + 0x670, 1.0f);
        mat3_transform(V(P + 0x670), V(P + 0x670), M(P + 0x640));
        lq_sq(P + 0x680, P + 0x90);
        return;
    }
    const uint32_t tgt = lwu(P + 0x5d8);
    float yaw;
    if (tgt != 0) {
        lq_sq(P + 0x680, P + 0x90);
        yaw = atan2_f(lwc1(tgt + 0x10) - lwc1(P + 0x80), lwc1(tgt + 0x14) - lwc1(P + 0x84));
    } else {
        lq_sq(P + 0x680, P + 0x90);
        yaw = angle_add(lwc1(P + 0x57c), lwc1(P + 0x5d0));
    }
    swc1(P + 0x688, yaw);
    swc1(P + 0x684, -lwc1(P + 0x580));
    euler_to_mat3_vu(M(P + 0x640), V(P + 0x680));
    vec_clear(V(P + 0x670));
    swc1(P + 0x670, 1.0f);
    mat3_transform(V(P + 0x670), V(P + 0x670), M(P + 0x640));
}


// orig: 0x0020b1d8 — while the right stick / look axes (P+0x2c0..0x2d4) move: head look angles
// into the bone table 0x17a680 (clamped yaw / pitch), stiffness 0.01 with +0x10e0; pad rumble in
// state 3 / early sub-state 4; and +0x12a8 frames of sparks (particle 0x1c, +0x12ac per hand
// joint 0x16 / 0x17, speed +0x12a0 +- +0x12a4, along the hero rotation when +0x12ae bit 0).
void hero_look_fx() {
    const uint32_t P = g::PLAYER;
    constexpr OvConst<0x0017a680u> T{};
    if (sub_0020d568() != 0) return;
    bool moving = u2f(0x3cf5c28fu) < abs_f(lwc1(P + 0x2c0)) || u2f(0x3cf5c28fu) < abs_f(lwc1(P + 0x2c4));
    if (!moving) {
        const float t = u2f(0x3d8efa35u);
        moving = t < abs_f(lwc1(P + 0x2c8)) || t < abs_f(lwc1(P + 0x2cc)) || t < abs_f(lwc1(P + 0x2d0)) || t < abs_f(lwc1(P + 0x2d4));
    }
    if (moving && lh(P + 0x308) != 1) {
        swc1(T + 0x324, -lwc1(P + 0x2c8));
        swc1(T + 0x3d4, -lwc1(P + 0x2cc));
        const float k = u2f(0x3f99999au), quarter = u2f(0x3f490fdbu), lim = u2f(0x3e32b8c2u);
        float yaw = atan2_f(0.2f, lwc1(P + 0x2c0)) * k;
        float pitch = -atan2_f(0.2f, lwc1(P + 0x2c4)) * k;
        if (quarter < yaw) yaw = quarter;
        if (yaw < -lim) yaw = -lim;
        if (lim < pitch) pitch = lim;
        if (pitch < -quarter) pitch = -quarter;
        swc1(T + 0x480, yaw);
        swc1(T + 0x530, pitch);
        swc1(T + 0x320, -angle_sub(lwc1(P + 0x2d0), -yaw));
        swc1(T + 0x3d0, -angle_sub(lwc1(P + 0x2d4), -pitch));
    }
    if (lwu(P + 0x10e0) != 0) {
        swc1(T + 0x41c, u2f(0x3c23d70au));
        swc1(T + 0x36c, u2f(0x3c23d70au));
    }
    if ((lhu(P + 0x12ae) & 0x80) != 0) {
        sw(P + 0x12a8, 0);
        return;
    }
    if (lw(P + 0x2084) == 3) sub_00233288(1, 2, 1, 0, u2f(0x3cac0831u), u2f(0x3b03126fu));
    if (lw(P + 0x208c) == 4 && lw(P + 0x418) > 0 && lw(P + 0x418) < frame_scale_i(0xc)) {
        sub_00233288(1, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
    }
    const int32_t left = lw(P + 0x12a8);
    if (left <= 0 || lh(P + 0x12ac) == 0) return;
    const Mat4* mat = (lhu(P + 0x12ae) & 1) != 0 ? &M(lwu(P + 0x2080) + 0xc0) : nullptr;
    sw(P + 0x12a8, static_cast<uint32_t>(left - 1));
    for (uint32_t joint : {0x16u, 0x17u}) {
        Vec4 pos = {};
        moby_chain_point_v(lwu(P + 0x2080), joint, pos);
        for (int32_t i = 0; i < lh(P + 0x12ac); ++i) {
            const float a = lwc1(P + 0x12a4);
            const float r = rand_range_f(-a, a);
            spawn_particle_1c_v(pos, mat, lwc1(P + 0x12a0) + r);
        }
    }
}


namespace {
// shared tail of the beam gadgets: glow point (offset o in the gadget's frame) coloured from the
// gadget colour (r, b scaled 0.2, alpha g * 32 / 200)
static void gadget_glow_point(uint32_t g, const Vec4& o, float size) {
    Vec4 p = o;
    mat3_transform(p, p, M(g + 0xc0));
    vec3_add(p, p, V(g + 0x10));
    const int32_t r = float_to_int(int_to_float(lbu(g + 0x90)) * 0.2f);
    const uint32_t gr = lbu(g + 0x91);
    const int32_t b = float_to_int(int_to_float(lbu(g + 0x92)) * 0.2f);
    const uint32_t a = static_cast<uint32_t>(static_cast<int32_t>(gr << 5) / 200);
    const uint32_t col = (a << 24) | (static_cast<uint32_t>(b) << 16) | (gr << 8) | static_cast<uint32_t>(r);
    sub_00208038_v(p, col, size, u2f(0x3dcccccdu));
}
// colour step of the beam gadgets: phase +0x10bc advanced by rate, two colours by phase window
static void gadget_beam_colour(uint32_t g, float rate, uint32_t off, uint32_t on, float hi) {
    const uint32_t P = g::PLAYER;
    const float ph = angle_add(lwc1(P + 0x10bc), lwc1(0x0015ed6c) * rate);
    swc1(P + 0x10bc, ph);
    const uint32_t c = (0.0f < ph && ph < hi) ? on : off;
    sw(g + 0x90, rgba_lerp(lwu(g + 0x90), c, u2f(0x3d8f5c29u)));
}
}  // namespace

// orig: 0x002080b0 — per-frame colours / glow points of the hero's gadgets: the item moby's two
// glow joints (+0x1158 == 5), the hero moby's pulsing tint (+0x1628), and per held gadget kind
// (+0x10b8: 0xd, 0x10, 0x11, 0x13, 0x14) its colour cycle (+0x10bc) and glow sprite.
void hero_gadget_glow() {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0x1158) == 5) {
        const uint32_t it = lwu(P + 0x1130);
        if (it != 0 && (lhu(it + 0x34) & 1) == 0) {
            for (uint32_t j = 0; j < 2; ++j) {
                Vec4 p = {};
                moby_chain_point_v(it, j, p);
                sub_00208038_v(p, 0x301eaa1eu, u2f(0x3d6978d5u), u2f(0x3e19999au));
            }
        }
    }
    const float ph = angle_add(lwc1(P + 0x1628), lwc1(0x0015ed6c) * u2f(0x4011361eu));
    swc1(P + 0x1628, ph);
    const float t = sin_vu(ph) * 0.5f + 0.5f;
    sw(lwu(P + 0x2080) + 0x90, rgba_lerp(0x806e8c6eu, 0x805a3232u, t));
    const uint32_t g = lwu(P + 0x1090);
    if (g == 0 || (lhu(g + 0x34) & 1) != 0) return;
    const uint32_t kind = lwu(P + 0x10b8) - 0xd;
    if (kind >= 8) return;
    switch (kind) {
    case 4: {  // 0x11
        gadget_beam_colour(g, u2f(0x408ba058u), 0x28c8c8c8u, 0x280a0a0au, u2f(0x4011361eu));
        const Vec4 o = Vec4{u2f(0x3c8e17a0u), u2f(0xbd20785eu), u2f(0x3dc51698u), 0.0f};
        gadget_glow_point(g, o, u2f(0x3e19999au));
        break;
    }
    case 7: {  // 0x14
        gadget_beam_colour(g, u2f(0x408ba058u), 0x00dcdcdcu, 0x000a0a0au, u2f(0x401c61aau));
        const Vec4 o = Vec4{u2f(0x3c8c1e21u), u2f(0xbdc058d3u), u2f(0x3e105647u), 0.0f};
        gadget_glow_point(g, o, u2f(0x3e2e147bu));
        break;
    }
    case 3: {  // 0x10
        if (!(frame_scale_i(0x32) < lw(P + 0x10b0))) return;
        const float k = lwc1(0x0015ed6c);
        const float rate = lbu(P + 0x20a8) != 0 ? k * u2f(0x4096cbe4u) : k * u2f(0x40490fdbu);
        const float a = angle_add(lwc1(P + 0x10bc), rate);
        swc1(P + 0x10bc, a);
        uint32_t lo = static_cast<uint32_t>(float_to_int(sin_vu(a) * 20.0f) + 0x1e);
        uint32_t mid = static_cast<uint32_t>(float_to_int(sin_vu(a) * 90.0f) + 0x96);
        const uint32_t hi = static_cast<uint32_t>(float_to_int(sin_vu(a) * 20.0f) + 0x1e) << 16;
        if (lbu(P + 0x20a8) != 0) std::swap(lo, mid);
        mid <<= 8;
        sw(g + 0x90, rgba_lerp(lwu(g + 0x90), hi | 0x20000000u | mid | lo, u2f(0x3e051eb8u)));
        int32_t al = lw(P + 0x10b0) - frame_scale_i(0x32);
        if (!(al < 0x21)) al = 0x20;
        const uint32_t col = (static_cast<uint32_t>(al) << 24) | hi | mid | lo;
        Vec4 p = {};
        moby_chain_point_v(g, 1, p);
        sub_00208038_v(p, col, u2f(0x3e19999au), u2f(0x3d23d70au));
        break;
    }
    case 6: {  // 0x13
        const float k = lwc1(0x0015ed6c);
        const float rate = lbu(P + 0x20a8) != 0 ? k * u2f(0x40ea927fu) : k * u2f(0x4011361eu);
        const float a = angle_add(lwc1(P + 0x10bc), rate);
        swc1(P + 0x10bc, a);
        uint32_t c0, c1, c2;
        if (lbu(P + 0x20a8) != 0) {
            c0 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 30.0f) + 0x32);
            c1 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 30.0f) + 0x32);
            c2 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 90.0f) + 0xaa);
        } else {
            c0 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 70.0f) + 0x64);
            c1 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 40.0f) + 0x46);
            c2 = static_cast<uint32_t>(float_to_int(sin_vu(a) * 40.0f) + 0x46);
        }
        sw(g + 0x90, rgba_lerp(lwu(g + 0x90), (c2 << 16) | 0x80000000u | (c1 << 8) | c0, u2f(0x3e19999au)));
        break;
    }
    case 0: {  // 0xd
        const float a = angle_add(lwc1(P + 0x10bc), lwc1(0x0015ed6c) * u2f(0x4096cbe4u));
        swc1(P + 0x10bc, a);
        const uint32_t c = (0.0f < a && a < u2f(0x40060a92u)) ? 0x281414d2u : 0x2814d214u;
        sw(g + 0x90, rgba_lerp(lwu(g + 0x90), c, lwc1(0x0015ed60) * u2f(0x3d4ccccdu)));
        Vec4 p = {};
        moby_chain_point_v(g, 0, p);
        sub_00208038_v(p, lwu(g + 0x90), u2f(0x3dcccccdu), u2f(0x3cf5c28fu));
        break;
    }
    default:
        break;
    }
}

// orig: 0x0020fea0 — per-slot update for the 0x50-byte hero slots at PLAYER+0x1090 while the
// slot is active (state 2): resolve the pending request words (+0x20b8/+0x20d4/+0x20f0/+0x210c,
// one per slot) into the slot's current kind (0x141660 table), and when a switch happened and
// the slot's countdown (+0x10a8) has run out, close the slot (state 3), reset the anims of its
// mobys and fire the switch effects. `prev` is the caller's $s1, handed on to use_agents.
void hero_slot_update(int32_t k, uint32_t prev) {
    if (on_rilgar()) { ril_call(ril_hero_slot_update_u, static_cast<uint32_t>(k), prev); return; }  // (Rilgar's own: L5:0x0023dc88)
    if (on_kalebo()) { kal_call(kal_hero_slot_update_u, static_cast<uint32_t>(k), prev); return; }  // (level 16's own: L16:0x0020d8b0)
    const uint32_t P = g::PLAYER;
    constexpr uint32_t KIND = 0x00141660, FLAGS = 0x0013c940 + 0x1a4;
    const uint32_t ku = static_cast<uint32_t>(k);
    const uint32_t s2 = ku * 4u;
    const uint32_t q = P + ku * 0x50u;
    if (k == 0) {
        const int32_t st = lw(P + 0x10b8);
        if (st != 8 && st != 0x17 && st != 9 && lbu(P + 0x10ac) == 2) sb(P + 0x10ac, 0);
        if (lbu(P + 0x20a7) != 0 && lw(P + 0x10b8) != 8) sb(P + 0x20a6, 1);
        if (lbu(P + 0x20a7) != 0 && lw(P + 0x10b8) == 8) {
            if (lw(P + 0x20b8) == 0x1f) sw(P + 0x20b8, 0);
            const int32_t v = lw(P + 0x20b8);
            if (v != 0x18 && v != 0x24) return;
        }
    }
    if (lbu(P + 0x20ac) != 0 && k == 0 && (lbu(P + 0x20a6) == 0 || lw(P + 0x10b8) == 8)) {
        sb(P + 0x20a6, 0);
        return;
    }
    bool changed = false;
    if ((lw(FLAGS) & 0x80) != 0 || lbu(P + 0x20a6) != 0) {
        if (k == 0 && lw(P + 0x10b8) != 8) {
            const int32_t v = lw(P + 0x10b8);
            if (lbu(P + 0x20a6) != 0 && v != 0 && v != 0x1f) sw(P + 0x20f0, static_cast<uint32_t>(v));
            sw(P + 0x20d4 + s2, 8);
            changed = true;
        }
    }
    if (k == 0 && (lw(FLAGS) & 0x20) != 0 && lbu(P + 0x20a7) == 0) {
        const int32_t cur = lw(P + 0x10b8), want = lw(KIND);
        if (cur != want && want != 0) {
            sw(P + 0x20d4, static_cast<uint32_t>(want));
            changed = true;
        }
    }
    // Request `req` for slot 1 when the matching kind is not already up.
    auto request1 = [&](int32_t req) {
        if (lw(KIND + 4) == req && lw(q + 0x10b8) != 0) return;
        sw(P + 0x20bc, static_cast<uint32_t>(req));
        const int32_t v = lw(q + 0x10b8);
        sw(P + s2 + 0x20f0, static_cast<uint32_t>(v != 0 ? v : 0x26));
    };
    if (k == 1) {
        if (sub_0020d568() != 0 || lw(P + 0x2084) == 0x12) {
            const uint32_t v = lwu(P + 0x1108);
            if (v != 0) {
                sw(P + 0x20f4, v);
                sw(P + 0x20bc, 0x26);
            }
        }
        if (lw(P + 0x208c) == 0xf && lbu(0x0013d4dd) != 0) request1(0x1d);
        if (lbu(P + 0x12e7) != 0 && lbu(0x0013d4dc) != 0) request1(0x1c);
    }
    bool req6 = false;
    if (k == 2) {
        const int32_t hs = lw(P + 0x2084);
        if (lw(P + 0x208c) == 0x11 || hs == 0x76 || hs == 0x6a || hs == 0x82 || lbu(P + 0x22cb) != 0) {
            if (!(lw(KIND + s2) == 6 && lw(q + 0x10b8) != 0) && lw(q + 0x10b8) != 6 && lw(P + s2 + 0x20d4) != 6 &&
                lbu(0x0013d4c6) != 0) {
                sw(P + s2 + 0x20b8, 6);
                req6 = true;
                const int32_t v = lw(q + 0x10b8);
                sw(P + s2 + 0x20f0, static_cast<uint32_t>(v != 0 ? v : 0x26));
            }
        } else if (lw(P + 0x1158) == 6 && lw(P + 0x20f8) != 0) {
            sw(P + 0x2114, 1);
        }
    }
    bool req4 = false;
    if (k == 3) {
        if (sub_0020d568() != 0 || lw(P + 0x2084) == 0x12) {
            const uint32_t a2 = lwu(P + 0x11a8);
            if (lw(KIND + 0xc) != 4 && a2 != 4u && lbu(0x0013d4c4) != 0) {
                sw(P + 0x20c4, 4);
                req4 = true;
                sw(P + 0x20fc, a2);
            }
        } else if (lw(P + 0x11a8) == 4 && lw(P + 0x20fc) != 0) {
            sw(P + 0x2118, 1);
        }
        if (lw(P + 0x2084) == 8 && lw(KIND + s2) == 4) sw(P + s2 + 0x20b8, 2);
    }
    // Kind code of the last request: 0x26 means "none".
    auto mark_kind = [&] { sw(OV(0x00179ac0) + static_cast<uint32_t>(lw(P + 0x20d4) * 0x4c) + 0x20, 0x20); };
    if (lw(P + 0x210c + s2) != 0) {
        sw(P + 0x210c + s2, 0);
        const int32_t a0 = lw(P + 0x20f0 + s2);
        if (a0 != 0 && a0 != lw(q + 0x10b8)) {
            const uint32_t kind = a0 == 0x26 ? 0u : static_cast<uint32_t>(a0);
            sw(P + 0x20f0 + s2, 0);
            sw(KIND + s2, kind);
            sw(P + s2 + 0x20d4, kind);
            if (k == 0) mark_kind();
            changed = true;
        }
    }
    if (lbu(P + 0x1ff5) != 0) {
        use_agents(prev);
        sw(P + s2 + 0x20b8, 0);
    }
    const uint32_t req = P + 0x20b8 + s2;
    if (lw(req) == 0x1f) {
        if (sub_0020fe30() != 0) {
            if (lh(P + 0x22de) == 0) {
                hero_sound_start(0x18, 0);
                sh(P + 0x22de, 0x12);
                sh(P + 0x22dc, 1);
            }
            changed = true;
            sw(P + s2 + 0x20d4, lwu(req));
            const uint32_t old = lwu(KIND + s2);
            sw(KIND + s2, lwu(req));
            sw(0x0015ed8c, old);
        }
        sw(P + s2 + 0x20b8, 0);
    }
    const int32_t r = lw(req);
    if (r != 0) {
        if (r == lw(P + 0x20d4 + s2)) {
            sw(req, 0);
        } else if (r == 0x26) {
            sw(P + 0x20d4 + s2, 0);
            sw(KIND + s2, 0);
            changed = true;
        } else {
            if (k == 0 && lw(P + 0x20b8) != 0x24 && lw(P + 0x10b8) != 8) sw(0x0015ed8c, lwu(KIND));
            const int32_t v = lw(P + s2 + 0x20b8);
            sw(P + s2 + 0x20d4, static_cast<uint32_t>(v));
            if (v != 0x24 && !(req6 && v == 6) && !(req4 && v == 4)) sw(KIND + s2, static_cast<uint32_t>(v));
            if (k == 0) mark_kind();
            changed = true;
        }
    }
    const uint32_t b = lbu(q + 0x10ac);
    if (b == 2) changed = false;
    if (b == 1 && lh(q + 0x10a8) == 0) sh(q + 0x10a8, lbu(q + 0x10ad));
    if (timer_tick_i16(ee_ref<int16_t>(q + 0x10a8)) == 0) changed = false;
    if (!changed) return;
    sw(q + 0x10b0, 3);
    sub_0020a500();
    const int32_t lo = frame_scale_i(0x32);
    const int32_t hi = frame_scale_i(0x5a);
    sw(P + 0x1010, static_cast<uint32_t>(rand_range(lo, hi)));
    sub_0020fde0(lw(P + 0x20d4 + s2));
    sub_0020fd80(lw(P + 0x20d4 + s2));
    if (lbu(P + 0x20a8) != 0) hero_hold_release();
    if (k == 0) {
        if (lw(P + 0x20d4) == 8) sw(0x0015ed90, 1);
        else sw(OV(0x00166c00u - 0x7e70), 0);
    }
    if (k == 3) {
        if (lw(P + 0x11a8) == 3) sw(0x0015ed94, 1);
        else sw(OV(0x00166c00u - 0x7e6c), 0);
    }
    sb(q + 0x10aa, 0);
    sw(P + s2 + 0x20b8, 0);
    sb(P + 0x20a6, 0);
    if (const uint32_t m = lwu(P + 0xd00); m != 0) sw(m + 0x34, 1);
    if (const uint32_t m = lwu(P + 0xd04); m != 0) sw(m + 0x34, 1);
    if (const uint32_t m = lwu(q + 0x1090); m != 0) moby_anim_set(m, 2, 0, 2);
    if (const uint32_t m = lwu(q + 0x1094); m != 0 && k != 3) moby_anim_set(m, 2, 0, 2);
    sw(q + 0x10b4, 3);
}

// orig: 0x0020f580 — place the mobys of the 0x50-byte hero slots (PLAYER+0x1090, second moby
// +0x1094 for slots 1/3/4) on the hero each frame: at the attach point of the slot's kind
// (table 0x179ac0, 0x4c bytes: +0xc point index, +0x18 smoothed flag) from the point arrays
// PLAYER+0xaf0 (positions) / PLAYER+0xac0 (matrices). Kinds 5..7 / 0x1c..0x1d get an encoded
// fixed pose; slot 3's second moby also carries the glow moby PLAYER+0x118c (class 0x4b4).
void hero_slot_mobys_update() {
    const uint32_t P = g::PLAYER;
    const uint32_t KINDS = OV(0x00179ac0), PTS = 0x0013fe10;
    for (int32_t k = 0; k < 7; ++k) {
        const int32_t mode = lw(OV(0x0015f5c4));
        if ((mode == 2 || mode == 6) && k != 4 && k != 5) continue;
        const uint32_t off = static_cast<uint32_t>(k) * 0x50u, q = P + off;
        const int32_t njs = (k == 1 || k == 3 || k == 4) ? 2 : 1;
        for (int32_t j = 0; j < njs; ++j) {
            const uint32_t m = lwu(q + (j == 1 ? 0x1094u : 0x1090u));
            if (m != 0) {
                sd(m + 0x38, ld(lwu(P + 0x2080) + 0x38));
                const uint32_t kind = KINDS + static_cast<uint32_t>(lw(q + 0x10b8) * 0x4c);
                uint32_t idx = lwu(kind + 0xc);
                if (k == 1 && j == 1) idx = 3;
                const uint32_t s0 = idx << 6;
                if (lbu(q + 0x10aa) != 0) {
                    lq_sq(q + 0x1070, P + 0xaf0 + s0);
                    mat_to_euler(PTS + s0, q + 0x1080);
                    moby_anim_advance(m);
                    moby_update_transform(m);
                    sh(m + 0x34, lhu(m + 0x34) & 0xfffb);
                } else if (lq_sq(m + 0x10, P + 0xaf0 + s0); lw(OV(0x00166c00u - 0x7570)) != 0 && lw(kind + 0x18) != 0) {
                    moby_anim_advance(m);
                    if (lw(OV(0x00166c00u - 0x7570)) == 0 || k != 0) mat_to_euler(PTS + s0, m + 0x40);
                    else hero_rot_smooth(m, PTS + s0);
                    moby_update_transform(m);
                    if (lbu(0x0015edb5) != 0) vec3_cross_ba(V(m + 0xe0), V(m + 0xc0), V(m + 0xd0));
                    moby_update_transform_kept(m);
                    sh(m + 0x34, lhu(m + 0x34) | 6);
                } else {
                    const int32_t s5 = sub_0020d4d0(k), s4 = sub_0020d518(k), s3 = sub_0020d540(k);
                    const bool plain = s5 == 0 && s4 == 0 && s3 == 0;
                    if (plain) moby_anim_advance(m);
                    moby_update_transform(m);
                    mat3_copy(M(m + 0xc0), M(PTS + s0));
                    if (plain) mat3_normalize_columns(m + 0xc0);
                    Vec4 t{1.0f, 0.0f, 0.0f, 0.0f};
                    mat3_transform(t, t, M(m + 0xc0));
                    const float len = vec2_length(t);
                    swc1(m + 0x44, -atan2_f(len, t.z));
                    swc1(m + 0x48, atan2_f(t.x, t.y));
                    moby_update_transform_kept(m);
                    sh(m + 0x34, lhu(m + 0x34) | 6);
                    const uint32_t cls = lwu(m + 0x24);
                    uint32_t key = 0;
                    if (s5 != 0) {
                        key = OV(0x0017c480);
                        moby_pose_encode(OV(0x0017a5c0), key, cls, 0, 0);
                    } else if (s4 != 0) {
                        key = OV(0x0017c680);
                        if (lh(m + 0xa6) == 0x1b1) moby_pose_encode(OV(0x0017a608), key, cls, 6, 0);
                        else moby_pose_encode(OV(0x0015f6a8), key, cls, 0, 0);
                    } else if (s3 != 0) {
                        const uint32_t B = OV(0x0017a680), KEEP = OV(0x0017a960);
                        const Vec4 keep0 = V(KEEP), keep1 = V(KEEP + 0xb0);
                        for (uint32_t o : {0x398u, 0x2e0u, 0x2e4u, 0x2e8u, 0x390u, 0x394u}) swc1(B + o, 1.0f);
                        key = j == 0 ? OV(0x0017c880u) : OV(0x0017c980u);
                        moby_pose_encode(j == 0 ? OV(0x0017a650u) : OV(0x0017a668u), key, cls, 0, 0);
                        V(KEEP) = keep0;
                        V(KEEP + 0xb0) = keep1;
                    }
                    if (key != 0) {
                        sw(m + 0x54, 0);
                        sw(m + 0x50, 0);
                        sw(m + 0x68, key);
                        sw(m + 0x6c, key);
                    }
                }
            }
            if (k != 3 || j != 1 || m == 0) continue;
            uint32_t g = lwu(P + 0x118c);
            if (g == 0) {
                const uint32_t v = moby_spawn(0x4b4);
                sw(P + 0x118c, v);
                if (v == 0) continue;
                sw(v + 0x90, 0x801432d7u);
                lq_sq(v + 0x10, P + 0x80);
                lq_sq(v + 0x40, P + 0x90);
                sh(v + 0x32, 0x40);
                sb(v + 0x31, 1);
                sb(v + 0x30, 0);
                sh(v + 0x34, 0);
                swc1(v + 0x2c, lwc1(v + 0x2c) * u2f(0x3fa66666u));
                moby_update_transform(v);
                g = lwu(P + 0x118c);
                if (g == 0) continue;
            }
            if ((lhu(g + 0x34) & 1) != 0) continue;
            Mat4 w;
            moby_chain_world_one_v(m, 6, w);
            V(P + 0x1d80) = w.r[3];
            sub_00207d00();
            V(g + 0x10) = w.r[3];
            mat3_copy(M(g + 0xc0), w);
            moby_update_transform(g);
            sd(g + 0x38, ld(lwu(P + 0x2080) + 0x38));
            const int32_t period = frame_scale_i(0x78);
            const float fp = int_to_float(period);
            const int32_t r = ee_rem(lw(OV(0x0015f5cc)), period);
            float a = fpu_div(static_cast<float>(r), fp);
            a = a + a;
            a = a * u2f(0x40490fdbu);
            const float sn = sin_vu(a + u2f(0xc0490fdbu));
            int32_t red = float_to_int(sn * 90.0f) + 0xd7;
            const int32_t grn = float_to_int(sn * 50.0f) + 0x32;
            const int32_t blu = float_to_int(sn * 10.0f) + 0x14;
            if (!(red < 0x100)) red = 0xff;
            sw(g + 0x90, (static_cast<uint32_t>(blu) << 16) | 0x80000000u | (static_cast<uint32_t>(grn) << 8) | static_cast<uint32_t>(red));
            sh(g + 0x34, lhu(g + 0x34) | 0x10);
        }
    }
}

// orig: 0x0020ea50 — per-frame hero layer work: the two anim layers, the held moby, the extra layer.
void hero_layers_tick() {
    hero_layers_update();
    hero_hold_update();
    hero_extra_layer_update();
}

// orig: 0x0020fca8 — close hero slot k: store `delay` (+0x10a4); unless already closing, mark it
// closing (state 3) and run its moby's update once (moby +0x74, unless being freed); then clear
// the slot and retire its mobys (slot 3 keeps the second one).
void hero_slot_close(int32_t k, int32_t delay) {
    const uint32_t q = g::PLAYER + static_cast<uint32_t>(k) * 0x50u;
    const int32_t st = lw(q + 0x10b4);
    sw(q + 0x10a4, static_cast<uint32_t>(delay));
    if (st != 3) {
        const uint32_t m = lwu(q + 0x1090);
        sw(q + 0x10b4, 3);
        if (m != 0) {
            const uint32_t b = lbu(m + 0x20);
            if (b != 0xfe && b != 0xfd && lwu(m + 0x74) != 0) ovl_call_moby_fn(lwu(m + 0x74), m);
        }
    }
    sw(q + 0x10b4, 0);
    sw(q + 0x10b8, 0);
    if (const uint32_t m = lwu(q + 0x1090); m != 0) {
        moby_retire(m);
        sw(q + 0x1090, 0);
    }
    if (const uint32_t m = lwu(q + 0x1094); m != 0 && k != 3) {
        moby_retire(m);
        sw(q + 0x1094, 0);
    }
}

// Per-frame update of the 7 hero slots: active slots (state 2) count up +0x10b0 and resolve
// their requests (hero_slot_update; slot 0 also nudges the three bone nodes at PLAYER+0x18f0),
// restarting the moby's set 1 when its anim looped; closing slots (state 3) count +0x10ab and
// close once the anim looped (slots 0/1 always); then each slot moby's own update runs (+0x74,
// not for slot 6). `prev` is the caller's $s1, handed on to hero_slot_update (it becomes the
// bone node pointer past the last node after slot 0).
// orig: 0x00210748
void hero_slots_update(uint32_t prev) {
    const uint32_t P = g::PLAYER;
    uint32_t s1 = prev;
    for (int32_t k = 0; k < 7; ++k) {
        const uint32_t q = P + static_cast<uint32_t>(k) * 0x50u;
        const uint32_t m = lwu(q + 0x1090);
        if (m == 0) {
            hero_slot_update(k, s1);
            continue;
        }
        const int32_t st = lw(q + 0x10b4);
        if (st == 2) {
            sw(q + 0x10b0, lwu(q + 0x10b0) + 1);
            hero_slot_update(k, s1);
            if (k == 0) {
                s1 = P + 0x18f0;
                for (uint32_t s0 = P; s0 < P + 0x210; s0 += 0xb0, s1 += 0xb0) {
                    if (lh(s0 + 0x1990) != -1) {
                        sh(s0 + 0x1992, 5);
                        bone_node_update(s1);
                    }
                }
            }
            if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) == 0) moby_anim_set(m, 1, 0, 2);
        } else if (st == 3) {
            sb(q + 0x10ab, lbu(q + 0x10ab) + 1);
            if ((lbu(m + 0x70) & 2) != 0 || k == 1) {
                hero_slot_close(k, lw(OV(0x00166c00u - 0x7808)) + 2);
            } else if (k == 0) {
                hero_slot_close(k, lw(OV(0x0015f3f8)) + 2);
            }
        }
        const uint32_t b = lbu(m + 0x20);
        if (b == 0xfe || b == 0xfd || k == 6) continue;
        if (const uint32_t f = lwu(m + 0x74); f != 0) ovl_call_moby_fn(f, m);
    }
}

}  // namespace rac1
