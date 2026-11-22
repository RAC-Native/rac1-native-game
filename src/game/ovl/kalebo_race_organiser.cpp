// Kalebo III's (level 16) class 1455: the hoverbike race's organiser (the Gadgetron CEO's model), a talking character of the talk system (talk_init /
// talk_update on its pvars +0x20). Talked to, he walks the hero to his spot and starts the race's scene; when the scene
// is played (the talk record's +0x36 back to 3) the hero is put on the start line, the course is set up (its groups
// shown, the death height and the sea lowered, the race's music) and the organiser waits in state 3 for the race's end:
// a score of 4,500 or more earns the skill point (0x13d422) and is kept as the best one (0x15ee6c); the race over
// (the hero's sub-state 0x1413dc no longer 0x16), the course is put away; the race's end screen's choice (+0xbc: 1, 2, 3)
// puts the hero back beside him or on the start line again. Every state turns his head to the hero within 8 (two joint
// springs, +0xe0 / +0x160). His five class hooks (the 749-type table 0x1e9050) are at the end of this file. Level 16
// only: `orig: L16:`, level data L16_ADDR, base ELF bare (the PAL 2.00 core is copied to the NTSC-U addresses).
// Registered in kalebo_mobys.cpp / carry_hooks_levels.cpp; checked by batch 250 (class1455, class1455_cases, the
// helpers' checks, class1455_fix10, the hooks) and batch 371 (the race's dumps).
//
// 1.02 Gameplay, fix 10 (the PAL 2.00 disc's own build of this function: L16:0x2e7dc0 / 0x2e7f70): talk_update not run
// while the hero is in the state 0x1d, and the hero's input locked (+0x20ac) as the race's scene ends; always taken on
// the PAL 2.00 disc (region_core_moved), as gemlik_ship.cpp's class 69 does.
#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/globals.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
using kalebo::Frame;
using kalebo::tick32;

constexpr uint32_t kHeroPos = 0x0013f3d0;     // the hero's position (PLAYER + 0x80)
constexpr uint32_t kHeroTarget = 0x0013f420;  // the point the hero looks at (PLAYER + 0xd0)
constexpr uint32_t kHeroVel = 0x0013f450;     // his velocity (PLAYER + 0x100)
constexpr uint32_t kScore = 0x0013fbf8;       // the race's score (PLAYER + 0x8a8)
constexpr uint32_t kHelpTime = 0x00141da2;    // the race help message's last time (in 600-frame units), a halfword
constexpr int32_t kSkillScore = 4500;         // the skill point's score

// the hero on the start line (L16 0x1d9840 / 0x1d9850: position, rotation) with the camera's two rows set behind him
// (L16 0x1d9860 / 0x1d9870 to the camera block 0x141050 / 0x141060)
void hero_to_start_line() {
    hero_place(L16_ADDR(0x001d9840), L16_ADDR(0x001d9850), 0, 1);
    lq_sq(0x00141050, L16_ADDR(0x001d9860));
    lq_sq(0x00141060, L16_ADDR(0x001d9870));
}

// the hero put back beside the organiser (L16 0x1d9860 / 0x1d9870: position, rotation)
void hero_to_organiser() { hero_place(L16_ADDR(0x001d9860), L16_ADDR(0x001d9870), 0, 1); }

// the race's help message (0x3e83) when the hero comes within 3.5 of the organiser after 18 seconds (1,080 frames) or
// more away: while he stays, the time is kept up to date at kHelpTime (the game's frame count 0x15eea4 in 600-frame units)
void race_help(uint32_t m) {
    if (lbu(0x0013d4de) != 0) return;
    if (!(vec2_distance(V(kHeroPos), V(m + 0x10)) < 3.5f)) return;
    const int32_t now = frame_scale_i(lw(0x0015eea4));
    const int32_t since = now - static_cast<int32_t>(lhu(kHelpTime)) * 0x258;
    const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f))));
    if (limit < since || static_cast<int32_t>(lhu(kHelpTime)) * 0x258 == 0) {
        sub_00203908(TXT(0x3e83), 0x87);
        return;
    }
    const int32_t units = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
    if (static_cast<int32_t>(lhu(kHelpTime)) < units) sh(kHelpTime, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
}

// the head turned toward the hero: within 8 and in front (90 degrees), his target point while he moves (and for 120
// frames after); else a point 6 away picked at random every 180 .. 300 frames (+0xcc), the hero's target taken once
// when he leaves (+0xc8). The joint springs' yaw / pitch (+0x144, +0x148, +0x1c8) and their rates.
void look_at_hero(uint32_t m, uint32_t pv) {
    const Frame f(0xd0);
    const uint32_t look = f.sp, head = f.sp + 0x10, d = f.sp + 0x20;
    float k = u2f(0x3ca3d70au), damp = u2f(0x3e99999au);  // (0.02, 0.3)
    if (lbu(m + 0x53) == 0) {
        bool timer_running = false;  // (+0xc8 set or ticked this frame: its zero test skipped)
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 8.0f) {
            const float a = atan2_f(fpu_sub(lwc1(kHeroTarget), lwc1(m + 0x10)), fpu_sub(lwc1(kHeroTarget + 4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), a) < u2f(0x3fc90fdbu)) {
                if (0.01f < vec3_length(V(kHeroVel))) sw(pv + 0xc8, static_cast<uint32_t>(frame_scale_i(0x78)));
                else tick32(pv + 0xc8);
                timer_running = true;
            }
        }
        if (!timer_running && lw(pv + 0xc8) != 0) {
            sw(pv + 0xc8, 0);
            lq_sq(pv + 0xd0, kHeroTarget);
        }
        if (tick32(pv + 0xcc) != 0) {  // (idle or expired: a new point)
            const float deg = u2f(0x3c8efa35u);
            sw(pv + 0xcc, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + 0xd0), 6.0f, yaw, pitch);
            vec3_add(V(pv + 0xd0), V(pv + 0xd0), V(m + 0x10));
        }
        if (lw(pv + 0xc8) != 0) {
            lq_sq(look, kHeroTarget);
            k = u2f(0x3d23d70au);  // (0.04)
            damp = u2f(0x3e99999au);
        } else {
            lq_sq(look, pv + 0xd0);
        }
        lq_sq(head, m + 0x10);
        swc1(head + 8, fpu_add(lwc1(head + 8), 1.0f));
        vec3_sub(V(d), V(look), V(head));
        float yaw = angle_sub(atan2_f(lwc1(d), lwc1(d + 4)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(d)), lwc1(d + 8));
        const float right = u2f(0x3fc90fdbu), tilt = u2f(0x3f060a92u);  // (90, 30 degrees)
        if (right < yaw) yaw = right;
        else if (yaw < -right) yaw = -right;
        if (tilt < pitch) pitch = tilt;
        else if (pitch < -tilt) pitch = -tilt;
        swc1(pv + 0x144, pitch);
        swc1(pv + 0x148, fpu_mul(yaw, u2f(0x3f333333u)));
        swc1(pv + 0x1c8, fpu_mul(yaw, u2f(0x3e99999au)));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0x150, 2.75f);
    const float dt = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xe0, 0, fpu_mul(k, dt), fpu_mul(damp, dt));
    const float dt2 = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x160, 1, fpu_mul(k, dt2), fpu_mul(damp, dt2));
}
}  // namespace

// The open sea's height (L16 0x161c74, read by class 1111) set to `z`.
// orig: L16:0x002de3b8
void kal_sea_height_set_l16(float z) { swc1(L16_ADDR(0x00161c74), z); }

// The race's course set up: the 16 groups of the organiser's pvars +0x60 shown (far, shown, solid), the 8 of +0xa0
// hidden; the death height (L16 0x15f638) 74, the sea at 76; the race's music (track 2, then 6).
// orig: L16:0x002e70c0
void kal_race_course_on_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 16; ++i) group_set_flags(lw(pv + 0x60 + i * 4), 1, 1, 1);
    for (uint32_t i = 0; i < 8; ++i) group_set_flags(lw(pv + 0xa0 + i * 4), 0, 0, 0);
    swc1(L16_ADDR(0x0015f638), 74.0f);
    kal_sea_height_set_l16(76.0f);
    music_track_request(2, 6);
}

// The race's course put away: the 16 groups of +0x60 hidden, the 8 of +0xa0 shown but not solid; the sea back at 100;
// the level's music (track 0, through 8).
// orig: L16:0x002e7168
void kal_race_course_off_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 16; ++i) group_set_flags(lw(pv + 0x60 + i * 4), 0, 0, 0);
    for (uint32_t i = 0; i < 8; ++i) group_set_flags(lw(pv + 0xa0 + i * 4), 1, 1, 0);
    kal_sea_height_set_l16(100.0f);
    music_track_request(0, 8);
}

// orig: L16:0x002e6808
void kal_class1455_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t talk = pv + 0x20;  // the talk record
    const bool fix10 = gameplay102_on(10) || region_core_moved();  // (1.02 Gameplay, fix 10: the PAL 2.00 disc's own code)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L16_ADDR(0x001671c0))) < 28.0f) {  // (the camera within 28)
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    class_mates_mark_fixed(m, 0);
    const uint32_t st = lbu(m + 0x20);
    switch (st < 5 ? st : 3u) {  // (jump table L16 0x1e8870; a state of 5 or more runs state 3's code)
    case 0: {  // (0x002e68bc) set-up: the talk, its reach 3.7, his facing kept (+0xc4), turned round unless 0x13dc1c
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        talk_init(m, talk);
        swc1(talk + 0xc, u2f(0x406ccccdu));
        swc1(pv + 0xc4, lwc1(m + 0x48));
        if (lw(0x0013dc1c) == 0) swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
        kal_race_course_off_l16(m);
        break;
    }
    case 1: {  // (0x002e6928) waiting to be talked to
        swc1(L16_ADDR(0x0015f638), lw(g::HERO_SUBSTATE) == 0xf ? 100.0f : 115.0f);  // (the death height)
        if (!(fix10 && lw(g::HERO_STATE) == 0x1d) && talk_update(m, talk) != 0) {
            swc1(m + 0x48, lwc1(pv + 0xc4));
            hero_walk_to_l1(3.5f, m);  // (the hero walked to 3.5 in front of him: the level's copy L16:0x25e948 of the shared walk-to)
            sb(m + 0x20, 2);
        }
        race_help(m);
        if (lh(talk + 0x36) == 3) {  // (the race's scene played: the race starts)
            sh(talk + 4, lhu(talk + 0x36));
            sh(talk + 0x36, 2);
            sw(talk + 0x38, lwu(L16_ADDR(0x0015f5cc)));
            sb(m + 0xbc, 0);
            hero_to_start_line();
            if (fix10) sb(g::PLAYER + 0x20ac, 1);  // (1.02 Gameplay, fix 10: the hero's input locked)
            sb(m + 0x20, 3);
            kal_race_course_on_l16(m);
        }
        if (lw(g::HERO_SUBSTATE) == 0x16) {  // (racing)
            sb(m + 0x20, 3);
            kal_race_course_on_l16(m);
        }
        break;
    }
    case 2: {  // (0x002e6b2c) his scene playing (the scene mode 0x15f5c4 = 2); then back to 1, or the race's movie
        if (lw(L16_ADDR(0x0015f5c4)) == 2) break;
        sb(m + 0x20, 1);
        if (lh(talk + 4) == 4) {
            level_movie_play(0);
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x002e6b64) after the movie: the prize (item 0x1f) and the level change
        if (lw(L16_ADDR(0x0015f5c4)) != 0) break;
        sb(m + 0x20, 1);
        item_give(0x1f, 1);
        level_change_request(0, -1);
        break;
    }
    default: {  // 3 (0x002e6b9c) the race on
        if (lw(kScore) >= kSkillScore) {
            if (lbu(0x0013d422) == 0) {
                sb(0x0013d422, 1);
                sound_play_indexed(1, 0, 0);
                sub_00263d40(TXT(0x53d6), -1);  // ("You just earned a Skill Point!")
            }
            if (lw(0x0015ee6c) < kSkillScore) sw(0x0015ee6c, lwu(kScore));
        }
        if (lw(g::HERO_SUBSTATE) == 0) {  // (the race over)
            sb(m + 0x20, 1);
            kal_race_course_off_l16(m);
        }
        if (lbu(m + 0xbc) == 1) {
            sb(m + 0x20, 1);
            hero_to_organiser();
            sb(m + 0xbc, 0);
            if (lbu(0x0013d507) == 0) {  // (his talk's next record: 4)
                sb(talk + 8, 1);
                sh(talk + 4, lhu(talk + 0x36));
                sh(talk + 0x36, 4);
                sw(talk + 0x38, lwu(L16_ADDR(0x0015f5cc)));
            }
            kal_race_course_off_l16(m);
        }
        if (lbu(m + 0xbc) == 2) {
            sb(m + 0x20, 1);
            hero_to_organiser();
            sb(m + 0xbc, 0);
            kal_race_course_off_l16(m);
        }
        if (lbu(m + 0xbc) == 3) {
            sb(m + 0xbc, 0);
            hero_to_start_line();
        }
        break;
    }
    }
    look_at_hero(m, pv);
}

// Class hook +0xc of the table: the carry hook 0xc, the moby in state 7 whatever it answers. Its answer. (The same code as
// Gaspar's gas_hook_2e2770_l9.)
// orig: L16:0x002e66c0
int32_t kal_hook_2e66c0_l16(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    sb(m + 0x20, 7);
    return r;
}

// Class hook +0x10: the moby's pvars +0x60 (0 without the moby or its pvars). (The same code as Gaspar's
// gas_hook_2e27a0_l9.)
// orig: L16:0x002e66f0
uint32_t kal_hook_2e66f0_l16(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

}  // namespace rac1
