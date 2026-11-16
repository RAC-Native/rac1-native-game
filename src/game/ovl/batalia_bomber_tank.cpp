// Batalia (level 8) own moby classes, hand-ported: class 253, the Bomber Tank (the Blarg tanks that drive along the fort's
// roads lobbing bombs at Ratchet; two of them come in through a wall, which bursts into pieces the first time), with its
// frame (its hits, its wreck and its end, its aim), its drive along its route, its smoke while wrecked, the wall it breaks
// and the bomb it fires (class 425). Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (the class's tuning at
// 0x1618a0 .. 0x1618fc), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
const float kDegree = u2f(0x3c8efa35u);         // pi / 180
const float kPi = u2f(0x40490fdbu);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kTwelfthPi = u2f(0x3e860a92u);      // 15 degrees
const float kQuarterPi = u2f(0x3f490fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(uint32_t i) { return lwu(L8_ADDR(0x0015ffd8)) + (i << 8); }

// the animation set `set` over `frames` (a raw count), unless it plays
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
}

// the tank's two tracks (pvars +0x160 / +0x164; 0 none) retired if there, then the tank
void retire_with_tracks(uint32_t m, uint32_t pv) {
    if (lwu(pv + 0x160) != 0) moby_retire(lwu(pv + 0x160));
    if (lwu(pv + 0x164) != 0) moby_retire(lwu(pv + 0x164));
    moby_retire(m);
}

// the tank's five debris pieces `cls[0..4]` thrown from its place
void tank_debris(uint32_t m, const uint32_t (&cls)[5]) {
    const uint32_t z = L8_ADDR(0x0015f580);
    for (uint32_t c : cls) debris_spawn(m, c, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
}

// the sound slot `slot`'s volume (+0x80 of its record)
int32_t slot_volume(int32_t slot) { return lw(0x0013e550u + static_cast<uint32_t>(slot * 0x70) + 0x80); }
}  // namespace

// The tank's frame before its state. A hit (mask 0x330000 on the tank, 0x210000 on each track; the largest of the three
// records; none from its own bombs, class 425, or from class 71) in any state but 0: its health +0x20 down by the damage
// (the sound 4 for any damage); alive, a flinch (the pulse's strength +0xa7 by the damage, its timer +0x26 30 frames, 60
// from the classes 176 / 177; in state 12 the animation 1); at 0 the first time a wreck (state 0x63: an explosion 3 above
// it, its debris 0x6c0 .. 0x6c4, the animation 1, the health 6, the flag 2 of +0x3e off), the second time its end: its
// bolts, the sound 2, an explosion 1 above it, its debris 0x6fe / 0x6ff, the tank and its tracks retired (on Batalia,
// with the hit from class 1633: its number +0x184's mark 0x15f5b0 .. 0x15f5bc set, and once the first two are marked the
// skill point 0x13d415, its jingle and the text 0x53d6). Then its colour pulse +0xa0 started (any hit), the three drawn
// (+0xa4), its target picked (+0xb0) in its area (the path +0x154), the hero's moby without one, the turret's yaw +0x170
// and pitch +0x178 seeking the target 3 lower (within 20 degrees up and 45 down; back to 0 when the target's mode +0xf4
// is 2), the turret's rotation +0x110 built from them and the pulse run for the tank and its tracks.
// orig: L8:0x002d4ca0
void bomber_tank_frame_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t at = sp + 0x20, hit_state = sp + 0x30, hit_damage = sp + 0x34;
    sw(hit_damage, 0);
    const uint32_t own = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    const uint32_t on_left = lwu(pv + 0x160) != 0 ? sub_0025a420(lwu(pv + 0x160), NOT_ADDR(0x210000), 0) : 0;
    const uint32_t on_right = lwu(pv + 0x164) != 0 ? sub_0025a420(lwu(pv + 0x164), NOT_ADDR(0x210000), 0) : 0;
    uint32_t hit = moby_largest(own, on_left, on_right, 0);
    if (hit != 0 && lwu(hit + 0x20) != 0) {
        const int32_t cls = lh(lwu(hit + 0x20) + 0xa6);
        if (cls == 0x1a9 || cls == 0x47) hit = 0;
    }
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_damage);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_damage, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0) {
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(hit_damage)));
        if (!(lwc1(hit_damage) == 0.0f)) moby_sound_start(4, 0, m);
        if (0.0f < lwc1(pv + 0x20)) {
            // a flinch
            const float dmg = lwc1(hit_damage);
            int32_t frames = 0x1e;
            if (dmg < 4.0f) {
                sb(pv + 0xa7, 0x78);
                // (the hit record's source read even without a record, as the original does)
                if (static_cast<uint32_t>(lhu(lwu(hit + 0x20) + 0xa6) - 0xb0u) < 2u) frames = 0x3c;
            } else {
                sb(pv + 0xa7, dmg < 6.0f ? 0xc8 : 0xe6);
            }
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(frames)));
            if (lbu(m + 0x20) == 0xc) anim_to(m, 1, 0);
        } else if (lbu(m + 0x20) != 0x63) {
            // wrecked
            lq_sq(at, m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), 3.0f));
            sb(pv + 0xa7, 0xfa);
            vec_clear(V(pv + 0x120));
            explosion_effect(m, pv + 0x40, at, 0x14, 8, 0x14, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            tank_debris(m, {0x6c0, 0x6c1, 0x6c2, 0x6c3, 0x6c4});
            anim_to(m, 1, 3);
            sb(m + 0x20, 0x63);
            swc1(pv + 0x20, 6.0f);
            sh(pv + 0x3e, lhu(pv + 0x3e) & 0xfffdu);
        } else {
            // its end
            if (lw(0x0015ed84) == 8) {  // (on Batalia: the skill point's marks)
                const int32_t which = lw(pv + 0x184);
                if (which != 0 && hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == 0x661) {
                    if (which == 1) sw(L8_ADDR(0x0015f5b0), 1);
                    if (which == 2) sw(L8_ADDR(0x0015f5b4), 1);
                    if (which == 3) sw(L8_ADDR(0x0015f5b8), 1);
                    if (which == 4) sw(L8_ADDR(0x0015f5bc), 1);
                    // (only the first two marks are tested)
                    if (lw(L8_ADDR(0x0015f5b0)) != 0 && lw(L8_ADDR(0x0015f5b4)) != 0 && lbu(0x0013d415) == 0) {
                        sb(0x0013d415, 1);
                        sound_play_indexed(1, 0, 0);
                        sub_00263d40(TXT(0x53d6), -1);
                    }
                }
            }
            lq_sq(at, m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
            moby_drop_bolts(m, 0, -1);
            moby_sound_start(2, 0, m);
            explosion_effect(m, pv + 0x40, at, 0x14, 8, 0x14, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            tank_debris(m, {0x6fe, 0x6fe, 0x6ff, 0x6ff, 0x6ff});
            retire_with_tracks(m, pv);
            ee_stack_free(0xf0);
            return;
        }
        color_pulse_start(m, pv + 0xa0);
    }
    sb(m + 0xa4, 0xff);
    if (lwu(pv + 0x160) != 0) sb(lwu(pv + 0x160) + 0xa4, 0xff);
    if (lwu(pv + 0x164) != 0) sb(lwu(pv + 0x164) + 0xa4, 0xff);
    const uint32_t area = lvd::path(lw(pv + 0x154));
    target_select(m, pv + 0xb0, 0, 0, area + 0x10, lw(area), 100.0f);
    if (lw(pv + 0xf0) == 0) sw(pv + 0xf0, lwu(0x001413d0));
    // the turret's aim
    const float accel = fpu_mul(dt_sq(), u2f(0x41490fdbu));  // 4 pi
    const float rate = fpu_mul(dt(), u2f(0x4116cbe4u));      // 3 pi
    if (lw(pv + 0xf4) != 2) {
        vec3_sub(V(at), V(pv + 0xb0), V(m + 0x10));
        swc1(at + 8, fpu_sub(lwc1(at + 8), 3.0f));
        const float heading = atan2_f(lwc1(at), lwc1(at + 4));
        float pitch = -atan2_f(vec2_length(V(at)), lwc1(at + 8));
        const float up = u2f(0x3eb2b8c2u);  // 20 degrees
        if (up < pitch) pitch = up;
        else if (pitch < -kQuarterPi) pitch = -kQuarterPi;
        yaw_seek(pv + 0x170, pv + 0x174, angle_sub(heading, lwc1(m + 0x48)), accel, accel, rate);
        yaw_seek(pv + 0x178, pv + 0x17c, pitch, accel, accel, rate);
    } else {
        yaw_seek(pv + 0x170, pv + 0x174, 0.0f, accel, accel, rate);
        yaw_seek(pv + 0x178, pv + 0x17c, 0.0f, accel, accel, rate);
    }
    quat_from_axis_angle(V(at), 1, lwc1(pv + 0x178));
    quat_from_axis_angle(V(pv + 0x110), 2, lwc1(pv + 0x170));
    quat_mul(V(pv + 0x110), V(at), V(pv + 0x110));
    // the colour pulse: the tank's, then each track's from the same start
    const uint32_t pulse0 = lhu(pv + 0xa0), pulse1 = lhu(pv + 0xa2);
    color_pulse_update(m, pv + 0xa0);
    if (lwu(pv + 0x160) != 0) {
        sh(pv + 0xa0, pulse0);
        sh(pv + 0xa2, pulse1);
        color_pulse_update(lwu(pv + 0x160), pv + 0xa0);
    }
    if (lwu(pv + 0x164) != 0) {
        sh(pv + 0xa2, pulse1);
        sh(pv + 0xa0, pulse0);
        color_pulse_update(lwu(pv + 0x164), pv + 0xa0);
    }
    ee_stack_free(0xf0);
}

// The tank driving along its route `route` (a path): heading for its point +0x168 (3/2 pi a second squared, at most pi a
// second, the rate +0x158), its speed +0x180 approaching its top speed +0x16c, on the floor; within 0.5 of the point the
// next one (looping). Its tracks put with it, their animation's speed its speed against 0x1618a0 a second (0 .. 2). While
// it moves (its velocity +0x40 over 0.1 a frame) its engine sound (1, slot +0x15c) started or turned up (by 1024 over 60
// frames up to 1024), else turned down and stopped below 256.
// orig: L8:0x002d55c8
void bomber_tank_drive_l8(uint32_t m, int32_t route) {
    const uint32_t path = lvd::path(route);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t goal = sp, step = sp + 0x10;
    const uint32_t pos = m + 0x10;
    lq_sq(goal, path + 0x10 + (lwu(pv + 0x168) << 4));
    const float accel = fpu_mul(dt_sq(), u2f(0x4096cbe4u));  // 3 pi / 2
    yaw_seek(m + 0x48, pv + 0x158, atan2_f(fpu_sub(lwc1(goal), lwc1(pos)), fpu_sub(lwc1(goal + 4), lwc1(pos + 4))), accel,
             accel, fpu_mul(dt(), kPi));
    approach_f(pv + 0x180, lwc1(pv + 0x16c), fpu_add(dt_sq(), dt_sq()));
    swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x180)));
    swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x180)));
    sw(step + 8, 0);
    vec3_add(V(pos), V(pos), V(step));
    swc1(m + 0x18, floor_probe_v(0.5f, V(pos), 0));
    if (vec2_distance(V(pos), V(goal)) < 0.5f) sw(pv + 0x168, static_cast<uint32_t>(ee_rem(lw(pv + 0x168) + 1, lw(path))));
    float track_speed = fpu_div(lwc1(pv + 0x180), fpu_mul(lwc1(L8_ADDR(0x001618a0)), dt()));
    if (2.0f < track_speed) track_speed = 2.0f;
    else if (track_speed < 0.0f) track_speed = 0.0f;
    for (uint32_t slot : {0x160u, 0x164u}) {
        const uint32_t track = lwu(pv + slot);
        if (track == 0) continue;
        swc1(track + 0x58, track_speed);
        lq_sq(track + 0x40, m + 0x40);
        lq_sq(track + 0x10, pos);
    }
    // the engine
    if (fpu_mul(dt(), u2f(0x3dcccccdu)) < vec3_length(V(pv + 0x40))) {  // (0.1)
        if (sub_0028d8c0(m, lw(pv + 0x15c)) == 0) {
            const int32_t slot = moby_sound_start(1, 4, m);
            sw(pv + 0x15c, static_cast<uint32_t>(slot));
            sound_slot_volume_set(slot, 1);
        } else if (slot_volume(lw(pv + 0x15c)) < 0x400) {
            const int32_t step_up = ee_div(0x400, frame_scale_i(0x3c));
            const int32_t slot = lw(pv + 0x15c);
            sound_slot_volume_set(slot, static_cast<uint32_t>(slot_volume(slot) + step_up));
        }
    } else if (sub_0028d8c0(m, lw(pv + 0x15c)) != 0) {
        const int32_t slot = lw(pv + 0x15c);
        if (slot_volume(slot) < 0x100) {
            sub_0028d918(slot);
            sw(pv + 0x15c, 0xffffffffu);
        } else {
            const int32_t step_down = ee_div(0x400, frame_scale_i(0x3c));
            sound_slot_volume_set(slot, static_cast<uint32_t>(slot_volume(slot) - step_down));
        }
    }
    ee_stack_free(0x90);
}

// The wrecked tank's smoke: `count` puffs (particle 2) from its joint `joint`, along the joint's third axis at
// 0x1618dc a second (the first third of them up to that, the rest 1 .. 1.2 times it), drifting at half its velocity +0x40
// and sideways at random (up to 0x1618e8 a second, a quarter of it for the first third), rising 0x1618e0 a second; their
// times 0x1618ec / 0x1618f0 (1 .. 1.2 times) and 0x1618f4 (1 .. 1.5 times) frames, their colours 0x1618d4 / 0x1618d8,
// the w lanes 0x1618f8 / 0x1618fc. (Each puff's place is also jittered by 0.05 at random and then not used: the random
// draws stay.)
// orig: L8:0x002d5950
void bomber_tank_smoke_l8(uint32_t m, uint32_t joint, int32_t count) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x190);
    const uint32_t frame = sp + 0x10, axis = sp + 0x30, at = sp + 0x50, vel = sp + 0x60, drift = sp + 0x70,
                   push = sp + 0x80, jitter = sp + 0x90;
    moby_chain_point_v(m, joint, V(at));
    moby_chain_world_one(m, joint, frame);
    const float jit = u2f(0x3d4ccccdu);  // 0.05
    const float more = u2f(0x3f99999au);  // 1.2
    for (int32_t i = 0; i < count;) {
        const float a = rand_angle();
        lq_sq(jitter, at);
        for (uint32_t lane = 0; lane < 12; lane += 4) {
            const float r = rand_range_f(-jit, jit);
            swc1(jitter + lane, fpu_add(lwc1(jitter + lane), r));
        }
        lq_sq(vel, pv + 0x40);
        lq_sq(drift, pv + 0x40);
        const bool late = count / 3 < i;
        const float base = lwc1(L8_ADDR(0x001618dc));
        const float speed = late ? rand_range_f(base, fpu_mul(base, more)) : rand_range_f(0.0f, base);
        vec3_set_length(V(push), V(axis), fpu_mul(speed, dt()));
        vec3_add(V(vel), V(vel), V(push));
        vec3_scale(V(drift), V(drift), 0.5f);
        auto spread = [&] {
            const float s = lwc1(L8_ADDR(0x001618e8));
            return late ? s : fpu_mul(s, 0.25f);
        };
        const float c = cos_vu(a);
        const float side_x = rand_range_f(0.0f, spread());
        swc1(drift, fpu_add(lwc1(drift), fpu_mul(c, fpu_mul(side_x, dt()))));
        const float s = sin_vu(a);
        ++i;
        const float side_y = rand_range_f(0.0f, spread());
        swc1(drift + 8, fpu_mul(lwc1(L8_ADDR(0x001618e0)), dt()));
        swc1(drift + 4, fpu_add(lwc1(drift + 4), fpu_mul(s, fpu_mul(side_y, dt()))));
        swc1(drift + 0xc, lwc1(L8_ADDR(0x001618fc)));
        swc1(vel + 0xc, lwc1(L8_ADDR(0x001618f8)));
        const float t0 = int_to_float(lw(L8_ADDR(0x001618ec)));
        const int32_t n0 = float_to_int(frame_scale_f(rand_range_f(t0, fpu_mul(t0, more))));
        const float t1 = int_to_float(lw(L8_ADDR(0x001618f0)));
        const int32_t n1 = float_to_int(frame_scale_f(rand_range_f(t1, fpu_mul(t1, more))));
        const float t2 = int_to_float(lw(L8_ADDR(0x001618f4)));
        const int32_t n2 = float_to_int(frame_scale_f(rand_range_f(t2, fpu_mul(t2, 1.5f))));
        spawn_particle_02(at, vel, drift, lwu(L8_ADDR(0x001618d4)), lwu(L8_ADDR(0x001618d8)), static_cast<uint32_t>(n0),
                          static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), -1);
    }
    ee_stack_free(0x190);
}

// The tank breaking through its wall (the moby +0x144; its group, +0x21 of it): with 50 moby slots free at least, up to
// 40 rubble pieces (class 784 or 785 at random, while 50 stay free) thrown from random points of the box +0x148, out
// within 15 degrees of the x axis at 15 a second and up at 8 .. 12, for 60 .. 90 frames; then each of the wall's live
// mobys (state below 0x7f, not the tank): its pieces (classes 588 .. 598) thrown the same way from their place on the
// wall (each piece's offset), for 10 .. 20 frames when thrown toward the camera (within 15 degrees), else 30 .. 45; its
// breakable part (class 472) blown up and retired.
// orig: L8:0x002d5d08
void bomber_tank_break_wall_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t list = lvd::group(lbu(table_moby(lwu(pv + 0x144)) + 0x21));
    if (list == 0) return;
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t vel = sp + 0x20, offset = sp + 0x30, rubble_vel = sp + 0x40;
    for (int32_t k = 0; k < 40 && lw(L8_ADDR(0x0015ffbc)) >= 50; ++k) {
        const uint32_t piece = moby_spawn((rand_mod(0xff) & 1) != 0 ? 0x310 : 0x311);
        if (piece == 0) continue;
        sb(piece + 0x30, 0xff);
        sh(piece + 0x32, 0x40);
        sb(piece + 0x31, 1);
        swc1(piece + 0x40, rand_angle());
        swc1(piece + 0x44, rand_angle());
        swc1(piece + 0x48, rand_angle());
        for (uint32_t lane = 0; lane < 12; lane += 4) swc1(vel + lane, rand_range_f(-1.0f, 1.0f));
        swc1(vel + 0xc, 1.0f);
        mat4_transform(V(vel), V(vel), M(lvd::box(lvd::BOXES, lw(pv + 0x148))));
        lq_sq(piece + 0x10, vel);
        const float a = rand_range_f(-kTwelfthPi, kTwelfthPi);
        vec_clear(V(offset));
        swc1(rubble_vel, fpu_mul(cos_vu(a), fpu_mul(dt(), 15.0f)));
        swc1(rubble_vel + 4, fpu_mul(sin_vu(a), fpu_mul(dt(), 15.0f)));
        swc1(rubble_vel + 8, rand_range_f(fpu_mul(dt(), 8.0f), fpu_mul(dt(), 12.0f)));
        class344_throw(piece, m, rubble_vel, offset, float_to_int(rand_range_f(60.0f, 90.0f)));
    }
    for (uint32_t e = list;; e += 2) {
        const uint32_t piece = table_moby(lhu(e) & 0x7fffu);
        if (lbu(piece + 0x20) < 0x7f && piece != m) {
            const uint32_t cls = lhu(piece + 0xa6);
            if (cls - 0x24cu < 11u) {
                const float a = rand_range_f(-kTwelfthPi, kTwelfthPi);
                swc1(vel, fpu_mul(cos_vu(a), fpu_mul(dt(), 15.0f)));
                swc1(vel + 4, fpu_mul(sin_vu(a), fpu_mul(dt(), 15.0f)));
                swc1(vel + 8, rand_range_f(fpu_mul(dt(), 8.0f), fpu_mul(dt(), 12.0f)));
                // the piece's place on the wall (the table 0x1fbce0: one case a class, 588 .. 598)
                uint32_t x = 0, y = 0, z = 0;
                switch (cls - 0x24cu) {
                case 0: x = 0x3fd08312u, y = 0xbfaba5e3u, z = 0x4137ae14u; break;   // (0x002d604c) 1.629, -1.341, 11.48
                case 1: x = 0x3fa645a2u, y = 0xc0b1db23u, z = 0x4137ae14u; break;   // (0x002d607c) 1.299, -5.558, 11.48
                case 2: x = 0x3f210625u, y = 0xc021ba5eu, z = 0x405a2d0eu; break;   // (0x002d60b4) 0.629, -2.527, 3.409
                case 3: x = 0x3c9ba5e3u, y = 0xc0c6147bu, z = 0x405a2d0eu; break;   // (0x002d60e4) 0.019, -6.19, 3.409
                case 4: x = 0x404c8b44u, y = 0xc006b852u, z = 0x40f91687u; break;   // (0x002d611c) 3.196, -2.105, 7.784
                case 5: x = 0x40070a3du, y = 0xc040d4feu, z = 0x40280000u; break;   // (0x002d614c) 2.11, -3.013, 2.625
                case 6: x = 0x408a8f5cu, y = 0xc0ce5e35u, z = 0x40ecb439u; break;   // (0x002d6180) 4.33, -6.449, 7.397
                case 7: x = 0x40524dd3u, y = 0xc0dd999au, z = 0x403d70a4u; break;   // (0x002d61b0) 3.286, -6.925, 2.96
                case 8: x = 0x3fc4fdf4u, y = 0xbf891687u, z = 0x41524dd3u; break;   // (0x002d61e8) 1.539, -1.071, 13.144
                case 9: x = 0x3fcc49bau, y = 0xc020c49cu, z = 0x4105be77u; break;   // (0x002d6218) 1.596, -2.512, 8.359
                default: x = 0x3f628f5cu, y = 0xc0ba5e35u, z = 0x41039581u; break;  // (0x002d6250, 10) 0.885, -5.824, 8.224
                }
                sw(rubble_vel, x);
                sw(rubble_vel + 4, y);
                sw(rubble_vel + 8, z);
                sw(rubble_vel + 0xc, 0);
                lq_sq(offset, rubble_vel);
                const float out = atan2_f(lwc1(vel), lwc1(vel + 4));
                const float to_camera = atan2_f(fpu_sub(lwc1(L8_ADDR(0x001675c0)), lwc1(m + 0x10)),
                                                fpu_sub(lwc1(L8_ADDR(0x001675c4)), lwc1(m + 0x14)));
                const float life = angle_dist(out, to_camera) < kTwelfthPi ? rand_range_f(10.0f, 20.0f) : rand_range_f(30.0f, 45.0f);
                class344_throw(piece, m, vel, offset, float_to_int(life));
            } else if (static_cast<int16_t>(cls) == 0x1d8) {
                vec_clear(V(vel));
                explosion_effect(piece, vel, piece + 0x10, 0x32, 0x3c, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 12.0f, 8.0f, 9.0f, 1.0f,
                                 15.0f);
                moby_retire(piece);
            }
        }
        if (lh(e) < -1) break;  // (the list's last entry: bit 15 set, and not 0xffff)
    }
    ee_stack_free(0x110);
}

// The wall already broken (on a later visit): its live pieces (classes 588 .. 598) and its breakable part (class 472)
// retired, the tank excepted.
// orig: L8:0x002d6450
void bomber_tank_wall_gone_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t list = lvd::group(lbu(table_moby(lwu(pv + 0x144)) + 0x21));
    if (list == 0) return;
    for (uint32_t e = list;; e += 2) {
        const uint32_t piece = table_moby(lhu(e) & 0x7fffu);
        if (lbu(piece + 0x20) < 0x7f && piece != m) {
            const uint32_t cls = lhu(piece + 0xa6);
            if (cls - 0x24cu < 11u || static_cast<int16_t>(cls) == 0x1d8) moby_retire(piece);
        }
        if (lh(e) < -1) break;  // (as in the wall's break)
    }
}

// The tank `m`'s bomb (class 425) at `at` with the velocity `vel` (EE addresses; its pvars +0): drawn, state 1, the tank
// its owner (+0x20), 300 frames to live (+0x10), +0x1c 1, +0x14 0, a random roll, its yaw a quarter turn from its
// velocity's heading. 0 when no moby is free.
// orig: L8:0x002dc8a0
uint32_t bomber_tank_bomb_l8(uint32_t m, uint32_t at, uint32_t vel) {
    const uint32_t b = moby_spawn(0x1a9);
    if (b == 0) return 0;
    const uint32_t bp = lwu(b + 0x78);
    sh(b + 0x32, 0xff);
    sb(b + 0x30, 0xff);
    sb(b + 0x20, 1);
    sb(b + 0x31, 1);
    sw(bp + 0x20, m);
    sw(bp + 0x10, static_cast<uint32_t>(frame_scale_i(0x12c)));
    swc1(bp + 0x1c, 1.0f);
    sw(bp + 0x14, 0);
    lq_sq(b + 0x10, at);
    lq_sq(bp, vel);
    swc1(b + 0x40, fpu_mul(rand_range_f(-180.0f, 180.0f), kDegree));
    swc1(b + 0x48, fpu_add(atan2_f(lwc1(vel), lwc1(vel + 4)), kHalfPi));
    moby_update_transform(b);
    return b;
}

// Class 253, the Bomber Tank. pvars: the hit record +0x20 (its health +0x20: 9, 6 once wrecked; its flinch timer +0x26;
// +0x28 / +0x29 / +0x2a / +0x2e / +0x30 its settings, +0x2e 2: gone at once), +0x3e flags, its velocity +0x40 (kept
// clear), +0x58 / +0x5a from 0x1618cc / 0x1618d0, its attached frame +0x60, its colour pulse +0xa0 (shared with its tracks),
// its target +0xb0 (+0xf0 the target moby, +0xf4 its mode: 2 none), a joint node +0x100, the turret's rotation +0x110, +0x120
// cleared when wrecked, the moby whose pvar +0 at 1.0 lets it in +0x140, its wall +0x144 (a moby; none: <= 0) and the box
// of the wall's rubble +0x148, the route of its entry +0x14c, its route +0x150, its targeting area (a path) +0x154, its yaw
// rate +0x158, its engine's sound slot +0x15c, its two tracks (class 331) +0x160 / +0x164, its route's point +0x168, its top
// speed +0x16c (2 a second), the turret's yaw +0x170 / rate +0x174 and pitch +0x178 / rate +0x17c, its speed +0x180, its
// number for the skill point +0x184 (1 .. 4). States: 0 set up (its tracks spawned, its health 9): with a moby +0x140 it
// waits hidden (0xb); with a wall +0x144 it waits hidden to break it (0xe), or when broken before (0x13d3b6) the wall's
// pieces retired and it drives from its route's start (6); else it drives (6); 3 fires: drives, its two bombs at its
// joints 1 / 2 on its animation 2's frames 4 and 10 (from the turret's aim, 0x1618a8 a second), back to driving (6)
// once the target is 22 away or none; 6 drives, firing (3, the animation 2, the hero's help voice 0x1f44 unless 0x141c38
// or 0x13d4cb) once the target is within 20; 0xb waits for the moby +0x140, then in at its entry's start, shown (6, the
// animation 1); 0xe waits for a target, then breaks its wall (the sound 6 when 0x13d3b6 was set) and marks it broken,
// shown (6); 0x63 wrecked: drives smoking (its joint 4 on the frames 59 / 119, 5 on 29 / 89 of its animation). After
// every state its attached frame is set from its move and its rotation.
// orig: L8:0x002d42d8
void bat_class253_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t old_pos = sp, old_rot = sp + 0x10, moved = sp + 0x20, muzzle = sp + 0x30;
    const uint32_t pos = m + 0x10;
    lq_sq(old_pos, pos);
    lq_sq(old_rot, m + 0x40);
    if (lbu(pv + 0x2e) == 2) {
        retire_with_tracks(m, pv);
        ee_stack_free(0xe0);
        return;
    }
    bomber_tank_frame_l8(m);
    const uint32_t target = lwu(pv + 0xf0);
    // the tank and its tracks hidden (flags 0x41) and uncoloured
    auto hide = [&](uint32_t own_flags) {
        sh(m + 0x34, own_flags);
        sh(lwu(pv + 0x160) + 0x34, lhu(lwu(pv + 0x160) + 0x34) | 0x41u);
        sh(lwu(pv + 0x164) + 0x34, lhu(lwu(pv + 0x164) + 0x34) | 0x41u);
        sw(m + 0x94, 0);
        sw(lwu(pv + 0x160) + 0x94, 0);
        sw(lwu(pv + 0x164) + 0x94, 0);
    };
    // the tank and its tracks at its place, shown (flags 0x41 off) in their classes' colours, driving (6) unless the
    // target's mode is 2
    auto come_in = [&] {
        lq_sq(lwu(pv + 0x160) + 0x10, pos);
        lq_sq(lwu(pv + 0x164) + 0x10, pos);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sh(lwu(pv + 0x160) + 0x34, lhu(lwu(pv + 0x160) + 0x34) & 0xffbeu);
        sh(lwu(pv + 0x164) + 0x34, lhu(lwu(pv + 0x164) + 0x34) & 0xffbeu);
        for (uint32_t one : {m, lwu(pv + 0x160), lwu(pv + 0x164)}) sw(one + 0x94, lwu(lwu(one + 0x24) + 0x10));
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        if (lw(pv + 0xf4) == 2) return;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 6);
        if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
    };
    switch (lbu(m + 0x20)) {  // (a compare tree, no jump table)
    case 0: {
        for (uint32_t slot : {0x160u, 0x164u}) {
            if (lwu(pv + slot) != 0) continue;
            const uint32_t track = moby_spawn(0x14b);
            sw(pv + slot, track);
            sb(track + 0x30, 0x40);
            sh(track + 0x32, 0x40);
            sb(track + 0x31, 1);
            sd(track + 0x38, ld(lwu(0x001413d0) + 0x38));
            sh(track + 0x34, slot == 0x160u ? 0x4000u : 0xc000u);
            lq_sq(track + 0x10, pos);
            sw(track + 0x94, 0);
            moby_anim_set(track, 0, 0, 0);
            moby_update_transform(track);
        }
        sub_0024f590(m, 0, pv + 0x100);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        swc1(pv + 0x16c, fpu_add(dt(), dt()));
        if (lw(pv + 0x140) > 0) {
            sb(m + 0x20, 0xb);
            hide((lhu(m + 0x34) & 0xefffu) | 0x41u);
            anim_to(m, 0, 0);
        } else if (lw(pv + 0x144) > 0 && lbu(0x0013d3b6) == 0) {
            sb(m + 0x20, 0xe);
            hide(lhu(m + 0x34) | 0x41u);
            anim_to(m, 0, 0);
        } else {
            if (lw(pv + 0x144) > 0) bomber_tank_wall_gone_l8(m);
            lq_sq(pos, lvd::path(lw(pv + 0x150)) + 0x10);
            sb(m + 0x20, 6);
            anim_to(m, 1, 0);
        }
        vec_clear(V(pv + 0x40));
        sb(pv + 0x29, 0);
        sb(pv + 0x28, 3);
        swc1(pv + 0x30, 3.0f);
        sb(pv + 0x2a, 0xc);
        sb(pv + 0x2e, 1);
        swc1(pv + 0x20, 9.0f);
        sh(pv + 0x3e, lhu(pv + 0x3e) | 2u);
        vec_clear(V(pv + 0x40));
        sw(pv + 0x178, 0);
        swc1(pv + 0x170, lwc1(m + 0x48));
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(lwc1(L8_ADDR(0x001618cc)))));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(L8_ADDR(0x001618d0)), 8.0f))));
        break;
    }
    case 6: {
        bomber_tank_drive_l8(m, lw(pv + 0x150));
        if (lw(pv + 0xf4) == 2) break;
        if (!(vec2_distance(V(pos), V(target + 0x10)) < 20.0f)) break;
        if (lhu(0x00141c38) == 0 && lbu(0x0013d4cb) == 0) sub_00203908(0x1f44, 0x5a);
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 3);
        if (set != 2) moby_anim_set(m, 2, 0, frame_scale_i(0xa));
        break;
    }
    case 3: {
        bomber_tank_drive_l8(m, lw(pv + 0x150));
        if (lw(pv + 0xf4) == 2 || 22.0f < vec2_distance(V(pos), V(target + 0x10))) {
            const uint32_t set = lbu(m + 0x53);
            sb(m + 0x20, 6);
            if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        }
        if (anim_crossed(m, 4.0f) == 0 && anim_crossed(m, 10.0f) == 0) break;
        // a bomb from the turret's muzzle (the joint 1 on the frame 4, else 2) along the turret's aim
        moby_chain_point_v(m, moby_anim_time(m) == 4.0f ? 1u : 2u, V(muzzle));
        const float speed = fpu_mul(lwc1(L8_ADDR(0x001618a8)), dt());
        const float yaw = angle_add(lwc1(pv + 0x170), lwc1(m + 0x48));
        const uint32_t vel = moved;
        swc1(vel, fpu_mul(cos_vu(yaw), fpu_mul(cos_vu(lwc1(pv + 0x178)), speed)));
        swc1(vel + 4, fpu_mul(sin_vu(yaw), fpu_mul(cos_vu(lwc1(pv + 0x178)), speed)));
        swc1(vel + 8, fpu_mul(-sin_vu(lwc1(pv + 0x178)), speed));
        const uint32_t bomb = bomber_tank_bomb_l8(m, muzzle, vel);
        if (bomb != 0) sb(bomb + 0x20, 1);
        break;
    }
    case 0xb: {
        const uint32_t key = table_moby(lwu(pv + 0x140));
        if (!(lwc1(lwu(key + 0x78)) == 1.0f)) break;
        lq_sq(pos, lvd::path(lw(pv + 0x14c)) + 0x10);
        come_in();
        break;
    }
    case 0xe: {
        if (lw(pv + 0xf4) == 2) break;
        bomber_tank_break_wall_l8(m);
        if (lbu(0x0013d3b6) != 0) moby_sound_start(6, 0, m);
        sb(0x0013d3b6, 1);  // (the wall broken)
        come_in();
        break;
    }
    case 0x63: {
        bomber_tank_drive_l8(m, lw(pv + 0x150));
        const float t = moby_anim_time(m);
        if ((59.0f <= t && t <= 60.0f) || (119.0f <= t && t <= 120.0f))
            bomber_tank_smoke_l8(m, 4, 10);
        else if ((29.0f <= t && t <= 30.0f) || (89.0f <= t && t <= 90.0f))
            bomber_tank_smoke_l8(m, 5, 10);
        break;
    }
    default:
        break;
    }
    vec3_sub(V(moved), V(pos), V(old_pos));
    attach_frame_set(pv + 0x60, moved, m + 0x40, old_rot);
    ee_stack_free(0xe0);
}

}  // namespace rac1
