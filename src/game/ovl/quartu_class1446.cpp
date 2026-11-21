// Quartu (level 15) class 1446, ported by hand from the level's own code: the level's talking character of the Giant Clank
// part. Its linked moby (the pvars +0x14c; -1: the character does nothing at all) is hidden while the hero is not Giant
// Clank; the character itself is hidden and still in a scene (0x15f5c4 == 2) and once done (state 9), else solid with its
// band (band_set). Its states (the table L15:0x1e35c0):
//   0 set up: its talk (talk_init on the pvars), the hero's range 20; its checkpoint (+0x280) collected already: done (9,
//     with 0x13dd50) or straight to 3, the level's other mobys of its checkpoint retired
//   1 the hero in its box (+0x27c) and free to talk (his substate +0x208c below 2 or 9): its talk; once over the checkpoint
//     marked, the other mobys of its checkpoint retired, to 2
//   2 after the scene: its checkpoint saved at its box +0x150 (unless +4 says "done": 9), to 3
//   3 the hero in his movement mode 2 (Giant Clank): its moby +0x158 retired; both its groups (+0x144, +0x148) gone: to 4,
//     a 2-second timer (+0x15c)
//   4 the timer out: the hero's mode left, its linked moby's animation 0, the level marked visited (0x10), the checkpoint
//     +0x284 marked, the music, the hero placed at the box +0x154 and the checkpoint saved there, to 5
//   5 scene 1, to 6; 6 the scene over: movie 0x10, to 7; 7 the movie over: the arrival message and scene 2 (unless in a
//     scene), then 8: the level change request, to 9
// Then, every frame (but state 9), its head turns toward a look point (joint_node_spring on its two nodes +0x160 / +0x1e0):
// the hero's head (0x13f420) while he is within 8 in front of it and moving (two seconds after he stops), else a point it
// picks at random around itself every 3 to 5 seconds. The original's order of calls and float operations is kept. Header
// quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

using namespace quartu;

namespace {

constexpr float HALF_PI = 1.5707963705062866f;  // 0x3fc90fdb
constexpr float DEG = 0.01745329238474369f;     // 0x3c8efa35: one degree in radians
constexpr uint32_t HERO_HEAD = 0x0013f420;      // the player block's +0xd0
constexpr uint32_t HERO_SPEED = 0x0013f450;     // the player block's +0x100

bool in_scene() { return lw(L15_ADDR(0x0015f5c4u)) == 2; }

// the level's other live mobys of its checkpoint (+0x280: their +0xb0) retired
void retire_checkpoint_mates(uint32_t m, uint32_t pv) {
    for (uint32_t o = lwu(L15_ADDR(0x0015ffe4u)); o != 0; o = lwu(o + 0x28)) {
        if (lbu(o + 0xb0) != lwu(pv + 0x280) || o == m) continue;
        const uint32_t st = lbu(o + 0x20);
        if (st == 0xfe || st == 0xfd) continue;
        moby_retire(o);
    }
}

}  // namespace

// orig: L15:0x002ec760
void qua_class1446_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 1);  // (the level's copy 0x2ec678: joint 1)
    if (lw(pv + 0x14c) == -1) return;
    if (lbu(HERO_FORM) != 2) {  // its linked moby hidden while the hero is not Giant Clank
        sw(lwu(OV(0x0015ffd8)) + (lwu(pv + 0x14c) << 8) + 0x94, 0);
        const uint32_t link = lwu(OV(0x0015ffd8)) + (lwu(pv + 0x14c) << 8);
        sh(link + 0x34, lhu(link + 0x34) | 0x41);
    }
    if (lbu(m + 0x20) == 9) {  // done
        sb(m + 0x30, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41);
        sw(m + 0x94, 0);
        return;
    }
    if (in_scene()) {
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41);
    } else {
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
        band_set(m);
    }

    switch (lbu(m + 0x20)) {
    case 0:  // (0x002ec88c)
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        swc1(pv + 0xc, 20.0f);
        talk_init(m, pv);
        if (lbu(0x0014c050 + lwu(pv + 0x280) + (lwu(0x0015ed84) << 4)) != 0xff) break;
        if (lbu(0x0013dd50) != 0) {
            sb(m + 0x20, 9);
            break;
        }
        sb(m + 0x20, 3);
        retire_checkpoint_mates(m, pv);
        break;
    case 1: {  // (0x002ec96c)
        if (point_in_box(HERO_POS, lw(pv + 0x27c)) == 0) break;
        const uint32_t free = lwu(HERO + 0x208c);
        if (!(free < 2 || free == 9)) break;
        if (talk_update(m, pv) == 0) break;
        checkpoint_mark_collected(lwu(pv + 0x280));
        sw(m + 0x58, 0);
        sb(m + 0x20, 2);
        retire_checkpoint_mates(m, pv);
        break;
    }
    case 2:  // (0x002eca4c)
        if (in_scene()) break;
        swc1(m + 0x58, 1.0f);
        if (lh(pv + 0x4) != 0) {
            sb(m + 0x20, 9);
            break;
        }
        if (const int32_t box = lw(pv + 0x150); box != -1) {
            const uint32_t b = lvd::box(lvd::BOXES, box);
            checkpoint_save(b + 0x30, b + 0x70);
        }
        sb(m + 0x20, 3);
        break;
    case 3: {  // (0x002ecac0)
        if (lbu(HERO + 0x20a4) != 2) break;
        if (const int32_t k = lw(pv + 0x158); k != -1) {
            const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(k) << 8);
            if (o != 0 && lbu(o + 0x20) != 0xfe && lbu(o + 0x20) != 0xfd) {
                moby_retire(o);
                sw(pv + 0x158, 0xffffffffu);
            }
        }
        if (moby_group_count_l1(lw(pv + 0x144), 0x40) != 0) break;
        if (moby_group_count_l1(lw(pv + 0x148), 0x40) != 0) break;
        sb(m + 0x20, 4);
        sw(pv + 0x15c, static_cast<uint32_t>(frame_scale_i(0x78)));
        break;
    }
    case 4:  // (0x002ecb6c)
        if (tick32(pv + 0x15c) == 0) break;
        hero_mode_exit();
        moby_set_anim(lwu(OV(0x0015ffd8)) + (lwu(pv + 0x14c) << 8), 0, 0);
        sh(pv + 0x36, 1);
        sb(pv + 0x8, 1);
        sw(pv + 0x140, 1);
        level_mark_visited(0x10);
        checkpoint_mark_collected(lwu(pv + 0x284));
        music_track_request(0, 5);
        if (lw(pv + 0x154) != -1) {
            const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0x154));
            hero_place(b + 0x30, b + 0x70, 0, 1);
            const uint32_t b2 = lvd::box(lvd::BOXES, lw(pv + 0x154));
            checkpoint_save(b2 + 0x30, b2 + 0x70);
        }
        sb(m + 0x20, 5);
        break;
    case 5:  // (0x002ecc38)
        cutscene_start(1);
        sb(m + 0x20, 6);
        break;
    case 6:  // (0x002ecc58)
        if (in_scene()) break;
        movie_play(0x10);
        sb(m + 0x20, 7);
        break;
    case 7:  // (0x002ecc8c)
        if (!in_scene()) {
            level_arrival_message(0x10);
            cutscene_start(2);
            sb(m + 0x20, 8);
        }
        level_change_request(0, -1);
        sb(m + 0x20, 9);
        break;
    case 8:  // (0x002eccb8)
        level_change_request(0, -1);
        sb(m + 0x20, 9);
        break;
    default:  // (10 and above)
        break;
    }

    // ---- the head's look point: the hero's head while he is near, in front and moving (two seconds after he stops)
    float stiff = u2f(0x3ca3d70a), damp = u2f(0x3e99999a);  // (0.02, 0.3)
    bool timer_left = false;  // (the hero is not in front: the look timer +0x274 read as it is)
    if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
        const float to_hero = atan2_f(fpu_sub(lwc1(HERO + 0xd0), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0xd4), lwc1(m + 0x14)));
        if (angle_dist(lwc1(m + 0x48), to_hero) < HALF_PI) {
            if (u2f(0x3c23d70a) < vec3_length(V(HERO_SPEED))) sw(pv + 0x274, static_cast<uint32_t>(frame_scale_i(0x78)));  // (0.01)
            else tick32(pv + 0x274);
        } else {
            timer_left = true;
        }
    } else {
        timer_left = true;
    }
    if (timer_left && lwu(pv + 0x274) != 0) {
        sw(pv + 0x274, 0);
        V(pv + 0x260) = V(HERO_HEAD);
    }
    // a new point around itself every 3 to 5 seconds: 6 away, up to 90 degrees to each side, up to 30 degrees up
    if (tick32(pv + 0x278) != 0) {
        sw(pv + 0x278, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
        const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), DEG));
        const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), DEG);
        vec_from_spherical_v(V(pv + 0x260), 6.0f, yaw, pitch);
        vec3_add(V(pv + 0x260), V(pv + 0x260), V(m + 0x10));
    }
    Vec4 look;
    if (lwu(pv + 0x274) != 0) {
        look = V(HERO_HEAD);
        stiff = u2f(0x3d23d70a);  // (0.04)
        damp = u2f(0x3e99999a);
    } else {
        look = V(pv + 0x260);
    }
    Vec4 eye = V(m + 0x10);
    eye.z = fpu_add(eye.z, 1.0f);
    Vec4 to;
    vec3_sub(to, look, eye);
    float yaw = angle_sub(atan2_f(to.x, to.y), lwc1(m + 0x48));
    float pitch = -atan2_f(vec2_length(to), to.z);
    if (HALF_PI < yaw) yaw = HALF_PI;
    else if (yaw < -HALF_PI) yaw = -HALF_PI;
    if (u2f(0x3f060a92) < pitch) pitch = u2f(0x3f060a92);  // (30 degrees)
    else if (pitch < u2f(0xbf060a92)) pitch = u2f(0xbf060a92);
    const float half_yaw = fpu_mul(yaw, 0.5f);
    swc1(pv + 0x248, half_yaw);
    swc1(pv + 0x1c4, fpu_mul(pitch, 1.25f));
    swc1(pv + 0x1c8, half_yaw);
    if (lbu(0x0015edb0) != 0) swc1(pv + 0x1d0, 2.75f);
    joint_node_spring(m, pv + 0x160, 1, fpu_mul(stiff, lwc1(0x0015ed64)), fpu_mul(damp, lwc1(0x0015ed64)));
    joint_node_spring(m, pv + 0x1e0, 0, fpu_mul(stiff, lwc1(0x0015ed64)), fpu_mul(damp, lwc1(0x0015ed64)));
}

}  // namespace rac1
