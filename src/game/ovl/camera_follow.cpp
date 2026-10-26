// Ported overlay code: the follow camera, camera type 0 (0x2e4160..0x2e9c20).
//
// The camera's work block *(cam + 0x70) (0x169110 for the active camera) holds: +0x00 the
// position, +0x10.. the mode flags, +0x20 the timers, +0x40 the target (the hero position
// and its offsets +0x50..+0xe0, the hero moby +0x100, the height +0xf0), +0x130 the offset
// from the target and the distances (+0x15c / +0x160), +0x190 / +0x1a8 the springs, +0x1d0
// the collision state, +0x220 the pitch state, +0x240 the previous target.
#include <game/ovl/kerwan_camera_tr.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/rilgar_hooks.h>
#include <game/ovl/blarg_hooks.h>
#include <game/ovl/umbris_hooks.h>
#include <game/ovl/gaspar_hooks.h>
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/pokitaru_hooks.h>
#include <game/ovl/hoven_hooks.h>
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
// orig: (helper) the work block of a camera
uint32_t work(uint32_t cam) { return lwu(cam + 0x70); }
}  // namespace

// orig: 0x002e4160 — empty in this build.
void cam0_nop_a(uint32_t) {}
// orig: 0x002e4428 — empty in this build.
void cam0_nop_b(uint32_t, uint32_t) {}
// orig: 0x002e5e30 — empty in this build.
void cam0_nop_c(uint32_t) {}
// orig: 0x002e9c20 — the follow camera never asks to take over.
int32_t cam0_accept(uint32_t, uint32_t) { return 0; }
// orig: 0x002e9cb8 — no exit work.
void cam0_exit(uint32_t) {}

// orig: 0x002e4168 — the hero position (0x13f3d0) to out.
void cam0_hero_pos(uint32_t, uint32_t out) { lq_sq(out, HERO_POS); }

// orig: 0x002e42c0 — the point the camera looks at: the hero, or while the hero hangs / swings
// (0x13f65c) the point above it (0x13f62c, at least 30 units: 0.2 along the facing, then a
// segment down to the ground). Returns 1, or 0 / -1 for the ground hit / miss.
int32_t cam0_target_v(Vec4& out, int32_t flag) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(0x0013f65c) == 0) {
        (void)cam;
        out = V(HERO_POS);
        return 1;
    }
    alignas(16) Vec4 a, b, t;
    const Vec4 pos = V(HERO_POS);
    const float h = lwc1(0x0013f62c);
    if (!(30.0f <= h) && flag != 0) {
        vec3_scale(t, V(OV(0x00166e40)), -h);
        vec3_add(out, pos, t);
        return 1;
    }
    vec3_scale(t, V(OV(0x00166e40)), 0.2f);
    vec3_add(a, pos, t);
    vec3_scale(t, V(OV(0x00166e40)), -30.0f);
    vec3_add(b, pos, t);
    if (segment_collide_v(a, b, 0x12, lwu(0x001413d0), nullptr) != 0) {
        out = V(OV(0x00173e60));
        return 0;
    }
    out = pos;
    return -1;
}
// orig: 0x002e42c0 (EE-address form)
int32_t cam0_target(uint32_t out, int32_t flag) { return cam0_target_v(V(out), flag); }

// orig: 0x002e4430 — put the target block on the current look-at point: the target +0x40
// (the point with its facing component replaced by the hero's), the look points +0x80 / +0xd0
// (height +0xf0 up the hero's up row), +0x90 (the distance +0x160 up), cleared offsets.
void cam0_reset_target(uint32_t cam) {
    const uint32_t w = work(cam);
    alignas(16) Vec4 p, a, up;
    cam0_target_v(p, 0);
    vec3_scale(a, V(OV(0x00166e40)), vec3_dot(p, V(OV(0x00166e40))));
    vec3_sub(V(w + 0x40), p, a);
    V(w + 0x50) = a;
    vec3_add(V(w + 0x40), V(w + 0x40), V(w + 0x50));
    vec3_scale(up, V(OV(0x00166e30)), lwc1(w + 0xf0));
    vec3_add(V(w + 0x80), up, V(w + 0x40));
    vec3_add(V(w + 0xd0), up, V(w + 0x40));
    vec_clear(V(w + 0xe0));
    sb(w + 0x104, 0);
    swc1(w + 0x108, 1.0f);
    sb(w + 0x105, 0);
    sh(w + 0x106, 0);
    lq_sq(w + 0xa0, w + 0x40);
    vec_clear(V(w + 0xb0));
    vec_clear(V(w + 0xc0));
    vec3_scale(up, V(OV(0x00166e30)), lwc1(w + 0x160));
    vec3_add(V(w + 0x90), up, V(w + 0x40));
}

// orig: 0x002e66b8 — place the active camera behind the target (reset != 0: from the hero's
// matrix, pulled in front of walls; nudged when it ends up on the target line) and aim it.
void cam0_place(int32_t reset) {
    const uint32_t cam = lwu(OV(0x00166e00));
    const uint32_t w = work(cam), t = w + 0x40, off = w + 0x130;
    alignas(16) Vec4 s8, s4, s2, look;
    if (reset != 0) {
        uint32_t mask = 0x94;
        const Vec4 local = Vec4{-lwc1(off + 0x2c), 0.0f, lwc1(off + 0x30), 0.0f};
        Mat4 hm;
        mat3_copy(hm, M(0x0013f350));
        Vec4 o;
        mat3_transform(o, local, M(lwu(t + 0xc0) + 0xc0));
        vec3_add(V(cam + 0x30), V(t), o);
        vec3_scale(s8, V(lwu(t + 0xc0) + 0xe0), lwc1(t + 0xb0));
        vec3_add(s4, V(t), s8);
        if (segment_collide_v(s4, V(cam + 0x30), 0x94, lwu(0x001413d0), nullptr) != 0) {
            if (vec2_distance(V(OV(0x00173e60)), s4) < u2f(0x3a83126fu)) mask = 0x96;
        }
        auto pull = [&](const Vec4& from) {  // to 0.5 short of the hit point
            Vec4 h;
            vec3_sub(h, V(OV(0x00173e60)), from);
            const float l = vec3_length(h);
            if (l == 0.0f) {
                lq_sq(cam + 0x30, t);
                return;
            }
            vec3_scale(h, h, fpu_div(l - 0.5f, l));
            vec3_add(V(cam + 0x30), from, h);
        };
        if (segment_collide_v(s4, V(cam + 0x30), mask, lwu(0x001413d0), nullptr) != 0) {
            Vec4 h;
            vec3_sub(h, V(OV(0x00173e60)), s4);
            const float l = vec3_length(h);
            if (l == 0.0f) {
                lq_sq(cam + 0x30, t);
            } else if (!(l < lwc1(OV(0x00161cc8)))) {
                pull(s4);
            } else {
                // too close to a wall: a free point near the target, then from there
                vec3_scale(s8, V(lwu(t + 0xc0) + 0xe0), 0.5f);
                vec3_add(s4, V(t), s8);
                const Vec4 start = s4;
                float r = 0.5f;
                for (int32_t i = 0; i < 3;) {
                    if (sphere_collide_v(s4, r, 4, 0) != 0) {
                        s4 = V(OV(0x00173e70));
                        ++i;
                        continue;
                    }
                    if (i != 0) break;
                    r = r + 0.25f;
                    ++i;
                }
                Vec4 dd;
                vec3_sub(dd, s4, start);
                const float l2 = vec3_length(dd);
                if (l2 == 0.0f) {
                    lq_sq(cam + 0x30, t);
                } else {
                    vec3_scale(dd, dd, fpu_div(lwc1(off + 0x2c), l2));
                    vec3_add(V(cam + 0x30), start, dd);
                    if (segment_collide_v(start, V(cam + 0x30), mask, lwu(0x001413d0), nullptr) != 0) pull(s4);
                }
            }
        }
        // on the line through the target along the facing: nudge it off
        Vec4 a, b;
        vec3_sub(a, V(cam + 0x30), V(t));
        vec3_scale(b, V(OV(0x00166e40)), vec3_dot(a, V(OV(0x00166e40))));
        vec3_sub(a, a, b);
        if (vec3_length(a) < u2f(0x3d4ccccdu)) {
            swc1(cam + 0x30, rand_mod(2) != 0 ? lwc1(cam + 0x30) + 0.5f : lwc1(cam + 0x30) - 0.5f);
            swc1(cam + 0x34, rand_mod(2) != 0 ? lwc1(cam + 0x34) + 0.5f : lwc1(cam + 0x34) - 0.5f);
            swc1(cam + 0x38, lwc1(cam + 0x38) + 0.5f);
        }
        (void)hm;
    }
    swc1(cam + 0x64, lwc1(cam + 0x30));
    swc1(cam + 0x68, lwc1(cam + 0x34));
    swc1(cam + 0x6c, lwc1(cam + 0x38));
    look = V(OV(0x00166e40));
    vec3_set_length(s8, look, lwc1(t + 0xb0));
    vec3_add(s4, s8, V(t));
    vec3_sub(s2, s4, V(cam + 0x30));
    vec3_set_length(V(cam), s2, 1.0f);
    V(cam + 0x20) = look;
    vec3_cross_ba(V(cam + 0x10), V(cam), V(cam + 0x20));
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
    lq_sq(cam + 0x40, cam);
}

// orig: 0x002e6bf8 — the target block when the follow camera takes over from a transition
// (type id 0 only): the target on the hero, its offsets along the facing, the look points.
void cam0_from_transition(uint32_t cam) {
    if (lh(cam + 0x86) != 0) return;
    const uint32_t w = work(cam), t = w + 0x40, off = w + 0x130;
    alignas(16) Vec4 back, a;
    vec3_scale(back, V(0x0013f5e0), -1.0f);
    lq_sq(t, HERO_POS);
    lq_sq(w + 0xa0, t);
    vec3_scale(V(w + 0x50), back, vec3_dot(V(t), back));
    vec_clear(V(w + 0xb0));
    vec_clear(V(w + 0xc0));
    vec_clear(V(w + 0xe0));
    vec3_scale(a, back, lwc1(off + 0x30));
    vec3_add(V(w + 0x90), a, V(t));
    vec3_scale(a, back, lwc1(t + 0xb0));
    vec3_add(V(w + 0xd0), a, V(t));
    lq_sq(w + 0x80, w + 0xd0);
    vec3_sub(V(off), V(cam + 0x30), V(w + 0x90));
    lq_sq(w + 0x140, off);
    vec3_add(V(w + 0x1f0), V(w + 0x90), V(off));
    sw(w + 0x220, 0);
}

// orig: 0x002e6d60 — the follow camera's init: the springs and limits of its work block, the
// offset from the target, then either the hand-over from a transition (+0x7d == 2) or a
// fresh placement behind the hero.
void cam0_init(uint32_t cam) {
    const uint32_t w0 = work(cam);
    sw(OV(0x00161d8c), 0);
    sw(OV(0x00161d90), 0);
    const uint32_t t = w0 + 0x40;
    lq_sq(t, HERO_POS);
    swc1(t + 0xb0, 1.5f);
    sb(t + 0xc4, 0);
    sb(t + 0xd6, 0);
    sw(t + 0xc0, lwu(0x001413d0));
    const float g = lwc1(OV(0x00161ca0));
    swc1(t + 0xe0, 0.2f);
    swc1(t + 0xe4, g);
    swc1(t + 0xdc, g);
    swc1(t + 0xe8, 0.2f);
    swc1(t + 0xb8, u2f(0x3ba3d70au));
    sh(t + 0xc6, 0);
    sh(t + 0xd8, 0);
    sw(t + 0xb4, 0);
    sh(t + 0xda, 0);
    sw(t + 0xbc, 0);
    const uint32_t off = work(cam) + 0x130;
    swc1(off + 0x54, u2f(0x3b449ba6u));
    swc1(off + 0x30, 2.0f);
    swc1(off + 0x58, u2f(0x40947ae1u));
    swc1(off + 0x2c, u2f(0x40947ae1u));
    swc1(off + 0x48, u2f(0x3b449ba6u));
    for (uint32_t o : {0x34u, 0x38u, 0x28u, 0x20u, 0x24u}) sw(off + o, 0);
    sh(off + 0x3c, 0);
    sh(off + 0x3e, 0);
    sw(off + 0x44, 0);
    sw(off + 0x50, 0);
    const uint32_t w = work(cam);
    sh(w + 0x10, 1);
    sh(w + 0x12, 0);
    sh(w + 0x14, 0);
    sh(w + 0x16, 0);
    swc1(OV(0x0015ef40), 0.75f);
    const uint32_t w8 = work(cam), tm = w8 + 0x20;
    const float h = lwc1(OV(0x0015ef44));
    sh(w8 + 0x20, 0);
    sw(tm + 0x10, 0);
    sw(tm + 0xc, 0);
    sh(tm + 2, 0);
    sw(tm + 0x14, 0);
    sw(tm + 0x18, 0);
    const uint32_t s1 = work(cam) + 0x190;
    sw(s1, 0);
    swc1(s1 + 0x14, u2f(0x4164923au));
    swc1(s1 + 8, u2f(0x3ba3d70au));
    swc1(s1 + 0xc, 0.2f);
    swc1(s1 + 0x10, u2f(0x3e20d97cu));
    sw(s1 + 4, 0);
    const uint32_t s2 = work(cam) + 0x1a8;
    sw(s2, 0);
    swc1(s2 + 0x14, h);
    swc1(s2 + 0x18, u2f(0x3f060a92u));
    for (uint32_t o : {0x24u, 8u, 4u, 0xcu, 0x10u, 0x1cu, 0x20u}) sw(s2 + o, 0);
    const uint32_t p = work(cam) + 0x220;
    sw(p, 0);
    sw(p + 0x10, 0);
    sh(p + 4, 0);
    sh(p + 6, 0);
    swc1(p + 8, lwc1(off + 0x2c));
    swc1(p + 0xc, lwc1(OV(0x00161d1c)));
    const uint32_t w9 = work(cam);
    const int32_t frames = frame_scale_i(0x7d0);
    const uint32_t col = w9 + 0x1d0;
    sw(col + 0x30, 0);
    sh(col + 0x3a, 0);
    sh(col + 0x36, 0);
    sh(col + 0x38, 0);
    sw(col + 0x4c, 0);
    sw(col + 0x48, lwu(OV(0x0015ef60)));
    sh(col + 0x34, static_cast<uint32_t>(frames));
    swc1(col + 0x44, lwc1(OV(0x00161cdc)));
    swc1(col + 0x3c, lwc1(OV(0x00161ce4)));
    swc1(col + 0x40, lwc1(OV(0x00161ce0)));
    vec_clear(V(w9 + 0x1e0));
    const uint32_t wa = work(cam);
    sh(wa + 0x268, 0);
    sw(wa + 0x264, 0);
    lq_sq(wa + 0x240, HERO_POS);
    lq_sq(wa + 0x250, HERO_POS);
    if (lbu(cam + 0x7d) == 2) {
        cam0_from_transition(cam);
        lq_sq(cam + 0x40, cam);
        if (lh(lwu(OV(0x00166e04)) + 0x86) == 7 && lh(0x0013f65c) == 0) sh(w8 + 0x20, 0);
        else sh(tm, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d24)))));
    } else {
        cam0_reset_target(cam);
        cam0_place(1);
    }
    vec3_sub(V(off), V(cam + 0x30), V(t + 0x50));
    lq_sq(off + 0x10, off);
    lq_sq(w, cam + 0x30);
    lq_sq(col + 0x20, cam + 0x30);
    alignas(16) Vec4 a, b;
    vec3_scale(a, V(0x0013f5e0), vec3_dot(V(off), V(0x0013f5e0)));
    vec3_sub(b, V(off), a);
    const float d = lwc1(off + 0x2c) - vec3_length(b);
    swc1(col + 0x30, d);
    if (d < u2f(0xbdcccccdu)) sh(col + 0x3a, 1);
    swc1(tm + 8, lwc1(off + 0x30));
    swc1(tm + 4, lwc1(t + 0xb0));
    sh(cam + 0x7e, 0);
}

// orig: 0x002e46b8 — carry the camera and its target block along with the hero's motion
// (0x13f490); the height spring follows the hero (0x13f49c) unless on a lift (class 0x22d,
// for 30 frames); while swinging (states 0xb / 0xc) also the across-facing offset.
void cam0_follow_motion(uint32_t cam) {
    const uint32_t w = work(cam), t = w + 0x40;
    const Vec4 dv = V(0x0013f490);
    vec3_add(V(cam + 0x30), dv, V(cam + 0x30));
    vec3_add(V(w + 0xa0), dv, V(w + 0xa0));
    vec3_add(V(t), dv, V(t));
    vec3_add(V(w + 0xd0), dv, V(w + 0xd0));
    vec3_add(V(w), dv, V(w));
    if (lh(0x0013f658) == 0) {
        const uint32_t plat = lwu(0x0013f64c);
        if (plat == 0 || lh(plat + 0xa6) != 0x22d || frame_scale_i(0x1e) < lw(OV(0x00167018))) swc1(work(cam) + 0x1c8, lwc1(0x0013f49c));
    }
    if (static_cast<uint32_t>(lw(0x001413d4) - 0xb) < 2u) {
        const uint32_t w3 = work(cam);
        alignas(16) Vec4 side, a, b;
        sh(w3 + 0x10, 0);
        vec3_scale(side, V(OV(0x00166e90)), lwc1(OV(0x00166eb4)));
        vec3_scale(a, V(OV(0x00166e40)), vec3_dot(V(OV(0x00166e40)), dv));
        vec3_sub(a, dv, a);
        vec3_sub(side, side, a);
        vec3_add(V(cam + 0x30), side, V(cam + 0x30));
        vec3_add(V(t + 0x60), side, V(t + 0x60));
        vec3_add(V(t), side, V(t));
        vec3_add(V(t + 0x90), side, V(t + 0x90));
        vec3_add(V(w3), side, V(w3));
        (void)b;
    }
}

// orig: 0x002e48a0 — the facing component of the point h up from the hero, along 0x166e40.
void cam0_along_facing(Vec4& out, float h) {
    alignas(16) Vec4 p;
    vec3_scale(p, V(OV(0x00166e30)), h);
    vec3_add(p, p, V(HERO_POS));
    vec3_scale(out, V(OV(0x00166e40)), vec3_dot(V(OV(0x00166e40)), p));
}

// orig: 0x002e4580 — the angle of the point h above the hero from the camera's x row, in the
// plane of its y row (signed by the z row).
float cam0_hero_angle(uint32_t cam, float h) {
    alignas(16) Vec4 p, a, d, b;
    p = V(HERO_POS);
    vec3_scale(a, V(OV(0x00166e30)), h);
    vec3_add(a, a, p);
    vec3_sub(d, a, V(cam + 0x30));
    vec3_scale(b, V(cam + 0x10), vec3_dot(d, V(cam + 0x10)));
    vec3_sub(d, d, b);
    const float dot = vec3_dot(d, V(cam));
    const float len = vec3_length(d);
    if (len == 0.0f) return 0.0f;
    float r = u2f(0x3fc90fdbu) - asin_f(fpu_div(dot, len));
    if (vec3_dot(d, V(cam + 0x20)) < 0.0f) r = -r;
    return r;
}

// orig: 0x00257e20 — cubic through p1..p2 (Catmull-Rom style weights) at t.
float cubic_interp(float p0, float p1, float p2, float p3, float t) {
    float a = p0 - p1;
    float b = p3 - p2;
    const float t2 = t * t;
    const float c = (p2 - p0) * t;
    b = b - a;
    a = a - b;
    return ((b * (t2 * t) + a * t2) + c) + p1;
}

// The setters the camera modes use on the active follow camera (type id 0 only).
// orig: 0x002e8450 — the distance target +0x170 (and +0x188 when `both`), rate +0x178.
void cam0_set_distance(int32_t both, float d, float rate) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    const uint32_t o = work(cam) + 0x130;
    sh(o + 0x3c, 1);
    swc1(o + 0x48, rate);
    swc1(o + 0x40, d);
    if (both != 0) swc1(o + 0x58, d);
}
// orig: 0x002e84b8 — the height target +0x17c, rate +0x184.
void cam0_set_height(float h, float rate) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    const uint32_t o = work(cam) + 0x130;
    swc1(o + 0x54, rate);
    sh(o + 0x3e, 1);
    swc1(o + 0x4c, h);
}
// orig: 0x002e84f0 — the look height +0xf4 (relative to the last frame's when `rel`), rate +0xf8.
void cam0_set_look_height(int32_t rel, float h, float rate) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    const uint32_t t = work(cam) + 0x40;
    sh(t + 0xda, 1);
    swc1(t + 0xb8, rate);
    swc1(t + 0xb4, rel != 0 ? lwc1(lwu(OV(0x00168fc0)) + 0xf0) + h : h);
}
// orig: 0x002e8568 — the mode flag +0x10.
void cam0_set_mode(int32_t m) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    sh(work(cam) + 0x10, static_cast<uint32_t>(m));
}
// orig: 0x002e8590 — the spring rates +0x11c / +0x120 (0 keeps the old one).
void cam0_set_rates(float a, float b) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    const uint32_t t = work(cam) + 0x40;
    if (a != 0.0f) swc1(t + 0xdc, a);
    if (b != 0.0f) swc1(t + 0xe0, b);
}
// orig: 0x002e8620 — the flag +0x226.
void cam0_set_flag226() {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) == 0) sh(work(cam) + 0x226, 1);
}
// orig: 0x002e8648 — the flag +0x116.
void cam0_set_flag116() {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) == 0) sb(work(cam) + 0x116, 1);
}

// orig: 0x002e86b0 — the signed angle between `dir` and the camera's offset around the up
// axis 0x166e30; when within `limit` (or always when 0) it sets the turn rate +0x1c4 from a
// cubic of angle / (pi / 2). Returns the angle (0 for degenerate input).
float cam0_turn_toward(uint32_t cam, const Vec4& dir, float limit) {
    const uint32_t w = work(cam), s = w + 0x1a8, off = w + 0x130;
    alignas(16) Vec4 a, b, c, x;
    vec3_scale(a, V(OV(0x00166e30)), vec3_dot(dir, V(OV(0x00166e30))));
    vec3_sub(b, dir, a);
    vec3_scale(a, V(OV(0x00166e30)), vec3_dot(V(off), V(OV(0x00166e30))));
    vec3_sub(c, V(off), a);
    const float dot = vec3_dot(b, c);
    float l = vec3_length(b);
    if (l == 0.0f) return 0.0f;
    l = l * vec3_length(c);
    if (l == 0.0f) return 0.0f;
    float sgn = -1.0f;
    const float ang = u2f(0x3fc90fdbu) - asin_f(fpu_div(dot, l));
    vec3_cross_ba(x, b, V(OV(0x00166e30)));
    if (0.0f <= vec3_dot(x, c)) sgn = 1.0f;
    const float r = angle_sub(u2f(0x40490fdbu), ang);
    if (limit != 0.0f && limit < abs_f(r)) return r;
    float k = fpu_div(r, u2f(0x3fc90fdbu));
    if (1.0f < k) k = 1.0f;
    swc1(s + 0x1c, cubic_interp(-1.0f, 0.0f, 1.0f, 0.0f, k) * sgn);
    return r;
}
// orig: 0x002e88c8 — cam0_turn_toward the direction from the target to `pos`.
float cam0_turn_toward_point(uint32_t cam, uint32_t pos, float limit) {
    alignas(16) Vec4 d;
    vec3_sub(d, V(pos), V(work(cam) + 0x40));
    return cam0_turn_toward(cam, d, limit);
}
// orig: 0x002e8918 — set the turn speed +0x1bc (unless 0) and turn toward the direction `dir`.
void cam0_turn_dir(uint32_t dir, float speed, float limit) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0 || speed == 0.0f) return;
    swc1(work(cam) + 0x1bc, speed);
    cam0_turn_toward(cam, V(dir), limit);
}
// orig: 0x002e8970 — set the turn speed +0x1bc and turn toward the point `pos`.
void cam0_turn_point(uint32_t pos, float speed, float limit) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0) return;
    swc1(work(cam) + 0x1bc, speed);
    cam0_turn_toward_point(cam, pos, limit);
}
// orig: 0x002e8378 — empty in this build (stack frame only).
void cam0_nop_d(uint32_t) {}

// orig: (helper) jalr to the per-frame camera hook *(0x167010): the levels' own hooks (Aridia's 0x2fa710: the look height from the
// float 0x16760c; Eudora's 0x2f0448, set by 0x2f0410: its camera's look height follows a value)
void cam0_hook_call(uint32_t fn) {
    if (on_aridia() && fn == L2_ADDR(0x002fa710)) {
        cam_mode_hook_2fa710_l2(0);
        return;
    }
    if (on_eudora() && fn == L4_ADDR(0x002f0448)) {
        eud_cam0_hook_l4(0);
        return;
    }
    if (on_orxon() && orxon_camera_hook_call(fn)) return;  // (level 10's own: 0x2f4b18)
    if (on_pokitaru() && pokitaru_camera_hook_call(fn)) return;  // (level 11's own: 0x327a90)
    if (on_hoven() && hoven_camera_hook_call(fn)) return;  // (level 12's own: 0x3148f0)
    if (on_gemlik() && gemlik_camera_hook_call(fn)) return;  // (level 13's own: 0x316b48)
    if (on_oltanis() && oltanis_camera_hook_call(fn)) return;  // (level 14's own: 0x3131e0)
    if (on_quartu() && quartu_camera_hook_call(fn)) return;  // (level 15's own: 0x2f6980)
    if (on_kalebo() && kalebo_camera_hook_call(fn)) return;  // (level 16's own: 0x2f3a88)
    if (on_veldin_orbit() && veldin_orbit_camera_hook_call(fn)) return;  // (level 17's own: 0x2fff38)
    if (on_veldin_finale() && veldin_finale_camera_hook_call(fn)) return;  // (level 18's own: 0x305318)
    if (on_gaspar() && gaspar_camera_hook_call(fn)) return;  // (level 9's own: 0x314220)
    if (on_umbris() && umbris_camera_hook_call(fn)) return;  // (level 7's own: 0x328090)
    if (on_blarg() && blarg_camera_hook_call(fn)) return;  // (level 6's own: 0x3147b0)
    if (on_rilgar() && rilgar_camera_hook_call(fn)) return;  // (Rilgar's own: 0x328f80)
    if (on_kerwan() && fn == L3_ADDR(0x002eb408)) {  // (Kerwan's own: kerwan_camera_tr.cpp)
        ker_cam0_hook_l3(0);
        return;
    }
    TODO_PORT(fn, "camera mode hook")();
}

// orig: 0x002e6000 — the follow camera's per-frame mode choices by the hero's state: the
// distance / height / look targets for swinging, ladders, gadgets and vehicles, the turn
// toward a pushed object, the hook *(0x167010), the "something interesting nearby" flag
// +0x230, and the slow-down when the ground keeps changing (0x161d94).
void cam0_modes(uint32_t cam) {
    const uint32_t P = 0x0013f350;
    const uint32_t w = work(cam), p = w + 0x220;
    int32_t ladder = 0;
    if (lw(0x001415d4) == 0xd && lw(p + 0x10) != 0x14d) {
        const float a = angle_add(lwc1(P + 0x4e4), u2f(0x40490fdbu));
        alignas(16) float d[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // (w: stack garbage in the original, unused)
        d[0] = cos_vu(a);
        d[1] = sin_vu(a);
        d[2] = 0.0f;
        // 0x2e8918 with the direction on the stack
        const uint32_t c = lwu(OV(0x00166e00));
        if (lh(c + 0x86) == 0 && u2f(0x3e567750u) != 0.0f) {
            swc1(work(c) + 0x1bc, u2f(0x3e567750u));
            const Vec4 dv = Vec4{d[0], d[1], d[2], d[3]};
            cam0_turn_toward(c, dv, d[2]);
        }
        cam0_set_flag116();
    }
    if (lw(0x001413dc) == 0x11) {
        swc1(work(cam) + 0xf0, 0.25f);
        cam0_set_height(0.5f, u2f(0x3b449ba6u));
    }
    if (lw(0x001413dc) == 5) {
        ladder = 1;
        swc1(work(cam) + 0xf0, 0.25f);
        cam0_set_height(lbu(0x001413f4) == 1 ? 1.5f : 2.5f, u2f(0x3b449ba6u));
    }
    if (lw(0x001413dc) == 0x10) swc1(work(cam) + 0xf0, 0.25f);
    if (lbu(0x001413f4) == 1) {
        if (lw(p) == 0) {
            const uint32_t t = work(cam) + 0x40;
            cam0_set_distance(1, 3.0f, u2f(0x3b449ba6u));
            if (ladder == 0) {
                swc1(t + 0xb0, 1.0f);
                cam0_set_height(1.0f, u2f(0x3b449ba6u));
            }
        }
        swc1(work(cam) + 0x20c, u2f(0x3f19999au));
    }
    if (lbu(0x001413f4) == 2) {
        if (lw(p + 0x10) < 2) {
            cam0_set_look_height(0, lwc1(OV(0x00161d98)), u2f(0x3ba3d70au));
            cam0_set_distance(1, lwc1(OV(0x00161d9c)), u2f(0x3b449ba6u));
            cam0_set_height(lwc1(OV(0x00161da0)), u2f(0x3b449ba6u));
        }
        cam0_set_flag226();
        const uint32_t col = work(cam) + 0x1d0;
        swc1(col + 0x3c, lwc1(OV(0x00161da8)));
        swc1(col + 0x44, lwc1(OV(0x00161da4)));
    }
    if (lbu(0x001413fa) != 0 && lw(0x001413dc) != 0xf) {
        cam0_set_mode(0);
        cam0_set_flag116();
        cam0_set_rates(u2f(0x3d23d70au), u2f(0x3e4ccccdu));
    }
    if (lwu(OV(0x00167010)) != 0) cam0_hook_call(lwu(OV(0x00167010)));
    const uint32_t obj = lwu(OV(0x00166edc));
    if (obj == 0 || !(lw(p + 0x10) < 2)) {
        sw(p + 0x10, 0);
    } else if (static_cast<int8_t>(lbu(obj + 0x20)) < 0) {
        sw(OV(0x00166edc), 0);
        sw(p + 0x10, 0);
    } else {
        if (0.3f <= abs_f(lwc1(0x0013ca40)) || u2f(0x3e99999au) <= abs_f(lwc1(0x0013ca44))) sw(OV(0x00166ee0), 0);
        sw(OV(0x00166ee0), lwu(OV(0x00166ee0)) + 1);
        if (frame_scale_i(0x190) < lw(OV(0x00166ee0))) sw(OV(0x00166ee0), static_cast<uint32_t>(frame_scale_i(0x190)));
        float f = int_to_float(lw(OV(0x00166ee0)));
        f = fpu_div(f, int_to_float(frame_scale_i(0x190)));
        cam0_turn_point(lwu(OV(0x00166edc)) + 0x10, f * u2f(0x3e567750u), 0.0f);
        sw(p + 0x10, 0);
    }
    const int32_t n = mobys_in_sphere(HERO_POS, 15.0f, 1, lwu(0x001413d0), 0);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t pv = moby_pvar_word0(lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4));
        if (pv == 0) continue;
        if (static_cast<float>(lbu(pv + 0xd)) == 0.0f) continue;
        sw(p + 0x10, 1);
        break;
    }
    if (lw(0x001413d4) == 0x81 && (lw(0x0013cae0) & 3) != 0) {
        cam0_set_mode(0);
        cam0_set_flag116();
        cam0_set_rates(u2f(0x3d23d70au), u2f(0x3e4ccccdu));
        const uint32_t w2 = work(cam);
        sh(w2 + 0x14, 0);
        sh(w2 + 0x16, 0);
        sh(w2 + 0x12, 0);
        return;
    }
    if (lw(OV(0x00161d94)) == 0) return;
    const uint32_t w3 = work(cam);
    float sum = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) sum = sum + angle_dist(lwc1(OV(0x00166ebc) + i * 4), lwc1(OV(0x00166ec0) + i * 4));
    sum = sum * 0.25f;
    float r = sum * 5.0f;
    if (1.0f < r) r = 1.0f;
    r = 1.0f - r;
    int16_t& slow = ee_ref<int16_t>(w3 + 0x12);
    int16_t& rest = ee_ref<int16_t>(w3 + 0x14);
    if (r < lwc1(OV(0x00161d84))) sh(w3 + 0x12, static_cast<uint32_t>(frame_scale_i(0x20)));
    if (lh(w3 + 0x12) != 0) {
        if (timer_tick_i16(slow) != 0) sh(w3 + 0x14, static_cast<uint32_t>(frame_scale_i(0x2d)));
        cam0_set_flag116();
        const int16_t k = static_cast<int16_t>(lhu(w3 + 0x16) + 1);
        sh(w3 + 0x16, static_cast<uint16_t>(k));
        if (frame_scale_i(0x1e) < k) sh(w3 + 0x16, static_cast<uint32_t>(frame_scale_i(0x1e)));
        float f = int_to_float(lh(w3 + 0x16));
        f = fpu_div(f, int_to_float(frame_scale_i(0x1e)));
        cam0_set_rates((lwc1(OV(0x00161d88)) - u2f(0x3c75c28fu)) * f + u2f(0x3c75c28fu), u2f(0x3e4ccccdu));
        swc1(OV(0x0015ef40), 0.0f);
        return;
    }
    if (lh(w3 + 0x14) == 0) {
        sh(w3 + 0x16, 0);
        swc1(OV(0x0015ef40), 0.75f);
        return;
    }
    timer_tick_i16(rest);
    float f = int_to_float(lh(w3 + 0x14));
    f = fpu_div(f, int_to_float(frame_scale_i(0x2d)));
    swc1(OV(0x0015ef40), f * -0.75f + 0.75f);
    sh(w3 + 0x16, 0);
}

// orig: 0x002e4930 — the target kind T+0xc4 (previous one +0xc5) by the hero's state:
// 1 hang / swing, 2 / 4 ledges, 3 / 5 / 6 / 8 swimming and wall states, 9 / 10 the grind;
// each change starts its blend (frames T+0xc6, step T+0xc8) and, for some, a lead along
// the facing (T+0xd0, T+0x30).
void cam0_target_kind(uint32_t cam) {
    const uint32_t w = work(cam), t = w + 0x40;
    const int32_t st = lw(0x001413dc);
    auto blend = [&](int32_t frames) {  // 0x2e4fa8
        sh(t + 0xc6, static_cast<uint32_t>(frames));
        swc1(t + 0xc8, fpu_div(1.0f, int_to_float(static_cast<int16_t>(frames))));
    };
    auto blend_fs = [&](uint32_t gp_word) { blend(frame_scale_i(lw(gp_word))); };
    auto set = [&](uint32_t from, uint32_t to) {
        sb(t + 0xc5, from);
        sb(t + 0xc4, to);
    };
    auto lead = [&](float f) {  // 0x2e4be4 / 0x2e4d58
        cam0_along_facing(V(t + 0x30), f);
        lq_sq(t + 0x20, t + 0x10);
    };
    auto lead_kind = [&](uint32_t lead_frames_gp, uint32_t lead_gp) {  // kinds 3 / 5 / 6
        const int32_t v = frame_scale_i(lw(lead_frames_gp));
        sh(t + 0xd4, static_cast<uint32_t>(v));
        swc1(t + 0xcc, fpu_div(1.0f, int_to_float(static_cast<int16_t>(v))));
        const uint16_t v2 = lhu(lead_gp == OV(0x00161d68) ? OV(0x00161d60u) : OV(0x00161d50u));
        sh(t + 0xc6, v2);
        const float s = fpu_div(1.0f, int_to_float(static_cast<int16_t>(v2)));
        sb(t + 0xd7, 0);
        swc1(t + 0xd0, lwc1(lead_gp));
        swc1(t + 0xc8, s);
        lead(lwc1(lead_gp));
    };
    const uint32_t k = lbu(t + 0xc4);
    if (k == 0) {
        if (st == 2) {
            set(0, 1);
            blend(lhu(OV(0x00161d3c)));
            return;
        }
        const int32_t hs = lw(0x001413d4);
        const bool still = lwc1(OV(0x00166eec)) == 0.0f;
        if (hs == 0xf && still) {
            set(0, 3);
            lead_kind(OV(0x00161d4c), OV(0x00161d58));
            return;
        }
        if (hs == 0xc && still) {
            set(lbu(t + 0xc4), 8);
            blend(lhu(OV(0x00161d60)));
            return;
        }
        if (hs == 0xb && still) {
            set(lbu(t + 0xc4), 6);
            lead_kind(OV(0x00161d40), OV(0x00161d48));
            return;
        }
        if (static_cast<uint32_t>(hs - 0xd) < 2u && still) {
            set(lbu(t + 0xc4), 5);
            lead_kind(OV(0x00161d5c), OV(0x00161d68));
            return;
        }
        if (st == 4 && still) {
            set(lbu(t + 0xc4), 2);
            blend(lhu(OV(0x00161d38)));
        }
        return;
    }
    const int32_t hs = lw(0x001413d4);
    if (k != 9 && k != 1 && hs == 0x14) {
        set(k, 9);
        const int32_t v = frame_scale_i(lw(OV(0x00161d6c)));
        sh(t + 0xd4, static_cast<uint32_t>(v));
        const float s = fpu_div(1.0f, int_to_float(static_cast<int16_t>(v)));
        sb(t + 0xd7, 0);
        sh(t + 0xc6, 0);
        swc1(t + 0xc8, 1.0f);
        swc1(t + 0xd0, lwc1(OV(0x00161d7c)));
        swc1(t + 0xcc, s);
        lead(lwc1(OV(0x00161d7c)));
        return;
    }
    if (k - 1 < 2u && st != 4 && st != 2) {
        sb(t + 0xc4, 0);
        swc1(t + 0xc8, 1.0f);
        sh(t + 0xc6, 0);
        return;
    }
    if (k == 7) {
        if (lh(0x0013f76e) == 0) {
            set(k, 6);
            lead_kind(OV(0x00161d40), OV(0x00161d48));
            return;
        }
        if (st == 2) {
            set(k, 1);
            blend_fs(OV(0x00161d54));
            return;
        }
    }
    if (k == 4 && st != 4) {
        set(4, st == 2 ? 1u : 0u);
        blend_fs(OV(0x00161d54));
        return;
    }
    if (k == 3 && hs == 0xb) {
        set(k, 6);
        lead_kind(OV(0x00161d40), OV(0x00161d48));
        return;
    }
    if (k == 5 && hs == 0xc) {
        set(k, 8);
        blend(lhu(OV(0x00161d60)));
        return;
    }
    if (k == 8 && hs != 0xc) {
        set(k, 0);
        blend_fs(OV(0x00161d64));
        return;
    }
    if (k == 6 && hs != 0xb) {
        set(k, 0);
        blend_fs(OV(0x00161d54));
        return;
    }
    if ((k == 3 || k == 5) && st != 4) {
        set(k, 0);
        blend_fs(OV(0x00161d54));
        return;
    }
    auto to_hang = [&] {  // 0x2e4eb8
        set(lbu(t + 0xc4), 1);
        const int32_t v = frame_scale_i(lw(OV(0x00161d54)));
        sh(t + 0xc6, static_cast<uint32_t>(v));
        swc1(t + 0xc8, fpu_div(1.0f, int_to_float(static_cast<int16_t>(v))));
        sh(work(cam) + 0x20, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d24)))));
        lq_sq(cam + 0x40, cam);
    };
    const uint32_t k2 = lbu(t + 0xc4);
    if (k2 == 9) {
        if (st == 2 || !(lh(0x0013f65c) < frame_scale_i(0x37))) {
            to_hang();
            return;
        }
    }
    if (k2 == 9 || lbu(t + 0xc4) == 0xa) {
        if (lbu(t + 0xc4) == 0xa && (st == 2 || !(lh(0x0013f65c) < frame_scale_i(0x37)))) {
            to_hang();
            return;
        }
    }
    const uint32_t k3 = lbu(t + 0xc4);
    if (k3 == 9 && hs != 0x14) {
        set(k3, 0xa);
        blend(lhu(OV(0x00161d78)));
        return;
    }
    if (lbu(t + 0xc4) == 0xa) {
        if (timer_tick_i16(ee_ref<int16_t>(t + 0xd4)) != 0) {
            set(lbu(t + 0xc4), 0);
            blend_fs(OV(0x00161d54));
            return;
        }
    }
    const uint32_t k4 = lbu(t + 0xc4);
    if (k4 == 5 && hs == 0x10) {
        set(k4, 0);
        blend_fs(OV(0x00161d64));
    }
}

// orig: 0x002e4fe8 — the target T (the hero position less its facing component) and the
// facing offset `out` by the target kind: plain, hang (from the point above the hero),
// the blended leads of kinds 3 / 5..7 (ending in 4 / 7 / 10), ledges, and the grind.
void cam0_target_offset(uint32_t cam, Vec4& out) {
    const uint32_t w = work(cam), t = w + 0x40;
    const Vec4 hero = V(HERO_POS);
    const Vec4& f = V(OV(0x00166e40));
    alignas(16) Vec4 a;
    auto plain = [&] {  // 0x2e555c
        vec3_scale(out, f, vec3_dot(hero, f));
        vec3_sub(V(t), hero, out);
    };
    if (lw(0x001413d4) == 0x77) {
        vec3_scale(out, f, vec3_dot(V(t), f));
        vec3_sub(V(t), V(t), out);
        vec3_scale(out, f, vec3_dot(f, V(w + 0xa0)));
        V(w + 0x50) = out;
        if (lbu(t + 0xc4) != 0xb) {
            sb(t + 0xc4, 0xb);
            lq_sq(cam + 0x40, cam);
            sh(work(cam) + 0x20, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d24)))));
        }
        return;
    }
    const uint32_t kind = lbu(t + 0xc4);
    switch (kind) {
    case 2: {
        bool to_hang = false;
        if (lw(0x001413d4) == 0x11) {
            to_hang = true;
        } else if (lh(0x0013f76e) != 0 && u2f(0x3e4ccccdu) < lwc1(0x0013f62c)) {
            const float r = cam0_hero_angle(cam, lwc1(t + 0xb0));
            if (r < lwc1(OV(0x00161d10)) || lwc1(OV(0x00161d0c)) < r) to_hang = true;
        }
        if (to_hang) {
            sb(t + 0xc4, 1);
            const uint16_t v = lhu(OV(0x00161d3c));
            sh(t + 0xc6, v);
            swc1(t + 0xc8, fpu_div(1.0f, int_to_float(static_cast<int16_t>(v))));
        }
        int32_t keep = 0;
        bool call = true;
        if (lh(0x0013f65c) != 0 && lwc1(OV(0x00166eec)) == 0.0f) {
            if (lh(0x0013f76e) == 0 || !(lw(0x0013f79c) < 0x10)) {
                keep = 1;
                call = false;
            }
        }
        alignas(16) Vec4 p;
        if (call) {
            if (cam0_target_v(p, 1) != 1) keep = 1;
            else if (vec3_dot(p, f) < vec3_length(V(t + 0x10))) keep = 1;
        }
        if (keep == 0) {
            cam0_target_v(p, 1);
            vec3_scale(out, f, vec3_dot(p, f));
            vec3_sub(V(t), p, out);
            return;
        }
        const float eps = u2f(0x3a83126fu);
        const bool turned = eps < abs_f(lwc1(OV(0x00166e40)) - lwc1(OV(0x00166e50))) || eps < abs_f(lwc1(OV(0x00166e44)) - lwc1(OV(0x00166e54))) ||
                            eps < abs_f(lwc1(OV(0x00166e48)) - lwc1(OV(0x00166e58)));
        if (turned) {
            vec3_scale(out, f, vec3_dot(hero, f));
            vec3_sub(V(t), hero, out);
        } else {
            out = V(t + 0x10);
            vec3_scale(a, f, vec3_dot(hero, f));
            vec3_sub(V(t), hero, a);
        }
        return;
    }
    case 3:
    case 5:
    case 6:
    case 7: {
        vec3_lerp(V(t + 0x10), V(t + 0x30), V(t + 0x20), static_cast<float>(lh(t + 0xd4)) * lwc1(t + 0xcc));
        if (lbu(t + 0xd7) == 0 && sphere_collide(cam + 0x30, u2f(0x3e4ccccdu), 0x12, 0) != 0) {
            lq_sq(t + 0x30, t + 0x10);
            sb(t + 0xd7, 1);
        }
        out = V(t + 0x10);
        vec3_scale(a, f, vec3_dot(hero, f));
        vec3_sub(V(t), hero, a);
        if (timer_tick_i16(ee_ref<int16_t>(t + 0xd4)) == 0) return;
        const uint32_t k = lbu(t + 0xc4);
        if (k == 9) {
            sb(t + 0xc4, 0xa);
            sh(t + 0xd4, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d6c)))));
            lq_sq(t + 0x20, t + 0x10);
            vec3_scale(a, f, lwc1(t + 0xd0));
            vec3_sub(V(t + 0x30), V(t + 0x20), a);
        } else if (k == 6) {
            sb(t + 0xc4, 7);
            sh(t + 0xd4, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d40)))));
            lq_sq(t + 0x30, t + 0x20);
            lq_sq(t + 0x20, t + 0x10);
        } else {
            sb(t + 0xc4, 4);
            const uint16_t v = lhu(OV(0x00161d54));
            sh(t + 0xc6, v);
            swc1(t + 0xc8, fpu_div(1.0f, int_to_float(static_cast<int16_t>(v))));
        }
        return;
    }
    case 8:
        out = V(t + 0x10);
        vec3_scale(a, f, vec3_dot(hero, f));
        vec3_sub(V(t), hero, a);
        return;
    case 9:
    case 10:
        vec3_scale(out, f, vec3_dot(hero, f));
        vec3_sub(V(t), hero, out);
        vec3_lerp(out, out, V(t + 0x20), lwc1(OV(0x00161d80)));
        return;
    default:
        plain();
        return;
    }
}

// orig: 0x002e55b8 — spring the vector at `cur` toward `target` (velocities W+0xe0..).
void cam0_spring_vec(uint32_t cam, uint32_t cur, const Vec4& target, float k, float d, float max) {
    const uint32_t w = work(cam);
    swc1(cur, camera_spring(lwc1(cur), target.x, k, d, max, w + 0xe0));
    swc1(cur + 4, camera_spring(lwc1(cur + 4), target.y, k, d, max, w + 0xe4));
    swc1(cur + 8, camera_spring(lwc1(cur + 8), target.z, k, d, max, w + 0xe8));
}

// orig: 0x002e5678 — the look point for target kind `kind`: `add` plus the hero (kind 1) or
// the target T; returns 0 for kinds 3..7.
int32_t cam0_look_point(uint32_t cam, const Vec4& add, Vec4& out, uint32_t kind) {
    const uint32_t t = work(cam) + 0x40;
    const uint32_t k = kind - 1;
    if (k == 0) {
        vec3_add(out, add, V(HERO_POS));
        return 1;
    }
    vec3_add(out, add, V(t));
    return (k < 7 && k >= 2) ? 0 : 1;
}

// orig: 0x002e5730 — spring the look point T+0x90 toward the kind's look point, with the
// rates blended from the previous kind's (0x161d30 / 0x161d34) to the new (0x161d28 / 0x161d2c).
void cam0_look_update(uint32_t cam, const Vec4& add) {
    const uint32_t t = work(cam) + 0x40;
    if (timer_tick_i16(ee_ref<int16_t>(work(cam) + 0x106)) != 0) sb(t + 0xc5, lbu(t + 0xc4));
    const float b = static_cast<float>(lh(t + 0xc6)) * lwc1(t + 0xc8);
    alignas(16) Vec4 p;
    cam0_look_point(cam, add, p, lbu(t + 0xc4));
    const uint32_t k = lbu(t + 0xc4);
    float rk, rd;
    if (k == 3 || k == 5 || k == 6 || k == 7 || k == 1) {
        const float k0 = lwc1(OV(0x00161d30)), d0 = lwc1(OV(0x00161d34));
        rk = k0 + (lwc1(OV(0x00161d28)) - k0) * b;
        rd = d0 + (lwc1(OV(0x00161d2c)) - d0) * b;
    } else {
        rk = lwc1(OV(0x00161d28));
        rd = lwc1(OV(0x00161d2c));
    }
    cam0_spring_vec(cam, t + 0x90, p, rk, rd, 0.0f);
}

// orig: 0x002e5830 — the target block per frame: the target and its facing offset, springs
// for the offset +0xa0 (rates T+0xe4 / T+0xe8 along the facing, T+0xdc / T+0xe0 across),
// the look point, and the height point +0x90 kept clear of floors and ceilings.
void cam0_target_update(uint32_t cam) {
    const uint32_t w = work(cam), t = w + 0x40, off = w + 0x130;
    alignas(16) Vec4 hero, o, f, across, up, p, top, bot, d, m;
    hero = V(HERO_POS);
    cam0_target_kind(cam);
    cam0_target_offset(cam, o);
    V(w + 0x50) = o;
    const Vec4 old = V(t);
    (void)old;
    vec3_scale(f, V(OV(0x00166e40)), vec3_dot(V(OV(0x00166e40)), V(w + 0xa0)));
    vec3_sub(across, V(w + 0xa0), f);
    f.x = camera_spring(f.x, lwc1(t + 0x10), lwc1(t + 0xe4), lwc1(t + 0xe8), 0.0f, w + 0xc0);
    f.y = camera_spring(f.y, lwc1(t + 0x14), lwc1(t + 0xe4), lwc1(t + 0xe8), 0.0f, w + 0xc4);
    f.z = camera_spring(f.z, lwc1(t + 0x18), lwc1(t + 0xe4), lwc1(t + 0xe8), 0.0f, w + 0xc8);
    across.x = camera_spring(across.x, lwc1(w + 0x40), lwc1(t + 0xdc), lwc1(t + 0xe0), 0.0f, w + 0xb0);
    across.y = camera_spring(across.y, lwc1(t + 4), lwc1(t + 0xdc), lwc1(t + 0xe0), 0.0f, w + 0xb4);
    across.z = camera_spring(across.z, lwc1(t + 8), lwc1(t + 0xdc), lwc1(t + 0xe0), 0.0f, w + 0xb8);
    vec3_add(V(t), V(t), V(w + 0x50));
    vec3_add(V(w + 0xa0), across, f);
    vec3_scale(up, V(OV(0x00166e30)), lwc1(t + 0xb0));
    int16_t& n = ee_ref<int16_t>(t + 0xd8);
    if (lbu(t + 0xd6) != 0) {
        const int16_t v = static_cast<int16_t>(lhu(t + 0xd8) + 1);
        sh(t + 0xd8, static_cast<uint16_t>(v));
        if (frame_scale_i(lw(OV(0x00161d20))) < v) sh(t + 0xd8, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161d20)))));
    } else {
        timer_tick_i16(n);
        if (lh(t + 0xd8) < 0) sh(t + 0xd8, 0);
    }
    {
        const float fr = int_to_float(frame_scale_i(lw(OV(0x00161d20))));
        vec3_lerp(m, V(t), V(w + 0xa0), fpu_div(static_cast<float>(lh(t + 0xd8)), fr));
        vec3_add(V(w + 0x80), up, m);
    }
    cam0_look_update(cam, up);
    const uint32_t mode = lbu(0x001413f4);
    vec3_scale(up, V(OV(0x00166e30)), mode == 2 ? 3.0f : (mode == 1 ? u2f(0x3ecccccdu) : 0.5f));
    vec3_add(p, hero, up);
    vec3_scale(up, V(OV(0x00166e30)), lwc1(off + 0x30));
    vec3_add(V(t + 0x50), up, V(t));
    const uint32_t aw = work(lwu(OV(0x00166e00)));
    const float half = lwc1(OV(0x00161cd8)) + (lwc1(aw + 0x214) * lwc1(aw + 0x20c)) * (lwc1(aw + 0x210) - 1.0f);
    vec3_scale(top, V(OV(0x00166e30)), 20.0f);
    vec3_add(top, p, top);
    if (segment_collide_v(p, top, lwu(OV(0x0015ef58)), lwu(0x001413d0), nullptr) != 0) top = V(OV(0x00173e60));
    vec3_scale(bot, V(OV(0x00166e30)), 20.0f);
    vec3_sub(bot, p, bot);
    if (segment_collide_v(p, bot, lwu(OV(0x0015ef58)), lwu(0x001413d0), nullptr) != 0) bot = V(OV(0x00173e60));
    vec3_sub(d, bot, top);
    alignas(16) Vec4 h;
    if (vec3_length(d) < half + half) {
        vec3_add(h, top, bot);
        vec3_scale(h, h, 0.5f);
    } else {
        vec3_sub(d, top, V(t));
        const float room = lwc1(off + 0x30);
        if (!(vec3_length(d) <= room + half)) {
            if (vec3_dot(bot, V(OV(0x00166e40))) < vec3_dot(V(t), V(OV(0x00166e40)))) bot = V(t);
            vec3_scale(up, V(OV(0x00166e30)), room);
            vec3_add(V(t + 0x50), up, bot);
            return;
        }
        vec3_scale(h, V(OV(0x00166e30)), half);
        vec3_sub(h, top, h);
    }
    V(t + 0x50) = h;
    vec3_sub(d, h, V(t));
    cam0_set_height(vec3_dot(d, V(OV(0x00166e30))), u2f(0x3b449ba6u));
}

// orig: 0x001ee318 — is pos inside the camera volume `rec` (+0x10 type, +0x14 index)? 3: box
// (0x1600ec), 5: sphere grown by r (0x1600f4), 6: cylinder (0x1600fc), 7: capsule (0x1600e4).
int32_t cam_volume_contains(const Vec4& pos, uint32_t rec, float r) {
    const Vec4 p = pos;
    const int32_t type = lw(rec + 0x10);
    alignas(16) Vec4 d, l;
    if (type == 5) {
        const uint32_t s = lwu(OV(0x001600f4)) + lwu(rec + 0x14) * 0x80;
        const float rad = vec3_length(V(s)) + r;
        return vec3_distance(p, V(s + 0x30)) < rad ? 1 : 0;
    }
    if (type == 3) {
        const uint32_t s = lwu(OV(0x001600ec)) + lwu(rec + 0x14) * 0x80;
        vec3_sub(d, p, V(s + 0x30));
        d.w = 0.0f;
        mat3_transform(l, d, M(s + 0x40));
        if (!(-1.0f <= l.x) || !(l.x <= 1.0f)) return 0;
        if (!(-1.0f <= l.y) || !(l.y <= 1.0f)) return 0;
        if (!(-1.0f <= l.z)) return 0;
        return l.z <= 1.0f ? 1 : 0;
    }
    if (type == 6) {
        const uint32_t s = lwu(OV(0x001600fc)) + lwu(rec + 0x14) * 0x80;
        vec3_sub(d, p, V(s + 0x30));
        d.w = 0.0f;
        mat3_transform(l, d, M(s + 0x40));
        if (!(vec2_length(l) < 1.0f)) return 0;
        if (!(-1.0f <= l.z)) return 0;
        return l.z <= 1.0f ? 1 : 0;
    }
    if (type == 7) {
        const uint32_t s = lwu(OV(0x001600e4)) + lwu(rec + 0x14) * 0x90;
        vec3_sub(d, p, V(s + 0x30));
        d.w = 0.0f;
        mat3_transform(l, d, M(s + 0x40));
        if (vec2_length(l) < 1.0f && -1.0f <= l.z && l.z <= 1.0f) return 1;
        alignas(16) Vec4 c, q;
        Vec4 e = Vec4{0.0f, 0.0f, 1.0f, 1.0f};
        mat3_transform(c, e, M(s));
        vec3_add(c, c, V(s + 0x30));
        vec3_sub(q, p, c);
        if (vec3_length(q) < lwc1(s + 0x80)) return 1;
        e = Vec4{0.0f, 0.0f, -1.0f, 1.0f};
        mat3_transform(c, e, M(s));
        vec3_add(c, c, V(s + 0x30));
        vec3_sub(q, p, c);
        return vec3_length(q) < lwc1(s + 0x80) ? 1 : 0;
    }
    return 0;
}

// orig: 0x001ee1b0 — the camera volume record (0x30 each, in the 64 x 64 grid of 16-unit
// cells at *(0x15ef80)) that contains pos (grown by r), or 0.
uint32_t cam_volume_find(const Vec4& pos, float r) {
    const Vec4 p = pos;
    const int32_t x = float_to_int(p.x * 0.0625f);
    const int32_t y = float_to_int(p.y * 0.0625f);
    if (x < 0 || y < 0 || !(x < 0x41) || !(y < 0x41)) {
        debug_printf_stub();
        return 0;
    }
    const uint32_t tab = lwu(OV(0x0015ef80));
    const uint32_t off = lwu(tab + static_cast<uint32_t>(y * 64 + x) * 4);
    if (off == 0) return 0;
    const uint32_t list = off + tab;
    for (int32_t i = 0; i < lw(list); ++i) {
        const uint32_t e = list + 0x10 + static_cast<uint32_t>(i) * 0x30;
        alignas(16) Vec4 d;
        vec3_sub(d, V(e), p);
        const float rad = lwc1(e + 0xc);
        d.w = 1.0f;
        if (vec2_length(d) < rad + r && cam_volume_contains(p, e, r) != 0) return e;
    }
    return 0;
}

// orig: 0x002e4290 — is pos inside a camera volume?
int32_t cam0_in_volume(const Vec4& pos) { return cam_volume_find(pos, 0.0f) != 0 ? 1 : 0; }

// orig: 0x002e4180 — push pos out of the sphere volume around it (r larger): 0x173e70 the
// pushed-out point, 0x173e60 the surface point. Returns 1 when inside one.
int32_t cam0_volume_push(const Vec4& pos, float r) {
    const Vec4 p = pos;
    const uint32_t e = cam_volume_find(p, r);
    if (e == 0) return 0;
    const uint32_t s = lwu(OV(0x001600f4)) + lwu(e + 0x14) * 0x80;
    alignas(16) Vec4 d;
    vec3_sub(d, pos, V(s + 0x30));
    const float rad = vec3_length(V(s));
    const float k = fpu_div(1.0f, vec3_length(d));
    vec3_scale(V(OV(0x00173e70)), d, (rad + r) * k);
    vec3_scale(V(OV(0x00173e60)), d, rad * k);
    vec3_add(V(OV(0x00173e70)), V(OV(0x00173e70)), V(s + 0x30));
    vec3_add(V(OV(0x00173e60)), V(OV(0x00173e60)), V(s + 0x30));
    return 1;
}

namespace {
// orig: (helper) the collision half-size of the active camera: 0x161cd8 + its zoom terms
float cam_radius() {
    const uint32_t aw = work(lwu(OV(0x00166e00)));
    return lwc1(OV(0x00161cd8)) + (lwc1(aw + 0x214) * lwc1(aw + 0x20c)) * (lwc1(aw + 0x210) - 1.0f);
}
}  // namespace

// orig: 0x002e7138 — the camera offset W+0x130 toward the collision-free point `pos`; the
// pull-in W+0x200 follows (kept at least 0x161cc8 out). `hit_volume` / `hit_wall` from the
// probe; returns 1 when the camera had to come in.
int32_t cam0_apply_probe(uint32_t cam, const Vec4& pos, int32_t hit_volume, int32_t hit_wall) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0, t = w + 0x40;
    const float before = vec3_length(V(off));
    vec3_sub(V(off), pos, V(w + 0x90));
    if (lh(col + 0x38) != 0) {
        vec3_set_length(V(off), V(off), lwc1(off + 0x2c) - lwc1(col + 0x30));
        return hit_wall;
    }
    float len = vec3_length(V(off));
    if (lbu(0x001413f4) != 2 && u2f(0x3d4ccccdu) < abs_f(lwc1(0x0013ca40)) && hit_wall != 0) {
        const float s = lwc1(OV(0x00161cb8)) * lwc1(0x0015ed60);
        len = len - s;
        sh(col + 0x34, static_cast<uint32_t>(frame_scale_i(0x884)));
    }
    const float min = lwc1(OV(0x00161cc8));
    if (hit_volume == 0 && lw(0x001413dc) != 2 && before < len) {
        vec3_scale(V(off), V(off), fpu_div(before, len));
        swc1(col + 0x30, lwc1(off + 0x2c) - before);
        return hit_wall;
    }
    if (len < min) {
        if (len <= u2f(0x38d1b717u)) vec3_set_length(V(off), V(lwu(t + 0xc0) + 0xc0), -min);
        else vec3_scale(V(off), V(off), fpu_div(min, len));
        len = lwc1(OV(0x00161cc8));
    }
    swc1(col + 0x30, lwc1(off + 0x2c) - len);
    return 1;
}

// orig: 0x002e7318 — probe the camera position (target point + offset) out of volumes and
// walls, up to three times; the side the wall is on goes to W+0x208 (1 / -1 / 0).
int32_t cam0_probe(uint32_t cam) {
    const uint32_t w = work(cam), col = w + 0x1d0, off = w + 0x130, t = w + 0x40;
    int32_t hit_volume = 0, hit_wall = 0;
    alignas(16) Vec4 p, d;
    vec3_add(p, V(w + 0x90), V(off));
    sh(col + 0x38, 2);
    const uint32_t mask = lwu(col + 0x48);
    const float r = cam_radius();
    for (int32_t i = 0; i < 3; ++i) {
        if (cam0_volume_push(p, r) != 0) {
            hit_volume = 1;
        } else if (sphere_collide_v(p, r, mask, lwu(OV(0x00166ed4))) != 0) {
            hit_wall = 1;
        } else {
            break;
        }
        if (lh(col + 0x38) != 0) {
            vec3_sub(d, V(OV(0x00173e60)), p);
            vec3_set_length(d, d, 1.0f);
            const float f = vec3_dot(V(OV(0x00166e40)), d);
            if (0.75f < f) sh(col + 0x38, 1);
            else if (f < -0.75f) sh(col + 0x38, 0xffff);
            else sh(col + 0x38, 0);
        }
        p = V(OV(0x00173e70));
    }
    const int32_t r2 = cam0_apply_probe(cam, p, hit_volume, hit_wall);
    vec3_add(V(col + 0x20), V(t + 0x50), V(off));
    if (lh(col + 0x38) == 2) sh(col + 0x38, 0);
    return r2;
}

// orig: 0x002e7520 — returns 0 in this build.
int32_t cam0_probe_none(uint32_t) { return 0; }

// orig: 0x002e7530 — move `a` by `dist` along (a - b), then out = (the point `back` from the
// look point W+0x90 toward it) - c.
void cam0_slide_step(uint32_t cam, Vec4& out, Vec4& a, const Vec4& b, const Vec4& c, float dist, float back) {
    const uint32_t w = work(cam);
    alignas(16) Vec4 d, e, f;
    vec3_sub(d, a, b);
    vec3_set_length(e, d, dist);
    vec3_add(a, a, e);
    vec3_sub(f, a, V(w + 0x90));
    vec3_set_length(f, f, back);
    alignas(16) Vec4 g;
    vec3_add(g, V(w + 0x90), f);
    vec3_sub(out, g, c);
}

// orig: 0x002e7618 — slide the probe point pos around the hit (up to three steps) while the
// segment from the look point stays too close to it; returns 1 when it slid.
int32_t cam0_slide(uint32_t cam, Vec4& pos, float back, float r) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0;
    const float rr = cam_radius();
    int32_t slid = 0;
    alignas(16) Vec4 acc, start, hit, push, end, near, step;
    vec_clear(acc);
    start = pos;
    hit = V(OV(0x00173e60));
    push = V(OV(0x00173e70));
    vec3_add(end, V(w + 0x90), V(off));
    auto need = [&](float& gap) {  // the clearance the camera needs at the nearest point
        const float dn = closest_point_on_segment_r(near, hit, V(w + 0x90), end, 0.0f);
        const float dl = vec3_distance(V(w + 0x90), near);
        const float c0 = lwc1(OV(0x00161cd8));
        const float k = fpu_div(dl, lwc1(off + 0x2c) - lwc1(col + 0x30));
        const float want = (c0 + (rr - c0) * k) * 0.75f;
        gap = want - dn;
        return dn < want;
    };
    float gap;
    if (need(gap)) {
        slid = 1;
        cam0_slide_step(cam, acc, near, hit, start, gap, back);
        pos = push;
        if (sphere_collide_v(pos, r, lwu(OV(0x0015ef5c)), lwu(0x001413d0)) != 0) {
            for (int32_t i = 0; i < 2; ++i) {
                vec3_sub(step, start, pos);
                vec3_add(hit, V(OV(0x00173e60)), step);
                if (!need(gap)) break;
                cam0_slide_step(cam, step, near, hit, start, gap, back);
                vec3_add(acc, acc, step);
                if (i == 1) break;
                pos = V(OV(0x00173e70));
                if (sphere_collide_v(pos, r, lwu(OV(0x0015ef5c)), lwu(0x001413d0)) == 0) break;
            }
        }
    }
    vec3_add(pos, start, acc);
    return slid;
}

// orig: 0x002e79c8 — for the first probe step: is the hit point (0x173e60) behind the camera
// offset as seen from `pos`?
int32_t cam0_hit_behind(uint32_t cam, int32_t step, const Vec4& pos) {
    if (step != 1) return 0;
    alignas(16) Vec4 d;
    vec3_sub(d, V(OV(0x00173e60)), pos);
    return vec3_dot(d, V(work(cam) + 0x130)) < 0.0f ? 1 : 0;
}

// orig: 0x002e7a30 — walk the probe from the look point along the offset in growing steps
// (count from W+0x210); where a wall is hit, slide around it and shorten the offset (and,
// while the stick turns the camera, pull in by 0x161cb8). Returns 1 when it slid.
int32_t cam0_walk_probe(uint32_t cam) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0, t = w + 0x40;
    int32_t slid = 0;
    alignas(16) Vec4 dir, p, s;
    const Vec4 o = V(off);
    p = V(w + 0x90);
    float travelled = 0.0f;
    vec3_set_length(dir, o, 1.0f);
    float r = lwc1(OV(0x00161cd8));
    for (int32_t i = 0; i < float_to_int(lwc1(col + 0x40) - 1.0f); ++i) {
        if (i > 0 && cam0_in_volume(p) == 0 && sphere_collide_v(p, r, lwu(OV(0x0015ef5c)), lwu(0x001413d0)) != 0 &&
            cam0_hit_behind(cam, i, p) == 0) {
            const Vec4 hit = V(OV(0x00173e60)), from = p;
            if (cam0_slide(cam, p, travelled, r) != 0) {
                slid = 1;
                if (lbu(0x001413f4) != 2 && u2f(0x3d4ccccdu) < abs_f(lwc1(0x0013ca40))) {
                    alignas(16) Vec4 d;
                    vec3_sub(d, hit, from);
                    vec3_set_length(d, d, 1.0f);
                    if (abs_f(vec3_dot(V(OV(0x00166e40)), d)) < 0.75f) {
                        const int32_t f = frame_scale_i(0x884);
                        const float pull = lwc1(col + 0x30) + lwc1(OV(0x00161cb8)) * lwc1(0x0015ed60);
                        const float min = lwc1(OV(0x00161cc8));
                        sh(col + 0x34, static_cast<uint32_t>(f));
                        swc1(col + 0x30, pull);
                        const float len = lwc1(off + 0x2c);
                        if (len - pull < min) swc1(col + 0x30, len - min);
                    }
                }
                alignas(16) Vec4 d;
                vec3_sub(d, p, V(t + 0x50));
                vec3_set_length(V(off), d, lwc1(off + 0x2c) - lwc1(col + 0x30));
                vec3_set_length(dir, V(off), 1.0f);
            }
        }
        const float len = lwc1(off + 0x2c);
        float k = len - lwc1(col + 0x30);
        const float g = lwc1(col + 0x44) * lwc1(col + 0x3c);
        k = fpu_div(k, len);
        r = r + g;
        travelled = travelled + r;
        vec3_scale(s, dir, r * k);
        vec3_add(p, p, s);
    }
    return slid;
}

// orig: 0x002e7d20 — keep the camera clear: mobys between the look point and the camera
// lose their collision for this frame (the "push" classes turn the offset aside instead),
// the probes run, the pull-in W+0x200 eases back over 0x7d0 frames when free, and a moby
// standing between the camera and the hero is pushed away (30 such frames reset the camera).
void cam0_collide(uint32_t cam) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0;
    int32_t blocked = 0, count = 0, coll_too = 0;
    const float reach = (lwc1(off + 0x2c) - lwc1(col + 0x30)) + 1.5f;
    if (lw(0x0015ed84) == 0xf) coll_too = lbu(0x001413f4) == 2 ? 1 : 0;
    const uint32_t look = w + 0x90;
    alignas(16) Vec4 mid;
    vec3_lerp(mid, V(look), V(OV(0x00166dc0)), 0.5f);
    const int32_t n = mobys_in_sphere_v(mid, reach * 0.5f, 1, lwu(0x001413d0), nullptr);
    uint32_t outp = OV(0x001e75c0);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t m = lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4);
        if (lwu(m + 0x94) == 0) continue;
        if (moby_coll_flag8(m) != 0 && coll_too == 0) continue;
        const int32_t cls = lh(m + 0xa6);
        if (cls != 0x72 && cls != 0xb && cls != 0x2c && cls != 0x9a && cls != 0xff && cls != 0x392 && cls != 0x452 && cls != 0x353) {
            sw(outp, m);
            outp += 4;
            ++count;
            sw(m + 0x94, 0);
            continue;
        }
        alignas(16) Vec4 d, a, side, x;
        vec3_sub(d, V(m + 0x10), V(look));
        vec3_scale(a, V(OV(0x00166e40)), vec3_dot(V(OV(0x00166e40)), d));
        vec3_sub(side, d, a);
        vec3_set_length(side, side, 1.0f);
        const float c = vec3_dot(side, V(off));
        const float ang = abs_f(u2f(0x3fc90fdbu) - asin_f(fpu_div(c, lwc1(off + 0x2c) - lwc1(col + 0x30))));
        if (!(ang < u2f(0x3edf66f3u))) continue;
        vec3_cross_ba(x, V(off), V(OV(0x00166e40)));
        vec3_set_length(x, x, 1.0f);
        vec_rotate_axis(V(off), V(off), V(OV(0x00166e30)), 0.0f < vec3_dot(x, d) ? u2f(0xbc8efa35u) : u2f(0x3c8efa35u));
    }
    const int32_t hs = lw(0x001413d4);
    if (hs != 0x7f && !(lw(0x0015ed84) == 0xd && hs == 0x7b)) blocked = cam0_walk_probe(cam) != 0 ? 1 : 0;
    if (cam0_probe_none(cam) != 0) blocked = 1;
    if (cam0_probe(cam) != 0) blocked = 1;
    const float len = lwc1(off + 0x2c);
    if (len - lwc1(col + 0x30) < lwc1(OV(0x00161cd0))) {
        swc1(col + 0x30, len - lwc1(OV(0x00161cd0)));
        sh(col + 0x34, static_cast<uint32_t>(frame_scale_i(0x7d0)));
    }
    int16_t& timer = ee_ref<int16_t>(col + 0x34);
    bool ease = false;
    if (lh(col + 0x3a) != 0) {
        ease = true;
    } else if (blocked == 0) {
        if (lh(col + 0x34) < frame_scale_i(0x7d0) && lh(col + 0x34) > 0) ease = true;
        else if (lwc1(0x0013ca40) < 0.3f && lwc1(0x0013ca44) < 0.3f) ease = true;
    }
    if (ease) {
        if (timer_tick_i16(timer) != 0) sh(col + 0x3a, 0);
        if (lh(col + 0x3a) != 0 && timer_tick_i16(timer) != 0) sh(col + 0x3a, 0);
        const float f = fpu_div(int_to_float(lh(col + 0x34)), int_to_float(frame_scale_i(0x7d0)));
        swc1(col + 0x30, cos_ease(0.0f, lwc1(col + 0x30), f));
    } else if (frame_scale_i(0x7d0) < lh(col + 0x34)) {
        timer_tick_i16(timer);
    } else {
        sh(col + 0x34, static_cast<uint32_t>(frame_scale_i(0x7d0)));
    }
    vec3_set_length(V(off), V(off), lwc1(off + 0x2c) - lwc1(col + 0x30));
    alignas(16) Vec4 campos;
    vec3_add(campos, V(off), V(look));
    bool clear = true;
    if (hs != 0x7f && !(lw(0x0015ed84) == 0xd && hs == 0x7b) &&
        segment_collide_v(V(look), campos, lwu(OV(0x0015ef58)), lwu(0x001413d0), nullptr) != 0 && cam0_in_volume(V(OV(0x00173e60))) == 0) {
        clear = false;
        const uint32_t m = lwu(OV(0x00173e58));
        if (m != 0 && moby_in_range_class_500(m) != 0) {
            alignas(16) Vec4 d;
            vec3_sub(d, V(m + 0x10), V(OV(0x00166dc0)));
            d.z = 0.0f;
            vec3_set_length(d, d, 1.0f);
            d.w = u2f(0x45afdf66u);
            alignas(16) uint8_t rec[0x30] = {};
            std::memcpy(rec, &d, 16);
            const uint32_t a = lwu(0x001413d0), b = 0x800000;
            const float strength = 20.0f;
            const uint32_t one = 1;
            std::memcpy(rec + 0x10, &a, 4);
            std::memcpy(rec + 0x14, &b, 4);
            std::memcpy(rec + 0x1c, &strength, 4);
            std::memcpy(rec + 0x20, &one, 4);
            rec[0x19] = 3;
            rec[0x18] = 3;
            const uint16_t cls = lhu(lwu(0x001413d0) + 0xa6);
            std::memcpy(rec + 0x1a, &cls, 2);
            moby_hit_record_v(lwu(OV(0x00173e58)), rec);
        }
        sw(col + 0x4c, lwu(col + 0x4c) + 1);
        if (!(lw(col + 0x4c) < frame_scale_i(0x1e))) camera_select_first();
    }
    if (clear) sw(col + 0x4c, 0);
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t m = lwu(OV(0x001e75c0) + static_cast<uint32_t>(i) * 4);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    }
}

// orig: 0x002e8f40 — while the mode flag W+0x10 is set, ease the offset toward the camera's
// last position by 0x15ef40; its length is capped at +0x15c and kept at least the pulled-in one.
void cam0_ease_offset(uint32_t cam) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0;
    if (lh(w + 0x10) <= 0) return;
    alignas(16) Vec4 d;
    vec3_sub(d, V(w), V(w + 0x90));
    vec3_lerp(V(off), V(off), d, lwc1(OV(0x0015ef40)));
    float len = vec3_length(V(off));
    const float max = lwc1(off + 0x2c);
    if (max < len) {
        len = max;
        vec3_set_length(V(off), V(off), len);
        swc1(col + 0x30, lwc1(off + 0x2c) - len);
        return;
    }
    const float m2 = max - lwc1(col + 0x30);
    if (len < m2) {
        len = m2;
        vec3_set_length(V(off), V(off), len);
    }
    swc1(col + 0x30, lwc1(off + 0x2c) - len);
}

// orig: 0x002e9aa0 — walk the 64 x 64 camera volume grid and hand every sphere volume to
// 0x1f6c30 (empty in this build).
void cam0_volume_pass() {
    for (int32_t y = 0; y < 0x40; ++y) {
        for (int32_t x = 0; x < 0x40; ++x) {
            const uint32_t tab = lwu(OV(0x0015ef80));
            const uint32_t off = lwu(tab + static_cast<uint32_t>(y * 64 + x) * 4);
            if (off == 0) continue;
            const uint32_t list = off + tab;
            for (int32_t i = 0; i < lw(list); ++i) {
                const uint32_t e = list + 0x10 + static_cast<uint32_t>(i) * 0x30;
                if (lw(e + 0x10) != 5) continue;
                const uint32_t s = lwu(OV(0x001600f4)) + lwu(e + 0x14) * 0x80;
                sub_001f6c30(s + 0x30, vec3_length(V(s)));
            }
        }
    }
}

// orig: 0x001eb328 — camera_spring for an angle: the shortest way round, result wrapped.
float camera_angle_spring(float cur, float target, float k, float d, float max, uint32_t vel) {
    const float diff = angle_sub(target, cur);
    float v = lwc1(vel);
    v = v + (k * diff - d * v);
    swc1(vel, v);
    if (max != 0.0f) {
        if (max < v) swc1(vel, max);
        else if (v < -max) swc1(vel, -max);
    }
    if (abs_f(diff) < lwc1(vel)) {
        swc1(vel, abs_f(diff));
    } else {
        const float n = -abs_f(diff);
        if (lwc1(vel) < n) swc1(vel, -abs_f(diff));
    }
    return angle_add(cur, lwc1(vel));
}

// orig: 0x002e9518 — when the hero runs toward the camera (or sideways, 0x166eb4), pull the
// camera back (distance +0x228 up to 4 / +0x22c) and give the distance spring its rates.
void cam0_backoff(uint32_t cam, float& k, float& d) {
    const uint32_t w = work(cam), p = w + 0x220, off = w + 0x130;
    bool start = false;
    if (lh(p + 6) == 0) {
        if (lh(p + 4) != 0 && lwc1(OV(0x00166eb4)) == 0.0f && vec3_dot(V(lwu(0x001413d0) + 0xc0), V(cam)) <= u2f(0xbe99999au)) start = true;
        else if (u2f(0x38d1b717u) < abs_f(lwc1(OV(0x00166eb4))) && vec3_dot(V(OV(0x00166e90)), V(cam)) <= u2f(0xbe99999au)) start = true;
    }
    if (start) {
        sh(p + 4, static_cast<uint32_t>(frame_scale_i(0x78)));
        float f = lwc1(OV(0x00166eb4)) * u2f(0x4164923au);
        if (1.0f < f) f = 1.0f;
        float far = 4.0f;
        if (lbu(0x001413f4) != 1) far = lwc1(p + 0xc);
        const float base = lwc1(off + 0x58);
        const float v = base + (far - base) * f;
        if (lwc1(p + 8) < v) swc1(p + 8, v);
        cam0_set_distance(0, lwc1(p + 8), u2f(0x3b449ba6u));
        k = lwc1(OV(0x00161cec)) + (lwc1(OV(0x00161d14)) - lwc1(OV(0x00161cec))) * f;
        d = lwc1(OV(0x00161cf0)) + (lwc1(OV(0x00161d18)) - lwc1(OV(0x00161cf0))) * f;
        return;
    }
    if (lh(p + 4) != 0) {
        sh(p + 4, 0);
        swc1(p + 8, lwc1(off + 0x58));
    }
    k = lwc1(OV(0x00161cec));
    d = lwc1(OV(0x00161cf0));
}

// orig: 0x002e9720 — the camera position: the offset W+0x140 springs toward W+0x130 in
// length (W+0x158), pitch (W+0x154) and yaw (W+0x150), keeps 15 degrees off the facing
// axis, and the camera sits at the look point + height + offset.
void cam0_position(uint32_t cam) {
    const uint32_t w = work(cam), off = w + 0x130, col = w + 0x1d0, cur = w + 0x140;
    const Vec4& up = V(OV(0x00166e30));
    vec3_add(V(w), V(w + 0x90), V(off));
    float target = lwc1(off + 0x2c) - lwc1(col + 0x30);
    float len = vec3_length(V(cur));
    float k, d;
    cam0_backoff(cam, k, d);
    len = camera_spring(len, target, k, d, 0.0f, w + 0x158);
    const float h_t = vec3_dot(V(off), up);
    const float pitch_t = asin_f(fpu_div(h_t, target));
    const float h_c = vec3_dot(V(cur), up);
    const float pitch = camera_angle_spring(asin_f(fpu_div(h_c, len)), pitch_t, lwc1(OV(0x00161cfc)), lwc1(OV(0x00161d00)), 0.0f, w + 0x154);
    alignas(16) Vec4 a, ft, fc, x, r, ax;
    vec3_scale(a, up, h_t);
    vec3_sub(ft, V(off), a);
    vec3_scale(a, up, h_c);
    vec3_sub(fc, V(cur), a);
    const float dt = vec3_dot(ft, fc);
    const float l = vec3_length(ft) * vec3_length(fc);
    const float yaw = camera_angle_spring(0.0f, u2f(0x3fc90fdbu) - asin_f(fpu_div(dt, l)), lwc1(OV(0x00161cf4)), lwc1(OV(0x00161cf8)), 0.0f, w + 0x150);
    vec3_cross_ba(x, fc, up);
    const float turn = (0.0f <= vec3_dot(x, ft)) ? yaw * 1.0f : yaw * -1.0f;
    vec_rotate_axis(r, fc, up, turn);
    vec3_cross_ba(ax, up, r);
    vec_rotate_axis(r, r, ax, pitch);
    vec3_set_length(V(cur), r, len);
    alignas(16) Vec4 hgt, base;
    vec3_scale(hgt, up, lwc1(off + 0x30));
    vec3_add(base, hgt, V(w + 0xa0));
    const float df = vec3_dot(V(cur), V(OV(0x00166e40)));
    const float lc = vec3_length(V(cur));
    if (lc != 0.0f) {
        const float ang = u2f(0x3fc90fdbu) - asin_f(fpu_div(df, lc));
        const float lim = u2f(0x3e860a92u);
        if (abs_f(ang) < lim) {
            alignas(16) Vec4 s;
            vec3_cross_ba(s, V(cur), V(OV(0x00166e40)));
            vec_rotate_axis(V(cur), V(cur), s, angle_sub(lim, abs_f(ang)));
            swc1(off + 0x24, 0.0f);
            swc1(off + 0x28, 0.0f);
            swc1(off + 0x20, 0.0f);
        }
    }
    vec3_add(V(cam + 0x30), base, V(cur));
}

// orig: 0x002e89b0 — the player's camera stick input (0x13ca40 / 0x13ca44, inverted by the
// options 0x15ede0 / 0x15eddc): yaw `a` (or the scripted +0x1c4) and pitch `b` (or +0x1cc).
// Turning the camera by hand for 60 frames also counts toward the per-level statistic at
// 0x141888 (uses / time) and its level bit 0x14188c.
void cam0_stick(uint32_t cam, float& a, float& b) {
    const uint32_t s = work(cam) + 0x1a8;
    if ((lw(s + 0x10) & 1) != 0) {
        a = 0.0f;
    } else {
        a = lwc1(0x0013ca40);
        if (lw(0x0015ede0) == 0) a = -a;
    }
    if (a == 0.0f) {
        sw(OV(0x00161d8c), 0);
        sw(OV(0x00161d90), 0);
        if (lwc1(s + 0x1c) != 0.0f) a = lwc1(s + 0x1c);
    } else {
        swc1(s + 0x14, lwc1(OV(0x0015ef44)));
        constexpr uint32_t ST = 0x00141848;
        if (lhu(ST + 0x40) < 3) {
            sw(OV(0x00161d8c), lwu(OV(0x00161d8c)) + 1);
            if (lw(OV(0x00161d90)) == 0 && frame_scale_i(0x3c) < lw(OV(0x00161d8c))) {
                if (!(0xfffeu < lhu(ST + 0x40))) sh(ST + 0x40, lhu(ST + 0x40) + 1u);
                const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
                if (lhu(ST + 0x42) < q) sh(ST + 0x42, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
                sw(ST + 0x44, lwu(ST + 0x44) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
                sw(OV(0x00161d90), 1);
            }
        }
    }
    if ((lw(s + 0x10) & 2) != 0) {
        b = fpu_div(lwc1(s + 0x24), u2f(0x3f32b8c2u));
        return;
    }
    const float y = lwc1(0x0013ca44);
    b = lw(0x0015eddc) == 0 ? y : -y;
    if (b == 0.0f) b = fpu_div(lwc1(s + 0x24), u2f(0x3f32b8c2u));
}

// orig: 0x002e8bb8 — turn the camera offset by the stick: yaw spring S+0 (rates 0x161cac /
// 0x161cb0) times S+0x14, pitch spring S+8 outside a 0.3 dead zone (limit 0x161cc0 / 0x161cbc),
// the pitch limited to +-0x161cc4 around the current elevation.
void cam0_turn(uint32_t cam, float a, float b) {
    const uint32_t w = work(cam), s = w + 0x1a8, off = w + 0x130;
    const Vec4& up = V(OV(0x00166e30));
    const float yaw = camera_spring(lwc1(s), a, lwc1(OV(0x00161cac)), lwc1(OV(0x00161cb0)), 0.0f, s + 4);
    swc1(s, yaw);
    swc1(off + 0x34, lwc1(s + 0x14) * yaw);
    float p;
    if (b < u2f(0xbe99999au)) p = (b + 0.3f) * u2f(0x3fb6db6eu);
    else if (0.3f < b) p = (b - 0.3f) * u2f(0x3fb6db6eu);
    else p = 0.0f;
    float lim = lwc1(OV(0x00161cc0));
    if (p != 0.0f) {
        lim = lwc1(OV(0x00161cbc));
        if (abs_f(p) < abs_f(lwc1(s + 8))) {
            if ((0.0f < p && 0.0f < lwc1(s + 8)) || (p < 0.0f && lwc1(s + 8) < 0.0f)) lim = lwc1(OV(0x00161cc0));
        }
    }
    const float ps = camera_spring(lwc1(s + 8), p, lwc1(OV(0x00161cac)), lwc1(OV(0x00161cb0)), lim, s + 0xc);
    swc1(s + 8, ps);
    const float want = ps * u2f(0x3f32b8c2u);
    vec_rotate_axis(V(off), V(off), up, a == 0.0f ? lwc1(s + 0x20) : lwc1(off + 0x34));
    const float h = vec3_dot(V(off), up);
    const float l = vec3_length(V(off));
    alignas(16) Vec4 side;
    vec3_cross_ba(side, up, V(off));
    vec3_set_length(side, side, 1.0f);
    const float el = atan2_f(l, h);
    if (abs_f(el) < abs_f(want) && lim == lwc1(OV(0x00161cc0))) swc1(s + 8, fpu_div(el, u2f(0x3f32b8c2u)));
    const float c = lwc1(OV(0x00161cc4));
    if (el < want) {
        if (want < angle_add(el, c)) swc1(off + 0x38, angle_sub(want, el));
        else swc1(off + 0x38, c);
    } else if (want < el) {
        if (lh(work(cam) + 0x208) < 0) sw(off + 0x38, 0);
        else if (angle_add(el, -c) < want) swc1(off + 0x38, angle_sub(want, el));
        else swc1(off + 0x38, -lwc1(OV(0x00161cc4)));
    } else {
        sw(off + 0x38, 0);
    }
    if (want < 0.0f) sh(work(cam) + 0x22, 1);
    vec_rotate_axis(V(off), V(off), side, lwc1(off + 0x38));
}

// orig: 0x002e9010 — aim the camera: x row toward the look point (flat), pitched toward the
// height points by a spring (W+0x34) plus a tilt from the offset's elevation, limited to
// +-70 degrees; grinding (kind 0xb) looks straight at the hero. A hand-over blend (W+0x20)
// eases from the old rows (cam+0x40).
void cam0_aim(uint32_t cam) {
    const uint32_t w = work(cam), t = w + 0x40, tm = w + 0x20, off = w + 0x130;
    const Vec4& up = V(OV(0x00166e30));
    if (lbu(t + 0xc4) == 0xb) {
        vec3_sub(V(cam), V(HERO_POS), V(cam + 0x30));
        vec3_set_length(V(cam), V(cam), 1.0f);
        timer_tick_i16(ee_ref<int16_t>(tm));
    } else {
        alignas(16) Vec4 a, look, here, d, b, c;
        vec3_scale(a, up, vec3_dot(V(w + 0x80), up));
        vec3_sub(look, V(w + 0x80), a);
        vec3_scale(a, up, vec3_dot(V(cam + 0x30), up));
        vec3_sub(here, V(cam + 0x30), a);
        vec3_sub(d, look, here);
        const float dl = vec3_length(d);
        if (0.05f <= dl) vec3_scale(V(cam), d, fpu_div(1.0f, dl));
        vec3_cross_ba(V(cam + 0x10), V(cam), up);
        vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
        swc1(tm + 8, camera_spring(lwc1(tm + 8), lwc1(off + 0x30), u2f(0x3b83126fu), 0.2f, 0.0f, w + 0x30));
        const float h4 = camera_spring(lwc1(tm + 4), lwc1(t + 0xb0), u2f(0x3b83126fu), 0.2f, 0.0f, w + 0x2c);
        swc1(tm + 4, h4);
        const float dh = lwc1(tm + 8) - h4;
        vec3_scale(b, up, -vec3_dot(V(w + 0x140), up));
        vec3_sub(b, b, d);
        vec3_sub(c, V(cam + 0x30), b);
        vec3_scale(b, up, -dh);
        vec3_sub(c, c, b);
        vec3_sub(d, c, V(cam + 0x30));
        const float l = vec3_length(d);
        if (l != 0.0f) {
            float pitch = u2f(0x3fc90fdbu) - asin_f(fpu_div(vec3_dot(V(cam), d), l));
            vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
            if (vec3_dot(V(cam + 0x20), d) < 0.0f) pitch = -pitch;
            const float el = atan2_f(vec3_length(V(off)), vec3_dot(V(off), up));
            float f = fpu_div(el, u2f(0x3f32b8c2u));
            float tilt = 0.0f;
            if (f < u2f(0xbdcccccdu)) {
                f = -f;
                if (0.5f < f) f = 1.0f - f;
                f = f + f;
                tilt = f * u2f(0x3e860a92u) + 0.0f;
            }
            const float tl = camera_angle_spring(lwc1(tm + 0x14), tilt, u2f(0x3ba3d70au), 0.2f, 0.0f, tm + 0x18);
            swc1(tm + 0x14, tl);
            float r = angle_sub(pitch, tl);
            if (u2f(0x3f9c61aau) < r) r = u2f(0x3f9c61aau);
            else if (r < u2f(0xbf9c61aau)) r = u2f(0xbf9c61aau);
            vec_rotate_axis(V(cam), V(cam), V(cam + 0x10), r);
            vec3_set_length(V(cam), V(cam), 1.0f);
        }
    }
    if (lh(tm) != 0) {
        timer_tick_i16(ee_ref<int16_t>(tm));
        const float f = fpu_div(int_to_float(lh(tm)), int_to_float(frame_scale_i(lw(OV(0x00161d24)))));
        vec3_lerp(V(cam), V(cam), V(cam + 0x40), f);
        vec3_set_length(V(cam), V(cam), 1.0f);
    }
    vec3_cross_ba(V(cam + 0x10), V(cam), up);
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
}

// orig: 0x002e9bb0 — the follow camera's position and aim for this frame.
void cam0_place_frame(uint32_t cam) {
    cam0_volume_pass();
    cam0_ease_offset(cam);
    float a, b;
    cam0_stick(cam, a, b);
    cam0_turn(cam, a, b);
    cam0_collide(cam);
    cam0_position(cam);
    cam0_nop_d(cam);
    cam0_aim(cam);
}

// orig: 0x002e5e38 — end of frame: the distance / height / look-height springs toward the
// targets set this frame (or the last frame's values), then the per-frame targets reset.
void cam0_frame_end(uint32_t cam) {
    const uint32_t w = work(cam), s = w + 0x1a8, off = w + 0x130, t = w + 0x40;
    swc1(s + 0x14, lwc1(OV(0x0015ef44)));
    sw(s + 0x1c, 0);
    sw(s + 0x20, 0);
    sw(s + 0x10, 0);
    sw(s + 0x24, 0);
    const uint32_t pw = lwu(OV(0x00168fc0)), pt = pw + 0x40, poff = pw + 0x130;
    swc1(off + 0x2c, camera_spring(lwc1(off + 0x2c), lh(off + 0x3c) != 0 ? lwc1(off + 0x40) : lwc1(poff + 0x2c), lwc1(off + 0x48),
                                   lwc1(OV(0x00161d04)), lwc1(s + 0x1c), w + 0x174));
    swc1(off + 0x30, camera_spring(lwc1(off + 0x30), lh(off + 0x3e) != 0 ? lwc1(off + 0x4c) : lwc1(poff + 0x30), lwc1(off + 0x54),
                                   lwc1(OV(0x00161d08)), 0.0f, off + 0x50));
    swc1(t + 0xb0, camera_spring(lwc1(t + 0xb0), lh(t + 0xda) != 0 ? lwc1(t + 0xb4) : lwc1(pt + 0xb0), lwc1(t + 0xb8), 0.2f, 0.0f, t + 0xbc));
    sh(off + 0x3c, 0);
    sh(off + 0x3e, 0);
    sw(off + 0x40, 0);
    sw(off + 0x4c, 0);
    const float g = lwc1(OV(0x00161ca0));
    swc1(off + 0x58, lwc1(poff + 0x2c));
    sh(t + 0xda, 0);
    sw(t + 0xb4, 0);
    sh(work(cam) + 0x10, 1);
    sh(work(cam) + 0x22, 0);
    sb(t + 0xd6, 0);
    swc1(t + 0xe4, g);
    swc1(t + 0xe8, 0.2f);
    swc1(t + 0xdc, g);
    swc1(t + 0xe0, 0.2f);
    const uint32_t col = work(cam) + 0x1d0;
    swc1(col + 0x44, lwc1(OV(0x00161cdc)));
    sw(col + 0x48, lwu(OV(0x0015ef60)));
    swc1(col + 0x3c, lwc1(OV(0x00161ce4)));
    swc1(col + 0x40, lwc1(OV(0x00161ce0)));
    const uint32_t p = work(cam) + 0x220;
    swc1(p + 0xc, lwc1(OV(0x00161d1c)));
    sh(p + 6, 0);
}

// orig: 0x002e9c28 — the follow camera's update.
void cam0_update(uint32_t cam) {
    const uint32_t look = work(cam) + 0x1d0;
    swc1(OV(0x0015ef44), lwc1(OV(0x00161db0) + lwu(0x0015ede4) * 4));
    cam0_follow_motion(cam);
    cam0_modes(cam);
    cam0_nop_a(cam);
    cam0_target_update(cam);
    cam0_nop_b(cam, look);
    cam0_place_frame(cam);
    cam0_nop_c(cam);
    cam0_frame_end(cam);
}

}  // namespace rac1
