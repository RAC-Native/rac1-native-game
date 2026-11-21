// Kalebo III (level 16), class 1410: the taxi, Kalebo III's build of the path-riding rafts (shared_floaters.h; Rilgar's
// class 998 in rilgar_ocean.cpp and Oltanis's class 712 in oltanis_raft.cpp are its siblings). Its three stages (pvars
// +0xac, the next +0xae) are camera points (+0x80 .. +0x88: box indices); between them it rides the level's paths (+0x90,
// +0x94, +0x9c; the ride's path at +0xa4). Waiting at a stage (state 1) with the hero aboard within its deck (1.2 across)
// it shows the stage's message (0x3e89 / 0x3e8a) and, with the button (0x13cae4 bit 0x10) while the level's word 0x15f594
// is 8, sets off (3); the taxi at the other end is told (2, hidden). It glides from the stage's point to the path's start
// (3), rides the path (4) and glides from its end to the next stage's point (5), then waits there (1, the next stage).
// While it moves (3 .. 5) the hero is held to it and its path laid out, and its sound (4, the slot +0xaa) kept; stopped,
// its sound stops. State 6 waits for the level's flag 0x13d4e1 and then comes back (shown). Level 16 only: `orig: L16:`,
// level data L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class1410, the P16.5 checks).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l16.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;

constexpr uint32_t HERO = 0x0013f350u;
float dt() { return lwc1(0x0015ed6cu); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70u); }  // its square
}  // namespace

// The taxi's next ride: by its stage (+0xac) the path to ride (the level's path of word +0x90 / +0x94 / +0x9c, at +0xa4)
// and the stage it ends at (+0xae: 1 / 0 / 1); another stage keeps the path it has. Its progress (+0xb0, +0xb4) back to 0
// and its pace (+0xc4 a frame, +0xc8 a frame squared) over the path's length (its first segment times its count).
// orig: L16:0x002e4a58
void kal_taxi_route_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t stage = lh(pv + 0xac);
    uint32_t word = 0, ends = 0;
    bool set = true;
    switch (stage) {  // (the original's compare chain)
    case 0: word = 0x90; ends = 1; break;  // (0x002e4ac0)
    case 1: word = 0x94; ends = 0; break;  // (0x002e4aa0)
    case 2: word = 0x9c; ends = 1; break;  // (0x002e4a90)
    default: set = false; break;           // (0x002e4ae0)
    }
    if (set) {
        const uint32_t path = lvd::path(lw(pv + word));
        sh(pv + 0xae, ends);
        sw(pv + 0xa4, path);
    }
    const uint32_t path = lwu(pv + 0xa4);
    const float len = fpu_mul(vec3_distance(V(path + 0x10), V(path + 0x20)), static_cast<float>(lw(path)));
    const float v = fpu_mul(dt(), 20.0f), a = fpu_mul(dt2(), 5.0f);
    sw(pv + 0xb4, 0);
    sw(pv + 0xb0, 0);
    const float inv = fpu_div(1.0f, len);
    swc1(pv + 0xc8, fpu_mul(a, inv));
    swc1(pv + 0xc4, fpu_mul(v, inv));
}

// orig: L16:0x002e43e0
void kal_class1410_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x100);
    const uint32_t sp = f.sp;
    lq_sq(sp, m + 0x10);
    lq_sq(sp + 0x10, m + 0x40);
    talker_word_set(m, (lhu(m + 0x34) ^ 1u) & 1u);
    // the box (a camera point) of stage pvar half `stage_half`: its place and turn at sp + at / sp + at + 0x20
    auto stage_point = [&](uint32_t stage_half, uint32_t at) {
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x80 + (static_cast<uint32_t>(lh(pv + stage_half)) << 2)));
        lq_sq(sp + at, box + 0x30);
        lq_sq(sp + at + 0x20, box + 0x70);
    };
    // the ride point and turn (+0x60, +0x74 / +0x78) at the progress +0xb0 between sp + 0x30 (turn + 0x50) and sp + 0x40
    // (+ 0x60); at 1: done
    auto glide = [&] {
        const float t = lwc1(pv + 0xb0);
        vec3_lerp(V(pv + 0x60), V(sp + 0x30), V(sp + 0x40), t);
        swc1(pv + 0x74, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x64), lwc1(sp + 0x54)), lwc1(pv + 0xb0)), lwc1(sp + 0x54)));
        swc1(pv + 0x78, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x68), lwc1(sp + 0x58)), lwc1(pv + 0xb0)), lwc1(sp + 0x58)));
        return 1.0f <= lwc1(pv + 0xb0);
    };
    switch (lbu(m + 0x20)) {  // (the table 0x1e8800)
    case 0:  // (0x002e446c)
        sh(pv + 0xaa, 0xffff);
        floater_arrive_l16(m);
        break;
    case 1: {  // (0x002e4484) waits at its stage: the hero standing on it (his ground moby +0x2fc, on his feet +0x30e)
        if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30e) != 0) break;
        mat3_transpose(M(sp + 0x30), M(m + 0xc0));
        vec3_sub(V(sp + 0x70), V(0x0013f3d0u), V(m + 0x10));
        sw(sp + 0x7c, 0);
        mat4_transform(V(sp + 0x70), V(sp + 0x70), M(sp + 0x30));
        if (!(abs_f(lwc1(sp + 0x74)) < u2f(0x3f99999au))) break;  // (within 1.2 of its middle line)
        kal_taxi_route_l16(m);
        floater_message(m, 0xae, TXT(0x3e89), TXT(0x3e8a));
        if ((lwu(0x0013cae4u) & 0x10u) == 0 || lw(L16_ADDR(0x0015f594)) != 8) break;
        sb(m + 0x20, 3);
        const uint32_t far = floater_find(m, lh(pv + 0xae), 0xac);
        if (far == 0) break;
        sb(far + 0x20, 2);
        sw(far + 0x94, 0);
        sh(far + 0x34, lhu(far + 0x34) | 1u);
        sb(far + 0x31, 0);
        break;
    }
    case 3:  // (0x002e4568) to the path's start
        seek_1d(pv + 0xb0, pv + 0xb4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.5f));
        stage_point(0xac, 0x30);
        path_point(0.0f, lwu(pv + 0xa4), 0, sp + 0x40, sp + 0x60, 0);
        if (glide()) {
            swc1(pv + 0xb4, 0.0f);
            swc1(pv + 0xb0, 0.0f);
            sb(m + 0x20, 4);
        }
        break;
    case 4:  // (0x002e4680) rides the path
        if (floater_ride(m, 0xb0, 0xa4) != 0) {
            sw(pv + 0xb0, 0);
            sw(pv + 0xb4, 0);
            sb(m + 0x20, 5);
        }
        break;
    case 5: {  // (0x002e46a0) from the path's end to the next stage's point
        seek_1d(pv + 0xb0, pv + 0xb4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.5f));
        stage_point(0xae, 0x40);
        const uint32_t path = lwu(pv + 0xa4);
        path_point(static_cast<float>(lw(path)), path, 0, sp + 0x30, sp + 0x50, 0);
        if (glide()) {
            sw(pv + 0xb0, 0);
            sw(pv + 0xb4, 0);
            sb(m + 0x20, 1);
            floater_next_stage(m, 0xac);
        }
        break;
    }
    case 6:  // (0x002e47c4) gone until the level's flag
        if (lbu(0x0013d4e1u) != 0) {
            const uint32_t anim = lwu(lwu(m + 0x24) + 0x10);
            const uint32_t flags = lhu(m + 0x34) & 0xfffeu;
            sb(m + 0x20, 1);
            sw(m + 0x94, anim);
            sh(m + 0x34, flags);
            sb(m + 0x31, 1);
        }
        break;
    default:  // (2: hidden at the far end; 7 and past: the table's bound, 0x002e47f4)
        break;
    }
    floater_bob(m, 0xb8);
    if (lbu(m + 0x20) - 3u < 3u) {  // (0x002e4808) moving: the hero held, the path laid out, the sound kept
        sh(0x0013f544u, 2);
        sh(HERO + 0x1f2, 2);
        floater_lay_path(m, 0xa0);
        if (sub_0028d8c0(m, lh(pv + 0xaa)) == 0) sh(pv + 0xaa, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    } else if (sub_0028d8c0(m, lh(pv + 0xaa)) != 0) {  // (0x002e4854) stopped: its sound stopped
        const int32_t slot = lh(pv + 0xaa);
        if (slot != -1) {
            const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sh(pv + 0xaa, 0xffff);
    }
    vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
    attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
}

}  // namespace rac1
