// Batalia (level 8) own moby classes, hand-ported: class 462, a Blarg gunship (two on the level; class 444 is its twin).
// It is built of twelve pieces held at chain points (a body 0x1bd, a turret 0x1ba with its gun 0x1b9, a spinning drum
// 0x1c0 with eight pods 0x1bb around it); it flies its circuit firing shots (class 435) at random from its gun, or, sent
// along its attack path, fires at its target when asked to. Hits wear it down: every eighth breaks a pod off, and with
// no pod left it bursts apart. With its helpers: the build, the hits, the pieces kept in place. Registered in
// batalia_mobys.cpp. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (its speed 0x161d24, the drum's spin
// 0x161d28, the pause flag 0x161d30), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t kPieces = 12;  // the piece records after the gunship's own (pvars +0x70 .. +0x12f)

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

// a path's point `i` (the points from +0x10, 0x10 each)
uint32_t path_pt(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the piece record `k` (pvars +0x60 + 0x10 k: +0 the piece, +4 its parent, +8 the piece's chain point, +0xc the parent's)
uint32_t piece_rec(uint32_t pv, uint32_t k) { return pv + 0x60 + (k << 4); }

// the gunship stepped towards `goal` (EE vectors; `step` the step) by at most its speed 0x161d24 a second
void step_to(uint32_t m, uint32_t goal, uint32_t step) {
    vec3_sub(V(step), V(goal), V(m + 0x10));
    const float len = vec3_length(V(step));
    const float most = fpu_mul(lwc1(L8_ADDR(0x00161d24)), dt());
    if (most < len) vec3_set_length(V(step), V(step), most);
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
}

// the follower record `rec`'s next point (+0 the point's index, +4 the direction, a signed byte, +0x10 the path)
int32_t next_point(uint32_t rec) {
    const uint32_t path = lwu(rec + 0x10);
    const uint32_t dir = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(rec + 4))));
    return ee_rem(static_cast<int32_t>(lwu(rec) + lwu(path) + dir), lw(path));
}

// the piece of record `rec` flung off by the gunship `m`: a burst of size `burst` at it, sent off away from the point
// `from` (an EE vector) at `speed` a second (the piece's pvars +0), its state 1, the record emptied
void fling(uint32_t m, uint32_t rec, uint32_t from, float burst, float speed) {
    const uint32_t piece = lwu(rec);
    const uint32_t ppv = lwu(piece + 0x78);
    explosion_burst(m, piece + 0x10, -1, burst, 13.0f);
    vec3_sub(V(ppv), V(lwu(rec) + 0x10), V(from));
    vec3_set_length(V(ppv), V(ppv), fpu_mul(dt(), speed));
    sb(lwu(rec) + 0x20, 1);
    sw(rec, 0);
}

// the pod of record `rec` broken off: flung away from its parent
void break_off(uint32_t m, uint32_t rec) { fling(m, rec, lwu(rec + 4) + 0x10, 2.0f, 10.0f); }
}  // namespace

// The gunship `m`'s pieces spawned, each with its record (piece_rec): 0 the gunship itself; 3 the drum (0x1c0, its point
// 1 on the gunship's 0); 1 the body (0x1bd, 0 on 2; its +0x2c 0.75 of its model's +0x24); 2 the turret (0x1ba, 1 on 1);
// 4 the gun (0x1b9, 0 on the turret's 0); 5 .. 12 the pods (0x1bb, 0 on the drum's 2 .. 9).
// orig: L8:0x002e8788
void gunship_build_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x60, m);
    sw(pv + 0x64, 0);
    sw(pv + 0x68, 0);
    sw(pv + 0x6c, 0);
    const uint32_t drum = bat_piece_spawn_l8(m, 0x1c0, 1, 0);
    sw(pv + 0x94, m);
    sw(pv + 0x98, 1);
    sw(pv + 0x9c, 0);
    sw(pv + 0x90, drum);
    const uint32_t body = bat_piece_spawn_l8(m, 0x1bd, 0, 2);
    sw(pv + 0x70, body);
    sw(pv + 0x7c, 2);
    sw(pv + 0x74, m);
    sw(pv + 0x78, 0);
    swc1(body + 0x2c, fpu_mul(lwc1(lwu(body + 0x24) + 0x24), 0.75f));  // (no test for a body not spawned)
    const uint32_t turret = bat_piece_spawn_l8(m, 0x1ba, 1, 1);
    sw(pv + 0x84, m);
    sw(pv + 0x8c, 1);
    sw(pv + 0x88, 1);
    sw(pv + 0x80, turret);
    const uint32_t gun = bat_piece_spawn_l8(turret, 0x1b9, 0, 0);
    sw(pv + 0xa0, gun);
    sw(pv + 0xa4, lwu(pv + 0x80));
    sw(pv + 0xa8, 0);
    sw(pv + 0xac, 0);
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t rec = piece_rec(pv, 5 + i);
        const uint32_t pod = bat_piece_spawn_l8(lwu(pv + 0x90), 0x1bb, 0, i + 2);
        sw(rec, pod);
        sw(rec + 0xc, i + 2);
        sw(rec + 4, lwu(pv + 0x90));
        sw(rec + 8, 0);
    }
}

// The gunship `m`'s hits (mask 0x10000 on itself and each piece; each piece hit has its +0xa4 0xff): the hull's tally
// +0x13c up by 1 for a hit on records 0 .. 2, 2 for 3 .. 4 (the larger only), 4 more for a pod (the first pod hit), the
// sound 1 (2 for a pod). At 8 or more the tally keeps its remainder by 8 and a pod breaks off (the pod hit, else one
// from a random place on; no sound then); with no pod left the gunship is destroyed (state 99, the sound).
// orig: L8:0x002e88d8
void gunship_hits_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t tally = 0;
    int32_t sound = -1;
    uint32_t pod_hit = 0;
    for (uint32_t k = 0; k < 13; ++k) {
        const uint32_t rec = piece_rec(pv, k);
        if (lwu(rec) == 0 || sub_0025a420(lwu(rec), 0x10000, 0) == 0) continue;
        if (k < 3) {
            if (tally <= 0) {
                tally = 1;
                sound = 1;
            }
        } else if (k < 5) {
            if (tally < 2) {
                tally = 2;
                sound = 1;
            }
        } else if (tally < 4) {
            tally += 4;
            pod_hit = k;
            sound = 2;
        }
        sb(lwu(rec) + 0xa4, 0xff);
    }
    const int16_t hits = static_cast<int16_t>(lhu(pv + 0x13c) + static_cast<uint32_t>(tally));
    sh(pv + 0x13c, static_cast<uint32_t>(hits));
    if (hits >= 8) {
        sh(pv + 0x13c, static_cast<uint32_t>(hits % 8));
        if (pod_hit != 0) {
            break_off(m, piece_rec(pv, pod_hit));
            return;
        }
        const int32_t from = float_to_int(rand_range_f(0.0f, 23.0f));
        for (int32_t i = 0; i < 8; ++i) {
            const uint32_t rec = piece_rec(pv, static_cast<uint32_t>((i + from) % 8 + 5));
            if (lwu(rec) != 0) {
                break_off(m, rec);
                return;
            }
        }
        sb(m + 0x20, 0x63);
    }
    if (sound >= 0) moby_sound_start(sound, 0, m);
}

// The gunship `m`'s pieces (records 1 .. 12) kept on their parents' chain points (each moved by the gap between its
// point and its parent's, turned as the gunship), the drum spun (+0x40) by 0x161d28 degrees a second, and each piece's
// transform updated.
// orig: L8:0x002e8ba0
void gunship_pieces_follow_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(192);
    const uint32_t at = sp, want = sp + 0x10, gap = sp + 0x20;
    for (uint32_t k = 1; k <= kPieces; ++k) {
        const uint32_t rec = piece_rec(pv, k);
        const uint32_t piece = lwu(rec);
        if (piece == 0) continue;
        swc1(piece + 0x48, lwc1(m + 0x48));
        moby_chain_point_v(piece, lwu(rec + 8), V(at));
        moby_chain_point_v(lwu(rec + 4), lwu(rec + 0xc), V(want));
        vec3_sub(V(gap), V(want), V(at));
        vec3_add(V(piece + 0x10), V(piece + 0x10), V(gap));
    }
    if (lwu(pv + 0x90) != 0) {
        const float spin = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161d28)), u2f(0x3c8efa35u)), dt());  // (degrees to radians)
        const float roll = angle_add(lwc1(lwu(pv + 0x90) + 0x40), spin);
        swc1(lwu(pv + 0x90) + 0x40, roll);
    }
    for (uint32_t k = 1; k <= kPieces; ++k) {
        const uint32_t piece = lwu(piece_rec(pv, k));
        if (piece != 0) moby_update_transform(piece);
    }
    ee_stack_free(192);
}

// Class 462, a Blarg gunship. pvars: +0x00 its attack run (a follower: +0x00 the point, +0x04 the direction, +0x10 the
// path, its number +0x14), +0x30 its circuit (the same: +0x30, +0x34, +0x40, the path number +0x44), +0x60 .. +0x12f
// its thirteen piece records (gunship_build_l8), +0x130 its target's moby index, +0x134 its turn rate, +0x138 a jump
// allowed (non-zero), +0x13a its shot timer (300 .. 600 frames), +0x13c its hits' tally, +0x13e its engine sound's slot.
// With 0x161d30 set nothing runs. The engine sound (3) restarted whenever it stopped. States: 0 set up (gone if its byte
// +0xb0 of the level's flags at 0x14c050 is 0xff): its pieces built on its circuit's first point, facing the next one,
// state 2; 1 the attack run: turns to its next point (0.004 / 0.3 / 0.02), its hits, its pieces, steps on (within 0.5
// the next point); asked to fire (+0xbc, cleared): a shot (class 435) from the gun's point 1, at the target, launched
// away from it (its heading plus pi plus -15 .. 45 degrees, at least 0.34 off 0; climbing 10 .. 30 degrees), put in the
// target's first free slot of its eight +0x60 .. +0x7c; 2 the circuit: its heading eased between the legs' headings by
// how far along it is, steps on, its pieces; with 0x15f5c4 2, +0x138 set and the double word 0x16d090 0x2_00000000 it
// first jumps to the point 15 facing the point 11; on its shot timer a shot (state 5, no target) from the gun's point
// 1, in a random direction, diving 15 .. 30 degrees; 3 back on its attack run's first point, facing the next, state 1;
// 99 destroyed: its pieces flung off with bursts, a burst at it, and gone. The others: nothing.
// orig: L8:0x002e8cd0
void bat_class462_update_l8(uint32_t m) {
    if (lw(L8_ADDR(0x00161d30)) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (sub_0028d8c0(m, lh(pv + 0x13e)) == 0) sh(pv + 0x13e, static_cast<uint32_t>(moby_sound_start(3, 4, m)));

    const uint32_t sp = ee_stack_alloc(240);
    const uint32_t goal = sp, step = sp + 0x10, muzzle = sp + 0x20, vel = sp + 0x30;
    const uint32_t pos = m + 0x10;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002e8d80) set up
        if (lbu(0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) {
            moby_retire(m);
            break;
        }
        sw(pv + 0x10, lvd::path(lw(pv + 0x14)));
        sw(pv + 0x40, lvd::path(lw(pv + 0x44)));
        sh(pv + 0x13a, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(300.0f, 600.0f)))));
        sh(m + 0x34, lhu(m + 0x34) | 0x4000u);
        lq_sq(pos, lwu(pv + 0x40) + 0x10);
        gunship_build_l8(m);
        sb(m + 0x20, 2);
        const uint32_t p1 = path_pt(lwu(pv + 0x40), 1);
        swc1(m + 0x48, heading(lwc1(pos), lwc1(pos + 4), lwc1(p1), lwc1(p1 + 4)));
        break;
    }
    case 1: {  // (0x002e8e68) the attack run
        const int32_t next = next_point(pv);
        const uint32_t target = lwu(L8_ADDR(0x0015ffd8)) + (lwu(pv + 0x130) << 8);
        const uint32_t p = path_pt(lwu(pv + 0x10), next);
        yaw_turn_damped(m, pv + 0x134, heading(lwc1(pos), lwc1(pos + 4), lwc1(p), lwc1(p + 4)), 0.004f, 0.3f, 0.02f);
        gunship_hits_l8(m);
        gunship_pieces_follow_l8(m);
        lq_sq(goal, path_pt(lwu(pv + 0x10), next));
        if (vec3_distance(V(pos), V(goal)) < 0.5f) sw(pv, static_cast<uint32_t>(next));
        step_to(m, goal, step);
        if (lbu(m + 0xbc) == 0) break;
        // asked to fire at its target
        sb(m + 0xbc, 0);
        moby_chain_point_v(lwu(pv + 0xa0), 1, V(muzzle));
        const float climb = rand_range_f(u2f(0x3e32b8c2u), u2f(0x3f060a92u));  // (10 .. 30 degrees)
        const float away = angle_add(heading(lwc1(pos), lwc1(pos + 4), lwc1(target + 0x10), lwc1(target + 0x14)),
                                     u2f(0x40490fdbu));  // (+ pi)
        float yaw = angle_add(away, rand_range_f(u2f(0xbe860a92u), u2f(0x3f490fdbu)));  // (-15 .. 45 degrees)
        if (0.0f < yaw) {
            if (yaw < 0.34f) yaw = 0.34f;
        } else if (yaw <= 0.0f) {
            if (-0.34f < yaw) yaw = -0.34f;
        }
        const float speed = lwc1(L8_ADDR(0x00161d24));
        vec3_set_length(V(vel), V(step), fpu_mul(fpu_add(speed, speed), dt()));
        const uint32_t shot = bat_class435_spawn_l8(muzzle, target, vel, climb, yaw);
        // (no test for a shot not spawned)
        swc1(shot + 0x2c, fpu_div(lwc1(lwu(shot + 0x24) + 0x24), 5.0f));
        const uint32_t tpv = lwu(target + 0x78);
        for (uint32_t i = 0; i < 8; ++i) {
            if (lwu(tpv + 0x60 + i * 4) == 0) {
                sw(tpv + 0x60 + i * 4, shot);
                break;
            }
        }
        break;
    }
    case 2: {  // (0x002e9154) the circuit
        const uint32_t rec = pv + 0x30;
        if (lw(L8_ADDR(0x0015f5c4)) == 2 && lh(pv + 0x138) != 0 && ld(L8_ADDR(0x0016d090)) == 0x200000000ull) {
            lq_sq(pos, path_pt(lwu(rec + 0x10), 15));
            sw(pv + 0x30, 15);
            const uint32_t p11 = path_pt(lwu(rec + 0x10), 11);
            swc1(m + 0x48, heading(lwc1(pos), lwc1(pos + 4), lwc1(p11), lwc1(p11 + 4)));
        }
        const uint32_t path = lwu(rec + 0x10);
        const int32_t at = lw(rec);
        const int32_t n = lw(path);
        const int32_t next = ee_rem(at + 1, n);
        const int32_t after = ee_rem(at + 2, n);
        const uint32_t p0 = path_pt(path, at), p1 = path_pt(path, next), p2 = path_pt(path, after);
        const float leg = heading(lwc1(p0), lwc1(p0 + 4), lwc1(p1), lwc1(p1 + 4));
        const float next_leg = heading(lwc1(p1), lwc1(p1 + 4), lwc1(p2), lwc1(p2 + 4));
        const float turn = angle_sub(leg, next_leg);
        const float share = fpu_div(vec3_distance(V(pos), V(p1)), vec3_distance(V(p0), V(p1)));
        swc1(m + 0x48, angle_add(fpu_mul(turn, share), next_leg));
        lq_sq(goal, path_pt(lwu(rec + 0x10), next));
        if (vec3_distance(V(pos), V(goal)) < 0.5f) sw(rec, static_cast<uint32_t>(next));
        step_to(m, goal, step);
        gunship_pieces_follow_l8(m);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x13a))) == 0) break;
        // a shot at random
        sh(pv + 0x13a, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(300.0f, 600.0f)))));
        moby_chain_point_v(lwu(pv + 0xa0), 1, V(muzzle));
        const float dive = rand_range_f(u2f(0xbf060a92u), u2f(0xbe860a92u));  // (-30 .. -15 degrees)
        const float yaw = rand_angle();
        const float speed = lwc1(L8_ADDR(0x00161d24));
        vec3_set_length(V(vel), V(step), fpu_mul(fpu_add(speed, speed), dt()));
        const uint32_t shot = bat_class435_spawn_l8(muzzle, 0, vel, dive, yaw);
        if (shot == 0) break;
        const float gun_yaw = lwc1(lwu(pv + 0xa0) + 0x48);
        sb(shot + 0x20, 5);
        swc1(shot + 0x48, gun_yaw);
        sw(shot + 0x44, 0);
        break;
    }
    case 3: {  // (0x002e9420) back to the attack run
        sb(m + 0x20, 1);
        const uint32_t path = lwu(pv + 0x10);
        lq_sq(pos, path + 0x10);
        const uint32_t p1 = path_pt(path, 1);
        swc1(m + 0x48, heading(lwc1(pos), lwc1(pos + 4), lwc1(p1), lwc1(p1 + 4)));
        gunship_pieces_follow_l8(m);
        break;
    }
    case 0x63:  // (0x002e946c) destroyed
        for (uint32_t k = 1; k <= kPieces; ++k) {
            const uint32_t rec = piece_rec(pv, k);
            if (lwu(rec) != 0) fling(m, rec, pos, 3.0f, 5.0f);
        }
        explosion_burst(m, pos, -1, 5.0f, 13.0f);
        moby_retire(m);
        break;
    default:
        break;
    }
    ee_stack_free(240);
}

}  // namespace rac1
