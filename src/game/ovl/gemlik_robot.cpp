// Gemlik Base's (level 13) class 1262: a walking robot that throws an electric beam along the ground and swings its arm
// (the level's own build of Blarg's and Orxon's class 1068, shared_class1068.cpp: another layout, other tunings, the
// beam's own thresholds, a floor test of its own). Level 13 only: the addresses are Gemlik's (`orig: L13:`), the
// level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp; its draw (L13:0x3098d0) is the shared
// beam_draw.
//
// The pvars: the hit record +0x20 (+0x20 the health, +0x24 / +0x28 / +0x30 / +0x58 / +0x5a its words, +0x26 the
// knock-back's timer), the timer flag +0x38, the knock-back record +0x60 (+0x68 the fall's speed, +0x78 / +0x7c its
// speeds, +0xb0 / +0xb4), the colour pulse's +0x80 / +0x88, the walker record +0xc0, the colour pulse +0x110 (+0x117),
// the target's record +0x120 (target_select's 0x48 bytes: +0x128 its height, +0x160 the target, +0x164 2 when out of
// reach), the home +0x170, the path +0x180, the chase timer +0x184 (180 .. 240 frames), the turn's speed +0x188, the
// range +0x190 (+0x19c without the timer, +0x1a0 with it), the attack's timer +0x194, the walls' path +0x198, the
// guard box +0x1a4, the chase's swerve +0x1a8, the arm's last point +0x1e0, the beam: its origin +0x1f0, its life
// +0x1fc (a frame count kept as a float), the ground's normal +0x200, its 16 points +0x210.. (each w its strength), the
// joint's node +0x400.
//
// States (the table L13:0x1f55f0, switched on the index): 0 sets up; 1 guards (turned to the target: the beam's
// run-up, 2, when the target is beyond L13:0x161e74 and in front, the arm, 5, when it is nearer, the chase, 11, while
// the timer runs; otherwise 7); 2 .. 4 throw the beam; 5 / 6 swing the arm; 7 walks toward the target swerving off
// walls; 11 / 12 chase and swing; 13 walks home; 15 knocked back; 16 dying. 8, 9, 10, 14 and past the table: nothing.
//
// PAL: the 2.00 disc's build (1.02) starts the beam's run-up (state 2) from state 1 only while the target is nearer
// than 15.0, and takes state 7 otherwise: 1.02 Fixes, fix 44 (fixes102_gemlik_robot_reach), the 2.00 disc's own code
// whatever the row. The three helpers and the 1.00 disc's build: relocations only.
#include <game/collision/collision.h>
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
constexpr int32_t kBeamPoints = 16;

// the beam's point i (+0x210 + 16 i; its w at +0xc the point's strength)
uint32_t beam_point(uint32_t pv, int32_t i) { return pv + 0x210 + static_cast<uint32_t>(i) * 0x10; }

// the yaw from the moby to the point (x, y)
float yaw_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the state set, and the animation `set` started over `frames` (scaled) unless it is the one playing
void go(uint32_t m, uint32_t state, uint32_t set, int32_t frames) {
    sb(m + 0x20, state);
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the yaw turned toward the target at its full rate (a turn a second squared, half a turn a second at most)
void face(uint32_t m, uint32_t pv, uint32_t target) {
    const float yaw = yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14));
    const float accel = fpu_mul(dt_sq(), u2f(0x40c90fdbu));
    yaw_seek(m + 0x48, pv + 0x188, yaw, accel, accel, fpu_mul(dt(), u2f(0x40490fdbu)));
}

// the yaw turned toward `yaw`, damped (a turn a second at most)
void turn_to(uint32_t m, uint32_t pv, float yaw) {
    yaw_turn_damped(m, pv + 0x188, yaw, u2f(0x3d4ccccdu), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40c90fdbu)));
}

// a step two long along the moby's yaw (its cosine and sine each added to itself; z 0), at `step`
void set_step_twice(uint32_t m, uint32_t step) {
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(step, fpu_add(c, c));
    const float s = sin_vu(lwc1(m + 0x48));
    sw(step + 8, 0);
    swc1(step + 4, fpu_add(s, s));
}

// The arm's swing (the animation's frames 9 .. 15): from its last point (+0x1e0) to the chain point 0 now, against the
// moby's position `up` above, swept in 5 steps with the hit record of a strike along its yaw (kind 1); then the chain
// point kept. The original's frame locals: sp+0x20 the point, sp+0x30 the moby's, sp+0x40 the strike's direction,
// sp+0x50 the record (sub_00259888 writes 0x24 bytes, swept_collide reads them).
void arm_swing(uint32_t m, uint32_t pv, uint32_t sp, float up) {
    const uint32_t point = sp + 0x20, from = sp + 0x30, dir = sp + 0x40, rec = sp + 0x50;
    moby_chain_point_v(m, 0, V(point));
    lq_sq(from, m + 0x10);
    swc1(from + 8, fpu_add(lwc1(from + 8), up));
    const float t = moby_anim_time(m);
    if (9.0f <= t && t <= 15.0f) {
        swc1(dir, cos_vu(lwc1(m + 0x48)));
        swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
        swc1(dir + 0xc, u2f(0x45afdf66u));
        swc1(dir + 8, 1.0f);
        sub_00259888(1.0f, rec, static_cast<int32_t>(m), 1, dir);
        sb(rec + 0x19, 1);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 0);
        swept_collide(V(point), V(from), V(pv + 0x1e0), V(from), m, static_cast<const uint32_t*>(ee_ram_ptr(rec)), 5);
    }
    lq_sq(pv + 0x1e0, point);
}

// the walk's end against the walls: pushed out of the path `+0x198` (0.75), and whether the line to the target crosses
// it within 0.8 of the moby (the crossing at `cross`)
bool walls(uint32_t m, uint32_t pv, uint32_t target, uint32_t cross) {
    path_push_out_2d(lwu(pv + 0x198), m + 0x10, m + 0x10, 0.75f);
    return path_crossing(lw(pv + 0x198), m + 0x10, target + 0x10, cross) != 0 &&
           vec3_distance(V(m + 0x10), V(cross)) < u2f(0x3f4ccccdu);
}

// the burst of its death or of a deadly floor: the explosion (the original's stack arguments 1, -1, 0), then retired
void burst(uint32_t m, uint32_t a1) {
    explosion_effect(m, a1, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
    moby_retire(m);
}
}  // namespace

// The beam's frame while its life (+0x1fc) lasts: at 20 frames left its full-strength points drop to 0.98; each point
// spreads out from the origin (the level's speed), is lifted to 0.35 above the ground's plane through the moby, settles
// onto the floor under it when the floor's probe (1 above to 3 below) meets something higher, and a full-strength
// point stopped by a wall drops to 0.98, a weak one fading out (two frames' time a frame); between two points of 0.98
// or more a strike (kind 0x10001) is swept along the segment. Then drawn (beam_draw) and sparking (beam_sparks).
// The original's frame (288 bytes): sp the point's next place, sp+0x10 its place about the moby (then the strike's hit
// record: sub_00259888 writes 0x24 bytes, segment_collide_v reads them), sp+0x20 its lift, sp+0x30 / +0x40 the floor
// probe's ends, sp+0x50 the life's word (timer_tick_i32).
// orig: L13:0x003094f8
void gml_3094f8_l13(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t left = sp + 0x50;
    sw(left, static_cast<uint32_t>(float_to_int(lwc1(pv + 0x1fc))));
    tick32(left);
    swc1(pv + 0x1fc, static_cast<float>(lw(left)));
    if (lw(left) == 0) return;
    const float weak = u2f(0x3f7ae148u);  // (0.98)
    if (lw(left) == 20)
        for (int32_t i = 0; i < kBeamPoints; ++i)
            if (1.0f <= lwc1(beam_point(pv, i) + 0xc)) swc1(beam_point(pv, i) + 0xc, weak);
    const uint32_t next = sp, rel = sp + 0x10, lift = sp + 0x20, from = sp + 0x30, to = sp + 0x40;
    const uint32_t origin = pv + 0x1f0, normal = pv + 0x200;
    const float above = u2f(0x3eb33333u);  // (0.35)
    for (int32_t i = 0; i < kBeamPoints; ++i) {
        const uint32_t p = beam_point(pv, i);
        vec3_sub(V(next), V(p), V(origin));
        sw(next + 8, 0);
        vec3_set_length(V(next), V(next), fpu_mul(lwc1(L13_ADDR(0x00161e7c)), dt()));
        vec3_add(V(next), V(p), V(next));
        vec3_sub(V(rel), V(next), V(m + 0x10));
        const float height = vec3_dot(V(rel), V(normal));
        vec3_set_length(V(lift), V(normal), fpu_add(-height, above));
        vec3_add(V(rel), V(rel), V(lift));
        const float lift_z = lwc1(lift + 8);
        lq_sq(from, p);
        swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
        lq_sq(to, p);
        swc1(to + 8, fpu_sub(lwc1(to + 8), 3.0f));
        if (segment_collide_v(V(from), V(to), 2, 0, nullptr) != 0 && lift_z < lwc1(next + 8))
            approach_f(next + 8, fpu_add(coll_hit_point().z, above), fpu_mul(dt(), 4.0f));
        bool stopped = false;
        if (1.0f <= lwc1(p + 0xc) && segment_collide_v(V(p), V(next), 2, 0, nullptr) != 0) {
            swc1(next + 0xc, weak);
            stopped = true;
        }
        if (!stopped && lwc1(next + 0xc) < 1.0f) approach_f(next + 0xc, 0.0f, fpu_add(dt(), dt()));
        lq_sq(p, next);
    }
    const uint32_t dir = sp, rec = sp + 0x10;
    for (int32_t i = 0; i < kBeamPoints - 1; ++i) {
        const uint32_t p = beam_point(pv, i), q = beam_point(pv, i + 1);
        if (!(weak <= lwc1(p + 0xc)) || !(weak <= lwc1(q + 0xc))) continue;
        swc1(dir, cos_vu(lwc1(m + 0x48)));
        swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
        swc1(dir + 8, 1.0f);
        swc1(dir + 0xc, u2f(0x45afdf66u));
        sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, dir);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 0);
        sb(rec + 0x19, 1);
        segment_collide_v(V(p), V(q), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
    }
    sub_001f93e0(L13_ADDR(0x003098d0), m);
    beam_sparks(m);
}

// The frame's common part: the size (the class's scale x L13:0x161e58), the beam (0x3094f8), the chase timer (+0x184,
// dealt 180 .. 240 frames when +0x38 is set), a hit (mask 0x330000; none from another class-1262 moby's beam: its damage
// zeroed) taken off the health (the death at 0 or below), then the hit's reaction (the table L13:0x1f5640: 1 / 2 the
// death, 3 .. 10 knocked back, 0, 11 and past the table the colour pulse only); while it is knocked back or dying (hit
// state 1, or state 16) only the pulse's update. Then the range (+0x1a0 while the timer runs), the target picked (+0x164
// = 2: out of the range from home, 3 above or below, or outside the guard box), the hero's moby the target when there is
// none; the floor (a deadly one, L13:0x1f5670's kinds 0, 1, 3, 11, 13: its bolts, a burst, retired; on a slope steeper
// than 40 degrees it slides 0.05 down the normal; it falls (9.8) or rises 0.05 a frame to the floor), and retired out of
// the space (2 .. 1021). The original's frame (176 bytes): sp+0x20 the slide (16), sp+0x30 / +0x34 the hit's state and
// damage words (moby_hit_react_r's, read back).
// orig: L13:0x00308d10
void gml_308d10_l13(uint32_t m) {
    const Frame f(176);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L13_ADDR(0x00161e58))));
    gml_3094f8_l13(m);
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x184, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    tick32(pv + 0x184);
    const uint32_t hit_state = sp + 0x30, hit_dmg = sp + 0x34;
    sw(hit_dmg, 0);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000), 0);
    int32_t reaction = 0;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    const uint32_t pulse = pv + 0x110;
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x10) {
        if (hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == 0x4ee) sw(hit_dmg, 0);
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        sw(pv + 0x80, 0x2cd);
        swc1(pv + 0x88, u2f(0x3f333333u));
        // the hit reactions' table (L13:0x1f5640): 1, 2 (0x308f00) the death; 3 .. 10 (0x308e7c) knocked back; 0, 11 and
        // past the table (0x308f90) nothing more
        const uint32_t r = static_cast<uint32_t>(reaction);
        if (r >= 3 && r <= 10) {
            sb(m + 0x20, 0xf);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            const float sx = fpu_mul(lwc1(L13_ADDR(0x00161e60)), dt()), sz = fpu_mul(lwc1(L13_ADDR(0x00161e64)), dt());
            swc1(pv + 0xb0, 8.0f);
            swc1(pv + 0x7c, sz);
            swc1(pv + 0xb4, 15.0f);
            swc1(pv + 0x78, sx);
            move_state_start(m, pv + 0x60, 7, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            sb(pv + 0x117, 0xfa);
        } else if (r == 1 || r == 2) {
            sb(m + 0x20, 0x10);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            const float sx = fpu_mul(lwc1(L13_ADDR(0x00161e68)), dt()), sz = fpu_mul(lwc1(L13_ADDR(0x00161e6c)), dt());
            swc1(pv + 0xb0, 9.0f);
            swc1(pv + 0xb4, 17.0f);
            swc1(pv + 0x7c, sz);
            swc1(pv + 0x78, sx);
            move_state_start(m, pv + 0x60, 8, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            sb(pv + 0x117, 0xfa);
            moby_drop_bolts(m, 0, -1);
        }
        color_pulse_start(m, pulse);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);

    swc1(pv + 0x190, lw(pv + 0x184) != 0 ? lwc1(pv + 0x1a0) : lwc1(pv + 0x19c));
    const uint32_t path = lvd::path(lw(pv + 0x180));
    if (target_select(m, pv + 0x120, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x190)) != 2) {
        if (lwc1(pv + 0x190) < vec2_distance(V(pv + 0x170), V(pv + 0x120))) sw(pv + 0x164, 2);
        else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
        const int32_t box = lw(pv + 0x1a4);
        if (box == -1 || point_in_box(pv + 0x120, box) != 0) sw(pv + 0x1a4, 0xffffffffu);
        else sw(pv + 0x164, 2);
    }
    if (lw(pv + 0x160) == 0) sw(pv + 0x160, lwu(0x001413d0));

    const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
    auto fall = [m, pv]() {
        const float v = fpu_sub(lwc1(pv + 0x68), fpu_mul(dt_sq(), u2f(0x411ccccdu)));  // (9.8)
        swc1(pv + 0x68, v);
        return fpu_add(lwc1(m + 0x18), v);
    };
    if (floor == 0.0f) {
        if (lbu(m + 0x20) < 15) swc1(m + 0x18, fall());
    } else {
        // the floor's kinds (L13:0x1f5670): 0, 1, 3, 11, 13 (0x309104) deadly; the others and past the table (0x30919c)
        switch (sub_001f35d8()) {
        case 0: case 1: case 3: case 11: case 13:
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            moby_drop_bolts(m, 0, -1);
            burst(m, L13_ADDR(0x00160680));
            return;
        default:
            break;
        }
        if (lbu(m + 0x20) < 15) {
            const Vec4 normal = coll_hit_normal();
            if (u2f(0x3f32b8c2u) < atan2_f(normal.z, vec2_length(normal))) {  // (40 degrees)
                vec3_set_length(V(sp + 0x20), normal, u2f(0x3d4ccccdu));
                vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x20));
            }
            const float z = lwc1(m + 0x18);
            if (floor < z) {
                const float nz = fall();
                if (nz < floor) {
                    sw(pv + 0x68, 0);
                    swc1(m + 0x18, floor);
                } else {
                    swc1(m + 0x18, nz);
                }
            } else if (z < floor) {
                swc1(m + 0x18, fpu_add(z, u2f(0x3d4ccccdu)));
            }
        }
    }
    const float lo = 2.0f, hi = 1021.0f;
    const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
    if (x < lo || hi < x || y < lo || hi < y || z < lo || hi < z) moby_retire(m);
}

// The beam thrown: the floor probed under the moby (the ground's normal +0x200, unit), its origin at the chain point 0,
// its 16 points on an arc 0.5 out from the origin, spread over L13:0x161e80 degrees about the moby's yaw, the two ends'
// strength 0 and the others' 1; its life L13:0x161e84 frames (scaled, as a float). The original's frame (176 bytes):
// sp a point's offset (16, vec3_add reads it).
// orig: L13:0x00309370
void gml_309370_l13(uint32_t m) {
    const Frame f(176);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    floor_probe_v(0.5f, V(m + 0x10), 0);
    vec3_set_length(V(pv + 0x200), coll_hit_normal(), 1.0f);
    moby_chain_point_v(m, 0, V(pv + 0x1f0));
    for (int32_t i = 0; i < kBeamPoints; ++i) {
        const float spread = fpu_mul(lwc1(L13_ADDR(0x00161e80)), u2f(0x3c8efa35u));  // (in radians)
        const float a = fpu_mul(fpu_sub(fpu_mul(int_to_float(i), u2f(0x3d800000u)), 0.5f), spread);  // (i / 16 - 0.5)
        swc1(sp, fpu_mul(cos_vu(angle_add(a, lwc1(m + 0x48))), 0.5f));
        swc1(sp + 4, fpu_mul(sin_vu(angle_add(a, lwc1(m + 0x48))), 0.5f));
        sw(sp + 8, 0);
        const uint32_t p = beam_point(pv, i);
        vec3_add(V(p), V(pv + 0x1f0), V(sp));
        swc1(p + 0xc, i == 0 || i == kBeamPoints - 1 ? 0.0f : 1.0f);
    }
    swc1(pv + 0x1fc, static_cast<float>(frame_scale_i(lw(L13_ADDR(0x00161e84)))));
}

// The update: the frame's common part (0x308d10), the joint scaled, the floor band near the camera (28), then the state
// (the file's head). The original's frame (288 bytes): sp+0x20 .. +0x30 the walk's step and its result (walker_record_step
// writes 16 at sp+0x30), sp+0x40 the walls' crossing (path_crossing writes 16), arm_swing's locals (sp+0x20 .. sp+0x73).
// orig: L13:0x00307e98
void gml_class1262_update_l13(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    gml_308d10_l13(m);
    const uint32_t target = lwu(pv + 0x160);
    joint_node_scale(m, 1, pv + 0x400, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L13_ADDR(0x001670c0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    const uint32_t step = sp + 0x20, cross = sp + 0x40;
    // the states' table (L13:0x1f55f0); 8, 9, 10, 14 (0x308cdc) do nothing
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x307f58)
        swc1(pv + 0x20, 2.0f);
        sb(pv + 0x28, 1);
        sb(pv + 0x58, 0xc);
        sb(pv + 0x5a, 8);
        sh(pv + 0x24, 2);
        swc1(pv + 0x30, 1.5f);
        walker_record_init_l1(pv + 0xc0);
        const float size = lwc1(L13_ADDR(0x00161e58));
        swc1(pv + 0xc8, 2.0f);
        swc1(pv + 0xcc, 0.5f);
        sw(pv + 0xc0, static_cast<uint32_t>(float_to_int(fpu_mul(size, u2f(0x44333333u)))));  // (x 716.8)
        swc1(pv + 0xc4, fpu_mul(lwc1(L13_ADDR(0x00161e58)), u2f(0x3f333333u)));         // (x 0.7)
        const float k = rand_range_f(u2f(0x3f733333u), u2f(0x3f866666u));               // (0.95 .. 1.05)
        swc1(pv + 0xe4, fpu_mul(fpu_mul(lwc1(L13_ADDR(0x00161e78)), dt()), k));
        lq_sq(pv + 0x170, m + 0x10);
        moby_chain_point_v(m, 0, V(pv + 0x1e0));
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x308038)
        face(m, pv, target);
        if (lw(pv + 0x164) == 2) {
            if (lw(pv + 0x184) == 0) break;
            go(m, 0xb, 1, 20);
            break;
        }
        const float dist = vec2_distance(V(m + 0x10), V(target + 0x10));
        if (!(lwc1(L13_ADDR(0x00161e74)) < dist)) {
            go(m, 5, 6, 10);
            break;
        }
        const float toward = yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14));
        const bool facing = angle_dist(lwc1(m + 0x48), toward) < u2f(0x3e860a92u);  // (15 degrees)
        // 1.02 Fixes, fix 44 (L13:0x309544 .. 0x3095c4 on the PAL 2.00 disc): the run-up only while the target is nearer
        // than 15.0; at 15 or more it takes state 7, as when it does not face the target
        const bool in_reach = !fixes102_gemlik_robot_reach() || dist < 15.0f;
        if (facing && in_reach) go(m, 2, 3, 10);
        else go(m, 7, 1, 20);
        break;
    }
    case 2:  // (0x3081bc)
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 3);
        gml_309370_l13(m);
        if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(10));
        break;
    case 3: {  // (0x308200)
        if ((lbu(m + 0x70) & 2) == 0) break;
        const float dist = vec2_distance(V(m + 0x10), V(target + 0x10));
        if (lw(pv + 0x164) != 2 && dist < 15.0f) {
            go(m, 4, 4, 20);
            sw(pv + 0x194, static_cast<uint32_t>(frame_scale_i(lw(L13_ADDR(0x00161e70)))));
        } else {
            go(m, 1, 0, 20);
        }
        break;
    }
    case 4:  // (0x308298)
        face(m, pv, target);
        if (tick32(pv + 0x194) != 0) {
            sb(m + 0x20, 3);
            gml_309370_l13(m);
            if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(5));
        } else if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(L13_ADDR(0x00161e74))) {
            moby_chain_point_v(m, 0, V(pv + 0x1e0));
            go(m, 5, 6, 10);
        } else if (lw(pv + 0x184) != 0) {
            go(m, 0xb, 1, 20);
        }
        break;
    case 5:  // (0x3083cc)
        face(m, pv, target);
        arm_swing(m, pv, sp, u2f(0x3e99999au));  // (0.3 up)
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 6);
        sb(m + 0xbc, 1);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(10));
        break;
    case 6:  // (0x308580)
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(m, lbu(m + 0xbc), 0, 20);
        break;
    case 7: {  // (0x3085a8)
        turn_to(m, pv, angle_add(yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14)), lwc1(pv + 0x1a8)));
        set_step_twice(m, step);
        if ((walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10) & 2u) == 0) {
            sw(pv + 0x1a8, 0);
        } else {  // (the original draws a number and drops it)
            libc_rand();
            swc1(pv + 0x1a8, fpu_mul(int_to_float(-1), u2f(0x3f8cbe4cu)));  // (-63 degrees)
        }
        const bool at_wall = walls(m, pv, target, cross);
        const float dist = vec2_distance(V(m + 0x10), V(target + 0x10));
        if (dist < 15.0f || at_wall) go(m, 2, 3, 10);
        else if (dist < lwc1(L13_ADDR(0x00161e74))) go(m, 5, 6, 10);
        break;
    }
    case 11: {  // (0x3087b8)
        turn_to(m, pv, yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14)));
        set_step_twice(m, step);
        walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10);
        const bool at_wall = walls(m, pv, target, cross);
        if (lw(pv + 0x184) == 0) {
            sb(m + 0x20, 0xd);
            break;
        }
        if (at_wall) {
            go(m, 0xc, 6, 10);
        } else if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(L13_ADDR(0x00161e74))) {
            moby_chain_point_v(m, 0, V(pv + 0x1e0));
            go(m, 0xc, 6, 10);
        }
        break;
    }
    case 12:  // (0x308964)
        arm_swing(m, pv, sp, 1.5f);
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(m, 0xb, 1, 20);
        break;
    case 13: {  // (0x308aa8)
        turn_to(m, pv, yaw_to(m, lwc1(pv + 0x170), lwc1(pv + 0x174)));
        set_step_twice(m, step);
        if (angle_dist(lwc1(m + 0x48), yaw_to(m, lwc1(pv + 0x170), lwc1(pv + 0x174))) < u2f(0x3f490fdbu)) {  // (45 degrees)
            walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10);
            path_push_out_2d(lwu(pv + 0x198), m + 0x10, m + 0x10, 0.75f);
        }
        if (vec3_distance(V(pv + 0x170), V(m + 0x10)) < 1.0f) go(m, 1, 0, 20);
        break;
    }
    case 15: {  // (0x308be0)
        const uint32_t r = enemy_physics(m, pv + 0x60);
        if ((r & 0x40u) != 0) {
            sq_zero(pv + 0x60);
            go(m, 1, 0, 10);
        } else if ((r & 0x120u) != 0) {
            moby_retire(m);
        }
        break;
    }
    case 16:  // (0x308c54)
        if ((enemy_physics(m, pv + 0x60) & 0x160u) == 0) break;
        burst(m, pv + 0x60);
        break;
    default:
        break;
    }
}

}  // namespace rac1
