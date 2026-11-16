// Batalia (level 8) own moby classes, hand-ported: class 424, the Shuttle (the green dropship of the arrival scene: it
// hovers, bobbing, at the end of its paths; Ratchet standing on it is offered the flight, and the button sends it along
// one of its three paths), and its helpers.
// Header batalia_own.h; registered in batalia_mobys.cpp. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR,
// the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x2fc the moby he stands on)
constexpr uint32_t HERO_POS = PLAYER + 0x80;

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// a path's point `i` (the points from +0x10, 0x10 each)
uint32_t path_pt(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the shuttle shown or hidden (+0x31, +0x34 bit 0)
void shuttle_show(uint32_t m, bool on) {
    const uint32_t flags = lhu(m + 0x34);
    sb(m + 0x31, on ? 1 : 0);
    sh(m + 0x34, on ? (flags & 0xfffeu) : (flags | 1u));
}

// the shuttle at the path point `at` of `path`, heading to the point `toward`; the path it rides +0x120
void shuttle_start_at(uint32_t pv, uint32_t path, uint32_t at, uint32_t toward) {
    lq_sq(pv + 0x60, at);
    swc1(pv + 0x78, atan2_f(fpu_sub(lwc1(toward), lwc1(pv + 0x60)), fpu_sub(lwc1(toward + 4), lwc1(pv + 0x64))));
    sw(pv + 0x120, path);
}
}  // namespace

// The shuttle's eight corners (the level's 0x1db280, through its rotation, at its place) written as the points of its own
// path (+0x8c), when that path has eight (Ratchet's push-out keeps him on it).
// orig: L8:0x002dc0c8
void shuttle_corners_l8(uint32_t m) {
    const uint32_t path = lvd::path(lw(lwu(m + 0x78) + 0x8c));
    if (lw(path) != 8) return;
    for (int32_t k = 0; k < 8; ++k) {
        const uint32_t pt = path_pt(path, k);
        mat3_transform(V(pt), V(L8_ADDR(0x001db280) + static_cast<uint32_t>(k) * 0x10), M(m + 0xc0));
        vec3_add(V(pt), V(pt), V(m + 0x10));
    }
}

// The shuttle brought to the end of its paths nearest Ratchet (+0x100 which: 0 the end of the path +0x88, 1 the start of
// +0x80, 3 the start of +0x84): when that changes, it is shown with its model, its ride's fraction +0xe4 0 toward 1
// (+0x104), at that path's end heading along it. From the end of +0x88 (0) it rides that path back to its start (+0x104
// 0) on the path +0x84 once the checkpoint +0x98 is taken, else on +0x80 once +0x94 is, else it is hidden while Ratchet
// is more than 32 away or within the level's first 5 frames.
// orig: L8:0x002dc180
void shuttle_to_nearest_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t first = lvd::path(lw(pv + 0x80)), second = lvd::path(lw(pv + 0x84)), back = lvd::path(lw(pv + 0x88));
    float best = vec3_distance(V(HERO_POS), V(path_pt(back, lw(back) - 1)));
    const float d_first = vec3_distance(V(HERO_POS), V(path_pt(first, 0)));
    const float d_second = vec3_distance(V(HERO_POS), V(path_pt(second, 0)));
    int32_t end = 0;
    if (d_first < best) {
        best = d_first;
        end = 1;
    }
    if (d_second < best) end = 3;
    if (lw(pv + 0x100) == end) return;
    sw(pv + 0x100, static_cast<uint32_t>(end));
    sw(pv + 0xe4, 0);
    swc1(pv + 0x104, 1.0f);
    shuttle_show(m, true);
    sw(pv + 0x124, 1);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    switch (end) {
    case 1:
        shuttle_start_at(pv, first, path_pt(first, 0), path_pt(first, 1));
        break;
    case 3:
        shuttle_start_at(pv, second, path_pt(second, 0), path_pt(second, 1));
        break;
    case 0: {
        // at the far end of the way back: it rides it back toward its start
        const int32_t n = lw(back);
        lq_sq(pv + 0x60, path_pt(back, n - 1));
        const uint32_t prev = path_pt(back, n - 2);
        swc1(pv + 0x78, atan2_f(fpu_sub(lwc1(prev), lwc1(pv + 0x60)), fpu_sub(lwc1(prev + 4), lwc1(pv + 0x64))));
        const uint32_t row = 0x0014c050 + (lwu(0x0015ed84) << 4);
        const bool first_cp = lbu(row + lwu(pv + 0x94)) == 0xff, second_cp = lbu(row + lwu(pv + 0x98)) == 0xff;
        swc1(pv + 0xe4, 1.0f);
        sw(pv + 0x104, 0);
        if (second_cp) {
            sw(pv + 0x120, second);
        } else if (first_cp) {
            sw(pv + 0x120, first);
        } else if (32.0f < vec2_distance(V(m + 0x10), V(HERO_POS)) || lw(L8_ADDR(0x0015f5cc)) < 5) {
            shuttle_show(m, false);
            sw(pv + 0x124, 0);
            sw(m + 0x94, 0);
        }
        break;
    }
    default:
        break;
    }
}

// The shuttle's ride: its fraction +0xe4 seeks its goal +0x104 (rate +0xe8; 0.1 of the frame time's square, at most 0.08
// a frame), its spin +0x110 by 50 times the rate; its place +0x60 / +0x70 the point of its path +0x120 at that fraction;
// near the goal (within 0.2) its turn +0x108 seeks pi (2.0944 / 6.2832 a second), else it is 0; riding back (goal 0) its
// heading +0x78 is turned by pi each frame; the heading turned by +0x108 is its heading. True once it is there.
// orig: L8:0x002dc648
bool shuttle_ride_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float accel = fpu_mul(dt_sq(), u2f(0x3dcccccdu));  // (0.1)
    const uint32_t path = lwu(pv + 0x120);
    seek_1d(pv + 0xe4, pv + 0xe8, lwc1(pv + 0x104), accel, accel, fpu_mul(dt(), u2f(0x3da3d70au)));  // (0.08)
    swc1(pv + 0x110, angle_add(lwc1(pv + 0x110), fpu_mul(lwc1(pv + 0xe8), 50.0f)));
    path_point(fpu_mul(lwc1(pv + 0xe4), static_cast<float>(lw(path) - 1)), path, 0, pv + 0x60, pv + 0x70, 0);
    sw(pv + 0x74, 0);
    if (abs_f(fpu_sub(lwc1(pv + 0xe4), lwc1(pv + 0x104))) < u2f(0x3e4ccccdu)) {  // (0.2)
        const float turn = fpu_mul(dt_sq(), u2f(0x40060a92u));                   // (2.0944: 2 pi / 3)
        seek_1d(pv + 0x108, pv + 0x10c, u2f(0x40490fd0u), turn, turn, fpu_mul(dt(), u2f(0x40c90fdbu)));  // (3.14159, 2 pi)
    } else {
        sw(pv + 0x108, 0);
    }
    if (lwc1(pv + 0x104) == 0.0f) swc1(pv + 0x78, angle_add(lwc1(pv + 0x78), u2f(0x40490fd0u)));
    swc1(pv + 0x78, angle_add(lwc1(pv + 0x78), lwc1(pv + 0x108)));  // (stored whether or not it is there: a delay slot)
    return lwc1(pv + 0xe4) == lwc1(pv + 0x104);
}

// The help line of the shuttle Ratchet stands on (owner 10): where the button takes it (TXT 0x1f4d before its ride back,
// 0x1f50 on the path +0x80, 0x1f4f on +0x88, 0x1f4e on +0x84); the request's answer.
// orig: L8:0x002dc7f0
int32_t shuttle_help_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t text;
    if (u2f(0x3f666666u) < lwc1(pv + 0x104)) text = 0x1f4d;  // (0.9)
    else if (lwu(pv + 0x120) == lvd::path(lw(pv + 0x80))) text = 0x1f50;
    else if (lwu(pv + 0x120) == lvd::path(lw(pv + 0x88))) text = 0x1f4f;
    else text = 0x1f4e;
    return hud_message_request(0xa, TXT(text));
}

// Class 424, the Shuttle. pvars: its attach frame +0x20 (what stands on it moves with it), its place +0x60 / rotation +0x70
// on its path, its heading +0x78, its three paths +0x80 / +0x84 / +0x88 and its own (+0x8c: its corners, -1 none), the
// checkpoints +0x94 / +0x98, its sparkle node +0xa0 (+0xa1 set: none), its tilt +0xb0 (from the spin +0x110), the end it
// is at +0x100, its ride +0xe4 (rate +0xe8, goal +0x104, turn +0x108 / +0x10c), the path it rides +0x120, shown +0x124.
// States: 0 set up (with no paths +0x80 -1: parked where it is, 4; else 2 at the end nearest Ratchet); 1 / 4 and above
// hover; 2 waits at its end for Ratchet on it (the help line, the push-out on its path +0x8c, the button 0x13cae4 bit 4
// while the help line is his: 3), elsewhere follows him to the nearest end; 3 rides (Ratchet held: +0x1f2 / +0x1f4 5, his
// path +0x22da), then back to 2 at the new end. Hidden in the cutscenes (0x15f5c4 2) and while +0x124 is 0. Every frame
// it bobs (floater_bob, its phases 0xec), its corners follow it and its attach frame takes its move and turn.
// orig: L8:0x002dbdb0
void bat_class424_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t old_pos = sp, old_rot = sp + 0x10, moved = sp + 0x20, push = sp + 0x30;
    lq_sq(old_pos, m + 0x10);
    lq_sq(old_rot, m + 0x40);
    shuttle_show(m, lw(L8_ADDR(0x0015f5c4)) != 2 && lw(pv + 0x124) != 0);
    if (lbu(pv + 0xa1) == 0) sub_0024f590(m, 0, pv + 0xa0);
    swc1(pv + 0x110, angle_add(lwc1(pv + 0x110), fpu_mul(dt(), u2f(0x3fc90fdbu))));  // (pi / 2 a second)
    quat_from_euler(pv + 0xb0, pv + 0x110);
    switch (lbu(m + 0x20)) {
    case 0:
        vec_clear(V(pv + 0x110));
        if (lw(pv + 0x80) == -1) {
            lq_sq(pv + 0x60, m + 0x10);
            lq_sq(pv + 0x70, m + 0x40);
            sb(m + 0x20, 4);
            sw(pv + 0x124, 1);
            break;
        }
        sh(pv + 0xe0, 0xffffu);
        sw(pv + 0x100, 0xffffffffu);
        shuttle_to_nearest_l8(m);
        sb(m + 0x20, 2);
        sb(m + 0x30, 0xff);
        break;
    case 2: {
        if (lwu(PLAYER + 0x2fc) != m) {
            shuttle_to_nearest_l8(m);
            break;
        }
        if (lw(L8_ADDR(0x00179c10)) != 0 || lw(L8_ADDR(0x00179c34)) != -1) break;  // (a voice line playing)
        lq_sq(push, HERO_POS);
        path_push_out_2d(lwu(pv + 0x8c), push, push, 0.5f);
        if (!(vec3_distance(V(HERO_POS), V(push)) < u2f(0x3a83126fu))) break;  // (on it: within 0.001)
        shuttle_help_l8(m);
        if ((lwu(0x0013cae4) & 0x10u) == 0 || lh(0x0013f65e) != 0) break;
        if (lw(L8_ADDR(0x0015f594)) == 10) sb(m + 0x20, 3);
        break;
    }
    case 3:
        sh(0x0013f544, 5);
        sh(0x0013f542, 5);
        sh(PLAYER + 0x22da, lhu(pv + 0x8c));
        if (shuttle_ride_l8(m)) {
            shuttle_to_nearest_l8(m);
            sb(m + 0x20, 2);
        }
        break;
    default:
        break;
    }
    floater_bob(m, 0xec);
    if (lw(pv + 0x8c) != -1) shuttle_corners_l8(m);
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x20, moved, old_rot, m + 0x40);
    ee_stack_free(0xd0);
}

}  // namespace rac1
