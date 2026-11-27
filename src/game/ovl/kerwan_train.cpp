// Kerwan's own moby classes: the train of Metropolis's ride (class 822, the locomotive) and its three helpers. Header
// kerwan_mobys.h, registered in kerwan_mobys.cpp. Addresses are Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR,
// the engine's words shared with Veldin are the level's own (OV), the base ELF's (the hero 0x13f350, the frame times) are bare.
//
// The locomotive drives an aircraft base moby (class 845, its pvar +0xc4, jet_update_base_l1) along the level's paths and
// pulls a chain of carriages (class 1210, the first at its pvar +0xa0, each the next at its own +0xc0) behind it. Its pvars:
// +0xa4 / +0xa8 / +0xac the paths of the ride (the first leg's, the station's heading, the return's), +0xb0 / +0xb4 the boxes the
// hero is tested in (the stop's, the station's), +0xbc its length to the first coupling, +0xc0 its top speed, +0xc8 its speed,
// +0xcc / +0xd0 the turn rates, +0xd4 the leg of the path it stops at, +0xd8 / +0x100 the two cameras of the ride (scripted
// camera mobys), +0xdc the gap to the engine, +0xe0 eight class 574 mobys of the station, +0x104 the whistle's wait,
// +0x108 the rolling sound's slot, +0x10c how long the hero has been below the track. The state is the moby's byte +0xbc.
#include <game/level/level_data.h>
#include <game/level/level_read.h>
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/sound_emitter.h>
#include <game/port_stubs.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;      // the hero (+0x80 his position)
constexpr uint32_t HERO_POS = 0x0013f3d0;  // (the same, +0x80)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kPi = u2f(0x40490fdbu);
const float kDegree = u2f(0x3c8efa35u);

// the moby of index `i` (the moby list in 0x100-byte steps)
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the camera's position (the level's view, Veldin's 0x166dc0)
uint32_t camera_pos() { return OV(0x00166dc0); }

// The gap the level keeps between two carriages' couplings (Kerwan's own word).
float coupling_gap() { return lwc1(L3_ADDR(0x00161368)); }

// The heading of a path's first leg (its point 0 toward point 1).
float first_leg_heading(uint32_t path) {
    return atan2_f(fpu_sub(lvd::path_f32(path + 0x20), lvd::path_f32(path + 0x10)), fpu_sub(lvd::path_f32(path + 0x24), lvd::path_f32(path + 0x14)));
}

// A scripted camera moby (`cam`'s pvar +0x18 / +0x14: its yaw and pitch) turned to look from the camera's place at `at`.
void camera_look_at(uint32_t cam_pv, uint32_t at) {
    const uint32_t eye = camera_pos();
    swc1(cam_pv + 0x18, atan2_f(fpu_sub(lvd::path_f32(at), lwc1(eye)), fpu_sub(lvd::path_f32(at + 4), lwc1(eye + 4))));
    const float d = vec2_distance(V(eye), lvd::path_row(at));
    swc1(cam_pv + 0x14, -atan2_f(d, fpu_sub(lvd::path_f32(at + 8), lwc1(eye + 8))));
}

// The moby's sound in the slot word `slot_word` stopped when that slot still plays it, and the word set to -1.
void sound_release(uint32_t m, uint32_t slot_word) {
    const int32_t slot = lw(slot_word);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(slot_word, 0xffffffffu);
}

// The rolling sound (sound 1 of the class, flags 4) kept playing in the slot +0x108, and the whistle (sound 0) when the
// wait +0x104 runs out, then a new wait of 400 .. 900 frames.
void train_sounds(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x108)) == 0) sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x104))) != 0) {
        moby_sound_start(0, 0, m);
        sw(pv + 0x104, static_cast<uint32_t>(rand_range(frame_scale_i(0x384), frame_scale_i(0x190))));
    }
}

// One carriage's frame after it moved: its animation and matrix, drawn, its coupling (chain point 1) into `coupling`, the
// attached frame (pvar +0x20) from its move since the last frame, its place and angles kept (+0xa0 / +0xb0).
void carriage_settle(uint32_t car, uint32_t cpv, uint32_t coupling, uint32_t moved) {
    moby_anim_advance(car);
    moby_update_transform(car);
    sh(car + 0x34, lhu(car + 0x34) | 6u);
    moby_chain_point_v(car, 1, V(coupling));
    vec3_sub(V(moved), V(car + 0x10), V(cpv + 0xa0));
    attach_frame_set(cpv + 0x20, moved, cpv + 0xb0, car + 0x40);
    lq_sq(cpv + 0xa0, car + 0x10);
    lq_sq(cpv + 0xb0, car + 0x40);
}
}  // namespace

// The train put down at `point` heading `heading`: the engine there, the locomotive its length +0xdc behind it (facing the
// other way, level), its attached frame from the move, its turn rates cleared, and each carriage set behind the coupling
// before it: the level's gap and the carriage's own length (+0xc4) behind, at the locomotive's height, facing the coupling.
// The engine is drawn and left in state 0.
// orig: L3:0x00292578
void train_place_l3(uint32_t m, uint32_t point, float heading) {
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t at = sp, behind = sp + 0x10, moved = sp + 0x20, old_pos = sp + 0x30, old_ang = sp + 0x40;
    const uint32_t gap = sp + 0x50, length = sp + 0x60;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t engine = moby_at(lw(pv + 0xc4));
    int32_t car_index = lw(pv + 0xa0);
    lq_sq(at, point);
    lq_sq(old_pos, m + 0x10);
    lq_sq(old_ang, m + 0x40);
    lq_sq(engine + 0x10, at);
    swc1(behind, fpu_mul(cos_vu(heading), -lwc1(pv + 0xdc)));
    swc1(behind + 4, fpu_mul(sin_vu(heading), -lwc1(pv + 0xdc)));
    sw(behind + 8, 0);
    vec3_add(V(m + 0x10), V(at), V(behind));
    sw(m + 0x44, 0);
    swc1(m + 0x48, angle_add(heading, kPi));
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x60, moved, old_ang, m + 0x40);
    sw(pv + 0xcc, 0);
    sw(pv + 0xd0, 0);
    // the coupling behind the locomotive: its length +0xbc behind its place
    swc1(behind, fpu_mul(cos_vu(heading), -lwc1(pv + 0xbc)));
    swc1(behind + 4, fpu_mul(sin_vu(heading), -lwc1(pv + 0xbc)));
    sw(behind + 8, 0);
    vec3_add(V(behind), V(behind), V(m + 0x10));
    while (car_index != -1) {
        const uint32_t car = moby_at(car_index);
        const uint32_t cpv = lwu(car + 0x78);
        lq_sq(cpv + 0xa0, car + 0x10);
        lq_sq(cpv + 0xb0, car + 0x40);
        swc1(gap, fpu_mul(cos_vu(heading), -coupling_gap()));
        swc1(gap + 4, fpu_mul(sin_vu(heading), -coupling_gap()));
        sw(gap + 8, 0);
        vec3_add(V(gap), V(gap), V(behind));
        swc1(length, fpu_mul(cos_vu(heading), -lwc1(cpv + 0xc4)));
        swc1(length + 4, fpu_mul(sin_vu(heading), -lwc1(cpv + 0xc4)));
        sw(length + 8, 0);
        vec3_add(V(car + 0x10), V(length), V(gap));
        swc1(car + 0x18, lwc1(m + 0x18));
        const float to_coupling = atan2_f(fpu_sub(lwc1(gap), lwc1(car + 0x10)), fpu_sub(lwc1(gap + 4), lwc1(car + 0x14)));
        sw(car + 0x44, 0);
        swc1(car + 0x48, angle_add(to_coupling, 0.0f));
        carriage_settle(car, cpv, behind, moved);
        car_index = lw(cpv + 0xc0);
    }
    sh(engine + 0x34, lhu(engine + 0x34) | 6u);
    sb(engine + 0x20, 0);
    ee_stack_free(0x120);
}

// The train under way: drawn (+0x72), the engine's speed (+0xfc) its gap +0xdc less the distance to it (not below 0) and
// the engine moved; the locomotive steps its speed +0xc8 along its heading and pitch and turns both toward the engine (rates
// from its speed against its top speed +0xc0); each carriage then follows its coupling: placed on the line from its own
// coupling point (chain point 0) toward the coupling ahead, the level's gap behind it, its length +0xc4 along it, facing it.
// Without carriages the locomotive is removed (a debug message in the original).
// orig: L3:0x00292890
void train_drive_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t start_pos = sp, start_ang = sp + 0x10, step = sp + 0x20, coupling = sp + 0x30;
    const uint32_t ahead = sp + 0x40, pull = sp + 0x50, own = sp + 0x60;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t engine = moby_at(lw(pv + 0xc4));
    const uint32_t epv = lwu(engine + 0x78);
    sb(m + 0x72, 0xff);
    lq_sq(start_pos, m + 0x10);
    lq_sq(start_ang, m + 0x40);
    int32_t car_index = lw(pv + 0xa0);
    if (car_index == -1) {
        moby_retire(m);  // (with a debug message naming the path +0xb8)
        ee_stack_free(0x160);
        return;
    }
    const float slack = fpu_sub(lwc1(pv + 0xdc), vec2_distance(V(m + 0x10), V(engine + 0x10)));
    swc1(epv + 0xfc, slack < 0.0f ? 0.0f : slack);
    jet_update_base_l1(engine);
    vec_from_spherical_v(V(step), lwc1(pv + 0xc8), angle_add(lwc1(m + 0x48), kPi), lwc1(m + 0x44));
    vec3_set_length(V(step), V(step), lwc1(pv + 0xc8));
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
    // the turn rates: the speed in 60ths of the top speed
    auto rate = [&] { return fpu_div(fpu_mul(lwc1(pv + 0xc8), 60.0f), lwc1(pv + 0xc0)); };
    {
        const float yaw_to = angle_add(atan2_f(fpu_sub(lwc1(engine + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(engine + 0x14), lwc1(m + 0x14))), kPi);
        const float r = rate();
        const float k = fpu_mul(fpu_mul(fpu_mul(r, 0.25f), kDegree), dt_sq());
        const float damp = fpu_mul(fpu_mul(fpu_mul(r, 0.5f), kDegree), dt_sq());
        swc1(m + 0x48, angle_seek_rate(pv + 0xcc, lwc1(m + 0x48), yaw_to, k, damp, fpu_mul(r, dt())));
    }
    {
        const float d = vec2_distance(V(m + 0x10), V(engine + 0x10));
        const float pitch_to = atan2_f(d, fpu_sub(lwc1(engine + 0x18), lwc1(m + 0x18)));
        const float r = rate();
        const float k = fpu_mul(fpu_mul(fpu_mul(r, 0.25f), kDegree), dt_sq());
        const float damp = fpu_mul(fpu_mul(fpu_mul(r, 0.5f), kDegree), dt_sq());
        swc1(m + 0x44, angle_seek_rate(pv + 0xd0, lwc1(m + 0x44), pitch_to, k, damp, fpu_mul(r, dt())));
    }
    moby_chain_point_v(m, 0, V(coupling));
    while (car_index != -1) {
        const uint32_t car = moby_at(car_index);
        const uint32_t cpv = lwu(car + 0x78);
        sb(car + 0x72, 0xff);
        moby_chain_point_v(car, 0, V(own));
        vec3_sub(V(ahead), V(own), V(coupling));
        vec2_set_length(V(ahead), V(ahead), coupling_gap());
        vec3_add(V(ahead), V(ahead), V(coupling));
        swc1(ahead + 8, lwc1(coupling + 8));
        vec3_sub(V(pull), V(ahead), V(car + 0x10));
        vec3_set_length(V(pull), V(pull), fpu_sub(vec3_length(V(pull)), lwc1(cpv + 0xc4)));
        vec3_add(V(car + 0x10), V(pull), V(car + 0x10));  // (the pull first: its w lane, the chain point's, is kept)
        swc1(car + 0x48, angle_add(atan2_f(fpu_sub(lwc1(ahead), lwc1(car + 0x10)), fpu_sub(lwc1(ahead + 4), lwc1(car + 0x14))), 0.0f));
        const float d = vec2_distance(V(car + 0x10), V(coupling));
        swc1(car + 0x44, -atan2_f(d, fpu_sub(lwc1(coupling + 8), lwc1(car + 0x18))));
        carriage_settle(car, cpv, coupling, step);
        car_index = lw(cpv + 0xc0);
    }
    vec3_sub(V(step), V(m + 0x10), V(start_pos));
    attach_frame_set(pv + 0x60, step, start_ang, m + 0x40);
    sh(engine + 0x34, lhu(engine + 0x34) | 6u);
    ee_stack_free(0x160);
}

// The train standing: drawn (+0x72 = 0x40), each carriage's attached frame from its move and its place and angles kept, the
// locomotive's too, the engine drawn. Without carriages the locomotive is removed (a debug message in the original).
// orig: L3:0x00292d10
void train_hold_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t start_pos = sp, start_ang = sp + 0x10, moved = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x72, 0x40);
    lq_sq(start_pos, m + 0x10);
    lq_sq(start_ang, m + 0x40);
    int32_t car_index = lw(pv + 0xa0);
    if (car_index == -1) {
        moby_retire(m);  // (with a debug message naming the path +0xb8)
        ee_stack_free(0xe0);
        return;
    }
    while (car_index != -1) {
        const uint32_t car = moby_at(car_index);
        const uint32_t cpv = lwu(car + 0x78);
        sb(car + 0x72, 0x40);
        vec3_sub(V(moved), V(car + 0x10), V(cpv + 0xa0));
        attach_frame_set(cpv + 0x20, moved, cpv + 0xb0, car + 0x40);
        lq_sq(cpv + 0xa0, car + 0x10);
        lq_sq(cpv + 0xb0, car + 0x40);
        car_index = lw(cpv + 0xc0);
    }
    vec3_sub(V(moved), V(m + 0x10), V(start_pos));
    attach_frame_set(pv + 0x60, moved, start_ang, m + 0x40);
    const uint32_t engine = moby_at(lw(pv + 0xc4));
    sh(engine + 0x34, lhu(engine + 0x34) | 6u);
    ee_stack_free(0xe0);
}

// Class 822, the train of the ride. Before its path +0xa4 is set it only resets its own pvars. Its state (+0xbc):
// 0 its set-up (its length to its coupling, its gap, its speed from the top speed, the leg +0xd4 of the return path +0xac
//   it stops at: the first point whose w is 1) -> 1;
// 1 put at the first path's start, the engine on that path at the top speed -> 2;
// 2 put at the station path's start, facing along it, the engine started on the first path -> 3;
// 3 waits at the station: when the hero (standing, or in states 9 / 12) is in the station's box and the camera +0x100 is
//   set, the ride starts: its music, the hero's talk word, the camera turned on the hero, the whistle's first wait -> 4;
// 4 rides: speeds up to the top speed, drives, its sounds; a hero who stays 60 frames 10 below the track ends the ride with a
//   fade (the screen's flag 0x141401); the camera follows the hero; the hero's talking moby (classes 822 / 1210's
//   0x336 / 0x4ba) gets word 3; once the hero stands on it, with the return path and the second camera set and that camera
//   free, the station's class 574s go to state 14 (the first not there yet is sent), the second camera is placed behind the
//   return path's start (its box) to look at it, the train put there at twice the speed -> 5 (the station's music);
// 5 drives back: slows down from the stop's leg on, and at speed 0 frees the camera, halves its top speed and stops its
//   sound -> 6; the camera keeps looking at the hero;
// 6 stands: when the hero is in the stop's box the station's class 574s in state 16 go to 17 and it starts over -> 1;
//   else it holds;
// 7 nothing.
// The flag OV(0x15fdfc) (the ride is on) is cleared each frame and set in states 3 (starting), 4 and 5.
// orig: L3:0x00292e98 (the jump table L3:0x1e2500 on the state +0xbc, in order)
void kerwan_class822_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0xb8, lhu(m + 0xa8));
    if (lw(pv + 0xa4) == -1) {
        sh(pv + 0x3e, 0xd);
        sb(pv + 0x28, 4);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        ee_stack_free(0xd0);
        return;
    }
    if (lw(pv + 0xc4) == -1) moby_retire(m);  // (with a debug message; the original goes on with the engine's index -1)
    const uint32_t engine = moby_at(lw(pv + 0xc4));
    const uint32_t epv = lwu(engine + 0x78);
    sw(OV(0x0015fdfc), 0);
    const uint32_t at = sp;
    auto station_574s_to_17 = [&] {
        for (uint32_t k = 0; k < 8; ++k) {
            const int32_t i = lw(pv + 0xe0 + k * 4);
            const uint32_t mob = moby_at(i);
            if (i == -1 || mob == 0) continue;
            const uint32_t st = lbu(mob + 0x20);
            if (st == 0xfe || st == 0xfd || lh(mob + 0xa6) != 0x23e) continue;
            if (st == 0x10) sb(mob + 0x20, 0x11);
        }
    };
    switch (lbu(m + 0xbc)) {
    case 0: {  // (0x00292f6c)
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sh(pv + 0x3e, 0xd);
        sb(pv + 0x28, 4);
        moby_chain_point_v(m, 0, V(at));
        const float length = vec2_distance(V(at), V(m + 0x10));
        const float speed = fpu_mul(lwc1(pv + 0xc0), dt());
        swc1(pv + 0xbc, length);
        swc1(pv + 0xdc, 20.0f);
        swc1(pv + 0xc8, speed);
        swc1(pv + 0xc0, speed);
        const int32_t back = lw(pv + 0xac);
        if (back != -1) {
            const uint32_t path = lvd::path(back);
            sw(pv + 0xd4, 0xffffffffu);
            for (int32_t i = 0; i < lw(path); ++i) {
                if (lwc1(path + static_cast<uint32_t>(i) * 16 + 0x1c) == 1.0f) {
                    sw(pv + 0xd4, static_cast<uint32_t>(i));
                    break;
                }
            }
        }
        sb(m + 0xbc, 1);
        sw(pv + 0x9c, 3);
        sh(engine + 0x34, lhu(engine + 0x34) | 6u);
        break;
    }
    case 1: {  // (0x00293078)
        lq_sq(at, lvd::path(lw(pv + 0xa4)) + 0x10);
        train_place_l3(m, at, 0.0f);
        sw(epv + 0x74, lwu(pv + 0xa4));
        swc1(epv + 0xfc, lwc1(pv + 0xc0));
        jet_update_base_l1(engine);
        sb(m + 0xbc, 2);
        break;
    }
    case 2: {  // (0x002930cc)
        const uint32_t station = lvd::path(lw(pv + 0xa8));
        lq_sq(at, station + 0x10);
        train_place_l3(m, at, first_leg_heading(station));
        sb(m + 0xbc, 3);
        sw(epv + 0x74, lwu(pv + 0xa4));
        sw(epv + 0x60, 1);
        sw(epv + 0x70, lvd::path(lw(pv + 0xa4)));
        sw(pv + 0xc8, 0);
        break;
    }
    case 3: {  // (0x00293158)
        train_hold_l3(m);
        if (point_in_box(HERO_POS, lw(pv + 0xb4)) == 0) break;
        if (lw(pv + 0x100) == -1) break;
        const uint32_t hero_state = lwu(0x001413dc);
        if (!(hero_state < 2 || hero_state == 9 || hero_state == 0xc)) break;
        sb(m + 0xbc, 4);
        sw(OV(0x0015fdfc), 1);
        music_track_request(4, 7);
        talker_word_set(m, 1);
        const uint32_t cam = moby_at(lw(pv + 0x100));
        sb(cam + 0xbc, 1);
        camera_look_at(lwu(cam + 0x78), HERO_POS);
        sw(lwu(cam + 0x78) + 0x2c, static_cast<uint32_t>(frame_scale_i(0x35c)));
        sw(pv + 0x104, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sw(pv + 0x108, 0xffffffffu);
        break;
    }
    case 4: {  // (0x00293280)
        const float top = lwc1(pv + 0xc0);
        const float speed = fpu_add(lwc1(pv + 0xc8), fpu_mul(fpu_div(top, 5.0f), dt()));
        swc1(pv + 0xc8, speed);
        if (top < speed) swc1(pv + 0xc8, top);
        sw(OV(0x0015fdfc), 1);
        train_drive_l3(m);
        train_sounds(m, pv);
        // the hero below the track (10 under its nearest point) for more than 60 frames: the ride ends with a fade
        bool below_counted = false;
        if (lh(0x0013f65e) != 0 && lw(0x001413dc) != 3) {
            int32_t idx = 0;
            float dist = 0.0f;
            path_closest_point_v(lwu(epv + 0x70), V(HERO_POS), V(at), &idx, &dist, 0, 999.0f, 5.0f, 0.0f);
            sw(pv + 0x10c, lwu(pv + 0x10c) + 1);
            below_counted = true;
            if (frame_scale_i(0x3c) < lw(pv + 0x10c) && lwc1(HERO_POS + 8) < fpu_sub(lwc1(at + 8), 10.0f)) {
                sound_release(m, pv + 0x108);
                screen_fade(frame_scale_i(0x10));
                sb(HERO + 0x20b1, 1);
                break;
            }
        }
        if (!below_counted) sw(pv + 0x10c, 0);
        {
            const uint32_t cam = moby_at(lw(pv + 0x100));
            if (lbu(cam + 0xbc) != 0) camera_look_at(lwu(cam + 0x78), HERO_POS);
        }
        const uint32_t talker = lwu(HERO + 0x2fc);
        if (talker != 0 && (lh(talker + 0xa6) == 0x336 || lh(talker + 0xa6) == 0x4ba)) talker_word_set(talker, 3);
        if (moby_hero_on_l1(m) == 0) break;
        if (lw(pv + 0xac) == -1 || lw(pv + 0xd8) == -1) break;
        if (lbu(moby_at(lw(pv + 0x100)) + 0xbc) != 0) break;
        // the station's class 574s: the first one alive not in state 14 is sent there
        for (uint32_t k = 0; k < 8; ++k) {
            const int32_t i = lw(pv + 0xe0 + k * 4);
            const uint32_t mob = moby_at(i);
            if (i == -1 || mob == 0) continue;
            const uint32_t st = lbu(mob + 0x20);
            if (st == 0xfe || st == 0xfd) continue;
            if (moby_is_in_state(mob, 0x23e, 0xe) == 0) {
                moby_state_hook(mob, 0xe);
                break;
            }
        }
        if (lw(pv + 0xd4) == -1) break;  // (a debug message in the original)
        // the return: the second camera behind the return path's start, looking at it; the train put there
        const uint32_t cam = moby_at(lw(pv + 0xd8));
        sb(cam + 0xbc, 1);
        const uint32_t back = lvd::path(lw(pv + 0xac));
        const uint32_t cpv = lwu(cam + 0x78);
        camera_look_at(cpv, back + 0x10);
        const float behind = fpu_add(lwc1(pv + 0xdc), 4.0f);
        const uint32_t box = lvd::box(lvd::BOXES, lw(cpv + 0x28));
        lvd::set_shape_f32(box + 0x30, fpu_mul(cos_vu(first_leg_heading(back)), -behind));
        lvd::set_shape_f32(box + 0x34, fpu_mul(sin_vu(first_leg_heading(back)), -behind));
        lvd::set_shape_word(box + 0x38, 0);
        {
            Vec4 at_back;
            vec3_add(at_back, V(box + 0x30), V(back + 0x10));
            lvd::set_shape_row(box + 0x30, at_back);
        }
        lvd::set_shape_f32(box + 0x38, fpu_add(lwc1(box + 0x38), u2f(0x402ccccdu)));
        lvd::set_shape_row(box + 0x70, Vec4{0.0f, 0.0f, 0.0f, 0.0f});
        lvd::set_shape_f32(box + 0x78, first_leg_heading(back));
        sw(cpv + 0x2c, static_cast<uint32_t>(frame_scale_i(0x2710)));
        lq_sq(at, back + 0x10);
        train_place_l3(m, at, first_leg_heading(back));
        sw(epv + 0x74, lwu(pv + 0xac));
        sw(epv + 0x60, 1);
        swc1(epv + 0xf0, 1.0f);
        const float top2 = fpu_add(lwc1(pv + 0xc0), lwc1(pv + 0xc0));
        swc1(pv + 0xc0, top2);
        swc1(epv + 0xfc, top2);
        jet_update_base_l1(engine);
        sb(m + 0xbc, 5);
        music_track_request(0, 8);
        break;
    }
    case 5: {  // (0x00293898)
        const uint32_t cpv = lwu(moby_at(lw(pv + 0xd8)) + 0x78);
        train_drive_l3(m);
        sw(OV(0x0015fdfc), 1);
        train_sounds(m, pv);
        if (!(lw(epv + 0x60) < lw(pv + 0xd4))) {
            sw(pv + 0xdc, 0);
            const float speed = fpu_sub(lwc1(pv + 0xc8), fpu_mul(fpu_div(lwc1(pv + 0xc0), u2f(0x3ff33333u)), dt()));
            swc1(pv + 0xc8, speed);
            if (speed < lwc1(pv + 0xdc)) swc1(pv + 0xc8, lwc1(pv + 0xdc));
            if (lwc1(pv + 0xc8) == lwc1(pv + 0xdc)) {
                sb(moby_at(lw(pv + 0xd8)) + 0xbc, 0);
                sb(m + 0xbc, 6);
                swc1(pv + 0xc0, fpu_mul(lwc1(pv + 0xc0), 0.5f));
                sound_release(m, pv + 0x108);
            }
        }
        camera_look_at(cpv, HERO_POS);
        break;
    }
    case 6: {  // (0x00293a68)
        if (point_in_box(HERO_POS, lw(pv + 0xb0)) == 0) {
            train_hold_l3(m);
            break;
        }
        station_574s_to_17();
        swc1(pv + 0xc8, lwc1(pv + 0xc0));
        swc1(pv + 0xdc, 20.0f);
        sb(m + 0xbc, 1);
        break;
    }
    default:  // 7 (0x00293b1c) and beyond the table
        break;
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
