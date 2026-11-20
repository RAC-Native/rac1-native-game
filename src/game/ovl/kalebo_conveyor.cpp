// Kalebo III's (level 16) class 471: a conveyor's segments. A group's segments of class 471 move together: each frame the
// segment eases its speed (pvar +0x6c) toward the level's conveyor speed (L16 0x16198c a second) in its direction and gives
// that move to the attach frame of every segment of its group (what stands on them is carried). Ratchet standing in one of
// its two boxes (pvars +0x64 / +0x68) turns it the other way. The nearest segment of the group to the camera, within 64,
// carries the hum (sound 0xd). Queues its draw callback L16:0x2c9cd0. Level 16 only: `orig: L16:`, level data L16_ADDR,
// base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class471, conveyor_*).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
using kalebo::for_each_group_moby;
using kalebo::Frame;

constexpr int16_t kSegmentClass = 0x1d7;  // class 471

// The hum: the group's segment nearest the camera (within 1024) takes the sound slot +0x74 when it is 64 or nearer: a slot
// already playing for a segment is moved to it (2.5 along its direction +0xe0 from it), else the sound is started (and, when
// it starts, placed 2.5 along `m`'s own direction from `m`).
// orig: L16:0x002c9a50
void conveyor_hum(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t nearest = 0;
    float best = 1024.0f;
    for_each_group_moby(lbu(m + 0x21), [&](uint32_t seg) {
        if (lh(seg + 0xa6) != kSegmentClass) return;
        const float d = vec3_distance(V(L16_ADDR(0x001671c0)), V(seg + 0x10));
        if (d < best) { nearest = seg; best = d; }
    });
    if (!(vec3_distance(V(nearest + 0x10), V(L16_ADDR(0x001671c0))) < 64.0f)) return;
    const int32_t slot = lw(pv + 0x74);
    if (slot >= 0) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        const uint32_t owner = lwu(e + 0x88);
        if (owner != 0 && lh(owner + 0xa6) == kSegmentClass) {
            Vec4 out;
            vec3_set_length(out, V(nearest + 0xe0), 2.5f);
            vec3_add(V(0x0013e5e0u + static_cast<uint32_t>(lw(pv + 0x74) * 0x70)), out, V(nearest + 0x10));
            sw(e + 0x88, nearest);
            return;
        }
    }
    const int32_t started = moby_sound_start(0, 0xd, nearest);
    sw(pv + 0x74, static_cast<uint32_t>(started));
    if (started < 0) return;
    Vec4 out;
    vec3_set_length(out, V(m + 0xe0), 2.5f);
    vec3_add(V(0x0013e5e0u + static_cast<uint32_t>(lw(pv + 0x74) * 0x70)), out, V(m + 0x10));
}

// The move `delta` (an EE vector) given to the attach frame (pvar +0x20) of every segment of the group.
// orig: L16:0x002c9c38
void conveyor_carry(int32_t group, uint32_t delta) {
    for_each_group_moby(group, [delta](uint32_t seg) {
        if (lh(seg + 0xa6) != kSegmentClass) return;
        attach_frame_set(lwu(seg + 0x78) + 0x20, delta, seg + (0x40), seg + 0x40);
    });
}

}  // namespace

// Class 471: state 0 sets the segment up (pvar +0x5c bit 4, its collision on; with no box +0x60 it stays in state 0, else it
// starts in state 2); state 1 runs forward until Ratchet is in box +0x64, state 2 backward until he is in box +0x68. In every
// state but 0 the hum follows.
// orig: L16:0x002c9878
void kal_class471_update_l16(uint32_t m) {
    const Frame f(80);
    const uint32_t pv = lwu(m + 0x78);
    // the move of the frame: the speed eased toward `target` (12 x dt^2 a frame), along the segment's direction +0xc0
    auto run = [m, pv, &f](float target) {
        if (lwc1(pv + 0x6c) != target) {
            approach_f(pv + 0x6c, target, fpu_mul(lwc1(0x0015ed70), 12.0f));
            vec3_set_length(V(f.sp), V(m + 0xc0), lwc1(pv + 0x6c));
            conveyor_carry(lbu(m + 0x21), f.sp);
        }
    };
    switch (lbu(m + 0x20)) {
    case 0:
        sw(pv + 0x5c, lwu(pv + 0x5c) | 4u);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        if (lw(pv + 0x60) == -1) sh(m + 0x34, lhu(m + 0x34) | 2u);
        else sb(m + 0x20, 2);
        sw(pv + 0x74, 0xffffffffu);
        break;
    case 1:
        run(fpu_mul(lwc1(L16_ADDR(0x0016198c)), lwc1(0x0015ed6c)));
        if (point_in_box(0x0013f3d0, lw(pv + 0x64)) != 0) sb(m + 0x20, 2);
        sub_001f93e0(L16_ADDR(0x002c9cd0), m);
        break;
    case 2:
        run(-fpu_mul(lwc1(L16_ADDR(0x0016198c)), lwc1(0x0015ed6c)));
        if (point_in_box(0x0013f3d0, lw(pv + 0x68)) != 0) sb(m + 0x20, 1);
        sub_001f93e0(L16_ADDR(0x002c9cd0), m);
        break;
    default:
        break;
    }
    if (lbu(m + 0x20) != 0) conveyor_hum(m);
}

}  // namespace rac1
