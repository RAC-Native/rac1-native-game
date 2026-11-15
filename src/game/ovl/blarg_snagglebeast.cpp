// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1051, the Alien Snagglebeast (the boss of the station's
// lab: it rises out of its pit, stalks and bites Ratchet, drops creatures on him, and with its health at 20 and at 10 goes
// back to its pit while its pack fights), with its frame, its circling, its rise's camera and the creatures it drops. It
// carries 1.02 Fixes fix 32 (the start of its fight held no longer once its countdown runs), which the PAL disc 2.00's own
// build has whatever the fix's row. Header blarg_own.h; registered in blarg_mobys.cpp (its health bar is the shared
// boss_health_bar_draw, its pump cameras shared_pump_camera.cpp). Addresses are level 6's (`orig: L6:`); its own data is
// L6_ADDR (the class's tuning at 0x161ef0 .. 0x161f14), the base ELF's are bare.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_pump_camera.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0 a direction, +0x80 his position)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
const float kDegree = u2f(0x3c8efa35u);  // pi / 180
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);
const float kQuarterPi = u2f(0x3f490fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the beast's yaw seeking `target` (its rate pvar +0xe0)
void turn_toward(uint32_t m, uint32_t pv, float target) {
    const float accel = fpu_mul(dt_sq(), kTwoPi);
    yaw_seek(m + 0x48, pv + 0xe0, target, accel, accel, fpu_mul(dt(), kPi));
}

// the animation set `set` over `frames` 60 Hz frames, unless it plays
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// its health bar's HUD element (+0x10c) taken down
void bar_down(uint32_t pv) {
    if (lw(pv + 0x10c) == -1) return;
    sub_002359f8(lwu(pv + 0x10c), 0);
    sw(pv + 0x10c, 0xffffffffu);
}

// back in its pit for a phase of its fight: at its home (+0xc0) facing Ratchet, the animation 6, the pump camera
void back_to_pit(uint32_t m, uint32_t pv) {
    lq_sq(m + 0x10, pv + 0xc0);
    swc1(m + 0x48, heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(HERO_POS), lwc1(HERO_POS + 4)));
    euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
    anim_to(m, 6, 5);
    pump_camera_start(m);
}
}  // namespace

// The beast's frame before its state (not in state 0). Its health bar's colour key (+0x108) to 0x14162a in states 4 .. 13;
// the bar's value (+0x110) its health +0x20 times 16. A hit (mask 0x330000): its damage half again from the classes 0xb0,
// 0x79 and 0x33b, two thirds from 0x131; outside a phase change (+0x170) its health crossing 20 or 10 (the countdown +0x12c
// at 10 frames at least) stops there and starts a phase change (+0x170, +0x174 its floor, 30 frames); during one the health
// stays at its floor. Killed (state 0xe: its bar down, its bolts, the checkpoint at the box +0xf8, the animation 0xa) or
// flinching, with its colour pulse +0x60. The flag 2 of +0x2e makes it die at once. Ratchet without his gadget 8 and
// holding 0x20 marks +0x178. The phase change counts down with the screen fading out, then (unless in state 1) puts it
// back in its pit: state 3 at 19.9 (its pack's first wave), or at 9.9 state 5 with Ratchet put at the box +0xf8; else the
// screen fades back in. Its glow (+0x90) pulses (0x161f0c degrees a second, 0x161f10 .. 0x161f14), it picks its target
// (+0x70) in its arena (the path +0xec), and with no target moby (+0xb0) yet, the hero's moby.
// orig: L6:0x002fa9c0
void beast_frame_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t hit_state = sp, hit_damage = sp + 4;
    if (static_cast<uint32_t>(lbu(m + 0x20) - 4) < 10u && lw(pv + 0x108) != -1) sh(0x0014162a, lhu(pv + 0x108));
    sw(pv + 0x110, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x20), 16.0f))));
    swc1(hit_damage, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t reaction;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_damage);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_damage, dmg);
    }
    const uint32_t state = lbu(m + 0x20);
    if (lw(hit_state) != 1 && state != 0xe && state != 3 && state != 5) {
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0) {
            const int32_t cls = lh(src + 0xa6);
            float scale = 0.0f;
            if (cls == 0xb0 || cls == 0x79 || cls == 0x33b) scale = 1.5f;
            else if (cls == 0x131) scale = u2f(0x3f2a7efau);  // 0.666
            if (scale != 0.0f) swc1(hit_damage, fpu_mul(lwc1(hit_damage), scale));
        }
        const float dmg = lwc1(hit_damage);
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        if (lw(pv + 0x170) != 0) {
            if (hp < lwc1(pv + 0x174)) swc1(pv + 0x20, lwc1(pv + 0x174));
        } else {
            // a phase change when the health crosses 20 or 10
            auto crosses = [&](float at) {
                return !(lw(pv + 0x12c) < frame_scale_i(0xa)) && lwc1(pv + 0x20) <= at && at < fpu_add(lwc1(pv + 0x20), dmg);
            };
            float floor = 0.0f;
            if (crosses(20.0f)) floor = 20.0f;
            else if (crosses(10.0f)) floor = 10.0f;
            if (floor != 0.0f) {
                swc1(pv + 0x174, floor);
                sw(pv + 0x170, 1);
                swc1(pv + 0x20, floor);
                sw(pv + 0x12c, static_cast<uint32_t>(frame_scale_i(0x1e)));
            }
        }
        if (lwc1(pv + 0x20) <= 0.0f) reaction = 1;
        else if (reaction == 1) reaction = 5;
        // (the reactions' table 0x201760: 1 / 2 killed, 3 .. 10 flinch, the rest nothing more)
        if (reaction == 1 || reaction == 2) {
            bar_down(pv);
            sb(m + 0x20, 0xe);
            moby_drop_bolts(m, 0, -1);
            const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0xf8));
            checkpoint_save(b + 0x30, b + 0x70);
            const uint32_t flags = lhu(m + 0x34) & 0xefffu;
            sh(m + 0x34, flags);
            if (lbu(m + 0x53) != 0xa) moby_anim_set(m, 0xa, 0, frame_scale_i(0xa));
            sb(pv + 0x67, 0xfa);
        } else if (reaction >= 3 && reaction <= 10) {
            sb(pv + 0x67, 0xc8);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    if ((lbu(pv + 0x2e) & 2) != 0 && lbu(m + 0x20) != 0xf) {  // (dead at once)
        bar_down(pv);
        sb(m + 0x20, 0xe);
        ee_stack_free(0x70);
        return;
    }
    if (lbu(m + 0x20) >= 2 && lw(0x00140408) != 8 && (lwu(0x0013cae0) & 0x20u) != 0) sw(pv + 0x178, 1);
    const int32_t count = lw(pv + 0x12c);
    if (count != 0 && lw(pv + 0x170) != 0) {
        // the phase change counting down, the screen fading out over its last 10 frames
        sw(pv + 0x12c, static_cast<uint32_t>(count - 1));
        if (lw(pv + 0x12c) < frame_scale_i(0xa))
            swc1(L6_ADDR(0x0015f3fc), fpu_sub(1.0f, fpu_div(int_to_float(lw(pv + 0x12c)), frame_scale_f(10.0f))));
        if (lw(pv + 0x12c) == 0 && lbu(m + 0x20) != 1) {
            if (10.0f < lwc1(pv + 0x20)) {
                swc1(pv + 0x20, u2f(0x419f3333u));  // 19.9
                sb(m + 0x20, 3);
                sw(pv + 0x170, 0);
                back_to_pit(m, pv);
            } else {
                swc1(pv + 0x20, u2f(0x411e6666u));  // 9.9
                sb(m + 0x20, 5);
                sw(pv + 0x170, 0);
                const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0xf8));
                hero_place(b + 0x30, b + 0x70, 0x72, 1);
                back_to_pit(m, pv);
            }
        }
    } else if (lw(pv + 0x170) == 0 && lw(pv + 0x12c) < frame_scale_i(0xa)) {
        // the screen fading back in
        const int32_t t = lw(pv + 0x12c) + 1;
        sw(pv + 0x12c, static_cast<uint32_t>(t));
        swc1(L6_ADDR(0x0015f3fc), fpu_sub(1.0f, fpu_div(int_to_float(t), 10.0f)));
    }
    const float glow = angle_add(lwc1(pv + 0x128), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x00161f0c)), kDegree), dt()));
    swc1(pv + 0x128, glow);
    sw(m + 0x90, rgba_lerp(lwu(L6_ADDR(0x00161f10)), lwu(L6_ADDR(0x00161f14)), fpu_mul(fpu_add(sin_vu(glow), 1.0f), 0.5f)));
    const uint32_t arena = lvd::path(lw(pv + 0xec));
    target_select(m, pv + 0x70, 0, 0, arena + 0x10, lw(arena), 255.0f);
    if (lw(pv + 0xb0) == 0) sw(pv + 0xb0, lwu(0x001413d0));
    ee_stack_free(0x70);
}

// The camera of the beast's rise (the uid-5 camera's +0x80 / +0x90): while it rises (the animation 8) from where the
// camera is, else moving (+0x118, seek_1d to 1) from 8 along the beast's facing to 7 behind Ratchet and 4 up, looking at
// the beast and tilting by up to 30 degrees less; 1 once there with the beast idle (the animation 1).
// orig: L6:0x002fb2a8
uint32_t beast_rise_camera_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t at = sp, angles = sp + 0x10, near = sp + 0x20, behind = sp + 0x30;
    vec_clear(V(angles));
    if (lbu(m + 0x53) == 8) {
        lq_sq(at, L6_ADDR(0x00167540));  // (the camera's place)
    } else {
        const float accel = fpu_mul(dt_sq(), 3.0f);
        seek_1d(pv + 0x118, pv + 0x11c, 1.0f, accel, accel, fpu_add(dt(), dt()));
        vec3_set_length(V(near), V(m + 0xc0), 8.0f);
        vec3_add(V(near), V(near), V(m + 0x10));
        swc1(near + 8, fpu_add(lwc1(HERO_POS + 8), 2.0f));
        vec3_set_length(V(behind), V(PLAYER), -7.0f);
        vec3_add(V(behind), V(behind), V(HERO_POS));
        swc1(behind + 8, fpu_add(lwc1(behind + 8), 4.0f));
        vec3_lerp(V(at), V(near), V(behind), lwc1(pv + 0x118));
    }
    swc1(angles + 8, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(at)), fpu_sub(lwc1(m + 0x14), lwc1(at + 4))));
    const float flat = vec2_distance(V(at), V(m + 0x10));
    const float pitch = -atan2_f(flat, fpu_sub(lwc1(m + 0x18), lwc1(at + 8)));
    swc1(angles + 4, fpu_sub(pitch, fpu_mul(fpu_mul(fpu_sub(1.0f, lwc1(pv + 0x118)), 30.0f), kDegree)));
    uid5_set_pvar_80(at);
    uid5_set_pvar_90(angles);
    const uint32_t done = (1.0f <= lwc1(pv + 0x118) && lbu(m + 0x53) == 1) ? 1u : 0u;
    ee_stack_free(0xd0);
    return done;
}

// The beast circling its arena's centre (+0xd0) at its radius (+0xdc) to stay across from Ratchet (by at most 45 degrees
// a frame of its bearing), facing its target moby; it walks (0x161ef8 a second, its speed +0xe8) only while Ratchet is
// within 16 and it is more than 1 from its spot (the animation 0 walking, 1 standing), kept inside its arena.
// orig: L6:0x002fbc88
void beast_circle_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t spot = sp, step = sp + 0x10;
    const uint32_t pos = m + 0x10;
    const uint32_t target = lwu(pv + 0xb0);
    const float hero_side = heading(lwc1(pv + 0xd0), lwc1(pv + 0xd4), lwc1(HERO_POS), lwc1(HERO_POS + 4));
    const float own_side = heading(lwc1(pv + 0xd0), lwc1(pv + 0xd4), lwc1(pos), lwc1(pos + 4));
    float swing = angle_sub(angle_add(hero_side, u2f(0x40490fd0u)), own_side);
    if (kQuarterPi < swing) swing = kQuarterPi;
    else if (swing < -kQuarterPi) swing = -kQuarterPi;
    const float a = angle_add(swing, own_side);
    swc1(spot, fpu_mul(cos_vu(a), lwc1(pv + 0xdc)));
    swc1(spot + 4, fpu_mul(sin_vu(a), lwc1(pv + 0xdc)));
    sw(spot + 8, 0);
    vec3_add(V(spot), V(spot), V(pv + 0xd0));
    turn_toward(m, pv, heading(lwc1(pos), lwc1(pos + 4), lwc1(target + 0x10), lwc1(target + 0x14)));
    if (1.0f < vec3_distance(V(pos), V(spot)) && vec2_distance(V(pos), V(HERO_POS)) < 16.0f)
        approach_f(pv + 0xe8, fpu_mul(lwc1(L6_ADDR(0x00161ef8)), dt()), fpu_mul(lwc1(L6_ADDR(0x00161efc)), dt_sq()));
    else
        approach_f(pv + 0xe8, 0.0f, fpu_mul(lwc1(L6_ADDR(0x00161efc)), dt_sq()));
    if (lwc1(pv + 0xe8) == 0.0f) {
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
    } else if (lbu(m + 0x53) != 0) {
        moby_anim_set(m, 0, 0, frame_scale_i(0x14));
    }
    vec3_sub(V(step), V(spot), V(pos));
    sw(step + 8, 0);
    vec3_set_length(V(step), V(step), lwc1(pv + 0xe8));
    vec3_add(V(pos), V(pos), V(step));
    path_push_out_2d(lwu(pv + 0xec), pos, pos, 1.0f);
    ee_stack_free(0xa0);
}

// The beast's pack (the group +0xf0) dropped in: each creature waiting (state 0xf) let out from where it waits, 15 .. 20
// higher.
// orig: L6:0x002fb090
void beast_drop_pack_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t list = lvd::group(lw(pv + 0xf0));
    if (list == 0) return;
    const uint32_t sp = ee_stack_alloc(0x40);
    for (uint32_t e = list;; e += 2) {
        const uint32_t c = lwu(L6_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        lq_sq(sp, c + 0x10);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), rand_range_f(15.0f, 20.0f)));
        creature_let_out_l6(lwu(L6_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8), sp);
        if (lh(e) < 0) break;
    }
    ee_stack_free(0x40);
}

// Class 1051, the Snagglebeast. pvars: health +0x20, the colour pulse +0x60, the target +0x70, its target moby +0xb0, the
// pump mode +0xb4 (2: Ratchet at a pump, the beast held back), its home +0xc0, the arena's centre +0xd0 and its radius
// +0xdc, its yaw rate +0xe0, its speed +0xe8, its arena (a path) +0xec, its pack +0xf0 (dropped) and +0xf4 (put away at
// the start, brought back), the checkpoint box +0xf8, its doors (door sets) +0xfc / +0x100, its panels (a group) +0x104,
// its bar's colour key +0x108, its bar +0x10c / +0x110, a timer +0x120, its glow's phase +0x128, a countdown +0x12c, the
// joint scale +0x130, the phase change +0x170 / +0x174, +0x178 (marked in the frame). States: 0 set up (17.5 lower, its
// pack +0xf4 put away; retired when beaten before, 0x13d3ab, unless 0x13d505); 1 waits for Ratchet (its target moby
// Ratchet's, not at a pump): 10 frames on (the screen fading), the doors shut, the panels out, the pump camera (2); 2
// rises out of its pit (its speed +0x48 under gravity, the animation 9 near the top, 6 then 1 once up) until its camera
// is done (7); 3 / 5 a pack's wave (the pack +0xf0 dropped, or +0xf4 brought back) while the pump camera follows (4 / 6);
// 4 / 6 circles until the pack is down (8); 7 waits while Ratchet is at a pump, 8 stalks its target (0x161ef4 a second,
// backing off within 0x161f04), its timer +0x120 sending it to circle (12) and back (11), bites within 0x161f00 and 0x161f08
// degrees (9 turn, 10 bite: seven spheres at its joints hit mobys between its animation's frames 1 and 8, 11 the bite's
// end); 13 goes home while Ratchet is at a pump; 14 dies (a burst, its debris 0x6a2 / 0x6a3, the doors open, the panels back,
// and unless +0x178 or 0x13d412 a skill point: its jingle and the text 0x53d6), or at once with +0x2e's flag 2; 15 gone:
// the cameras of type 0x12 flagged, and in a cutscene (0x15f5c4 2) its first door opened and the beast retired.
// orig: L6:0x002f9a28
void blg_class1051_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t pos = m + 0x10;
    beast_frame_l6(m);
    const uint32_t target = lwu(pv + 0xb0);
    joint_node_scale(m, lwu(L6_ADDR(0x00161ef0)), pv + 0x130, u2f(0x3ff33333u));  // 1.9
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L6_ADDR(0x00167540))) < 45.0f) {  // (near the camera)
        moby_floor_band(m);
        sb(m + 0x7f, 0x24);
    }
    if (static_cast<uint32_t>(lbu(m + 0x20) - 3) < 11u)  // (states 3 .. 13: its health bar)
        sw(pv + 0x10c, static_cast<uint32_t>(camera_request(0x16, 0xffff, L6_ADDR(0x00245b40), L6_ADDR(0x00245c60),
                                                            L6_ADDR(0x00249738), pv + 0x110, 0x1ef)));
    auto doors_shut = [&] {
        door_set_shut_l6(table_moby(lw(pv + 0xfc)));
        door_set_shut_l6(table_moby(lw(pv + 0x100)));
    };
    auto settle = [&](uint32_t next) {  // after the pack's drop: the pump camera done, the next state
        if (pump_camera_follow(m) != 0) {
            sb(m + 0x20, next);
            blarg_class1051_switch(m);
        }
    };
    auto to_stalk = [&] {
        sb(m + 0x20, 8);
        sw(pv + 0x120, static_cast<uint32_t>(frame_scale_i(0x4b0)));
        anim_to(m, 3, 0x14);
    };
    switch (lbu(m + 0x20)) {  // (the table 0x201720: one case a state, 0 .. 15; 4 runs into 6)
    case 0: {
        swc1(pv + 0x20, 30.0f);
        sb(pv + 0x28, 3);
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 1);
        sh(pv + 0x24, 0x1e);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(14.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x41933333u))));  // 18.4
        sw(pv + 0x120, static_cast<uint32_t>(frame_scale_i(0x4b0)));
        sw(pv + 0x178, 0);
        lq_sq(pv + 0xc0, pos);
        sw(pv + 0x10c, 0xffffffffu);
        const uint32_t arena = lvd::path(lw(pv + 0xec));
        sw(pv + 0x114, lwu(arena));
        const int32_t n = lw(arena);
        vec3_lerp(V(pv + 0xd0), V(arena + 0x10), V(arena + 0x10 + (static_cast<uint32_t>(n / 2) << 4)), 0.5f);
        swc1(pv + 0xdc, vec2_distance(V(pv + 0xc0), V(pv + 0xd0)));
        swc1(pv + 0x128, rand_angle());
        sw(pv + 0x12c, static_cast<uint32_t>(frame_scale_i(0x1e)));
        const float z = fpu_add(lwc1(m + 0x18), 17.5f);
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x4001u) & 0xefffu);
        swc1(m + 0x18, z);
        group_put_away_l6(lw(pv + 0xf4));
        if (lbu(0x0013d3ab) == 0 || lbu(0x0013d505) != 0) {
            sb(m + 0x20, 1);
        } else {
            moby_retire(m);
        }
        break;
    }
    case 1: {
        // (1.02 Fixes, fix 32: the 1.02 build tests the pump mode and its target only before the countdown starts)
        const bool fixed = fixes102_snagglebeast_start();
        if (!fixed || lw(pv + 0x170) == 0) {
            if (lw(pv + 0xb4) == 2) break;
            if (lh(target + 0xa6) != 0) break;
        }
        if (lw(pv + 0x170) == 0) {
            sw(pv + 0x170, 1);
            sw(pv + 0x12c, static_cast<uint32_t>(frame_scale_i(0xa)));
            break;
        }
        if (lw(pv + 0x12c) != 0) break;
        doors_shut();
        panels_slide_out_l6(lw(pv + 0x104));
        sw(pv + 0x170, 0);
        sb(m + 0x20, 2);
        const uint32_t model = lwu(lwu(m + 0x24) + 0x10);
        sh(m + 0x34, (lhu(m + 0x34) | 0x1000u) & 0xfffeu);
        const bool rising = lbu(m + 0x53) == 8;
        sw(m + 0x94, model);
        if (!rising) moby_anim_set(m, 8, 0, frame_scale_i(1));
        pump_camera_start(m);
        break;
    }
    case 2: {
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x48)));
        const float speed = fpu_sub(lwc1(pv + 0x48), fpu_mul(dt_sq(), 30.0f));
        swc1(pv + 0x48, speed);
        if (lbu(m + 0x53) == 8) {
            const float frames_left = abs_f(fpu_div(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0xc8)), speed));
            if (frames_left <= 6.0f && lbu(m + 0x53) != 9) moby_anim_set(m, 9, 0, frame_scale_i(5));
        } else {
            if (lwc1(m + 0x18) < lwc1(pv + 0xc8)) swc1(m + 0x18, lwc1(pv + 0xc8));
            if (lwc1(m + 0x54) == 0.0f && (lbu(m + 0x70) & 2) != 0) {
                if (lbu(m + 0x53) == 9) moby_anim_set(m, 6, 0, frame_scale_i(0x1e));
                else anim_to(m, 1, 0x1e);
            }
        }
        if (beast_rise_camera_l6(m) != 0) {
            sb(m + 0x20, 7);
            blarg_class1051_switch(m);
        }
        break;
    }
    case 3:
    case 5: {
        if (lbu(m + 0x53) == 6 && lwc1(m + 0x54) == 0.0f && (lbu(m + 0x70) & 2) != 0) {
            moby_anim_set(m, 1, 0, frame_scale_i(0x14));
            if (lbu(m + 0x20) == 3) beast_drop_pack_l6(m);
            else group_bring_back_l6(lw(pv + 0xf4));
        }
        settle(lbu(m + 0x20) == 3 ? 4 : 6);
        break;
    }
    case 4:
    case 6: {
        if (lbu(m + 0x20) == 4) beast_drop_pack_l6(m);
        beast_circle_l6(m);
        const uint32_t st = lbu(m + 0x20);
        if (st == 4) {
            if (group_walkers_down_l6(lw(pv + 0xf0)) != 0) to_stalk();
        } else if (st == 6) {
            if (moby_group_count_l1(lw(pv + 0xf4), -1) == 0) to_stalk();
        }
        break;
    }
    case 7: {
        if (lw(pv + 0xb4) == 2) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 8);
        if (set != 3) moby_anim_set(m, 3, 0, frame_scale_i(0x14));
        break;
    }
    case 8: {
        const uint32_t tpos = target + 0x10;
        const float dist = vec2_distance(V(pos), V(tpos));
        const float off = angle_dist(lwc1(m + 0x48), heading(lwc1(pos), lwc1(pos + 4), lwc1(tpos), lwc1(tpos + 4)));
        turn_toward(m, pv, heading(lwc1(pos), lwc1(pos + 4), lwc1(tpos), lwc1(tpos + 4)));
        if (off < kQuarterPi && lwc1(L6_ADDR(0x00161f00)) < dist)
            approach_f(pv + 0xe8, fpu_mul(lwc1(L6_ADDR(0x00161ef4)), dt()), fpu_mul(lwc1(L6_ADDR(0x00161efc)), dt_sq()));
        else if (dist < lwc1(L6_ADDR(0x00161f04)))
            approach_f(pv + 0xe8, -fpu_mul(lwc1(L6_ADDR(0x00161ef4)), dt()), fpu_mul(lwc1(L6_ADDR(0x00161efc)), dt_sq()));
        else
            approach_f(pv + 0xe8, 0.0f, fpu_mul(dt_sq(), 8.0f));
        vec3_sub(V(sp), V(tpos), V(pos));
        sw(sp + 8, 0);
        vec3_set_length(V(sp), V(sp), lwc1(pv + 0xe8));
        vec3_add(V(pos), V(pos), V(sp));
        path_push_out_2d(lwu(pv + 0xec), pos, pos, 1.0f);
        if (lw(pv + 0xb4) == 2) {
            sb(m + 0x20, 0xd);
            break;
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x120))) != 0) {
            sw(pv + 0x120, static_cast<uint32_t>(frame_scale_i(0x168)));
            sb(m + 0x20, 0xc);
            break;
        }
        if (dist < lwc1(L6_ADDR(0x00161f00)) && off < fpu_mul(lwc1(L6_ADDR(0x00161f08)), kDegree)) {
            const uint32_t set = lbu(m + 0x53);
            sb(m + 0x20, 9);
            sb(m + 0xbc, 7);
            if (set != 4) moby_anim_set(m, 4, 0, frame_scale_i(0x14));
        }
        break;
    }
    case 9: {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x120)));
        turn_toward(m, pv, lwc1(m + 0x48));
        if ((lbu(m + 0x70) & 2) == 0) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 0xa);
        if (set != 5) moby_anim_set(m, 5, 0, frame_scale_i(0x14));
        break;
    }
    case 10: {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x120)));
        if (1.0f < moby_anim_time(m) && moby_anim_time(m) < 8.0f) {
            // the bite: a hit record (the yaw's direction, damage 1, the beast its source) at seven joints
            const uint32_t rec = sp + 0x10, joint = sp + 0x40;
            const float yaw = lwc1(m + 0x48);
            const float k = u2f(0x3ee66666u);  // 0.45 (a stack word nothing reads)
            swc1(sp, fpu_mul(cos_vu(yaw), k));
            swc1(sp + 4, fpu_mul(sin_vu(yaw), k));
            swc1(sp + 0x2c, 1.0f);
            sw(sp + 0x30, 1);
            sw(sp + 8, 0);
            sw(sp + 0x20, m);
            sw(sp + 0x24, 1);
            swc1(rec, cos_vu(lwc1(m + 0x48)));
            swc1(rec + 4, sin_vu(lwc1(m + 0x48)));
            sw(rec + 8, 0);
            for (uint32_t j = 0; j < 7; ++j) {
                moby_chain_point_v(m, j, V(joint));
                mobys_in_sphere(joint, j < 5 ? 0.5f : u2f(0x3f8ccccdu), 0, m, rec);  // (1.1 at the last two)
            }
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, lbu(m + 0xbc));
        if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        break;
    }
    case 11: {
        if ((lbu(m + 0x70) & 2) == 0) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, lbu(m + 0xbc));
        if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        break;
    }
    case 12: {
        beast_circle_l6(m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x120))) == 0) break;
        sw(pv + 0x120, static_cast<uint32_t>(frame_scale_i(0x4b0)));
        sb(m + 0xbc, 7);
        sb(m + 0x20, 0xb);
        anim_to(m, 6, 0x14);
        break;
    }
    case 13: {
        const uint32_t home = pv + 0xc0;
        const float dist = vec2_distance(V(pos), V(home));
        const float off = angle_dist(lwc1(m + 0x48), heading(lwc1(pos), lwc1(pos + 4), lwc1(home), lwc1(home + 4)));
        turn_toward(m, pv, heading(lwc1(pos), lwc1(pos + 4), lwc1(home), lwc1(home + 4)));
        if (off < kQuarterPi) {
            vec3_sub(V(sp), V(home), V(pos));
            sw(sp + 8, 0);
            vec3_set_length(V(sp), V(sp), fpu_mul(lwc1(L6_ADDR(0x00161ef4)), dt()));
            vec3_add(V(pos), V(pos), V(sp));
            path_push_out_2d(lwu(pv + 0xec), pos, pos, 1.0f);
        }
        if (lw(pv + 0xb4) != 2) {
            sb(m + 0x20, 7);
            break;
        }
        if (!(dist < 1.0f)) break;
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 7);
        if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        break;
    }
    case 14: {
        sb(0x0013d3ab, 1);  // (beaten)
        sw(pv + 0x110, 0);
        sb(m + 0x30, 0xff);
        if (lbu(pv + 0x2e) == 2) {
            door_set_open_l6(table_moby(lw(pv + 0x100)));
            panels_slide_back_l6(lw(pv + 0x104));
            const uint32_t set = lbu(m + 0x53);
            sb(m + 0x20, 0xf);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sb(m + 0x31, 0);
            sw(m + 0x94, 0);
            if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        lq_sq(sp, pos);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.0f));
        enemy_death_burst(m, sp, 2.0f, 20.0f);
        debris_burst(m, 0x6a2, 1, 0x6a3, 3, 6, 2);
        door_set_open_l6(table_moby(lw(pv + 0x100)));
        panels_slide_back_l6(lw(pv + 0x104));
        {
            const uint32_t set = lbu(m + 0x53);
            sb(m + 0x20, 0xf);
            if (set != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        }
        const uint32_t flags = lhu(m + 0x34);
        sw(m + 0x94, 0);
        sb(m + 0x31, 0);
        sh(m + 0x34, flags | 1u);
        if (lw(pv + 0x178) != 0 || lbu(0x0013d412) != 0) break;
        sb(0x0013d412, 1);  // (a skill point)
        sound_play_indexed(1, 0, 0);
        sub_00263d40(TXT(0x53d6), -1);
        break;
    }
    case 15: {
        for (uint32_t i = 0; i < 48; ++i) {  // (the level's live cameras: the first of type 0x12 flagged)
            if (lw(L6_ADDR(0x0016a010) + i * 4) == 0) continue;
            const uint32_t cam = L6_ADDR(0x001678d0) + i * 0xa0;
            if (lh(cam + 0x86) != 0x12) continue;
            camera_type12_flag(cam);
            break;
        }
        if (lw(L6_ADDR(0x0015f5c4)) != 2) break;
        door_set_open_l6(table_moby(lw(pv + 0xfc)));
        moby_retire(m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0xe0);
}

}  // namespace rac1
