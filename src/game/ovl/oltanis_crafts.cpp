// Oltanis (level 14), classes 908 and 921 (and the rocket 922, below): flying craft riding the level's paths (pvars +0x68, the place +0x60 / +0x64)
// at the level's speed, springing toward the path's point (+0x70 the springs), turning toward the way (+0x88), pitching
// (+0x84) and banking by the turn (+0x80), with their sound (4, the slot +0x6c) and four exhaust jets (particle 0x17:
// two puffs and three trails on either side). A hit of kind 0x800000 blows one up (hidden, its sound stopped; class 921
// only while it carries nothing, +0x8c = -1); on Oltanis the third such hit (0x15ee08 counts them) sets the flag
// 0x13d420 once, with a sound and a message. Gone, it comes back at its path's start after the level's delay.
// Class 908 loops its path for good (its jets on screen only). Class 921 can carry a moby (+0x8c, a moby index, with
// the moby that one names at its pvar +4): it waits hidden (state 1) until the level's three slots of things done
// (+0x98 .. +0xa0) are all done and the hero, on his feet and on no moby, stands in its box (+0xa4) seeing the
// carried moby (within 0.349 of the camera's pitch and 0.489 of its yaw, nothing between); then (its save bits set) it
// shows and flies its path (2) with the moby held under it, handing it its state 2 once it is past the path's 56th
// point, and at the path's end (or when the camera is no longer of type 19) it marks the scripted camera's entry
// (+0x90) and goes. Done before (its uid's byte or save bit), it takes the carried mobys with it at once. Its jets
// add a glow on one side. Ported by hand from Oltanis's own code (L14:0x2fc0f0, 0x2fc9a0, 0x2fc3e8, 0x2fcbb0,
// 0x2fd918, 0x2fd310, 0x2fe238, 0x2fdbb8, 0x2fded0), the original's order of calls and float operations kept.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

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

// a class's tuning words in the level's data (their addresses, read where used)
struct Steer {
    uint32_t k, d, turn_div, bank;  // the springs' rate and damping, the turn that gives a full bank, the bank's size
};
struct Jets {
    uint32_t fwd, up, side, step, color, glow;  // the jets' offsets along +0xc0 / +0xe0 / +0xd0 / +0xc0, the puffs' colour, the glow's offset (0: none)
    bool band;                                  // the particles' draw band from the moby's (+0x32)
    bool two_sides;                             // jets on both sides (class 922's: one)
};

// Toward the path's point now at sp (its springs), the turn toward the way and the pitch, the bank by the turn; no turn
// when the way is within 0.01 on x or y. (The common part of 0x2fc9a0 and 0x2fd918.)
void craft_steer(uint32_t m, uint32_t pv, uint32_t sp, const Steer& s) {
    spring_to(m + 0x10, pv + 0x70, lwc1(sp), lwc1(s.k), lwc1(s.d), 0.0f);
    spring_to(m + 0x14, pv + 0x74, lwc1(sp + 4), lwc1(s.k), lwc1(s.d), 0.0f);
    spring_to(m + 0x18, pv + 0x78, lwc1(sp + 8), lwc1(s.k), lwc1(s.d), 0.0f);
    vec3_sub(V(sp + 0x10), V(sp), V(m + 0x10));
    if (!(u2f(0x3c23d70au) < abs_f(lwc1(sp + 0x10)))) return;
    if (!(u2f(0x3c23d70au) < abs_f(lwc1(sp + 0x14)))) return;
    const float yaw = atan2_f(lwc1(sp + 0x10), lwc1(sp + 0x14));
    float bank = angle_sub(lwc1(m + 0x48), yaw);
    yaw_turn_damped(m, pv + 0x88, yaw, lwc1(s.k), lwc1(s.d), 0.0f);
    const float pitch = -atan2_f(vec2_length(V(sp + 0x10)), lwc1(sp + 0x18));
    spring_to(m + 0x44, pv + 0x84, pitch, lwc1(s.k), lwc1(s.d), 0.0f);
    bank = fpu_div(bank, lwc1(s.turn_div));
    if (1.0f < bank) bank = 1.0f;
    if (bank < -1.0f) bank = -1.0f;
    spring_to(m + 0x40, pv + 0x80, fpu_mul(bank, lwc1(s.bank)), u2f(0x3b449ba6u), lwc1(s.d), 0.0f);
}

// The four exhaust jets: on each side (+0xd0 times `side` either way, from a point `fwd` along +0xc0 and `up` along
// +0xe0), two puffs and, `step` on along +0xc0, three trails; class 921's add a glow (`glow` along +0xc0 from the
// middle point) after the first side's.
void craft_jets(uint32_t m, const Jets& j) {
    const Frame f(0x100);
    const uint32_t sp = f.sp, at = sp + 0x10, mid = sp + 0x20;
    auto band = [m] { return static_cast<uint32_t>(((static_cast<int32_t>(static_cast<int16_t>(lhu(m + 0x32))) >> 5) << 4) + float_to_int(4.0f)); };
    auto puffs = [&] {
        for (int k = 0; k < 2; ++k) {
            const int32_t r = rand_mod(16);
            const int32_t spin = rand_mod(2) == 0 ? r : -r;
            const uint32_t p = spawn_particle_17(at, spin, L14_ADDR(0x0015f580), lwu(j.color), u2f(0x3e4ccccdu), 1.0f, u2f(0x3f666666u), 200000.0f);
            if (p == 0) continue;
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(12)));
            if (j.band) sb(p + 9, band());
            sw(p + 0x24, 2);
            sb(p + 0x2a, 0x7f);
            sb(p + 0x2b, lbu(p + 0xa));
        }
    };
    auto trails = [&] {
        int32_t spin = 16;
        float size = 100000.0f;
        int32_t life = frame_scale_i(2);
        for (int k = 0; k < 3; ++k) {
            const uint32_t p = spawn_particle_17(at, spin, L14_ADDR(0x0015f580), 0x7fffffffu, u2f(0x3d4ccccdu), 1.0f, 1.0f, size);
            if (p != 0) {
                sh(p + 0xa, static_cast<uint32_t>(life));
                if (j.band) sb(p + 9, band());
                sb(p + 8, static_cast<uint32_t>(rand_mod(255)));
                sw(p + 0x24, 2);
                sb(p + 0x2a, 0x7f);
                sb(p + 0x2b, lbu(p + 0xa));
            }
            spin = -spin;
            life <<= 1;
            size = fpu_sub(size, 20000.0f);
        }
    };
    vec3_scale(V(sp), V(m + 0xc0), lwc1(j.fwd));
    vec3_add(V(at), V(m + 0x10), V(sp));
    vec3_scale(V(sp), V(m + 0xe0), lwc1(j.up));
    vec3_add(V(at), V(at), V(sp));
    lq_sq(mid, at);
    sw(sp + 0x34, m + 0xd0);
    sw(sp + 0x30, m + 0xc0);
    vec3_scale(V(sp), V(m + 0xd0), lwc1(j.side));
    vec3_add(V(at), V(at), V(sp));
    puffs();
    vec3_scale(V(sp), V(lwu(sp + 0x30)), lwc1(j.step));
    vec3_add(V(at), V(at), V(sp));
    trails();
    if (j.glow != 0) {  // (0x002fd548) a soft glow, two times in three
        vec3_scale(V(sp), V(m + 0xc0), lwc1(j.glow));
        vec3_add(V(at), V(mid), V(sp));
        const uint32_t grey = static_cast<uint32_t>(rand_range(0x40, 0x80));
        const uint32_t colour = grey | (grey << 16) | (grey << 8) | 0x7f000000u;
        if (rand_mod(3) != 0) {
            const int32_t r = rand_mod(4);
            const int32_t spin = rand_mod(2) == 0 ? r : -r;
            const uint32_t p = spawn_particle_17(at, spin, L14_ADDR(0x0015f580), colour, u2f(0x3d4ccccdu), 1.0f, 1.0f, 300000.0f);
            if (p != 0) {
                sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x3c)));
                if (rand_mod(2) != 0) sb(p + 3, 0x44);
                sb(p + 0x2a, 0x40);
                sw(p + 0x24, 2);
                sb(p + 0x2b, static_cast<uint32_t>(frame_scale_i(0x3c)));
            }
        }
    }
    if (!j.two_sides) return;
    vec3_scale(V(sp), V(lwu(sp + 0x34)), lwc1(j.side));
    vec3_sub(V(at), V(mid), V(sp));
    puffs();
    vec3_scale(V(sp), V(lwu(sp + 0x30)), lwc1(j.step));
    vec3_add(V(at), V(at), V(sp));
    trails();
}

// The craft back at its path's start (its first segment's heading), its springs still, after its delay.
void craft_respawn(uint32_t m, uint32_t pv, uint32_t state, int32_t delay) {
    const uint32_t path = lvd::path(lw(pv + 0x68));
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(delay)));
    sb(m + 0x20, state);
    sw(pv + 0x60, 0);
    sw(pv + 0x64, 0);
    lq_sq(m + 0x10, path + 0x10);
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
    swc1(pv + 0x80, 0.0f);
    swc1(pv + 0x78, 0.0f);
    swc1(pv + 0x74, 0.0f);
    swc1(pv + 0x70, 0.0f);
    swc1(pv + 0x88, 0.0f);
    swc1(pv + 0x84, 0.0f);
}

// its sound (4) started again when it is not playing
void craft_sound(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x6c)) == 0) sw(pv + 0x6c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
}
// its sound stopped (when it still plays for it), the slot cleared
void craft_sound_stop(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x6c);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x6c, 0xffffffffu);
}

// blown up: into state `gone`, the kills counted on Oltanis (the third raises the flag 0x13d420 once)
void craft_blow_up(uint32_t m, uint32_t pv, uint32_t gone) {
    if (lw(0x0015ed84u) == 14) {
        const int32_t kills = lw(0x0015ee08u) + 1;
        sw(0x0015ee08u, static_cast<uint32_t>(kills));
        if (!(kills < 3) && lbu(0x0013d420u) == 0) {
            sb(0x0013d420u, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
    }
    sb(m + 0x20, gone);
    explosion_burst(m, m + 0x10, -1, 5.0f, 13.0f);
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    craft_sound_stop(m, pv);
}

// Class 908 along its path (looping).
// orig: L14:0x002fc9a0
void craft908_follow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x90);
    int32_t seg = lw(pv + 0x60);
    float frac = lwc1(pv + 0x64);
    path_advance_v(lvd::path(lw(pv + 0x68)), V(f.sp), &seg, &frac, 1, fpu_mul(lwc1(L14_ADDR(0x00161f48)), lwc1(0x0015ed6cu)));
    sw(pv + 0x60, static_cast<uint32_t>(seg));
    swc1(pv + 0x64, frac);
    craft_steer(m, pv, f.sp, Steer{L14_ADDR(0x00161f4c), L14_ADDR(0x00161f50), L14_ADDR(0x00161f58), L14_ADDR(0x00161f54)});
}

// Class 908's jets: on screen only, flying.
// orig: L14:0x002fc3e8
void craft908_jets(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 2 || lbu(m + 0x31) == 0) return;
    craft_jets(m, Jets{L14_ADDR(0x00161f30), L14_ADDR(0x00161f38), L14_ADDR(0x00161f34), L14_ADDR(0x00161f3c), L14_ADDR(0x00161f44), 0, true, true});
}

// Class 921 along its path (looping only while it carries nothing, +0x8c < 0): carrying, at the path's end or once
// the camera is no longer of type 19, it marks the scripted camera's entry (+0x90), stops its sound and goes (1).
// orig: L14:0x002fd918
int32_t craft921_follow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x90);
    int32_t seg = lw(pv + 0x60);
    float frac = lwc1(pv + 0x64);
    const int32_t at_end = path_advance_v(lvd::path(lw(pv + 0x68)), V(f.sp), &seg, &frac, static_cast<int32_t>(lwu(pv + 0x8c) >> 31),
                                          fpu_mul(lwc1(L14_ADDR(0x00161f78)), lwc1(0x0015ed6cu)));
    sw(pv + 0x60, static_cast<uint32_t>(seg));
    swc1(pv + 0x64, frac);
    if (lw(pv + 0x8c) != 0 && (at_end != 0 || lh(lwu(L14_ADDR(0x00167500)) + 0x86) != 0x13)) {
        cam_entry_mark_type19(lw(pv + 0x90));
        craft_sound_stop(m, pv);
        moby_retire(m);
        return 1;
    }
    craft_steer(m, pv, f.sp, Steer{L14_ADDR(0x00161f7c), L14_ADDR(0x00161f80), L14_ADDR(0x00161f88), L14_ADDR(0x00161f84)});
    return 0;
}

// Class 921's jets: flying (state 2) only.
// orig: L14:0x002fd310
void craft921_jets(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 3 || st == 1) return;
    craft_jets(m, Jets{L14_ADDR(0x00161f60), L14_ADDR(0x00161f68), L14_ADDR(0x00161f64), L14_ADDR(0x00161f6c), L14_ADDR(0x00161f74),
                       L14_ADDR(0x00161f70), false, true});
}

// The carried moby `p` and the one it names (its pvar +4) retired, each while alive (its state byte below 0x80).
// orig: L14:0x002fe238
void cargo_retire(uint32_t p) {
    const uint32_t q = level_moby(lw(lwu(p + 0x78) + 4));
    if (q != 0 && static_cast<int8_t>(lbu(q + 0x20)) >= 0) moby_retire(q);
    if (static_cast<int8_t>(lbu(p + 0x20)) >= 0) moby_retire(p);
}

// Class 922's jets: one side only, with the glow.
// orig: L14:0x002fded0
void rocket922_jets(uint32_t m) {
    craft_jets(m, Jets{L14_ADDR(0x00161f90), L14_ADDR(0x00161f98), L14_ADDR(0x00161f94), L14_ADDR(0x00161f9c), L14_ADDR(0x00161fa4),
                       L14_ADDR(0x00161fa0), false, false});
}

// a moby's bit by its uid in a table of words (the save's bits of this level, the level's own)
void set_uid_bit(uint32_t table, uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const uint32_t w = table + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(uid)) >> 5) << 2);
    sw(w, lwu(w) | (1u << (uid & 0x1f)));
}
bool slot_done(uint32_t pv, uint32_t word) { return lbu(0x0014c050u + lwu(pv + word) + (lwu(0x0015ed84u) << 4)) == 0xff; }
}  // namespace

// orig: L14:0x002fc0f0
void olt_class908_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, 0x00800000u, 0);
    sb(m + 0xa4, 0xff);
    if (hit != 0 && lbu(m + 0x20) != 2) craft_blow_up(m, pv, 2);  // (0x002fc128)
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002fc24c) set up: its path (none, or one without points: retired)
        if (lw(pv + 0x68) == -1 || lw(lvd::path(lw(pv + 0x68))) == 0) {
            debug_printf_stub();  // (the original prints the uid)
            moby_retire(m);
            return;
        }
        path_mover_start(m);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0x6c, 0xffffffffu);
        sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161f5c)))));
        break;
    case 1:  // (0x002fc2d4)
        craft908_follow(m);
        break;
    case 2:  // (0x002fc2e4) gone: back when its timer runs out
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c))) != 0) craft_respawn(m, pv, 1, lw(L14_ADDR(0x00161f5c)));
        break;
    default:
        break;
    }
    craft908_jets(m);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 2) return;
    craft_sound(m, pv);
}

// orig: L14:0x002fcbb0
void olt_class921_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {  // done before (the level's byte by its uid, its save bit): gone with its cargo
        const uint32_t uid = lhu(m + 0xb2);
        const int32_t id = static_cast<int16_t>(uid);
        if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(id) + 0x454) != 0 ||
            ((lw(0x0014c190u + (lwu(0x0015ed84u) << 8) + (static_cast<uint32_t>(id >> 5) << 2)) >> (uid & 0x1f)) & 1) != 0) {
            cargo_retire(level_moby(lw(pv + 0x8c)));
            moby_retire(m);
            return;
        }
    }
    const uint32_t hit = sub_0025a420(m, 0x00800000u, 0);
    sb(m + 0xa4, 0xff);
    if (hit != 0 && lw(pv + 0x8c) == -1 && lbu(m + 0x20) != 3) craft_blow_up(m, pv, 3);  // (0x002fcc8c)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002fcdc0) set up: its path, the three slots
        if (lw(pv + 0x68) == -1 || lw(lvd::path(lw(pv + 0x68))) == 0) {
            debug_printf_stub();  // (the original prints the uid)
            moby_retire(m);
            return;
        }
        if (lw(pv + 0x98) < 0 || lw(pv + 0x9c) < 0 || lw(pv + 0xa0) < 0) {
            moby_retire(m);
            return;
        }
        path_mover_start(m);
        if (lw(pv + 0x8c) != 0) {  // a carrier: hidden with its cargo until it is called
            sb(m + 0x20, 1);
            sb(m + 0x30, 0xff);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            sw(m + 0x94, 0);
            const uint32_t p = level_moby(lw(pv + 0x8c));
            sh(p + 0x34, lhu(p + 0x34) | 0x41u);
            sw(pv + 0x94, 0);
        } else {
            sb(m + 0x20, 2);
            sb(m + 0x30, 0x80);
        }
        sh(m + 0x32, 0x80);
        sw(pv + 0x6c, 0xffffffffu);
        sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161f8c)))));
        break;
    }
    case 1: {  // (0x002fcecc) hidden: called when the slots are done and the hero, in its box, sees the cargo
        if (!slot_done(pv, 0x98) || !slot_done(pv, 0x9c) || !slot_done(pv, 0xa0)) break;
        if (lh(0x0013f65cu) != 0 || lwu(0x0013f64cu) != 0) break;
        if (!point_in_box(0x0013f3d0u, lw(pv + 0xa4))) break;
        const Frame f(0xc0);
        const uint32_t sp = f.sp;
        const uint32_t p = level_moby(lw(pv + 0x8c));
        const uint32_t q = level_moby(lw(lwu(p + 0x78) + 4));
        lq_sq(sp, q + 0x10);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), 4.0f));
        const uint32_t cam = L14_ADDR(0x001674c0);
        const float dist = vec2_distance(V(cam), V(sp));
        const float pitch_off = angle_dist(lwc1(L14_ADDR(0x001674d4)), -atan2_f(dist, fpu_sub(lwc1(sp + 8), lwc1(L14_ADDR(0x001674c8)))));
        const float yaw = atan2_f(fpu_sub(lwc1(q + 0x10), lwc1(L14_ADDR(0x001674c0))), fpu_sub(lwc1(q + 0x14), lwc1(L14_ADDR(0x001674c4))));
        if (!(angle_dist(lwc1(L14_ADDR(0x001674d8)), yaw) < u2f(0x3efa35ddu))) break;
        if (!(pitch_off < u2f(0x3eb2b8c2u))) break;
        if (segment_collide_v(V(cam), V(sp), 0x12, q, nullptr) != 0) break;
        set_uid_bit(0x0014c190u + (lwu(0x0015ed84u) << 8), m);
        set_uid_bit(L14_ADDR(0x001babd0), m);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(p + 0x34, lhu(p + 0x34) & 0xffbeu);
        sb(m + 0x20, 2);
        cam_entry_mark(lw(pv + 0x90));
        sh(q + 0x32, 0xff);
        sb(q + 0x30, 0xff);
        break;
    }
    case 2: {  // (0x002fd108) flying, the cargo held under it
        if (craft921_follow(m) != 0) return;
        if (lw(pv + 0x8c) == 0) break;
        const int32_t held = lw(pv + 0x94);
        if (!(held < 2)) break;
        if (held == 0 && !(lw(pv + 0x60) < 56)) sw(pv + 0x94, 1);
        const Frame f(0xc0);
        const uint32_t sp = f.sp, at = sp + 0x10;
        vec3_scale(V(at), V(m + 0xc0), u2f(0x3e570a3du));
        vec3_scale(V(sp), V(m + 0xe0), u2f(0xbf9eb852u));
        vec3_add(V(at), V(at), V(sp));
        vec3_add(V(at), V(m + 0x10), V(at));
        const uint32_t p = level_moby(lw(pv + 0x8c));
        lq_sq(p + 0x10, at);
        lq_sq(p + 0x40, m + 0x40);
        if (lw(pv + 0x94) != 1) break;
        moby_state_hook(p, 2);
        sw(pv + 0x94, 2);
        break;
    }
    case 3:  // (0x002fd1f4) gone: back (flying) when its timer runs out
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c))) != 0) craft_respawn(m, pv, 2, lw(L14_ADDR(0x00161f8c)));
        break;
    default:
        break;
    }
    craft921_jets(m);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 3 || st == 1) return;
    craft_sound(m, pv);
}

// Class 922: a rocket. Set up (0), it waits (1) lifted 1.5 with its target box (pvars +0) and the moby it destroys
// (+4); launched (2, by another class) it flies toward the box's point at the level's speed (0x161fa8 a second along
// its +0xc0 row), rolling, its pitch and heading easing toward the point, its jets and sound (4, the slot +8) on; within
// a step of it, it stops its sound, retires that moby and goes off at the point.
// orig: L14:0x002fdbb8
void olt_class922_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002fdc1c) set up
        if (lw(pv) < 0 || lw(pv + 4) < 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.5f));
        sw(pv + 8, 0xffffffffu);
        sw(pv + 0xc, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161fb0)))));
        return;
    case 2: {  // (0x002fdc68) flying
        const Frame f(0xb0);
        const uint32_t sp = f.sp, way = sp + 0x20;
        vec3_sub(V(way), V(lvd::box(lvd::BOXES, lw(pv)) + 0x30), V(m + 0x10));
        const float left = vec3_length(V(way));
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc)));
        const float step = fpu_mul(lwc1(L14_ADDR(0x00161fa8)), lwc1(0x0015ed6cu));
        if (left < step || left <= 0.0f) {  // (0x002fdce4) there: its target gone, an explosion at the point
            const int32_t slot = lw(pv + 8);
            if (slot != -1) {
                const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
            }
            const uint32_t target = level_moby(lw(pv + 4));
            sw(pv + 8, 0xffffffffu);
            if (target != 0) moby_retire(target);
            lq_sq(m + 0x10, lvd::box(lvd::BOXES, lw(pv)) + 0x30);
            lq_sq(sp + 0x30, m + 0xc0);
            swc1(sp + 0x38, 1.0f);
            explosion_effect(m, sp + 0x30, m + 0x10, 0x14, 10, 12, 0, 1, 10, -1, 0, 0.0f, 0.0f, 10.0f, 6.0f, 9.0f, 1.0f, 30.0f);
            moby_retire(m);
            return;
        }
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), u2f(0x3d0efa35u)));
        swc1(m + 0x44, angle_lerp(lwc1(m + 0x44), -atan2_f(vec2_length(V(way)), lwc1(sp + 0x28)), lwc1(L14_ADDR(0x00161fac))));
        swc1(m + 0x48, angle_lerp(lwc1(m + 0x48), atan2_f(lwc1(sp + 0x20), lwc1(sp + 0x24)), lwc1(L14_ADDR(0x00161fac))));
        vec3_scale(V(sp + 0x30), V(m + 0xc0), step);
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x30));
        rocket922_jets(m);
        if (sub_0028d8c0(m, lw(pv + 8)) == 0) sw(pv + 8, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        return;
    }
    default:  // (1: waiting)
        return;
    }
}

}  // namespace rac1
