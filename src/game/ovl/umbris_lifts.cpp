// Umbris's own moby classes: the platforms that carry the hero along a path and fade out at its end to start over (class 38)
// or wait at its end while he stands on them (class 1474, shown once its flag byte of the level is set) — hand-ported from
// Umbris's own overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`);
// Umbris-only data is L7_ADDR, the base ELF's are bare (the hero 0x13f3d0, the frame time 0x15ed6c, the sound slots 0x13e550).
//
// The pvars (as Gaspar's lift 1150's): its path +0xb4 (-1: it does nothing), its first end +0xa0 (0 the path's start), the
// attach frame +0x60, where it is along the path +0xa4 (0 .. 1) at the speed +0xa8 toward +0xac, the ride's time +0xb0
// (seconds), its rise this frame +0xbc, the hero gone off it +0xb8, a pause +0xba, its sound +0xc4, whether it carries the
// hero alone (+0xc8 0: the hero is told to hold still while it rides).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;

// the sound in the moby's slot +0xc4 stopped when it is still the moby's and playing; the slot -1
void sound_off(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0xc4);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0xc4, 0xffffffffu);
}

// the set-up at its first end (the start, or the end with +0xa0 set: there it waits, +0xbc its value)
void lift_setup(uint32_t m, uint32_t pv, uint32_t path) {
    const uint32_t first = lbu(pv + 0xa0);
    sb(pv + 0x28, 4);
    sh(pv + 0x3e, 5);
    sw(pv + 0x20, 0);
    sh(pv + 0x24, 0);
    sb(m + 0xbc, first);
    const float at = first != 0 ? 0.0f : 1.0f;
    swc1(pv + 0xa4, at);
    V(m + 0x10) = V(float_to_int(at) == 0 ? path + 0x10 : path + 0x10 + ((lwu(path) - 1) << 4));
    sh(pv + 0xba, 0);
    sw(pv + 0xa8, 0);
    sw(pv + 0xbc, 0);
    sb(m + 0x20, 1);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sw(pv + 0xc4, 0xffffffffu);
}
// The lift's update; `at_end` is the state it takes on reaching the path's end (0 for class 38: it fades and starts over,
// 3 for class 1474: it waits while the hero stands on it). 0: shrinking by its class's size over 60 frames, then back at the
// path's start at full size (1). 1: the hero on it after he stepped off sets it going (held still unless it carries him alone).
// 2: riding (as Gaspar's lift: the hero under it stops it a moment; arrived, the hero let go, its pause). 3: back to 0 once
// the hero is off it. The hero carried by the attach frame; off it by 2 it marks him gone.
void lift_update(uint32_t m, uint32_t pv, uint32_t path, uint32_t at_end) {
    const uint32_t pos = m + 0x10;
    const Vec4 old = V(pos);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xba)));
    if (lbu(m + 0x20) == 0) {
        lift_setup(m, pv, path);
        if (at_end == 3) {  // (class 1474: shown, its class's draw distance)
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
    }
    const uint32_t start = path + 0x10;
    switch (lbu(m + 0xbc)) {
        case 0: {  // fading out at the end, then back at the start
            const uint32_t cls = lwu(m + 0x24);
            const float s = fpu_sub(lwc1(m + 0x2c), fpu_div(lwc1(cls + 0x24), 60.0f));
            swc1(m + 0x2c, s);
            if (s < 0.0f) {
                swc1(m + 0x2c, lwc1(cls + 0x24));
                V(pos) = V(start);
                sb(m + 0xbc, 1);
                swc1(pv + 0xa4, 0.0f);
            }
            break;
        }
        case 1: {  // waiting at the start: the hero back on it sets it going
            if (lh(pv + 0xb8) == 0 || moby_hero_on_l1(m) == 0) break;
            if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) hero_set_state(0x72, 1);
            sb(m + 0xbc, 2);
            const float t = frame_scale_f(fpu_mul(lwc1(pv + 0xb0), 60.0f));
            sh(pv + 0xb8, 0);
            swc1(pv + 0xac, fpu_div(1.0f, t));
            break;
        }
        case 2: {  // riding
            bool stop = lh(pv + 0xba) != 0;
            if (!stop && lwc1(pv + 0xbc) < 0.0f && vec2_distance(V(pos), V(HERO_POS)) < 2.0f &&
                abs_f(fpu_sub(lwc1(HERO_POS + 8), lwc1(m + 0x18))) < 4.0f && 1.0f < fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8))) {
                // the hero under it going down: a pause
                if (lh(pv + 0xba) == 0) sh(pv + 0xba, static_cast<uint32_t>(frame_scale_i(30)));
                stop = true;
            }
            if (stop) {
                sw(pv + 0xa8, 0);
                sound_off(m, pv);
                break;
            }
            if (sub_0028d8c0(m, lw(pv + 0xc4)) == 0) sw(pv + 0xc4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            const float speed = fpu_add(lwc1(pv + 0xa8), fpu_mul(lwc1(pv + 0xac), lwc1(0x0015ed6c)));
            swc1(pv + 0xa8, speed);
            if (abs_f(lwc1(pv + 0xac)) < abs_f(speed)) swc1(pv + 0xa8, lwc1(pv + 0xac));
            const float at = fpu_add(lwc1(pv + 0xa4), lwc1(pv + 0xa8));
            swc1(pv + 0xa4, at);
            if (0.5f < abs_f(fpu_sub(at, 0.5f))) {  // arrived
                if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) hero_enter_idle();
                if (0.0f < lwc1(pv + 0xac)) {
                    sb(m + 0xbc, at_end);
                    swc1(pv + 0xa4, 1.0f);
                } else {
                    sb(m + 0xbc, 1);
                    swc1(pv + 0xa4, 0.0f);
                }
                sw(pv + 0xa8, 0);
                sh(pv + 0xba, static_cast<uint32_t>(frame_scale_i(15)));
                sound_off(m, pv);
            }
            const int32_t k = float_to_int(fpu_mul(int_to_float(lw(path) - 1), lwc1(pv + 0xa4)));
            const float frac = fpu_sub(fpu_mul(int_to_float(lw(path) - 1), lwc1(pv + 0xa4)), int_to_float(k));
            const uint32_t p0 = start + (static_cast<uint32_t>(k) << 4);
            if (k == lw(path) - 1) {
                V(pos) = V(p0);
            } else {
                Vec4 d;
                vec3_sub(d, V(p0 + 0x10), V(p0));
                vec3_scale(d, d, frac);
                vec3_add(V(pos), d, V(p0));
            }
            swc1(pv + 0xbc, fpu_sub(lwc1(m + 0x18), old.z));
            break;
        }
        case 3:  // waiting at the end while the hero stands on it (class 1474)
            if (at_end == 3 && moby_hero_on_l1(m) == 0) sb(m + 0xbc, 0);
            break;
        default:
            break;
    }
    Vec4 delta;
    vec3_sub(delta, V(pos), old);
    {
        // (the attach frame takes the move by address: the original's stack)
        const uint32_t sp = ee_stack_alloc(0x10);
        V(sp) = delta;
        attach_frame_set(pv + 0x60, sp, m + 0x40, m + 0x40);
        ee_stack_free(0x10);
    }
    if (moby_hero_on_l1(m) != 0) return;
    if (!(2.0f < vec2_distance(V(pos), V(HERO_POS)))) return;
    if (lh(pv + 0xba) == 0) sh(pv + 0xb8, 1);
}

}  // namespace

// Class 38, the platform that rides its path, fades out at its end and starts over (see lift_update).
// orig: L7:0x002cd2b0
void umb_class38_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xb4) == -1) return;
    lift_update(m, pv, lvd::path(lw(pv + 0xb4)), 0);
}

// Class 1474, the platform that waits at its path's end while the hero stands on it; hidden (flags 0x41, no draw distance)
// until its flag byte of the level (0x15fc88 + its +0xb0) is set.
// orig: L7:0x002cdb28
void umb_class1474_update_l7(uint32_t m) {
    if (lbu(L7_ADDR(0x0015fc88) + lbu(m + 0xb0)) != 0xff) {
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41);
        return;
    }
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xb4) == -1) return;
    lift_update(m, pv, lvd::path(lw(pv + 0xb4)), 3);
}

}  // namespace rac1
