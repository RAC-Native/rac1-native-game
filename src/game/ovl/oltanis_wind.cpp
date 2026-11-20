// Oltanis (level 14), class 610: a gust of wind along the moby's heading. Inside its outer box (pvar +0x500) it sets the
// level's wind vector (0x162230 .. 0x162240) and blows its 48 motes (+0x20: a string of points, +0x320 / +0x380 their
// life left / life, +0x3e0 their alpha, +0x440 their speeds) through its box (+0x504), and while the hero is in the air
// (or swinging) inside the blowing box (+0x00) it pushes him (his velocity +0xf0) with a force that fades over +0x508
// frames (+0x50a / +0x50c); once he leaves it the push dies away over 0x161c04 frames (+0xc / +0x10; +0xe = pushing).
// Pvars: +0x08 the force, +0x14 / +0x18 / +0x1c the box's half sizes (the first doubled).
#include <game/level/level_data.h>
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

// orig: L14:0x002eb388 -- mote k back at the box's upwind end, at a random place across it, with a new speed and life
void class610_mote_respawn(uint32_t m, int32_t k, const Vec4& dir) {
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 up{0.0f, 0.0f, 1.0f, 0.0f};
    Vec4 across{};
    vec3_cross_ba(across, dir, up);
    const uint32_t p = pv + 0x20u + (static_cast<uint32_t>(k) << 4);
    V(p) = V(lvd::box(lvd::BOXES, lw(pv + 0x504)) + 0x30);
    const float back = -lwc1(pv + 0x14);
    const float side = rand_range_signed_f(0.0f, lwc1(pv + 0x18));
    const float high = rand_range_signed_f(0.0f, lwc1(pv + 0x1c));
    Vec4 off{};
    vec3_scale(off, dir, back);
    vec3_add(V(p), V(p), off);
    vec3_scale(off, across, side);
    vec3_add(V(p), V(p), off);
    vec3_scale(off, up, high);
    vec3_add(V(p), V(p), off);
    swc1(p + 0xc, rand_range_f(lwc1(L14_ADDR(0x00161c38)), lwc1(L14_ADDR(0x00161c3c))));
    const float speed = fpu_mul(rand_range_f(static_cast<float>(lw(L14_ADDR(0x00161c30))), static_cast<float>(lw(L14_ADDR(0x00161c34)))),
                                lwc1(0x0015ed6cu));
    swc1(pv + 0x440u + (static_cast<uint32_t>(k) << 2), speed);
    const int32_t life = frame_scale_i(float_to_int(fpu_div(fpu_add(lwc1(pv + 0x14), lwc1(pv + 0x14)), speed)));
    const uint32_t h = pv + (static_cast<uint32_t>(k) << 1);
    sh(h + 0x320, static_cast<uint16_t>(life));
    sh(h + 0x380, static_cast<uint16_t>(life));
    sh(h + 0x3e0, 0);
}

// orig: L14:0x002eb568 -- the motes moved and faded in / out; spent ones respawned (only the first 24 while the hero is in
// the box); one in the box near the camera and facing it is cut short; the draw callback queued (L14:0x2eb810, generated)
void class610_motes_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 dir{cos_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48)), 0.0f, 0.0f};
    const uint32_t at = ee_stack_alloc(0x10);  // (point_in_box takes the hero's place, a unit higher, by address)
    V(at) = V(HERO + 0x80);
    swc1(at + 0x8, fpu_add(lwc1(at + 0x8), 1.0f));
    const int32_t inside = point_in_box(at, lw(pv + 0x504));
    ee_stack_free(0x10);
    for (int32_t k = 0; k < 48; ++k) {
        const uint32_t h = pv + (static_cast<uint32_t>(k) << 1);
        const uint32_t p = pv + 0x20u + (static_cast<uint32_t>(k) << 4);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(h + 0x320))) != 0) {
            if (inside == 0 || k < 24) class610_mote_respawn(m, k, dir);
            continue;
        }
        const int32_t fade = lw(L14_ADDR(0x00161c2c));
        const int32_t life = lh(h + 0x380), left = lh(h + 0x320);
        const int32_t peak = lw(L14_ADDR(0x00161c10));
        float t;
        if (life - fade < left) t = fpu_div(int_to_float(life - left), int_to_float(fade));          // fading in
        else t = fpu_div(int_to_float(left), int_to_float(lh(h + 0x380) - lw(L14_ADDR(0x00161c2c))));  // fading out
        sh(h + 0x3e0, static_cast<uint16_t>(float_to_int(fpu_add(fpu_mul(fpu_sub(static_cast<float>(peak), 0.0f), t), 0.0f))));
        Vec4 step{};
        vec3_scale(step, dir, lwc1(pv + 0x440u + (static_cast<uint32_t>(k) << 2)));
        vec3_add(V(p), V(p), step);
        if (inside == 0) continue;
        if (!(abs_f(fpu_sub(lwc1(p + 0x0), lwc1(L14_ADDR(0x001674c0)))) < 8.0f)) continue;
        if (!(abs_f(fpu_sub(lwc1(p + 0x4), lwc1(L14_ADDR(0x001674c4)))) < 8.0f)) continue;
        if (!(u2f(0x4016cbe4u) < angle_dist(lwc1(m + 0x48), lwc1(L14_ADDR(0x001674d8))))) continue;
        if (frame_scale_i(20) < lh(h + 0x320)) sh(h + 0x320, static_cast<uint16_t>(frame_scale_i(20)));
    }
    draw_callback_add(L14_ADDR(0x002eb810), m);
}

// the push on the hero's velocity along the heading
void push_hero(uint32_t m, float force, float z) {
    Vec4 push{fpu_mul(cos_vu(lwc1(m + 0x48)), force), fpu_mul(sin_vu(lwc1(m + 0x48)), force), z, 0.0f};
    vec3_add(V(HERO + 0xf0), V(HERO + 0xf0), push);
}

}  // namespace

// orig: L14:0x002eaf88
void olt_class610_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        if (lw(pv + 0x0) < 0 || lw(pv + 0x504) < 0 || lw(pv + 0x500) < 0) {
            debug_printf_stub();  // (the original prints the uid and which of the three boxes is missing)
            moby_retire(m);
            return;
        }
        sh(pv + 0xe, 0);
        const float length = vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 0x504)) + 0x0));
        swc1(pv + 0x14, fpu_add(length, length));
        swc1(pv + 0x18, vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 0x504)) + 0x10)));
        swc1(pv + 0x1c, vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 0x504)) + 0x20)));
        const int32_t fade = frame_scale_i(lh(pv + 0x508));
        sh(pv + 0x50a, static_cast<uint16_t>(fade));
        swc1(pv + 0x50c, fpu_div(1.0f, int_to_float(static_cast<int16_t>(fade))));
        const Vec4 dir{cos_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48)), 0.0f, 0.0f};
        for (int32_t k = 0; k < 48; ++k) class610_mote_respawn(m, k, dir);
    }
    if (point_in_box(HERO + 0x80, lw(pv + 0x500)) != 0) {
        {
            const float c = fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(lwc1(L14_ADDR(0x00161c08)), lwc1(0x0015ed6cu)));
            swc1(L14_ADDR(0x00162230), c);
            sw(L14_ADDR(0x00162238), 0);
            swc1(L14_ADDR(0x00162240), 1.0f);
            swc1(L14_ADDR(0x00162234), fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(lwc1(L14_ADDR(0x00161c08)), lwc1(0x0015ed6cu))));
        }
        class610_motes_update(m);
        // the hero in the air (+0x308 == 0, not hero state 0xd at +0x2284; swinging (+0x12e2) or off the ground (+0x30c))
        // inside the blowing box
        bool blown = lh(HERO + 0x308) == 0 && lw(HERO + 0x2284) != 0xd;
        if (blown && lbu(HERO + 0x12e2) == 0) blown = lh(HERO + 0x30c) != 0;
        if (blown) blown = point_in_box(HERO + 0x80, lw(pv + 0x0)) != 0;
        if (blown) {
            timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x50a)));
            const float faded = fpu_mul(int_to_float(lh(pv + 0x50a)), lwc1(pv + 0x50c));
            sh(pv + 0xe, 1);
            const float force = fpu_mul(fpu_sub(1.0f, faded), fpu_mul(lwc1(pv + 0x8), lwc1(0x0015ed6cu)));
            push_hero(m, force, 0.0f);
            const int32_t calm = frame_scale_i(lw(L14_ADDR(0x00161c04)));
            sh(pv + 0xc, static_cast<uint16_t>(calm));
            swc1(pv + 0x10, fpu_div(1.0f, int_to_float(static_cast<int16_t>(calm))));
            return;
        }
    }
    // out of it: the last push dies away
    if (lh(pv + 0xe) == 0 || lh(pv + 0xc) == 0) return;
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xc))) != 0) {
        sh(pv + 0xe, 0);
        const int32_t fade = frame_scale_i(lh(pv + 0x508));
        sh(pv + 0x50a, static_cast<uint16_t>(fade));
        swc1(pv + 0x50c, fpu_div(1.0f, int_to_float(static_cast<int16_t>(fade))));
    }
    if (lh(HERO + 0x308) != 0 || lbu(HERO + 0x12e2) == 0) {  // (on the ground, or not swinging: at most 15 frames left)
        if (frame_scale_i(15) < lh(pv + 0xc)) {
            const int32_t calm = frame_scale_i(15);
            sh(pv + 0xc, static_cast<uint16_t>(calm));
            swc1(pv + 0x10, fpu_div(1.0f, static_cast<float>(static_cast<int16_t>(calm))));
        }
    }
    const float t = fpu_mul(int_to_float(lh(pv + 0xc)), lwc1(pv + 0x10));
    const float force = fpu_add(fpu_mul(fpu_sub(fpu_mul(lwc1(pv + 0x8), lwc1(0x0015ed6cu)), 0.0f), t), 0.0f);
    push_hero(m, force, 0.0f);
}

}  // namespace rac1
