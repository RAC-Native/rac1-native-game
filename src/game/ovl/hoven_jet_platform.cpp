// Hoven's (level 12) class 240: the platforms on two jets that carry the hero over the gaps: along a path, swinging between
// two points, riding another moby, waiting for a moby or a group, falling. Level 12 only: the addresses are Hoven's
// (`orig: L12:`), the level's own data L12_ADDR, the base ELF's bare. Registered in hoven_mobys.cpp.
//
// The moby: +0x20 the state (below), +0x40 / +0x44 / +0x48 its angles; the pvars: +0x3e flags, +0x40 its move this frame
// (+0x48 its height, what states 6 and 8 sink and drop), +0x60 the attach frame (attach_frame_set: what rides it follows), +0xa0 the path follower (+0 the point,
// +4 its direction, +0x10 the path), +0xb4 the path's index, +0xd0 / +0xd4 its top speed / acceleration, +0xd8 the swing's
// angle, +0xdc a group it waits for, +0xe0 the swing's pause, +0xe4 the set-up's kind, +0xe8 / +0xec the fall's spins,
// +0xf0 the moby it rides, +0xf8 the yaw's spring, +0xfc a moby it waits for (-1 none).
//
// States (the table L12:0x20c460): 0 set up (by +0xe4, the table L12:0x20c490), 1 at rest, 2 swing with the hero's weight,
// 3 a swing's pause, 4 swing, 5 the pause of 4, 6 drift, 7 along its path, 8 fall, 9 ride the moby +0xf0.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
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

// the moby `i` of the level's moby table
uint32_t moby_at(int32_t i) { return lwu(L12_ADDR(0x0015ffd8)) + static_cast<uint32_t>(i << 8); }

// the swing's place between its path's first two points: the swing angle's sine folded into 0 .. 1
float swing_weight(uint32_t pv) {
    const float kHalfPi = u2f(0x3fc90fdbu), kPi = u2f(0x40490fdbu);
    float s = sin_vu(lwc1(pv + 0xd8));
    const float a = lwc1(pv + 0xd8);
    if (kHalfPi < a && a < kPi) s = fpu_sub(2.0f, s);
    if (-kPi < a && a < -kHalfPi) s = fpu_sub(2.0f, s);
    s = fpu_add(fpu_mul(s, 0.5f), 0.5f);
    if (1.0f < s) s = fpu_sub(s, 2.0f);
    return abs_f(s);
}
}  // namespace

// The jets' smoke and flames: two particles of type 0x15 and two of 0x17 at the points L12:0x161920 and its mirror (y)
// under the platform, blown down; only while it is drawn.
// orig: L12:0x002e3948
void jet_platform_jets(uint32_t m) {
    if (lbu(m + 0x31) == 0) return;
    const Frame f(224);
    const uint32_t sp = f.sp, smoke = sp, flame = sp + 0x10, left = sp + 0x20, right = sp + 0x30;
    const uint32_t mat = m + 0xc0, pos = m + 0x10;
    vec_clear(V(sp + 0x40));
    swc1(smoke + 0, -rand_range_f(0.0f, fpu_mul(dt(), 5.0f)));
    {
        const float t = fpu_add(dt(), dt());
        swc1(smoke + 4, rand_range_f(-t, t));
    }
    {
        const float t = fpu_add(dt(), dt());
        swc1(smoke + 8, rand_range_f(-t, t));
    }
    mat3_transform(V(smoke), V(smoke), M(mat));
    mat3_transform(V(left), V(L12_ADDR(0x00161920)), M(mat));
    vec3_add(V(left), V(left), V(pos));
    swc1(flame + 0, -rand_range_f(0.0f, dt()));
    swc1(flame + 4, rand_range_f(-dt(), dt()));
    swc1(flame + 8, rand_range_f(-dt(), dt()));
    mat3_transform(V(flame), V(flame), M(mat));
    lq_sq(right, L12_ADDR(0x00161920));
    swc1(right + 4, -lwc1(right + 4));
    mat3_transform(V(right), V(right), M(mat));
    vec3_add(V(right), V(right), V(pos));
    for (const uint32_t at : {left, right}) {
        const int32_t n = rand_range(frame_scale_i(8), frame_scale_i(0x11));
        spawn_particle_15_v(V(at), V(smoke), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(n), 1, 10000.0f);
        spawn_particle_17(at, 6, flame, 0x404040u, 0.05f, u2f(0x3f8147aeu), u2f(0x3f83d70au), 30000.0f);
    }
}

// A frame of its move toward `target` (an EE point): the speed eased up to its top (+0xd0) and, with `ease`, down to stop
// there (else held at the top speed), the tilt +0x40 / +0x44 toward `ang`'s +0 / +4, the yaw sprung toward `ang`'s +8.
// (The original's caller also passes whether the hero is on it, which this function never reads.)
// orig: L12:0x002e3c08
void jet_platform_move(uint32_t m, uint32_t ang, uint32_t target, uint32_t ease) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), move = pv + 0x40;
    vec3_sub(V(sp), V(target), V(m + 0x10));
    const float dist = vec3_length(V(sp));
    float speed = vec3_length(V(move));
    if (ease != 0) {
        if (speed == 0.0f) speed = fpu_mul(dt(), u2f(0x3c23d70au));
        const float acc = fpu_mul(lwc1(pv + 0xd4), dt_sq());
        const float stop = fpu_div(fpu_mul(speed, speed), fpu_add(dist, dist));
        if (acc < stop) speed = fpu_sub(speed, acc);
        else if (speed < fpu_mul(lwc1(pv + 0xd0), dt())) speed = fpu_add(speed, acc);
    } else {
        const float top = fpu_mul(lwc1(pv + 0xd0), dt());
        if (speed < top) speed = fpu_add(speed, fpu_mul(lwc1(pv + 0xd4), dt_sq()));
        else if (top < speed) speed = fpu_sub(speed, fpu_mul(lwc1(pv + 0xd4), dt_sq()));
    }
    vec3_set_length(V(move), V(sp), speed);
    approach_f(m + 0x40, lwc1(ang + 0), fpu_div(angle_dist(lwc1(ang + 0), lwc1(m + 0x40)), lwc1(L12_ADDR(0x00161918))));
    approach_f(m + 0x44, lwc1(ang + 4), fpu_div(angle_dist(lwc1(ang + 4), lwc1(m + 0x44)), lwc1(L12_ADDR(0x00161918))));
    const float k = lwc1(0x0015ed64);
    spring_angle(m + 0x48, pv + 0xf8, 0, lwc1(ang + 8), fpu_mul(k, u2f(0x3ba3d70au)), fpu_mul(k, u2f(0x3e99999au)),
                 fpu_mul(dt(), u2f(0x3ff5be0bu)));
    vec3_add(V(m + 0x10), V(m + 0x10), V(move));
}

// The tilt the hero's weight gives it (`out` +0 / +4: his place in its frame over L12:0x161914), none when he is not on it.
// orig: L12:0x002e3e28
void jet_platform_tilt(uint32_t m, uint32_t out, uint32_t on) {
    if (on == 0) {
        sw(out + 4, 0);
        sw(out + 0, 0);
        return;
    }
    const Frame f(160);
    const uint32_t sp = f.sp, rel = sp + 0x10, turn = sp + 0x20;
    vec3_sub(V(rel), V(0x0013f3d0), V(m + 0x10));
    vec_clear(V(sp));
    swc1(sp + 8, -lwc1(m + 0x48));
    euler_to_mat4_vu(M(turn), V(sp));
    mat4_transform(V(rel), V(rel), M(turn));
    const float k = lwc1(L12_ADDR(0x00161914));
    const float y = -lwc1(rel + 4);
    const float x = fpu_div(lwc1(rel + 0), k);
    swc1(out + 4, x);
    swc1(out + 0, fpu_div(y, k));
}

// The update: while the moby +0xfc lives or the group +0xdc has mobys, only the jets; else the state's part, then the attach
// frame from this frame's move and turn (what rides it moves with it).
// orig: L12:0x002e3ed8
void hov_class240_update_l12(uint32_t m) {
    const Frame f(256);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    const uint32_t was = sp + 0x20, was_ang = sp + 0x30, ang = sp + 0x50, goal = sp + 0x60;
    lq_sq(was, pos);
    lq_sq(was_ang, m + 0x40);
    if (lbu(m + 0x20) != 0) {
        const int32_t wait = lw(pv + 0xfc);
        if (wait != -1 && static_cast<int8_t>(lbu(moby_at(wait) + 0x20)) >= 0) {
            jet_platform_jets(m);
            return;
        }
        if (lbu(m + 0x20) != 0 && lw(pv + 0xdc) != -1 && moby_group_count_l1(lw(pv + 0xdc), -1) != 0) {
            jet_platform_jets(m);
            return;
        }
    }
    // the hero's tilt into `ang`, then the move toward `target`
    auto tilt_and_move = [m, ang](uint32_t target, uint32_t ease) {
        const uint32_t on = moby_hero_on_l1(m) != 0 ? 1u : 0u;
        jet_platform_tilt(m, ang, on);
        jet_platform_move(m, ang, target, ease);
    };
    // the platform's path: its index +0xb4, followed from its point 0 forward (+0xa4 = 1)
    auto take_path = [m, pv]() {
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sb(pv + 0xa4, 1);
        sw(pv + 0xb0, lvd::path(lw(pv + 0xb4)));
        sw(pv + 0xa0, 0);
    };
    auto set_if_zero = [](uint32_t a, float v) {
        if (lwc1(a) == 0.0f) swc1(a, v);
    };
    const uint32_t st = lbu(m + 0x20);
    switch (st) {
    case 0:  // set up by its kind (the table L12:0x20c490)
        switch (lw(pv + 0xe4)) {
        case 0:  // along its path (7) (0x2e3fe4)
            take_path();
            set_if_zero(pv + 0xd0, 6.0f);
            set_if_zero(pv + 0xd4, 6.0f);
            sh(pv + 0x3e, lhu(pv + 0x3e) | 9);
            vec_clear(V(pv + 0x40));
            sb(m + 0x20, 7);
            break;
        case 1:  // swing with the hero's weight (2) (0x2e4080)
            take_path();
            set_if_zero(pv + 0xd4, 2.0f);
            set_if_zero(pv + 0xd0, 4.0f);
            swc1(pv + 0xd8, rand_angle());
            sh(pv + 0x3e, lhu(pv + 0x3e) | 9);
            vec_clear(V(pv + 0x40));
            sb(m + 0x20, 2);
            break;
        case 2:  // swing on its back (4) (0x2e4124)
            swc1(m + 0x44, u2f(0xbfc90fdbu));
            swc1(m + 0x48, fpu_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
            take_path();
            set_if_zero(pv + 0xd4, 8.0f);
            set_if_zero(pv + 0xd0, 8.0f);
            vec_clear(V(pv + 0x40));
            sb(m + 0x20, 4);
            break;
        case 3:  // at rest (1) (0x2e41d8)
            sh(m + 0x32, 0);
            sb(m + 0x20, 1);
            sw(m + 0x94, 0);
            break;
        case 4:  // ride (9) (0x2e41f0)
            sb(m + 0x20, 9);
            swc1(pv + 0xd0, 4.0f);
            swc1(pv + 0xd4, 4.0f);
            sh(pv + 0x3e, lhu(pv + 0x3e) & 0xfffe);
            break;
        default:
            break;
        }
        break;
    case 7: {  // along its path: the next point reached, its speed from the point's w (-1: kept) (0x2e421c)
        jet_platform_jets(m);
        const uint32_t path = lwu(pv + 0xb0);
        const int32_t n = lw(path);
        const int32_t next = ee_rem(lw(pv + 0xa0) + n + static_cast<int8_t>(lbu(pv + 0xa4)), n);
        const uint32_t point = path + (static_cast<uint32_t>(next) << 4) + 0x10;
        lq_sq(goal, point);
        if (vec3_distance(V(pos), V(point)) < 0.5f) {
            sw(pv + 0xa0, static_cast<uint32_t>(next));
            const float w = lwc1(lwu(pv + 0xb0) + (static_cast<uint32_t>(next) << 4) + 0x1c);
            if (w != -1.0f) swc1(pv + 0xd0, w);
        }
        vec3_sub(V(sp + 0x70), V(goal), V(pos));
        swc1(ang + 8, atan2_f(lwc1(sp + 0x70), lwc1(sp + 0x74)));
        tilt_and_move(goal, 0);
        break;
    }
    case 2: {  // swing with the hero's weight between its path's points 0 and 1 (0x2e434c)
        swc1(pv + 0xd8, angle_add(lwc1(pv + 0xd8), fpu_mul(dt(), u2f(0x3f9c61aau))));
        sw(pv + 0xd4, 0);
        const uint32_t path = lwu(pv + 0xb0);
        const float t = swing_weight(pv);
        lq_sq(ang, path + 0x10);
        lq_sq(goal, path + 0x20);
        vec4_lerp(pos, ang, goal, t);
        swc1(ang + 8, lwc1(m + 0x48));
        tilt_and_move(pos, 1);
        break;
    }
    case 4: {  // swing between its path's points 0 and 1 (0x2e44e8)
        swc1(pv + 0xd8, angle_add(lwc1(pv + 0xd8), fpu_mul(dt(), u2f(0x3f9c61aau))));
        const uint32_t path = lwu(pv + 0xb0);
        const float t = swing_weight(pv);
        lq_sq(ang, path + 0x10);
        lq_sq(goal, path + 0x20);
        vec4_lerp(pos, ang, goal, t);
        swc1(m + 0x44, u2f(0xbfc90fdbu));
        break;
    }
    case 3:    // a swing's pause: its end, the swing again (2) (0x2e463c)
    case 5: {  // the pause of 4: its end, swing (4) (0x2e46e4)
        if (st == 3) swc1(ang + 8, lwc1(m + 0x48));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xe0))) != 0) {
            sb(pv + 0xa4, static_cast<int8_t>(lbu(pv + 0xa4)) != 1 ? 1u : 2u);
            sb(m + 0x20, st == 3 ? 2u : 4u);
        }
        if (st == 3) {
            tilt_and_move(pos, 1);
        } else {
            if (static_cast<int8_t>(lbu(pv + 0xa4)) == 2) jet_platform_jets(m);
            lq_sq(ang, m + 0x40);
            jet_platform_move(m, ang, pos, 1);
        }
        break;
    }
    case 6: {  // drift on its move, slowly sinking; not drawn: gone (0x2e475c)
        if (vec3_length(V(pv + 0x40)) < fpu_mul(dt(), 6.0f)) swc1(pv + 0x48, fpu_add(lwc1(pv + 0x48), fpu_mul(dt_sq(), 6.0f)));
        vec3_add(V(pos), V(pos), V(pv + 0x40));
        if (lbu(m + 0x31) == 0) moby_retire(m);
        break;
    }
    case 8: {  // fall, spinning; back at its path's first point: set up again (0); below 0: gone (0x2e47c4)
        vec3_add(V(pos), V(pos), V(pv + 0x40));
        // (the original's L12:0x212638, a stub that returns 0, decides whether it hit something: never)
        if (zero_stub() != 0 && lwu(pv + 0xb0) == 0) {
            sw(sp + 0, 1);
            sw(sp + 8, 0xffffffffu);
            sw(sp + 0x10, 0);
            explosion_effect(m, pv + 0x40, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 2.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            moby_retire(m);
            return;
        }
        swc1(pv + 0x48, fpu_sub(lwc1(pv + 0x48), fpu_mul(dt_sq(), 7.0f)));
        swc1(m + 0x40, angle_add(lwc1(pv + 0xe8), lwc1(m + 0x40)));
        swc1(m + 0x44, angle_add(lwc1(pv + 0xec), lwc1(m + 0x44)));
        const uint32_t path = lwu(pv + 0xb0);
        if (path != 0 && vec2_distance(V(pos), V(path + 0x10)) < 0.5f) sb(m + 0x20, 0);
        if (lwc1(m + 0x18) < 0.0f) moby_retire(m);
        break;
    }
    case 9: {  // ride the moby +0xf0: at its height (0.2 over it with the hero on, else 0.4) (0x2e4914)
        const uint32_t other = moby_at(lw(pv + 0xf0));
        lq_sq(ang, pos);
        swc1(ang + 8, lwc1(other + 0x18));
        vec_clear(V(goal));
        swc1(goal + 8, lwc1(m + 0x48));
        const bool on = moby_hero_on_l1(m) != 0;
        swc1(ang + 8, fpu_add(lwc1(ang + 8), on ? u2f(0x3e4ccccdu) : u2f(0x3ecccccdu)));
        jet_platform_tilt(m, goal, on ? 1u : 0u);
        jet_platform_move(m, goal, ang, 1);
        break;
    }
    default:  // 1, 10 up (0x2e49e8)
        break;
    }
    // what rides it follows this frame's move and turn
    vec3_sub(V(sp + 0x40), V(pos), V(was));
    attach_frame_set(pv + 0x60, sp + 0x40, was_ang, m + 0x40);
}

}  // namespace rac1
