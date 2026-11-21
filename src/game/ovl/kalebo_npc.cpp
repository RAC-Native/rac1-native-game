// Kalebo III (level 16), class 1377: a character Ratchet talks to (the talk record at pvars +0x20). State 0 sets the talk up
// (reach 20) and waits (1); in 1, with Ratchet free (his halfword +0x30e clear, not in the sub-states 0xf / 0x16) inside
// the box of pvars +0x160, the talk runs, and once it is over (2) and the scene has ended (the level's mode word 0x15f5c4
// no longer 2) Ratchet is put at the level's spot 0x1d96c0, the help message 0x3e82 shown, item 0x21 given, the
// checkpoint saved at him and the level change asked for (3). In every state the level's flags 0x13d3f8 (Ratchet once
// in the box of pvars +0x164) and 0x13d3f9 (once in his sub-state 0xf) are set, and while its first animation plays its
// head follows Ratchet's look point within 8 (or a random point every 3 .. 5 seconds), the joint nodes at pvars +0x60
// (joint 0) and +0xe0 (joint 2) springing to it. Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare.
// Registered in kalebo_mobys.cpp; checked by batch 250 (class1377, the P16.6 checks).
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>

#include <game/ovl/variants_l16.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;

constexpr uint32_t HERO = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t HERO_LOOK = 0x0013f420u;  // (the hero's look point, +0xd0)
constexpr uint32_t HERO_VEL = 0x0013f450u;

// The head's look: while animation 0 plays, Ratchet's look point when he is within 8 and less than a quarter turn off its
// yaw (held for 120 frames once he moves), else the wandering point pvars +0x170 (a new one 6 away, up to 90 degrees off
// its yaw and 30 up, every 180 .. 300 scaled frames: the timer +0x16c); the yaw (within a quarter turn) and the pitch
// (within 30 degrees) of that point from 1.5 above its place go to the nodes (pvars +0xc4 the pitch, +0xc8 / +0x148 the
// yaw x 0.7 / 0.3). Then the big-head scale 2.75 (0x15edb0) and both nodes' springs, faster while it follows Ratchet.
void npc_head_look(uint32_t m, uint32_t pv) {
    float k = u2f(0x3ca3d70au), damp = u2f(0x3e99999au);  // (0.02, 0.3)
    if (lbu(m + 0x53) == 0) {
        const Frame f(0xb0);
        const uint32_t target = f.sp, from = f.sp + 0x10, d = f.sp + 0x20;
        const uint32_t track = pv + 0x168, wait = pv + 0x16c, wander = pv + 0x170;
        bool lost = true;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO_LOOK), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_LOOK + 4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                lost = false;
                if (u2f(0x3c23d70au) < vec3_length(V(HERO_VEL))) sw(track, static_cast<uint32_t>(frame_scale_i(0x78)));
                else kalebo::tick32(track);
            }
        }
        if (lost && lw(track) != 0) {
            sw(track, 0);
            lq_sq(wander, HERO_LOOK);
        }
        if (kalebo::tick32(wait) != 0) {
            const float deg = u2f(0x3c8efa35u);
            sw(wait, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float tilt = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(wander), 6.0f, yaw, tilt);
            vec3_add(V(wander), V(wander), V(m + 0x10));
        }
        if (lw(track) != 0) {
            lq_sq(target, HERO_LOOK);
            k = u2f(0x3d23d70au);  // (0.04)
        } else {
            lq_sq(target, wander);
        }
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), 1.5f));
        vec3_sub(V(d), V(target), V(from));
        float yaw = angle_sub(atan2_f(lwc1(d), lwc1(d + 4)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(d)), lwc1(d + 8));
        if (u2f(0x3fc90fdbu) < yaw) yaw = u2f(0x3fc90fdbu);
        else if (yaw < u2f(0xbfc90fdbu)) yaw = u2f(0xbfc90fdbu);
        if (u2f(0x3f060a92u) < pitch) pitch = u2f(0x3f060a92u);
        else if (pitch < u2f(0xbf060a92u)) pitch = u2f(0xbf060a92u);
        swc1(pv + 0xc4, pitch);
        swc1(pv + 0xc8, fpu_mul(yaw, u2f(0x3f333333u)));   // (0.7)
        swc1(pv + 0x148, fpu_mul(yaw, u2f(0x3e99999au)));  // (0.3)
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xd0, 2.75f);
    float dt = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x60, 0, fpu_mul(k, dt), fpu_mul(damp, dt));
    dt = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xe0, 2, fpu_mul(k, dt), fpu_mul(damp, dt));
}
}  // namespace

// orig: L16:0x002e3190
void kal_class1377_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t talk = pv + 0x20;
    class_mates_mark_fixed(m, 1);
    switch (lbu(m + 0x20)) {  // (the original's compare chain)
    case 0:  // (0x002e3204)
        talk_init(m, talk);
        sb(m + 0x20, 1);
        swc1(pv + 0x2c, 20.0f);
        sb(m + 0x30, 0xff);
        break;
    case 1: {  // (0x002e322c)
        if (lh(HERO + 0x30e) != 0) break;
        const uint32_t sub = lwu(0x001413dc);
        if (sub == 0xf || sub == 0x16) break;
        if (point_in_box(HERO_POS, lw(pv + 0x160)) != 0 && talk_update(m, talk) != 0) sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002e3284)
        if (lw(L16_ADDR(0x0015f5c4)) == 2) break;  // (the talk's scene still playing)
        sb(m + 0x20, 1);
        const uint32_t spot = L16_ADDR(0x001d96c0);  // (a place +0 and a turn +0x10)
        hero_place(spot, spot + 0x10, 0, 1);
        sub_00203908(TXT(0x3e82), 0x84);
        item_give(0x21, 1);
        checkpoint_mark_collected(lbu(m + 0xb0));
        checkpoint_save(HERO_POS, HERO_POS + 0x10);
        level_change_request(0, -1);
        sb(m + 0x20, 3);
        break;
    }
    default:  // (0x002e32e8)
        break;
    }

    if (lbu(0x0013d3f8) == 0 && point_in_box(HERO_POS, lw(pv + 0x164)) != 0) sb(0x0013d3f8, 1);
    if (lbu(0x0013d3f9) == 0 && lw(0x001413dc) == 0xf) sb(0x0013d3f9, 1);
    npc_head_look(m, pv);
}

}  // namespace rac1
