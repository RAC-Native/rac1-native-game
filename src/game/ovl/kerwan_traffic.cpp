// Kerwan's own moby classes: Metropolis's air traffic (classes 75, 115 .. 120 and 132 run the same update), craft that fly
// the level's paths on a spline, bank into their turns, trail exhaust and smoke, hoot now and then, and burst into debris and
// bolts when shot, to come back a while later where nobody sees it. Header kerwan_mobys.h, registered in kerwan_mobys.cpp.
// Addresses are Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR, the engine's words shared with Veldin are the level's
// own (OV), the base ELF's are bare. The classes' own branches (which sound, which smoke, which debris) are switches on the
// class number, written out per class from the level's jump tables.
//
// The pvars: +0x60 the path follower (+0x60 the point, +0x64 the direction, +0x65 its start state, +0x6c, +0x70 the path,
// +0x74 the path's index), +0x90 / +0xa0 the tangents, +0xb0 / +0xc0 the leg's ends (+0xbc the leg's length), +0xd0 / +0xe0
// the spline's point now and before, +0xf0 the exhaust's place, +0x100 the phase along the leg, +0x104 / +0x108 the spline's
// tension, +0x10c the speed, +0x110 .. +0x128 the turn rates (roll, pitch) and the roll's gains, +0x12c the wait before it
// flies, +0x12e / +0x12f the smoke's countdowns, +0x131, +0x132 the yaw's offset (degrees), +0x134 / +0x138 / +0x13c the
// speed's sway (its size, its rate in degrees, its phase), +0x140 / +0x144 the bank's sway, +0x148 the horn's wait (-1: the
// first, +0x14c + up to +0x14e), +0x14a the time gone after a hit, +0x150 the engine sound's slot.
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float rate() { return lwc1(0x0015ed60); }   // (the frame's rate scale)
const float kDegree = u2f(0x3c8efa35u);
const float kTwoPi = u2f(0x40c90fdbu);

// the camera's position (the level's view, Veldin's 0x166dc0)
uint32_t camera_pos() { return OV(0x00166dc0); }

// The moby's sound in the slot word `slot_word` stopped when that slot still plays it, and the word set to -1.
void sound_release(uint32_t m, uint32_t slot_word) {
    const int32_t slot = lw(slot_word);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(slot_word, 0xffffffffu);
}

// `n` sparks (particle 0x16) from the craft's chain point `joint(k)`, each thrown at a random direction `speed` x the frame
// time fast plus the craft's own move `move`, lasting `life` 60 Hz frames, of size `size`, white-grey (`c_start`) to grey.
template <typename Joint>
void exhaust_sparks(uint32_t m, uint32_t sp, uint32_t move, int32_t n, Joint joint, float speed, uint32_t c_start, int32_t life, float size) {
    const uint32_t at = sp + 0x10, vel = sp + 0x20;
    for (int32_t k = 0; k < n; ++k) {
        moby_chain_point_v(m, joint(k), V(at));
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(V(vel), fpu_mul(dt(), speed), a, b);
        vec3_add(V(vel), V(vel), V(move));
        particle_16_spawn(at, vel, c_start, 0x00808080u, frame_scale_i(life), size);
    }
}

// The next smoke wait: 7 .. 20 frames.
void smoke_wait(uint32_t pv) { sb(pv + 0x12e, static_cast<uint32_t>(rand_range(frame_scale_i(7), frame_scale_i(0x14)))); }
}  // namespace

// The level's sound `idx` counted from the level's own sounds (after the shared ones: +OV(0x15f574)), started with `flags` on
// `moby`; its slot keeps the index (+0x7e) and the moby (+0x88). The slot, or -1 (none past the list's count 0x15f5f0).
// orig: L3:0x0027a618
int32_t kerwan_level_sound_start_l3(int32_t idx, uint32_t flags, uint32_t moby) {
    const int32_t i = idx + lw(OV(0x0015f574));
    if (!(i < lw(OV(0x0015f5f0)))) return -1;
    const int32_t s = sound_start(lwu(OV(0x0015f5f4)) + (static_cast<uint32_t>(i) << 5), flags, moby, 0, 0x400);
    if (s >= 0) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s * 0x70);
        sh(e + 0x7e, static_cast<uint32_t>(i));
        sw(e + 0x88, moby);
    }
    return s;
}

// The engine's sound: none while the craft is still (state 0: the slot word -1); on the craft's eighth of the frames (the
// frame counter's low 3 bits against the moby's), when it is not gone after a hit and its sound is not playing, the class's
// engine: sound 1 for classes 75, 115, 116, 118 and 119, sound 0 for 117 and 120 (flags 4), its slot kept in +0x150.
// orig: L3:0x0029c9d0 (the jump table L3:0x1e2710 on the class - 75)
void traffic_engine_sound_l3(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        sw(pv + 0x150, 0xffffffffu);
        return;
    }
    if ((lwu(OV(0x0015f5cc)) & 7u) != ((static_cast<uint32_t>(static_cast<int32_t>(m) >> 8)) & 7u)) return;
    if (lh(pv + 0x14a) != 0) return;
    const int32_t slot = lw(pv + 0x150);
    if (slot >= 0 && sub_0028d8c0(m, slot) != 0) return;
    int32_t sound;
    switch (static_cast<int16_t>(lhu(m + 0xa6) - 0x4b)) {
    case 75 - 75: case 115 - 75: case 116 - 75: case 118 - 75: case 119 - 75: sound = 1; break;  // (0x0029ca7c)
    case 117 - 75: case 120 - 75: sound = 0; break;                                               // (0x0029ca70)
    default: return;                                                                              // (0x0029ca90)
    }
    sw(pv + 0x150, static_cast<uint32_t>(moby_sound_start(sound, 4, m)));
}

// A puff of smoke (class 235) at the craft's exhaust (+0xf0), its angles with a random roll, `scale` x its class's size,
// always drawn: its pvars the craft (+0), the colours `colour` / `colour_end` (+4 / +8, the start also its colour), its life
// `life` (+0xc and +0xe) and rise `rise` (+0x14). The puff, or 0.
// orig: L3:0x002bad40 (its second argument, the exhaust's place, is not read)
uint32_t traffic_puff_spawn_l3(uint32_t m, uint32_t colour, uint32_t colour_end, int32_t life, float rise, float scale) {
    const uint32_t puff = moby_spawn(0xeb);
    if (puff == 0) return 0;
    const uint32_t ppv = lwu(puff + 0x78);
    swc1(puff + 0x2c, fpu_mul(lwc1(puff + 0x2c), scale));
    sb(puff + 0x30, 0xff);
    sh(puff + 0x32, 0x7f);
    sb(puff + 0x31, 1);
    sw(ppv, m);
    lq_sq(puff + 0x10, lwu(m + 0x78) + 0xf0);
    lq_sq(puff + 0x40, m + 0x40);
    swc1(puff + 0x40, rand_angle());
    sw(ppv + 8, colour_end);
    sh(ppv + 0xe, static_cast<uint32_t>(life));
    swc1(ppv + 0x14, rise);
    sh(ppv + 0xc, static_cast<uint32_t>(life));
    sw(ppv + 4, colour);
    moby_set_color_hi(puff, colour & 0xff, (colour >> 8) & 0xff, (colour >> 16) & 0xff);
    moby_update_transform(puff);
    return puff;
}

// The craft's exhaust after its move `move` (this frame's), unless it is hidden (+0x34 bit 0), by class:
// 116: two sparks a frame from joints 0 and 1 (on screen only);
// 115, 117, 120, 132: on screen, each time the countdown +0x12e runs out, while it flies slower than 40 a frame within 75 of
//   the camera, ten sparks from joint 0; then a new countdown;
// 118: the same with fifteen bigger sparks within 90;
// 75, 119: the smoke countdowns +0x12f / +0x12e; the exhaust's place (+0xf0, joint 0) kept up to date, and when +0x12e runs out
//   (and +0x12f off screen) within 90 of the camera a puff (class 75's colours its own); a new countdown each.
// orig: L3:0x0029caa8 (the jump table L3:0x1e27d0 on the class - 75)
void traffic_exhaust_l3(uint32_t m, uint32_t move_in, uint32_t off_screen) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t move = sp;
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(move, move_in);
    auto slow_near = [&](float range) {  // moving slower than 40 a frame, within `range` of the camera
        if (!(vec3_length(V(move)) < fpu_mul(dt(), 40.0f))) return false;
        return vec3_distance(V(m + 0x10), V(camera_pos())) < range;
    };
    if ((lhu(m + 0x34) & 1u) != 0) {
        ee_stack_free(0xb0);
        return;
    }
    switch (static_cast<int16_t>(lhu(m + 0xa6) - 0x4b)) {
    case 116 - 75:  // (0x0029cb28)
        if (off_screen != 0) break;
        vec3_scale(V(move), V(move), u2f(0x3eb33333u));
        exhaust_sparks(m, sp, move, 2, [](int32_t k) { return static_cast<uint32_t>(k); }, u2f(0x3e19999au), 0x8040c0f0u, 0x14, 84000.0f);
        break;
    case 115 - 75: case 117 - 75: case 120 - 75: case 132 - 75:  // (0x0029cbfc)
        if (off_screen != 0) break;
        if (countdown_u8(pv + 0x12e) == 0) break;
        if (slow_near(75.0f)) {
            vec3_scale(V(move), V(move), u2f(0x3f333333u));
            exhaust_sparks(m, sp, move, 10, [](int32_t) { return 0u; }, u2f(0x3f19999au), 0x80808080u, 0x23, 105000.0f);
        }
        smoke_wait(pv);
        break;
    case 118 - 75:  // (0x0029cd50)
        if (off_screen != 0) break;
        if (countdown_u8(pv + 0x12e) == 0) break;
        if (slow_near(90.0f)) {
            vec3_scale(V(move), V(move), 0.5f);
            exhaust_sparks(m, sp, move, 15, [](int32_t) { return 0u; }, u2f(0x3f99999au), 0x80808080u, 0x2d, 168000.0f);
        }
        smoke_wait(pv);
        break;
    case 75 - 75: case 119 - 75: {  // (0x0029cea0)
        countdown_u8(pv + 0x12f);
        if (off_screen != 0 && lbu(pv + 0x12f) == 0) break;
        countdown_u8(pv + 0x12e);
        if (lbu(pv + 0x12e) == 0 || lbu(pv + 0x12f) != 0) moby_chain_point_v(m, 0, V(pv + 0xf0));
        if (lbu(pv + 0x12e) != 0) break;
        if (slow_near(90.0f)) {
            const bool own = lh(m + 0xa6) == 0x4b;
            traffic_puff_spawn_l3(m, own ? 0x30a0u : 0xa040u, own ? 0x90a0u : 0xa080u, frame_scale_i(0x32), fpu_mul(dt(), 2.5f), u2f(0x3e19999au));
            sb(pv + 0x12f, static_cast<uint32_t>(frame_scale_i(0x32)));
        }
        sb(pv + 0x12e, static_cast<uint32_t>(frame_scale_i(0xd)));
        break;
    }
    default:  // (0x0029cfc0)
        break;
    }
    ee_stack_free(0xb0);
}

// The craft's attitude on the spline: its yaw turned by a fraction (the frame's rate over +0x120) of the way to the spline's
// heading (+ the class's yaw offset +0x132 in degrees), its pitch toward the spline's slope and its roll toward the turn's
// share (+0x124, less the roll times +0x128), both through angle_seek_rate with the rates +0x118 / +0x110.
// orig: L3:0x0029cff0
void traffic_attitude_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t pv = lwu(m + 0x78);
    const float offset = fpu_mul(static_cast<float>(static_cast<int32_t>(lh(pv + 0x132))), kDegree);
    // (the original first computes the yaw less the offset and throws it away: angle_sub is pure)
    const float heading = angle_add(atan2_f(fpu_sub(lwc1(pv + 0xd0), lwc1(pv + 0xe0)), fpu_sub(lwc1(pv + 0xd4), lwc1(pv + 0xe4))), offset);
    const float diff = angle_sub(heading, lwc1(m + 0x48));
    const float turn = fpu_mul(diff, fpu_mul(fpu_div(1.0f, lwc1(pv + 0x120)), rate()));
    swc1(m + 0x48, angle_add(turn, lwc1(m + 0x48)));
    vec3_sub(V(sp), V(pv + 0xd0), V(pv + 0xe0));
    const float slope = -atan2_f(vec2_length(V(sp)), lwc1(sp + 8));
    {
        const float r = lwc1(pv + 0x118);
        const float k = fpu_mul(fpu_mul(fpu_mul(r, 10.0f), kDegree), dt_sq());
        const float damp = fpu_mul(fpu_mul(fpu_mul(r, 5.0f), kDegree), dt_sq());
        swc1(m + 0x44, angle_seek_rate(pv + 0x11c, lwc1(m + 0x44), slope, k, damp, fpu_mul(fpu_mul(r, kDegree), dt())));
    }
    const float lean = angle_wrap_mod(fpu_mul(turn, lwc1(pv + 0x124)));
    const float roll_to = angle_wrap_mod(fpu_mul(angle_sub(lwc1(m + 0x40), lean), lwc1(pv + 0x128)));
    {
        const float r = lwc1(pv + 0x110);
        const float k = fpu_mul(fpu_mul(fpu_mul(r, 10.0f), kDegree), dt_sq());
        const float damp = fpu_mul(fpu_mul(fpu_mul(r, 5.0f), kDegree), dt_sq());
        swc1(m + 0x40, angle_seek_rate(pv + 0x114, lwc1(m + 0x40), roll_to, k, damp, fpu_mul(fpu_mul(r, kDegree), dt())));
    }
    ee_stack_free(0x60);
}

// The horn (classes 51, 64, 75, 115, 116, 118 and 119): the first wait +0x14c + up to +0x14e frames; after each, sound 0 when
// the craft is between 15 and 90 from the camera, and a new wait of 2200 .. 3200 frames.
// orig: L3:0x0029d1b8 (the jump table L3:0x1e28c0 on the class - 51)
void traffic_horn_l3(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (static_cast<int16_t>(lhu(m + 0xa6) - 0x33)) {
    case 51 - 51: case 64 - 51: case 75 - 51: case 115 - 51: case 116 - 51: case 118 - 51: case 119 - 51:  // (0x0029d208)
        break;
    default:  // (0x0029d2c8)
        return;
    }
    int32_t lo, hi;
    if (lh(pv + 0x148) == -1) {
        lo = lh(pv + 0x14c);
        hi = lo + lh(pv + 0x14e);
    } else {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x148))) == 0) return;
        const float d = vec3_distance(V(m + 0x10), V(camera_pos()));
        if (15.0f < d && d < 90.0f) moby_sound_start(0, 0, m);
        lo = frame_scale_i(0x898);
        hi = frame_scale_i(0x898) + frame_scale_i(0x3e8);
    }
    sh(pv + 0x148, static_cast<uint32_t>(frame_scale_i(rand_range(lo, hi))));
}

// A hit on the craft (kinds 0xa30000; not the record of kind 0x102): its bolts (10 .. 20 for the hits with bit 0x800000, else
// 3 .. 5), gone for 600 frames, an explosion at its middle (class 75's 2.747 behind its nose) moving with half its move, its
// debris (three pieces per class, 115 .. 120 and 132) thrown with its forward axis and a lift, and the level's sound 0 (class
// 795 also, once, sound 1 and the help text 0x53d6). Each frame: its address left as the last update with hits, the hit cleared;
// while it is gone, hidden and silent, until the wait runs out where it is off screen: then it shows again (its class's
// collision back).
// orig: L3:0x0029d2e0 (the jump table L3:0x1e29e0 on the class - 115)
void traffic_hit_l3(uint32_t m, uint32_t move, uint32_t off_screen) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t at = sp + 0x20, drift = sp + 0x30, throw_vel = sp + 0x40;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0xa30000u), 0);
    if (hit != 0 && lhu(hit + 0x28) != 0x102) {
        vec_clear(V(at));
        if (lh(m + 0xa6) == 0x4b) {
            const float back = u2f(0xc02fced9u);
            swc1(at, fpu_mul(cos_vu(lwc1(m + 0x48)), back));
            swc1(at + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), back));
            sw(at + 8, 0);
        }
        lq_sq(drift, move);
        vec3_scale(V(drift), V(drift), 0.5f);
        vec3_add(V(at), V(at), V(m + 0x10));
        if ((lwu(hit + 0x24) & 0x800000u) != 0) bolts_drop(m, 10, 20, 2, -1);
        else bolts_drop(m, 3, 5, 2, -1);
        sh(pv + 0x14a, static_cast<uint32_t>(frame_scale_i(0x258)));
        explosion_effect(m, drift, at, 0x14, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
        vec3_scale(V(throw_vel), V(m + 0xc0), fpu_mul(rate(), u2f(0x3d99999au)));
        swc1(throw_vel + 8, fpu_add(lwc1(throw_vel + 8), fpu_mul(rate(), u2f(0x3da3d70au))));
        auto debris = [&](uint32_t cls) {
            const uint32_t zero = OV(0x0015f580);
            debris_spawn(m, cls, m + 0x10, m + 0x40, frame_scale_i(0x5a), 0, throw_vel, zero, zero, fpu_mul(dt_sq(), 12.0f));
        };
        auto three = [&](uint32_t a, uint32_t b, uint32_t c) { debris(a); debris(b); debris(c); };
        switch (static_cast<int16_t>(lhu(m + 0xa6) - 0x73)) {
        case 115 - 115: three(0x6a9, 0x6aa, 0x6ab); break;  // (0x0029d4fc)
        case 116 - 115: three(0x6ac, 0x6ad, 0x6ae); break;  // (0x0029d5c0)
        case 117 - 115: three(0x6af, 0x6b0, 0x6b1); break;  // (0x0029d684)
        case 118 - 115: three(0x6b2, 0x6b3, 0x6b4); break;  // (0x0029d748)
        case 119 - 115: three(0x6b5, 0x6b6, 0x6b7); break;  // (0x0029d80c)
        case 120 - 115: three(0x6b8, 0x6b9, 0x6ba); break;  // (0x0029d8d0)
        case 132 - 115: three(0x6bc, 0x6be, 0x6bf); break;  // (0x0029d9a0)
        default: break;                                      // (0x0029da68)
        }
        if (lh(m + 0xa6) == 0x31b && lbu(0x0013d40c) == 0) {
            sb(0x0013d40c, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        kerwan_level_sound_start_l3(0, 0, m);
    }
    sw(OV(0x00160770), L3_ADDR(0x0029dba8u));
    sb(m + 0xa4, 0xff);
    if (lh(pv + 0x14a) != 0) {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x14a))) != 0 && off_screen != 0) {
            sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        } else {
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            sound_release(m, pv + 0x150);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        }
    }
    ee_stack_free(0xe0);
}

// Classes 75, 115 .. 120 and 132, the air traffic. First, off screen or not (when not always drawn, +0x31: its place within
// its draw distance +0x32); a craft of a group (+0x21) flying, the group's first member, finds whether any member is on
// screen, and when none is all go to state 3 for 5 frames; its engine sound and horn. The state (+0x20):
// 0 on its path (+0x74; a path closed on itself loses its repeated last point): always drawn, its start on the path (the
//   nearest point when the follower has no start), the spline's first leg, its angles from the tangent, the sway's phase
//   (+0x13c over 2 pi: degrees; -1 with a sway: random), placed, banked -> 3 when it waits (+0x12c), else 1; without a path -> 2;
// 1 flies: the phase along the leg past 1 starts the next leg (the tangents, the next point); the speed swayed (+0x134 x the
//   sine of +0x13c, which turns by +0x138 degrees) over the leg's length; the next point of the Hermite spline; on screen its
//   attitude (traffic_attitude_l3), off screen level and facing along; placed at the spline's point before (and banked on
//   screen); then its exhaust and its hits after its move;
// 2 without a path: -> 0 once it has one; its sound stopped;
// 3 waits (+0x12c) -> 1.
// orig: L3:0x0029dba8
void kerwan_class75_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t origin = sp, old_ang = sp + 0x10, old_pos = sp + 0x20, probe = sp + 0x30, move = sp + 0x40, iter = sp + 0x50;
    sd(origin, 0);
    sd(origin + 8, 0);
    uint32_t off_screen = 0;
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x31) == 0) {
        lq_sq(probe, m + 0x10);
        swc1(probe + 0xc, 7.0f);
        off_screen = point_on_screen(probe, static_cast<float>(static_cast<int32_t>(lh(m + 0x32)))) == -1 ? 1u : 0u;
    }
    if (pv == 0) {
        ee_stack_free(0xf0);
        return;
    }
    const uint32_t fol = pv + 0x60;
    const uint32_t group = lbu(m + 0x21);
    if (group != 0xff && lbu(m + 0x20) != 0 && lbu(m + 0x20) != 3) {
        group_first(iter, static_cast<int32_t>(group), 0, 0);
        if (lwu(iter) == m) {
            bool seen = false;
            for (uint32_t mate = lwu(iter); mate != 0; mate = lwu(iter)) {
                if (moby_on_screen_l1(mate, int_to_float(lh(mate + 0x32))) != -1) {
                    seen = true;
                    break;
                }
                group_next(iter, lwu(iter), 0, 0);
            }
            if (!seen) {
                group_first(iter, static_cast<int32_t>(lbu(m + 0x21)), 0, 0);
                while (lwu(iter) != 0) {
                    const uint32_t mate = lwu(iter);
                    sb(mate + 0x20, 3);
                    sh(lwu(mate + 0x78) + 0x12c, static_cast<uint32_t>(frame_scale_i(5)));
                    group_next(iter, lwu(iter), 0, 0);
                }
            }
        }
    }
    traffic_engine_sound_l3(m);
    traffic_horn_l3(m);
    switch (lbu(m + 0x20)) {
    case 0: {
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        if (lw(fol + 0x14) != -1) {
            const uint32_t path = lvd::path(lw(fol + 0x14));
            if (vec3_distance(V(path + 0x10), V(path + static_cast<uint32_t>(lw(path)) * 16)) < 0.5f) lvd::set_path_count(path, static_cast<uint32_t>(lw(path) - 1));
        }  // (else a debug message naming the moby and its class)
        sb(m + 0x30, 0xff);
        sb(pv + 0x2b, 1);
        sb(pv + 0x2c, 0x25);
        if (500.0f < lwc1(pv + 0x140)) swc1(pv + 0x140, fpu_div(fpu_mul(rand_range_f(-1.0f, 1.0f), lwc1(pv + 0x140)), 1000.0f));
        if (500.0f < lwc1(pv + 0x144)) swc1(pv + 0x144, fpu_div(fpu_mul(rand_range_f(-1.0f, 1.0f), lwc1(pv + 0x144)), 1000.0f));
        if (lw(fol + 0x14) == -1) {
            sb(m + 0x20, 2);
            break;
        }
        {
            const int32_t index = static_cast<int32_t>(m - lwu(OV(0x0015ffd8))) >> 8;
            sb(pv + 0x131, static_cast<uint32_t>(ee_rem(index / 2, 7)));  // (overwritten with 7 below, as the original does)
        }
        path_follower_init_l1(fol);
        if (lbu(fol + 5) == 0) {
            if (!(lw(fol) > 0)) {
                const int32_t near = path_nearest_l1(m + 0x10, lwu(fol + 0x10), 0.0f);
                sw(fol, static_cast<uint32_t>(near));
                if (static_cast<int8_t>(lbu(fol + 4)) >= 0) {
                    if (near == lw(lwu(fol + 0x10)) - 1) sw(fol, 0);
                } else if (near == 0) {
                    sw(fol, 0);
                }
            }
            const uint32_t point = lwu(fol + 0x10) + 0x10 + (static_cast<uint32_t>(lw(fol)) << 4);
            swc1(fol + 0xc, vec3_distance(V(m + 0x10), V(point)));
            swc1(fol + 0x18, atan2_f(fpu_sub(lwc1(point), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14))));
            sb(fol + 5, 2);
            sw(fol + 8, 0);
            sw(fol + 0x1c, 0);
        }
        auto point = [&] { return lwu(fol + 0x10) + 0x10 + (static_cast<uint32_t>(lw(fol)) << 4); };
        lq_sq(pv + 0xe0, point());
        swc1(pv + 0x100, 1.0f);
        path_spline_tangent_l1(pv + 0xa0, fol, 0, lwc1(pv + 0x104), lwc1(pv + 0x108));
        lq_sq(pv + 0xb0, point());
        lq_sq(pv + 0xc0, point());
        sw(m + 0x40, 0);
        swc1(m + 0x44, -atan2_f(vec2_length(V(pv + 0xa0)), lwc1(pv + 0xa8)));
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(pv + 0xa0), lwc1(origin)), fpu_sub(lwc1(pv + 0xa4), lwc1(origin + 4))));
        sw(pv + 0x114, 0);
        sw(pv + 0x11c, 0);
        lq_sq(m + 0x10, pv + 0xe0);
        jet_banking(m, m + 0x10, 0x140);
        const float phase = lwc1(pv + 0x13c);
        sb(pv + 0x131, 7);
        if (kTwoPi < phase) swc1(pv + 0x13c, fpu_mul(phase, kDegree));
        else if (lwc1(pv + 0x134) != 0.0f && phase == -1.0f) swc1(pv + 0x13c, rand_angle());
        lq_sq(pv + 0xd0, pv + 0xc0);
        path_measure_l1(fol, 0, m, lwc1(pv + 0x104), lwc1(pv + 0x108));
        if (lh(pv + 0x12c) > 0) {
            sb(m + 0x20, 3);
        } else {
            sh(pv + 0x14a, 0);
            sb(m + 0x20, 1);
        }
        break;
    }
    case 1: {
        lq_sq(old_ang, m + 0x40);
        lq_sq(old_pos, m + 0x10);
        if (1.0f <= lwc1(pv + 0x100)) {
            swc1(pv + 0x100, fpu_sub(lwc1(pv + 0x100), 1.0f));
            const int32_t next = path_index_advance_l1(fol);
            lq_sq(pv + 0x90, pv + 0xa0);
            path_spline_tangent_l1(pv + 0xa0, fol, 1, lwc1(pv + 0x104), lwc1(pv + 0x108));
            lq_sq(pv + 0xb0, pv + 0xc0);
            const int32_t after = path_index_wrap_l1(fol, next + 1);
            lq_sq(pv + 0xc0, lwu(fol + 0x10) + 0x10 + (static_cast<uint32_t>(after) << 4));
        }
        const float sway = fpu_mul(lwc1(pv + 0x134), sin_vu(lwc1(pv + 0x13c)));
        const float speed = fpu_add(lwc1(pv + 0x10c), sway);
        const float step = fpu_div(speed, lwc1(pv + 0xbc));
        swc1(pv + 0x13c, angle_add(lwc1(pv + 0x13c), fpu_mul(lwc1(pv + 0x138), kDegree)));
        lq_sq(pv + 0xe0, pv + 0xd0);
        hermite_l1(pv + 0xd0, pv + 0xb0, pv + 0xc0, pv + 0x90, pv + 0xa0, fpu_add(lwc1(pv + 0x100), step));
        if (off_screen == 0) {
            traffic_attitude_l3(m);
        } else {
            vec_clear(V(m + 0x40));
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(pv + 0xd0), lwc1(pv + 0xe0)), fpu_sub(lwc1(pv + 0xd4), lwc1(pv + 0xe4))));
        }
        swc1(pv + 0x100, fpu_add(lwc1(pv + 0x100), step));
        lq_sq(m + 0x10, pv + 0xe0);
        if (off_screen == 0) jet_banking(m, m + 0x10, 0x140);
        vec3_sub(V(move), V(m + 0x10), V(old_pos));
        lq_sq(probe, move);
        traffic_exhaust_l3(m, probe, off_screen);
        traffic_hit_l3(m, probe, off_screen);
        break;
    }
    case 2:
        if (lw(fol + 0x14) != -1) sb(m + 0x20, 0);
        sound_release(m, pv + 0x150);
        break;
    case 3:
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x12c))) != 0) sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    ee_stack_free(0xf0);
}

}  // namespace rac1
