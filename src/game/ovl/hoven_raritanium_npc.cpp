// Hoven's (level 12) class 282: the man who hands Ratchet the Raritanium (once: the flag 0x13d389; gone afterwards), with
// the moby he holds (class 0x11f, carried at his joint 2), and the level's helper that carries a moby at a joint (also
// class 294's). Level 12 only: the addresses are Hoven's (`orig: L12:`), the level's own data L12_ADDR, the base ELF's bare.
// Registered in hoven_mobys.cpp.
//
// The moby: +0x20 the state (0 set up, 1 waiting for the talk, 2 the talk's end), +0x53 the animation set playing,
// +0xb0 its checkpoint; the pvars: +0x20 the talk record, +0x60 / +0xe0 the head's and the body's joint springs, +0xc4 /
// +0xc8 / +0x148 the head's pitch and turns, +0x160 where he looks, +0x170 the box the checkpoint is saved at (-1 none),
// +0x178 the frames he keeps looking at the hero, +0x17c the next look's timer, +0x180 the moby he holds.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l12.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his place, +0xd0 his head's, +0x100 his move)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// The moby `child` carried at `m`'s joint `joint`: placed and turned with it, animated, shown and hidden with it.
// orig: L12:0x0027b9c0
void moby_carry_at_joint(uint32_t m, uint32_t child, uint32_t joint) {
    const Frame f(128);
    const uint32_t sp = f.sp;
    moby_chain_world_one(m, joint, sp);
    lq_sq(child + 0x10, sp + 0x30);
    moby_anim_advance(child);
    moby_update_transform(child);
    mat3_copy(M(child + 0xc0), M(sp));
    mat3_normalize_columns(child + 0xc0);
    moby_update_transform_kept(child);
    const uint32_t flags = lhu(child + 0x34);
    sh(child + 0x34, (lhu(m + 0x34) & 1) != 0 ? (flags | 0x41) : (flags & 0xffbe));
    sh(child + 0x34, lhu(child + 0x34) | 6);
}

// The update: the talk (its end: the Raritanium given, the message, the checkpoint, the level's restart), his head
// looking at the hero near and in front of him or about at random, the joints' springs, the moby he holds.
// orig: L12:0x002e6c48
void hov_class282_update_l12(uint32_t m) {
    const Frame f(176);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 1u);
    switch (lbu(m + 0x20)) {
    case 0:  // set up: gone when the Raritanium was given; else the talk (0x2e6cb8)
        if (lbu(0x0013d389) != 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        talk_init(m, pv + 0x20);
        band_set(m);
        break;
    case 1:  // the talk: done, the camera put on him (hero_walk_to_l1, L12:0x27bea8), its end (0x2e6cf0)
        if (lbu(0x0013d389) == 0 && talk_update(m, pv + 0x20) != 0) {
            hero_walk_to_l1(2.5f, m);
            sw(m + 0x58, 0);
            sb(m + 0x20, 2);
        }
        break;
    case 2:  // the talk's end, once the scene is over: the Raritanium (0x2e6d34)
        if (lw(L12_ADDR(0x0015f5c4)) != 2) {
            swc1(m + 0x58, 1.0f);
            sb(0x0013d389, 1);
            sub_00263d40(TXT(0x2ee7), frame_scale_i(0x12c));  // ("You've obtained some Raritanium!")
            sb(0x0013d3e9, 1);
            checkpoint_mark_collected(lbu(m + 0xb0));
            if (lw(pv + 0x170) != -1) {
                const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x170));
                checkpoint_save(box + 0x30, box + 0x70);
            }
            level_change_request(0, -1);
            sb(m + 0x20, 1);
        }
        break;
    default:
        break;
    }

    // the head: at the hero when he is near, in front and moving (or a while after), else about at random
    float spring_k = u2f(0x3ca3d70au), spring_d = u2f(0x3e99999au);
    if (lbu(m + 0x53) == 0) {
        const uint32_t pos = m + 0x10;
        bool tick_timer = false, at_hero = false;
        if (vec2_distance(V(pos), V(HERO + 0x80)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO + 0xd0), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0xd4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                at_hero = true;
                if (u2f(0x3c23d70au) < vec3_length(V(HERO + 0x100))) sw(pv + 0x178, static_cast<uint32_t>(frame_scale_i(0x78)));
                else tick_timer = true;
            }
        }
        if (tick_timer) timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x178)));
        if (!at_hero && lwu(pv + 0x178) != 0) {
            sw(pv + 0x178, 0);
            lq_sq(pv + 0x160, HERO + 0xd0);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x17c))) != 0) {  // a new look about, 3 .. 5 s on
            const float deg = u2f(0x3c8efa35u);
            sw(pv + 0x17c, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + 0x160), 6.0f, yaw, pitch);
            vec3_add(V(pv + 0x160), V(pv + 0x160), V(pos));
        }
        if (lwu(pv + 0x178) != 0) {
            lq_sq(sp, HERO + 0xd0);
            spring_k = u2f(0x3d23d70au);
            spring_d = u2f(0x3e99999au);
        } else {
            lq_sq(sp, pv + 0x160);
        }
        // the head's turn (at most 90 degrees) and pitch (at most 30) toward the point
        lq_sq(sp + 0x10, pos);
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 1.0f));
        const uint32_t d = sp + 0x20;
        vec3_sub(V(d), V(sp), V(sp + 0x10));
        float turn = angle_sub(atan2_f(lwc1(d + 0), lwc1(d + 4)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(d)), lwc1(d + 8));
        const float kHalfPi = u2f(0x3fc90fdbu), kSixth = u2f(0x3f060a92u);
        if (kHalfPi < turn) turn = kHalfPi;
        else if (turn < -kHalfPi) turn = -kHalfPi;
        if (kSixth < pitch) pitch = kSixth;
        else if (pitch < -kSixth) pitch = -kSixth;
        swc1(pv + 0xc4, pitch);
        swc1(pv + 0xc8, fpu_mul(turn, u2f(0x3e99999au)));
        swc1(pv + 0x148, fpu_mul(turn, u2f(0x3f333333u)));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xd0, 2.75f);
    {
        const float k = lwc1(0x0015ed64);
        joint_node_spring(m, pv + 0x60, 1, fpu_mul(spring_k, k), fpu_mul(spring_d, k));
    }
    {
        const float k = lwc1(0x0015ed64);
        joint_node_spring(m, pv + 0xe0, 0, fpu_mul(spring_k, k), fpu_mul(spring_d, k));
    }
    // the moby he holds: spawned the first frame (class 0x11f), then carried at his joint 2
    if (lwu(pv + 0x180) != 0) {
        moby_carry_at_joint(m, lwu(pv + 0x180), 2);
        return;
    }
    const uint32_t held = moby_spawn(0x11f);
    sw(pv + 0x180, held);
    if (held == 0) return;
    sh(held + 0x32, lhu(m + 0x32));
    sb(lwu(pv + 0x180) + 0x31, 1);
    const uint32_t h = lwu(pv + 0x180);
    lq_sq(h + 0x10, m + 0x10);
    lq_sq(h + 0x40, m + 0x40);
    moby_update_transform(m);
}

}  // namespace rac1
