// Veldin's finale (level 18): the boss's (class 1422: its update in veldin_finale_boss.cpp) shots and its
// launches of the level's other mobys, hand-written (part 1b of its port): its shell (class 564), its ring (class 624),
// its burst (class 628) and its energy ball (class 983) spawned and moved, a walking bomb (class 568), a robot (class
// 1906) and its divers (class 1355) thrown out of their groups. The boss's update (veldin_finale_boss.cpp) calls them; the shots' own updates are veldin_finale_shots.cpp and
// veldin_finale_energy_ball.cpp. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the
// base ELF's bare (the frame time's square 0x15ed70). Declared in veldin_finale_own.h.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr int16_t BOMB_CLASS = 0x238;   // class 568
constexpr int16_t DIVER_CLASS = 0x54b;  // class 1355
constexpr int16_t ROBOT_CLASS = 0x772;  // class 1906

uint32_t first_moby() { return lwu(L18_ADDR(0x0015ffd8)); }

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// a moby taken out of its group shown again with its class's colour (+0x24's +0x10 at +0x94)
void class_colour(uint32_t o) { sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10)); }

// the lob's gravity: ten times the frame time's square, downward
float lob_gravity() { return -fpu_mul(lwc1(0x0015ed70), 10.0f); }
}  // namespace

// ---- the shots

// The boss's shell (class 564) at the EE point `at` with the velocity `vel`, toward `to`, over `frames` frames (half of
// them at +0x30), its flags `flags` (+0x38), f12 / f13 into +0x24 / +0x28; grey (0xc0), scaled by L18:0x161960, turned
// along its velocity (yaw +0x48, pitch +0x44), its sound started. The shell, 0 when none could be spawned.
// orig: L18:0x002d5348
uint32_t vf_shell_spawn_l18(uint32_t at, uint32_t vel, uint32_t to, int32_t frames, uint32_t flags, float f12, float f13) {
    const uint32_t m = moby_spawn(0x234);
    if (m == 0) return 0;
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    moby_set_color_hi(m, 0xc0, 0xc0, 0xc0);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), lwc1(L18_ADDR(0x00161960))));
    lq_sq(m + 0x10, at);
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(pv + 0x00, vel);
    lq_sq(pv + 0x10, to);
    sw(pv + 0x38, flags);
    sw(pv + 0x30, static_cast<uint32_t>(frames >> 1));
    swc1(pv + 0x24, f12);
    swc1(pv + 0x28, f13);
    sw(pv + 0x2c, 0);
    sw(pv + 0x34, static_cast<uint32_t>(frames));
    swc1(m + 0x48, atan2_f(lwc1(vel + 0), lwc1(vel + 4)));
    swc1(m + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    moby_sound_start(0, 0, m);
    return m;
}

// The boss's ring (class 624) at the EE point `at`, its direction `dir` (+0, its yaw +0x48), its owner `owner` (+0x34),
// its widest radius `radius` (+0x28), its speed `speed` (+0x20); scaled by L18:0x161bf8, its colour L18:0x161bfc. (The
// original returns no moby: its v0 is a leftover, which the boss does not read.)
// orig: L18:0x002db938
void vf_ring_spawn_l18(uint32_t at, uint32_t dir, uint32_t owner, float radius, float speed) {
    const uint32_t m = moby_spawn(0x270);
    if (m == 0) return;
    sb(m + 0x30, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), lwc1(L18_ADDR(0x00161bf8))));
    sh(m + 0x32, 0xff);
    sh(m + 0x34, lhu(m + 0x34) | 0x10);
    sw(m + 0x90, lwu(L18_ADDR(0x00161bfc)));
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(m + 0x10, at);
    swc1(m + 0x48, atan2_f(lwc1(dir + 0), lwc1(dir + 4)));
    lq_sq(pv + 0x00, dir);
    sw(pv + 0x34, owner);
    swc1(pv + 0x28, radius);
    swc1(pv + 0x20, speed);
    sw(pv + 0x30, 0);
}

// The boss's burst (class 628) of the owner `owner` (+0xc) at the EE point `at`, its radius `radius` (+0), its life 3
// (+8). The burst, 0 when none could be spawned.
// orig: L18:0x002dc3e0
uint32_t vf_burst_spawn_l18(uint32_t owner, uint32_t at, float radius) {
    const uint32_t m = moby_spawn(0x274);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x30, 0xff);
    lq_sq(m + 0x10, at);
    sw(pv + 0xc, owner);
    swc1(pv + 0x0, radius);
    sw(pv + 0x8, 3);
    return m;
}

// The burst `m` moved to the EE point `at` with the radius `radius`, its life two frames longer, at most
// frame_scale_i(L18:0x161c9c).
// orig: L18:0x002dc458
void vf_burst_move_l18(uint32_t m, uint32_t at, float radius) {
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(m + 0x10, at);
    const int32_t life = lw(pv + 0x8) + 2;
    swc1(pv + 0x0, radius);
    sw(pv + 0x8, static_cast<uint32_t>(life));
    if (frame_scale_i(lw(L18_ADDR(0x00161c9c))) < lw(pv + 0x8)) sw(pv + 0x8, static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161c9c)))));
}

// The boss's energy ball (class 983) of the owner `owner` (+0x20) at the EE point `at`, its life `frames` (+0x34), its
// arc's wait 5 (+0x28). The ball, 0 when none could be spawned.
// orig: L18:0x002ea0f0
uint32_t vf_ball_spawn_l18(uint32_t owner, uint32_t at, uint32_t frames) {
    const uint32_t m = moby_spawn(0x3d7);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    lq_sq(m + 0x10, at);
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x20, owner);
    sw(pv + 0x28, 5);
    sw(pv + 0x34, frames);
    sw(pv + 0x24, 0);
    return m;
}

// The ball `m` held at the EE point `at` (+0x28 frame_scale_i(5), +0x24 `f12`); with `go` set and the ball in its
// state 0, sent off toward the EE point `target` (+0; state 1, +0x28 0).
// orig: L18:0x002ea168
void vf_ball_launch_l18(uint32_t m, uint32_t at, uint32_t target, uint32_t go, float f12) {
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(m + 0x10, at);
    sw(pv + 0x28, static_cast<uint32_t>(frame_scale_i(5)));
    swc1(pv + 0x24, f12);
    if (go == 0 || lbu(m + 0x20) != 0) return;
    sb(m + 0x20, 1);
    sw(pv + 0x28, 0);
    lq_sq(pv + 0x00, target);
}

// ---- the launches out of a group

// The group's last walking bomb (class 568) at rest (state 5) thrown from the EE point `from` toward the EE point `to`
// (whose height is raised by 0.75 first: the caller's vector changes), landing L18:0x161994 short of it, at the speed
// `speed` on a lob (sub_0025abf0); its fuse `fuse` (+0x194), its thrower `owner` (+0x198); shown, its sound started.
// The bomb, 0 when the group has none at rest.
// orig: L18:0x002d5cf8
uint32_t vf_bomb_throw_l18(uint32_t owner, int32_t group, uint32_t from, uint32_t to, uint32_t fuse, float speed) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 0;
    uint32_t bomb = 0;
    for (;; e += 2) {
        const uint32_t o = first_moby() + ((lhu(e) & 0x7fff) << 8);
        if (lh(o + 0xa6) == BOMB_CLASS && lbu(o + 0x20) == 5) bomb = o;
        if (lh(e) < 0) break;
    }
    if (bomb == 0) return 0;
    sb(bomb + 0x30, 0xff);
    sh(bomb + 0x32, 0xff);
    sb(bomb + 0x20, 1);
    sh(bomb + 0x34, (lhu(bomb + 0x34) & 0xffbe) | 0x1000);
    sb(bomb + 0x31, 1);
    sb(bomb + 0xbc, 0);
    class_colour(bomb);
    if (lbu(bomb + 0x53) != 0) moby_anim_set(bomb, 0, 0, 1);
    lq_sq(bomb + 0x10, from);
    Frame f(0x90);
    const uint32_t land = f.sp;  // (its landing point, sp+0)
    sw(bomb + 0x40, 0);
    swc1(bomb + 0x44, -lwc1(L18_ADDR(0x00161994)));
    swc1(to + 8, fpu_add(lwc1(to + 8), 0.75f));
    vec3_sub(V(land), V(to), V(from));
    vec2_set_length(V(land), V(land), fpu_sub(vec2_length(V(land)), lwc1(L18_ADDR(0x00161994))));
    vec3_add(V(land), V(land), V(from));
    const uint32_t pv = lwu(bomb + 0x78);
    lq_sq(pv + 0x170, land);
    vec3_sub(V(pv + 0x180), V(land), V(from));
    sw(pv + 0x188, 0);
    vec3_set_length(V(pv + 0x180), V(pv + 0x180), speed);
    sh(pv + 0xc8, 0);
    swc1(pv + 0x188, sub_0025abf0(from, land, nullptr, speed, lob_gravity()));
    swc1(bomb + 0x48, atan2_f(lwc1(pv + 0x180), lwc1(pv + 0x184)));
    sw(pv + 0x194, fuse);
    sw(pv + 0x198, owner);
    sw(pv + 0x19c, 0);
    moby_sound_start(0, 0, bomb);
    return bomb;
}

// The group's first robot (class 1906) waiting (state 8) sent from the EE point `from` toward the EE point `to` at the
// speed `speed` on a lob (sub_0025abf0: +0x1d0 its velocity, +0x1d8 the lob's), spinning (+0x1ec: 15 .. 45 degrees
// either way), shown (state 1). 1 when one was, else 0.
// orig: L18:0x002fc668
uint32_t vf_robot_launch_l18(int32_t group, uint32_t from, uint32_t to, float speed) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 0;
    for (;; e += 2) {
        const uint32_t o = first_moby() + ((lhu(e) & 0x7fff) << 8);
        if (lh(o + 0xa6) == ROBOT_CLASS && lbu(o + 0x20) == 8) {
            const uint32_t pv = lwu(o + 0x78);
            sh(o + 0x34, lhu(o + 0x34) & 0xffbe);
            sh(o + 0x34, lhu(o + 0x34) | 0x1000);
            class_colour(o);
            lq_sq(o + 0x10, from);
            sb(o + 0x20, 1);
            swc1(pv + 0x1f4, 1.0f);
            vec3_sub(V(pv + 0x1d0), V(to), V(from));
            sw(pv + 0x1d8, 0);
            vec3_set_length(V(pv + 0x1d0), V(pv + 0x1d0), speed);
            swc1(pv + 0x1d8, sub_0025abf0(from, to, nullptr, speed, lob_gravity()));
            swc1(pv + 0x1ec, fpu_mul(rand_range_signed_f(15.0f, 45.0f), u2f(0x3c8efa35u)));  // (degrees to radians)
            return 1;
        }
        if (lh(e) < 0) break;
    }
    return 0;
}

// Every diver (class 1355) of the group waiting (state 1) sent off from the EE point `from` (its position and its
// trail's four points +0x10 .. +0x40), its owner `owner` (+0x16c), shown (state 2). 0 when one was, 1 when none (or the
// group has no list).
// orig: L18:0x002f0030
uint32_t vf_divers_launch_l18(uint32_t owner, int32_t group, uint32_t from) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 1;
    uint32_t none = 1;
    for (;; e += 2) {
        const uint32_t o = first_moby() + ((lhu(e) & 0x7fff) << 8);
        if (lh(o + 0xa6) == DIVER_CLASS && lbu(o + 0x20) == 1) {
            const uint32_t pv = lwu(o + 0x78);
            none = 0;
            lq_sq(o + 0x10, from);
            for (uint32_t k = 0; k < 4; ++k) lq_sq(pv + 0x10 + k * 0x10, o + 0x10);
            sw(pv + 0x16c, owner);
            sb(o + 0x31, 1);
            sh(o + 0x34, lhu(o + 0x34) & 0xfffe);
            sb(o + 0x20, 2);
            class_colour(o);
        }
        if (lh(e) < 0) break;
    }
    return none;
}

}  // namespace rac1
