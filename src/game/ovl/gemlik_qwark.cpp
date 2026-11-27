// Gemlik Base's (level 13) class 388: Qwark's ship in the space fight, the leader of the squadron's fighters (class 111,
// gemlik_fighter.cpp). It waits until Ratchet flies his ship, then flies its paths through the fight's stages (its
// +0xbc, 1 .. 7): it fires homing missiles (class 82) and drops mines (class 83; both gemlik_boss_shots.cpp), calls
// its fighters (stage 4), makes runs past the hero (state 5, with a beam drawn from its nose: the draw callback
// 0x2ea8c8 at the end of this file), loses a part at each stage's health, gloats while Ratchet's ship is gone, and at
// the end spins down and crashes. Its lines go to the dialog stream (0x1516ec: 50000 + a line of the table
// L13:0x1d3558). Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base
// ELF's bare. Registered in gemlik_mobys.cpp.
//
// Its pvars: +0x20 its health (+0x24 its full health, a halfword; +0x26 its pulse's time), +0x60 its colour pulse,
// +0x70 its velocity, +0x80 the aim point of a run, +0x9c / +0x9e the path's point it flies to / from, +0xa4 / +0xa8
// its pitch's / yaw's turning speeds, +0xac its speed, +0xb0 its five paths (path numbers), +0xc4 its ten parts and
// +0xec itself (the eleven subjects of its hits), +0xf0 the damage piece (class 0x191), +0xf8 the attack's timer,
// +0xfc the launcher's side, +0xfd a shots' counter, +0xfe missiles (0) or mines (1), +0xff a run's turn, +0x100 a
// run's timer, +0x104 the HUD's health bar (element 0x11; -1 none) and +0x108 its value, +0x10c its direction along
// the path (+1 / -1), +0x10e the squadron's fighters alive, +0x10f the ones attacking this frame, +0x110 its glow
// (class 0x160), +0x114 / +0x118 / +0x11c its engine's, glow's and run's sounds (sound slots, -1 none), +0x120 its
// light (-1 none), +0x124 the attack's cooldown, +0x12c a camera entry, +0x130 a box, +0x134 the moby index of
// Ratchet's ship model, +0x138 the lines said (flags; a count while it gloats), +0x13a a line queued, +0x13b its path
// slot, +0x13c the lines said (a bit each), +0x144 / +0x146 two timers that hold lines off, +0x148 a line held.
// States (the table L13:0x1f4f60): 0 set up, 1 wait for Ratchet's ship, 2 approach, 3 / 4 fight, 5 a run past the
// hero, 6 gloat, 7 dying, 8 crash, 9 reset.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (+0x80)
constexpr uint32_t SHIP = 0x00140940;      // the ship the hero flies (+0x15f0: a moby or 0)

// the level's moby of index `idx`
uint32_t moby_at(int32_t idx) { return lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
// point `i` of the path `path` (+0 its count, its points from +0x10)
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }
// the path of slot `slot` of its five
uint32_t slot_path(uint32_t pv, int32_t slot) { return lvd::path(lw(pv + 0xb0 + (static_cast<uint32_t>(slot) << 2))); }
// the sound slot `slot` plays for `m`
bool slot_playing_for(int32_t slot, uint32_t m) {
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    return lwu(e + 0x88) == m && lbu(e + 0x74) != 0;
}
// the sound of the slot word `at` stopped when it still plays for `m`; the word -1
void sound_stop(uint32_t at, uint32_t m) {
    const int32_t s = lw(at);
    if (s == -1) return;
    if (slot_playing_for(s, m)) sub_0028d918(s);
    sw(at, 0xffffffffu);
}
// the sound `idx` kept playing for `m` in the slot word `at` (started again when it stopped)
void sound_keep(uint32_t at, int32_t idx, uint32_t m) {
    const int32_t s = lw(at);
    if (s != -1 && slot_playing_for(s, m)) return;
    sw(at, 0xffffffffu);
    sw(at, static_cast<uint32_t>(moby_sound_start(idx, 4, m)));
}
// inside the fight's space (22 .. 1001 on each axis)
void keep_in_space(uint32_t m) {
    for (uint32_t k = 0x10; k < 0x1c; k += 4) {
        if (lwc1(m + k) < 22.0f) swc1(m + k, 22.0f);
        if (1001.0f < lwc1(m + k)) swc1(m + k, 1001.0f);
    }
}
// `to` placed and turned as `from` (its place +0x10 aside: its rotation +0x40 and matrix +0xc0 .. +0xef)
void pose_as(uint32_t to, uint32_t from) {
    lq_sq(to + 0x40, from + 0x40);
    lq_sq(to + 0xc0, from + 0xc0);
    lq_sq(to + 0xd0, from + 0xd0);
    lq_sq(to + 0xe0, from + 0xe0);
}
// Ratchet's ship's pvars +0x118, +0x88, +0xec, +0x8c zeroed (the ship the hero flies: no test that he flies one)
void ship_clear(uint32_t ship) {
    const uint32_t spv = lwu(ship + 0x78);
    sw(spv + 0x118, 0);
    sw(spv + 0x88, 0);
    sw(spv + 0xec, 0);
    sw(spv + 0x8c, 0);
}
// a ship of class 0x45 the hero flies given +0xbc = `v`
void ship_bc(uint8_t v) {
    const uint32_t ship = lwu(SHIP);
    if (ship != 0 && lh(ship + 0xa6) == 0x45) sb(ship + 0xbc, v);
}
// the HUD's health bar (element 0x11, drawn by L13:0x245cf8 with +0x108 and the full health) requested for this frame
void health_bar(uint32_t pv) {
    const uint32_t full = (static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x24))) << 4) | 0xfu;
    sw(pv + 0x104, static_cast<uint32_t>(camera_request(0x11, 0xffff, L13_ADDR(0x00241868), L13_ADDR(0x00241988),
                                                        L13_ADDR(0x00245cf8), pv + 0x108, full)));
}
// onto the path of slot `slot`: the point nearest the ship, then 3 .. 7 on (wrapping), the one before it
void join_path(uint32_t m, uint32_t pv, int32_t slot) {
    sh(pv + 0x9c, static_cast<uint32_t>(path_nearest_l1(m + 0x10, slot_path(pv, slot), 0.0f)));
    const int32_t r = rand_range(3, 7);
    sh(pv + 0x9c, lhu(pv + 0x9c) + static_cast<uint32_t>(r));
    const uint32_t path = slot_path(pv, slot);
    const int32_t at = ee_rem(lh(pv + 0x9c), lw(path));
    sh(pv + 0x9c, static_cast<uint32_t>(at));
    sh(pv + 0x9e, static_cast<uint32_t>(at - 1));
    if (static_cast<int16_t>(at - 1) < 0) sh(pv + 0x9e, lhu(path) - 1u);
}
// the new gloat line's index L13:0x161950 after a line said: one on (0 .. 3)
void gloat_next() {
    const int32_t v = lw(L13_ADDR(0x00161950)) + 1;
    sw(L13_ADDR(0x00161950), static_cast<uint32_t>(v % 4));
}
}  // namespace

// The glow (class 0x160) spawned over `owner` (its +0x10, its pvars +0 the owner), at its class's scale. 0 when none.
// orig: L13:0x002e8438
uint32_t gml_2e8438_l13(uint32_t owner) {
    const uint32_t g = moby_spawn(0x160);
    if (g == 0) return 0;
    const uint32_t gpv = lwu(g + 0x78);
    sh(g + 0x32, 0x3ff);
    sb(g + 0x30, 0xff);
    sb(g + 0x31, 1);
    sb(g + 0x20, 0);
    sb(g + 0xbc, 0);
    lq_sq(g + 0x10, owner + 0x10);
    sw(gpv + 0, owner);
    sw(gpv + 4, 0);
    sh(g + 0x34, lhu(g + 0x34) | 0x200u);
    moby_update_transform(g);
    moby_copy_hero_color(g);
    swc1(g + 0x2c, lwc1(lwu(g + 0x24) + 0x24));
    return g;
}

// A line of Qwark's: of the `n` lines from `line` in the table L13:0x1d3558 (bytes), one at random that was not said
// yet (a bit of +0x13c each; below line 12 a said line is forgotten again in one case of four), queued for the dialog
// stream (0x1516ec: 50000 + it) with +0x13a set. Lines below 12 wait for +0x148 clear and +0x144 out, the others for
// +0x146 out (23 .. 26 also for +0x148 clear); lines below 29 for the stream idle (0x151720 0, 0x1516ec -1), the
// others are queued at once. 1 when a line was queued.
// orig: L13:0x002e86f8
int32_t gml_2e86f8_l13(uint32_t /*m*/, uint32_t pv, int32_t line, int32_t n) {
    int32_t k = ee_rem(libc_rand(), n);
    uint32_t said = lbu(L13_ADDR(0x001d3558) + static_cast<uint32_t>(line + k));
    if (line < 29) {
        if (line < 12) {
            if (lbu(pv + 0x148) != 0 || lh(pv + 0x144) > 0) return 0;
        } else {
            if (lh(pv + 0x146) > 0) return 0;
            if (lbu(pv + 0x148) != 0 && !(line < 23) && line < 27) return 0;
        }
    }
    const bool idle = lw(0x00151720) == 0 && lw(0x001516ec) == -1;
    if (line < 29) {
        if (!idle) return 0;
        for (int32_t tries = 0; tries < n; ++tries) {
            const uint32_t bit = 1u << (said & 31);
            if ((lwu(pv + 0x13c) & bit) == 0) {
                sw(0x001516ec, said + 0xc350);
                sb(pv + 0x13a, 1);
                sw(pv + 0x13c, lwu(pv + 0x13c) | bit);
                return 1;
            }
            if (line < 12 && rand_mod(4) == 0) sw(pv + 0x13c, lwu(pv + 0x13c) & ~bit);
            k = ee_rem(k + 1, n);
            said = lbu(L13_ADDR(0x001d3558) + static_cast<uint32_t>(line + k));
        }
        if (line < 12) sw(pv + 0x13c, lwu(pv + 0x13c) & ~(1u << (said & 31)));
        return 0;
    }
    sw(0x001516ec, said + 0xc350);
    sb(pv + 0x13a, 1);
    return 1;
}

// The engines' smoke: at six chain points (L13:0x1d3520) of the parts L13:0x1d3508 still on (index 10: the ship
// itself), scaled with the ship: on odd frames a small dark puff, on even ones a bright puff (for the part the stage
// broke off, L13:0x1d3478[stage - 1], in one frame of nine only), then a flame.
// orig: L13:0x002e8920
void gml_2e8920_l13(uint32_t m, uint32_t pv) {
    const Frame f(208);
    const uint32_t sp = f.sp, pos = sp, vel = sp + 0x10;
    for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t part_idx = lwu(L13_ADDR(0x001d3508) + i * 4);
        if (lwu(pv + 0xc4 + (part_idx << 2)) == 0) continue;
        sq_zero(vel);
        const float k = fpu_div(lwc1(m + 0x2c), fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 4.0f));
        moby_chain_point_v(m, lwu(L13_ADDR(0x001d3520) + i * 4), V(pos));
        bool bright = true;
        if ((lwu(L13_ADDR(0x0015f5cc)) & 1) != 0) {
            const int32_t life = frame_scale_i(rand_range(0xf, 0x14));
            spawn_particle_4(pos, vel, 0xcf0000ffu, 0xcf, static_cast<uint32_t>(life),
                             static_cast<uint32_t>(float_to_int(fpu_mul(k, 350.0f))), static_cast<uint32_t>(-100), 1);
            bright = false;
        } else if (lbu(m + 0xbc) != 0 && lwu(L13_ADDR(0x001d3478) + (static_cast<uint32_t>(lbu(m + 0xbc) - 1) << 2)) == part_idx &&
                   rand_mod(9) != 0) {
            bright = false;
        }
        if (bright) {
            const int32_t life = frame_scale_i(rand_range(0xf, 0x16));
            spawn_particle_4(pos, vel, 0x6000ffffu, 0x80, static_cast<uint32_t>(life),
                             static_cast<uint32_t>(float_to_int(fpu_mul(k, 600.0f))), static_cast<uint32_t>(-100), 1);
        }
        const int32_t size = float_to_int(fpu_mul(rand_range_f(100.0f, 250.0f), k));
        const int32_t life = frame_scale_i(rand_range(8, 0xc));
        spawn_particle_4(pos, vel, 0xefff7f4fu, 0xff0000u, static_cast<uint32_t>(life), static_cast<uint32_t>(size),
                         static_cast<uint32_t>(-size), 1);
    }
}

// The damaged ship's sparks: at each chain point of the list L13:0x1d3538 (to its -1), jittered, scaled with the
// ship: on odd frames a spark, on even ones (from stage 1 in one frame of twelve only) a bigger one.
// orig: L13:0x002e8b58
void gml_2e8b58_l13(uint32_t m, uint32_t /*pv*/) {
    const Frame f(128);
    const uint32_t sp = f.sp, pos = sp, vel = sp + 0x10;
    for (uint32_t e = L13_ADDR(0x001d3538); lw(e) != -1; e += 4) {
        sq_zero(vel);
        float k = fpu_div(lwc1(m + 0x2c), fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 4.0f));
        k = fpu_mul(k, rand_range_f(0.8f, 1.1f));
        moby_chain_point_v(m, lwu(e), V(pos));
        metal_25f8e0(pos, fpu_mul(k, 1.4f));
        if ((lwu(L13_ADDR(0x0015f5cc)) & 1) != 0) {
            const int32_t life = frame_scale_i(rand_range(0x14, 0x3c));
            const int32_t size = float_to_int(fpu_mul(rand_range_f(450.0f, 550.0f), k));
            spawn_particle_4(pos, vel, 0xdf000fffu, 0xcf, static_cast<uint32_t>(life), static_cast<uint32_t>(size),
                             static_cast<uint32_t>(-100), 1);
            continue;
        }
        if (lbu(m + 0xbc) != 0 && rand_mod(0xc) != 0) continue;
        const int32_t life = frame_scale_i(rand_range(0x14, 0x32));
        const int32_t size = float_to_int(fpu_mul(rand_range_f(600.0f, 800.0f), k));
        spawn_particle_4(pos, vel, 0x8000ffffu, 0x80, static_cast<uint32_t>(life), static_cast<uint32_t>(size),
                         static_cast<uint32_t>(-100), 1);
    }
}

// A part of class `cls` spawned at the ship's chain point `joint` (its pvars +0x3c the ship, +0x40 its time 600
// frames, +0x44 the joint), at the ship's scale, posed as the ship. 0 when none.
// orig: L13:0x002ecd10
uint32_t gml_2ecd10_l13(uint32_t m, int32_t joint, int32_t cls) {
    const uint32_t p = moby_spawn(static_cast<uint32_t>(cls));
    if (p == 0) return 0;
    const float scale = fpu_div(fpu_mul(lwc1(lwu(p + 0x24) + 0x24), lwc1(m + 0x2c)), lwc1(lwu(m + 0x24) + 0x24));
    const uint32_t ppv = lwu(p + 0x78);
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    sb(p + 0x31, 1);
    sb(p + 0x20, 0);
    swc1(p + 0x2c, scale);
    sw(ppv + 0x44, static_cast<uint32_t>(joint));
    moby_chain_point_v(m, static_cast<uint32_t>(joint), V(p + 0x10));
    sw(ppv + 0x3c, m);
    moby_update_transform(p);
    pose_as(p, m);
    sw(ppv + 0x40, static_cast<uint32_t>(frame_scale_i(0x258)));
    moby_update_transform(p);
    return p;
}

// The parts kept on the ship (each at its chain point, inside the space, posed as the ship); the damage piece of the
// stage (class 0x191 at the chain point L13:0x1d34a8[stage]; none in state 5: retired) spawned again when it went, kept
// on likewise; the glow kept at the ship's place, posed as it.
// orig: L13:0x002e8d28
void gml_2e8d28_l13(uint32_t m, uint32_t pv) {
    for (int32_t i = 0; i < 10; ++i) {
        const uint32_t p = lwu(pv + 0xc4 + (static_cast<uint32_t>(i) << 2));
        if (p == 0) continue;
        moby_chain_point_v(m, static_cast<uint32_t>(i), V(p + 0x10));
        keep_in_space(p);
        pose_as(p, m);
    }
    int32_t joint = lw(L13_ADDR(0x001d34a8) + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2));
    if (lbu(m + 0x20) == 5) joint = -1;
    if (joint == -1) {
        const uint32_t piece = lwu(pv + 0xf0);
        if (piece != 0 && alive(piece)) {
            moby_retire(piece);
            sw(pv + 0xf0, 0);
        }
    } else {
        const uint32_t old = lwu(pv + 0xf0);
        if (old == 0 || !alive(old)) sw(pv + 0xf0, gml_2ecd10_l13(m, joint, 0x191));
        const uint32_t piece = lwu(pv + 0xf0);
        if (piece != 0 && alive(piece)) {
            moby_chain_point_v(m, static_cast<uint32_t>(joint), V(piece + 0x10));
            keep_in_space(piece);
            pose_as(piece, m);
        }
    }
    const uint32_t glow = lwu(pv + 0x110);
    if (glow != 0) {
        lq_sq(glow + 0x10, m + 0x10);
        pose_as(glow, m);
    }
}

// The ship turned toward `yaw` / `pitch` (yaw_turn_damped, angle_seek_rate: half a turn a second at most), rolled
// into its turn (less the more it rolls already, within a quarter turn), the roll eased back by 1.5 % a frame.
// orig: L13:0x002e9018
void gml_2e9018_l13(uint32_t m, uint32_t pv, float yaw, float pitch) {
    const float k2 = u2f(0x40c90fdbu), k1 = u2f(0x40490fdbu);  // (2 pi, pi)
    yaw_turn_damped(m, pv + 0xa8, yaw, fpu_mul(dt_sq(), k1), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k2));
    swc1(m + 0x44, angle_seek_rate(pv + 0xa4, lwc1(m + 0x44), pitch, fpu_mul(dt_sq(), k1), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k2)));
    const float quarter = u2f(0x3fc90fdbu);
    const float room = fpu_div(fpu_sub(quarter, abs_f(lwc1(m + 0x40))), quarter);
    const float roll = fpu_add(lwc1(m + 0x40), fpu_mul(fpu_mul(-lwc1(pv + 0xa8), room), 0.5f));
    swc1(m + 0x40, quarter < roll ? quarter : roll);
    if (lwc1(m + 0x40) < -quarter) swc1(m + 0x40, -quarter);
    swc1(m + 0x40, fpu_mul(lwc1(m + 0x40), u2f(0x3f7c28f6u)));  // (0.985)
    (void)angle_wrap_mod(lwc1(m + 0x40));  // (the original returns it; no caller reads it)
}

// The ship flown one frame along the path of slot `slot` from its point +0x9e toward +0x9c (the next one, in its
// direction +0x10c, once it is within its speed of it, wrapping at the path's ends), its velocity turned toward the
// point the more the nearer it is, and, `turn` set, turned along its way (toward the hero in state 5). 1 when the
// point wrapped.
// orig: L13:0x002e9160
int32_t gml_2e9160_l13(uint32_t m, uint32_t pv, int32_t slot, bool turn) {
    const Frame f(160);
    const uint32_t sp = f.sp;
    const uint32_t path = slot_path(pv, slot);
    const float done = vec3_distance(V(path_point(path, lh(pv + 0x9e))), V(m + 0x10));
    const float seg = vec3_distance(V(path_point(path, lh(pv + 0x9e))), V(path_point(path, lh(pv + 0x9c))));
    const float left = fpu_sub(seg, done);
    int32_t wrapped = 0;
    float k;
    if (0.0f <= left && left < lwc1(pv + 0xac)) {
        const uint32_t to = lhu(pv + 0x9c);
        sh(pv + 0x9e, to);
        sh(pv + 0x9c, to + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(pv + 0x10c)))));
        if (!(lh(pv + 0x9c) < lw(path))) {
            sh(pv + 0x9c, 0);
            wrapped = 1;
        }
        if (lh(pv + 0x9c) < 0) {
            sh(pv + 0x9c, lhu(path) - 1u);
            wrapped = 1;
        }
        k = 0.0f;
    } else {
        k = fpu_div(done, seg);
    }
    if (k < 0.05f) k = 0.05f;
    if (1.0f < k) k = 1.0f;
    vec3_scale(V(pv + 0x70), V(pv + 0x70), fpu_sub(1.0f, k));
    vec3_sub(V(sp), V(path_point(path, lh(pv + 0x9c))), V(m + 0x10));
    vec3_set_length(V(sp), V(sp), lwc1(pv + 0xac));
    vec3_scale(V(sp), V(sp), k);
    vec3_add(V(pv + 0x70), V(pv + 0x70), V(sp));
    vec3_set_length(V(pv + 0x70), V(pv + 0x70), lwc1(pv + 0xac));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x70));
    float yaw = atan2_f(lwc1(sp), lwc1(sp + 4));
    float pitch = -atan2_f(vec2_length(V(sp)), lwc1(sp + 8));
    if (lbu(m + 0x20) == 5) {
        yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
        const float d = vec2_distance(V(m + 0x10), V(HERO_POS));
        pitch = -atan2_f(d, fpu_sub(lwc1(HERO_POS + 8), lwc1(m + 0x18)));
    }
    if (turn) gml_2e9018_l13(m, pv, yaw, pitch);
    return wrapped;
}

// The path flight: the way that leads away from the hero (the next point or the one before it, whichever is further
// from him; a tie keeps the way +1 or turns -1 to +1's... see below); flying the other way it slows by 10 % a frame and
// turns round below 0.15 a frame; flying that way its speed is eased toward 30 (90 in stage 4) over the hero's
// distance. Then one frame along (0x2e9160). Its result, with 2 set when the point reached is a marker (its w above
// 0); 0 without a path.
// orig: L13:0x002e9408
int32_t gml_2e9408_l13(uint32_t m, uint32_t pv, int32_t slot, bool turn) {
    const Frame f(176);
    const uint32_t sp = f.sp;
    const uint32_t path = slot_path(pv, slot);
    int32_t r = 0;
    if (path != 0) {
        int32_t next = lh(pv + 0x9c) + 1;
        if (!(next < lw(path))) next = 0;
        lq_sq(sp, path_point(path, next));
        const float dn = vec3_distance(V(sp), V(HERO_POS));
        int32_t prev = lh(pv + 0x9c) - 1;
        if (prev < 0) prev = lw(path) - 1;
        lq_sq(sp + 0x10, path_point(path, prev));
        const float dp = vec3_distance(V(sp + 0x10), V(HERO_POS));
        const int32_t dir = static_cast<int8_t>(lbu(pv + 0x10c));
        int32_t want;
        if (dn < dp) want = -1;
        else if (dp < dn) want = 1;
        else want = dir == 1 ? 1 : -1;
        if (want != dir) {
            const float slower = fpu_mul(lwc1(pv + 0xac), fpu_add(fpu_mul(step60(), u2f(0xbdccccd0u)), 1.0f));  // (-0.1)
            swc1(pv + 0xac, slower);
            if (slower < fpu_mul(step60(), 0.15f)) {  // turned round
                const uint32_t to = lhu(pv + 0x9c);
                sb(pv + 0x10c, static_cast<uint32_t>(want));
                sh(pv + 0x9c, lhu(pv + 0x9e));
                sh(pv + 0x9e, to);
            }
        } else {
            const float d = vec3_distance(V(m + 0x10), V(HERO_POS));
            const float k = lbu(m + 0xbc) == 4 ? 3.0f : 1.0f;
            const float speed = lwc1(pv + 0xac);
            const float eased = fpu_add(speed, fpu_mul(fpu_sub(fpu_div(fpu_mul(k, 30.0f), d), speed), fpu_mul(step60(), 0.12f)));
            swc1(pv + 0xac, eased < 0.0f ? 0.0f : eased);
        }
        r = gml_2e9160_l13(m, pv, slot, turn);
    }
    if (0.0f < lwc1(path + (static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x9c))) << 4) + 0x1c)) r |= 2;
    return r;
}

// Part `idx` breaks off (when still on): a burst at it, sent off with 95 % of the ship's velocity (jittered), tumbling
// (its state 3, its spin +0x30 .. +0x38), its colour 0x404040, its hit cleared; the slot emptied.
// orig: L13:0x002e9680
void gml_2e9680_l13(uint32_t m, uint32_t pv, int32_t idx) {
    const uint32_t slot = pv + 0xc4 + (static_cast<uint32_t>(idx) << 2);
    const uint32_t p = lwu(slot);
    if (p == 0) return;
    explosion_burst(m, p + 0x10, -1, 2.0f, 13.0f);
    const uint32_t ppv = lwu(p + 0x78);
    const float half = u2f(0x40490fdbu);  // (pi)
    vec3_scale(V(ppv + 0x10), V(pv + 0x70), 0.95f);
    metal_25f8e0(ppv + 0x10, fpu_mul(dt(), 30.0f));
    sb(p + 0x20, 3);
    vec3_add(V(p + 0x10), V(p + 0x10), V(ppv + 0x10));
    const float a = fpu_mul(dt(), u2f(0x3eb2b8c2u));  // (20 degrees)
    swc1(ppv + 0x30, rand_range_f(-a, a));
    const float b = fpu_mul(dt(), half);
    swc1(ppv + 0x34, rand_range_f(-b, b));
    const float c = fpu_mul(dt(), half);
    swc1(ppv + 0x38, rand_range_f(-c, c));
    sd(p + 0x38, 0x0040404000000000ull);
    sb(p + 0xa4, 0xff);
    sw(slot, 0);
}

// The hit that takes the ship's last health: bolts dropped, not drawn hidden, its pulse, onto slot 4's path (its first
// point, facing the second), 80 a second, Ratchet's ship model (moby +0x134) shown again, the camera entry +0x12c
// marked, state 7 (dying) for 600 frames, line 0x22, the HUD's bar freed, every line to say again.
// orig: L13:0x002e97e0
void gml_2e97e0_l13(uint32_t m, uint32_t pv, uint32_t hs) {
    sw(hs, 0);
    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
    moby_drop_bolts(m, 0, -1);
    sb(pv + 0x67, 0x78);
    color_pulse_start(m, pv + 0x60);
    sb(pv + 0x13b, 4);
    sh(pv + 0x9c, 0);
    vec3_set_length(V(pv + 0x70), V(lvd::box(lvd::BOXES, lw(pv + 0x130))), fpu_mul(dt(), 10.0f));
    const uint32_t path = slot_path(pv, lbu(pv + 0x13b));
    const int32_t i = lh(pv + 0x9c);
    lq_sq(m + 0x10, path_point(path, i));
    const uint32_t p0 = path_point(path, i), p1 = path_point(path, i + 1);
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(p1), lwc1(p0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4))));
    const float d = vec2_distance(V(p0), V(p1));
    swc1(m + 0x44, -atan2_f(d, fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8))));
    sb(pv + 0x10c, 1);
    const uint32_t model = moby_at(lw(pv + 0x134));
    sh(model + 0x34, lhu(model + 0x34) | 2u);
    sh(model + 0x34, lhu(model + 0x34) | 1u);
    sb(model + 0x31, 0);
    cam_entry_mark(lw(pv + 0x12c));
    sb(m + 0x20, 7);
    swc1(pv + 0xac, fpu_mul(dt(), 80.0f));
    sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x258)));
    gml_2e86f8_l13(m, pv, 0x22, 1);
    sh(pv + 0x138, 0);
    sb(pv + 0x148, 0);
    if (lw(pv + 0x104) != -1) {
        sub_002359f8(lwu(pv + 0x104), 0);
        sw(pv + 0x104, 0xffffffffu);
    }
    sw(pv + 0x13c, 0xffffffffu);
}

// The ship's hits (before state 6): on the damage piece first, then on its parts and itself (the eleven words from
// +0xc4), the first that takes damage (none from its own missiles / mines nor an asteroid): the health taken (more in
// stage 7) and its pulse; the last health -> 0x2e97e0 (dying: the next stage); a stage's health passed: its part off,
// the next stage (state 5 back to 4), its run's sound stopped; at stage 7 every remaining part off. Below a stage's
// health plus 0.3 of a stage, +0x148 (a line held). A new stage: its line (2: 0xc of 4, 3: 0x10 with the run's timer,
// 4: 0x11, 6: 0x13, 7: 0x16); hit in stage 7 with +0x148: lines 0x14 (two of them, once each). The pulse every frame.
// orig: L13:0x002e9a48
void gml_2e9a48_l13(uint32_t m, uint32_t pv) {
    const Frame f(208);
    const uint32_t sp = f.sp;
    if (lbu(m + 0x20) < 6) {
        const uint32_t hs = pv + 0x20;
        const float per_stage = fpu_div(static_cast<float>(lh(pv + 0x24)), 6.0f);
        const int32_t bc = lbu(m + 0xbc);
        sw(sp + 4, 0);
        const float stage_hp = bc < 5 ? fpu_mul(fpu_sub(6.0f, int_to_float(bc)), per_stage)
                                      : fpu_mul(fpu_sub(6.0f, int_to_float(bc - 1)), per_stage);
        const float warn_hp = fpu_add(stage_hp, fpu_mul(per_stage, 0.3f));
        int32_t taken = 0;  // 1 damaged, 2 a stage passed, 3 the last health
        // a stage's part off; `sound_first`: the damage piece's order (the sound before the part, and only with one)
        auto stage_passed = [&](bool sound_first) {
            const int32_t part = lw(L13_ADDR(0x001d3478) + (static_cast<uint32_t>(lbu(m + 0xbc) - 1) << 2));
            if (sound_first) {
                if (part != -1) {
                    moby_sound_start(7, 0, m);
                    gml_2e9680_l13(m, pv, part);
                }
            } else {
                if (part != -1) gml_2e9680_l13(m, pv, part);
                moby_sound_start(7, 0, m);
            }
            if (lbu(m + 0x20) == 5) sb(m + 0x20, 4);
            sb(m + 0xbc, lbu(m + 0xbc) + 1);
            sound_stop(pv + 0x11c, m);
            sw(sp + 4, 1);
            ship_clear(lwu(SHIP));
            sh(pv + 0x138, 0);
            sb(pv + 0x148, 0);
            taken = 2;
            if (lbu(m + 0xbc) == 7) {
                for (int32_t k = lbu(m + 0xbc) - 1; k < 10; ++k) gml_2e9680_l13(m, pv, lw(L13_ADDR(0x001d3478) + (static_cast<uint32_t>(k) << 2)));
                moby_sound_start(7, 0, m);
            }
        };
        // the hit on `src` (the word `at`); `piece`: the damage piece's (its own damage rule)
        auto hit_on = [&](uint32_t at, bool piece) {
            const uint32_t hit = sub_0025a420(lwu(at), NOT_ADDR(0x230000), 0);
            int32_t st = lw(sp);
            moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
            sw(sp, static_cast<uint32_t>(st));
            if (hit != 0 && lwu(hit + 0x20) != 0) {
                const uint32_t by = lhu(lwu(hit + 0x20) + 0xa6);
                if (by - 0x52u < 2u || static_cast<int16_t>(by) == 0xd4) {
                    sb(lwu(at) + 0xa4, 0xff);
                    return;
                }
            }
            if (lw(sp) < 2) return;
            const float dmg = lwc1(hit + 0x2c);
            if (0.0f < dmg) {
                const uint32_t kind = lhu(hit + 0x2a);
                if (piece) {
                    if (kind == 0x3f1) swc1(hit + 0x2c, 0.3f);
                } else if (kind == 0x127) {
                    swc1(hit + 0x2c, fpu_mul(dmg, 0.5f));
                } else if (kind == 0x3f1) {
                    swc1(hit + 0x2c, 0.1f);
                }
            }
            if (hit != 0 && lbu(m + 0xbc) == 7) swc1(hit + 0x2c, fpu_mul(lwc1(hit + 0x2c), 1.5f));
            const float health = lwc1(hs);
            if (health <= lwc1(hit + 0x2c)) {
                gml_2e97e0_l13(m, pv, hs);
                taken = 3;
            } else {
                swc1(hs, fpu_sub(health, lwc1(hit + 0x2c)));
                sb(pv + 0x67, 0xfa);
                taken = 1;
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0)));
                color_pulse_start(m, pv + 0x60);
            }
            sb(lwu(at) + 0xa4, 0xff);
            if (lbu(m + 0x20) == 7) {
                sw(sp + 4, 1);
                sb(m + 0xbc, lbu(m + 0xbc) + 1);
                ship_clear(lwu(SHIP));
                return;
            }
            const float now = lwc1(hs);
            if (now < stage_hp) stage_passed(piece);
            else if (now < warn_hp) sb(pv + 0x148, 1);
        };
        if (lwu(pv + 0xf0) != 0) hit_on(pv + 0xf0, true);
        for (uint32_t i = 0; taken == 0 && i < 11; ++i)
            if (lwu(pv + 0xc4 + (i << 2)) != 0) hit_on(pv + 0xc4 + (i << 2), false);
        if (taken > 0) {
            if (lw(sp + 4) != 0) {
                switch (lbu(m + 0xbc) - 1u) {  // (the table L13:0x1f4eb0, by the new stage - 1)
                case 1:  // (0x002ea1f4)
                    gml_2e86f8_l13(m, pv, 0xc, 4);
                    break;
                case 2:  // (0x002ea210)
                    sw(pv + 0x100, static_cast<uint32_t>(frame_scale_i(0x78)));
                    gml_2e86f8_l13(m, pv, 0x10, 1);
                    break;
                case 3:  // (0x002ea22c)
                    gml_2e86f8_l13(m, pv, 0x11, 1);
                    break;
                case 5:  // (0x002ea23c)
                    gml_2e86f8_l13(m, pv, 0x13, 1);
                    break;
                case 6:  // (0x002ea24c)
                    gml_2e86f8_l13(m, pv, 0x16, 1);
                    break;
                default:  // (0, 4, 7: 0x002ea2e8; beyond the table: nothing)
                    break;
                }
            } else if (lbu(m + 0xbc) == 7 && lbu(pv + 0x148) != 0) {
                if ((lhu(pv + 0x138) & 1) == 0 && gml_2e86f8_l13(m, pv, 0x14, 2) != 0) sh(pv + 0x138, lhu(pv + 0x138) | 1u);
                if ((lhu(pv + 0x138) & 2) == 0 && gml_2e86f8_l13(m, pv, 0x14, 2) != 0) sh(pv + 0x138, lhu(pv + 0x138) | 2u);
            }
        }
    }
    color_pulse_update(m, pv + 0x60);
}

// A homing missile (class 0x52, gemlik_boss_shots.cpp) spawned at `at` turned as `rot` (EE vectors) by `m` at
// `target` (0 none): `frames` to fly, `speed` and its top `top`, its velocity the maker's pvars' +0xc0 (one frame's
// move first), its smoke trail. 0 when none.
// orig: L13:0x002c13b0
uint32_t gml_2c13b0_l13(uint32_t m, uint32_t at, uint32_t target, uint32_t rot, int32_t frames, float speed, float top) {
    const uint32_t p = moby_spawn(0x52);
    if (p == 0) return 0;
    const uint32_t ppv = lwu(p + 0x78);
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    sb(p + 0x31, 1);
    sb(p + 0x20, 0);
    sw(ppv + 0x24, target);
    sw(ppv + 0x20, m);
    lq_sq(p + 0x10, at);
    lq_sq(p + 0x40, rot);
    swc1(ppv + 0x2c, speed);
    swc1(ppv + 0x38, top);
    sw(ppv + 0x28, static_cast<uint32_t>(frames));
    sw(ppv + 0x30, 0);
    sw(ppv + 0x34, 0);
    lq_sq(ppv, lwu(m + 0x78) + 0xc0);
    if (lbu(p + 0x53) != 1) moby_anim_set(p, 1, 0, 0xa);
    vec3_add(V(p + 0x10), V(p + 0x10), V(ppv));
    if (lwu(ppv + 0x24) != 0) lq_sq(ppv + 0x10, lwu(ppv + 0x24) + 0x10);
    swc1(p + 0x2c, lwc1(lwu(p + 0x24) + 0x24));
    moby_update_transform(p);
    spawn_particle_1a(p, 0x60808080u, static_cast<uint32_t>(frames), 0, 500000.0f);
    return p;
}

// The point ahead of Ratchet's ship (the ship the hero flies: no test) a shot from `from` at `speed` meets: along the
// ship's heading (its yaw / pitch with its turn speeds +0x90 / +0x94), as far as the ship flies (+0x64) in the shot's
// time, written to `out` (an EE vector).
// orig: L13:0x002ea330
void gml_2ea330_l13(uint32_t out, uint32_t from, float speed) {
    const uint32_t ship = lwu(SHIP);
    const uint32_t spv = lwu(ship + 0x78);
    const float yaw = angle_add(lwc1(ship + 0x48), lwc1(spv + 0x90));
    const float pitch = angle_add(lwc1(lwu(SHIP) + 0x44), lwc1(spv + 0x94));
    const float d = vec3_distance(V(from), V(lwu(SHIP) + 0x10));
    const float sspeed = lwc1(spv + 0x64);
    vec_from_spherical_v(V(out), fpu_mul(fpu_div(d, fpu_add(sspeed, speed)), sspeed), yaw, -pitch);
    vec3_add(V(out), V(out), V(lwu(SHIP) + 0x10));
}

// The yaw from `from` to its aim (the point ahead of Ratchet's ship for `speed` above 0, the ship itself otherwise;
// the hero without a ship of class 0x45), written to `yaw_out` (an EE float, 0 none) and returned; the pitch's
// negative to `pitch_out` (0 none).
// orig: L13:0x002ea400
float gml_2ea400_l13(uint32_t from, uint32_t pitch_out, uint32_t yaw_out, float speed) {
    const Frame f(112);
    const uint32_t sp = f.sp;
    const uint32_t ship = lwu(SHIP);
    float yaw, d, dz;
    if (ship != 0 && lh(ship + 0xa6) == 0x45) {
        if (0.0f < speed) gml_2ea330_l13(sp, from, speed);
        else lq_sq(sp, ship + 0x10);
        yaw = atan2_f(fpu_sub(lwc1(sp), lwc1(from)), fpu_sub(lwc1(sp + 4), lwc1(from + 4)));
        d = vec2_distance(V(from), V(sp));
        dz = fpu_sub(lwc1(sp + 8), lwc1(from + 8));
    } else {
        yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(from)), fpu_sub(lwc1(HERO_POS + 4), lwc1(from + 4)));
        d = vec2_distance(V(from), V(HERO_POS));
        dz = fpu_sub(lwc1(HERO_POS + 8), lwc1(from + 8));
    }
    const float pitch = -atan2_f(d, dz);
    if (yaw_out != 0) swc1(yaw_out, yaw);
    if (pitch_out != 0) swc1(pitch_out, pitch);
    return yaw;
}

// A missile launched from one of the two launchers (chain points 0x10 / 0x12; turn about, in stage 6 at random) at
// Ratchet's ship: turned as the ship's yaw with a 25-degree pitch, its speed 18 a second above the ship's, for 300
// frames, its sound. The missile or 0.
// orig: L13:0x002ea540
uint32_t gml_2ea540_l13(uint32_t m, uint32_t pv) {
    const Frame f(112);
    const uint32_t sp = f.sp, at = sp, rot = sp + 0x10;
    const uint32_t joint = (lbu(pv + 0xfc) & 1) != 0 ? 0x12 : 0x10;
    if (lbu(m + 0xbc) != 6 || (libc_rand() & 1) != 0) sb(pv + 0xfc, lbu(pv + 0xfc) ^ 1u);
    moby_chain_point_v(m, joint, V(at));
    sw(rot, 0);
    // (the aim's angles, written to rot +4 / +8, are written over below)
    gml_2ea400_l13(at, rot + 4, rot + 8, lbu(m + 0x20) != 5 ? fpu_mul(dt(), 17.0f) : -1.0f);
    const float speed = fpu_add(fpu_mul(dt(), 18.0f), lwc1(pv + 0xac));
    swc1(rot + 8, lwc1(m + 0x48));
    swc1(rot + 4, u2f(0x3edf66f3u));  // (25 degrees)
    const int32_t frames = frame_scale_i(0x12c);
    const uint32_t p = gml_2c13b0_l13(m, at, lwu(SHIP), rot, frames, speed, fpu_mul(dt(), 18.0f));
    if (p != 0) moby_sound_start(2, 0, m);
    return p;
}

// A mine (class 0x53, gemlik_boss_shots.cpp) dropped by `m` at `at` with the velocity `vel` (EE vectors): its health
// 20, 360 frames, at a tenth of its class's scale. 0 when none.
// orig: L13:0x002c1f28
uint32_t gml_2c1f28_l13(uint32_t m, uint32_t at, uint32_t vel) {
    const uint32_t p = moby_spawn(0x53);
    if (p == 0) return 0;
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    sb(p + 0x31, 1);
    sb(p + 0x20, 0);
    sb(p + 0xbc, 0);
    swc1(p + 0x2c, fpu_mul(lwc1(lwu(p + 0x24) + 0x24), 0.1f));
    const uint32_t ppv = lwu(p + 0x78);
    lq_sq(p + 0x10, at);
    lq_sq(ppv + 0x60, vel);
    sw(ppv + 0x74, m);
    sb(ppv + 0x28, 0);
    swc1(ppv + 0x20, 20.0f);
    sh(ppv + 0x24, 0x14);
    swc1(ppv + 0x30, 0.5f);
    sb(ppv + 0x29, 1);
    sw(ppv + 0x70, static_cast<uint32_t>(frame_scale_i(0x168)));
    if (lbu(p + 0x53) != 1) moby_anim_set(p, 1, 0, 0xa);
    moby_update_transform(p);
    moby_copy_hero_color(p);
    return p;
}

// A mine dropped at chain point 0xe: toward the point ahead of Ratchet's ship (20 a second, its sides jittered by
// 30 a second, 5 .. 30 a second more; with no ship of class 0x45 a random drift within 30 a second), plus the
// ship's velocity; its sound. The mine or 0.
// orig: L13:0x002ea6b8
uint32_t gml_2ea6b8_l13(uint32_t m, uint32_t pv) {
    const Frame f(240);
    const uint32_t sp = f.sp, at = sp, vel = sp + 0x10, ahead = sp + 0x20, way = sp + 0x30, side = sp + 0x40;
    moby_chain_point_v(m, 0xe, V(at));
    const uint32_t ship = lwu(SHIP);
    if (ship != 0 && lh(ship + 0xa6) == 0x45) {
        gml_2ea330_l13(ahead, at, fpu_mul(dt(), 20.0f));
        vec3_sub(V(way), V(ahead), V(at));
        vec3_set_length(V(vel), V(way), fpu_mul(dt(), 20.0f));
        const float j = fpu_mul(dt(), 30.0f);
        vec3_set_length(V(side), V(lwu(SHIP) + 0xd0), rand_range_f(-j, j));
        const float j2 = fpu_mul(dt(), 30.0f);
        vec3_set_length(V(way), V(lwu(SHIP) + 0xe0), rand_range_f(-j2, j2));
        vec3_add(V(side), V(side), V(way));
        const float hi = fpu_mul(dt(), 30.0f), lo = fpu_mul(dt(), 5.0f);
        vec3_set_length(V(side), V(side), rand_range_f(lo, hi));
        vec3_add(V(vel), V(vel), V(side));
    } else {
        rand_sphere_point(vel, fpu_mul(dt(), 0.0f), fpu_mul(dt(), 30.0f));
    }
    vec3_add(V(vel), V(vel), V(pv + 0x70));
    const uint32_t p = gml_2c1f28_l13(m, at, vel);
    if (p != 0) moby_sound_start(3, 0, m);
    return p;
}

// The attack of the stage (the table L13:0x1f4ed0 by the stage - 1): stage 1 bursts of eleven missiles (state 4) and
// pauses (state 3, line 1 of 2 once); 2 and 7 missiles and mines in turns (7: mines only); 3, 5, 6 the runs (state 5
// when 60 .. 100 from the hero, the run's timer and cooldown out), else missiles (3) or missiles and mines in turns;
// 4 the fighters' stage (stage 5 once none of them is left, line 0x1b of 2).
// orig: L13:0x002eac00
void gml_2eac00_l13(uint32_t m, uint32_t pv) {
    const Frame f(80);
    const uint32_t sp = f.sp;
    auto missile_then = [&](int32_t wait) {  // a missile, the next in 60 frames, the cooldown `wait`
        gml_2ea540_l13(m, pv);
        sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(pv + 0x124, static_cast<uint32_t>(frame_scale_i(wait)));
    };
    switch (lbu(m + 0xbc) - 1u) {
    case 0:  // (0x002eac48)
        if (lbu(m + 0x20) == 4) {
            if (tick32(pv + 0xf8) == 0) return;
            gml_2ea540_l13(m, pv);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x3c)));
            const uint32_t n = lbu(pv + 0xfd) + 1u;
            sb(pv + 0xfd, n);
            if ((n & 0xff) < 11) return;
            sb(pv + 0xfd, 0);
            sb(m + 0x20, 3);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x12c)));
        } else {
            if (tick32(pv + 0xf8) == 0) return;
            if ((lhu(pv + 0x138) & 1) == 0 && gml_2e86f8_l13(m, pv, 1, 2) != 0) sh(pv + 0x138, lhu(pv + 0x138) | 1u);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sb(m + 0x20, 4);
        }
        return;
    case 1: case 6:  // (0x002eaf74)
        if (tick32(pv + 0xf8) == 0) return;
        if (lbu(m + 0xbc) != 2) {
            sb(m + 0x20, 3);
            gml_2ea6b8_l13(m, pv);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x14)));
            return;
        }
        if (countdown_u8(pv + 0xfd) != 0) {
            const uint32_t mines = lbu(pv + 0xfe) ^ 1u;
            sb(pv + 0xfe, mines);
            if (mines != 0) {
                sb(pv + 0xfd, static_cast<uint32_t>(rand_range(0x14, 0x32)));
                sb(m + 0x20, 3);
            } else {
                sb(pv + 0xfd, static_cast<uint32_t>(rand_range(4, 6)));
                sb(m + 0x20, 4);
            }
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x3c)));
        } else if (lbu(pv + 0xfe) != 0) {
            gml_2ea6b8_l13(m, pv);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x14)));
        } else {
            gml_2ea540_l13(m, pv);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
        return;
    case 2: case 4: case 5: {  // (0x002ead04)
        tick32(pv + 0x124);
        tick32(pv + 0x100);
        tick32(pv + 0xf8);
        if (lbu(m + 0x20) == 5) {
            if (lw(pv + 0x100) == 0) {
                if (lw(pv + 0x124) != 0) return;
                sw(pv + 0x100, static_cast<uint32_t>(frame_scale_i(0x258)));
                sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x78)));
                sb(m + 0x20, 4);
                moby_sound_start(5, 0, m);
                return;
            }
            // (the doubleword +0xf8 masked with 0xff000000ffffffff: the timer out and the run's turn +0xff 1)
            if (lw(pv + 0xf8) != 0 || lbu(pv + 0xff) != 1) return;
            missile_then(0x78);
            return;
        }
        sound_stop(pv + 0x11c, m);
        moby_chain_point_v(m, 0x13, V(sp));
        const float d = vec3_distance(V(sp), V(HERO_POS));
        if (lw(pv + 0x100) == 0 && d < 100.0f && 60.0f < d) {  // a run starts
            if (lw(pv + 0xf8) != 0 || lw(pv + 0x124) != 0) return;
            sw(pv + 0x100, static_cast<uint32_t>(frame_scale_i(0x258)));
            lq_sq(pv + 0x80, sp);
            sb(pv + 0xff, 0);
            sb(m + 0x20, 5);
            return;
        }
        if (lbu(m + 0xbc) == 3) {
            sb(m + 0x20, 4);
            if (lw(pv + 0xf8) != 0) return;
            missile_then(0x78);
            return;
        }
        if (lw(pv + 0xf8) != 0) return;
        if (countdown_u8(pv + 0xfd) != 0 && lw(pv + 0x124) == 0) {
            const uint32_t mines = lbu(pv + 0xfe) ^ 1u;
            sb(pv + 0xfe, mines);
            if (mines != 0) {
                sb(pv + 0xfd, static_cast<uint32_t>(rand_range(0x14, 0x32)));
                sb(m + 0x20, 3);
            } else {
                sb(pv + 0xfd, static_cast<uint32_t>(rand_range(4, 6)));
                sb(m + 0x20, 4);
            }
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x78)));
            return;
        }
        if (lbu(pv + 0xfe) == 0) {
            missile_then(0x78);
        } else {
            gml_2ea6b8_l13(m, pv);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x14)));
            sw(pv + 0x124, static_cast<uint32_t>(frame_scale_i(0xf0)));
        }
        return;
    }
    case 3:  // (0x002eb030)
        sb(m + 0x20, 3);
        if (lbu(pv + 0x10e) != 0) return;
        sb(m + 0xbc, 5);
        gml_2e86f8_l13(m, pv, 0x1b, 2);
        ship_clear(lwu(SHIP));
        return;
    default:
        return;
    }
}

// orig: L13:0x002eb098
void gml_class388_update_l13(uint32_t m) {
    const Frame f(304);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    // the health bar's value +0x108: the health in sixteenths (the low four bits 15 when full), its sixth's
    // sixteenths in the low four bits; 0 at 0.5 or below
    {
        sw(pv + 0x108, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x20), 16.0f))) & 0xfff0u);
        const int32_t full = float_to_int(fpu_mul(fpu_sub(static_cast<float>(lh(pv + 0x24)), 0.5f), 16.0f));
        if (!(lw(pv + 0x108) < full)) sw(pv + 0x108, lwu(pv + 0x108) | 0xfu);
        const float health = lwc1(pv + 0x20);
        if (!(0.5f < health)) {
            sw(pv + 0x108, 0);
        } else {
            const int32_t hp = float_to_int(health);
            const int32_t sixth = ee_div(lh(pv + 0x24), 6);
            const uint32_t bar = lwu(pv + 0x108);
            int32_t frac = ee_div(ee_rem(hp, sixth) << 4, sixth);
            if (!(-1 < frac)) frac = 0;
            sw(pv + 0x108, bar + (static_cast<uint32_t>(frac) & 0xfu));
        }
    }
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4f60)
    case 0: {  // (0x002eb1c8): set up (retired when the save says destroyed)
        const uint32_t saved = lbu(m + 0xb0);
        if (saved != 0xff && lbu(0x0014c050u + saved + (lwu(0x0015ed84) << 4)) == 0xff) {
            moby_retire(m);
            return;
        }
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 4.0f));
        for (uint32_t i = 0; i < 10; ++i) sw(pv + 0xc4 + i * 4, gml_2ecd10_l13(m, static_cast<int32_t>(i), lw(L13_ADDR(0x001d34d8) + i * 4)));
        sw(pv + 0xec, m);
        for (uint32_t k = 0; k < 5; ++k) {  // a path that closes on its first point loses its last
            const uint32_t path = lvd::path(lw(pv + 0xb0 + k * 4));
            if (vec3_distance(V(path + 0x10), V(path + (static_cast<uint32_t>(lw(path)) << 4))) < 0.5f) lvd::set_path_count(path, lwu(path) - 1u);
        }
        swc1(pv + 0xac, fpu_mul(fpu_mul(dt(), 42.0f), 0.1f));
        swc1(pv + 0x70, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0xac)));
        const float s = sin_vu(lwc1(m + 0x48));
        sw(pv + 0x78, 0);
        swc1(pv + 0x74, fpu_mul(s, lwc1(pv + 0xac)));
        lq_sq(m + 0x10, path_point(lvd::path(lw(pv + 0xb0)), 0));
        sh(pv + 0x9e, 0);
        sh(pv + 0x9c, 1);
        sh(m + 0x32, 0xff);
        sb(m + 0x30, 0xff);
        sb(pv + 0x10c, 1);
        sb(pv + 0xfe, 0);
        sb(pv + 0xfc, 0);
        sb(m + 0x20, 1);
        sb(pv + 0x10e, 8);
        sw(pv + 0x110, 0);
        sw(pv + 0x114, 0xffffffffu);
        sw(pv + 0x118, 0xffffffffu);
        sw(pv + 0x11c, 0xffffffffu);
        swc1(pv + 0x20, static_cast<float>(lh(pv + 0x24)));
        sw(pv + 0x120, 0xffffffffu);
        sh(pv + 0x138, 0);
        sb(pv + 0x148, 0);
        sw(pv + 0x13c, 0);
        if (lw(pv + 0x104) != -1) {
            sub_002359f8(lwu(pv + 0x104), 0);
            sw(pv + 0x104, 0xffffffffu);
        }
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        break;
    }
    case 1: {  // (0x002eb3c8): wait until Ratchet flies a ship (his state 0x32)
        if (lw(0x001413d4) != 0x32 || lwu(SHIP) == 0) break;
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sh(m + 0x32, 0x3ff);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (lwu(SHIP) != 0) {
            const uint32_t spv = lwu(lwu(SHIP) + 0x78);
            if (lw(L13_ADDR(0x00161954)) == -1) {  // the fight's first frame
                sw(L13_ADDR(0x00161954), lwu(0x0015eea4));
                debug_printf_stub();  // (the format L13:0x1f4ef0 with the frame / 60 and / 360: the release build prints nothing)
            }
            // the health by the time the fight took to start (in units of 360 frames)
            const uint32_t waited = (lwu(0x0015eea4) - lwu(L13_ADDR(0x00161954))) / 0x168u;
            if (!(static_cast<int32_t>(waited) < 41)) {
                debug_printf_stub();  // (L13:0x161958)
                sh(pv + 0x24, 0x66);
                swc1(pv + 0x20, 102.0f);
                sw(spv + 0x138, 0x221);
            } else if (!(static_cast<int32_t>(waited) < 26)) {
                debug_printf_stub();  // (L13:0x161968)
                sh(pv + 0x24, 0x78);
                swc1(pv + 0x20, 120.0f);
                sw(spv + 0x138, 0x234);
            }
        }
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002eb4ec): approach along path 0, then stage 1
        ship_bc(0);
        health_bar(pv);
        swc1(pv + 0xac, fpu_add(lwc1(pv + 0xac), fpu_mul(fpu_sub(fpu_mul(dt(), 42.0f), lwc1(pv + 0xac)), 0.1f)));
        if (gml_2e9160_l13(m, pv, 0, true) == 0) break;
        sb(m + 0xbc, 1);
        ship_clear(lwu(HERO + 0x15f0));
        sb(pv + 0x13b, lbu(L13_ADDR(0x001d3448) + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2)));
        sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x14)));
        sb(m + 0x20, 3);
        join_path(m, pv, lbu(pv + 0x13b));
        break;
    }
    case 3: case 4: {  // (0x002eb6b0): fight
        health_bar(pv);
        ship_bc(0);
        gml_2eac00_l13(m, pv);
        if ((gml_2e9408_l13(m, pv, lbu(pv + 0x13b), true) & 2) != 0) {  // a marker: the stage's path slot
            const uint32_t e = L13_ADDR(0x001d3448) + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2);
            if (static_cast<int32_t>(lbu(pv + 0x13b)) != lw(e)) {
                sb(pv + 0x13b, lbu(e));
                join_path(m, pv, lw(e));
            }
        }
        if (lbu(m + 0xbc) != 4) gml_2e9a48_l13(m, pv);
        else color_pulse_update(m, pv + 0x60);
        break;
    }
    case 5: {  // (0x002eb86c): a run past the hero
        health_bar(pv);
        gml_2eac00_l13(m, pv);
        const uint32_t aim = pv + 0x80, nose = sp + 0x30, way = sp + 0x20, from = sp + 0x40, seen = sp + 0x50;
        moby_chain_point_v(m, 0x13, V(nose));
        const float to_hero = vec3_distance(V(aim), V(HERO_POS));
        sound_keep(pv + 0x11c, 4, m);
        if (lbu(pv + 0xff) == 0) {  // the aim moved toward the hero, then held (line 0x17 once)
            ship_bc(0);
            const float speed = fpu_add(lwc1(pv + 0xac), fpu_mul(fpu_sub(fpu_mul(dt(), 30.0f), lwc1(pv + 0xac)), 0.1f));
            swc1(pv + 0xac, speed);
            if (fpu_mul(dt(), 40.0f) < to_hero) {
                vec3_sub(V(way), V(HERO_POS), V(aim));
                vec3_set_length(V(way), V(way), fpu_mul(dt(), 40.0f));
                vec3_add(V(aim), V(aim), V(way));
            } else {
                lq_sq(way, HERO_POS);
                sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x78)));
                if ((lhu(pv + 0x138) & 4) == 0 && gml_2e86f8_l13(m, pv, 0x17, 4) != 0) sh(pv + 0x138, lhu(pv + 0x138) | 4u);
                sb(pv + 0xff, 1);
            }
            if ((gml_2e9160_l13(m, pv, lbu(pv + 0x13b), true) & 2) != 0) {  // (never: 0x2e9160 gives 0 / 1)
                const uint32_t e = L13_ADDR(0x001d3448) + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2);
                if (static_cast<int32_t>(lbu(pv + 0x13b)) != lw(e)) {
                    sb(pv + 0x13b, lbu(e));
                    join_path(m, pv, lw(e));
                }
            }
            // the aim's line from 8 off the nose: met by nothing or an asteroid, off screen hidden, or 100 off: the
            // run's timer out
            vec3_sub(V(from), V(aim), V(nose));
            vec3_set_length(V(from), V(from), 8.0f);
            vec3_add(V(from), V(from), V(nose));
            if (segment_collide_v(V(from), V(aim), 0, m, nullptr) != 0) {
                const uint32_t met = coll_hit_moby();
                if (met == 0 || lh(met + 0xa6) == 0xd4) sw(pv + 0x100, 0);
            }
            lq_sq(seen, aim);
            swc1(seen + 0xc, 1.0f);
            if (!(point_on_screen(seen, 100.0f) > 0) && lbu(m + 0x31) == 0) {
                debug_printf_stub();  // (L13:0x1f4f18)
                sw(pv + 0x100, 0);
            }
            if (100.0f < vec3_distance(V(from), V(HERO_POS))) {
                debug_printf_stub();  // (L13:0x1f4f38)
                sw(pv + 0x100, 0);
            }
        } else {  // turned to the hero, slowed, the aim walked to 30 off the nose, Ratchet's ship dragged onto it
            const float yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
            const float d = vec2_distance(V(m + 0x10), V(HERO_POS));
            gml_2e9018_l13(m, pv, yaw, -atan2_f(d, fpu_sub(lwc1(HERO_POS + 8), lwc1(m + 0x18))));
            ship_bc(1);
            swc1(pv + 0xac, fpu_mul(lwc1(pv + 0xac), 0.8f));
            (void)vec3_distance(V(m + 0x10), V(HERO_POS));  // (the original's result is left unused)
            moby_chain_point_v(m, 0x13, V(seen));
            vec3_sub(V(from), V(aim), V(seen));
            vec3_set_length(V(from), V(from), 30.0f);
            vec3_add(V(from), V(from), V(seen));
            vec3_sub(V(way), V(from), V(aim));
            const float off = vec3_distance(V(from), V(HERO_POS));
            if (!(off == 30.0f)) {
                const float step = fpu_mul(dt(), 15.0f);
                vec3_set_length(V(way), V(way), step < off ? step : off);
                vec3_add(V(aim), V(aim), V(way));
            }
            const uint32_t ship = lwu(HERO + 0x15f0);
            if (ship != 0) {
                lq_sq(ship + 0x10, aim);
                const uint32_t spv = lwu(ship + 0x78);
                const float limit = u2f(0x3ec49809u), per = u2f(0x3e32b8c2u);  // (22 degrees, 10 degrees)
                const float dyaw = angle_sub(atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(ship + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(ship + 0x14))),
                                             lwc1(spv + 0x38));
                const float ayaw = abs_f(dyaw);
                const float dd = vec2_distance(V(lwu(HERO + 0x15f0) + 0x10), V(m + 0x10));
                const float dpitch = angle_sub(-atan2_f(dd, fpu_sub(lwc1(m + 0x18), lwc1(lwu(HERO + 0x15f0) + 0x18))), lwc1(spv + 0x34));
                const float apitch = abs_f(dpitch);
                if (limit < ayaw)
                    swc1(spv + 0x38, angle_add(lwc1(spv + 0x38), angle_wrap_mod(fpu_mul(fpu_div(fpu_sub(ayaw, limit), per), dyaw))));
                if (limit < apitch)
                    swc1(spv + 0x34, angle_add(lwc1(spv + 0x34), angle_wrap_mod(fpu_mul(fpu_div(fpu_sub(apitch, limit), per), dpitch))));
            }
        }
        if (lbu(m + 0xbc) != 4) gml_2e9a48_l13(m, pv);
        else color_pulse_update(m, pv + 0x60);
        // the beam drawn while it faces the hero (within 60 degrees)
        const float to = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
        if (angle_dist(lwc1(m + 0x48), to) < u2f(0x3f860a92u)) sub_001f93e0(L13_ADDR(0x002ea8c8), m);
        break;
    }
    case 6: {  // (0x002ebf84): gloat, once the dialog stream is idle (line 0x1d + L13:0x161950)
        ship_bc(0);
        gml_2e9160_l13(m, pv, lbu(pv + 0x13b), true);
        if (lw(0x00151720) != 0 || lw(0x001516ec) != -1 || lh(pv + 0x138) > 0) break;
        if (gml_2e86f8_l13(m, pv, lw(L13_ADDR(0x00161950)) + 0x1d, 1) == 0) break;
        gloat_next();
        sh(pv + 0x138, lhu(pv + 0x138) + 1u);
        break;
    }
    case 7: {  // (0x002ec034): dying along its path, then the crash
        ship_bc(0);
        if (lw(pv + 0xf8) == 1) {
            gml_2e9680_l13(m, pv, 8);
            moby_sound_start(7, 0, m);
        }
        tick32(pv + 0xf8);
        const bool wrapped = gml_2e9160_l13(m, pv, lbu(pv + 0x13b), false) != 0;
        if (wrapped || !(lh(pv + 0x9c) < lw(slot_path(pv, lbu(pv + 0x13b))) - 1)) {
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sb(m + 0x20, 8);
        }
        const float k2 = u2f(0x40c90fdbu), k1 = u2f(0x40490fdbu);  // (2 pi, pi)
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x40860a92u))));  // (240 degrees a second)
        {  // turned along the path toward its point 3 on
            const uint32_t path = slot_path(pv, lbu(pv + 0x13b));
            const int32_t i = lh(pv + 0x9c), last = lw(path) - 1;
            const int32_t j = i + 3 < last ? i + 3 : last;
            const uint32_t p0 = path_point(path, i), p1 = path_point(path, j);
            const float yaw = atan2_f(fpu_sub(lwc1(p1), lwc1(p0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4)));
            const float d = vec2_distance(V(p0), V(p1));
            const float pitch = -atan2_f(d, fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8)));
            yaw_turn_damped(m, pv + 0xa8, yaw, fpu_mul(dt_sq(), k1), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k2));
            swc1(m + 0x44, angle_seek_rate(pv + 0xa4, lwc1(m + 0x44), pitch, fpu_mul(dt_sq(), k1), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k2)));
        }
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3f7fced9u)));     // (0.99925)
        swc1(pv + 0xac, fpu_mul(lwc1(pv + 0xac), u2f(0x3f7fcb92u)));   // (0.9992)
        if (rand_mod(0x11) == 0) {  // a burst about the ship now and then
            const uint32_t at = sp + 0x20, off = sp + 0x30;
            const float k = fpu_div(lwc1(m + 0x2c), fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 3.0f));
            vec3_set_length(V(off), V(m + 0xc0), fpu_mul(rand_range_f(-5.0f, 5.0f), k));
            vec3_add(V(at), V(m + 0x10), V(off));
            vec3_set_length(V(off), V(m + 0xd0), fpu_mul(rand_range_f(-4.0f, 4.0f), k));
            vec3_add(V(at), V(at), V(off));
            vec3_set_length(V(off), V(m + 0xe0), fpu_mul(rand_range_f(-4.0f, 4.0f), k));
            vec3_add(V(at), V(at), V(off));
            explosion_burst(m, at, -1, fpu_mul(k, 1.6f), 13.0f);
        }
        gml_2e8b58_l13(m, pv);
        if (lw(0x00151720) != 0 || lw(0x001516ec) != -1) break;
        if (lw(pv + 0xf8) < frame_scale_i(0x1a4) && lw(pv + 0x13c) < 0) {  // line 0x23 once
            if (gml_2e86f8_l13(m, pv, 0x23, 1) == 0) break;
            sw(pv + 0x13c, lwu(pv + 0x13c) & 0x7fffffffu);
            break;
        }
        if (lw(pv + 0xf8) < frame_scale_i(0xb4) && (lwu(pv + 0x13c) & 0x40000000u) != 0 && gml_2e86f8_l13(m, pv, 0x24, 1) != 0)  // line 0x24
            sw(pv + 0x13c, lwu(pv + 0x13c) & 0xbfffffffu);
        break;
    }
    case 8: {  // (0x002ec4b0): crash (an explosion at 30 frames), then gone: Ratchet's ship model to its state 8
        ship_bc(0);
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x40860a92u))));
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x48, lwc1(L13_ADDR(0x001670d8)));
        swc1(m + 0x2c, fpu_mul(scale, 0.98f));
        swc1(m + 0x44, lwc1(L13_ADDR(0x001670d4)));
        if (lw(pv + 0xf8) == frame_scale_i(0x1e))
            explosion_effect(m, pv + 0x70, 0, 0x1e, 8, 0x20, 7, 1, 5, -1, 0, 0.0f, 0.0f, 5.0f, 3.0f, 0.0f, 1.0f, 0.0f);
        gml_2e8b58_l13(m, pv);
        if (tick32(pv + 0xf8) == 0) break;
        const uint32_t model = moby_at(lw(pv + 0x134));
        sb(model + 0x20, 8);
        sh(model + 0x34, lhu(model + 0x34) | 2u);
        if (lw(pv + 0x120) != -1) region_clear(lwu(pv + 0x120));
        cam_entry_mark_type19(lw(pv + 0x12c));
        moby_retire(m);
        return;
    }
    case 9: {  // (0x002ec600): reset
        if (lwu(pv + 0xf0) != 0) {
            moby_retire(lwu(pv + 0xf0));
            sw(pv + 0xf0, 0);
        }
        if (lwu(pv + 0x110) != 0) {
            moby_retire(lwu(pv + 0x110));
            sw(pv + 0x110, 0);
        }
        for (uint32_t i = 0; i < 10; ++i) {
            const uint32_t p = lwu(pv + 0xc4 + i * 4);
            if (p == 0) continue;
            moby_retire(p);
            sw(pv + 0xc4 + i * 4, 0);
        }
        sound_stop(pv + 0x118, m);
        sound_stop(pv + 0x114, m);
        sound_stop(pv + 0x11c, m);
        if (lw(pv + 0x120) != -1) region_clear(lwu(pv + 0x120));
        if (lw(pv + 0x104) != -1) {
            sub_002359f8(lwu(pv + 0x104), 0);
            sw(pv + 0x104, 0xffffffffu);
        }
        const uint32_t path = lvd::path(lw(pv + 0xb0));
        lq_sq(m + 0x10, path + 0x10);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(m + 0x14))));
        sb(pv + 0x10c, 1);
        sb(pv + 0x148, 0);
        sh(pv + 0x9c, 0);
        sh(pv + 0x9e, 0);
        for (uint32_t o = 0xa0; o < 0xb0; o += 4) sw(pv + o, 0);
        sw(pv + 0xf4, 0);
        sw(pv + 0xf8, 0);
        for (uint32_t o = 0xfc; o < 0x100; ++o) sb(pv + o, 0);
        sw(pv + 0x100, 0);
        sb(pv + 0x10d, 0);
        sb(pv + 0x10e, 0);
        sb(pv + 0x10f, 0);
        sw(pv + 0x124, 0);
        sh(pv + 0x138, 0);
        sb(pv + 0x13a, 0);
        sb(pv + 0x13b, 0);
        sw(pv + 0x13c, 0);
        sh(pv + 0x144, 0);
        sh(pv + 0x146, 0);
        sb(m + 0xbc, 0);
        sq_zero(pv + 0x70);
        sq_zero(pv + 0x80);
        sb(m + 0x20, 0);
        return;
    }
    default:
        break;
    }

    // every frame
    keep_in_space(m);
    if (lbu(m + 0xbc) == 4) {  // the fighters' stage: its glow kept (state 1), its sound
        const uint32_t glow = lwu(pv + 0x110);
        if (glow == 0) sw(pv + 0x110, gml_2e8438_l13(m));
        else if (!(lbu(glow + 0x20) < 3)) sb(glow + 0x20, 1);
        sound_keep(pv + 0x118, 6, m);
    } else if (lwu(pv + 0x110) != 0) {  // its glow fading (state 3)
        sound_stop(pv + 0x118, m);
        const uint32_t glow = lwu(pv + 0x110);
        if (glow == 0 || lh(glow + 0xa6) != 0x160 || !alive(glow)) sw(pv + 0x110, 0);
        else if (lbu(glow + 0x20) < 3) sb(glow + 0x20, 3);
    }
    if (lbu(m + 0x20) < 2) {
        sb(pv + 0x10f, 0);
    } else {
        gml_2e8d28_l13(m, pv);
        if (lbu(m + 0x20) != 8 || frame_scale_i(0x78) < lw(pv + 0xf8)) gml_2e8920_l13(m, pv);
        sound_keep(pv + 0x114, 0, m);
        const uint32_t at = sp + 0x20;  // the engine's light, 7 behind the ship
        mat3_transform(V(at), V(L13_ADDR(0x00160660)), M(m + 0xc0));
        vec3_set_length(V(at), V(at), 7.0f);
        vec3_sub(V(at), V(m + 0x10), V(at));
        swc1(at + 0xc, 50.0f);
        if (lw(pv + 0x120) == -1) sw(pv + 0x120, static_cast<uint32_t>(light_add(at, 50.0f, 0.0f, 3.0f, 0.5f, 0.01f)));
        else lq_sq(L13_ADDR(0x001805c0) + (lwu(pv + 0x120) << 5) + 0x10, at);
        sb(pv + 0x10f, 0);
    }
    if (lbu(m + 0x20) != 6) {  // Ratchet's ship model gone or exploding: it gloats
        const uint32_t model = moby_at(lw(pv + 0x134));
        const bool gone = model == 0 || lh(model + 0xa6) != 0x45 || lbu(model + 0x20) == 0xfe || lbu(model + 0x20) == 0xfd ||
                          lbu(model + 0x20) == 7;
        if (gone) {
            sb(m + 0x20, 6);
            if (gml_2e86f8_l13(m, pv, lw(L13_ADDR(0x00161950)) + 0x1d, 1) != 0) {
                gloat_next();
                sh(pv + 0x138, 1);
            } else {
                sh(pv + 0x138, 0);
            }
        }
    }
    if (lbu(pv + 0x13a) != 0) {  // a line queued: stopped when the stream's state is 3, lines held off 360 / 60 frames
        if (lh(0x0015172a) == 3) {
            dialog_stream_stop();
            sh(pv + 0x144, static_cast<uint32_t>(frame_scale_i(0x168)));
            sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
            sb(pv + 0x13a, 0);
        }
    } else if (lbu(m + 0x20) != 6 && lw(0x001413d4) == 0x32) {  // a taunt now and then while Ratchet flies
        const uint32_t ship = lwu(SHIP);
        if (ship != 0 && lh(ship + 0xa6) == 0x45 && lwu(lwu(ship + 0x78) + 0x120) - 1u < 9999u && rand_mod(2) == 0)
            gml_2e86f8_l13(m, pv, 3, 9);
    }
    tick16(pv + 0x144);
    tick16(pv + 0x146);
    if (lbu(m + 0x20) - 2u < 4u && (lbu(0x0014095f) & 1) != 0) sb(m + 0x20, 9);
}

// The beam drawn by class 388 in a run (queued by its state 5): quads of the effect texture 0x34 from its nose (chain
// point 0x13) to its aim point +0x80 (100 at most), one a unit apart, crossed with the view's direction
// (L13:0x160680), half a unit wide, scrolling with the frame counter, fading over the last 10 units.
// orig: L13:0x002ea8c8
void gml_cb_2ea8c8_l13(uint32_t m) {
    const Frame f(416);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    // the quad's description at sp (corners +0 .. +0x30, colours +0x40, texture coordinates +0x50, GS words +0x70)
    const uint64_t tex = effect_tex0(0x34);
    sd(sp + 0x80, 0x0000ff9000000260ull);
    sw(sp + 0x40, 0x40808080u);
    sd(sp + 0x78, tex);
    sd(sp + 0x70, 4);
    sd(sp + 0x88, 0x0000008000000048ull);
    sw(sp + 0x4c, 0x40808080u);
    sw(sp + 0x48, 0x40808080u);
    sw(sp + 0x44, 0x40808080u);
    swc1(sp + 0x50, 0.0f);
    swc1(sp + 0x60, 0.0f);
    swc1(sp + 0x58, 1.0f);
    swc1(sp + 0x68, 1.0f);
    swc1(sp + 0x54, 0.0f);
    swc1(sp + 0x5c, 0.0f);
    swc1(sp + 0x64, 1.0f);
    swc1(sp + 0x6c, 1.0f);
    const uint32_t nose = sp + 0x90, step = sp + 0xa0, at = sp + 0xb0, back = sp + 0xc0;
    const uint32_t side = sp + 0xd0, up = sp + 0xe0, mid = sp + 0xf0;
    moby_chain_point_v(m, 0x13, V(nose));
    vec3_sub(V(step), V(pv + 0x80), V(nose));
    vec3_clamp_length_vu(V(step), V(step), 100.0f);
    const float len = vec3_length(V(step));
    const float k = fpu_div(len, 100.0f);
    const float unit = k < 1.0f ? 1.0f : k;
    vec3_set_length(V(step), V(step), unit);
    lq_sq(at, nose);
    const int32_t phase = ee_rem(lw(L13_ADDR(0x0015f5cc)), 20);
    vec3_set_length(V(back), V(step), fpu_div(int_to_float(phase), 20.0f));
    vec3_sub(V(at), V(at), V(back));
    float done = 0.0f;
    if (!(done < len)) return;
    do {
        const float half = fpu_add(fpu_div(fpu_mul(done, 0.0f), fpu_add(len, len)), 0.5f);
        vec3_cross_ba(V(side), V(step), V(L13_ADDR(0x00160680)));
        vec3_set_length(V(side), V(side), half);
        vec3_cross_ba(V(up), V(step), V(side));
        vec3_set_length(V(up), V(up), half);
        vec3_sub(V(mid), V(at), V(up));
        vec3_sub(V(sp), V(mid), V(side));
        vec3_add(V(sp + 0x10), V(mid), V(side));
        vec3_add(V(mid), V(at), V(up));
        vec3_sub(V(sp + 0x20), V(mid), V(side));
        vec3_add(V(sp + 0x30), V(mid), V(side));
        if (10.0f < len && fpu_sub(len, 10.0f) < done) {  // the last 10 units fade
            const uint32_t c = (static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(len, done), 6.4f))) << 24) | 0x808080u;
            sw(sp + 0x40, c);
            sw(sp + 0x4c, c);
            sw(sp + 0x48, c);
            sw(sp + 0x44, c);
        }
        done = fpu_add(done, unit);
        quad_draw(sp, 0);
        vec3_add(V(at), V(at), V(step));
    } while (done < len);
}

}  // namespace rac1
