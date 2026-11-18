// Hoven's (level 12) class 328: the man who fixes the ship and hands Clank the Hydro-Pack (item 4, flag 0x13d4c4; message
// TXT(0x2ee6), checkpoint, level restart), with the two props he spawns while 0x13da1c is zero (classes 0x4c1 and 0x196).
// Level 12 only: the addresses are Hoven's (`orig: L12:`), the level's own data L12_ADDR, the base ELF's bare.
// Registered in hoven_mobys.cpp.
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

struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// The Hydro-Pack NPC's update: his animation (by 0x13da1c), the talk and reward, his head looking at the hero or about
// at random, the two props (+0x164 class 0x4c1, +0x168 class 0x196), and the joints' springs.
// orig: L12:0x002eb570
void hov_class328_update_l12(uint32_t m) {
    const Frame f(192);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    class_mates_mark_level(m, 1u);
    if (lw(0x0013da1c) == 0) {
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 2);
    } else {
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 2);
    }

    switch (lbu(m + 0x20)) {
    case 0:  // set up (0x2eb614 / 0x2eb63c)
        sb(m + 0x20, 1);
        talk_init(m, pv + 0x20);
        swc1(pv + 0x2c, u2f(0x3fd9999au));
        band_set(m);
        if (lbu(0x0013d4c4) != 0) sh(pv + 0x56, 0xffffu);
        break;
    case 1:  // waiting for the talk (0x2eb678)
        if (lbu(0x0013d4c4) == 0 && talk_update(m, pv + 0x20) != 0) {
            hero_walk_to_l1(u2f(0x402147aeu), m);
            sw(m + 0x58, 0);
            sb(m + 0x20, 2);
        }
        break;
    case 2:  // the talk's end, once the scene is over (0x2eb6c0)
        if (lw(L12_ADDR(0x0015f5c4)) != 2) {
            swc1(m + 0x58, 1.0f);
            checkpoint_mark_collected(lbu(m + 0xb0));
            if (lw(pv + 0x160) != -1) {
                const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x160));
                checkpoint_save(box + 0x30, box + 0x70);
            }
            sb(m + 0x20, 1);
            if (lh(pv + 0x24) == 2) {
                item_give(4, 0);
                sub_00263d40(TXT(0x2ee6), -1);
                if (lw(pv + 0x160) != -1) {
                    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x160));
                    checkpoint_save(box + 0x30, box + 0x70);
                }
                level_change_request(0, -1);
            }
        }
        break;
    default:
        break;
    }

    float spring_k = u2f(0x3ca3d70au), spring_d = u2f(0x3e99999au);
    bool look_active = false;
    if (lbu(m + 0x53) == 0) {
        look_active = true;
        const uint32_t pos = m + 0x10;
        bool tick_timer = false, at_hero = false;
        if (vec2_distance(V(pos), V(HERO + 0x80)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO + 0xd0), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0xd4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                at_hero = true;
                if (u2f(0x3c23d70au) < vec3_length(V(HERO + 0x100))) sw(pv + 0x184, static_cast<uint32_t>(frame_scale_i(0x78)));
                else tick_timer = true;
            }
        }
        if (tick_timer) timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x184)));
        if (!at_hero && lwu(pv + 0x184) != 0) {
            sw(pv + 0x184, 0);
            lq_sq(pv + 0x170, HERO + 0xd0);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x188))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            sw(pv + 0x188, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + 0x170), 6.0f, yaw, pitch);
            vec3_add(V(pv + 0x170), V(pv + 0x170), V(pos));
        }
        if (lwu(pv + 0x184) != 0) {
            lq_sq(sp, HERO + 0xd0);
            spring_k = u2f(0x3d23d70au);
            spring_d = u2f(0x3e99999au);
        } else {
            lq_sq(sp, pv + 0x170);
        }
    }

    // the two props (+0x164 class 0x4c1, +0x168 class 0x196): spawned and shown outside scenes while 0x13da1c is zero
    if (lw(0x0013da1c) != 0) {
        if (lwu(pv + 0x168) != 0) {
            moby_retire(lwu(pv + 0x168));
            sw(pv + 0x168, 0);
        }
        if (lwu(pv + 0x164) != 0) {
            moby_retire(lwu(pv + 0x164));
            sw(pv + 0x164, 0);
        }
    } else {
        uint32_t c1 = lwu(pv + 0x164);
        if (c1 == 0) {
            c1 = moby_spawn(0x4c1);
            sw(pv + 0x164, c1);
            if (c1 != 0) {
                sh(c1 + 0x32, lhu(m + 0x32));
                sb(lwu(pv + 0x164) + 0x31, 1);
                const uint32_t p1 = lwu(pv + 0x164);
                lq_sq(p1 + 0x10, m + 0x10);
                lq_sq(p1 + 0x40, m + 0x40);
                moby_update_transform(p1);
                sh(lwu(pv + 0x164) + 0x34, 0);
                moby_anim_set(lwu(pv + 0x164), 1, 0, 2);
            }
        } else if (lw(L12_ADDR(0x0015f5c4)) == 2) {
            sh(c1 + 0x34, lhu(c1 + 0x34) | 1u);
            sb(lwu(pv + 0x164) + 0x31, 0);
        } else {
            sh(c1 + 0x34, lhu(c1 + 0x34) & 0xfffeu);
            sb(lwu(pv + 0x164) + 0x31, 1);
        }

        uint32_t c2 = lwu(pv + 0x168);
        if (c2 == 0) {
            c2 = moby_spawn(0x196);
            sw(pv + 0x168, c2);
            if (c2 != 0) {
                sh(c2 + 0x32, lhu(m + 0x32));
                sb(lwu(pv + 0x168) + 0x31, 1);
                const uint32_t p2 = lwu(pv + 0x168);
                lq_sq(p2 + 0x10, m + 0x10);
                lq_sq(p2 + 0x40, m + 0x40);
                moby_update_transform(p2);
            }
        } else if (lw(L12_ADDR(0x0015f5c4)) == 2) {
            const uint32_t p1 = lwu(pv + 0x164);
            sh(p1 + 0x34, lhu(p1 + 0x34) | 1u);
            sb(lwu(pv + 0x164) + 0x31, 0);
            const uint32_t p2 = lwu(pv + 0x168);
            sh(p2 + 0x34, lhu(p2 + 0x34) | 1u);
            sb(lwu(pv + 0x168) + 0x31, 0);
        } else {
            const uint32_t p1 = lwu(pv + 0x164);
            sh(p1 + 0x34, lhu(p1 + 0x34) & 0xfffeu);
            sb(lwu(pv + 0x164) + 0x31, 1);
            const uint32_t p2 = lwu(pv + 0x168);
            sh(p2 + 0x34, lhu(p2 + 0x34) & 0xfffeu);
            sb(lwu(pv + 0x168) + 0x31, 1);
        }
    }

    if (look_active) {
        lq_sq(sp + 0x10, m + 0x10);
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
        const float half_turn = fpu_mul(turn, 0.5f);
        swc1(pv + 0x148, half_turn);
        swc1(pv + 0xc8, half_turn);
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
}

}  // namespace rac1
