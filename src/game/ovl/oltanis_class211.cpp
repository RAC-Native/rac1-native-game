// Oltanis (level 14), class 211: a drone that waits on its pad until camera type 20 (0x14) is the live camera, rises
// along its path (+0x84, from the pad to the path's first point, timed by +0x6c / +0x70), climbs to its rail (pvar
// +0x60: a grind rail's path, the rail record's +0x10), rides it (5) and then chases the hero along it (6): it keeps to the
// rail's point nearest the hero, a little behind (+0x74), springs toward it, turns its gun (chain 4) toward the hero and
// sweeps a beam (the class 1471 effect: fx1471_claim / fx1471_set) that hurts what it meets (a hit record handed to
// segment_collide_v) in sub-state 3, with a glow and a light while the beam opens (2) and closes (4). It blows up, dropping
// a class 28 shell and rocket, when it reaches its end box (+0x1cc). Every frame but the first two it trails a jet of
// particles from chain 1 (with a light while it rises) and queues its draw callback (L14:0x2d71f0, still generated).
// Pvars: +0x60 the rail, +0x64 / +0x68 the rail cursor (segment, fraction), +0x6c the timer, +0x70 its inverse, +0x74 the
// lag behind the hero, +0x78 the rail's length from the drone's way on, +0x80 the wait before rising, +0x84 the rise
// path, +0x88 the climb path (3 points), +0x8c / +0x8e the climb's segments on the rail, +0x90 .. +0x98 the springs'
// speeds, +0xa0 the gun's matrix (chain 0), +0xd0 the beam's start, +0xe0 the jet's place, +0xf0 the light, +0x100 /
// +0x140 / +0x180 joint nodes (chains 2, 3, 4), +0x110 / +0x150 / +0x190 their rotations, +0x1c0 / +0x1c4 the beam's
// timers, +0x1c8 the pause before the next sweep, +0x1cc the end box, +0x1d0 the gun's rotation at the sweep's start,
// +0x1e0 the beam's end, +0x200 chain 6's point (the draw callback's). m+0xbc is the beam's sub-state (0 idle, 1 start,
// 2 opening, 3 firing, 4 closing, 5 pausing).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class28.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;
constexpr float kMaxTurn = 1.2217304706573486f;  // 70 degrees (0x3f9c61aa)

// the path of the drone's rail (pvar +0x60: a grind rail; its record's +0x10)
uint32_t rail_path(uint32_t pv) { return lwu(lvd::rail(lw(pv + 0x60)) + 0x10); }

// a segment's length: the w lane of its first point
float segment_length(uint32_t path, int32_t k) { return lwc1(path + 0x1cu + (static_cast<uint32_t>(k) << 4)); }

float clamp_turn(float a) {
    if (kMaxTurn < a) return kMaxTurn;
    if (a < -kMaxTurn) return -kMaxTurn;
    return a;
}

// a light record of the level's light table (0x1809c0, 0x20 each) moved to `pos` with the radius `radius`
uint32_t light_record(int32_t light) { return L14_ADDR(0x001809c0) + (static_cast<uint32_t>(light) << 5); }

// orig: L14:0x002d6358 -- the rail's segments where the climb path's last two points meet it, and the rail's length
// between them (the lag starts at 0x161a38)
void class211_rail_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t rail = rail_path(pv);
    swc1(pv + 0x74, lwc1(L14_ADDR(0x00161a38)));
    sw(pv + 0x68, 0);
    sw(pv + 0x64, 0);
    const float h = lwc1(pv + 0x68);
    Vec4 near{};
    int32_t seg = 0;
    float frac = h;
    path_closest_point_v(rail, V(lvd::path(lw(pv + 0x88)) + 0x30), near, &seg, &frac, 0, 20.0f, 5.0f, h);
    sh(pv + 0x8c, static_cast<uint16_t>(seg));
    seg = 0;
    frac = h;
    path_closest_point_v(rail, V(lvd::path(lw(pv + 0x88)) + 0x20), near, &seg, &frac, 0, 20.0f, 5.0f, h);
    sh(pv + 0x8e, static_cast<uint16_t>(seg));
    const int32_t from = static_cast<int16_t>(seg);
    const int32_t to = lh(pv + 0x8c);
    swc1(pv + 0x78, h);
    for (int32_t k = from; k < to; ++k) swc1(pv + 0x78, fpu_add(lwc1(pv + 0x78), segment_length(rail, k)));
}

// orig: L14:0x002d64d0 -- the rise: the drone between the rail's first point and the rise path's first point by the
// timer; true once the timer is out (or idle)
bool class211_rise_step(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t rail = rail_path(pv);
    const bool done = 0 < timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x6c)));
    const float t = fpu_mul(int_to_float(lw(pv + 0x6c)), lwc1(pv + 0x70));
    vec3_lerp(V(m + 0x10), V(rail + 0x10), V(lvd::path(lw(pv + 0x84)) + 0x10), t);
    return done;
}

// orig: L14:0x002d7198 -- along the rail at 0x161a30 a second
void class211_ride(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t seg = lw(pv + 0x64);
    float frac = lwc1(pv + 0x68);
    path_advance_v(rail_path(pv), V(m + 0x10), &seg, &frac, 0, fpu_mul(lwc1(L14_ADDR(0x00161a30)), lwc1(0x0015ed6cu)));
    sw(pv + 0x64, static_cast<uint32_t>(seg));
    swc1(pv + 0x68, frac);
}

// orig: L14:0x002d67e8 -- the chase along the rail, the gun's aim and the beam's sub-states
void class211_chase(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t rail = rail_path(pv);
    const uint32_t frame = ee_stack_alloc(0x1c0);  // (the original's frame: the matrices and the beam's hit record live in it)
    const uint32_t at = frame + 0x10;             // the rail point ahead, then the hero's offset
    const uint32_t hit = frame + 0x180;            // the beam's hit record, then the glow's place

    // the lag behind the hero grows from 0x161a38 toward -3 while the drone is still on its climb's segments
    {
        const int32_t from = lh(pv + 0x8e);
        const int32_t seg = lw(pv + 0x64);
        if (!(seg < from)) {
            float run = 0.0f;
            const float frac = lwc1(pv + 0x68);
            const float total = lwc1(pv + 0x78);
            const float lag0 = lwc1(L14_ADDR(0x00161a38));
            for (int32_t k = from; k < seg; ++k) run = fpu_add(run, segment_length(rail, k));
            float t = fpu_div(fpu_add(run, frac), total);
            if (1.0f < t) t = 1.0f;
            swc1(pv + 0x74, fpu_add(lag0, fpu_mul(fpu_sub(-3.0f, lag0), t)));
        }
    }
    sw(pv + 0x68, 0);
    const float zero_h = lwc1(pv + 0x68);
    sw(pv + 0x64, 0);
    {
        Vec4 near{};
        int32_t seg = 0;
        float frac = zero_h;
        path_closest_point_v(rail, V(HERO + 0x80), near, &seg, &frac, 0, 20.0f, 5.0f, zero_h);
        sw(pv + 0x64, static_cast<uint32_t>(seg));
        swc1(pv + 0x68, frac);
    }
    {
        int32_t seg = lw(pv + 0x64);
        float frac = lwc1(pv + 0x68);
        path_advance_v(rail, V(frame), &seg, &frac, 0, -lwc1(pv + 0x74));
        sw(pv + 0x64, static_cast<uint32_t>(seg));
        swc1(pv + 0x68, frac);
    }
    spring_to(m + 0x10, pv + 0x90, lwc1(frame + 0x0), 0.01f, u2f(0x3e4ccccdu), zero_h);
    spring_to(m + 0x14, pv + 0x94, lwc1(frame + 0x4), 0.01f, u2f(0x3e4ccccdu), zero_h);
    spring_to(m + 0x18, pv + 0x98, lwc1(frame + 0x8), 0.01f, u2f(0x3e4ccccdu), zero_h);
    {  // facing along the rail, five points on
        const uint32_t p0 = rail + (static_cast<uint32_t>(lw(pv + 0x64)) << 4);
        const uint32_t p5 = rail + (static_cast<uint32_t>(lw(pv + 0x64) + 5) << 4);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(p5 + 0x10), lwc1(p0 + 0x10)), fpu_sub(lwc1(p5 + 0x14), lwc1(p0 + 0x14))));
    }
    if (lbu(m + 0xbc) == 5 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c8))) != 0) sb(m + 0xbc, 1);

    if (lbu(m + 0xbc) == 1) {  // a sweep starts: the gun's rotation now (chain 5's frame in chain 4's)
        sw(pv + 0x1c0, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161a3c)))));
        sw(pv + 0x1c4, 0);
        sb(m + 0xbc, 2);
        Mat4 gun{}, body{};
        moby_chain_world_one_v(m, 4, gun);
        moby_chain_world_one_v(m, 5, body);
        vec3_set_length(body.r[0], body.r[0], 1.0f);
        vec3_set_length(body.r[1], body.r[1], 1.0f);
        vec3_set_length(body.r[2], body.r[2], 1.0f);
        Mat4 inv{};
        mat3_transpose(inv, body);
        inv.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        Mat4 rel{};
        mat4_mul(rel, inv, gun);
        mat_to_quat(V(pv + 0x1d0), rel);
    }

    const uint8_t beam = lbu(m + 0xbc);
    float face = lwc1(m + 0x48);
    if (static_cast<uint32_t>(beam - 2) < 3) {
        const uint32_t gun = pv + 0xa0;
        const uint32_t beam_end = pv + 0x1e0;
        moby_chain_world_one(m, 0, gun);
        float open = 1.0f;
        {  // the beam's end along the rail ahead: the farther the more the beam has run
            const float run = fpu_div(int_to_float(lw(pv + 0x1c0)), int_to_float(frame_scale_i(lw(L14_ADDR(0x00161a3c)))));
            const float k0 = lwc1(L14_ADDR(0x00161a40));
            int32_t seg = lw(pv + 0x64);
            float frac = lwc1(pv + 0x68);
            path_advance_v(rail, V(at), &seg, &frac, 0, fpu_add(k0, fpu_mul(fpu_sub(1.0f, k0), run)));
        }
        V(beam_end) = V(at);
        Vec4 dir{};
        vec3_sub(dir, V(at), V(m + 0x10));
        face = atan2_f(dir.x, dir.y);
        float turn = clamp_turn(angle_sub(face, lwc1(m + 0x48)));
        sw(pv + 0x1c4, lw(pv + 0x1c4) + 1);
        if (frame_scale_i(30) < lw(pv + 0x1c4)) {
            if (lbu(m + 0xbc) == 4) {
                sb(m + 0xbc, 0);
            } else {
                fx1471_claim(m, 3, 1);
                sb(m + 0xbc, 3);
            }
            sw(pv + 0x1c4, static_cast<uint32_t>(frame_scale_i(30)));
        }
        const uint8_t sub = lbu(m + 0xbc);
        if (sub == 2 || sub == 4) {
            open = fpu_div(int_to_float(lw(pv + 0x1c4)), int_to_float(frame_scale_i(30)));
            if (sub == 4) open = fpu_sub(1.0f, open);
            turn = fpu_add(fpu_mul(fpu_sub(turn, 0.0f), open), 0.0f);
        }
        quat_from_axis_angle(V(pv + 0x150), 2, turn);
        {  // the gun's rotation toward the beam's end, relative to chain 5's frame
            Vec4 tip{};
            moby_chain_point_v(m, 4, tip);
            const Vec4 up{0.0f, 0.0f, 1.0f, 1.0f};
            Mat4 look{};
            vec3_set_length(look.r[1], dir, 1.0f);
            vec3_cross_ba(look.r[2], look.r[1], up);
            vec3_set_length(look.r[2], look.r[2], 1.0f);
            vec3_cross_ba(look.r[0], look.r[2], look.r[1]);
            look.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
            Mat4 body{};
            moby_chain_world_one_v(m, 5, body);
            vec3_set_length(body.r[0], body.r[0], 1.0f);
            vec3_set_length(body.r[1], body.r[1], 1.0f);
            vec3_set_length(body.r[2], body.r[2], 1.0f);
            Mat4 inv{};
            mat3_transpose(inv, body);
            inv.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
            Mat4 rel{};
            mat4_mul(rel, inv, look);
            const Vec4 grow{0.0f, fpu_div(512.0f, lwc1(m + 0x2c)), 0.0f, 1.0f};
            Vec4 q{};
            mat_to_quat(q, rel);
            if (sub == 2 || sub == 4) quat_nlerp(V(pv + 0x190), V(pv + 0x1d0), q, open);
            else V(pv + 0x190) = q;
            sb(pv + 0x183, 1);
            swc1(pv + 0x18c, 1.0f);
            V(pv + 0x1a0) = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
            V(pv + 0x1b0) = grow;
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c0))) != 0) {
            sb(m + 0xbc, 4);
            sw(pv + 0x1c4, 0);
        }
        const uint8_t now = lbu(m + 0xbc);
        if (now == 3) {  // firing: the beam, and what it meets hit (damage 5627.92, the drone's class)
            if (fx1471_set(m, gun, beam_end) == 0) fx1471_claim(m, 3, 1);
            swc1(hit + 0x0, cos_vu(lwc1(m + 0x48)));
            swc1(hit + 0x4, sin_vu(lwc1(m + 0x48)));
            swc1(hit + 0x8, 1.0f);
            swc1(hit + 0xc, u2f(0x45afdf66u));
            sw(hit + 0x10, m);
            sw(hit + 0x14, 0x00010001u);
            sb(hit + 0x18, 0);
            sb(hit + 0x19, 1);
            sh(hit + 0x1a, lhu(m + 0xa6));
            swc1(hit + 0x1c, 1.0f);
            sw(hit + 0x20, 1);
            segment_collide_v(V(pv + 0xd0), V(beam_end), 9, m, static_cast<const uint32_t*>(ee_ram_ptr(hit)));
        } else if (now == 2 || now == 4) {  // opening / closing: a glow at the beam's start and a light
            Vec4 g{};
            vec3_scale(g, V(gun), lwc1(L14_ADDR(0x00161a44)));
            vec3_add(V(hit), g, V(pv + 0xd0));
            const uint32_t p = spawn_particle_45(hit, L14_ADDR(0x0015f580), 0x7f, m);
            if (p != 0) {
                swc1(p + 0xc, fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x00161a48)), 1000.0f), open), 1000.0f));
                sh(p + 0x36, 1);
                sh(p + 0x34, static_cast<uint16_t>(float_to_int(fpu_add(fpu_mul(open, 111.0f), 16.0f))));
                const int32_t life = frame_scale_i(4);
                sh(p + 0xa, static_cast<uint16_t>(life));
                swc1(p + 0x30, fpu_div(1.0f, int_to_float(static_cast<int16_t>(life))));
                sw(p + 0x38, 0x007f7f7fu);
            }
            const uint32_t lp = frame + 0x190;
            V(lp) = V(pv + 0xd0);
            swc1(lp + 0x8, fpu_sub(lwc1(lp + 0x8), 0.5f));
            const float radius = fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x00161a4c)), 0.1f), open), 0.1f);
            if (lw(pv + 0xf0) == -1) {
                sw(pv + 0xf0, static_cast<uint32_t>(light_add(lp, radius, 0.0f, lwc1(L14_ADDR(0x00161a50)),
                                                              lwc1(L14_ADDR(0x00161a54)), lwc1(L14_ADDR(0x00161a58)))));
            }
            if (lw(pv + 0xf0) >= 0) {
                const uint32_t rec = light_record(lw(pv + 0xf0));
                V(rec + 0x10) = V(lp);
                swc1(rec + 0x1c, radius);
                swc1(rec + 0x8, lwc1(L14_ADDR(0x00161a58)));
                swc1(rec + 0x0, lwc1(L14_ADDR(0x00161a50)));
                swc1(rec + 0x4, lwc1(L14_ADDR(0x00161a54)));
            }
        } else if (lw(pv + 0xf0) != -1) {
            region_clear(lwu(pv + 0xf0));
            sw(pv + 0xf0, 0xffffffffu);
        }
    } else if (lw(pv + 0xf0) != -1) {
        region_clear(lwu(pv + 0xf0));
        sw(pv + 0xf0, 0xffffffffu);
    }

    // the head (chain 2) turned toward the hero, at most 70 degrees off the drone's way
    Vec4 to_hero{};
    vec3_sub(to_hero, V(HERO + 0x80), V(m + 0x10));
    quat_from_axis_angle(V(pv + 0x110), 2, clamp_turn(angle_sub(atan2_f(to_hero.x, to_hero.y), face)));
    ee_stack_free(0x1c0);
}

// orig: L14:0x002d7490 -- every frame from state 2 on: the jet's particles from chain 1 (and its light while the drone
// rises: states 3 .. 5), chain 6's point for the draw callback, the callback queued
void class211_jet(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0x30);
    const uint32_t at = frame;          // chain 1's point (light_add and spawn_particle_45 take it by address)
    const uint32_t drift = frame + 0x20;
    moby_chain_point_v(m, 1, V(at));
    {
        Vec4 back{};
        vec3_sub(back, V(at), V(L14_ADDR(0x001674c0)));
        vec3_set_length(back, back, -0.5f);
        vec3_add(V(pv + 0xe0), V(at), back);
    }
    if (static_cast<uint32_t>(lbu(m + 0x20) - 3) < 3) {
        swc1(at + 0x8, fpu_add(lwc1(at + 0x8), 1.0f));
        if (lw(pv + 0xf0) == -1) sw(pv + 0xf0, static_cast<uint32_t>(light_add(at, lwc1(L14_ADDR(0x00161a60)), 0.0f, 0.5f, 0.5f, 1.0f)));
        if (lw(pv + 0xf0) >= 0) {
            const uint32_t rec = light_record(lw(pv + 0xf0));
            V(rec + 0x10) = V(at);
            swc1(rec + 0x1c, lwc1(L14_ADDR(0x00161a60)));
        }
        swc1(at + 0x8, fpu_sub(lwc1(at + 0x8), 1.0f));
    }
    rand_dir_scaled(drift, u2f(0x3ba3d70au), u2f(0x3cf5c28fu));
    {
        const uint32_t p = spawn_particle_45(at, drift, 0x7f, m);
        if (p != 0) swc1(p + 0xc, rand_range_f(6000.0f, 32000.0f));
    }
    for (int k = 0; k < 3; ++k) {
        const uint32_t p = spawn_particle_45(at, L14_ADDR(0x0015f580), 0x7f, m);
        if (p == 0) continue;
        if (k == 2 && rand_mod(8) == 0) swc1(p + 0xc, 180000.0f);
        else swc1(p + 0xc, rand_range_f(80000.0f, 120000.0f));
        const int32_t life = frame_scale_i(2);
        sh(p + 0xa, static_cast<uint16_t>(life));
        sh(p + 0x36, 3);
        sw(p + 0x38, 0x007f7f7fu);
        swc1(p + 0x30, fpu_div(1.0f, int_to_float(static_cast<int16_t>(life))));
    }
    moby_chain_point_v(m, 6, V(pv + 0x200));
    draw_callback_add(L14_ADDR(0x002d71f0), m);
    ee_stack_free(0x30);
}

}  // namespace

// orig: L14:0x002d5f40
void olt_class211_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    // jump table L14:0x001fc530 (7 entries)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d5f94) the set-up: a rail with points, a climb path of 3 points, an end box; else retired
        const int32_t rail = lw(pv + 0x60);
        if (rail == -1 || lw(lwu(lvd::rail(rail) + 0x10)) == 0 || lw(pv + 0x88) == -1 || lw(lvd::path(lw(pv + 0x88))) != 3 ||
            lw(pv + 0x1cc) == -1) {
            moby_retire(m);
            return;
        }
        class211_rail_init(m);
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        sh(m + 0x34, static_cast<uint16_t>((lhu(m + 0x34) & 0xefffu) | 0x41u));
        sw(pv + 0xf0, 0xffffffffu);
        sw(pv + 0xf4, 0xffffffffu);
        mem_fill32(pv + 0x100, 0, 0x40);
        sub_0024f590(m, 2, pv + 0x100);
        mem_fill32(pv + 0x140, 0, 0x40);
        sub_0024f590(m, 3, pv + 0x140);
        mem_fill32(pv + 0x180, 0, 0x40);
        sub_0024f590(m, 4, pv + 0x180);
        return;
    }
    case 1:  // (0x002d6090) waiting for camera type 20
        if (lh(lwu(L14_ADDR(0x00167500)) + 0x86) != 0x14) return;
        sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x80))));
        sb(m + 0x20, 2);
        return;
    case 2:  // (0x002d60c0) the wait, then shown at the rise path's start
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x6c))) == 0) return;
        sh(m + 0x34, static_cast<uint16_t>((lhu(m + 0x34) | 0x1000u) & 0xffbeu));
        sb(m + 0x20, 3);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        moby_set_anim(m, 2, 0);
        swc1(m + 0x58, 0.5f);
        sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161a2c)))));
        swc1(pv + 0x70, fpu_div(1.0f, int_to_float(lw(pv + 0x6c))));
        V(m + 0x10) = V(lvd::path(lw(pv + 0x84)) + 0x10);
        [[fallthrough]];
    case 3:  // (0x002d6158) unfolding: the rise starts at the animation's frame 3; at its end the climb
        if (3.0f <= moby_anim_time(m)) class211_rise_step(m);
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == lbu(m + 0x53)) {
            sb(m + 0x20, 4);
            moby_anim_set(m, 0, 0, 10);
            swc1(m + 0x58, 1.0f);
        }
        break;
    case 4:  // (0x002d61cc) the rise to the rail
        if (class211_rise_step(m)) {
            sb(m + 0x20, 5);
            sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161a34)))));
        }
        break;
    case 5:  // (0x002d61f0) along the rail
        class211_ride(m);
        break;
    case 6: {  // (0x002d6200) the chase; at the end box it blows up
        class211_chase(m);
        if (point_in_box(m + 0x10, lw(pv + 0x1cc)) == 0) break;
        const uint32_t at = ee_stack_alloc(0x10);
        moby_chain_point_v(m, 2, V(at));
        const uint32_t shell = class28_spawn_shell(m, at);
        if (shell != 0) {
            const uint32_t spv = lwu(shell + 0x78);
            const float vx = fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed60u), u2f(0x3ecccccdu)));
            const float vy = fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(lwc1(0x0015ed60u), u2f(0x3ecccccdu)));
            swc1(spv + 0x0, vx);
            swc1(spv + 0x4, vy);
        }
        class28_spawn_rocket(m, L14_ADDR(0x00161a5c), 0);
        explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, 1, 1, 5, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        ee_stack_free(0x10);
        moby_retire(m);
        return;
    }
    default:
        break;
    }
    class211_jet(m);
}

}  // namespace rac1
