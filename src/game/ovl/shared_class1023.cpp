// Class 1023 of Batalia (L8) and Gaspar (L9): one port of the two levels' copies, which are the same code relinked (word by
// word: only the relocations differ; the jump tables' entries the same offsets). Its addresses are the base ELF's (bare: the
// hero 0x13f350.., his moby 0x1413d0, the frame time 0x15ed6c / its square 0x15ed70), the engine's (OV: the camera
// 0x166dc0, the game mode 0x15f5c4, the difficulty's two factors 0x15f5d0 / 0x15f5d4, the path table 0x1b04b0, the
// segment's hit moby 0x173e58, the zero vector 0x15f580) or the level's own: one block of the class's numbers (tuning()).
// The jump tables (the states, the hit reactions) are switches on the index. Header shared_class1023.h.
//
// The pvars: the hit record +0x20.. (the health +0x20), the colour pulse +0x60.. (+0x67 its time), the move record +0x70..,
// the walker record +0xd0.. (+0xe0 the point reached), the target's point +0x120, the target +0x160 and its kind +0x164
// (2: none in sight), the size's joint record +0x170, the leap's start +0x1c0, the patrol path +0x1d0 (its index), the home
// path +0x1d4, the alert timer +0x1d8 (a half) and the sight range +0x1dc (+0x1e4 its base), the turn speed +0x1e0, the
// shots' spread +0x1e8, the fire timer +0x1ec, the patrol timer +0x1f0, the path point +0x1f4, the glow moby +0x1f8, the
// patrol path +0x1fc (-1 none), the leaps +0x1da, the patrol's direction +0x20c, the shot's index +0x20e.
#include <game/ovl/shared_class1023.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t GLOW_CLASS = 0x401;  // the glow moby it carries (class 1025)
constexpr uint32_t SHOT_CLASS = 0x50c;  // its shot (class 1292)

// The class's numbers (the level's own block): +0 the size's scale, +4 the knock-back's gravity, +0xc / +0x10 / +0x14 the
// knock-backs' speeds, +0x30 the walk's speed, +0x34 / +0x38 the leap's walker numbers, +0x3c the fire interval (frames)
// and +0x40 the spread's step, +0x44 the shots' speed, +0x48 the strike's range, +0x4c the leaps before a pause, +0x50 the
// leap's height.
uint32_t tuning(uint32_t o) {
    if (on_level(GASPAR)) return L9_ADDR(0x00161bd0) + o;
    return L8_ADDR(0x00162180) + o;
}

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time2() { return lwc1(0x0015ed70); }
// a path of the level's table (by index)
uint32_t level_path(int32_t i) { return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(i) << 2)); }
// the point `i` of `path`
uint32_t path_point(uint32_t path, int32_t i) { return path + (static_cast<uint32_t>(i) << 4) + 0x10; }
// a pvar timer word ticked
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }
// a random count 0..3 (libc_rand's remainder by 4, as the original's shifts)
int32_t rand4() { return libc_rand() % 4; }

// the yaw turned toward the target (+0x160's moby)
void turn_to(uint32_t m, uint32_t pv, uint32_t target) {
    const float to = atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14)));
    const float accel = fpu_mul(frame_time2(), u2f(0x410ba058));
    yaw_seek(m + 0x48, pv + 0x1e0, to, accel, accel, fpu_mul(frame_time(), u2f(0x41490fdb)));
}

// a patrol step past the path's ends: the direction turned and the point two back
void patrol_bounce(uint32_t pv, uint32_t path, int32_t at, bool clear_reached) {
    const int32_t dir = lh(pv + 0x20c);
    const int32_t next = at + dir;
    sw(pv + 0x1f4, static_cast<uint32_t>(next));
    if (next < lw(path) && next >= 0) return;
    const uint32_t back = 0u - static_cast<uint32_t>(dir);
    if (clear_reached) sw(pv + 0xe0, 0);
    sh(pv + 0x20c, back);
    sw(pv + 0x1f4, static_cast<uint32_t>(next + (static_cast<int32_t>(back << 16) >> 15)));
}

// the leap toward the other of the home path's two points: its start, the point, the turn's animation
void leap_start(uint32_t m, uint32_t pv) {
    lq_sq(pv + 0x1c0, m + 0x10);
    sw(pv + 0x1f4, (lwu(pv + 0x1f4) + 1) & 1);
    sb(m + 0x20, 4);
}
void leap_turn(uint32_t m, uint32_t pv, uint32_t sp) {
    const uint32_t d = sp + 0x10;
    vec3_sub(V(d), V(path_point(level_path(lw(pv + 0x1d4)), lw(pv + 0x1f4))), V(m + 0x10));
    const uint32_t set = vec3_dot(V(m + 0xd0), V(d)) < 0.0f ? 0x12u : 0x10u;
    if (lbu(m + 0x53) == set) return;
    moby_anim_set(m, set, 0, frame_scale_i(5));
}
// the strike's start (state 7; its animation unless already on)
void strike(uint32_t m) {
    sb(m + 0x20, 7);
    if (lbu(m + 0x53) == 0xd) return;
    moby_anim_set(m, 0xd, 0, rand4() + 8);
}
// the pause after the leaps (state 9)
void pause(uint32_t m, uint32_t pv, bool scaled) {
    sh(pv + 0x1da, 0);
    if (lbu(m + 0x53) != 0xa) {
        if (scaled) {
            const int32_t base = frame_scale_i(7);
            moby_anim_set(m, 0xa, 2, base + rand_range(0, frame_scale_i(5)));
        } else {
            moby_anim_set(m, 0xa, 2, rand4() + 5);
        }
    }
    sb(m + 0x20, 9);
}
}  // namespace

// orig: L8:0x00307190 L9:0x00308e00
uint32_t class1023_shot_spawn(uint32_t vel, uint32_t pos, uint32_t owner, uint32_t target, int32_t life, uint32_t index) {
    const uint32_t s = moby_spawn(SHOT_CLASS);
    if (s == 0) return 0;
    sb(s + 0x30, 0xff);
    sh(s + 0x32, 0x7e);
    sb(s + 0x20, 1);
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x31, 1);
    lq_sq(s + 0x10, pos);
    lq_sq(pv, vel);
    const int32_t frames = frame_scale_i(life);
    sw(pv + 0x10, owner);
    sw(pv + 0x14, target);
    sw(pv + 0x2c, index);
    sw(pv + 0x28, 0);
    sw(pv + 0x18, static_cast<uint32_t>(frames));
    swc1(s + 0x48, atan2_f(lwc1(pv), lwc1(pv + 4)));
    swc1(s + 0x44, -atan2_f(vec2_length(V(pv)), lwc1(pv + 8)));
    moby_update_transform(s);
    return s;
}

// orig: L8:0x003024d8 L9:0x00301d50
void class1023_frame(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t point = sp, turn = sp + 0x10, state = sp + 0x50, damage = sp + 0x54;
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(tuning(0x00))));
    swc1(pv + 0xf4, fpu_mul(lwc1(tuning(0x30)), frame_time()));
    // the glow moby it carries at its joint 4 (spawned the first time)
    if (lwu(pv + 0x1f8) == 0) {
        const uint32_t g = moby_spawn(GLOW_CLASS);
        sw(pv + 0x1f8, g);
        if (g != 0) {
            sh(g + 0x32, 0x40);
            sb(lwu(pv + 0x1f8) + 0x30, 0x40);
            sb(lwu(pv + 0x1f8) + 0x31, 1);
            const uint32_t gg = lwu(pv + 0x1f8);
            sh(gg + 0x34, lhu(gg + 0x34) | 0x100);
            moby_chain_point_v(m, 4, V(point));
            lq_sq(lwu(pv + 0x1f8) + 0x10, point);
        }
        sh(m + 0x34, lhu(m + 0x34) | 0x1000);
    } else {
        moby_chain_point_v(m, 4, V(point));
        moby_chain_world_one(m, 4, turn);
        mat3_copy(M(lwu(pv + 0x1f8) + 0xc0), M(turn));
        const uint32_t g = lwu(pv + 0x1f8);
        lq_sq(g + 0x10, point);
        moby_update_transform(g);
    }

    // the hits: a reaction from the hit record, or (out of health) the death's; the class 71 hits (the hero's?) keep the
    // knock-back
    swc1(damage, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = lw(state);
    float dmg = lwc1(damage);
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(state, static_cast<uint32_t>(st));
    swc1(damage, dmg);
    bool pulse = false;
    if (lw(state) != 1 && lbu(m + 0x20) != 0xb) {
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(damage)));
        uint32_t kind = 3;
        if (lh(lwu(hit + 0x20) + 0xa6) != 0x47) kind = static_cast<uint32_t>(react);
        if (lwc1(pv + 0x20) <= 0.0f) kind = 1;
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(512.0f)));
        swc1(pv + 0x98, 0.5f);
        sw(pv + 0x94, 9);
        swc1(pv + 0x80, fpu_mul(lwc1(tuning(0x04)), frame_time2()));
        sb(pv + 0xad, 0);
        // the yaw away from the hit's source
        auto away = [m, hit]() {
            const uint32_t src = lwu(hit + 0x20);
            return atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14)));
        };
        switch (kind) {  // (the table 0x1fc690 / 0x208f60)
            case 9: case 10:  // (0x3026dc)
                sb(pv + 0x67, 0xfa);
                break;
            case 3: case 4: case 5: case 6: case 7: case 8: {  // knocked back (0x3026e8)
                swc1(pv + 0x88, fpu_mul(lwc1(tuning(0x0c)), frame_time()));
                swc1(pv + 0x8c, fpu_mul(frame_time(), 3.0f));
                swc1(pv + 0xc0, 5.0f);
                swc1(pv + 0xc4, 10.0f);
                swc1(sp + 0x58, away());
                lq_sq(sp, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x58)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
                move_state_start(m, pv + 0x70, 8, 1, 0, lwc1(sp + 0x58));
                sb(m + 0x20, 0xa);
                sb(pv + 0x67, 0x78);
                break;
            }
            case 1: case 2: {  // killed: thrown, its bolts (0x302794)
                const float g = fpu_mul(lwc1(tuning(0x04)), frame_time2());
                const float up = fpu_mul(lwc1(tuning(0x14)), frame_time());
                const float out = fpu_mul(lwc1(tuning(0x10)), frame_time());
                sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                sw(pv + 0x90, 0x200);
                swc1(pv + 0x80, g);
                swc1(pv + 0x88, up);
                swc1(pv + 0xc0, 11.0f);
                swc1(pv + 0xc4, 16.0f);
                swc1(pv + 0x98, 0.25f);
                swc1(pv + 0x8c, out);
                swc1(sp + 0x5c, away());
                lq_sq(sp, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x5c)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
                move_state_start(m, pv + 0x70, 0xc, 1, 0, lwc1(sp + 0x5c));
                sb(m + 0x20, 0xb);
                sb(pv + 0x67, 0xf0);
                bolts_drop(m, 2, 3, 0, -1);
                break;
            }
            default:  // 0, 11 and past the table (0x302884)
                break;
        }
        pulse = true;
    }
    if (pulse) color_pulse_start(m, pv + 0x60);
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);

    // the alert: a longer sight while its timer runs (set again by the hit record's flag +0x38)
    if (lwu(pv + 0x38) != 0) {
        sh(pv + 0x1d8, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1d8)));
    float sight = lwc1(pv + 0x1e4);
    if (lh(pv + 0x1d8) != 0) sight = fpu_add(sight, 6.0f);
    swc1(pv + 0x1dc, sight);

    // the target: picked a quarter of the frames (within the patrol path's polygon, the sight, three units of height), else
    // followed while it lives
    if (rand_mod(4) == 0) {
        const uint32_t poly = level_path(lw(pv + 0x1d0));
        const int32_t r = target_select(m, pv + 0x120, 0, 0, poly + 0x10, lw(poly), lwc1(pv + 0x1dc));
        if (r != 2) {
            if (lwc1(pv + 0x1dc) < vec2_distance(V(m + 0x10), V(pv + 0x120))) sw(pv + 0x164, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
        }
    } else {
        const uint32_t t = lwu(pv + 0x160);
        const uint32_t tst = t != 0 ? lbu(t + 0x20) : 0;
        if (t == 0 || tst == 0xfe || tst == 0xfd) {
            sw(pv + 0x160, 0);
            sw(pv + 0x164, 2);
        } else {
            lq_sq(pv + 0x120, t + 0x10);
        }
    }
    if (lwu(pv + 0x160) == 0) sw(pv + 0x160, lwu(0x001413d0));
    // walking, firing, striking: kept on the floor
    const uint32_t s = lbu(m + 0x20);
    if (s == 0 || s == 5 || s == 7 || s == 4) {
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        if (floor < lwc1(m + 0x18)) swc1(m + 0x18, floor);
    }
    ee_stack_free(0xc0);
}

// orig: L8:0x00301158 L9:0x003009d0
void class1023_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t camera = OV(0x00166dc0);
    if (lbu(m + 0x31) == 0 && 40.0f < vec3_distance(V(m + 0x10), V(camera))) return;
    joint_node_scale(m, 6, pv + 0x170, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(camera)) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    if (lw(OV(0x0015f5c4)) == 2) return;
    const uint32_t sp = ee_stack_alloc(0xe0);
    class1023_frame(m);
    uint32_t patrol = 0;
    const int32_t patrol_i = lw(pv + 0x1fc);
    const uint32_t target = lwu(pv + 0x160);
    if (patrol_i != -1) patrol = level_path(patrol_i);

    // the hero's spin attack near: it ducks now and then
    if (lwu(0x001413d4) == 0x3d && lw(HERO + 0x198) < frame_scale_i(30)) {
        const uint32_t s = lbu(m + 0x20);
        if (s != 8 && s != 0xa && s != 4 && s != 0xb && vec2_distance(V(m + 0x10), V(HERO_POS)) < 25.0f && rand_mod(0x31) == 0) {
            if (lbu(m + 0x53) != 0xe) moby_anim_set(m, 0xe, 0, frame_scale_i(11));
            sb(m + 0x20, 8);
        }
    }
    if (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) == 2) {
        moby_retire(m);
        if (lwu(pv + 0x1f8) != 0) moby_retire(lwu(pv + 0x1f8));
        ee_stack_free(0xe0);
        return;
    }
    const uint32_t out = sp + 0x10;
    switch (lbu(m + 0x20)) {  // (the table 0x1fc660 / 0x208f30)
        case 0: {  // set up: its numbers, at its home path's start, patrolling (or chasing without a patrol path)
            sb(pv + 0x29, 0);
            sb(pv + 0x2e, 1);
            sb(pv + 0x28, 1);
            swc1(pv + 0x20, 2.0f);
            sh(pv + 0x24, 2);
            sb(pv + 0x58, static_cast<uint32_t>(float_to_int(15.0f)));
            sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(12.0f)));
            if (lw(pv + 0x1d0) == -1 || lw(pv + 0x1d4) == -1) {  // (a path not set: a warning, the moby gone)
                debug_printf_stub();
                moby_retire(m);
                break;
            }
            walker_record_init_l1(pv + 0xd0);
            swc1(pv + 0xd8, 0.5f);
            swc1(pv + 0xdc, 0.5f);
            sw(pv + 0xd0, static_cast<uint32_t>(float_to_int(512.0f)));
            const float turn = fpu_mul(frame_time2(), u2f(0x40df66f3));
            swc1(pv + 0x110, turn);
            swc1(pv + 0x114, fpu_mul(frame_time(), u2f(0x410ba058)));
            swc1(pv + 0xf8, u2f(0x3eb2b8c2));
            swc1(pv + 0xfc, 0.2f);
            swc1(pv + 0x10c, turn);
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
            lq_sq(m + 0x10, level_path(lw(pv + 0x1d4)) + 0x10);
            uint32_t next = 3;
            if (patrol != 0) {
                sh(pv + 0x20c, static_cast<uint32_t>((libc_rand() & 1) * 2 - 1));
                const int32_t at = path_nearest_l1(m + 0x10, patrol, 0.0f);
                patrol_bounce(pv, patrol, at, false);
                next = 1;
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, rand4() + 7);
            }
            sb(m + 0x20, next);
            sh(pv + 0x1da, 0);
            break;
        }
        case 1: {  // patrolling its path to and fro; back home when the target is in sight long enough
            const int32_t at = lw(pv + 0x1f4);
            if (vec2_distance(V(m + 0x10), V(path_point(patrol, at))) < lwc1(pv + 0xe0)) patrol_bounce(pv, patrol, at, true);
            doomlet_259028(m, pv + 0xd0, path_point(patrol, lw(pv + 0x1f4)), out);
            if (target == 0 || lw(pv + 0x164) == 2 || tick32(pv + 0x1f0) == 0) break;
            sw(pv + 0x1f4, static_cast<uint32_t>(path_nearest_l1(m + 0x10, level_path(lw(pv + 0x1d4)), 0.0f)));
            sb(m + 0x20, 2);
            break;
        }
        case 2: {  // back to the home path's nearer point
            const int32_t at = path_nearest_l1(m + 0x10, level_path(lw(pv + 0x1d4)), 0.0f);
            sw(pv + 0x1f4, static_cast<uint32_t>(at));
            const float d = vec2_distance(V(m + 0x10), V(path_point(level_path(lw(pv + 0x1d4)), at)));
            if (d < 1.0f || d < lwc1(pv + 0xe0)) {
                swc1(pv + 0xe0, 0.0f);
                sb(m + 0x20, 3);
                break;
            }
            doomlet_259028(m, pv + 0xd0, path_point(level_path(lw(pv + 0x1d4)), lw(pv + 0x1f4)), out);
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, rand4() + 7);
            break;
        }
        case 3: {  // facing the target: a pause after enough leaps, a leap when the patrol timer runs out, a strike up close
            turn_to(m, pv, target);
            if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != static_cast<uint32_t>(libc_rand() & 1)) {
                const uint32_t set = static_cast<uint32_t>(libc_rand() & 1);
                const int32_t base = frame_scale_i(7);
                moby_anim_set(m, set, 0, base + libc_rand() % 7);
            }
            if (lw(tuning(0x4c)) < lh(pv + 0x1da)) {
                pause(m, pv, true);
                break;
            }
            if (target == 0) break;
            if (lw(pv + 0x164) != 2 && tick32(pv + 0x1f0) != 0) {
                leap_start(m, pv);
                leap_turn(m, pv, sp);
                break;
            }
            if (!(vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tuning(0x48)))) break;
            sb(m + 0x20, 7);
            if (lbu(m + 0x53) != 0xd) moby_anim_set(m, 0xd, 0, frame_scale_i(10));
            break;
        }
        case 4: {  // the leap: along the arc from its start to the point; then fire when it sees the target, else wait
            turn_to(m, pv, target);
            const float t = moby_anim_time(m);
            if (12.0f < t && t < 23.0f && lbu(m + 0x52) == lbu(m + 0x53)) {
                float k = fpu_sub(1.0f, fpu_div(fpu_sub(23.0f, t), 11.0f));
                if (1.0f < k) k = 1.0f;
                else if (k < 0.0f) k = 0.0f;
                vec3_lerp(V(out), V(pv + 0x1c0), V(path_point(level_path(lw(pv + 0x1d4)), lw(pv + 0x1f4))), k);
                const float h = fpu_add(fpu_sub(k, 0.5f), fpu_sub(k, 0.5f));
                swc1(out + 8, fpu_add(lwc1(out + 8), fpu_mul(lwc1(tuning(0x50)), fpu_sub(1.0f, fpu_mul(h, h)))));
                lq_sq(m + 0x10, out);
                break;
            }
            if ((lbu(m + 0x70) & 2) == 0) break;
            const uint32_t to = sp + 0x20;
            lq_sq(out, m + 0x10);
            lq_sq(to, target + 0x10);
            swc1(to + 8, fpu_add(lwc1(to + 8), 0.75f));
            swc1(out + 8, fpu_add(lwc1(out + 8), 1.0f));
            segment_collide_v(V(out), V(to), 0, m, nullptr);
            if (lw(tuning(0x4c)) < lh(pv + 0x1da)) {
                pause(m, pv, false);
                break;
            }
            if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tuning(0x48))) {
                strike(m);
                break;
            }
            if (lwu(OV(0x00173e58)) == target) {  // (in sight: fire)
                swc1(pv + 0x1e8, 1.0f);
                sb(m + 0x20, 5);
                sh(pv + 0x20e, 0);
                if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, frame_scale_i(10));
                break;
            }
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(5));
            sw(pv + 0x1f0, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(45.0f, 90.0f)))));
            break;
        }
        case 5: {  // firing: shots at the interval, spreading wider, one in five (by its index) skipped when it is easy; a leap
                   // once the spread is wide
            if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tuning(0x48))) {
                sw(pv + 0x1ec, 0);
                strike(m);
                break;
            }
            if (tick32(pv + 0x1ec) == 0) break;
            const float easy = lwc1(OV(0x0015f5d4));
            const int32_t extra = float_to_int(fpu_add(fpu_mul(easy, u2f(0x3fa66666)), lwc1(OV(0x0015f5d0))));
            sw(pv + 0x1ec, static_cast<uint32_t>(frame_scale_i(lw(tuning(0x3c)) + extra)));
            swc1(pv + 0x1e8, fpu_add(lwc1(pv + 0x1e8), lwc1(tuning(0x40))));
            const bool hard = lwc1(OV(0x0015f5d4)) < 0.8f && lwc1(OV(0x0015f5d0)) < 0.8f;
            if (hard || (lhu(pv + 0x20e) & 1) == 0) {
                const uint32_t from = sp + 0x10, vel = sp + 0x20;
                moby_chain_point_v(lwu(pv + 0x1f8), 0, V(from));
                const float yaw = lwc1(m + 0x48);
                swc1(vel, fpu_mul(cos_vu(yaw), fpu_mul(lwc1(tuning(0x44)), frame_time())));
                const float speed = fpu_mul(lwc1(tuning(0x44)), frame_time());
                swc1(vel + 4, fpu_mul(sin_vu(yaw), speed));
                sw(vel + 8, 0);
                swc1(vel + 8, fpu_mul(fpu_div(-1.0f, lwc1(pv + 0x1e8)), speed));
                const uint32_t shot = class1023_shot_spawn(vel, from, m, target, frame_scale_i(0xb4), static_cast<uint32_t>(lh(pv + 0x20e)));
                if (shot != 0) swc1(shot + 0x2c, fpu_mul(lwc1(shot + 0x2c), u2f(0x3fd9999a)));
            }
            const uint32_t n = lhu(pv + 0x20e) + 1;
            sh(pv + 0x20e, n);
            if (!(static_cast<int16_t>(n) < 5)) sh(pv + 0x20e, 0);
            if (!(20.0f < lwc1(pv + 0x1e8))) break;
            lq_sq(pv + 0x1c0, m + 0x10);
            sw(pv + 0x1f4, (lwu(pv + 0x1f4) + 1) & 1);
            sh(pv + 0x1da, lhu(pv + 0x1da) + 1);
            sb(m + 0x20, 4);
            leap_turn(m, pv, sp);
            break;
        }
        case 6: {  // stepping back toward its home path's start, then facing again
            turn_to(m, pv, target);
            const float t = moby_anim_time(m);
            if (12.0f < t && t < 23.0f && lwc1(m + 0x54) == 0.0f) {
                vec3_sub(V(out), V(level_path(lw(pv + 0x1d4)) + 0x10), V(m + 0x10));
                vec3_clamp_length(out, fpu_mul(frame_time(), 24.0f));
                walker_move(m, out, 0, lwc1(tuning(0x34)), lwc1(tuning(0x38)), 0.0f);
                break;
            }
            if ((lbu(m + 0x70) & 2) == 0) break;
            sw(pv + 0x1f0, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(45.0f, 90.0f)))));
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
            break;
        }
        case 7: {  // the strike: the mobys in a sphere at its joint 4 hit in the animation's window
            turn_to(m, pv, target);
            const float t = moby_anim_time(m);
            if (16.5f < t && t < 20.0f) {
                const uint32_t at = sp + 0x10, dir = sp + 0x20, rec = sp + 0x30;
                moby_chain_point_v(m, 4, V(at));
                vec3_sub(V(dir), V(at), V(m + 0x10));
                sw(dir + 8, 0);
                vec3_set_length(V(dir), V(dir), 1.0f);
                swc1(dir + 8, 1.0f);
                swc1(at + 8, fpu_sub(lwc1(at + 8), 0.25f));
                sub_00259888(1.0f, rec, static_cast<int32_t>(m), 1, dir);
                mobys_in_sphere(at, 0.8f, 0, m, rec);
            }
            if ((lbu(m + 0x70) & 2) != 0) sb(m + 0x20, 2);
            break;
        }
        case 8: case 9:  // ducking, pausing: then facing again
            if ((lbu(m + 0x70) & 2) == 0) break;
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
            break;
        case 10: {  // knocked back: on landing a strike or a volley
            const uint32_t r = enemy_physics(m, pv + 0x70);
            if ((r & 0x40) == 0) {
                if ((r & 0x120) != 0) moby_retire(m);
                break;
            }
            if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tuning(0x48))) {
                strike(m);
                break;
            }
            swc1(pv + 0x1e8, 1.0f);
            sb(m + 0x20, 5);
            sh(pv + 0x20e, 0);
            if (lbu(m + 0x53) != 6) {
                const int32_t base = frame_scale_i(10);
                moby_anim_set(m, 6, 0, base + rand_range(0, frame_scale_i(7)));
            }
            break;
        }
        case 11: {  // dying: on landing its bolts, a blast, three pieces, gone with its glow moby
            if ((enemy_physics(m, pv + 0x70) & 0x160) == 0) break;
            vec_clear(V(sp + 0x10));
            const uint32_t zero = OV(0x0015f580);
            moby_drop_bolts(m, 0, -1);
            explosion_burst(m, m + 0x10, 0, 0.5f, 13.0f);
            for (uint32_t cls = 0x69c; cls <= 0x69e; ++cls) {
                sw(sp, zero);
                debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            }
            moby_retire(m);
            if (lwu(pv + 0x1f8) != 0) moby_retire(lwu(pv + 0x1f8));
            break;
        }
        default:
            break;
    }
    ee_stack_free(0xe0);
}

}  // namespace rac1
