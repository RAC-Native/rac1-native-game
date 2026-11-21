// Class 408 of Quartu (level 15) and Drek's fleet (17): an alarm the guards of class 44 raise (shared_class44.cpp). One port
// of both levels' copies, the same code relinked: the alarm's frame state is the level's own data (one block, per level
// below), the alarm's draw callback the level's own copy; the group lists 0x1ab840, the moby table 0x15ffd8, the frame
// counters 0x15f5c4 / 0x15f5cc are the engine's (OV); the hero (0x13f350.., 0x1413d4 its state), the sound channels 0x13e550,
// the level number 0x15ed84 and the frame time 0x15ed6c the base ELF's. Header shared_class408.h.
//
// The alarm's block: +0 the frame counter of the last alarm run, +4 an alarm raised this frame, +8 the alarm's level (0 .. 1),
// +0xc its sound playing, +0x10 the sound's channel, +0x14 the moby the sound plays on.
// The pvars: +0 a kind (2: another start), +4 the group of class-78 mobys it wakes, +8 raised, +0xc the frames until it wakes
// them again (i32) and +0x10 that time, +0x14 the light's phase, +0x18 the frames it stays raised (i32).
#include <game/ovl/shared_class408.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr int32_t ALARM_CLASS = 0x198;  // (408)
constexpr uint32_t HERO_STATE = 0x001413d4;
constexpr uint32_t HERO_DEAD = 0x65;

// the alarm's block (the level's own words)
uint32_t alarm(uint32_t o) { return (on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161988) : L15_ADDR(0x00161ab4)) + o; }
// the level's copy of the alarm's draw callback
uint32_t alarm_draw() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002cc740) : L15_ADDR(0x002cbd88); }

float frame_dt() { return lwc1(0x0015ed6c); }
bool shown(uint32_t m) { return (lbu(m + 0x70) & 2) != 0; }  // (the animation's end reached)

// the state set, and the animation `set` started (over 10 frames) unless it runs already
void to_state(uint32_t m, uint32_t state, uint32_t set, int32_t frames = 0xa) {
    sb(m + 0x20, state);
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the light's colour between `a` and `b` as its phase turns, the phase advanced by `turns` turns a second
void light(uint32_t m, uint32_t pv, uint32_t a, uint32_t b, float turns) {
    const float t = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x14)), 1.0f), 0.5f);
    sw(m + 0x90, rgba_lerp(a, b, t));
    swc1(pv + 0x14, angle_add(lwc1(pv + 0x14), fpu_mul(frame_dt(), turns)));
}

// the frame's end of every state: the alarm raised while this one is, the frame's alarm run once
void frame_end(uint32_t m, uint32_t pv) {
    if (lwu(pv + 8) != 0) sw(alarm(4), 1);
    const uint32_t frame = lwu(OV(0x0015f5cc));
    if (lwu(alarm(0)) == frame) return;
    sw(alarm(0), frame);
    class408_alarm(m);
    sw(alarm(4), 0);
}
}  // namespace

// orig: L15:0x002cb4c8 L17:0x002cbe80
void class408_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {  // (the table L15:0x1e2fe0)
    case 0: {  // (0x002cb510) the start: the alarm's block cleared
        const uint32_t kind = lwu(pv);
        sw(alarm(0), 0);
        sw(alarm(4), 0);
        sw(alarm(8), 0);
        sb(m + 0x20, kind == 2 ? 6u : 3u);
        const uint32_t set = kind == 2 ? 4u : 2u;
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, 0);
        break;
    }
    case 1: {  // (0x002cb574) raised, its light pulsing
        const float t = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x14)), 1.0f), 0.5f);
        sw(m + 0x90, rgba_lerp(NOT_ADDR(0x801414bc), NOT_ADDR(0x801414dc), t));
        swc1(pv + 0x14, angle_add(lwc1(pv + 0x14), fpu_mul(frame_dt(), u2f(0x40c90fdbu))));
        if (lwu(pv + 8) == 0 || lwu(OV(0x0015f5c4)) == 2) break;
        if (lwu(pv) != 1) {
            sb(m + 0x20, 5);
            break;
        }
        to_state(m, 6, 4, 0x1e);
        break;
    }
    case 2:  // (0x002cb624)
        if (shown(m)) to_state(m, 3, 2);
        break;
    case 3:  // (0x002cb65c) down: raised again
        if (lwu(pv + 8) == 0 || lwu(OV(0x0015f5c4)) == 2) break;
        to_state(m, 4, 3);
        break;
    case 4: {  // (0x002cb69c) going up
        if (!shown(m)) break;
        if (lwu(pv + 8) == 0 || lwu(OV(0x0015f5c4)) == 2) {
            if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0, frame_scale_i(0xa));
            sb(m + 0x20, 0xa);
            break;
        }
        if (lwu(pv) == 1) {
            to_state(m, 7, 5);
            break;
        }
        if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0, frame_scale_i(0xa));
        sb(m + 0x20, 5);
        break;
    }
    case 5: {  // (0x002cb738) up, flashing, until its time runs out (or a scene)
        if (shown(m) && lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
        light(m, pv, NOT_ADDR(0x801414dc), NOT_ADDR(0x8004048c), u2f(0x41490fdbu));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) == 0 && lwu(OV(0x0015f5c4)) != 2) break;
        class408_raise(m, 0);
        sw(pv + 8, 0);
        if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0, frame_scale_i(0xa));
        sb(m + 0x20, 0xa);
        break;
    }
    case 6:  // (0x002cb838)
        if (lwu(pv + 8) != 0) to_state(m, 7, 5);
        break;
    case 7:  // (0x002cb86c)
        if (shown(m)) to_state(m, 8, 6);
        break;
    case 8: {  // (0x002cb8a4) watching: wakes its class-78 mobys at the hero in sight within 16 units, until its time runs out
        const uint32_t sp = ee_stack_alloc(0x40);
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 16.0f && lwu(OV(0x0015f5c4)) != 2 &&
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc))) != 0) {
            lq_sq(sp, m + 0x10);
            swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.0f));
            if (segment_collide_v(V(sp), V(0x0013f420), 2, 0, nullptr) == 0 && class78_wake(lw(pv + 4), m + 0x10) != 0)
                sw(pv + 0xc, lwu(pv + 0x10));
        }
        ee_stack_free(0x40);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) == 0 && lwu(OV(0x0015f5c4)) != 2) break;
        class408_raise(m, 0);
        sw(pv + 8, 0);
        to_state(m, 9, 7);
        break;
    }
    case 9:  // (0x002cb9b0)
        if (!shown(m)) break;
        if (lwu(pv) == 2) to_state(m, 6, 4);
        else to_state(m, 0xa, 8);
        break;
    case 10:  // (0x002cba2c)
        if (shown(m)) to_state(m, 2, 1);
        break;
    default:
        break;
    }
    frame_end(m, pv);
}

// orig: L15:0x002cbc30 L17:0x002cc5e8
void class408_alarm(uint32_t m) {
    const float step = fpu_add(frame_dt(), frame_dt());
    if (lwu(HERO_STATE) == HERO_DEAD) {
        approach_f(alarm(8), 0.0f, step);
        class408_raise(m, 0);
    } else if (lwu(alarm(4)) != 0 && (lwu(0x0015ed84) != 15 || 240.0f < lwc1(0x0013f3d4))) {
        approach_f(alarm(8), 1.0f, step);  // (on Quartu only past the hero's y 240)
    } else {
        approach_f(alarm(8), 0.0f, step);
    }
    if (lwc1(alarm(8)) == 0.0f) {
        if (sub_0028d8c0(lwu(alarm(0x14)), lw(alarm(0x10))) == 0) {
            sw(alarm(0xc), 0);
        } else {
            sub_0028d918(lw(alarm(0x10)));
            sw(alarm(0x14), 0);
            sw(alarm(0x10), 0xffffffffu);
            sw(alarm(0xc), 0);
        }
        return;
    }
    sw(alarm(0xc), 1);
    if (sub_0028d8c0(lwu(alarm(0x14)), lw(alarm(0x10))) == 0) {
        sw(alarm(0x10), static_cast<uint32_t>(moby_sound_start(5, 0x11, m)));
        sw(alarm(0x14), m);
    }
    draw_callback_add(alarm_draw(), m);
}

// orig: L15:0x002cbac0 L17:0x002cc478
void class408_raise(uint32_t m, int32_t frames) {
    const uint32_t pv = lwu(m + 0x78);
    if (frames == -1) {
        sw(alarm(8), 0);
        frames = 0;
    }
    const uint32_t raised = 0 < frames ? 1u : 0u;
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) {
        const int32_t t = frame_scale_i(frames);
        sw(pv + 8, raised);
        sw(pv + 0x18, static_cast<uint32_t>(t));
        return;
    }
    uint32_t e = lwu(OV(0x001ab840) + (group << 2));
    if (e == 0) return;
    for (;; e += 2) {
        const uint32_t o = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(o + 0xa6) == ALARM_CLASS) {
            const uint32_t opv = lwu(o + 0x78);
            sw(opv + 8, raised);
            sw(opv + 0x18, static_cast<uint32_t>(frame_scale_i(frames)));
        }
        if (lh(e) < 0) break;
    }
}

// orig: L15:0x002cbba0 L17:0x002cc558
void class408_wake(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 1) to_state(m, 2, 1);
    if (lbu(m + 0x20) == 3 && lwu(pv + 8) == 0) sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(0x78)));
}

}  // namespace rac1
