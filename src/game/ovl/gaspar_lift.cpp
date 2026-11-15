// Gaspar's own moby class 1150, a lift that rides a path between its two ends — hand-ported from Gaspar's own overlay.
// Header gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR,
// the base ELF's are bare (the hero 0x13f3d0, the frame time 0x15ed6c, the level number 0x15ed84, the per-level bit sets
// 0x14c190, the sound slots 0x13e550).
//
// The pvars: its path +0xb4 (-1: it does nothing), the box that switches it on +0xc0 (-1: on), its first end +0xa0 (0 the
// path's start), the attach frame +0x60, where it is along the path +0xa4 (0 .. 1) at the speed +0xa8 toward +0xac, the
// ride's time +0xb0 (seconds), its rise this frame +0xbc, the hero gone off it +0xb8, a pause +0xba, its sound +0xc4,
// whether it carries the hero alone (+0xc8 0: the hero is told to hold still while it rides).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;

// the sound in the moby's slot `slot` stopped when it is still the moby's and playing; the slot -1 (and the speed 0)
void sound_off(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0xc4);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0xc4, 0xffffffffu);
}

// the ride's speed set: one way (`dir` 1 or -1) over the ride's time; it leaves its end
void set_off(uint32_t m, uint32_t pv, float dir) {
    sb(m + 0xbc, 2);
    const float t = frame_scale_f(fpu_mul(lwc1(pv + 0xb0), 60.0f));
    sh(pv + 0xb8, 0);
    swc1(pv + 0xac, fpu_div(dir, t));
}

// the hero on it told to hold still (state 0x72) unless it carries him alone
void hold_hero(uint32_t m, uint32_t pv) {
    if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) hero_set_state(0x72, 1);
}
}  // namespace

// Class 1150. Off until the hero is in its box (+0xc0) or its bit of the level is set (set then, in the level's bit sets).
// Set up once at its first end. At an end (+0xbc 0: the path's start, 1: its end) it waits for the hero to be nearer the
// other end than to its own (or, after the hero stepped off, on it again), then rides: the hero held still, its sound, along
// the path at a speed easing toward the ride's (0.5 past the middle it has arrived: the hero let go, its sound, a short pause).
// Riding, the hero under it (within 2 across and 1 .. 4 below) stops it a moment. The hero it carries moves with it (the
// attach frame); the hero off it by 2 marks it to wait.
// orig: L9:0x003033a0
void gas_class1150_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xb4) == -1) return;
    const uint32_t path = lvd::path(lw(pv + 0xb4));
    bool on = true;
    if (lw(pv + 0xc0) != -1) {
        bool switched = point_in_box(HERO_POS, lw(pv + 0xc0)) != 0;
        if (!switched) {
            const int32_t idx = lh(m + 0xb2);
            if (lbu(L9_ADDR(0x001bb3b0) + static_cast<uint32_t>(idx) + 0x454) != 0) {
                switched = true;
            } else {
                const uint32_t w = 0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (lwu(0x0015ed84) << 8);
                if (((lw(w) >> (lhu(m + 0xb2) & 31)) & 1) == 0) on = false;
            }
        }
        if (on && lw(pv + 0xc0) != -1) {
            const uint32_t hu = lhu(m + 0xb2);
            const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(hu)) >> 5) << 2;
            const uint32_t w = 0x0014c190 + word + (lwu(0x0015ed84) << 8);
            sw(w, lwu(w) | (1u << (hu & 31)));
            sw(L9_ADDR(0x001ba650) + word, lwu(L9_ADDR(0x001ba650) + word) | (1u << (hu & 31)));
            sw(pv + 0xc0, 0xffffffffu);
        }
    }
    const uint32_t pos = m + 0x10;
    const Vec4 old = V(pos);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xba)));
    if (lbu(m + 0x20) == 0) {  // set up at its first end
        const uint32_t first = lbu(pv + 0xa0);
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 5);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sb(m + 0xbc, first);
        const float at = first != 0 ? 0.0f : 1.0f;
        swc1(pv + 0xa4, at);
        V(pos) = V(float_to_int(at) == 0 ? path + 0x10 : path + 0x10 + ((lwu(path) - 1) << 4));
        sh(pv + 0xba, 0);
        sw(pv + 0xa8, 0);
        sw(pv + 0xbc, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0xc4, 0xffffffffu);
    }
    const uint32_t last = path + (lwu(path) << 4), start = path + 0x10;
    switch (lbu(m + 0xbc)) {
        case 0: {  // at the path's start: the hero nearer its end (or back on it after stepping off) sets it off
            if (!on) break;
            if (lh(pv + 0xb8) == 0 || moby_hero_on_l1(m) == 0) {
                const float to_end = vec3_distance(V(HERO_POS), V(last));
                if (!(vec3_distance(V(HERO_POS), V(start)) < to_end)) break;
            }
            hold_hero(m, pv);
            set_off(m, pv, -1.0f);
            break;
        }
        case 1: {  // at its end
            if (!on) break;
            if (lh(pv + 0xb8) == 0 || moby_hero_on_l1(m) == 0) {
                const float to_start = vec3_distance(V(HERO_POS), V(start));
                if (!(vec3_distance(V(HERO_POS), V(last)) < to_start)) break;
            }
            hold_hero(m, pv);
            moby_sound_start(1, 0, m);
            set_off(m, pv, 1.0f);
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
            float speed = fpu_add(lwc1(pv + 0xa8), fpu_mul(lwc1(pv + 0xac), lwc1(0x0015ed6c)));
            swc1(pv + 0xa8, speed);
            if (abs_f(lwc1(pv + 0xac)) < abs_f(speed)) swc1(pv + 0xa8, lwc1(pv + 0xac));
            const float at = fpu_add(lwc1(pv + 0xa4), lwc1(pv + 0xa8));
            swc1(pv + 0xa4, at);
            if (0.5f < abs_f(fpu_sub(at, 0.5f))) {  // arrived
                if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) hero_set_state(0, 1);
                if (0.0f < lwc1(pv + 0xac)) {
                    sb(m + 0xbc, 0);
                    swc1(pv + 0xa4, 1.0f);
                } else {
                    sb(m + 0xbc, 1);
                    swc1(pv + 0xa4, 0.0f);
                }
                moby_sound_start(2, 0, m);
                sw(pv + 0xa8, 0);
                sh(pv + 0xba, static_cast<uint32_t>(frame_scale_i(15)));
                sound_off(m, pv);
            }
            const int32_t n1 = lw(path) - 1;
            const int32_t k = float_to_int(fpu_mul(int_to_float(n1), lwc1(pv + 0xa4)));
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

}  // namespace rac1
