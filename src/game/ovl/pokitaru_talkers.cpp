// Pokitaru's (level 11) talkers, ported by hand from the level's own code: class 90, Bob (Big Al's brother: his talk
// gives the Thruster-Pack, then the level change), and class 298 (a talker on the beach: a skill point's timer while
// the hero stands by, his talk sets the level's flags 0x13d4e3 / 0x13d50b, a hint and the level change). Both turn
// their heads toward the hero (or a point they look at) with the same code at their own pvar offsets.
// The original's order of calls and float operations kept.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l11.h>
#include <game/level/level_data.h>

namespace rac1 {

namespace {
constexpr uint32_t kHeroPos = 0x0013f3d0;    // the hero's position (the hero's block 0x13f350 + 0x80)
constexpr uint32_t kHeroTarget = 0x0013f420;  // the point the hero looks at (+0xd0)
const float kHalfPi = u2f(0x3fc90fdbu);
const float kSixthPi = u2f(0x3f060a92u);

// Where a talker keeps its head's state (pvar offsets differ by class).
struct Head {
    uint32_t look_at;       // the point it looks at when the hero is not watched (a quad)
    uint32_t watch_timer;   // frames it keeps watching the hero after he moved (120)
    uint32_t wander_timer;  // frames to its next random look-at point
    uint32_t tilt, turn, turn2;  // the joint angles written: the tilt, the turn (twice, halved)
    uint32_t stiff;         // a spring's stiffness forced to 2.75 while 0x15edb0 is set
    uint32_t node0, node1;  // the springs of joints 0 and 1
};

// While the hero's watch timer counts (he moved within 8 in front of it, 90 degrees either side) the head follows the
// hero's look-at point (0x13f420), else a random point 6 away in front of it (picked every 180 .. 300 frames); the
// turn is clamped to 90 degrees, the tilt to 30; the two springs pull the joints (harder while watching the hero).
// `looking`: the animation in which it turns its head (the springs still pull otherwise).
void head_turn(uint32_t m, bool looking, const Head& h) {
    float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);  // (0.02, 0.3)
    if (looking) {
        auto forget = [&] {
            if (lw(h.watch_timer) == 0) return;
            sw(h.watch_timer, 0);
            lq_sq(h.look_at, kHeroTarget);
        };
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 8.0f) {
            const float to_hero = atan2_f(fpu_sub(lwc1(kHeroTarget), lwc1(m + 0x10)), fpu_sub(lwc1(kHeroTarget + 4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), to_hero) < kHalfPi) {
                if (u2f(0x3c23d70au) < vec3_length(V(0x0013f450))) sw(h.watch_timer, static_cast<uint32_t>(frame_scale_i(0x78)));
                else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(h.watch_timer)));
            } else {
                forget();
            }
        } else {
            forget();
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(h.wander_timer))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            const float wait = frame_scale_f(rand_range_f(180.0f, 300.0f));
            sw(h.wander_timer, static_cast<uint32_t>(float_to_int(wait)));
            const float side = fpu_mul(rand_range_f(-90.0f, 90.0f), deg);
            const float yaw = angle_add(lwc1(m + 0x48), side);
            const float up = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(h.look_at), 6.0f, yaw, up);
            vec3_add(V(h.look_at), V(h.look_at), V(m + 0x10));
        }
        Vec4 target{};
        if (lw(h.watch_timer) != 0) {
            target = V(kHeroTarget);
            k = u2f(0x3d23d70au);  // (0.04)
        } else {
            target = V(h.look_at);
        }
        Vec4 eye = V(m + 0x10), dv{};
        eye.z = fpu_add(eye.z, 1.0f);
        vec3_sub(dv, target, eye);
        float turn = angle_sub(atan2_f(dv.x, dv.y), lwc1(m + 0x48));
        float tilt = -atan2_f(vec2_length(dv), dv.z);
        if (kHalfPi < turn) turn = kHalfPi;
        else if (turn < -kHalfPi) turn = -kHalfPi;
        if (kSixthPi < tilt) tilt = kSixthPi;
        else if (tilt < -kSixthPi) tilt = -kSixthPi;
        swc1(h.tilt, tilt);
        swc1(h.turn, fpu_mul(turn, 0.5f));
        swc1(h.turn2, fpu_mul(turn, 0.5f));
    }
    if (lbu(0x0015edb0) != 0) swc1(h.stiff, u2f(0x40300000u));
    joint_node_spring(m, h.node0, 0, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    joint_node_spring(m, h.node1, 1, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
}
}  // namespace

// Class 90, Bob: state 0 starts his talk (3, the talk done before, when 0x13d4c3 is set; else 1), talk radius 3.7;
// 1 talks while the hero is in his box (pvar +0x40), then the hero walks to 2.7 in front of him (2); 2, once the scene
// is over, back to 1 and, when the talk's answer (+4) was 2, gives the Thruster-Pack (item 3), saves the checkpoint at
// the hero, asks for the level change and the goal's announcement. His head turns while his first animation plays.
// orig: L11:0x002d0710
void pok_class90_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_level(m, 0);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, lbu(0x0013d4c3) != 0 ? 3u : 1u);
        talk_init(m, pv);
        swc1(pv + 0xc, u2f(0x406ccccdu));
        break;
    case 1:
        if (point_in_box(kHeroPos, lw(pv + 0x40)) != 0 && talk_update(m, pv) != 0) {
            hero_walk_to_l1(u2f(0x402ccccdu), m);
            sb(m + 0x20, 2);
        }
        break;
    case 2:
        if (lw(L11_ADDR(0x0015f5c4)) == 2) break;  // (the scene still playing)
        sb(m + 0x20, 1);
        if (lh(pv + 4) != 2) break;
        item_give(3, 1);
        checkpoint_save(kHeroPos, kHeroPos + 0x10);
        level_change_request(0, -1);
        sub_002039a0();
        sub_00203908(TXT(0x2afb), 0x3d);
        break;
    default:
        break;
    }
    head_turn(m, lbu(m + 0x53) == 0, Head{pv + 0x170, pv + 0x184, pv + 0x188, pv + 0xd4, pv + 0x158, pv + 0xd8, pv + 0xe0, pv + 0x70, pv + 0xf0});
}

// Class 298: near the camera (within 30) it keeps its floor band; state 0 (its idle animation 1) retires it once his
// errand is done (0x13d50b), else starts his talk (radius 4) in 1; 1 talks (the hero then walks to 3 in front of it, 2)
// and, while neither 0x13d389 nor 0x13d4e3 is set and the hero stands within 4, runs a skill point's clock (skill
// point 0x86: its announcement when more than 18 seconds passed since the time it keeps, or none was kept; else the
// time, in 10-second units, kept); its checkpoint (the box pvar +0x48) is saved when the hero stands at it; 2, once the
// scene is over, back to 1 and, when the talk's answer (+4) was 2, sets 0x13d4e3 / 0x13d50b, clears 0x13d389, shows a
// hint for 300 frames, asks for the level change and the goal's announcement. Its head turns in its animation 1.
// orig: L11:0x002f0e40
void pok_class298_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L11_ADDR(0x001677c0))) < 30.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }
    auto checkpoint = [&] {
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (lbu(0x0014c050 + lbu(m + 0xb0) + (level << 4)) == 0xff) return;
        if (!(vec3_distance(V(kHeroPos), V(m + 0x10)) < 4.0f)) return;
        if (!(abs_f(fpu_sub(lwc1(kHeroPos + 8), lwc1(m + 0x18))) < 4.0f)) return;
        checkpoint_mark_collected(lbu(m + 0xb0));
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x48));
        checkpoint_save(box + 0x30, box + 0x70);
    };
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        if (lbu(0x0013d50b) != 0) {
            moby_retire(m);
            return;
        }
        talk_init(m, pv);
        swc1(pv + 0xc, 4.0f);
        break;
    case 1:
        if (talk_update(m, pv) != 0) {
            hero_walk_to_l1(3.0f, m);
            sb(m + 0x20, 2);
        }
        if (lbu(0x0013d389) == 0 && lbu(0x0013d4e3) == 0 && vec3_distance(V(m + 0x10), V(kHeroPos)) < 4.0f) {
            const uint32_t kept = static_cast<uint32_t>(lhu(0x00141d9a)) * 600u;  // (skill point 0x86's half +2: 0x141968 + 0x86 * 8)
            const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(kept);
            const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(int_to_float(frame_scale_i(0x12)), 60.0f))));  // (cvt.w.s)
            if (limit < since || kept == 0) {
                sub_00203908(TXT(0x2b03), 0x86);
            } else {
                const int32_t now = ee_div(frame_scale_i(lw(0x0015eea4)), 600);
                if (static_cast<int32_t>(lhu(0x00141d9a)) < now) sh(0x00141d9a, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
            }
        }
        checkpoint();
        break;
    case 2:
        if (lw(L11_ADDR(0x0015f5c4)) == 2) break;  // (the scene still playing)
        sb(m + 0x20, 1);
        if (lh(pv + 4) != 2) break;
        sb(0x0013d4e3, 1);
        sb(0x0013d50b, 1);
        sb(0x0013d389, 0);
        sub_00263d40(TXT(0x2b06), frame_scale_i(0x12c));
        level_change_request(0, -1);
        sub_00203908(TXT(0x2af9), 0x3b);
        break;
    default:
        break;
    }
    head_turn(m, lbu(m + 0x53) == 1, Head{pv + 0x150, pv + 0x160, pv + 0x164, pv + 0xb4, pv + 0x138, pv + 0xb8, pv + 0xc0, pv + 0x50, pv + 0xd0});
}

}  // namespace rac1
