// Class 615, the Trespasser's pad, and its puzzle: one port of the copies of Aridia (L2), Eudora (L4), Blarg (L6), Batalia
// (L8), Pokitaru (L11), Gemlik (L13) and the Veldin finale (L18), which are the same code relinked (word by word: only the
// relocations differ). Its addresses are the base ELF's (bare: the hero 0x13f350.., the pad 0x13c940.., the level number
// 0x15ed84, the per-level flag words 0x14c190, the frame time 0x15ed6c / its square 0x15ed70), the engine's (OV: the frame
// counter 0x15f5cc, the camera 0x166c80.. and its scripted goal 0x166dc0 / 0x166dd0, the camera records 0x1600ec, the HUD
// word 0x17e508, the moby flags 0x1bb230 / 0x1ba4d0, the save state 0x179410 / 0x179434) or the level's own: a block of the
// puzzle's numbers (data(), its texture coordinates, radii, widths and colours), a block of the frame's quads and matrix
// (frame_block()) and the draw callback's own address, which the pad queues. Header shared_class615.h.
//
// The pvars: the timer +0x10 (a half), the "on the pad" flag +0x12, the rings' turns +0x14 / +0x18 / +0x1c (each a slot
// offset 0..11), the rings' twelve slots +0x20 / +0x50 / +0x80 (0 empty, 1 a gap, 2 a link, 3 a new link), the twelve
// lights +0xb0 (0 off, 1 unlit, 2 lit), the active ring +0xe0 (1..3) and its turn in progress +0xe4 (radians), the camera
// record +0xe8 (-1 none), the texture base +0xec, the camera's start +0xf0 / +0x100 and the blend +0x110 / its speed
// +0x114, the sound flags +0x11e.
#include <game/ovl/shared_class615.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;          // the hero's record
constexpr uint32_t HERO_POS = 0x0013f3d0;      // his position (+0x80)
constexpr uint32_t PAD = 0x0013c940;           // the pad's record: +0x1a4 the buttons pressed, +0x1c0 the sticks
constexpr uint32_t TRESPASSER_ITEM = 0x1a;     // the Trespasser, the item in hand (+0x10b8)
constexpr uint32_t PAD_STATE = 0x72;           // the hero's state on the pad
constexpr uint32_t LINK = 2, NEW_LINK = 3;     // a slot's link, and a link that has just reached a light
constexpr int32_t SLOTS = 12;

// The puzzle's numbers (the level's own block): +0 / +4 / +8 the pad's colours (ready, waiting, the hero off it), +0xc the
// frame's texture coordinates, +0x1c / +0x2c / +0x3c the markers', +0x4c / +0x5c the links', +0x6c the rings', +0x7c the
// texture base; the ring widths +0x90 .. +0xb8 and radii +0x98 / +0xa4 / +0xb0 / +0xbc (words), the links' scroll +0xc0
// (a float) and texture +0xc4; the colours +0xf4 (the frame), +0xfc (the markers), +0x100 / +0x104 (a ring, the active
// one's glow), +0x10c / +0x110 / +0x114 (the links), +0x11c .. +0x124 (the lights).
uint32_t data(uint32_t o) {
    if (on_level(EUDORA)) return L4_ADDR(0x001619f0) + o;
    if (on_level(BLARG)) return L6_ADDR(0x00161970) + o;
    if (on_level(BATALIA)) return L8_ADDR(0x00161e40) + o;
    if (on_level(POKITARU)) return L11_ADDR(0x00161ad0) + o;
    if (on_level(GEMLIK)) return L13_ADDR(0x00161a30) + o;
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x00161ac0) + o;
    return L2_ADDR(0x00161aa0) + o;
}
// The frame's block (the level's own): four quads' corners (+0, 0x20 each), then the frame's matrix +0xa0.
uint32_t frame_block(uint32_t o) {
    if (on_level(EUDORA)) return L4_ADDR(0x001d2f70) + o;
    if (on_level(BLARG)) return L6_ADDR(0x001d4a20) + o;
    if (on_level(BATALIA)) return L8_ADDR(0x001e9300) + o;
    if (on_level(POKITARU)) return L11_ADDR(0x001d36b0) + o;
    if (on_level(GEMLIK)) return L13_ADDR(0x001d36e0) + o;
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001d36d0) + o;
    return L2_ADDR(0x001d3310) + o;
}
// The level's copy of the puzzle's draw callback (the address the pad queues)
uint32_t puzzle_draw_fn() {
    if (on_level(EUDORA)) return L4_ADDR(0x002d53f8);
    if (on_level(BLARG)) return L6_ADDR(0x002e2258);
    if (on_level(BATALIA)) return L8_ADDR(0x002f2948);
    if (on_level(POKITARU)) return L11_ADDR(0x002fd6c0);
    if (on_level(GEMLIK)) return L13_ADDR(0x002f5f30);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x002d98e8);
    return L2_ADDR(0x002d93e8);
}
// The hero back to his idle state: Blarg's build of it is its own (still generated); Aridia's goes through the Veldin port's
// dispatch, Eudora's is the Veldin port's code
void hero_idle() {
    if (on_level(BLARG)) { blg_239528_l6(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); return; }
    hero_enter_idle();
}

float frame_time() { return lwc1(0x0015ed6c); }
int32_t frame_counter() { return lw(OV(0x0015f5cc)); }
// a word of the level's numbers as a float (cvt.s.w), and its negation's
float word_f(uint32_t a) { return static_cast<float>(lw(a)); }
float neg_word_f(uint32_t a) { return static_cast<float>(static_cast<int32_t>(0u - lwu(a))); }
// a ring's slot `x` (taken modulo 12 as the original's `div`: a negative x gives a negative slot)
uint32_t slot(uint32_t ring, int32_t x) { return ring + (static_cast<uint32_t>(ee_rem(x, SLOTS)) << 2); }

// The moby's saved flag (its index +0xb2 in the level's word table 0x14c190, or its byte at 0x1bb230 + 0x454): the pad
// already solved
bool solved_before(uint32_t m) {
    const uint32_t idx = lhu(m + 0xb2);
    const int32_t sidx = static_cast<int16_t>(idx);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(sidx) + 0x454) != 0) return true;
    const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(sidx >> 5) << 2) + (lwu(0x0015ed84) << 8));
    return ((static_cast<int32_t>(word) >> (idx & 0x1f)) & 1) != 0;
}
// ... and recorded (the level's word table and the moby bits 0x1ba4d0)
void record_solved(uint32_t m) {
    uint32_t idx = lhu(m + 0xb2);
    const uint32_t w = 0x0014c190 + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2) + (lwu(0x0015ed84) << 8);
    sw(w, lwu(w) | (1u << (idx & 0x1f)));
    idx = lhu(m + 0xb2);
    const uint32_t b = OV(0x001ba4d0) + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2);
    sw(b, lwu(b) | (1u << (idx & 0x1f)));
}

// A screen corner's XYZ2 doubleword: `depth` (the Z word and the upper bits) plus Y from `y_hi` (the upper half, negated,
// + 0x8000) and X from `x` (+ 0x8000), both in the GS's fixed point already
uint64_t corner(uint64_t depth, uint32_t x, uint32_t y_hi) {
    const uint32_t y = 0u - y_hi - 0x80000000u;
    return depth + y + static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(x + 0x8000u)));
}
constexpr uint64_t DEPTH = 0x00fffff000000000ull;  // (0xfffff000 << 24)
}  // namespace

// orig: L2:0x002dae88 L4:0x002d6e98 L6:0x002e3cf8 L8:0x002f43e8 L11:0x002ff160 L13:0x002f79d0 L18:0x002db388
void class615_frame_draw() {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t xyz = sp, colours = sp + 0x20, uvs = sp + 0x30;
    for (uint32_t q = 0; q < 4; ++q) {
        const uint32_t corners = frame_block(q * 0x20);
        for (uint32_t k = 0; k < 4; ++k) sd(xyz + k * 8, ld(corners + k * 8));
        for (uint32_t k = 0; k < 4; ++k) sw(uvs + k * 4, lwu(data(0x0c + k * 4)));
        for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, lwu(data(0xf4)));
        gs_quad_packet(xyz, uvs, colours, effect_tex0(lw(data(0x7c)) + 0x28), 1);
    }
    ee_stack_free(0x90);
}

// orig: L2:0x002da6c0 L4:0x002d66d0 L6:0x002e3530 L8:0x002f3c20 L11:0x002fe998 L13:0x002f7208 L18:0x002dabc0
void class615_marker_draw(uint32_t kind, float angle, float radius) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t xyz = sp, colours = sp + 0x20, uvs = sp + 0x30, turn = sp + 0x40, angles = sp + 0x80;
    const uint32_t uv = kind == 0 ? data(0x1c) : (kind == 1 ? data(0x2c) : data(0x3c));
    for (uint32_t i = 0; i < 4; ++i) sw(uvs + i * 4, lwu(uv + i * 4));
    for (uint32_t i = 0; i < 4; ++i) sw(colours + i * 4, lwu(data(0xfc)));
    const float scale = 16.0f;
    const int32_t x = float_to_int(fpu_mul(fpu_mul(sin_vu(angle), radius), scale));
    const int32_t y = float_to_int(fpu_mul(fpu_mul(cos_vu(angle), radius), scale));
    const uint64_t centre = static_cast<uint64_t>(static_cast<int64_t>(x)) + 0x00fffff080000000ull -
                            static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(y) << 16))) + 0x8000u;
    for (uint32_t i = 0; i < 4; ++i) sd(xyz + i * 8, centre);
    // the corners: the frame's matrix turned by -angle, each row's x (into Y) and y (into X) in sixteenths
    vec_clear(V(angles));
    swc1(angles + 8, -angle);
    euler_to_mat4_vu(M(turn), V(angles));
    mat4_mul(M(turn), M(turn), M(frame_block(0xa0)));
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t row = turn + i * 0x10, c = xyz + i * 8;
        const int32_t rx = float_to_int(fpu_mul(lwc1(row), scale));
        sd(c, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rx) << 16))) + ld(c));
        const int32_t ry = float_to_int(fpu_mul(lwc1(row + 4), scale));
        sd(c, static_cast<uint64_t>(static_cast<int64_t>(ry)) + ld(c));
    }
    const uint64_t tex0 = effect_tex0(lw(data(0x7c)) + 0x29);
    gs_quad_packet(xyz, uvs, colours, tex0, 1);
    gs_quad_packet(xyz, uvs, colours, tex0, 1);
    ee_stack_free(0xf0);
}

// orig: L2:0x002da3d8 L4:0x002d63e8 L6:0x002e3248 L8:0x002f3938 L11:0x002fe6b0 L13:0x002f6f20 L18:0x002da8d8
void class615_ring_draw(uint64_t tex0, uint32_t colour, float a0, float a1, float w0, float w1) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t xyz = sp, colours = sp + 0x20, uvs = sp + 0x30;
    const float scale = 16.0f;
    const float step = fpu_div(angle_sub(a1, a0), 5.0f);
    for (uint32_t i = 0; i < 4; ++i) sw(uvs + i * 4, lwu(data(0x6c + i * 4)));
    for (uint32_t i = 0; i < 4; ++i) sw(colours + i * 4, colour);
    // a corner at angle `a`, `w` out: X from the sine, Y from the cosine
    auto at = [scale](float a, float w) {
        const int32_t y = float_to_int(fpu_mul(fpu_mul(cos_vu(a), w), scale));
        const int32_t x = float_to_int(fpu_mul(fpu_mul(sin_vu(a), w), scale));
        return corner(DEPTH, static_cast<uint32_t>(x), static_cast<uint32_t>(y) << 16);
    };
    for (int32_t k = 0; k < 5; ++k) {
        const float s = angle_add(a0, fpu_mul(step, static_cast<float>(k)));
        const float e = angle_add(a0, fpu_mul(step, static_cast<float>(k + 1)));
        sd(xyz, at(s, w0));
        sd(xyz + 8, at(s, w1));
        sd(xyz + 0x10, at(e, w0));
        sd(xyz + 0x18, at(e, w1));
        gs_quad_packet(xyz, uvs, colours, tex0, 1);
    }
    ee_stack_free(0x110);
}

// orig: L2:0x002da938 L4:0x002d6948 L6:0x002e37a8 L8:0x002f3e98 L11:0x002fec10 L13:0x002f7480 L18:0x002dae38
void class615_link_draw(uint32_t colour, float angle, float r0, float r1) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t xyz = sp, colours = sp + 0x20, uvs = sp + 0x30;
    // the first quad's texture scrolls with the frame counter (its rate the block's float +0xc0), by whole 512ths
    const int32_t scroll = float_to_int(fpu_mul(static_cast<float>(frame_counter()), lwc1(data(0xc0))));
    const uint32_t shift = static_cast<uint32_t>(scroll % 512) << 16;
    for (uint32_t i = 0; i < 4; ++i) sw(uvs + i * 4, lwu(data(0x4c + i * 4)) + shift);
    for (uint32_t i = 0; i < 4; ++i) sw(colours + i * 4, colour);
    // the quad along the ray, `spread` (cos / sin times the width) to each side; Y from the cosines, X from the sines, as
    // whole pixels (<< 20 / << 4)
    auto quad = [&](auto spread) {
        const float s0 = fpu_mul(sin_vu(angle), r0), c0 = fpu_mul(cos_vu(angle), r0);
        const float s1 = fpu_mul(sin_vu(angle), r1), c1 = fpu_mul(cos_vu(angle), r1);
        const float x0 = fpu_add(s0, spread(cos_vu(angle))), x1 = fpu_sub(s0, spread(cos_vu(angle)));
        const float y0 = fpu_sub(c0, spread(sin_vu(angle))), y1 = fpu_add(c0, spread(sin_vu(angle)));
        const float x2 = fpu_add(s1, spread(cos_vu(angle))), x3 = fpu_sub(s1, spread(cos_vu(angle)));
        const float y2 = fpu_sub(c1, spread(sin_vu(angle))), y3 = fpu_add(c1, spread(sin_vu(angle)));
        auto put = [xyz](uint32_t k, float x, float y) {
            sd(xyz + k * 8, corner(DEPTH, static_cast<uint32_t>(float_to_int(x)) << 4, static_cast<uint32_t>(float_to_int(y)) << 20));
        };
        put(0, x0, y0);
        put(1, x1, y1);
        put(2, x2, y2);
        put(3, x3, y3);
    };
    quad([](float t) { return fpu_add(t, t); });
    const uint64_t tex0 = effect_tex0(lw(data(0xc4)));
    gs_quad_packet(xyz, uvs, colours, tex0, 0);
    for (uint32_t i = 0; i < 4; ++i) sw(uvs + i * 4, lwu(data(0x5c + i * 4)));
    quad([](float t) { return fpu_mul(t, 5.0f); });
    gs_quad_packet(xyz, uvs, colours, tex0, 0);
    ee_stack_free(0x110);
}

// orig: L2:0x002d93e8 L4:0x002d53f8 L6:0x002e2258 L8:0x002f2948 L11:0x002fd6c0 L13:0x002f5f30 L18:0x002d98e8
void class615_puzzle_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t ring0 = pv + 0x20, ring1 = pv + 0x50, ring2 = pv + 0x80, lights = pv + 0xb0;
    const float deg = u2f(0x3c8efa35);        // one degree
    const float tenth = u2f(0x3ea0d97c);      // pi / 10: half a slot's link
    const float twelfth = u2f(0x3e860a92);    // pi / 12: half a slot
    sub_002a0ba8(0x47, 0x5380b);
    const uint64_t tex0 = effect_tex0(0x10);
    class615_frame_draw();
    for (uint32_t i = 0; i < SLOTS; ++i)
        if (lw(lights + i * 4) != 0) sw(lights + i * 4, 1);

    // the active ring's turn in progress, as a slot (+1 / -1 past a tenth of a turn; else 0), for the active ring's links
    int32_t turn0 = 0, turn1 = 0, turn2 = 0;
    {
        const float t = lwc1(pv + 0xe4);
        const int32_t dir = tenth < t ? 1 : (t < -tenth ? -1 : 0);
        if (dir != 0) {
            const int32_t active = lw(pv + 0xe0);
            if (active == 1) turn0 = dir;
            else if (active == 2) turn1 = dir;
            else turn2 = dir;
        }
    }
    const auto at = [](uint32_t ring, int32_t x) { return lw(slot(ring, x)); };
    // a slot's angle on a ring (`which` 1..3: the active ring's turn added)
    const auto slot_angle = [&](int32_t which, int32_t i) {
        float a = -tenth;
        if (lw(pv + 0xe0) == which) a = angle_add(a, lwc1(pv + 0xe4));
        return angle_add(a, fpu_mul(static_cast<float>(i * 30), deg));
    };
    // a link from radius `self` to `to`, and (`back`) its other half
    const auto link = [&](float a, uint32_t colour, float self, float to, bool back) {
        class615_link_draw(colour, angle_add(a, tenth), self, to);
        if (back) class615_link_draw(lwu(data(0x114)), angle_add(a, tenth), to, self);
    };
    // a link that reaches no ring: toward the light across, which it lights (the slot becomes a new link)
    const auto to_light = [&](uint32_t cell, int32_t was, int32_t x, uint32_t& colour) {
        const uint32_t light = slot(lights, x);
        if (lw(light) != 0) {
            sw(cell, NEW_LINK);
            sw(light, static_cast<uint32_t>(was));
            colour = lwu(data(0x110));
        }
        return neg_word_f(data(0xbc));
    };

    // the inner ring's links: to the middle ring, the outer ring, across them, across its own, or out to a light
    for (int32_t i = 0; i < SLOTS; ++i) {
        const float a = slot_angle(1, i);
        const int32_t self = i + lw(pv + 0x14);
        const uint32_t cell = slot(ring0, self);
        const int32_t was = lw(cell);
        if (was != static_cast<int32_t>(LINK)) continue;
        uint32_t colour = lwu(data(0x10c));
        bool back = true;
        float to;
        const int32_t x1 = i + lw(pv + 0x18) + turn0 - turn1;
        if (at(ring1, x1) != 0) {
            to = word_f(data(0xa4));
        } else {
            const int32_t x2 = i + lw(pv + 0x1c) + turn0 - turn2;
            if (at(ring2, x2) != 0) to = word_f(data(0x98));
            else if (at(ring2, x2 + 6) != 0) to = neg_word_f(data(0x98));
            else if (at(ring1, x1 + 6) != 0) to = neg_word_f(data(0xa4));
            else if (at(ring0, self + 6) != 0) to = neg_word_f(data(0xb0));
            else { back = false; to = to_light(cell, was, i + turn0 + 6, colour); }
        }
        link(a, colour, word_f(data(0xb0)), to, back);
    }
    // the middle ring's: to the outer ring, across it, across its own, across the inner one, or out to a light
    for (int32_t i = 0; i < SLOTS; ++i) {
        const float a = slot_angle(2, i);
        const int32_t self = i + lw(pv + 0x18);
        const uint32_t cell = slot(ring1, self);
        const int32_t was = lw(cell);
        if (was != static_cast<int32_t>(LINK)) continue;
        uint32_t colour = lwu(data(0x10c));
        bool back = true;
        float to;
        const int32_t x2 = i + lw(pv + 0x1c) + turn1 - turn2;
        if (at(ring2, x2) != 0) to = word_f(data(0x98));
        else if (at(ring2, x2 + 6) != 0) to = neg_word_f(data(0x98));
        else if (at(ring1, self + 6) != 0) to = neg_word_f(data(0xa4));
        else if (at(ring0, i + lw(pv + 0x14) + turn1 - turn0 + 6) != 0) to = neg_word_f(data(0xb0));
        else { back = false; to = to_light(cell, was, i + turn1 + 6, colour); }
        link(a, colour, word_f(data(0xa4)), to, back);
    }
    // the outer ring's: across its own, across the middle and inner ones, or out to a light
    for (int32_t i = 0; i < SLOTS; ++i) {
        const float a = slot_angle(3, i);
        const int32_t self = i + lw(pv + 0x1c);
        const uint32_t cell = slot(ring2, self);
        const int32_t was = lw(cell);
        if (was != static_cast<int32_t>(LINK)) continue;
        uint32_t colour = lwu(data(0x10c));
        bool back = true;
        float to;
        if (at(ring2, self + 6) != 0) to = neg_word_f(data(0x98));
        else if (at(ring1, i + lw(pv + 0x18) + turn2 - turn1 + 6) != 0) to = neg_word_f(data(0xa4));
        else if (at(ring0, i + lw(pv + 0x14) + turn2 - turn0 + 6) != 0) to = neg_word_f(data(0xb0));
        else { back = false; to = to_light(cell, was, i + turn2 + 6, colour); }
        link(a, colour, word_f(data(0x98)), to, back);
    }

    // the rings (the active one glowing at the frame counter's pace) and their slots' markers; a new link's marker drawn once
    // as new, then a link again
    struct Ring {
        uint32_t base, turn, wide, narrow, radius;
    };
    const Ring rings[3] = {{ring0, pv + 0x14, 0xac, 0xa8, 0xb0}, {ring1, pv + 0x18, 0xa0, 0x9c, 0xa4}, {ring2, pv + 0x1c, 0x94, 0x90, 0x98}};
    for (int32_t r = 0; r < 3; ++r) {
        const Ring& ring = rings[r];
        for (int32_t i = 0; i < SLOTS; ++i) {
            float a = -twelfth;
            uint32_t colour;
            if (lw(pv + 0xe0) != r + 1) {
                colour = lwu(data(0x104));
            } else {
                const uint32_t glow = lwu(data(0x100));
                const float phase = fpu_mul(static_cast<float>(ee_rem(frame_counter(), 60) * 6), deg);
                colour = rgba_lerp(glow, glow & 0xffffffu, fpu_add(fpu_mul(sin_vu(phase), 0.5f), 0.5f));
                const float t = lwc1(pv + 0xe4);
                if (!(t == 0.0f)) a = angle_add(a, t);
            }
            a = angle_add(a, fpu_mul(static_cast<float>(i * 30), deg));
            class615_ring_draw(tex0, colour, a, angle_add(a, u2f(0x3f060a92)), word_f(data(ring.wide)), word_f(data(ring.narrow)));
            const int32_t v = at(ring.base, i + lw(ring.turn));
            if (v == 0) continue;
            const uint32_t kind = v >= static_cast<int32_t>(NEW_LINK) ? 0u : (v == static_cast<int32_t>(LINK) ? 1u : 2u);
            class615_marker_draw(kind, angle_add(a, twelfth), word_f(data(ring.radius)));
            if (kind == 0) sw(slot(ring.base, i + lw(ring.turn)), LINK);
        }
    }

    // the twelve lights: lit, or unlit (blinking every 32 frames)
    for (int32_t i = 0; i < SLOTS; ++i) {
        const float a = angle_add(u2f(0xbdb2b8c2), fpu_mul(static_cast<float>(i * 30), deg));
        const float b = angle_add(a, u2f(0x3e32b8c2));
        const int32_t v = lw(lights + static_cast<uint32_t>(i) * 4);
        if (v == 0) continue;
        uint32_t colour;
        if (v == static_cast<int32_t>(LINK)) colour = lwu(data(0x124));
        else colour = (frame_counter() & 0x20) != 0 ? lwu(data(0x120)) : lwu(data(0x11c));
        class615_ring_draw(tex0, colour, a, b, word_f(data(0xb8)), word_f(data(0xb4)));
    }
    sub_002a0ba8(0x47, 0x5360b);
}

// orig: L2:0x002d92f0 L4:0x002d5300 L6:0x002e2160 L8:0x002f2850 L11:0x002fd5c8 L13:0x002f5e38 L18:0x002d97f0
void class615_hero_to_pad(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x40);
    moby_chain_point_v(m, 0, V(sp));
    vec3_sub(V(sp), V(sp), V(HERO_POS));
    vec3_clamp_length(sp, fpu_mul(frame_time(), 4.0f));
    vec3_add(V(HERO_POS), V(HERO_POS), V(sp));
    float turn = angle_sub(angle_add(lwc1(m + 0x48), u2f(0x40490fd0)), lwc1(HERO + 0x98));
    const float most = fpu_mul(frame_time(), u2f(0x40c90fdb));
    if (most < turn) turn = most;
    else if (turn < -most) turn = -most;
    swc1(HERO + 0x98, angle_add(turn, lwc1(HERO + 0x98)));
    ee_stack_free(0x40);
}

// orig: L2:0x002d8910 L4:0x002d4920 L6:0x002e1780 L8:0x002f1e70 L11:0x002fcbe8 L13:0x002f5458 L18:0x002d8e10
void class615_camera_goal(uint32_t pos, uint32_t rot, uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const int32_t record = lw(lwu(m + 0x78) + 0xe8);
    if (record != -1) {
        const uint32_t r = (static_cast<uint32_t>(record) << 7) + lwu(OV(0x001600ec));
        lq_sq(pos, r + 0x30);
        lq_sq(rot, r + 0x70);
        ee_stack_free(0x80);
        return;
    }
    // the side of the pad (its yaw +/- a quarter turn) nearer the camera's goal
    const float left = angle_add(lwc1(m + 0x48), u2f(0x3fc90fdb));
    const float right = angle_add(lwc1(m + 0x48), u2f(0xbfc90fdb));
    const float toward = atan2_f(fpu_sub(lwc1(OV(0x00166dc0)), lwc1(m + 0x10)), fpu_sub(lwc1(OV(0x00166dc4)), lwc1(m + 0x14)));
    const float side = angle_dist(toward, left) < angle_dist(toward, right) ? left : right;
    vec3_add(V(sp), V(HERO_POS), V(m + 0x10));
    vec3_scale(V(sp), V(sp), 0.5f);
    lq_sq(pos, sp);
    swc1(pos, fpu_add(lwc1(pos), fpu_add(cos_vu(side), cos_vu(side))));
    swc1(pos + 4, fpu_add(lwc1(pos + 4), fpu_add(sin_vu(side), sin_vu(side))));
    swc1(pos + 8, fpu_add(lwc1(pos + 8), 1.0f));
    sw(rot, 0);
    sw(rot + 4, 0);
    swc1(rot + 8, angle_add(side, u2f(0x40490fdb)));
    ee_stack_free(0x80);
}

// orig: L2:0x002d8ad0 L4:0x002d4ae0 L6:0x002e1940 L8:0x002f2030 L11:0x002fcda8 L13:0x002f5618 L18:0x002d8fd0
void class615_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t goal_pos = sp, goal_rot = sp + 0x10;
    const float two_pi = u2f(0x40c90fdb);
    switch (lbu(m + 0x20)) {
        case 0: {  // set up: the frame's matrix, the texture base; a pad solved before is done
            sh(pv + 0x12, 0);
            sb(m + 0x20, 1);
            const float r = 17.0f;
            swc1(frame_block(0xa0), r);
            swc1(frame_block(0xa4), r);
            swc1(frame_block(0xb0), r);
            swc1(frame_block(0xb4), -r);
            swc1(frame_block(0xc0), -r);
            swc1(frame_block(0xc4), r);
            swc1(frame_block(0xd0), -r);
            swc1(frame_block(0xd4), -r);
            sw(data(0x7c), lwu(pv + 0xec));
            if (lbu(m + 0xb0) != 0xff && solved_before(m)) sb(m + 0x20, 4);
            break;
        }
        case 1: {  // waiting for the hero on the pad with the Trespasser in hand (its colour says which)
            if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30e) != 0) {
                sh(pv + 0x12, 0);
                sw(m + 0x90, lwu(data(0x08)));
                break;
            }
            if (lh(pv + 0x12) == 0) {
                sh(pv + 0x12, 1);
                moby_sound_start(3, 0, m);
            }
            const bool ready = lwu(HERO + 0x1090) != 0 && lwu(HERO + 0x10b8) == TRESPASSER_ITEM;
            sw(m + 0x90, ready ? lwu(data(0x00)) : lwu(data(0x04)));
            if (!ready || lbu(lwu(HERO + 0x1090) + 0xbc) == 0) break;
            const int32_t t = frame_scale_i(300);
            sh(pv + 0x11c, 0);
            sh(pv + 0x11e, 0);
            sh(pv + 0x10, static_cast<uint32_t>(t));
            hero_set_state(PAD_STATE, 0);
            hero_anim_set(0x57, 0, static_cast<float>(frame_scale_i(6)));
            camera_scripted_set(OV(0x00166dc0), OV(0x00166dd0), 1, 0, 0);
            sb(m + 0x20, 2);
            sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(60)));
            sw(pv + 0x110, 0);
            lq_sq(pv + 0xf0, OV(0x00166dc0));
            lq_sq(pv + 0x100, OV(0x00166dd0));
            break;
        }
        case 2: {  // the puzzle
            class615_hero_to_pad(m);
            class615_camera_goal(goal_pos, goal_rot, m);
            const float accel = fpu_add(lwc1(0x0015ed70), lwc1(0x0015ed70));
            seek_1d(pv + 0x110, pv + 0x114, 1.0f, accel, accel, fpu_add(frame_time(), frame_time()));
            const float blend = lwc1(pv + 0x110);
            vec3_lerp(V(goal_pos), V(pv + 0xf0), V(goal_pos), blend);
            swc1(goal_rot + 8, angle_add(fpu_mul(angle_sub(lwc1(goal_rot + 8), lwc1(pv + 0x108)), lwc1(pv + 0x110)), lwc1(pv + 0x108)));
            swc1(goal_rot + 4, angle_add(fpu_mul(angle_sub(lwc1(goal_rot + 4), lwc1(pv + 0x104)), lwc1(pv + 0x110)), lwc1(pv + 0x104)));
            uid5_set_pvar_80(goal_pos);
            uid5_set_pvar_90(goal_rot);
            sw(OV(0x0017e508), 1);
            hud_message_force(6, 0);
            const uint32_t cam = lwu(OV(0x00166e00));
            if (cam != 0 && lh(cam + 0x86) != 5) camera_scripted_set(OV(0x00166dc0), OV(0x00166dd0), 1, 0, 0);
            // the first unlit light (12: all lit)
            int32_t unlit = 0;
            bool playing = false;
            if (!(0.99f <= lwc1(pv + 0x110)) ||
                !(angle_dist(lwc1(OV(0x00166c80) + 0x158), lwc1(goal_rot + 8)) < u2f(0x3c8efa35))) {
                // the camera not there yet: the hero's look-around animation
                if ((lwu(0x0013fde8) & 2) != 0) hero_anim_set(0x58, 0, static_cast<float>(frame_scale_i(6)));
            } else {
                playing = true;
                sub_001f93e0(puzzle_draw_fn(), m);
                while (unlit < SLOTS && lw(pv + 0xb0 + static_cast<uint32_t>(unlit) * 4) != 1) ++unlit;
            }
            if (playing) {
                const float t = lwc1(pv + 0xe4);
                // a ring's turn: its turn word one slot on (+11 or +13, modulo 12)
                const auto step_ring = [pv](int32_t by) {
                    const int32_t active = lw(pv + 0xe0);
                    const uint32_t w = active == 1 ? pv + 0x14 : (active == 2 ? pv + 0x18 : pv + 0x1c);
                    const int32_t v = lw(w) + by;
                    sw(w, static_cast<uint32_t>(ee_div(v, SLOTS)));
                    sw(w, static_cast<uint32_t>(ee_rem(v, SLOTS)));
                };
                if (0.0f < t) {
                    const float a = angle_add(t, fpu_mul(frame_time(), two_pi));
                    swc1(pv + 0xe4, a);
                    if (u2f(0x3f060a92) < a) {
                        swc1(pv + 0xe4, 0.0f);
                        step_ring(11);
                    }
                } else if (t < 0.0f) {
                    const float a = angle_add(t, -fpu_mul(frame_time(), two_pi));
                    swc1(pv + 0xe4, a);
                    if (a < u2f(0xbf060a92)) {
                        swc1(pv + 0xe4, 0.0f);
                        step_ring(13);
                    }
                } else if (unlit == SLOTS) {
                    // solved: the timer runs out to the next state
                    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) sb(m + 0x20, 3);
                } else if ((ld(PAD + 0x1c0) & 0x0000a00000000000ull) != 0) {
                    // the stick turns the active ring
                    sh(pv + 0x11e, lhu(pv + 0x11e) | 1);
                    moby_sound_start(0, 0, m);
                    const float speed = frame_time();
                    if ((lwu(PAD + 0x1c4) & 0x8000) != 0) swc1(pv + 0xe4, -fpu_mul(speed, two_pi));
                    else swc1(pv + 0xe4, fpu_mul(speed, two_pi));
                } else {
                    // up / down picks the ring
                    const uint32_t pressed = lwu(PAD + 0x1a4);
                    bool moved = false;
                    if ((pressed & 0x1000) != 0) {
                        sh(pv + 0x11e, lhu(pv + 0x11e) | 2);
                        const int32_t r = lw(pv + 0xe0) - 1;
                        sw(pv + 0xe0, static_cast<uint32_t>(r));
                        if (r == 0) sw(pv + 0xe0, 1);
                        else moved = true;
                    } else if ((pressed & 0x4000) != 0) {
                        sh(pv + 0x11e, lhu(pv + 0x11e) | 2);
                        const int32_t r = lw(pv + 0xe0) + 1;
                        sw(pv + 0xe0, static_cast<uint32_t>(r));
                        if (r == 4) sw(pv + 0xe0, 3);
                        else moved = true;
                    }
                    if (moved) moby_sound_start(1, 0, m);
                }
            }
            if (unlit == SLOTS) break;
            // leaving: triangle (unless the save state forbids it), or the hero out of his pad state
            bool leave = false;
            if ((lwu(0x0013cae4) & 0x10) != 0 && lwu(OV(0x00179410)) == 0 && lw(OV(0x00179434)) == -1) leave = true;
            if (!leave) leave = lwu(0x001413d4) != PAD_STATE || lwu(HERO + 0x10b8) != TRESPASSER_ITEM;
            if (!leave) break;
            sb(m + 0x20, 1);
            hydro_2eac18(1);
            hero_idle();
            sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(60)));
            break;
        }
        case 3:  // solved: recorded once the hero's animation allows, the hero back
            if ((lwu(0x0013fde8) & 2) == 0) break;
            if (lbu(m + 0xb0) != 0xff) record_solved(m);
            hydro_2eac18(1);
            hero_set_state(0, 1);
            sb(m + 0xbc, 1);
            sb(m + 0x20, 4);
            break;
        default:
            break;
    }
    color_pulse_update(m, pv);
    ee_stack_free(0x90);
}

}  // namespace rac1
