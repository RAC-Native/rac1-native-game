// Kalebo III (level 16), class 1667 (and any class of the same update): a group of mobys going round the level's path of
// pvars +0 in a closed line. State 0 lines the group up (0x2e7ba0: in a random order, each one's place along the path
// spread evenly, its leader the one before, its rise and speed drawn from the level's ranges) or, with no group, retires
// it; state 1 moves it along the path (pvars +4 the place, +8 the speed easing to +0x18, +0x14 the rise a frame), takes
// over the leader's speed when it catches up with it (closer than 0x161e68: they swap), and slows both down when closer
// than 1.5. Hidden (+0x31 clear) and more than 0x161e6c + 1 behind its leader, it is put farther on, at a point off the
// screen (a few times just behind the leader while that one is shown, +0x1c the count). A class of an odd number leaves
// a trail of particle 0x16 behind it near the camera (0x2e7a30, every other frame); one of an even number keeps its
// sound 4 playing (the slot pvars +0x1e) and, while it plays, counts as shown. Level 16 only: `orig: L16:`, level data
// L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class1667, the P16.6 checks).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>

#include <game/ovl/variants_l16.h>

#include <cstdint>
#include <vector>

namespace rac1 {

namespace {
using kalebo::Frame;

float dt() { return lwc1(0x0015ed6cu); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70u); }  // its square
}  // namespace

// The trail: shown, on the frames of its own parity (the frame counter's low bit against its address's bit 8) and within
// 75 of the camera, one particle 0x16 half a unit behind it (along its matrix's row 0), sent off at a random direction
// with the level's speed and pushed by its move since the frame's start (`was`: its place then, an EE vector).
// orig: L16:0x002e7a30
void kal_flock_trail_l16(uint32_t m, uint32_t was) {
    const Frame f(0xa0);
    const uint32_t from = f.sp, at = f.sp + 0x10, push = f.sp + 0x20, vel = f.sp + 0x30;
    lq_sq(from, was);
    if (lbu(m + 0x31) == 0) return;
    if ((lwu(L16_ADDR(0x0015f5cc)) & 1u) != ((m >> 8) & 1u)) return;
    const uint32_t pos = m + 0x10;
    if (!(vec3_distance(V(pos), V(L16_ADDR(0x001671c0))) < 75.0f)) return;  // (the camera)
    const float moved = vec3_distance(V(pos), V(from));
    vec3_scale(V(push), V(m + 0xc0), fpu_mul(lwc1(L16_ADDR(0x00161ea0)), moved));
    const float speed = fpu_mul(lwc1(L16_ADDR(0x00161ea4)), dt());
    vec3_set_length(V(at), V(m + 0xc0), -0.5f);
    vec3_add(V(at), V(at), V(pos));
    const float yaw = rand_angle();
    const float pitch = rand_angle();
    vec_from_spherical_v(V(vel), speed, yaw, pitch);
    vec3_add(V(vel), V(vel), V(push));
    const float size = fpu_mul(lwc1(L16_ADDR(0x00161ea8)), 210000.0f);
    particle_16_spawn(at, vel, lwu(L16_ADDR(0x00161e98)), lwu(L16_ADDR(0x00161e9c)), frame_scale_i(0x23), size);
}

// The line-up of the group of moby `m` (+0x21) on its path (pvars +0, none: nothing): the members taken in a random order
// (the last of the group's list always last), each placed at (n - k) x (the path's length / n) for the k-th taken, its
// leader (+0x10) the one taken before it (the first's: the list's last), the path's length at +0xc, its rise (+0x14) and
// speed (+8 and +0x18) drawn from the level's ranges, shown with the level's draw distance (+0x32), its scale (+0x2c)
// times 0x161e94, +0x94 cleared, state 1.
// orig: L16:0x002e7ba0
void kal_flock_line_up_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t e = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
    if (e == 0) return;
    const int32_t path_i = lw(pv);
    if (path_i == -1) return;
    const float len = fpu_sub(static_cast<float>(lw(lvd::path(path_i))), 1.0f);  // (its point count, less one)
    std::vector<uint32_t> left;  // (the original's list on its stack: read by itself only)
    for (;; e += 2) {
        left.push_back(kalebo::moby_at(lhu(e)));
        if (lh(e) < 0) break;
    }
    const int32_t n = static_cast<int32_t>(left.size());
    uint32_t leader = left[static_cast<size_t>(n - 1)];
    int16_t place = static_cast<int16_t>(n);
    for (int32_t k = 0; k < n; ++k) {
        uint32_t member;
        if (k == n - 1) {
            member = left[static_cast<size_t>(k)];
        } else {
            int32_t r = rand_mod(n - 1);
            while (left[static_cast<size_t>(r)] == 0) r = ee_rem(r + 1, n - 1);  // (the next one not taken yet)
            member = left[static_cast<size_t>(r)];
            left[static_cast<size_t>(r)] = 0;
        }
        const float at = static_cast<float>(place);
        const uint32_t mpv = lwu(member + 0x78);
        sb(member + 0x30, 0xff);
        sw(member + 0x94, 0);
        sh(member + 0x32, lhu(L16_ADDR(0x00161e90)));
        swc1(member + 0x2c, fpu_mul(lwc1(member + 0x2c), lwc1(L16_ADDR(0x00161e94))));
        const float step = fpu_div(len, int_to_float(n));
        sw(mpv + 0x10, leader);
        swc1(mpv + 0xc, len);
        leader = member;
        swc1(mpv + 4, fpu_mul(at, step));
        swc1(mpv + 0x14, rand_range_f(lwc1(L16_ADDR(0x00161e7c)), lwc1(L16_ADDR(0x00161e80))));
        const float speed = fpu_mul(rand_range_f(lwc1(L16_ADDR(0x00161e70)), lwc1(L16_ADDR(0x00161e74))), dt());
        swc1(mpv + 0x18, speed);
        swc1(mpv + 8, speed);
        sb(member + 0x20, 1);
        place = static_cast<int16_t>(place - 1);
    }
}

// orig: L16:0x002e76b8
void kal_class1667_update_l16(uint32_t m) {
    const Frame f(0xb0);
    const uint32_t was = f.sp, turn = f.sp + 0x10, ahead = f.sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv));
    const bool trail = (lhu(m + 0xa6) & 1u) != 0;
    bool sounding = false;
    if (!trail) {
        if (sub_0028d8c0(m, lh(pv + 0x1e)) != 0) sounding = true;
        else sh(pv + 0x1e, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    } else {
        lq_sq(was, m + 0x10);
    }

    switch (lbu(m + 0x20)) {  // (the original's compare chain)
    case 0:  // (0x002e7768)
        if (lbu(m + 0x21) == 0xff) {
            moby_retire(m);
            return;
        }
        kal_flock_line_up_l16(m);
        break;
    case 1: {  // (0x002e7798)
        const uint32_t lpv = lwu(lwu(pv + 0x10) + 0x78);  // (the leader's pvars)
        path_point(lwc1(pv + 4), path, 0, m + 0x10, m + 0x40, 0);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x14)));
        const float ease = fpu_mul(lwc1(L16_ADDR(0x00161e78)), dt2());
        swc1(pv + 4, fpu_add(lwc1(pv + 4), lwc1(pv + 8)));
        approach_f(pv + 8, lwc1(pv + 0x18), ease);
        float gap = fpu_sub(lwc1(lpv + 4), lwc1(pv + 4));  // (how far behind its leader, round the path)
        if (gap < 0.0f) gap = fpu_add(gap, lwc1(pv + 0xc));
        if (gap < lwc1(L16_ADDR(0x00161e68))) {
            const float mine = lwc1(pv + 0x18), its = lwc1(lpv + 0x18);
            if (its < mine) {
                swc1(lpv + 0x18, mine);
                swc1(pv + 0x18, its);
            }
        }
        if (gap < 1.5f) {
            const float brake = fpu_mul(dt2(), 50.0f);
            approach_f(pv + 8, lwc1(pv + 0x18), brake);
            approach_f(lpv + 8, lwc1(lpv + 0x18), brake);
        }
        if (lbu(m + 0x31) != 0 || sounding) {
            sh(pv + 0x1c, 0x1e);
        } else if (fpu_add(lwc1(L16_ADDR(0x00161e6c)), 1.0f) < gap) {
            const float rise = rand_range_f(lwc1(L16_ADDR(0x00161e7c)), lwc1(L16_ADDR(0x00161e80)));
            float to;
            if (lbu(lwu(pv + 0x10) + 0x31) != 0 && lh(pv + 0x1c) != 0) {  // (just behind its shown leader)
                const float back = rand_range_f(lwc1(L16_ADDR(0x00161e68)), lwc1(L16_ADDR(0x00161e6c)));
                sh(pv + 0x1c, lhu(pv + 0x1c) - 1u);
                to = fpu_sub(lwc1(lpv + 4), back);
            } else {
                to = fpu_add(lwc1(pv + 4), fpu_mul(lwc1(L16_ADDR(0x00161e8c)), dt()));
            }
            if (to < 0.0f) to = fpu_add(to, lwc1(pv + 0xc));
            path_point(to, path, 0, ahead, turn, 1);
            swc1(ahead + 0xc, 2.0f);
            swc1(ahead + 8, fpu_add(lwc1(ahead + 8), rise));
            if (point_on_screen(ahead, static_cast<float>(lw(L16_ADDR(0x00161e90)))) == -1) {
                const float speed = rand_range_f(lwc1(L16_ADDR(0x00161e70)), lwc1(L16_ADDR(0x00161e74)));
                swc1(pv + 4, to);
                swc1(pv + 0x14, rise);
                swc1(pv + 0x18, fpu_mul(speed, dt()));
            }
        }
        const float t = lwc1(pv + 4);
        if (lwc1(pv + 0xc) < t) swc1(pv + 4, fpu_sub(t, lwc1(pv + 0xc)));
        break;
    }
    default:  // (0x002e79e8)
        break;
    }
    if (trail) kal_flock_trail_l16(m, was);
}

}  // namespace rac1
