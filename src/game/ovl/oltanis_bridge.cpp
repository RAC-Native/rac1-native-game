// Oltanis (level 14), classes 1395 and 1416. Ported by hand from Oltanis's own code (L14:0x305d28, 0x306158, 0x306b78),
// the original's order of calls and float operations kept.
//
// Class 1395, a bridge of six pieces under a lid: set up (state 0), it spawns its lid (class 1396, +0 of its pvars) and
// lays its six pieces (the level's mobys named at +0x28 .. +0x3c, kept at +4 .. +0x18) along its +0xc0 axis, each a step
// lower (the level's step and start), from its place. Then it waits (1) for the moby named at +0x1c to reach its state 2;
// then (2) the pieces tip down one after the other from the last (+0x22 counts them, +0x20 / +0x24 the timer and its
// inverse, each with its sound), and (3) the lid turns down from the level's angle to 0 with its sound, the pieces laid
// again along the lid's own +0xe0 axis from its place; at the end its sound stops and the pieces are given their state 2.
//
// Class 1416, a pad that glows: set up (0) it sinks 0.35; done before (its uid's byte or save bit, or with a checkpoint
// slot (+8 not 0) whose slot is 0xff) it is lit (2) at once. Waiting (1) it pulses its colour; the hero stepping on it (his
// moby under him, 0x13f64c, and the halfword 0x13f65e clear) sets its bits (the save's and the level's), lights it with
// its sound. Lit, it opens the way: the points of the path named at its pvars +0 whose w lane equals its +4 are given 0.
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
struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }

// a moby's bit by its uid in a table of words (the save's bits of this level, the level's own)
void set_uid_bit(uint32_t table, uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const uint32_t w = table + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(uid)) >> 5) << 2);
    sw(w, lwu(w) | (1u << (uid & 0x1f)));
}
// a moby done before: its uid's byte of the level, or its save bit
bool uid_done(uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const int32_t id = static_cast<int16_t>(uid);
    if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(id) + 0x454) != 0) return true;
    return ((lw(0x0014c190u + (lwu(0x0015ed84u) << 8) + (static_cast<uint32_t>(id >> 5) << 2)) >> (uid & 0x1f)) & 1) != 0;
}

// the moby's sound in slot `at` stopped (while still its own and playing), the slot cleared
void sound_stop(uint32_t m, uint32_t at) {
    const int32_t slot = lw(at);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(at, 0xffffffffu);
}

// the lid (class 1396) at the bridge's place and angles
// orig: L14:0x00306158
uint32_t lid_spawn(uint32_t m) {
    const uint32_t o = moby_spawn(0x574);
    if (o == 0) return 0;
    lq_sq(o + 0x10, m + 0x10);
    lq_sq(o + 0x40, m + 0x40);
    sb(o + 0x30, 0x40);
    sb(o + 0x20, 0);
    sh(o + 0x32, 0x40);
    sb(o + 0x31, 1);
    moby_update_transform(o);
    return o;
}

// piece k's drop below the line (the level's step and start)
float piece_drop(int32_t k) { return fpu_add(fpu_mul(lwc1(L14_ADDR(0x001621f8)), static_cast<float>(k)), lwc1(L14_ADDR(0x001621fc))); }

// the lit pad's way open: the points of its path (pvars +0) whose w lane is its +4 given 0
void open_way(uint32_t pv) {
    if (lw(pv) == -1) return;
    const uint32_t path = lvd::path(lw(pv));
    for (int32_t i = 0; i < lw(path); ++i) {
        const uint32_t w = path + (static_cast<uint32_t>(i) << 4) + 0x1c;
        if (lwc1(w) == lwc1(pv + 4)) sw(w, 0);
    }
}
}  // namespace

// orig: L14:0x00305d28
void olt_class1395_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xa0);
    const uint32_t off = f.sp;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x00305d98) the lid spawned, the six pieces laid along its axis
        if (lw(pv) == 0) {
            sw(pv, lid_spawn(m));
            vec3_scale(V(off), V(m + 0xc0), lwc1(L14_ADDR(0x001621f4)));
            vec3_add(V(lwu(pv) + 0x10), V(m + 0x10), V(off));
        }
        for (int32_t k = 0; k < 6; ++k) {
            const uint32_t slot = pv + 4 + static_cast<uint32_t>(k) * 4;
            if (lw(slot) != 0) continue;
            sw(slot, level_moby(lw(pv + 0x28 + static_cast<uint32_t>(k) * 4)));
            vec3_scale(V(off), V(m + 0xc0), lwc1(L14_ADDR(0x001621f4)));
            vec3_add(V(lwu(slot) + 0x10), V(m + 0x10), V(off));
            const uint32_t o = lwu(slot);
            swc1(o + 0x18, fpu_add(lwc1(o + 0x18), piece_drop(k)));
        }
        sw(pv + 0x40, 0xffffffffu);
        if (lw(pv) != 0) sb(m + 0x20, 1);
        return;
    }
    case 1: {  // (0x00305e84) waiting for the moby it watches to reach its state 2; then the last piece tips first
        const uint32_t watched = lbu(level_moby(lw(pv + 0x1c)) + 0x20);
        if (watched != 2) return;
        sb(m + 0x20, watched);
        sh(pv + 0x22, 5);
        const int32_t len = frame_scale_i(lw(L14_ADDR(0x00162204)));
        sh(pv + 0x20, static_cast<uint32_t>(len));
        swc1(pv + 0x24, fpu_div(1.0f, int_to_float(static_cast<int16_t>(len))));
        moby_sound_play(0, 0, lwu(pv + 4 + (static_cast<uint32_t>(lh(pv + 0x22)) << 2)), 0x575);
        return;
    }
    case 2: {  // (0x00305f04) a piece tipping down (from upright to flat), then the next; after the first the lid
        const uint32_t piece = lwu(pv + 4 + (static_cast<uint32_t>(lh(pv + 0x22)) << 2));
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x20)));
        swc1(piece + 0x44, angle_lerp(u2f(0x3fc90fdbu), 0.0f, fpu_mul(int_to_float(lh(pv + 0x20)), lwc1(pv + 0x24))));  // (pi / 2)
        if (lh(pv + 0x20) != 0) return;
        sh(pv + 0x20, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00162204)))));
        const int16_t next = static_cast<int16_t>(lhu(pv + 0x22) - 1);
        sh(pv + 0x22, static_cast<uint16_t>(next));
        if (next >= 0) {
            moby_sound_play(0, 0, lwu(pv + 4 + (static_cast<uint32_t>(next) << 2)), 0x575);
            return;
        }
        sb(m + 0x20, 3);
        const int32_t len = frame_scale_i(lw(L14_ADDR(0x0016220c)));
        sh(pv + 0x20, static_cast<uint32_t>(len));
        swc1(pv + 0x24, fpu_div(1.0f, int_to_float(static_cast<int16_t>(len))));
        return;
    }
    case 3: {  // (0x00305fd4) the lid turning down with its sound, the pieces laid along it; at the end they are let go
        const uint32_t lid = lwu(pv);
        if (sub_0028d8c0(m, lw(pv + 0x40)) == 0) sw(pv + 0x40, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x20)));
        swc1(lid + 0x44, lerp_f(fpu_mul(lwc1(L14_ADDR(0x00162200)), u2f(0x3c8efa35u)), 0.0f,  // (degrees)
                                fpu_mul(int_to_float(lh(pv + 0x20)), lwc1(pv + 0x24))));
        for (int32_t k = 0; k < 6; ++k) {
            vec3_scale(V(off), V(lid + 0xe0), piece_drop(k));
            vec3_add(V(lwu(pv + 4 + static_cast<uint32_t>(k) * 4) + 0x10), V(lid + 0x10), V(off));
        }
        if (lh(pv + 0x20) != 0) return;
        sound_stop(m, pv + 0x40);
        sb(m + 0x20, 4);
        for (uint32_t k = 0; k < 6; ++k) sb(lwu(pv + 4 + k * 4) + 0x20, 2);
        return;
    }
    default:
        return;
    }
}

// orig: L14:0x00306b78
void olt_class1416_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x00306bac) sunk 0.35; done before: lit at once
        swc1(pv + 0xc, rand_angle());
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 0.35f));
        const bool done = uid_done(m) ||
                          (lw(pv + 8) != 0 && lbu(0x0014c050u + (lwu(0x0015ed84u) << 4) + lbu(m + 0xb0)) == 0xff);
        if (!done) {
            sb(m + 0x20, 1);
            return;
        }
        sb(m + 0x20, 2);
        sw(m + 0x90, 0x80208020u);
        sb(m + 0xbc, 2);
        open_way(pv);
        return;
    }
    case 1: {  // (0x00306d00) pulsing (32 .. 128 grey); the hero stepping on it lights it
        const float phase = angle_add(lwc1(pv + 0xc), fpu_mul(lwc1(0x0015ed6cu), u2f(0x40c90fdbu)));  // (2 pi a second)
        swc1(pv + 0xc, phase);
        int32_t grey = float_to_int(fpu_mul(fpu_sub(fpu_mul(sin_vu(phase), 4.0f), 3.0f), 128.0f));
        if (!(grey < 129)) grey = 0x80;
        else if (!(0x1f < grey)) grey = 0x20;
        const uint32_t g = static_cast<uint32_t>(grey);
        sw(m + 0x90, 0x80000000u | (g << 16) | (g << 8) | g);
        if (lwu(0x0013f64cu) != m) return;  // (the moby under the hero)
        if (lh(0x0013f65eu) != 0) return;
        set_uid_bit(0x0014c190u + (lwu(0x0015ed84u) << 8), m);
        set_uid_bit(L14_ADDR(0x001babd0), m);
        sb(m + 0x20, 2);
        sb(m + 0xbc, 1);
        sw(m + 0x90, 0x80208020u);
        moby_sound_start(0, 0, m);
        open_way(pv);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
