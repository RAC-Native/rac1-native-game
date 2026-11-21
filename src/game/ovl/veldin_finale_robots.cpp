// Veldin's finale (level 18) class 1906: the boss's small robots (18 placed, in the boss's groups: it throws them into the
// arena, vf_robot_launch_l18, after vf_robots_out_l18 has taken them out of their state 8). Set up (state 0) it waits
// hidden (state 8) with one hit point, facing either way. Thrown (state 1) it falls along its velocity +0x1d0 onto the
// floor (idle, state 2), or below height 10 it blows up (state 6). Idle it walks (state 3) toward its target (the record
// +0x180 target_select fills from the path +0x1e0 within 512; with none, the hero's point) once it has one or the point
// is more than 10 away; walking it stops within 5 of the hero's point, or attacks (state 4) within 1.5 of its target;
// attacking it turns toward it and strikes at the animation's frame 14 (its chain's contact), and walks again when the
// target is more than 2.5 away. Hit to 0 it either blows up (state 6) or is knocked away (state 5) and blows up when it
// lands. Carried (state 7, its carry hook +0xc: vf_robot_hookc_l18) it is thrown back to idle. While the hero is in the
// level's state 0x72 or a scene plays it is held hidden (state 8). Every frame its glow pulses (+0x90), its size is the
// class's x L18:0x1625fc, once a frame one robot queues the level's draw L18:0x2fca98, and while its pop +0x1f4 shrinks
// back to 0 its matrix is stretched and its draw L18:0x2fc860 queued. Level 18 only: the addresses are the finale's
// (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in veldin_finale_mobys.cpp; the helpers'
// declarations in veldin_finale_own.h.
//
// The pvars: +0x20 health, +0x24 / +0x28 the hit's kind / box, +0x30, +0x40 the blast's velocity, +0x58 / +0x5c, +0x60
// the colour pulse, +0x70 the movement record (move_state_start / enemy_physics / carry_update; +0x80 .. +0xc4 its
// knock-back), +0x138, +0x140, +0x180 the target's record (target_select's 0x48 bytes: +0x180 its point, +0x1c0 its moby,
// +0x1c4 the result: 2 = none), +0x1d0 the throw's velocity, +0x1e0 its path, +0x1e8 its turn's speed, +0x1ec its
// heading's offset, +0x1f0 its walking speed, +0x1f4 its pop, +0x1f8 its glow's phase.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position, +0x208c his mode, +0x560 his point record, +0x2080 his moby)
constexpr float TWO_PI = 6.2831854820251465f;

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the robot shown at rest: animation set 0 (unless it is in it), no colour, drawn and solid again
void to_rest(uint32_t m) {
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
}

// the robot to a state with an animation set (when not in it already)
void to_state(uint32_t m, uint32_t st, uint32_t set, int32_t frames) {
    sb(m + 0x20, st);
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the robot held out of sight (state 8: hidden, out of the collision, no colour)
void hide(uint32_t m) {
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
}
}  // namespace

// The robot's every frame before its state (none in states 0 and 8): once a frame (the level's frame counter
// L18:0x15f5cc kept in L18:0x162604) one robot not carried queues the draw L18:0x2fca98; its glow's phase turned (faster
// in states 4 / 5) and its colour (+0x90) pulsed between L18:0x16260c and 0x162610; below height 100 it blows up (state
// 6, unless carried); its size the class's x L18:0x1625fc. With the hero in his state 0x72 or a scene playing it is held
// hidden (state 8, unless carried). Else the hits (mask 0x330000): at 0 health it blows up (state 6) or, one time in two,
// is knocked away from the hit (state 5); then its target (the record +0x180 from its path within 512; with none, the
// hero's point: his record's current point in his mode 0xf, else his position) and its moby (+0x1c0, the hero's moby when
// none).
// orig: L18:0x002fc100
void vf_robot_frame_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint8_t st = lbu(m + 0x20);
    if (st == 0 || st == 8) return;
    if (st != 7 && lw(L18_ADDR(0x0015f5cc)) != lw(L18_ADDR(0x00162604))) {
        sw(L18_ADDR(0x00162604), lwu(L18_ADDR(0x0015f5cc)));
        sub_001f93e0(L18_ADDR(0x002fca98), m);
    }
    const float rate = static_cast<uint32_t>(lbu(m + 0x20) - 4) < 2 ? lwc1(L18_ADDR(0x00162618)) : lwc1(L18_ADDR(0x00162614));
    const float phase = angle_add(lwc1(pv + 0x1f8), fpu_mul(fpu_mul(rate, 0.01745329238474369f), dt()));
    swc1(pv + 0x1f8, phase);
    const uint32_t c = rgba_lerp(lwu(L18_ADDR(0x0016260c)), lwu(L18_ADDR(0x00162610)), fpu_add(fpu_mul(sin_vu(phase), 0.5f), 0.5f));
    sw(m + 0x90, c);
    if (lwc1(m + 0x18) < 100.0f && lbu(m + 0x20) != 7) sb(m + 0x20, 6);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L18_ADDR(0x001625fc))));
    if ((lw(0x001413d4) == 0x72 || lw(L18_ADDR(0x0015f5c4)) == 2) && lbu(m + 0x20) != 7) {  // (0x002fc238) held hidden
        sb(m + 0x20, 8);
        to_rest(m);
        hide(m);
        return;
    }
    {  // (0x002fc288) the hits (a carried robot too)
        const Frame f(160);
        const uint32_t sp = f.sp;
        const uint32_t st_w = sp + 0x10, dmg = sp + 0x14, yaw = sp + 0x18;
        swc1(dmg, 0.0f);
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
        {
            int32_t react_st = lw(st_w);  // (stale: the frame's word, as the original reads it)
            float react_dmg = lwc1(dmg);
            moby_hit_react_r(m, hit, pv + 0x20, 0, react_st, &react_dmg, nullptr, 4);
            sw(st_w, static_cast<uint32_t>(react_st));
            swc1(dmg, react_dmg);
        }
        if (lw(st_w) != 1 && lbu(m + 0x20) != 6 && lwc1(dmg) != 0.0f) {
            const float health = fpu_sub(lwc1(pv + 0x20), lwc1(dmg));
            swc1(pv + 0x20, health);
            if (health <= 0.0f) {
                if (rand_mod(2) != 0) {
                    sb(m + 0x20, 6);
                } else {  // (0x002fc328) knocked away from the hit
                    sb(pv + 0xad, 0);
                    swc1(pv + 0x98, 0.5f);
                    swc1(pv + 0x8c, fpu_mul(lwc1(L18_ADDR(0x00162628)), dt()));
                    swc1(pv + 0x80, fpu_mul(lwc1(L18_ADDR(0x0016261c)), dt_sq()));
                    swc1(pv + 0x84, fpu_mul(lwc1(L18_ADDR(0x00162620)), dt_sq()));
                    swc1(pv + 0xbc, fpu_add(dt(), dt()));
                    swc1(pv + 0x88, fpu_mul(lwc1(L18_ADDR(0x00162624)), dt()));
                    sw(pv + 0x94, 0x29);
                    sw(pv + 0x90, 0x200);
                    sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                    lq_sq(sp, hit + 0x10);
                    vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                                     *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
                    move_state_start(m, pv + 0x70, 3, 1, 0, lwc1(yaw));
                    swc1(pv + 0xc0, 7.5f);
                    swc1(pv + 0xc4, 15.0f);
                    sb(m + 0x20, 5);
                    sw(m + 0x94, 0);
                    sb(pv + 0x67, 0x78);
                    color_pulse_start(m, pv + 0x60);
                }
            }
        }
    }
    // (0x002fc410)
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    const uint32_t target = pv + 0x180;
    const uint32_t path = lvd::path(lw(pv + 0x1e0));
    target_select(m, target, 0, 0, path + 0x10, lw(path), 512.0f);
    if (lw(pv + 0x1c4) == 2) {
        if (lw(HERO + 0x208c) == 0xf) {
            const uint32_t rec = lwu(HERO + 0x560);
            lq_sq(target, rec + (lwu(rec) << 4));
        } else {
            lq_sq(target, HERO + 0x80);
        }
    }
    if (lwu(pv + 0x1c0) == 0) sw(pv + 0x1c0, lwu(HERO + 0x2080));
}

// The robot's step toward the EE point `goal`: turned toward it (its heading's offset +0x1ec added), its speed +0x1f0
// sought to the distance, moved along its facing (and down by twice the frame time), kept inside its path's area. The
// distance to the goal.
// orig: L18:0x002fc4e0
float vf_robot_walk_l18(uint32_t m, uint32_t goal) {
    const Frame f(144);
    const uint32_t point = f.sp, step = f.sp + 0x10, pos = f.sp + 0x20;
    lq_sq(point, goal);
    const uint32_t pv = lwu(m + 0x78);
    const float yaw = angle_add(atan2_f(fpu_sub(lwc1(point + 0), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14))),
                                lwc1(pv + 0x1ec));
    yaw_seek(m + 0x48, pv + 0x1e8, yaw, fpu_mul(dt_sq(), TWO_PI), fpu_mul(dt_sq(), TWO_PI), fpu_mul(dt(), TWO_PI));
    swc1(pos, 0.0f);
    const float d = vec2_distance(V(m + 0x10), V(point));
    seek_1d(pos, pv + 0x1f0, d, fpu_mul(dt_sq(), 6.0f), fpu_mul(dt_sq(), 6.0f), fpu_mul(dt(), 5.0f));
    swc1(step + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x1f0)));
    swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x1f0)));
    swc1(step + 8, -fpu_add(dt(), dt()));
    walker_move(m, step, 0, 0.5f, 0.5f, 0.0f);
    path_push_out_2d(lwu(pv + 0x1e0), m + 0x10, m + 0x10, 0.5f);
    return d;
}

// Class 1906's carry hook +0xc (the table at the class's +0x2c): the shared carry hook, then the robot carried (state
// 7). (Veldin's class 749 hook 0x2d57d0 is the same code with the state 9.)
// orig: L18:0x002fcca0
int32_t vf_robot_hookc_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    sb(m + 0x20, 7);
    return r;
}

// orig: L18:0x002fbb98
void vf_class1906_update_l18(uint32_t m) {
    const Frame f(128);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    vf_robot_frame_l18(m);
    switch (lbu(m + 0x20)) {  // (the table L18:0x1f2c40; 8 and past: the tail only)
    case 0: {  // set up, hidden (0x002fbbe4)
        sb(m + 0x20, 8);
        const uint16_t flags = lhu(m + 0x34);
        sh(m + 0x34, rand_mod(2) != 0 ? (flags | 0x8000) : (flags & 0x7fff));
        sh(m + 0x34, lhu(m + 0x34) | 0x100);
        sb(m + 0x73, lbu(L18_ADDR(0x00162608)));
        sw(pv + 0x140, L18_ADDR(0x001625f0));
        sb(pv + 0x58, 8);
        swc1(pv + 0x20, 1.0f);
        sb(pv + 0x5c, 1);
        swc1(pv + 0x30, 0.5f);
        sh(pv + 0x24, 1);
        swc1(pv + 0x1f8, rand_angle());
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sb(m + 0x31, 0);
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 1) & 0xefff);
        break;
    }
    case 1: {  // thrown, falling (0x002fbc84)
        swc1(pv + 0x1d8, fpu_sub(lwc1(pv + 0x1d8), fpu_mul(dt_sq(), 10.0f)));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x1d0));
        const uint32_t probe = sp + 0x20;
        lq_sq(probe, m + 0x10);
        swc1(probe + 8, fpu_add(lwc1(probe + 8), 1.0f));
        const float floor = floor_probe_v(0.5f, V(probe), 0);
        const float z = lwc1(m + 0x18);
        if (z < floor) {  // landed: idle
            swc1(m + 0x18, floor);
            sb(m + 0x20, 2);
            to_rest(m);
        } else if (z < 10.0f) {
            sb(m + 0x20, 6);
        }
        break;
    }
    case 2:  // idle (0x002fbd5c): off once it has a target, or the hero's point is more than 10 away
        if (lw(pv + 0x1c4) != 2 || 10.0f < vec2_distance(V(m + 0x10), V(pv + 0x180))) to_state(m, 3, 1, 0xa);
        break;
    case 3: {  // walking (0x002fbd88)
        lq_sq(sp + 0x20, pv + 0x180);
        const float d = vf_robot_walk_l18(m, sp + 0x20);
        if (lw(pv + 0x1c4) == 2 && d < 5.0f) {  // at the hero's point: idle
            sb(m + 0x20, 2);
            to_rest(m);
        } else if (d < 1.5f) {  // at its target: the strike
            sb(m + 0x20, 4);
            if (lbu(m + 0x53) != 2)
                moby_anim_set(m, 2, static_cast<int32_t>(f2u(eer::cvt_w(lwc1(L18_ADDR(0x00162600))))), frame_scale_i(5));
        }
        break;
    }
    case 4: {  // striking (0x002fbe48)
        const float yaw = atan2_f(fpu_sub(lwc1(pv + 0x180), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x184), lwc1(m + 0x14)));
        yaw_seek(m + 0x48, pv + 0x1e8, yaw, fpu_mul(dt_sq(), TWO_PI), fpu_mul(dt_sq(), TWO_PI), fpu_mul(dt(), TWO_PI));
        if (anim_crossed(m, 14.0f) != 0) moby_chain_contact(m, 1, 1, 0, 1, 0, 0.5f, 1.0f);
        if ((lbu(m + 0x70) & 2) == 0) break;  // (the strike not over)
        if (lw(pv + 0x1c4) == 2) {
            sb(m + 0x20, 2);
            to_rest(m);
        } else if (2.5f < vec2_distance(V(m + 0x10), V(pv + 0x180))) {
            to_state(m, 3, 1, 0xa);
        }
        break;
    }
    case 5:  // knocked away (0x002fbf7c): blows up when it lands
        if ((enemy_physics(m, pv + 0x70) & 0x140) == 0) break;
        [[fallthrough]];
    case 6: {  // blowing up (0x002fbf94), then hidden for good (state 8)
        const uint32_t at = sp + 0x20;
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 0.5f));
        explosion_effect(m, pv + 0x40, at, 5, 2, 4, 4, 0, 1, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 9.0f, 0.5f, 0.0f);
        sb(m + 0x20, 8);
        to_rest(m);
        hide(m);
        break;
    }
    case 7:  // carried (0x002fbd3c): thrown back to idle
        if (carry_update(m, pv + 0x70) != 0) {
            sb(m + 0x20, 2);
            sh(pv + 0x138, 0);
        }
        break;
    default:
        break;
    }
    // (0x002fc04c) its matrix from its angles; popping: stretched up and in while +0x1f4 shrinks back to 0, its draw queued
    euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
    if (lwc1(pv + 0x1f4) == 0.0f) return;
    approach_f(pv + 0x1f4, 0.0f, fpu_mul(dt(), 4.0f));
    vec3_scale(V(m + 0xe0), V(m + 0xe0), fpu_add(lwc1(pv + 0x1f4), 1.0f));
    const float in = fpu_sub(1.0f, lwc1(pv + 0x1f4));
    vec3_scale(V(m + 0xc0), V(m + 0xc0), in);
    vec3_scale(V(m + 0xd0), V(m + 0xd0), in);
    sub_001f93e0(L18_ADDR(0x002fc860), m);
}

}  // namespace rac1
