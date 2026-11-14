// Aridia's own moby classes: the transport pod that carries the hero along a path between two stations, its doors and
// its camera, hand-ported from Aridia's overlay (`orig: L2:`). Aridia-only data is L2_ADDR, the engine's through OV() and
// the level-data facade; the base ELF's (the hero 0x13f350, the game record 0x141968, the switches 0x14c050) are bare.
// Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l2.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position)

float dt() { return lwc1(0x0015ed6c); }
float dt2() { return lwc1(0x0015ed70); }

// the moby at index `i` of the moby table
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the doors' yaws: the pod's, turned by how far each door stands above it (the level's 0x161c3c degrees a unit)
void doors_turn(uint32_t m, uint32_t pv) {
    const float deg = u2f(0x3c8efa35u);
    for (const uint32_t d : {0x84u, 0x88u}) {
        const float off = fpu_mul(fpu_mul(fpu_sub(lwc1(lwu(pv + d) + 0x18), lwc1(m + 0x18)), lwc1(L2_ADDR(0x00161c3c))), deg);
        swc1(lwu(pv + d) + 0x48, angle_add(lwc1(m + 0x48), off));
    }
    moby_update_transform(lwu(pv + 0x84));
    moby_update_transform(lwu(pv + 0x88));
}

// both doors at their heights (pvar +0x94 / +0x98 above the pod) within eps: `off` is minus the height (the original
// subtracts a positive height or adds a negative one: the same IEEE operation)
bool doors_at(uint32_t m, uint32_t pv, float off1, float off2, float eps) {
    if (!(abs_f(fpu_add(fpu_sub(lwc1(lwu(pv + 0x84) + 0x18), lwc1(m + 0x18)), off1)) < eps)) return false;
    return abs_f(fpu_add(fpu_sub(lwc1(lwu(pv + 0x88) + 0x18), lwc1(m + 0x18)), off2)) < eps;
}

// the two doors (classes 0x421 / 0x422, pvar +0x84 / +0x88), the pod's look and place
void doors_spawn(uint32_t m, uint32_t pv) {
    for (const auto& [cls, slot] : {std::pair<uint32_t, uint32_t>{0x421u, 0x84u}, {0x422u, 0x88u}}) {
        sw(pv + slot, moby_spawn(cls));
        sh(lwu(pv + slot) + 0x32, 0x40);
        sb(lwu(pv + slot) + 0x31, 1);
        sd(lwu(pv + slot) + 0x38, ld(m + 0x38));
        sh(lwu(pv + slot) + 0x34, lhu(m + 0x34));
        lq_sq(lwu(pv + slot) + 0x10, m + 0x10);
        lq_sq(lwu(pv + slot) + 0x40, m + 0x40);
    }
}

// the station box `slot` (pvar +0x60 the arrival, +0x64 the start)
uint32_t station(uint32_t pv, uint32_t slot) { return lvd::box(lvd::BOXES, lw(pv + slot)); }

// the pod's speed toward the station's height `goal` (the level's 0x161c30 / 0x161c34)
void pod_seek(uint32_t m, uint32_t pv, float goal) {
    const float acc = fpu_mul(lwc1(L2_ADDR(0x00161c34)), dt2());
    seek_1d(m + 0x18, pv + 0x9c, goal, acc, acc, fpu_mul(lwc1(L2_ADDR(0x00161c30)), dt()));
}

// the camera's look from `at` toward `from` (EE vectors) as the class-5 moby's angles: its yaw and pitch at rot (+8 / +4)
void look_from(uint32_t from, uint32_t at, uint32_t rot) {
    vec_clear(V(rot));
    swc1(rot + 8, atan2_f(fpu_sub(lwc1(from), lwc1(at)), fpu_sub(lwc1(from + 4), lwc1(at + 4))));
    swc1(rot + 4, -atan2_f(vec2_distance(V(at), V(from)), fpu_sub(lwc1(from + 8), lwc1(at + 8))));
}
}  // namespace

// The doors' heights moved toward their goals (the pod's height plus pvar +0x94 / +0x98; the speeds +0x8c / +0x90 and the
// level's 0x161c30 .. 0x161c38), then turned.
// orig: L2:0x002def80
void pod_doors_seek_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (const auto& [d, h, v] : {std::tuple<uint32_t, uint32_t, uint32_t>{0x84u, 0x94u, 0x8cu}, {0x88u, 0x98u, 0x90u}}) {
        const float goal = fpu_add(lwc1(m + 0x18), lwc1(pv + h));
        const float acc = fpu_mul(lwc1(L2_ADDR(0x00161c34)), dt2());
        const float dec = fpu_mul(lwc1(L2_ADDR(0x00161c38)), dt2());
        seek_1d(lwu(pv + d) + 0x18, pv + v, goal, acc, dec, fpu_mul(lwc1(L2_ADDR(0x00161c30)), dt()));
    }
    doors_turn(m, pv);
}

// The doors put at the pod (at their heights +0x94 / +0x98 above it, still), then turned.
// orig: L2:0x002df0b0
void pod_doors_snap_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t d1 = lwu(pv + 0x84);
    lq_sq(d1 + 0x10, m + 0x10);
    lq_sq(lwu(pv + 0x88) + 0x10, m + 0x10);
    swc1(d1 + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x94)));
    sw(pv + 0x8c, 0);
    swc1(lwu(pv + 0x88) + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x98)));
    sw(pv + 0x90, 0);
    doors_turn(m, pv);
}

// The hero let out of the pod: standing (state 0) when he barely moves (0x13f4b0 under 2.7 a frame), else landing (3).
// orig: L2:0x00237478
void pod_hero_out_l2() {
    if (lwc1(0x0013f4b0) < fpu_mul(dt(), u2f(0x402ccccdu))) hero_set_state_l1(0, 1);
    else hero_set_state_l1(3, 1);
}

// The current camera (0x167400)'s mode `mode` (+0x88): 2 / 3 set its time +0xe0 and the type record's +4 to `t`; 3 also
// aims it at the hero from where it is (sub_001ed630).
// orig: L2:0x002f8a18
void pod_camera_mode_l2(uint32_t mode, uint32_t t) {
    const uint32_t cam = lwu(L2_ADDR(0x00167400));
    const uint32_t rec = lwu(cam + 0x70);
    sb(cam + 0x88, mode);
    if (mode != 2 && mode != 3) return;
    if (mode == 3) {
        lq_sq(rec + 0x110, HERO + 0x80);
        sub_001ed630(rec + 0xfc, L2_ADDR(0x001673c0), HERO + 0x80);
    }
    sw(rec + 0xe4, t);
    sw(rec + 0xe0, t);
}

// The transport pod (pvars: the path +0x68 with its position +0xa4 / speed +0xa8 / end +0xa0 / rates +0xb0 / +0xb4, the
// stations' boxes +0x60 / +0x64 (the one it is at: +0xbc), the four gate mobys +0x70 .. +0x7c, the attachment record +0x20).
// States (the original's jump table at L2:0x1faa30): 0 set up (at the start, 6 under its floor, its doors), 1 waits for its
// switch, 2 opens the gates (their mobys slid along their facing for 30 frames), 3 rises (sound 0 at the top), 4 opens its
// doors, 5 waits: the hero near the other station calls it there (sunk, 3); the hero in it (and not in a vehicle) starts
// the ride (6: the camera scripted 240 frames, its angles the level's 0x161c48 .., the path's end the other station), 6
// asks (sub_00233f38: 2 rides (7, the hero in state 0x72), 0 or 180 frames out: the hero out, 0xd), 7 closes the doors, 8
// sinks and jumps to the path (9; the hero placed in the pod, the camera mode 2), 9 rides the path (to 0xa), 0xa rises at
// the arrival, 0xb opens the doors (dialogue 0x7dd after an hour of play once), the hero out (0xd), 0xc rises back, 0xd
// waits for the hero to leave it (5), 0xe .. 0x11 a second build (doors spawned, opened while 0x161c40 is set). Every
// frame what stands in it moves with it (the frame's move, at most 4).
// orig: L2:0x002ddc88
void transport_pod_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t moved = sp, turn0 = sp + 0x10;
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(turn0, m + 0x40);
    if (!(lwc1(L2_ADDR(0x00161c44)) == 0.0f)) {  // (the level's debug flag: the path's points shown)
        for (int32_t i = 0; i < lw(lvd::path(lw(pv + 0x68))); ++i) {
            lq_sq(sp + 0x20, lvd::path(lw(pv + 0x68)) + static_cast<uint32_t>(i << 4) + 0x10);
            debug_particle_l1(sp + 0x20, m, 4, 0x800000ffu, 0x7f, 2, 0xff, u2f(0x3dcccccdu));
        }
    }
    auto open_test = [&](float off1, float off2, float eps, uint32_t next) {
        if (doors_at(m, pv, off1, off2, eps)) sb(m + 0x20, next);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002dddd8)
        sb(m + 0x30, 0xff);
        sb(m + 0xbc, 1);
        lq_sq(m + 0x10, station(pv, 0x64) + 0x30);
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 6.0f));
        doors_spawn(m, pv);
        swc1(pv + 0x98, -1.0f);
        swc1(pv + 0x94, -1.0f);
        const uint32_t path = lvd::path(lw(pv + 0x68));
        const float a = fpu_mul(dt2(), u2f(0x3dcccccdu));
        const float b = fpu_mul(dt(), u2f(0x3e99999au));
        swc1(pv + 0xb4, fpu_mul(a, static_cast<float>(lw(path))));
        swc1(pv + 0xb0, fpu_mul(b, static_cast<float>(lw(path))));
        pod_doors_snap_l2(m);
        break;
    }
    case 1:  // (0x002ddf5c)
        if (lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) {
            sw(pv + 0x80, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sb(m + 0x20, 2);
        }
        break;
    case 2: {  // (0x002ddfa4)
        const float step = fpu_mul(dt(), 5.0f);
        const float back = -step;
        const uint32_t d = sp + 0x20;
        for (const auto& [slot, len] : {std::pair<uint32_t, float>{0x70u, back}, {0x74u, step}, {0x78u, back}, {0x7cu, step}}) {
            vec3_set_length(V(d), V(moby_at(lw(pv + slot)) + 0xc0), len);
            vec3_add(V(moby_at(lw(pv + slot)) + 0x10), V(moby_at(lw(pv + slot)) + 0x10), V(d));
        }
        for (const uint32_t slot : {0x70u, 0x74u, 0x78u, 0x7cu}) moby_update_transform(moby_at(lw(pv + slot)));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x80))) == 0) break;
        sb(m + 0x20, 3);
        moby_sound_start(1, 0, m);
        break;
    }
    case 3:  // (0x002de168)
    case 10: {  // (0x002dea4c)
        const bool rising = lbu(m + 0x20) == 3;
        pod_seek(m, pv, lwc1(station(pv, 0x60) + 0x38));
        pod_doors_snap_l2(m);
        const float top = lwc1(station(pv, 0x60) + 0x38);
        if (!((rising ? fpu_sub(top, u2f(0x38d1b717u)) : top) <= lwc1(m + 0x18))) break;
        sb(m + 0x20, rising ? 4u : 0xbu);
        moby_sound_start(0, 0, m);
        break;
    }
    case 4:  // (0x002de204)
        swc1(pv + 0x94, 2.0f);
        swc1(pv + 0x98, -4.0f);
        pod_doors_seek_l2(m);
        open_test(-2.0f, 4.0f, u2f(0x38d1b717u), 5);
        break;
    case 5: {  // (0x002de298)
        // the hero near the other station: the pod goes there (sunk, rising: 3)
        auto call_to = [&](uint32_t slot, uint32_t at) {
            lq_sq(m + 0x10, station(pv, slot) + 0x30);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 6.0f));
            swc1(pv + 0x98, -1.0f);
            swc1(pv + 0x94, -1.0f);
            pod_doors_snap_l2(m);
            sb(m + 0xbc, at);
            sb(m + 0x20, 3);
            moby_sound_start(1, 0, m);
        };
        if (lbu(m + 0xbc) == 0) {
            if (vec2_distance(V(HERO + 0x80), V(station(pv, 0x64) + 0x30)) < 16.0f) {
                call_to(0x64, 1);
                break;
            }
        } else if (lbu(m + 0xbc) == 1) {
            if (vec2_distance(V(HERO + 0x80), V(station(pv, 0x60) + 0x30)) < 16.0f) {
                call_to(0x60, 0);
                break;
            }
        }
        if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30e) != 0 || lbu(HERO + 0x20a4) != 0) break;
        // the ride: the path from this station's end, the camera scripted from above the pod toward the path's start
        const uint32_t from = sp + 0x30, to = sp + 0x20, at = sp + 0x40, rot = sp + 0x50;
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), 2.0f));
        float start;
        if (lbu(m + 0xbc) == 0) {
            sw(pv + 0xa0, 0);
            swc1(pv + 0xa4, static_cast<float>(lw(lvd::path(lw(pv + 0x68))) - 1));
            lq_sq(to, station(pv, 0x60) + 0x70);
            start = lwc1(pv + 0xa4);
        } else {
            sw(pv + 0xa4, 0);
            sw(pv + 0xa0, static_cast<uint32_t>(lw(lvd::path(lw(pv + 0x68))) - 1));
            lq_sq(to, station(pv, 0x64) + 0x70);
            start = lwc1(pv + 0xa4);
        }
        const int32_t i = float_to_int(start);
        lq_sq(at, lvd::path(lw(pv + 0x68)) + static_cast<uint32_t>(i << 4) + 0x10);
        look_from(from, at, rot);
        sb(m + 0x20, 6);
        sw(pv + 0xac, static_cast<uint32_t>(frame_scale_i(0xb4)));
        hero_cam_target_set(m + 0x10, lwc1(to + 8), 0);
        camera_scripted_set(L2_ADDR(0x001673c0), L2_ADDR(0x001673d0), 3, static_cast<uint32_t>(frame_scale_i(0xf0)), 0);
        camera_angles_set(lwc1(L2_ADDR(0x00161c48)), lwc1(L2_ADDR(0x00161c4c)), lwc1(L2_ADDR(0x00161c50)), lwc1(L2_ADDR(0x00161c54)),
                          lwc1(L2_ADDR(0x00161c58)), lwc1(L2_ADDR(0x00161c60)));
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(rot);
        break;
    }
    case 6: {  // (0x002de630)
        const int32_t r = sub_00233f38();
        if (r == 2) {
            sb(m + 0x20, 7);
            moby_sound_start(2, 0, m);
            hero_set_state_l1(0x72, 0);
        } else if (r == 0) {
            hydro_2eac18(2);
            sb(m + 0x20, 0xd);
        } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xac))) != 0) {
            hydro_2eac18(2);
            sb(m + 0x20, 0xd);
            pod_hero_out_l2();
        }
        break;
    }
    case 7:  // (0x002de6c4)
        swc1(pv + 0x98, -1.0f);
        swc1(pv + 0x94, -1.0f);
        pod_doors_seek_l2(m);
        open_test(1.0f, 1.0f, u2f(0x3c23d70au), 8);
        break;
    case 8: {  // (0x002de750)
        pod_seek(m, pv, fpu_sub(lwc1(station(pv, 0x60) + 0x38), 6.0f));
        pod_doors_snap_l2(m);
        if (!(lwc1(m + 0x18) < fpu_sub(lwc1(station(pv, 0x60) + 0x38), u2f(0x40bff7cfu)))) break;
        uint32_t here, other;
        if (lbu(m + 0xbc) == 0) {
            sb(m + 0xbc, 1);
            here = 0x64;
            other = 0x60;
        } else {
            sb(m + 0xbc, 0);
            here = 0x60;
            other = 0x64;
        }
        const uint32_t shift = sp + 0x20, place = sp + 0x30, facing = sp + 0x40;
        lq_sq(m + 0x10, station(pv, here) + 0x30);
        lq_sq(facing, station(pv, here) + 0x70);
        vec3_sub(V(shift), V(station(pv, here) + 0x30), V(station(pv, other) + 0x30));
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 6.0f));
        vec3_add(V(place), V(HERO + 0x80), V(shift));
        hero_place(place, facing, 0x72, 0);
        swc1(pv + 0x9c, -lwc1(pv + 0x9c));
        sb(m + 0x20, 9);
        pod_camera_mode_l2(2, static_cast<uint32_t>(frame_scale_i(0xb4)));
        break;
    }
    case 9: {  // (0x002de910)
        const float rate = lwc1(pv + 0xb4);
        seek_1d(pv + 0xa4, pv + 0xa8, static_cast<float>(lw(pv + 0xa0)), rate, rate, lwc1(pv + 0xb0));
        const int32_t end = lw(pv + 0xa0);
        if (end != 0 ? static_cast<float>(end) <= lwc1(pv + 0xa4) : lwc1(pv + 0xa4) <= static_cast<float>(end)) sb(m + 0x20, 0xa);
        const uint32_t from = sp + 0x20, rot = sp + 0x30, at = sp + 0x40;
        path_point(lwc1(pv + 0xa4), lvd::path(lw(pv + 0x68)), 0, at, rot, 0);
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), 8.0f));
        look_from(from, at, rot);
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(rot);
        break;
    }
    case 11: {  // (0x002deaec)
        swc1(pv + 0x94, 2.0f);
        swc1(pv + 0x98, -4.0f);
        pod_doors_seek_l2(m);
        const float eps = u2f(0x3c23d70au);
        if (!doors_at(m, pv, -2.0f, 4.0f, eps)) break;
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(0x00141892)) * 0x258;
        const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0xe10)), 60.0f))));
        if (limit < since && (lwu(0x00141bac) & (1u << (lwu(0x0015ed84) & 31))) == 0) sub_00203908(0x7dd, 0x48);
        sb(m + 0x20, 0xd);
        hydro_2eac18(2);
        hero_set_state_l1(0, 1);
        break;
    }
    case 12:  // (0x002dec20)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(dt(), 4.0f)));
        if (lwc1(station(pv, 0x60) + 0x38) <= lwc1(m + 0x18)) sb(m + 0x20, 5);
        break;
    case 13:  // (0x002dec74)
        if (lwu(HERO + 0x2fc) != m) sb(m + 0x20, 5);
        break;
    case 14:  // (0x002dec90)
        doors_spawn(m, pv);
        sb(m + 0x20, 0xf);
        break;
    case 15:  // (0x002ded4c)
        swc1(pv + 0x94, lwc1(L2_ADDR(0x00161c20)));
        swc1(pv + 0x98, lwc1(L2_ADDR(0x00161c24)));
        pod_doors_seek_l2(m);
        if (!(lwc1(L2_ADDR(0x00161c40)) == 0.0f)) sb(m + 0x20, 0x10);
        break;
    case 16:  // (0x002ded90)
        swc1(pv + 0x94, 2.0f);
        swc1(pv + 0x98, -4.0f);
        pod_doors_seek_l2(m);
        if (lwc1(L2_ADDR(0x00161c40)) == 0.0f) sb(m + 0x20, 0xf);
        else open_test(-2.0f, 4.0f, u2f(0x3c23d70au), 0x11);
        break;
    case 17:  // (0x002dee40)
        swc1(pv + 0x98, -1.0f);
        swc1(pv + 0x94, -1.0f);
        pod_doors_seek_l2(m);
        if (lwc1(L2_ADDR(0x00161c40)) == 0.0f) sb(m + 0x20, 0xf);
        else open_test(1.0f, 1.0f, u2f(0x3c23d70au), 0x10);
        break;
    default:  // past the table
        break;
    }
    // what stands in it moved with it: the frame's move (none beyond 4: a jump to the path), the turn
    vec3_add(V(moved), V(moved), V(m + 0x10));
    if (4.0f < vec3_length(V(moved))) vec_clear(V(moved));
    attach_frame_set(pv + 0x20, moved, turn0, m + 0x40);
    ee_stack_free(0x110);
}

}  // namespace rac1
