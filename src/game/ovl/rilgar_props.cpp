// Rilgar's (level 5) smaller moving props: the rocking floats (class 810), the props riding on another moby (812), the
// shared hum (838), the fog and light zones (841) and the gates that slide open and sink (843). Ported by hand from
// Rilgar's own code (L5:0x30bdd8, 0x30bf98, 0x30dc68, 0x30e1f8, 0x30e508), the original's order of calls and float
// operations kept.
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x2f0 the height of the water he is in)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
float deg() { return u2f(0x3c8efa35u); }  // (one degree in radians)
uint32_t moby_at(int32_t i) { return lwu(L5_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(i) << 8); }
}  // namespace

// Class 810, a rocking float: it rolls (+0x40) and pitches (+0x44) on two sine phases (+0x6c, +0x60) at the level's rates
// and sizes (0x161cd4 .. 0x161ce0, degrees), creaks now and then (its timer +0x78, the level's range 0x161ce4 ..
// 0x161ce8 frames), and carries what rides it (the attach frame +0x20).
// orig: L5:0x0030bdd8
void ril_class810_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    // the next creak: the level's range of frames
    auto reroll = [pv] {
        const float lo = static_cast<float>(lw(L5_ADDR(0x00161ce4u))), hi = static_cast<float>(lw(L5_ADDR(0x00161ce8u)));
        sw(pv + 0x78, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(lo, hi)))));
    };
    switch (lbu(m + 0x20)) {
    case 0:  // (0x0030be20) set up: the creak's timer, the two phases at random
        reroll();
        swc1(pv + 0x60, rand_angle());
        swc1(pv + 0x6c, rand_angle());
        sb(m + 0x20, 1);
        break;
    case 1: {  // (0x0030be68) rocks
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x78))) != 0) {
            moby_sound_start(0, 0, m);
            reroll();
        }
        const uint32_t sp = ee_stack_alloc(0xb0);
        lq_sq(sp, m + 0x10);
        lq_sq(sp + 0x10, m + 0x40);
        swc1(pv + 0x60, angle_add(lwc1(pv + 0x60), fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cd4u)), deg()), dt())));
        swc1(pv + 0x6c, angle_add(lwc1(pv + 0x6c), fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cd8u)), deg()), dt())));
        swc1(m + 0x44, fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cdcu)), deg()), sin_vu(lwc1(pv + 0x60))));
        swc1(m + 0x40, fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161ce0u)), deg()), sin_vu(lwc1(pv + 0x6c))));
        vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
        attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
        ee_stack_free(0xb0);
        break;
    }
    default:
        break;
    }
}

// Class 812, a prop riding on another moby (its index +0x80; -1: none): its place and angles kept relative to that moby
// (+0x60 / +0x70), and what rides the prop carried (the attach frame +0x20).
// orig: L5:0x0030bf98
void ril_class812_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x0030bfd8) set up: its place relative to its carrier
        sw(pv + 0x5c, 1);
        if (lw(pv + 0x80) < 0) break;
        sub_00260668(m, moby_at(lw(pv + 0x80)), m + 0x10, m + 0x40, pv + 0x60, pv + 0x70);
        sb(m + 0x20, 1);
        break;
    case 1: {  // (0x0030c018) rides
        const uint32_t sp = ee_stack_alloc(0x90);
        lq_sq(sp, m + 0x10);
        lq_sq(sp + 0x10, m + 0x40);
        sub_002604f0(m, moby_at(lw(pv + 0x80)), pv + 0x60, pv + 0x70, m + 0x10, m + 0x40);
        vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
        attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
        ee_stack_free(0x90);
        break;
    }
    default:
        break;
    }
}

// Class 838, one of a class's mobys sharing one looping sound (the slot word *(+4)): within 64 of the camera the
// nearest of them holds it (the slot's moby +0x88 and position +0x90), started when the slot is free or held by another
// class; the draw (0x30de90) queued once a frame (the stamp 0x161d38); when its switch moby (+0) is set (+0xbc) it
// stops: a sound, no collision, state 2.
// orig: L5:0x0030dc68
void ril_class838_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    constexpr uint32_t kSlots = 0x0013e550;  // (the sound slots, 0x70 bytes each: +0x88 the moby, +0x90 the position)
    switch (state) {
    case 0:  // (0x0030dcd4) set up: hidden, the shared slot none
        sb(m + 0x20, 1);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        sw(lwu(pv + 4), 0xffffffffu);
        break;
    case 1: {  // (0x0030dcf8)
        const uint32_t frame = lwu(L5_ADDR(0x0015f5ccu));
        if (lwu(L5_ADDR(0x00161d38u)) != frame) {
            sw(L5_ADDR(0x00161d38u), frame);
            sub_001f93e0(L5_ADDR(0x0030de90u), m);
        }
        const uint32_t cam = L5_ADDR(0x001671c0u);
        if (vec3_distance(V(m + 0x10), V(cam)) < 64.0f) {
            const int32_t slot = lw(lwu(pv + 4));
            bool start = slot < 0;
            bool take = false;
            if (!start) {
                const uint32_t o = lwu(kSlots + static_cast<uint32_t>(slot) * 0x70 + 0x88);
                if (o == 0 || lh(o + 0xa6) != 0x346) start = true;
                else if (o != m) {
                    if (lbu(o + 0x20) != state) take = true;
                    else take = vec3_distance(V(m + 0x10), V(cam)) < vec3_distance(V(o + 0x10), V(cam));
                }
            }
            if (start) {
                sw(lwu(pv + 4), static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            } else if (take) {
                sw(kSlots + static_cast<uint32_t>(lw(lwu(pv + 4))) * 0x70 + 0x88, m);
                lq_sq(kSlots + 0x90 + static_cast<uint32_t>(lw(lwu(pv + 4))) * 0x70, m + 0x10);
            }
        }
        const int32_t sw_moby = lw(pv);
        if (sw_moby >= 0 && lbu(moby_at(sw_moby) + 0xbc) != 0) {
            moby_sound_start(1, 0, m);
            sw(m + 0x94, 0);
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2:  // (0x0030de54) stopped
        if (lwu(m + 0x94) != 0) sw(m + 0x94, 0);
        break;
    default:
        break;
    }
}

// Class 841, the fog and light zones: the level's fog (0x15f444 .. 0x15f454) switched to the zone's own (colour 0x49 /
// 0x53 / 0x0c, near 0, far 33792, 255) when the hero enters its box +0x2c, and back to the level's (saved in state 0)
// in its box +0x28; inside the box +0x38 the level's light colour (0x161200) blended between +0x30 and +0x34 along the
// box's first axis; inside the box +0x28 the hero's water height put out of reach (500).
// orig: L5:0x0030e1f8
void ril_class841_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hero = kHero + 0x80;
    if (point_in_box(hero, lw(pv + 0x28)) != 0) swc1(kHero + 0x2f0, 500.0f);
    // the level's fog from the colour bytes at `c` and the floats at `f` (near / far / two more, as the zone keeps them)
    auto set_fog = [](uint32_t c, uint32_t f0, uint32_t f1, uint32_t f2, uint32_t f3) {
        sb(L5_ADDR(0x0015f444u), lbu(c));
        sb(L5_ADDR(0x0015f445u), lbu(c + 1));
        sb(L5_ADDR(0x0015f446u), lbu(c + 2));
        const float a = lwc1(f0), b = lwc1(f1), d = lwc1(f2), e = lwc1(f3);
        swc1(L5_ADDR(0x0015f448u), a);
        swc1(L5_ADDR(0x0015f44cu), b);
        swc1(L5_ADDR(0x0015f450u), d);
        swc1(L5_ADDR(0x0015f454u), e);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x0030e284) set up: the level's fog and light saved, the zone's set
        sb(pv + 0x20, lbu(L5_ADDR(0x0015f444u)));
        sb(pv + 0x21, lbu(L5_ADDR(0x0015f445u)));
        sb(pv + 0x22, lbu(L5_ADDR(0x0015f446u)));
        const float a = lwc1(L5_ADDR(0x0015f448u)), b = lwc1(L5_ADDR(0x0015f44cu)), c = lwc1(L5_ADDR(0x0015f450u)), d = lwc1(L5_ADDR(0x0015f454u));
        swc1(pv, a);
        swc1(pv + 8, b);
        swc1(pv + 4, c);
        swc1(pv + 0xc, d);
        sb(pv + 0x23, 0x49);
        sb(pv + 0x24, 0x53);
        sb(pv + 0x25, 0xc);
        swc1(pv + 0x18, 33792.0f);
        swc1(pv + 0x14, 255.0f);
        sw(pv + 0x10, 0);
        sw(pv + 0x1c, 0);
        sw(pv + 0x34, 0x40408070u);
        sw(pv + 0x30, lbu(L5_ADDR(0x00161200u)) + (lbu(L5_ADDR(0x00161201u)) << 8) + (lbu(L5_ADDR(0x00161202u)) << 16) +
                          (lbu(L5_ADDR(0x00161203u)) << 24));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x0030e35c) outside: the zone's fog on entering its box
        if (point_in_box(hero, lw(pv + 0x2c)) == 0) break;
        set_fog(pv + 0x23, pv + 0x10, pv + 0x18, pv + 0x14, pv + 0x1c);
        sb(m + 0x20, 2);
        break;
    case 2:  // (0x0030e3cc) inside: the level's fog back on leaving through its box
        if (point_in_box(hero, lw(pv + 0x28)) == 0) break;
        set_fog(pv + 0x20, pv, pv + 8, pv + 4, pv + 0xc);
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    if (point_in_box(hero, lw(pv + 0x38)) != 0) {  // the light's blend along the box
        const uint32_t sp = ee_stack_alloc(0x80);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x38));
        vec3_sub(V(sp), V(hero), V(box + 0x30));
        sw(sp + 0xc, 0);
        mat3_transform(V(sp + 0x10), V(sp), M(lvd::box(lvd::BOXES, lw(pv + 0x38)) + 0x40));
        const uint32_t c = rgba_lerp(lwu(pv + 0x30), lwu(pv + 0x34), fpu_mul(fpu_add(lwc1(sp + 0x10), 1.0f), 0.5f));
        sb(L5_ADDR(0x00161201u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 8));
        sb(L5_ADDR(0x00161202u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 16));
        sb(L5_ADDR(0x00161203u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 24));
        sb(L5_ADDR(0x00161200u), c);
        ee_stack_free(0x80);
    }
}

// Class 843, a gate in a box (+0x10, the level's box list; +0x18 how far from the box's centre along its first axis,
// the other way when +0x1c): when its switch moby (+0x14) is set (+0xbc) it slides out 3 times as far (its velocity
// +0, eased), then sinks 5.9 below the box's centre (its speed +8) and stays.
// orig: L5:0x0030e508
void ril_class843_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t off = sp;
    // the offset along the box's axis, `k` times the distance, flipped by +0x1c
    auto offset = [&](uint32_t box, float k_dist) {
        vec3_set_length(V(off), V(box + 0x10), k_dist);
        if (lw(pv + 0x1c) != 0) vec3_scale(V(off), V(off), -1.0f);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x0030e56c) set up: in its box, along its axis
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        lq_sq(m + 0x10, box + 0x30);
        swc1(m + 0x48, lwc1(box + 0x78));
        vec3_set_length(V(off), V(box + 0x10), lwc1(pv + 0x18));
        if (lw(pv + 0x1c) != 0) {
            vec3_scale(V(off), V(off), -1.0f);
            sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        }
        vec3_add(V(m + 0x10), V(m + 0x10), V(off));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x0030e5e8) waits for its switch
        if (lbu(moby_at(lw(pv + 0x14)) + 0xbc) == 0) break;
        moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        vec_clear(V(pv));
        break;
    case 2: {  // (0x0030e62c) slides out
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        sw(sp + 0x10, 0);
        offset(box, fpu_mul(lwc1(pv + 0x18), 3.0f));
        vec3_add(V(off), V(box + 0x30), V(off));
        vec3_sub(V(off), V(off), V(m + 0x10));
        const float left = vec3_length(V(off));
        swc1(sp + 0x14, vec3_length(V(pv)));
        seek_1d(sp + 0x10, sp + 0x14, left, fpu_mul(dt2(), 20.0f), fpu_mul(dt2(), 30.0f), fpu_mul(dt(), 10.0f));
        vec3_set_length(V(pv), V(off), lwc1(sp + 0x14));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        if (vec3_length(V(off)) < u2f(0x3c23d70au)) sb(m + 0x20, 3);  // (within 0.01)
        break;
    }
    case 3: {  // (0x0030e74c) sinks
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        const float depth = u2f(0x40bccccdu);  // (5.9)
        seek_1d(m + 0x18, pv + 8, fpu_sub(lwc1(box + 0x38), depth), fpu_mul(dt2(), 20.0f), fpu_mul(dt2(), 40.0f), fpu_mul(dt(), 10.0f));
        if (lwc1(m + 0x18) < fpu_sub(lwc1(box + 0x38), depth)) sb(m + 0x20, 4);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x80);
}

}  // namespace rac1
