// Rilgar's class 918: the hoverboard race's organiser, a talking character of the talk system (talk_init / talk_update on
// its pvar block: +0x20 the name "Race organiser"-like string L5:0x161ed8, +4 / +8 / +0x36 / +0x38 the talk's records).
// Talked to, he walks the hero to a spot 2 in front of him; when the talk reaches its record 3 the race starts: the hero
// put on the start line (L5:0x215bc0, turned as 0x215bd0), the camera's two rows set (0x215be0 / 0x215bf0 into
// 0x141050 / 0x141060), the race's music. The race's end screen's choice (+0xbc: 1 back to him, 2 back to him, 3 the race
// again) puts the hero back beside him (0x215be0 / 0x215bf0) with the level's music, or on the start line again; 1
// without the race won (0x13d388) moves his talk on to its record 4. His state 3 waits for the prize's scene (the scene
// mode 0x15f5c4 2) and, at the talk's record 4, marks the race won, tells the prize (5013, "You won a Platinum
// Zoomerator!") and asks for the level change. Every frame he turns his head to the hero within 8 (or to a random point
// 6 away), counts the crowd's cheer (L5:0x161ed0) from the hero's tricks before him (ril_316810_l5 / ril_316990_l5 shape
// the crowd by it) and moves his idle animations. Hand-ported from Rilgar's overlay (`orig: L5:`); declared in
// rilgar_mobys.h, registered in rilgar_mobys.cpp. The level's data (the spots, the talk's name, the script table, the
// scene record 0x16cc60) are L5_ADDR; the base ELF's (the hero 0x13f350, the camera 0x141050) are bare.
//
// Two forks of the later builds (pal.md of P5.4):
// - 1.02 Fixes, fix 22: the race ended in his state 3 (+0xbc 3) restarts the level's music, as 1 and 2 do; and the skill
//   point earned at the race (0x13d411) is told after the prize ("You just earned a Skill Point!", 21462, through TXT).
//   The PAL 1.00 disc's own code already does both (L5:0x3166c0, 0x316814), the PAL 2.00 disc's too (0x318210,
//   0x318364): taken on any PAL disc (the region flag 0x15ee90, region_core_moved for the 2.00 disc) whatever the row.
// - 1.02 Gameplay, fix 10: talk_update (and the race's start) skipped while the hero is in the state 0x1d, and his input
//   locked (+0x20ac) as the race starts. The PAL 2.00 disc's own code (0x3180a8, 0x318150): taken there whatever the row
//   (region_core_moved), as kalebo_race_organiser.cpp's class 1455; the PAL 1.00 disc does not have it.
#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/globals.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/rilgar_own.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;     // the hero's block (+0x80 his position, +0x100 his velocity, +0x198 his state's frames)
constexpr uint32_t HERO = 0x0013f3d0;  // his position
constexpr uint32_t AIM = 0x0013f420;   // his head (the point characters look at)

// a timer word ticked (timer_tick_i32 on the word, written back): 0 while it runs
int32_t tick(uint32_t a) {
    int32_t t = lw(a);
    const int32_t r = timer_tick_i32(t);
    sw(a, static_cast<uint32_t>(t));
    return r;
}

// the hero on the start line, the camera's rows set for the race
void hero_to_start_line() {
    hero_place(L5_ADDR(0x00215bc0), L5_ADDR(0x00215bd0), 0, 1);
    V(0x00141050) = V(L5_ADDR(0x00215be0));
    V(0x00141060) = V(L5_ADDR(0x00215bf0));
}

// the hero back beside the organiser
void hero_to_organiser() { hero_place(L5_ADDR(0x00215be0), L5_ADDR(0x00215be0) + 0x10, 0, 1); }

}  // namespace

// The crowd's two cheering joints (the moby's joints 2 and 3, the nodes +0x140 / +0x1c0 of the pvar block `pv`): while
// the cheer (L5:0x161ed0) is up, their sizes (+0x1b0 / +0x230: 1 + 0.15 a cheer, at most 3.7) and their sway (+0x1a4 /
// +0x224: 0.61 of a sine over 128 frames) set; each pulled to its pose by a spring.
// orig: L5:0x00316810
void ril_316810_l5(uint32_t m, uint32_t pv) {
    for (uint32_t j = 0; j < 2; ++j) {
        const uint32_t node = pv + j * 0x80;
        const int32_t cheer = lw(L5_ADDR(0x00161ed0));
        if (cheer != 0) {
            float size = fpu_add(fpu_mul(static_cast<float>(cheer), 0.15000000596046448f), 1.0f);
            if (3.700000047683716f < size) size = 3.700000047683716f;
            swc1(node + 0x1b0, size);
            const float t = fpu_mul(static_cast<float>(lw(L5_ADDR(0x0015f5cc)) & 0x7f), 0.0078125f);
            swc1(node + 0x1a4, fpu_mul(sin_vu(fpu_mul(t, 3.1415927410125732f)), 0.6108652353286743f));
        }
        const float dt = lwc1(0x0015ed64);
        joint_node_spring(m, node + 0x140, j + 2, fpu_mul(dt, 0.05000000074505806f), fpu_mul(dt, 0.30000001192092896f));
    }
}

// In the race's scenes (the scene mode 2), with the cheer up (or the option byte 0x15edb0 set): the scene's mobys of the
// organiser's class (the scene record L5:0x16cc60, +0x44 of them at +0x178) given the crowd's joints; with 0x15edb0, the
// scene's node L5:0x17c8a0 first put on such a moby's joint 0 (when it is not set) at the size 2.75.
// orig: L5:0x00316990
void ril_316990_l5(uint32_t m) {
    if (lw(L5_ADDR(0x0015f5c4)) != 2) return;
    if (lbu(0x0015edb0) == 0 && lw(L5_ADDR(0x00161ed0)) == 0) return;
    const uint32_t scene = L5_ADDR(0x0016cc60);
    for (int32_t i = 0; i < lh(scene + 0x44); ++i) {
        const uint32_t o = lwu(scene + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != lh(m + 0xa6)) continue;
        const uint32_t node = L5_ADDR(0x0017c8a0);
        if (lbu(0x0015edb0) != 0 && lbu(node + 1) == 0) {
            sub_0024f590(o, 0, node);
            swc1(node + 0x28, 2.75f);
            swc1(node + 0x20, 2.75f);
            swc1(node + 0x24, 2.75f);
        }
        if (lw(L5_ADDR(0x00161ed0)) != 0) ril_316810_l5(o, lwu(m + 0x78));
    }
}
// orig: L5:0x00316ab8
void ril_class918_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);  // (also his talk record)
    // (1.02 Fixes, fix 22, and 1.02 Gameplay, fix 10: the later builds' own code, see the head of the file)
    const bool fix22 = fixes102_on(22) || region_core_moved() || lw(0x0015ee90) != 0;
    const bool fix10 = gameplay102_on(10) || region_core_moved();
    ril_316990_l5(m);

    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {  // (0x00316b2c) set up: on the floor, his talk (its first record's scene 0xd until both talkers 0x13d6bc /
                    // 0x13d6cc have talked)
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
        swc1(m + 0x18, floor_probe(0.5f, m + 0x10, 0));
        if (lw(0x0013d6bc) == 0 || lw(0x0013d6cc) == 0) {
            sh(lwu(L5_ADDR(0x001b1af8) + static_cast<uint32_t>(talker_index(m)) * 4) + 4, 0xd);
        }
        sw(pv + 0x20, L5_ADDR(0x00161ed8));
        sb(m + 0x20, 1);
        talk_init(m, pv);
        band_set(m);
    } else if (st == 1) {  // (0x00316be0) waiting to be talked to; the race's start
        if (!(fix10 && lw(g::HERO_STATE) == 0x1d)) {
            if (talk_update(m, pv) != 0) {
                hero_walk_to_l1(2.0f, m);
                sb(m + 0x20, 3);
            }
            if (lh(pv + 0x36) == 3) {
                sh(pv + 4, lhu(pv + 0x36));
                sw(pv + 0x38, lwu(L5_ADDR(0x0015f5cc)));
                sh(pv + 0x36, 2);
                sb(m + 0xbc, 0);
                hero_to_start_line();
                if (fix10) sb(P + 0x20ac, 1);  // (1.02 Gameplay, fix 10: the hero's input locked)
                music_track_request(2, 6);
            }
        }
        // the race's end screen's choice
        if (lbu(m + 0xbc) == 1) {
            hero_to_organiser();
            music_track_request(0, 8);
            if (lbu(0x0013d388) != 0) {
                sb(m + 0xbc, 0);
            } else {  // (the race not won: his talk's next record, 4)
                const uint32_t frame = lwu(L5_ADDR(0x0015f5cc));
                const uint32_t record = lhu(pv + 0x36);
                sh(pv + 0x36, 4);
                sh(pv + 4, record);
                sw(pv + 0x38, frame);
                sb(pv + 8, 1);
                sb(m + 0xbc, 0);
            }
        }
        if (lbu(m + 0xbc) == 2) {
            music_track_request(0, 8);
            hero_to_organiser();
            sb(m + 0xbc, 0);
        }
        if (lbu(m + 0xbc) == 3) {
            if (fix22) music_track_request(0, 8);  // (1.02 Fixes, fix 22: the level's music)
            sb(m + 0xbc, 0);
            hero_to_start_line();
        }
        // his idle animations: from 0 or 2 now and then (20 to 40 seconds) to 1; at an animation's end 0 or 2 at random
        const uint32_t anim = lbu(m + 0x53);
        if ((anim == 0 || anim == 2) && tick(pv + 0x244) != 0) {
            sw(pv + 0x244, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(1200.0f, 2400.0f)))));
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        } else if ((lbu(m + 0x70) & 2) != 0) {
            const uint32_t next = rand_mod(2) == 0 ? 2u : 0u;
            if (lbu(m + 0x53) != next) moby_anim_set(m, next, 0, frame_scale_i(0xa));
        }
    } else if (st == 3) {  // (0x00316e3c) the prize's scene: at its end back to 1; at the talk's record 4 the prize
        if (lw(L5_ADDR(0x0015f5c4)) != 2) {
            sb(m + 0x20, 1);
            if (lh(pv + 4) == 4) {
                sb(0x0013d388, 1);
                sub_00263d40(TXT(0x1395), frame_scale_i(0x12c));
                if (fix22 && lbu(0x0013d411) != 0) {  // (1.02 Fixes, fix 22: the skill point told)
                    sound_play_indexed(1, 0, 0);
                    sub_00263d40(TXT(0x53d6), -1);
                }
                level_change_request(0, -1);
            }
        }
    } else if (lbu(m + 0xbc) != 0) {  // (0x00316e8c) 2 and above: a choice of the end screen puts the hero back beside him
        hero_to_organiser();
        sb(m + 0xbc, 0);
    }

    // ---- his head: in his animations 0 and 2, toward the hero's head while the hero is within 8 before him (kept for 2
    // seconds once the hero stands still), else toward a point 6 away picked every 3 to 5 seconds
    float k = 0.019999999552965164f, damping = 0.30000001192092896f;
    const uint32_t anim = lbu(m + 0x53);
    if (anim == 0 || anim == 2) {
        bool facing = false;
        if (vec2_distance(V(m + 0x10), V(HERO)) < 8.0f) {
            const float to_hero = atan2_f(fpu_sub(lwc1(AIM), lwc1(m + 0x10)), fpu_sub(lwc1(AIM + 4), lwc1(m + 0x14)));
            facing = angle_dist(lwc1(m + 0x48), to_hero) < 1.5707963705062866f;
        }
        if (facing) {
            if (0.009999999776482582f < vec3_length(V(P + 0x100))) sw(pv + 0x248, static_cast<uint32_t>(frame_scale_i(0x78)));
            else tick(pv + 0x248);
        } else if (lw(pv + 0x248) != 0) {
            sw(pv + 0x248, 0);
            V(pv + 0x250) = V(AIM);
        }
        if (tick(pv + 0x24c) != 0) {
            sw(pv + 0x24c, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), 0.01745329238474369f));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), 0.01745329238474369f);
            vec_from_spherical_v(V(pv + 0x250), 6.0f, yaw, pitch);
            vec3_add(V(pv + 0x250), V(pv + 0x250), V(m + 0x10));
        }
        Vec4 target;
        if (lw(pv + 0x248) != 0) {
            target = V(AIM);
            k = 0.029999999329447746f;
            damping = 0.30000001192092896f;
        } else {
            target = V(pv + 0x250);
        }
        Vec4 head = V(m + 0x10);
        head.z = fpu_add(head.z, 1.0f);
        Vec4 d{};
        vec3_sub(d, target, head);
        float yaw = angle_sub(atan2_f(d.x, d.y), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(d), d.z);
        if (1.5707963705062866f < yaw) yaw = 1.5707963705062866f;
        else if (yaw < -1.5707963705062866f) yaw = -1.5707963705062866f;
        if (0.5235987901687622f < pitch) pitch = 0.5235987901687622f;
        else if (pitch < -0.5235987901687622f) pitch = -0.5235987901687622f;
        swc1(pv + 0xa4, pitch);
        swc1(pv + 0xa8, fpu_mul(yaw, 0.6000000238418579f));
        swc1(pv + 0x128, fpu_mul(yaw, 0.4000000059604645f));
    }

    // ---- the cheer: the hero within 15 before him doing a trick (state 0xb at its sub-state 0xf) raises it (at most 20),
    // standing (state 4) for a third of a second lowers it every tenth frame
    if (vec2_distance(V(m + 0x10), V(HERO)) < 15.0f) {
        const float to_hero = atan2_f(fpu_sub(lwc1(P + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(P + 0x84), lwc1(m + 0x14)));
        if (angle_dist(lwc1(m + 0x48), to_hero) < 1.2217304706573486f) {
            if (lw(P + 0x2084) == 0xb && lw(P + 0x198) == 0xf) {
                const int32_t cheer = lw(pv + 0x260) + 1;
                sw(pv + 0x260, static_cast<uint32_t>(cheer < 21 ? cheer : 20));
                sw(L5_ADDR(0x00161ed0), lwu(pv + 0x260));
            }
            if (lw(P + 0x2084) == 4 && frame_scale_i(0x14) < lw(P + 0x198) && ee_rem(lw(L5_ADDR(0x0015f5cc)), 10) == 0) {
                const int32_t cheer = lw(pv + 0x260) - 1;
                sw(pv + 0x260, static_cast<uint32_t>(cheer < 0 ? 0 : cheer));
                sw(L5_ADDR(0x00161ed0), lwu(pv + 0x260));
            }
        }
    }

    // ---- outside the scenes: the crowd's joints and his head's two joints pulled by their springs
    if (lw(L5_ADDR(0x0015f5c4)) == 2) return;
    ril_316810_l5(m, pv);
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xb0, 2.75f);
    float dt = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x40, 0, fpu_mul(k, dt), fpu_mul(damping, dt));
    dt = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xc0, 1, fpu_mul(k, dt), fpu_mul(damping, dt));
}

}  // namespace rac1
