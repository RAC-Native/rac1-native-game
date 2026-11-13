// Class 655 of Quartu (level 15) and Drek's fleet (17): an electric field. While the hero is in its area (pvar +0x124) the
// field runs its cycle: charging (state 1), it strikes the hero with an arc when the hero stands on the ground in one of its
// boxes, until a switch of class 830 (or a class-1179 moby) is in state 2: then it is down (state 3) for a time. One port of
// both levels' copies, the same code relinked: the arc's words and its points are the level's own data (two blocks, per level
// below), the draw callbacks the level's own copies; the moby table 0x15ffd8, the spots 0x1600ec, the lights 0x1802c0, the fog
// 0x15f444.., the camera 0x166dc0, the done bytes 0x1ba5d0 / 0x1bb230 (+0x454), the flag words 0x1ba4d0 are the engine's (OV);
// the hero (0x13f350.., its moby 0x1413d0 and state 0x1413d4), the sound channels 0x13e550, the level number 0x15ed84, the
// flag words 0x14c190, the view's up axis 0x13f5e0 the base ELF's. Header shared_class655.h.
//
// The pvars: +0 .. +0xc four switches (moby indices), +0x10 the charge time and +0x12 the down time (frames, i16), +0x14 / +0x80
// two sound channels on the moby +0x84, +0x18 the down time (seconds), +0x1c the frames striking, +0x20 .. +0x4c twelve boxes
// (the ground), +0x50 .. +0x5c and +0x88 .. +0x8a the fog saved, +0x60 .. +0x6c four boxes (the hero's light), +0x70 a box,
// +0x74 .. +0x7c three emitters (moby indices), +0x8c a switch held, +0x90 three lists of eight spots (0x20 bytes each), the
// arc's ends +0xf0 / +0x100 / +0x110, +0x120 its light, +0x124 / +0x128 two boxes.
#include <game/ovl/shared_class655.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr int32_t SWITCH_CLASS = 0x33e;  // (830)
constexpr int32_t LIFT_CLASS = 0x49b;    // (1179)
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_HEAD = 0x0013f420;
constexpr uint32_t HERO_MOBY = 0x001413d0;
constexpr uint32_t UP = 0x0013f5e0;
constexpr uint32_t SOUND_SET = 0x28f;
constexpr int32_t POINTS = 20;  // (the main arc's)

// the level's words of the field and its arc (one block)
uint32_t words(uint32_t o) { return (on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161a58) : L15_ADDR(0x00161bc8)) + o; }
// the arc's points and their offsets (one block): +0x10 the main arc's twenty points (0x10 each), +0x150 its drawn points,
// +0x290 the points' height offsets, +0x2e0 their sideways offsets, +0x420 the drawn points' height offsets, +0x460 the
// four branches (0x50 each: five points)
uint32_t arc(uint32_t o) { return (on_level(VELDIN_ORBIT) ? L17_ADDR(0x001d39c0) : L15_ADDR(0x001d3740)) + o; }
// the level's copies of the arc's draw and of the down state's draw
uint32_t arc_draw() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002d6d88) : L15_ADDR(0x002d8710); }
uint32_t down_draw() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002d5e38) : L15_ADDR(0x002d77c0); }

uint32_t first_moby() { return lwu(OV(0x0015ffd8)); }
uint32_t moby_at(uint32_t index) { return first_moby() + (index << 8); }
uint32_t spot(uint32_t index) { return lwu(OV(0x001600ec)) + (index << 7); }

// the sound on channel `slot`'s word stopped, when that channel still plays it on `owner`
void stop_sound(uint32_t slot, uint32_t owner) {
    const int32_t ch = lw(slot);
    if (ch == -1) return;
    const uint32_t c = 0x0013e550 + static_cast<uint32_t>(ch * 0x70);
    if (lwu(c + 0x88) == owner && lbu(c + 0x74) != 0) sub_0028d918(ch);
}
// both of the field's sounds stopped (their channels -1)
void stop_sounds(uint32_t pv) {
    stop_sound(pv + 0x14, lwu(pv + 0x84));
    sw(pv + 0x14, 0xffffffffu);
    stop_sound(pv + 0x80, lwu(pv + 0x84));
    sw(pv + 0x80, 0xffffffffu);
}
// a time in seconds (a level word or a pvar) as frames
int32_t seconds(float s) { return float_to_int(frame_scale_f(fpu_mul(s, 60.0f))); }

// charging again: its times set, state 1
void recharge(uint32_t m, uint32_t pv) {
    sh(pv + 0x10, static_cast<uint32_t>(seconds(lwc1(words(0)))));
    sh(pv + 0x12, static_cast<uint32_t>(frame_scale_i(0xc8)));
    sw(pv + 0x1c, 0);
    sb(m + 0x20, 1);
}

// the hero in one of the four light boxes (+0x60): its light level from the box's spot (outside a scene)
void hero_light(uint32_t pv) {
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t box = pv + 0x60 + k * 4;
        if (point_in_box(HERO_POS, lw(box)) == 0) continue;
        if (sub_0020d568() == 0) continue;
        swc1(HERO + 0x2f0, lwc1(spot(lwu(box)) + 0x38));
    }
}

// the arc's start: its ends from the spots of the list nearest the hero (two neighbours at random), its light there
void arc_from_spots(uint32_t m, uint32_t pv) {
    uint32_t best = 0;
    for (uint32_t list = pv + 0x90, k = 0; k < 3; ++k, list += 0x20) {
        if (lw(list) == -1) continue;
        if (best != 0) {
            const float far = vec3_distance(V(spot(lwu(best)) + 0x30), V(HERO_POS));
            if (!(vec3_distance(V(spot(lwu(list)) + 0x30), V(HERO_POS)) < far)) continue;
        }
        best = list;
    }
    if (best == 0) return;
    int32_t n = 8;
    if (lw(best) == -1) {
        n = 0;
    } else {
        for (int32_t i = 1; i < 8; ++i)
            if (lw(best + static_cast<uint32_t>(i) * 4) == -1) {
                n = i;
                break;
            }
    }
    if (n < 2) return;
    const uint32_t pick = best + (static_cast<uint32_t>(rand_range(0, n - 2)) << 2);
    const int32_t light = lw(pv + 0x120);
    lq_sq(pv + 0xf0, spot(lwu(pick)) + 0x30);
    lq_sq(pv + 0x110, spot(lwu(pick + 4)) + 0x30);
    lq_sq(pv + 0x100, pv + 0x110);
    if (light == -1) {
        sw(pv + 0x120, static_cast<uint32_t>(light_add(pv + 0xf0, 8.0f, 0.0f, 1.0f, 1.0f, 0.5f)));
    } else {
        const uint32_t l = OV(0x001802c0) + (static_cast<uint32_t>(light) << 5);
        lq_sq(l + 0x10, pv + 0xf0);
        swc1(l + 0x1c, 8.0f);
        swc1(l + 4, 1.0f);
        swc1(l + 8, 0.5f);
        sw(l + 0xc, 0);
        swc1(l, 1.0f);
    }
    class655_arcs_reset(m);
    class655_arcs(m);
    draw_callback_add(arc_draw(), m);
}

// the strike: the arc from the hero's head to a point about it, the hero hit (a push away from the field's facing), the fog
// lit (saved on the first frame), restored when the field went down this frame
void strike(uint32_t m, uint32_t pv) {
    if (lwu(pv + 0x84) != 0 && lwu(pv + 0x84) != lwu(HERO_MOBY)) stop_sounds(pv);
    sw(pv + 0x84, lwu(HERO_MOBY));
    sw(pv + 0x14, static_cast<uint32_t>(moby_sound_play(0, 4, lwu(HERO_MOBY), SOUND_SET)));
    const int32_t second = moby_sound_play(1, 4, lwu(HERO_MOBY), SOUND_SET);
    sw(pv + 0x80, static_cast<uint32_t>(second));
    if (lwu(pv + 0x1c) == 0) {
        swc1(pv + 0x50, lwc1(OV(0x0015f448)));
        swc1(pv + 0x54, lwc1(OV(0x0015f44c)));
        swc1(pv + 0x58, lwc1(OV(0x0015f450)));
        swc1(pv + 0x5c, lwc1(OV(0x0015f454)));
        sb(pv + 0x88, lbu(OV(0x0015f444)));
        sb(pv + 0x89, lbu(OV(0x0015f445)));
        sb(pv + 0x8a, lbu(OV(0x0015f446)));
    }
    sw(pv + 0x1c, lwu(pv + 0x1c) + 1);
    sb(OV(0x0015f446), 0x7f);
    sb(OV(0x0015f444), 0x7f);
    sb(OV(0x0015f445), 0x7f);
    swc1(OV(0x0015f44c), 131072.0f);
    sw(OV(0x0015f448), 0);
    swc1(OV(0x0015f450), rand_range_f(128.0f, 255.0f));
    sw(OV(0x0015f454), 0);
    const float r = rand_range_f(5.0f, 8.0f);
    const float a = rand_angle();
    const float b = rand_angle();
    vec_from_spherical_v(V(pv + 0x110), r, a, b);
    vec3_add_inplace(V(pv + 0x110), V(HERO_HEAD));
    lq_sq(pv + 0x100, pv + 0x110);
    lq_sq(pv + 0xf0, HERO_HEAD);
    class655_arcs_reset(m);
    class655_arcs(m);
    draw_callback_add(arc_draw(), m);
    // the hit record: its source, kind 0x10001, its push (a fifth along the field's facing, made a unit), damage 1
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t rec = sp + 0x10;
    sw(rec + 0x10, m);
    sw(rec + 0x14, 0x10001);
    swc1(rec + 0x1c, 1.0f);
    swc1(rec, fpu_mul(cos_vu(lwc1(m + 0x48)), 0.2f));
    const float y = fpu_mul(sin_vu(lwc1(m + 0x48)), 0.2f);
    sw(rec + 8, 0);
    swc1(rec + 4, y);
    vec2_set_length(V(rec), V(rec), 1.0f);
    sb(rec + 0x19, 1);
    sh(rec + 0x1a, lhu(m + 0xa6));
    swc1(rec + 8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));
    sb(rec + 0x18, 0);
    moby_hit_record(lwu(HERO_MOBY), rec);
    ee_stack_free(0x40);
    if (lbu(m + 0x20) != 2) return;
    swc1(OV(0x0015f448), lwc1(pv + 0x50));
    swc1(OV(0x0015f44c), lwc1(pv + 0x54));
    swc1(OV(0x0015f450), lwc1(pv + 0x58));
    swc1(OV(0x0015f454), lwc1(pv + 0x5c));
    sb(OV(0x0015f444), lbu(pv + 0x88));
    sb(OV(0x0015f445), lbu(pv + 0x89));
    sb(OV(0x0015f446), lbu(pv + 0x8a));
}

// the emitters' hum on the one nearest the camera (moving the sounds to it), the strike's second sound while charging, the
// charge's end, and a switch in state 2 (or a class-1179 moby) bringing the field down
void hum_and_switches(uint32_t m, uint32_t pv) {
    const uint32_t cam = OV(0x00166dc0);
    uint32_t nearest = 0;
    for (uint32_t k = 0; k < 3; ++k) {
        const int32_t idx = lw(pv + 0x74 + k * 4);
        if (idx == -1) continue;
        if (nearest != 0) {
            const float far = vec2_distance(V(nearest + 0x10), V(cam));
            if (!(vec2_distance(V(moby_at(static_cast<uint32_t>(idx)) + 0x10), V(cam)) < far)) continue;
        }
        nearest = moby_at(static_cast<uint32_t>(idx));
    }
    if (lwu(pv + 0x84) == 0) {
        sw(pv + 0x84, nearest);
    } else {
        const float far = vec2_distance(V(lwu(pv + 0x84) + 0x10), V(cam));
        if (vec2_distance(V(nearest + 0x10), V(cam)) < far) {
            stop_sounds(pv);
            sw(pv + 0x84, nearest);
        }
    }
    if (sub_0028d8c0(lwu(pv + 0x84), lw(pv + 0x14)) == 0)
        sw(pv + 0x14, static_cast<uint32_t>(moby_sound_play(0, 5, lwu(pv + 0x84), SOUND_SET)));
    if (lbu(m + 0x20) == 1 && sub_0028d8c0(lwu(pv + 0x84), lw(pv + 0x80)) == 0)
        sw(pv + 0x80, static_cast<uint32_t>(moby_sound_play(1, 5, lwu(pv + 0x84), SOUND_SET)));
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x12))) != 0 && lbu(m + 0x20) == 2) recharge(m, pv);
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t idx = lw(pv + k * 4);
        if (idx == -1) continue;
        const uint32_t o = moby_at(static_cast<uint32_t>(idx));
        if (!((lh(o + 0xa6) == SWITCH_CLASS && lbu(o + 0x20) == 2) || moby_is_in_state(o, LIFT_CLASS, 2) != 0)) continue;
        sh(pv + 0x12, static_cast<uint32_t>(seconds(lwc1(pv + 0x18))));
        sb(m + 0x20, 3);
        moby_sound_play(2, 0, moby_at(lwu(pv + k * 4)), SOUND_SET);
        sw(pv + 0x8c, 0);
    }
}
}  // namespace

// orig: L15:0x002d6810 L17:0x002d4e88
void class655_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state != 0 && point_in_box(HERO_POS, lw(pv + 0x124)) == 0) {  // the hero away: quiet
        if (lwu(pv + 0x84) != 0) {
            stop_sound(pv + 0x14, lwu(pv + 0x84));
            sw(pv + 0x14, 0xffffffffu);
            stop_sound(pv + 0x80, lwu(pv + 0x84));
            sw(pv + 0x80, 0xffffffffu);
        }
        const int32_t light = lw(pv + 0x120);
        if (light != -1) {
            region_clear(static_cast<uint32_t>(light));
            sw(pv + 0x120, 0xffffffffu);
        }
        return;
    }
    switch (state) {
    case 0: {  // the start: its switches' done marks cleared
        sb(m + 0x30, 0xff);
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + k * 4);
            if (idx == -1) continue;
            const uint32_t o = moby_at(static_cast<uint32_t>(idx));
            if (lh(o + 0xa6) != SWITCH_CLASS) {
                class1179_lift(o);
                continue;
            }
            const int32_t flag = static_cast<int16_t>(lhu(o + 0xb2));
            sb(OV(0x001baa24) + static_cast<uint32_t>(flag), 0);
            sb(OV(0x001bb684) + static_cast<uint32_t>(flag), 0);
            const uint32_t bit = 1u << (lhu(o + 0xb2) & 0x1f);
            const uint32_t level_word = 0x0014c190 + (static_cast<uint32_t>(flag >> 5) << 2) + (lwu(0x0015ed84) << 8);
            sw(level_word, lwu(level_word) & ~bit);
            const uint32_t word = OV(0x001ba4d0) + (static_cast<uint32_t>(flag >> 5) << 2);
            sw(word, lwu(word) & ~bit);
        }
        recharge(m, pv);
        sw(pv + 0x120, 0xffffffffu);
        sw(pv + 0x80, 0xffffffffu);
        sw(pv + 0x14, 0xffffffffu);
        break;
    }
    case 1:
    case 2: {
        bool in_area = false, grounded = false;
        if (state == 1) {
            grounded = class655_hero_grounded() != 0;
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) {  // charged: striking (state 2)
                sh(pv + 0x12, static_cast<uint32_t>(seconds(lwc1(words(4)))));
                sb(m + 0x20, 2);
                if (lwu(pv + 0x84) != 0) {
                    stop_sound(pv + 0x80, lwu(pv + 0x84));
                    sw(pv + 0x80, 0xffffffffu);
                }
            }
            for (uint32_t k = 0; k < 12; ++k)
                if (point_in_box(HERO_POS, lw(pv + 0x20 + k * 4)) != 0) {
                    in_area = true;
                    break;
                }
            if (lbu(m + 0x20) == 2) {
                for (uint32_t k = 0; k < 3; ++k) {
                    const int32_t idx = lw(pv + 0x74 + k * 4);
                    if (idx != -1) moby_set_color_hi(moby_at(static_cast<uint32_t>(idx)), 0x40, 0x40, 0x40);
                }
            } else {
                for (uint32_t k = 0; k < 3; ++k) {
                    const int32_t c = rand_range(0x40, 0x7f);
                    const int32_t idx = lw(pv + 0x74 + k * 4);
                    if (idx != -1) moby_set_color_hi(moby_at(static_cast<uint32_t>(idx)), c, c, c);
                }
            }
            // the arc between spots, now and then, unless it strikes the hero
            if (!(in_area && grounded) && rand_mod(1) == 0 && lwc1(words(8)) == 0.0f) arc_from_spots(m, pv);
            if (in_area && grounded) strike(m, pv);
        }
        hum_and_switches(m, pv);
        break;
    }
    case 3: {  // down: a while, the switch held keeps it so
        for (uint32_t k = 0; k < 3; ++k) {
            const int32_t idx = lw(pv + 0x74 + k * 4);
            if (idx != -1) moby_set_color_hi(moby_at(static_cast<uint32_t>(idx)), 0x40, 0x40, 0x40);
        }
        const int32_t light = lw(pv + 0x120);
        if (light != -1) {
            region_clear(static_cast<uint32_t>(light));
            sw(pv + 0x120, 0xffffffffu);
        }
        if (lh(pv + 0x12) != 0) sub_001f93e0(down_draw(), m);
        if (lwu(pv + 0x84) != 0) {
            stop_sounds(pv);
            sw(pv + 0x84, 0);
        }
        uint32_t held = 1;
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + k * 4);
            if (idx == -1) continue;
            const uint32_t o = moby_at(static_cast<uint32_t>(idx));
            if (lwu(HERO + 0x2fc) != o || lh(o + 0xa6) != SWITCH_CLASS || lh(HERO + 0x30e) != 0) continue;
            if (lwu(pv + 0x8c) != 0) {
                const int32_t left = lh(pv + 0x12);
                if (left < seconds(lwc1(pv + 0x18)) - frame_scale_i(0x3c)) {
                    sh(pv + 0x12, static_cast<uint32_t>(seconds(lwc1(pv + 0x18))));
                    moby_sound_play(2, 0, moby_at(lwu(pv + k * 4)), SOUND_SET);
                }
            }
            held = 0;
        }
        sw(pv + 0x8c, held);
        bool up = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x12))) != 0;
        if (!up) {
            if (point_in_box(HERO_POS, lw(pv + 0x70)) != 0 && lh(HERO + 0x30e) == 0) up = true;
            else if (point_in_box(HERO_POS, lw(pv + 0x128)) != 0) up = true;
        }
        if (!up) break;
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + k * 4);
            if (idx == -1) continue;
            const uint32_t o = moby_at(static_cast<uint32_t>(idx));
            if (lh(o + 0xa6) == SWITCH_CLASS) sb(o + 0x20, 1);
            else class1179_lift(o);
        }
        recharge(m, pv);
        break;
    }
    default:
        break;
    }
    hero_light(pv);
}

// orig: L15:0x002d79e8 L17:0x002d6060
void class655_arcs_reset(uint32_t m) {
    if (lwu(words(0x30)) == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t line = sp, step = sp + 0x10;
    const int32_t frames = lw(words(0xc4));
    sw(words(0x30), 0);
    sw(words(0x11c), 0);
    sw(words(0x118), static_cast<uint32_t>(frame_scale_i(frames)));
    sw(words(0x98), 0x7f2020);
    vec3_sub(V(line), V(pv + 0x100), V(pv + 0xf0));
    vec3_set_length(V(step), V(line), fpu_div(lwc1(words(0x34)), 20.0f));
    lq_sq(arc(0x10), pv + 0xf0);
    for (uint32_t i = 0; i < 19; ++i) {
        vec3_add(V(arc(0x20) + i * 0x10), V(arc(0x10) + i * 0x10), V(step));
        sw(arc(0x294) + i * 4, 0);
        vec_clear(V(arc(0x2f0) + i * 0x10));
        sw(arc(0x424) + i * 4, 0);
    }
    for (uint32_t k = 0; k < 4; ++k) sh(words(0xfe) - k * 2, 0xffff);
    sh(words(0x10c), 8);
    sh(words(0x10e), 4);
    sh(words(0x116), 2);
    sh(words(0x108), 8);
    sh(words(0x110), 4);
    sh(words(0x10a), 4);
    sh(words(0x112), 2);
    sh(words(0x114), 4);
    sw(words(0x4c), 0);
    sw(words(0x54), 0);
    sw(words(0x68), 0);
    ee_stack_free(0xa0);
}

// orig: L15:0x002d7bb0 L17:0x002d6228
void class655_arcs(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1f0);
    const uint32_t dir = sp, rnd = sp + 0x10, bend = sp + 0x20, drift = sp + 0x30, side = sp + 0x40, to_end = sp + 0x50;
    const uint32_t across = sp + 0x60, up = sp + 0x70, base = sp + 0x80, to_cam = sp + 0x90, jolt = sp + 0xa0;
    const uint32_t from = pv + 0xf0, to = pv + 0x110;
    const uint32_t bright = lbu(0x0013e533);
    swc1(words(0x4c), angle_add(lwc1(words(0x4c)), lwc1(words(0x50))));
    swc1(words(0x54), angle_sub(lwc1(words(0x54)), lwc1(words(0x58))));
    swc1(words(0x68), angle_add(lwc1(words(0x68)), lwc1(words(0x6c))));
    float seg = fpu_mul(vec3_distance(V(from), V(to)), 0.05f);  // (a twentieth of the arc)
    sw(words(0x98), bright != 0 ? 0x7f2040u : 0x7f2020u);
    vec3_sub(V(dir), V(pv + 0x100), V(from));
    vec3_set_length(V(dir), V(dir), 1.0f);
    vec3_cross_ba(V(rnd), V(dir), V(UP));
    vec_rotate_axis(V(dir), V(dir), V(rnd), u2f(0x3e32b8c2u));  // (10 degrees)
    vec3_scale(V(drift), V(dir), seg);
    lq_sq(arc(0x10), from);
    lq_sq(arc(0x150), from);
    // the main arc: each point a step from the last along a drifting direction, its height waving, toward the end
    for (int32_t i = 1; i < POINTS; ++i) {
        const uint32_t k = static_cast<uint32_t>(i);
        const uint32_t point = arc(0x10) + k * 0x10, last = point - 0x10, drawn = arc(0x150) + k * 0x10;
        const float fi = static_cast<float>(i);
        const float share = fpu_mul(int_to_float(i), 0.05f);
        const float lo = lwc1(words(0x44));
        const float span = fpu_sub(lwc1(words(0x48)), lo);
        const float phase = fpu_add(lwc1(words(0x4c)), fpu_mul(fi, lwc1(words(0x40))));
        float wave = fpu_mul(fpu_add(lo, fpu_mul(span, share)), sin_vu(angle_wrap_mod(phase)));
        wave = fpu_add(wave, rand_range_signed_f(lwc1(words(0x84)), lwc1(words(0x88))));
        const uint32_t height = arc(0x290) + k * 4;
        const float z = fpu_sub(lwc1(point + 8), lwc1(height));
        swc1(height, wave);
        swc1(point + 8, z);
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(V(rnd), seg, a, b);
        swc1(rnd + 8, fpu_mul(lwc1(rnd + 8), 0.5f));
        vec3_lerp(V(bend), V(drift), V(rnd), lwc1(words(0xd0)));
        const float len = vec3_length(V(bend));
        if (len == 0.0f) {
            lq_sq(point, last);
        } else {
            vec3_scale(V(bend), V(bend), fpu_div(seg, len));
            vec3_add(V(point), V(last), V(bend));
            lq_sq(drift, bend);
        }
        swc1(point + 8, fpu_add(lwc1(point + 8), wave));
        vec3_sub(V(rnd), V(point), V(last));
        vec3_sub(V(to_end), V(to), V(point));
        const float left = vec3_length(V(to_end));
        if (left != 0.0f) {  // the drift turned toward the end, harder near it
            float pull = 0.1f;
            if (i == 19) {
                pull = 0.5f;
                seg = left;
            } else if (!(i < 16)) {
                pull = 0.5f;
                seg = fpu_div(left, int_to_float(0x13 - i));
            }
            vec3_scale(V(to_end), V(to_end), fpu_div(seg, left));
            vec3_lerp(V(drift), V(drift), V(to_end), pull);
        }
        if (i < 10) {  // the drawn point: waved up and sideways
            const float turn = angle_wrap_mod(fpu_add(lwc1(words(0x54)), fpu_mul(fi, lwc1(words(0x5c)))));
            const float amp = fpu_mul(lwc1(words(0x60)), sin_vu(angle_wrap_mod(fpu_add(lwc1(words(0x68)), fpu_mul(fi, lwc1(words(0x70)))))));
            lq_sq(drawn, point);
            const float s = sin_vu(turn);
            const float jitter = rand_range_signed_f(0.0f, 0.1f);
            const float lift = fpu_add(fpu_mul(amp, s), jitter);
            swc1(arc(0x420) + k * 4, lift);
            swc1(drawn + 8, fpu_add(lwc1(drawn + 8), lift));
            const float yaw = angle_add(atan2_f(lwc1(rnd), lwc1(rnd + 4)), u2f(0x3fc90fdbu));
            const float width = fpu_mul(lwc1(words(0x64)), cos_vu(turn));
            swc1(side, fpu_mul(cos_vu(yaw), width));
            const float sy = fpu_mul(sin_vu(yaw), width);
            swc1(side + 8, 0.0f);
            swc1(side + 4, sy);
            lq_sq(arc(0x2e0) + k * 0x10, side);
            vec3_add(V(drawn), V(drawn), V(side));
        } else {  // mirrored: the offsets of point 21 - i
            const uint32_t twin = 0x15 - k;
            lq_sq(drawn, point);
            swc1(drawn + 8, fpu_add(lwc1(drawn + 8), lwc1(arc(0x420) + twin * 4)));
            vec3_add(V(drawn), V(drawn), V(arc(0x2e0) + twin * 0x10));
            lq_sq(side, arc(0x2e0) + k * 0x10);
        }
    }
    // the four branches: two from the start, two from the middle (+0x100); each re-forms when its timer runs out, else
    // flickers, its fade from its timers
    vec3_cross_ba(V(across), V(dir), V(UP));
    vec3_set_length(V(across), V(across), 1.0f);
    vec3_set_length(V(up), V(UP), 1.0f);
    for (uint32_t j = 0; j < 4; ++j) {
        float reach;
        if (j < 2) {
            lq_sq(base, from);
            reach = 0.4f;
        } else {
            lq_sq(base, pv + 0x100);
            reach = 0.5f;
        }
        const uint32_t timer = words(0xf8) + j * 2;
        const uint32_t p0 = arc(0x470) + j * 0x50;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(timer))) != 0) {
            sh(timer, static_cast<uint32_t>(frame_scale_i(lh(words(0x108) + j * 2))));
            sh(words(0x100) + j * 2, 0);
            const float pitch = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
            const float yaw = rand_range_f(u2f(0xbf860a92u), u2f(0x3e32b8c2u));
            vec3_scale(V(to_end), V(dir), reach);
            vec_rotate_axis(V(to_end), V(to_end), V(across), yaw);
            vec_rotate_axis(V(to_end), V(to_end), V(up), pitch);
            lq_sq(p0, base);
            vec3_add(V(p0 + 0x10), V(base), V(to_end));
            float sign = 1.0f;
            for (uint32_t q = 0; q < 3; ++q) {
                const uint32_t a = p0 + 0x10 + q * 0x10;
                vec3_sub(V(to_cam), V(a), V(OV(0x00166dc0)));
                const float t = fpu_mul(rand_range_f(u2f(0x3e32b8c2u), u2f(0x3f75be0bu)), sign);
                sign = -sign;
                vec_rotate_axis(V(to_end), V(to_end), V(to_cam), t);
                vec3_add(V(a + 0x10), V(a), V(to_end));
            }
        } else {
            for (uint32_t q = 0; q < 4; ++q) {
                const uint32_t d = jolt + 0x10 + q * 0x10;
                vec3_sub(V(d), V(p0 + 0x10 + q * 0x10), V(p0 + q * 0x10));
                swc1(d, fpu_add(lwc1(d), rand_range_signed_f(0.0f, 0.1f)));
                swc1(d + 4, fpu_add(lwc1(d + 4), rand_range_signed_f(0.0f, 0.1f)));
                swc1(d + 8, fpu_add(lwc1(d + 8), rand_range_signed_f(0.0f, 0.1f)));
            }
            lq_sq(p0, base);
            for (uint32_t q = 0; q < 4; ++q) vec3_add(V(p0 + 0x10 + q * 0x10), V(p0 + q * 0x10), V(jolt + 0x10 + q * 0x10));
            const int32_t a = lh(words(0x110) + j * 2), b = lh(timer);
            const int32_t gap = a < b ? b - a : a - b;
            const float fade = fpu_div(int_to_float(gap), int_to_float(frame_scale_i(0xf)));
            sh(words(0x100) + j * 2, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, fade), 32.0f))));
        }
    }
    // three sparks: two at the start along a turned direction, one at the end
    const float pitch = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
    const float yaw = rand_range_f(u2f(0xbf060a92u), u2f(0x3f860a92u));
    vec3_scale(V(to_end), V(dir), fpu_mul(lwc1(words(0xc8)), lwc1(0x0015ed60)));
    vec_rotate_axis(V(to_end), V(to_end), V(across), yaw);
    vec_rotate_axis(V(to_end), V(to_end), V(up), pitch);
    int32_t spin = rand_range(1, 8);
    if (rand_mod(2) != 0) spin = -spin;
    float size = rand_range_f(0.2f, 0.6f);
    spawn_particle_35(from, static_cast<uint32_t>(frame_scale_i(0xf)), lwu(words(0x98)) | 0x7f000000u, 0, static_cast<uint32_t>(spin), to_end,
                      fpu_mul(size, 0.3f), size, 0.0f);
    spin = -spin;
    spawn_particle_35(from, static_cast<uint32_t>(frame_scale_i(0xf)), 0x307f7f7fu, 0, static_cast<uint32_t>(spin), to_end,
                      fpu_mul(size, 0.15f), fpu_mul(size, 0.5f), 0.0f);
    spin = rand_range(1, 8);
    if (rand_mod(2) != 0) spin = -spin;
    size = rand_range_f(0.15f, 3.0f);
    spawn_particle_35(to, static_cast<uint32_t>(frame_scale_i(0xc)), lwu(words(0x98)) | 0x7f000000u, 0, static_cast<uint32_t>(spin),
                      OV(0x0015f580), fpu_mul(size, 0.1f), size, 0.0f);
    ee_stack_free(0x1f0);
}

// orig: L15:0x002d6770 L17:0x002d4de8
uint32_t class655_hero_grounded() {
    if (sub_0020d568() != 0) return 1;
    if (lwu(0x001413d4) - 0x18u >= 2) return 0;
    const uint32_t sp = ee_stack_alloc(0x40);
    lq_sq(sp, HERO_POS);
    swc1(sp + 8, fpu_add(lwc1(sp + 8), 2.0f));
    const float floor = floor_probe_v(0.5f, V(sp), 0);
    ee_stack_free(0x40);
    if (sub_001f35d8() != 0) return 0;
    return lwc1(HERO_POS + 8) <= floor ? 1u : 0u;
}

// orig: L15:0x002d95a0 L17:0x002d77a8
uint32_t class1179_lift(uint32_t o) {
    if (moby_is_in_state(o, LIFT_CLASS, 2) != 0) {
        sb(o + 0x20, 1);
        swc1(o + 0x18, fpu_add(lwc1(o + 0x18), 1.5f));
    }
    return 1;  // (on both paths: the original's delay slot sets it before the test's branch)
}

}  // namespace rac1
