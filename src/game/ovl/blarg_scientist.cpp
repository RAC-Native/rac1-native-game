// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1105, the Scrawny Scientist, the citizen who sells
// Ratchet the Grindboots (a talker of the talk system, waiting in his room behind two door sets until the creatures there
// are down; the sale ends in a short scene: the camera moves along the room while his voice plays, the screen fades,
// and he is gone once Ratchet is back). Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's
// (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x90 his rotation, +0xd0 his target, +0x100
                                         // his velocity, +0x208c his sub-state)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
const float kHalfPi = u2f(0x3fc90fdbu);
const float kSixthPi = u2f(0x3f060a92u);
const float kDegree = u2f(0x3c8efa35u);  // pi / 180

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float step() { return lwc1(0x0015ed64); }   // the frame rate's step

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }
// the door set whose index the pvar `word` holds
uint32_t door_at(uint32_t word) { return table_moby(lw(word)); }

// a camera vector (x, y, z, 0) at `at`
void set_vec(uint32_t at, float x, float y, float z) {
    swc1(at, x);
    swc1(at + 4, y);
    swc1(at + 8, z);
    sw(at + 0xc, 0);
}

// the angle from `a` to `b` at `t`, as the original computes it: a + (b - a) * t
float view_angle_at(float a, float b, float t) { return angle_add(fpu_mul(angle_sub(b, a), t), a); }
}  // namespace

// Class 1105, the Scrawny Scientist (the Grindboots' seller). pvars: the talk record from +0 (talk_init / talk_update: its
// state +4 (2: the Grindboots bought), +8, its range +0xc, the name +0x20), the head's joint springs +0x40 (pitch +0xa4,
// yaw +0xa8; +0xb0 the big-head cheat's scale) and +0xc0 (yaw +0x128), the spot he looks at +0x140, the wait while he
// looks at Ratchet +0x158, the wait for a new spot +0x15c, the door sets +0x160 (his room's way in) and +0x164 (his room's
// way out), the box +0x168 (the talk's long range), the group of creatures +0x170, the talk's start delay +0x174, a step
// still to take +0x176, the scene's camera move +0x178 (seek_1d to 1, its rate +0x17c). States: 0 set up (gone with the
// flag 0x13d505; with 0x13d8ac he stands 2 forward, else crouched (animation 1) with the way out shut and his step to
// take; the way in shut, the talk started after 45 frames) (1); 1 talks once the group's creatures are down, the range
// 16 when the talk's +8 is 1 and Ratchet is in the box +0x168, else 3.7; the talk done: his step taken, Ratchet sent 2.2
// in front of him at the scene's end (2); the way in opened while Ratchet's sub-state is 15; 2 (no cutscene running)
// standing (animation 0): the talk left (+4 under 2) takes the checkpoint and opens the way out (1); the Grindboots
// bought: the item given (equipped), the voice 0x1775, the screen dark, the scene's camera at its start, Ratchet held in
// the scene (state 0x72), the way out shut and he hidden (3); 3 the camera moves to its end (0.1 a second, the way in
// opened past 0.1) while the screen fades in, then out once his voice is over past 0.2 (or the move done); dark, Ratchet
// is let go (hydro_2eac18(0)) (4); 4 the screen fades in (5); 5 when Ratchet's sub-state is 15 the way out opens and he is
// retired. In every state that stays, his head turns toward Ratchet's target or a spot nearby (the joint springs).
// orig: L6:0x00301070
void blg_class1105_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xe0);
    class_mates_mark_level(m, 1u);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L6_ADDR(0x00167540))) < 24.0f) {  // (near the camera)
        moby_floor_band(m);
        sb(m + 0x7f, 0x13);
    }
    // he takes his step: 2 forward along his +0xc0 axis, set down on the floor
    auto step_forward = [&]() {
        const uint32_t ahead = sp;
        vec3_set_length(V(ahead), V(m + 0xc0), 2.0f);
        vec3_add(V(m + 0x10), V(m + 0x10), V(ahead));
        swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
        sh(pv + 0x176, 0);
    };
    // the screen's fade 0x15f3fc moved toward `target` (4 a second)
    auto fade_to = [&](float target) { approach_f(L6_ADDR(0x0015f3fc), target, fpu_mul(dt(), 4.0f)); };
    switch (lbu(m + 0x20)) {  // (the table 0x2018a0: one case a state, 0 .. 5)
    case 0:
        if (lbu(0x0013d505) != 0) {
            moby_retire(m);
            ee_stack_free(0xe0);
            return;
        }
        if (lw(0x0013d8ac) != 0) {
            step_forward();
        } else {
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(10));
            sh(pv + 0x176, 1);
            door_set_shut_l6(door_at(pv + 0x164));
        }
        sh(pv + 0x174, static_cast<uint32_t>(frame_scale_i(0x2d)));
        door_set_shut_l6(door_at(pv + 0x160));
        sw(pv + 0x20, L6_ADDR(0x00201880));  // (his name)
        sb(m + 0x20, 1);
        talk_init(m, pv);
        break;
    case 1:
        if (lbu(pv + 0x8) == 1 && point_in_box(HERO_POS, lw(pv + 0x168)) != 0) swc1(pv + 0xc, 16.0f);
        else swc1(pv + 0xc, 3.7f);
        if (group_walkers_down_l6(lw(pv + 0x170)) != 0 &&
            timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x174))) != 0 && talk_update(m, pv) != 0) {
            sb(m + 0x20, 2);
            if (lh(pv + 0x176) != 0) step_forward();
            // (the hero's walk-to: Ratchet put 2.2 in front of him when the scene ends; L6:0x272670, the shared port: batch 501)
            hero_walk_to_l1(2.2f, m);
        }
        if (lw(PLAYER + 0x208c) == 0xf) door_set_open_l6(door_at(pv + 0x160));
        break;
    case 2:
        if (lw(L6_ADDR(0x0015f5c4)) == 2) break;  // (a cutscene running)
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
        if (lh(pv + 0x4) < 2) {
            checkpoint_mark_collected(lbu(m + 0xb0));
            door_set_open_l6(door_at(pv + 0x164));
            sb(m + 0x20, 1);
        }
        if (lh(pv + 0x4) == 2) {
            // the Grindboots bought
            item_give(0x1d, 1);
            level_change_request(0, -1);
            sub_00203908(0x1775, 0x2c);
            swc1(L6_ADDR(0x0015f3fc), 1.0f);
            sw(pv + 0x178, 0);
            set_vec(sp, 178.47f, 319.76f, 146.31f);
            set_vec(sp + 0x10, 0.07f, 0.18f, -1.77f);
            camera_scripted_set(sp, sp + 0x10, 1, 0, 0);
            hero_place(HERO_POS, PLAYER + 0x90, 0x72, 1);
            door_set_shut_l6(door_at(pv + 0x164));
            sb(m + 0x20, 3);
            sb(m + 0x31, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sw(m + 0x94, 0);
        }
        break;
    case 3: {
        // the camera moves from (from, look_from) to (to, look_to)
        const uint32_t from = sp, to = sp + 0x10, look_from = sp + 0x20, look_to = sp + 0x30, at = sp + 0x40, look = sp + 0x50;
        set_vec(from, 178.47f, 319.76f, 146.31f);
        set_vec(to, 172.55f, 316.01f, 145.27f);
        set_vec(look_from, 0.07f, 0.18f, -1.77f);
        set_vec(look_to, 0.07f, 0.13f, -1.21f);
        const float accel = fpu_mul(dt_sq(), 0.1f);
        seek_1d(pv + 0x178, pv + 0x17c, 1.0f, accel, accel, fpu_mul(dt(), 0.1f));
        if (0.1f < lwc1(pv + 0x178)) door_set_open_l6(door_at(pv + 0x160));
        const float t = lwc1(pv + 0x178);
        vec3_lerp(V(at), V(from), V(to), t);
        swc1(look + 8, view_angle_at(lwc1(look_from + 8), lwc1(look_to + 8), t));
        swc1(look + 4, view_angle_at(lwc1(look_from + 4), lwc1(look_to + 4), t));
        swc1(look + 0, view_angle_at(lwc1(look_from + 0), lwc1(look_to + 0), t));
        const uint32_t cam = lwu(L6_ADDR(0x00167580));
        if (cam != 0 && lh(cam + 0x86) != 5) camera_scripted_set(from, look_from, 1, 0, 0);
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(look);
        // the screen fades out once the move is done, or past 0.2 with no help voice playing (0x179b90 / 0x179bb4)
        const float moved = lwc1(pv + 0x178);
        const bool out = 1.0f <= moved ||
                         (lw(L6_ADDR(0x00179b90)) == 0 && lw(L6_ADDR(0x00179bb4)) == -1 && 0.2f < moved);
        fade_to(out ? 1.0f : 0.0f);
        if (lwc1(L6_ADDR(0x0015f3fc)) == 1.0f) {
            hero_place(HERO_POS, PLAYER + 0x90, 0, 0);
            hydro_2eac18(0);
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4:
        fade_to(0.0f);
        if (lwc1(L6_ADDR(0x0015f3fc)) == 0.0f) sb(m + 0x20, 5);
        break;
    case 5:
        if (lw(PLAYER + 0x208c) != 0xf) break;
        door_set_open_l6(door_at(pv + 0x164));
        moby_retire(m);
        ee_stack_free(0xe0);
        return;
    default:
        break;
    }
    // his head (standing, animation 0): toward Ratchet's target while Ratchet is within 8 in front of him (moving: kept
    // 120 frames after he stops), else toward the spot +0x140 (6 away at random, a new one every 180 .. 300 frames)
    float k = 0.02f, damp = 0.3f;
    if (lbu(m + 0x53) == 0) {
        const uint32_t pos = m + 0x10;
        bool facing = false;
        if (vec2_distance(V(pos), V(HERO_POS)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(PLAYER + 0xd0), lwc1(m + 0x10)), fpu_sub(lwc1(PLAYER + 0xd4), lwc1(m + 0x14)));
            facing = angle_dist(lwc1(m + 0x48), yaw) < kHalfPi;
        }
        if (facing) {
            if (0.01f < vec3_length(V(PLAYER + 0x100))) sw(pv + 0x158, static_cast<uint32_t>(frame_scale_i(0x78)));
            else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x158)));
        } else if (lw(pv + 0x158) != 0) {
            sw(pv + 0x158, 0);
            lq_sq(pv + 0x140, PLAYER + 0xd0);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x15c))) != 0) {
            sw(pv + 0x15c, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), kDegree));
            const float tilt = fpu_mul(rand_range_f(0.0f, 30.0f), kDegree);
            vec_from_spherical_v(V(pv + 0x140), 6.0f, yaw, tilt);
            vec3_add(V(pv + 0x140), V(pv + 0x140), V(pos));
        }
        const uint32_t target = sp, eye = sp + 0x10, d = sp + 0x20;
        if (lw(pv + 0x158) != 0) {
            lq_sq(target, PLAYER + 0xd0);
            k = 0.04f;
            damp = 0.3f;
        } else {
            lq_sq(target, pv + 0x140);
        }
        lq_sq(eye, pos);
        swc1(eye + 8, fpu_add(lwc1(eye + 8), 1.0f));
        vec3_sub(V(d), V(target), V(eye));
        float yaw = angle_sub(atan2_f(lwc1(d), lwc1(d + 4)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(d)), lwc1(d + 8));
        if (kHalfPi < yaw) yaw = kHalfPi;
        else if (yaw < -kHalfPi) yaw = -kHalfPi;
        if (kSixthPi < pitch) pitch = kSixthPi;
        else if (pitch < -kSixthPi) pitch = -kSixthPi;
        swc1(pv + 0xa4, pitch);
        swc1(pv + 0xa8, fpu_mul(yaw, 0.6f));
        swc1(pv + 0x128, fpu_mul(yaw, 0.4f));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xb0, 2.75f);  // (the big-head cheat)
    joint_node_spring(m, pv + 0x40, 0, fpu_mul(k, step()), fpu_mul(damp, step()));
    joint_node_spring(m, pv + 0xc0, 2, fpu_mul(k, step()), fpu_mul(damp, step()));
    ee_stack_free(0xe0);
}

}  // namespace rac1
