// Class 638's update and helpers: one port of the copies Kalebo III and Veldin carry (the same code relinked; each level's
// jump tables of the states and of the hit reactions are its own, switched on here by the index). Each level keeps three
// switches of its own (per level below: the hover's bob and sound, the aim of its joints, the sparks) and the two six-colour
// tables of its burst; the rest is the engine's (OV: the paths 0x1b04b0, the moby groups 0x1ab840, the mobys 0x15ffd8, the
// camera 0x166dc0, the particle load 0x15f5d0, the scene state 0x15f5c4 / 0x16c890) or the base ELF's (bare: the hero
// 0x13f350, his moby 0x1413d0, the frame time 0x15ed6c, the level number 0x15ed84, the skill point's counter 0x15ee10 and
// flag 0x13d425). Header shared_class638.h.
//
// The pvars: +0x20 its health, +0x60 the colour pulse, +0x70 the move record, +0xd0 the target point, +0x110 the moby it
// aims at, +0x114 its target state (2: none), +0x120 its path (-1: none), +0x124 its range, +0x128 attached, +0x12c the
// target path, +0x130 / +0x1b0 / +0x230 its joints' springs, +0x2b0 the hover's goal, +0x2c0 the hover point, +0x2cc the turn
// it keeps while attached, +0x2d0 the moby it is attached to, +0x2d4 the seek speed, +0x2d8 the turn rate, +0x2dc / +0x2de
// a timer and the shots of a burst, +0x2e0 the look-round timer, +0x2e4 .. +0x2fc the bob (three phases, three rates, its
// size +0x2ec), +0x300 the path point, +0x302 a timer, +0x304 its sound, +0x308 counts towards the skill point.
#include <game/ovl/shared_class638.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;      // (0x3c8efa35)
constexpr float FOUR_PI = 12.566370964050293f;   // (0x41490fdb)
constexpr float QUARTER_PI = 0.7853981852531433f;  // (0x3f490fdb)
constexpr float TWENTY_DEG = 0.3490658402442932f;  // (0x3eb2b8c2)
constexpr int32_t SHOT_CLASS = 0x31;
constexpr int32_t CLASS_WRENCH = 0x47;  // (a hit of the wrench's keeps the kill counting towards the skill point)

// the level's three switches: the bob and its sound (+0), the joints' aim (+4), the sparks (+8)
uint32_t switches() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x00161cdc) : L16_ADDR(0x00161a74); }
// the burst's two six-colour tables (+0, +0x18)
uint32_t burst_colours() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x001f2710) : L16_ADDR(0x001e8300); }

float dt() { return lwc1(0x0015ed6c); }

// the point `i` of the path `path` (+0x10 the first)
uint32_t path_point(int32_t path, int32_t i) {
    return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(path) << 2)) + (static_cast<uint32_t>(i) << 4) + 0x10;
}

// the animation set changed to `set` over `frames` (scaled) unless it is already that set
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// a random frame count from `lo` .. `hi` frames, scaled to the frame rate
int32_t rand_frames(float lo, float hi) { return float_to_int(frame_scale_f(rand_range_f(lo, hi))); }

// the yaw turned towards (x, y), at most two turns a second
void turn_to(uint32_t m, uint32_t pv, float x, float y) {
    const float yaw = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
    const float rate = fpu_mul(lwc1(0x0015ed70), FOUR_PI);
    yaw_seek(m + 0x48, pv + 0x2d8, yaw, rate, rate, fpu_mul(dt(), FOUR_PI));
}

// the next of the two path points it moves between
int16_t other_point(uint32_t pv) { return static_cast<int16_t>((lh(pv + 0x300) + 1) % 2); }

// the knock-back or the fall: the move state `set` from the hitter's side at the speeds +0x88 / +0x8c
void knocked(uint32_t m, uint32_t pv, uint32_t hit, uint32_t sp, uint32_t yaw_at, uint32_t set) {
    swc1(pv + 0xc4, -1.0f);
    swc1(pv + 0xc0, -1.0f);
    const uint32_t from = lwu(hit + 0x20);
    const float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(from + 0x14)));
    swc1(yaw_at, yaw);
    lq_sq(sp, hit + 0x10);
    vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(yaw_at)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                     *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
    move_state_start(m, pv + 0x70, set, 1, 0, lwc1(yaw_at));
}
}  // namespace

// The hover point moved towards `to` at the seek speed (+0x2d4, eased towards the distance); the distance it was.
// orig: L16:0x002d3eb8 L18:0x002ddae0
float class638_seek(uint32_t m, uint32_t to) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t pv = lwu(m + 0x78), hover = pv + 0x2c0;
    const float d = vec3_distance(V(hover), V(to));
    const float turn = fpu_mul(lwc1(0x0015ed70), 4.0f);
    sw(sp + 0x10, 0);
    seek_1d(sp + 0x10, pv + 0x2d4, d, turn, turn, fpu_mul(dt(), 5.0f));
    vec3_sub(V(sp), V(to), V(hover));
    vec3_set_length(V(sp), V(sp), lwc1(pv + 0x2d4));
    vec3_add(V(hover), V(hover), V(sp));
    ee_stack_free(0x70);
    return d;
}

// The look-round timer (+0x2e0) of every live moby of its group (all its group's members), or its own when it has none.
// orig: L16:0x002d3f78 L18:0x002ddba0
void class638_group_timers(uint32_t m, int32_t lo, int32_t hi) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) {
        sw(lwu(m + 0x78) + 0x2e0, static_cast<uint32_t>(rand_frames(static_cast<float>(lo), static_cast<float>(hi))));
        return;
    }
    for (uint32_t e = lwu(OV(0x001ab840) + (group << 2));; e += 2) {
        const uint32_t member = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(member + 0x20)) >= 0)
            sw(lwu(member + 0x78) + 0x2e0, static_cast<uint32_t>(rand_frames(static_cast<float>(lo), static_cast<float>(hi))));
        if (lh(e) < 0) break;
    }
}

// The turn towards its path point: within 45 degrees ahead the animation 1, else 4 or 5 by the side (the moby's +0x34
// bit 15 swaps them).
// orig: L16:0x002d4070 L18:0x002ddc98
void class638_turn(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pt = path_point(lw(pv + 0x120), lh(pv + 0x300)) - 0x10;
    const float yaw = atan2_f(fpu_sub(lwc1(pt + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(pt + 0x14), lwc1(m + 0x14)));
    const float off = angle_sub(yaw, lwc1(m + 0x48));
    const bool swapped = (lhu(m + 0x34) & 0x8000u) != 0;
    if (off < QUARTER_PI && -QUARTER_PI < off) anim_to(m, 1, 0x14);
    else if (off < 0.0f) anim_to(m, swapped ? 4 : 5, 0x14);
    else anim_to(m, swapped ? 5 : 4, 0x14);
}

// Its burst: puffs (fewer and slower the nearer the camera) at its joint 2, shards (two more while the particle load is low and
// the camera is off).
// orig: L16:0x002d41f8 L18:0x002dde20
void class638_burst(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t to_cam = sp, still = sp + 0x10, at = sp + 0x20, col_a = sp + 0x30, col_b = sp + 0x50;
    moby_chain_point_v(m, 2, V(at));
    vec3_sub(V(to_cam), V(OV(0x00166dc0)), V(at));
    vec_clear(V(still));
    const float cam_dist = vec3_length(V(to_cam));
    const int32_t puffs = cam_dist < 8.0f ? float_to_int(cam_dist) + 2 : 10;
    const float close = cam_dist < 7.0f ? fpu_sub(7.0f, cam_dist) : 0.0f;
    const uint32_t colours = burst_colours();
    for (int32_t i = puffs; i > 0; --i) {
        const float r = rand_range_f(8.0f, 10.0f);
        for (uint32_t k = 0; k < 0x18; k += 4) sw(col_a + k, lwu(colours + k));
        for (uint32_t k = 0; k < 0x18; k += 4) sw(col_b + k, lwu(colours + 0x18 + k));
        const float speed = fpu_sub(fpu_mul(r, dt()), fpu_mul(close, dt()));
        const uint32_t c0 = col_a + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t c1 = col_b + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const int32_t n1 = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t n2 = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(at, still, lwu(c0), lwu(c1), static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), 0, 0, 400000.0f, speed);
    }
    if (lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && 9.0f < cam_dist) {  // (0.95)
        shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, 4.0f);
        shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(0x18)), 0x7f, 0x20, 0, 0x20, 4.0f);
    }
    shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7f, 0x40, 0, 0x30, 4.0f);
    shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(0x1b)), 0x60, 0x10, 0, 0x40, 3.5f);
    shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(0x1d)), 0x20, 0, 0, 0x20, 3.0f);
    ee_stack_free(0x140);
}

// The joints aimed at its target (the hero's +0xd0 when the moby it aims at is the hero's, else its target point +0xd0): the
// head (+0x194 / +0x198, +0x214 / +0x218: half the turn, each within 45 degrees) and, in states 4 and 5, the gun (+0x294 pitch
// within 45 degrees, +0x298 yaw within 20) from a point ahead of it; then the three joints' springs. Nothing aimed while it has
// no target (+0x114 at 2).
// orig: L16:0x002d4590 L18:0x002de1b8
void class638_aim(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t from = sp, target = sp + 0x10, inv = sp + 0x20, rel = sp + 0x60, gun = sp + 0x70, gun_rel = sp + 0x80, up = sp + 0x90;
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x114) != 2) {
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), 2.0f));
        lq_sq(target, lh(lwu(pv + 0x110) + 0xa6) == 0 ? 0x0013f420u : pv + 0xd0);
        mat3_transpose(M(inv), M(m + 0xc0));
        vec3_sub(V(rel), V(target), V(from));
        mat3_transform(V(rel), V(rel), M(inv));
        float yaw = atan2_f(lwc1(rel), lwc1(rel + 4));
        float pitch = -atan2_f(vec2_length(V(rel)), lwc1(rel + 8));
        if (QUARTER_PI < yaw) yaw = QUARTER_PI;
        else if (yaw < -QUARTER_PI) yaw = -QUARTER_PI;
        if (QUARTER_PI < pitch) pitch = QUARTER_PI;
        else if (pitch < -QUARTER_PI) pitch = -QUARTER_PI;
        const float half_yaw = fpu_mul(yaw, 0.5f), half_pitch = fpu_mul(pitch, 0.5f);
        swc1(pv + 0x218, half_yaw);
        swc1(pv + 0x214, half_pitch);
        swc1(pv + 0x198, half_yaw);
        swc1(pv + 0x194, half_pitch);
        if (lbu(m + 0x20) - 4u < 2u) {
            vec3_set_length(V(gun), V(m + 0xc0), 1.0f);
            vec3_set_length(V(up), V(m + 0xd0), 0.5f);
            vec3_add(V(gun), V(gun), V(from));
            vec3_add(V(gun), V(gun), V(up));
            vec3_sub(V(gun_rel), V(target), V(gun));
            mat3_transform(V(gun_rel), V(gun_rel), M(inv));
            float gun_pitch = -atan2_f(vec2_length(V(gun_rel)), lwc1(gun_rel + 8));
            float gun_yaw = -atan2_f(lwc1(gun_rel), lwc1(gun_rel + 4));
            if (QUARTER_PI < gun_pitch) gun_pitch = QUARTER_PI;
            else if (gun_pitch < -QUARTER_PI) gun_pitch = -QUARTER_PI;
            if (TWENTY_DEG < gun_yaw) gun_yaw = TWENTY_DEG;
            else if (gun_yaw < -TWENTY_DEG) gun_yaw = -TWENTY_DEG;
            swc1(pv + 0x294, gun_pitch);
            swc1(pv + 0x298, gun_yaw);
        }
    }
    const float k = u2f(0x3cf5c28fu), d = u2f(0x3e99999au);  // (0.03, 0.3)
    joint_node_spring(m, pv + 0x130, 1, fpu_mul(lwc1(0x0015ed64), k), fpu_mul(lwc1(0x0015ed64), d));
    joint_node_spring(m, pv + 0x1b0, 2, fpu_mul(lwc1(0x0015ed64), k), fpu_mul(lwc1(0x0015ed64), d));
    joint_node_spring(m, pv + 0x230, 3, fpu_mul(lwc1(0x0015ed64), k), fpu_mul(lwc1(0x0015ed64), d));
    ee_stack_free(0x130);
}

// Sparks from its joints 4 and 5 (each a quarter of the frames left out at random) while it is shown (+0x31) and not
// attached (state 10): a particle of type 2 thrown back along the joint's axis, pale blue to purple.
// orig: L16:0x002d4908 L18:0x002de530
void class638_sparks(uint32_t m) {
    if (lbu(m + 0x31) == 0 || lbu(m + 0x20) == 10) return;
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t joint = sp + 0x10, axis = sp + 0x30, at = sp + 0x40, vel_a = sp + 0x50, vel_b = sp + 0x60, jitter = sp + 0x70;
    for (uint32_t i = 0; i < 2; ++i) {
        if (0.75f < rand_range_f(0.0f, 1.0f)) continue;
        moby_chain_world_one(m, i + 4, joint);
        vec3_set_length(V(vel_a), V(axis), fpu_mul(fpu_mul(fpu_add(rand_range_f(-0.2f, 0.2f), 1.0f), -3.0f), dt()));
        const float drop = fpu_mul(fpu_mul(fpu_add(rand_range_f(-0.2f, 0.2f), 1.0f), -2.0f), dt());
        vec_clear(V(vel_b));
        swc1(vel_b + 8, drop);
        const float spread = fpu_mul(rand_range_f(-1.5f, 1.5f), dt());
        rand_sphere_point(jitter, spread, spread);
        vec3_add(V(vel_b), V(vel_b), V(jitter));
        swc1(vel_a + 0xc, rand_range_f(0.2f, 0.2f));
        swc1(vel_b + 0xc, rand_range_f(0.4f, 0.4f));
        const uint32_t c0 = rgba_lerp(0x801010ffu, 0x801080ffu, rand_range_f(0.0f, 1.0f));
        const uint32_t c1 = rgba_lerp(0x400000c0u, 0x4030ffffu, rand_range_f(0.0f, 1.0f));
        const int32_t n1 = float_to_int(frame_scale_f(fpu_mul(fpu_add(rand_range_f(-0.1f, 0.1f), 1.0f), 5.0f)));
        const int32_t n2 = float_to_int(frame_scale_f(fpu_mul(fpu_add(rand_range_f(-0.1f, 0.1f), 1.0f), 20.0f)));
        const int32_t n3 = float_to_int(frame_scale_f(fpu_mul(fpu_add(rand_range_f(-0.1f, 0.1f), 1.0f), 20.0f)));
        spawn_particle_02(at, vel_a, vel_b, c0, c1, static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), static_cast<uint32_t>(n3), -1);
    }
    ee_stack_free(0x160);
}

// A hit (not its own shots'): the health taken by the damage, the look-round timers of its group reset; by the reaction 1 / 2
// it dies (falls, drops its bolts; on Veldin a kill by the wrench alone adds to the skill point's ten, whose tenth shows
// the message), 3 .. 8 it is knocked back (state 8), 9 / 10 its colour flashes. Then its colour pulse, its look-round timer (a new
// one when asked: +0x38) and its target: the hero's (or none in the hero's mode 0x16; the hero moby when it aims at nothing),
// within its range or none.
// orig: L16:0x002d38b0 L18:0x002dd4d8
void class638_hits(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t knock = sp, state = sp + 0x10, dmg = sp + 0x14, yaw_a = sp + 0x18, yaw_b = sp + 0x1c;
    const uint32_t pv = lwu(m + 0x78);
    sw(dmg, 0);
    swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    if (hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == SHOT_CLASS) hit = 0;
    int32_t reaction;
    {
        int32_t st = lw(state);
        float d = lwc1(dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &d, nullptr, 4);
        sw(state, static_cast<uint32_t>(st));
        swc1(dmg, d);
    }
    const uint32_t pulse = pv + 0x60;
    if (lw(state) != 1 && lbu(m + 0x20) != 9) {
        class638_group_timers(m, 300, 360);
        if (hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) != CLASS_WRENCH) sw(pv + 0x308, 0);
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(512.0f)));
        const float fall = fpu_mul(lwc1(0x0015ed70), 20.0f);
        swc1(pv + 0x98, 0.5f);
        swc1(pv + 0x84, 0.0f);
        swc1(pv + 0x80, fall);
        sw(pv + 0x94, 1);
        sb(pv + 0xad, 0);
        switch (reaction) {  // (the level's jump table of twelve reactions)
        case 9: case 10:  // (L16:0x002d3a2c)
            sb(pv + 0x67, 0xfa);
            break;
        case 3: case 4: case 5: case 6: case 7: case 8: {  // (L16:0x002d3a3c)
            const float speed = fpu_mul(dt(), 6.0f);
            swc1(pv + 0x8c, speed);
            swc1(pv + 0x88, speed);
            knocked(m, pv, hit, knock, yaw_a, 6);
            sb(m + 0x20, 8);
            sb(pv + 0x67, 0x78);
            break;
        }
        case 1: case 2: {  // (L16:0x002d3adc)
            if (lw(0x0015ed84) == 0x12 && lw(pv + 0x308) != 0) {
                const int32_t kills = lw(0x0015ee10) + 1;
                sw(0x0015ee10, static_cast<uint32_t>(kills));
                if (kills >= 10 && lbu(0x0013d425) == 0) {
                    sb(0x0013d425, 1);
                    sound_play_indexed(1, 0, 0);
                    sub_00263d40(TXT(0x53d6), -1);
                }
            }
            swc1(pv + 0x80, fpu_mul(lwc1(0x0015ed70), 20.0f));
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            if (lbu(0x001413f4) == 2) {
                const float step = dt();
                swc1(m + 0x58, 0.5f);
                swc1(pv + 0x88, fpu_mul(step, 24.0f));
                swc1(pv + 0x8c, fpu_mul(step, 8.0f));
            } else {
                const float speed = fpu_mul(dt(), 8.0f);
                swc1(pv + 0x8c, speed);
                swc1(pv + 0x88, speed);
            }
            knocked(m, pv, hit, knock, yaw_b, 7);
            sb(m + 0x20, 9);
            sb(pv + 0x67, 0xf0);
            moby_drop_bolts(m, 0, -1);
            break;
        }
        default:  // (0, 11 and past the table)
            break;
        }
        color_pulse_start(m, pulse);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2e0)));
    if (lw(pv + 0x38) != 0) sw(pv + 0x2e0, static_cast<uint32_t>(rand_frames(180.0f, 240.0f)));
    const int32_t looking = lw(pv + 0x2e0);
    sw(pv + 0x38, 0);
    const float range = looking != 0 ? 255.0f : lwc1(pv + 0x124);
    const uint32_t point = pv + 0xd0;
    if (lw(0x0013f350 + 0x208c) == 0x16) {
        sw(pv + 0x114, 2);
    } else {
        const int32_t path = lw(pv + 0x12c);
        if (path >= 0 && looking == 0) {
            const uint32_t p = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(path) << 2));
            target_select(m, point, 0, 0, p + 0x10, lw(p), 24.0f);
        } else {
            target_select_any(m, point, 24.0f);
        }
    }
    if (range < vec2_distance(V(m + 0x10), V(point))) sw(pv + 0x114, 2);
    if (lwu(pv + 0x110) == 0) sw(pv + 0x110, lwu(0x0013f350 + 0x2080));
    ee_stack_free(0x90);
}

// Its hits first; its aim the moby it aims at (the hero's place for the hero's moby or a moby of class 0x1a3). Its states: 0
// sets it up (the bob, a path point, a side) and goes to 2 (or attached: 10), 1 returns to its post (+0x2b0), 2 waits (its
// timer; with a target it turns to it (4) at random, else it goes on along its path: 3, or 7 when the timer is up), 3 / 7
// move to the path point, 4 turns to the target and aims (5), 5 fires three shots a burst at the animation's frames 2, 17
// and 32 (three bursts, then back to 2), 8 is knocked back, 9 falls and bursts where it lands, 10 rides the moby it is
// attached to. Then (by the level's switches) the bob about the hover point and its sound, the joints' aim, the sparks; on
// Veldin it leaves during the scene of the states 3 .. 5.
// orig: L16:0x002d2cf0 L18:0x002dc918
void class638_update(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t aim = sp, tmp = sp + 0x10, dir = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    class638_hits(m);
    {
        const uint32_t t = lwu(pv + 0x110);
        const int16_t cls = static_cast<int16_t>(lh(t + 0xa6));
        lq_sq(aim, cls == 0 || cls == 0x1a3 ? 0x0013f410u : t + 0x10);
    }
    if (lbu(0x0015edb7) != 0) moby_set_70(pv + 0x130, 2.5f);
    bool retired = false;
    switch (lbu(m + 0x20)) {  // (the level's jump table of eleven states)
    case 0: {  // (L16:0x002d2d90)
        sw(pv + 0x2ec, 0);
        sb(pv + 0x29, 1);
        sh(pv + 0x24, 3);
        sb(pv + 0x28, 2);
        swc1(pv + 0x30, 1.5f);
        swc1(pv + 0x20, 3.0f);
        swc1(pv + 0x2f4, rand_angle());
        swc1(pv + 0x2fc, rand_angle());
        swc1(pv + 0x2e8, rand_angle());
        swc1(pv + 0x2f0, fpu_mul(fpu_mul(rand_range_f(90.0f, 180.0f), DEG), dt()));
        swc1(pv + 0x2f8, fpu_mul(fpu_mul(rand_range_f(90.0f, 180.0f), DEG), dt()));
        swc1(pv + 0x2e4, fpu_mul(fpu_mul(rand_range_f(90.0f, 180.0f), DEG), dt()));
        sh(pv + 0x302, static_cast<uint32_t>(rand_frames(180.0f, 360.0f)));
        {
            const int32_t side = rand_mod(2);
            const uint32_t bits = lhu(m + 0x34);
            sh(m + 0x34, side != 0 ? (bits | 0x8000u) : (bits & 0x7fffu));
        }
        const int32_t path = lw(pv + 0x120);
        if (path >= 0) {
            const uint32_t p = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(path) << 2));
            const int32_t i = float_to_int(rand_range_f(0.0f, static_cast<float>(lw(p) - 1)));
            sh(pv + 0x300, static_cast<uint32_t>(i));
            lq_sq(m + 0x10, path_point(lw(pv + 0x120), static_cast<int16_t>(i)));
        }
        sw(pv + 0x308, 1);
        lq_sq(pv + 0x2c0, m + 0x10);
        lq_sq(pv + 0x2b0, m + 0x10);
        if (lw(pv + 0x128) == 0) {
            sb(m + 0x20, 2);
            anim_to(m, 0, 10);
        } else {
            sb(m + 0x20, 10);
            sb(m + 0x31, 0);
            sh(m + 0x34, (lhu(m + 0x34) | 1u) & 0xefffu);
            sw(m + 0x94, 0);
            anim_to(m, 1, 0x14);
        }
        break;
    }
    case 10: {  // (L16:0x002d2f7c)
        const uint32_t ride = lwu(pv + 0x2d0);
        if (ride == 0) break;
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        mat3_transform(V(tmp), V(pv + 0x2c0), M(ride + 0xc0));
        vec3_add(V(m + 0x10), V(tmp), V(lwu(pv + 0x2d0) + 0x10));
        swc1(m + 0x48, angle_add(lwc1(lwu(pv + 0x2d0) + 0x48), lwc1(pv + 0x2cc)));
        break;
    }
    case 1: {  // (L16:0x002d2fdc)
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2dc))) == 0) {
            vec3_set_length(V(tmp), V(m + 0xc0), fpu_mul(static_cast<float>(lh(pv + 0x2dc)), 0.1f));
            vec3_add(V(tmp), V(tmp), V(m + 0x10));
            class638_seek(m, tmp);
            break;
        }
        turn_to(m, pv, lwc1(pv + 0x2b0), lwc1(pv + 0x2b4));
        const float d = class638_seek(m, pv + 0x2b0);
        anim_to(m, 0, 10);
        if (d < 1.0f) sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (L16:0x002d30f4)
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2dc)));
        if (lw(pv + 0x114) == 2) {  // (no target: along its path)
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x302))) == 0) break;
            if (lw(pv + 0x120) < 0) break;
            sh(pv + 0x300, static_cast<uint32_t>(other_point(pv)));
            sb(m + 0x20, 3);
            sh(pv + 0x302, static_cast<uint32_t>(frame_scale_i(0xb4)));
            class638_turn(m);
            break;
        }
        if (lh(pv + 0x2dc) != 0) break;
        if (lw(pv + 0x120) >= 0) {
            sb(m + 0x20, 7);
            sh(pv + 0x2dc, static_cast<uint32_t>(rand_frames(30.0f, 90.0f)));
            sh(pv + 0x300, static_cast<uint32_t>(other_point(pv)));
            class638_turn(m);
            break;
        }
        if (rand_mod(20) != 0) break;
        sb(m + 0x20, 4);
        anim_to(m, 2, 10);
        break;
    }
    case 3: {  // (L16:0x002d3214)
        const float d = class638_seek(m, path_point(lw(pv + 0x120), lh(pv + 0x300)));
        if (d < 0.01f || timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x302))) != 0) {
            sb(m + 0x20, 2);
            anim_to(m, 0, 10);
            sh(pv + 0x302, static_cast<uint32_t>(rand_frames(180.0f, 360.0f)));
        }
        if (lwc1(pv + 0x2d4) < fpu_add(dt(), dt()) && d < 0.5f && lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x1e));
        break;
    }
    case 4:  // (L16:0x002d3320)
        turn_to(m, pv, lwc1(aim), lwc1(aim + 4));
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 5);
        anim_to(m, 3, 10);
        break;
    case 5: {  // (L16:0x002d33b0)
        turn_to(m, pv, lwc1(aim), lwc1(aim + 4));
        if (anim_crossed(m, 2.0f) != 0 || anim_crossed(m, 17.0f) != 0 || anim_crossed(m, 32.0f) != 0) {
            moby_chain_point_v(m, 0, V(tmp));
            vec3_sub(V(dir), V(aim), V(tmp));
            vec3_set_length(V(dir), V(dir), fpu_mul(dt(), 20.0f));
            class49_spawn(m, dir, tmp);
            break;
        }
        if (anim_crossed(m, 10.0f) == 0) break;
        const uint32_t bursts = lhu(pv + 0x2de) + 1u;
        sh(pv + 0x2de, bursts);
        if (static_cast<int16_t>(bursts) < 3) break;
        sb(m + 0x20, 2);
        anim_to(m, 0, 10);
        sh(pv + 0x2dc, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sh(pv + 0x2de, 0);
        break;
    }
    case 7: {  // (L16:0x002d3520)
        const float d = class638_seek(m, path_point(lw(pv + 0x120), lh(pv + 0x300)));
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2dc)));
        if (d < 0.01f || lh(pv + 0x2dc) == 0) {
            sb(m + 0x20, 4);
            anim_to(m, 2, 0x1e);
        }
        if (d < 0.5f && lwc1(pv + 0x2d4) < fpu_add(dt(), dt())) anim_to(m, 2, 0x1e);
        break;
    }
    case 8:  // (L16:0x002d3610)
        if ((lbu(m + 0x70) & 2) != 0) {
            sb(m + 0x20, 2);
            anim_to(m, 0, 10);
        } else {
            vec3_set_length(V(tmp), V(m + 0xc0), -dt());
            vec3_add(V(m + 0x10), V(m + 0x10), V(tmp));
        }
        break;
    case 9: {  // (L16:0x002d3684)
        lq_sq(tmp, m + 0x10);
        if ((enemy_physics(m, pv + 0x70) & 0x160u) != 0) {
            class638_burst(m);
            moby_sound_start(0, 0, m);
            moby_retire(m);
            retired = true;
            break;
        }
        vec3_sub(V(tmp), V(m + 0x10), V(tmp));
        vec3_add(V(pv + 0x2c0), V(pv + 0x2c0), V(tmp));
        break;
    }
    default:  // (6 and past the table: nothing)
        break;
    }
    if (retired) {
        ee_stack_free(0xa0);
        return;
    }

    const uint32_t on = switches();
    if (lw(on) != 0) {
        swc1(pv + 0x2bc, lwc1(m + 0x18));
        const uint32_t st = lbu(m + 0x20);
        if (st != 0 && st != 10) {
            approach_f(pv + 0x2ec, 0.25f, fpu_mul(dt(), 0.25f));
            swc1(pv + 0x2f4, angle_add(lwc1(pv + 0x2f4), lwc1(pv + 0x2f0)));
            swc1(pv + 0x2fc, angle_add(lwc1(pv + 0x2fc), lwc1(pv + 0x2f8)));
            swc1(pv + 0x2e8, angle_add(lwc1(pv + 0x2e8), lwc1(pv + 0x2e4)));
            swc1(m + 0x10, fpu_add(lwc1(pv + 0x2c0), fpu_mul(lwc1(pv + 0x2ec), sin_vu(lwc1(pv + 0x2f4)))));
            swc1(m + 0x14, fpu_add(lwc1(pv + 0x2c4), fpu_mul(lwc1(pv + 0x2ec), sin_vu(lwc1(pv + 0x2fc)))));
            swc1(m + 0x18, fpu_add(lwc1(pv + 0x2c8), fpu_mul(lwc1(pv + 0x2ec), sin_vu(lwc1(pv + 0x2e8)))));
        }
        if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 32.0f && sub_0028d8c0(m, lw(pv + 0x304)) == 0)
            sw(pv + 0x304, static_cast<uint32_t>(moby_sound_start(7, 4, m)));
    }
    if (lw(on + 4) != 0) class638_aim(m);
    if (lw(on + 8) != 0 && lw(0x001413dc) != 0x16) class638_sparks(m);
    if (lw(0x0015ed84) == 0x12 && lw(OV(0x0015f5c4)) == 2 && lwu(OV(0x0016c890)) - 3u < 3u) moby_retire(m);
    ee_stack_free(0xa0);
}

}  // namespace rac1
