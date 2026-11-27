// Pokitaru's (level 11) raft (class 1075), ported by hand from the level's own code (L11:0x309ac0 and its helpers
// 0x30a500, 0x30a850): a paddle-wheeled raft that carries the hero along a path (pvar +0xa0) while he stands on it,
// rising out of the water at its end once its zone's enemies are gone, bobbing on the waves, its four wheels turning
// and spraying. The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/variants_l11.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
// The sound slot the pvar word `word` names (a looping sound of `m`) stopped when it still plays for `m`; the word -1.
void sound_stop(uint32_t m, uint32_t word) {
    if (sub_0028d8c0(m, lw(word)) == 0) return;
    const int32_t slot = lw(word);
    if (slot != -1) {
        const uint32_t ch = 0x0013e550 + static_cast<uint32_t>(slot) * 0x70;
        if (lwu(ch + 0x88) == m && lbu(ch + 0x74) != 0) sub_0028d918(slot);
    }
    sw(word, 0xffffffffu);
}
// The looping sound `sound` (0 idle, 1 moving) started into the pvar word `word` unless it plays.
void sound_keep(uint32_t m, uint32_t word, int32_t sound) {
    if (sub_0028d8c0(m, lw(word)) == 0) sw(word, static_cast<uint32_t>(moby_sound_start(sound, 4, m)));
}
}  // namespace

// The four paddle wheels (pvar +0x100 + k * 0x40 their nodes, +0xf0 + k * 4 their angles, +0x110 + k * 0x40 their
// turns): each node set up once (sub_0024f590), each wheel turned (700 degrees a second, 150 while the raft waits in
// state 1); near the camera (38), on alternate frames while it waits, and while it is in the water (below 224.5), each
// wheel's joint sprays (particle 0x22: twice a wheel, once while it waits) backwards from the raft.
// orig: L11:0x0030a500
void pok_30a500_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t row = pv + (k << 6);
        if (lbu(row + 0x101) == 0) sub_0024f590(m, k, row + 0x100);
        const float rate = lbu(m + 0x20) != 1 ? fpu_mul(dt(), u2f(0x41437a14u)) : fpu_mul(dt(), u2f(0x40278d36u));
        if (lbu(row + 0x101) == 0) continue;
        const float a = angle_add(lwc1(pv + 0xf0 + (k << 2)), rate);
        swc1(pv + 0xf0 + (k << 2), a);
        quat_from_axis_angle(V(row + 0x110), 0, a);
    }
    if (38.0f < vec2_distance(V(m + 0x10), V(L11_ADDR(0x001677c0u)))) return;
    if (lbu(m + 0x20) == 1 && (lwu(L11_ADDR(0x0015f5ccu)) & 1u) != 0) return;
    if (!(lwc1(m + 0x18) < u2f(0x43608000u))) return;
    for (uint32_t k = 0; k < 4; ++k) {
        Vec4 joint{};
        moby_chain_point_v(m, k, joint);
        for (int32_t n = (lbu(m + 0x20) ^ 1u) == 0 ? 1 : 2; n != 0; --n) {
            Vec4 at{}, vel{};
            const float jitter = u2f(0x3e99999au);  // (0.3)
            at.x = fpu_add(joint.x, rand_range_signed_f(0.0f, jitter));
            at.y = fpu_add(joint.y, rand_range_signed_f(0.0f, jitter));
            at.z = fpu_add(joint.z, rand_range_signed_f(0.0f, jitter));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_add(rand_range_signed_f(0.0f, u2f(0x3dfa35ddu)), u2f(0x40490fdbu)));
            float speed = fpu_mul(rand_range_f(2.0f, 5.0f), dt());
            if (lbu(m + 0x20) == 1) speed = fpu_mul(speed, u2f(0x3ecccccdu));
            vel.x = fpu_mul(cos_vu(yaw), speed);
            vel.y = fpu_mul(sin_vu(yaw), speed);
            vel.z = fpu_mul(rand_range_f(-2.0f, 2.0f), dt());
            const float size = rand_range_f(6300.0f, 14700.0f);
            spawn_particle_22_v(at, vel, size, 222.5f);
        }
    }
}

// The raft's zone (the moby group pvar +0xb4) clear: no live enemy of class 1246 in it that is not in a harmless state
// (a group out of range or empty counts as clear).
// orig: L11:0x0030a850
bool pok_30a850_l11(uint32_t m) {
    const int32_t group = lh(lwu(m + 0x78) + 0xb4);
    if (group < 0 || lvd::group_count() < group) return true;
    uint32_t e = lvd::group(group);
    if (e == 0) return true;
    for (;; e += 2) {
        const uint32_t o = lwu(L11_ADDR(0x0015ffd8u)) + ((lhu(e) & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(o + 0x20)) >= 0 && lh(o + 0xa6) == 0x4de && !pok_316128_l11(o)) return false;
        if (lh(e) < 0) return true;
    }
}

// Class 1075: the wheels every frame (its height first put back from the bob, +0xe4); state 0 sets it up (its
// animation 1, the second path +0xac made path +0xa8's points in its own frame, at the path +0xa0's start, the
// wheels' angles at random, no sounds), 1 waits for the level's word (+0xb8) and then takes it to the path's end
// (5); 2 carries the hero along the path (sped up to the level's speed while he stands on it, the moving sound, else
// slowing, the idle sound) to the end (the sounds stopped, 3), the camera pulled back on the path's marked points;
// 3 waits for its zone cleared, then rises (4) or not (5); 4 rises out of the water to 6 above the path's point; 5
// keeps 1 (the hero below it) or 8 above it. Then the bob on the waves (0.13), its tilt (sub_00262b80) and the frame
// of what rides it (pvar +0x60).
// orig: L11:0x00309ac0
void pok_class1075_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t start = sp, start_ang = sp + 0x10, moved = sp + 0x20;
    lq_sq(start, m + 0x10);
    lq_sq(start_ang, m + 0x40);
    if (lbu(m + 0x20) != 0) swc1(m + 0x18, lwc1(pv + 0xe4));
    pok_30a500_l11(m);
    auto point = [&](uint32_t path, uint32_t i) { return path + 0x10 + (i << 4); };
    auto bring_back = [&] {  // path +0xa8's points made the world's again from path +0xac's (in its frame)
        const uint32_t world = lvd::path(lw(pv + 0xa8)), local = lvd::path(lw(pv + 0xac));
        for (int32_t i = 0; i < lw(world); ++i) {
            mat3_transform(V(point(world, static_cast<uint32_t>(i))), V(point(local, static_cast<uint32_t>(i))), M(m + 0xc0));
            vec3_add(V(point(world, static_cast<uint32_t>(i))), V(point(world, static_cast<uint32_t>(i))), V(m + 0x10));
            lvd::path_rows_written(point(world, static_cast<uint32_t>(i)));
        }
    };
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b460)
    case 0: {  // (0x00309b7c)
        sh(m + 0x32, 0xff);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 1);
        const uint32_t world = lvd::path(lw(pv + 0xa8)), local = lvd::path(lw(pv + 0xac));
        Mat4 inv;
        mat3_transpose(inv, M(m + 0xc0));
        for (int32_t i = 0; i < lw(world); ++i) {
            Vec4 rel{};
            vec3_sub(rel, V(point(world, static_cast<uint32_t>(i))), V(m + 0x10));
            mat4_transform(V(point(local, static_cast<uint32_t>(i))), rel, inv);
            lvd::path_rows_written(point(local, static_cast<uint32_t>(i)));
        }
        const uint32_t path = lvd::path(lw(pv + 0xa0));
        sw(pv + 0xc4, path);
        lq_sq(m + 0x10, path + 0x10);
        sw(pv + 0xd4, 1);
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 4; ++k) swc1(pv + 0xf0 + (k << 2), rand_angle());
        sw(pv + 0xcc, 0xffffffffu);
        sw(pv + 0xc8, 0xffffffffu);
        sw(pv + 0x9c, lwu(pv + 0x9c) | 1u);
        break;
    }
    case 1: {  // (0x00309cd8)
        if (lw(pv + 0xb8) == 0) break;
        const uint32_t path = lvd::path(lw(pv + 0xa0));
        sw(pv + 0xc4, path);
        const uint32_t n = lwu(path);
        lq_sq(m + 0x10, path + (n << 4) - 0x10);
        const uint32_t last = path + ((n - 1) << 4);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(last + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(last + 0x14), lwc1(m + 0x14))));
        sb(m + 0x20, 5);
        break;
    }
    case 2: {  // (0x00309d78)
        const uint32_t path = lwu(pv + 0xc4);
        const Vec4 target = V(point(path, lwu(pv + 0xd4)));
        if (lwu(0x0013f64c) == m) {
            sound_keep(m, pv + 0xcc, 1);
            sound_stop(m, pv + 0xc8);
            approach_f(pv + 0xd0, fpu_mul(lwc1(L11_ADDR(0x00161dd4u)), dt()), fpu_mul(dt2(), 6.0f));
        } else {
            sound_keep(m, pv + 0xc8, 0);
            sound_stop(m, pv + 0xcc);
            approach_f(pv + 0xd0, 0.0f, fpu_mul(dt2(), 6.0f));
        }
        const uint32_t step = ee_stack_alloc(0x10);
        vec3_sub(V(step), target, V(m + 0x10));
        vec3_clamp_length(step, lwc1(pv + 0xd0));
        vec3_add(V(m + 0x10), V(m + 0x10), V(step));
        if (lw(pv + 0xd4) < lw(lwu(pv + 0xc4)) - 1) {
            if (vec3_distance(V(m + 0x10), target) < fpu_mul(lwc1(L11_ADDR(0x00161dd4u)), dt())) {
                const uint32_t next = lwu(pv + 0xd4) + 1;
                sw(pv + 0xd4, next);
                if (lwc1(lwu(pv + 0xc4) + (next << 4) + 0x1c) == 37.0f) sw(pv + 0xc0, 1);
            }
        } else if (vec2_length(V(step)) < u2f(0x3a83126fu)) {
            sound_stop(m, pv + 0xcc);
            sound_stop(m, pv + 0xc8);
            sb(m + 0x20, 3);
        }
        ee_stack_free(0x10);
        const float deg = u2f(0x3c8efa35u);
        const float turn = fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161dd8u)), deg), dt2());
        yaw_seek(m + 0x48, pv + 0xd8, atan2_f(fpu_sub(target.x, lwc1(m + 0x10)), fpu_sub(target.y, lwc1(m + 0x14))), turn, turn,
                 fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161ddcu)), deg), dt()));
        if (lw(pv + 0xc0) != 0) {
            cam0_set_distance(0, lwc1(L11_ADDR(0x00161decu)), u2f(0x3b449ba6u));
            cam0_set_height(lwc1(L11_ADDR(0x00161df0u)), u2f(0x3b449ba6u));
        }
        bring_back();
        break;
    }
    case 3:  // (0x0030a1c8)
        if (!pok_30a850_l11(m)) break;
        if (lw(pv + 0xb0) == 0) {
            sb(m + 0x20, 5);
        } else {
            sw(pv + 0xd0, 0);
            sb(m + 0x20, 4);
        }
        break;
    case 4: {  // (0x0030a214)
        const float z = lwc1(point(lwu(pv + 0xc4), lwu(pv + 0xd4)) + 8);
        seek_1d(m + 0x18, pv + 0xd0, fpu_add(z, 6.0f), dt2(), dt2(), dt());
        if (fpu_add(z, u2f(0x40bccccdu)) <= lwc1(m + 0x18)) sb(m + 0x20, 5);
        break;
    }
    case 5: {  // (0x0030a2bc)
        const float z = lwc1(point(lwu(pv + 0xc4), lwu(pv + 0xd4)) + 8);
        if (lw(pv + 0xb0) == 0) break;
        if (lwc1(0x0013f3d8) < lwc1(m + 0x18)) seek_1d(m + 0x18, pv + 0xd0, fpu_add(z, 1.0f), dt2(), dt2(), dt());
        else seek_1d(m + 0x18, pv + 0xd0, fpu_add(z, 8.0f), dt2(), dt2(), dt());
        break;
    }
    default:
        break;
    }
    // the bob, the tilt and the riders' frame (0x0030a388)
    const float t = dt();
    swc1(pv + 0xe4, lwc1(m + 0x18));
    const float phase = angle_add(lwc1(pv + 0xe0), fpu_mul(t, u2f(0x3fc90fdbu)));
    swc1(pv + 0xe0, phase);
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(sin_vu(phase), u2f(0x3e051eb8u))));
    sub_00262b80(m, pv + 0xe8, pv + 0xec, u2f(0x3d2b92a6u), fpu_mul(t, u2f(0x3edf66f3u)), fpu_mul(t, u2f(0x3f1c61aau)));
    vec3_sub(V(moved), V(m + 0x10), V(start));
    attach_frame_set(pv + 0x60, moved, start_ang, m + 0x40);
    ee_stack_free(0x30);
}

}  // namespace rac1
