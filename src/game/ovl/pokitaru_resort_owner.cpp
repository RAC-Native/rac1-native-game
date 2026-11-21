// Pokitaru's (level 11) resort owner (class 114), ported by hand from the level's own code (L11:0x2d0fa8 and its
// helpers): he waits at his place (the box pvar +0xe4), talks, follows the hero through the resort's zones (paths
// pvar +0x60 .. as polygons, their walls +0x70 .., the doors +0x80 ..) while no enemy of the zone (the groups pvar
// +0x90 ..) is near, has the hero carry the switches (+0xf0 / +0xf4) to their places, opens the gate (class 65, +0x104)
// and at the end gives the O2 Mask (item 6) and asks for the level change. On a PAL 2.00 disc the scene's walk-to
// flag is the 1.02 build's byte (see pok_2d14b0_l11). The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_path_links.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

namespace {
constexpr uint32_t kHeroPos = 0x0013f3d0;  // the hero's position (the hero's block 0x13f350 + 0x80)

uint32_t moby_at(uint32_t index) { return lwu(L11_ADDR(0x0015ffd8u)) + (index << 8); }  // the level's moby `index`
uint32_t pv_moby(uint32_t pv, uint32_t off) { return moby_at(lwu(pv + off)); }          // the moby a pvar word names
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
bool collected(uint32_t index) {          // the level's checkpoint `index` (0x14c050's row of the level) taken (0xff)
    return lbu(0x0014c050 + index + (static_cast<uint32_t>(lw(0x0015ed84)) << 4)) == 0xff;
}
void anim_to(uint32_t m, uint32_t set) {  // the animation set `set` unless it plays, blended over 20 frames
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0x14));
}
// the zone he is in (pvar +0x158) and the path of its polygon / walls
uint32_t zone(uint32_t pv) { return lwu(pv + 0x158); }
uint32_t zone_polygon(uint32_t pv) { return lvd::path(lw(pv + 0x60 + (zone(pv) << 2))); }
}  // namespace

// The gate `gate` (class 65) waiting closed (state 1) opens (2), its twin (pvar +8) too; 0x13d3e2 noted.
// orig: L11:0x002cb990
void pok_2cb990_l11(uint32_t gate) {
    const uint32_t pv = lwu(gate + 0x78);
    if (lh(gate + 0xa6) != 0x41 || lbu(gate + 0x20) != 1) return;
    sb(0x0013d3e2, 1);
    sb(gate + 0x20, 2);
    const uint32_t twin = lwu(pv + 8);
    if (twin != 0) sb(twin + 0x20, 2);
}

// The switch `m` (state 1) held by the hero (0x13f64c) while he is not busy (0x13f65e): it is let go (state 2, back to
// its animation 0). True when it was.
// orig: L11:0x0030a480
bool pok_30a480_l11(uint32_t m) {
    if (lbu(m + 0x20) != 1) return false;
    const uint32_t held = lwu(0x0013f64c);
    if (held != m || lh(0x0013f65e) != 0) return false;
    sb(held + 0x20, 2);
    if (lbu(held + 0x53) != 0) moby_anim_set(held, 0, 0, frame_scale_i(0x14));
    return true;
}

// The switch `m`'s pvar word +0xb8 set (it was placed).
// orig: L11:0x0030a840
static void pok_30a840_l11(uint32_t m) { sw(lwu(m + 0x78) + 0xb8, 1); }

// The moves of his states: walking (4 .. 6, 8, 10) he turns toward his goal (pvar +0x120) and walks at his speed
// (+0x154), falling at 20 / s^2 (+0x148 his drop, +0x138 the fall); 7 is his jump (from the animation's frame 9, a
// ballistic arc to the goal over 60 frames, until the frame 29); 9 puts the switch he carries at its place and
// waits for the hero to let go of it; 13 opens the gate when the scene reaches its last frame (2000).
// orig: L11:0x002d1030
void pok_2d1030_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21aa40)
    case 4: case 5: case 6: case 8: case 10: {  // (0x002d107c)
        const float to = atan2_f(fpu_sub(lwc1(pv + 0x120), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x124), lwc1(m + 0x14)));
        const float four_pi = u2f(0x41490fdbu);
        yaw_seek(m + 0x48, pv + 0x150, to, fpu_mul(dt2(), four_pi), fpu_mul(dt2(), u2f(0x41c90fdbu)), fpu_mul(dt(), four_pi));
        swc1(pv + 0x130, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x154)));
        const float s = sin_vu(lwc1(m + 0x48));
        const float fall = fpu_sub(lwc1(pv + 0x148), fpu_mul(dt2(), 20.0f));
        swc1(pv + 0x134, fpu_mul(s, lwc1(pv + 0x154)));
        swc1(pv + 0x138, fall);
        if (0.0f < fall) swc1(pv + 0x138, 0.0f);
        break;
    }
    case 7: {  // (0x002d1140)
        const uint32_t vel = pv + 0x130, goal = pv + 0x120;
        if (anim_crossed(m, 9.0f) != 0) {
            vec3_sub(V(vel), V(goal), V(m + 0x10));
            const float frames = int_to_float(frame_scale_i(0x3c));
            vec3_scale(V(vel), V(vel), fpu_div(1.0f, frames));
            const float speed = vec2_length(V(vel));
            swc1(pv + 0x138, sub_0025abf0(m + 0x10, goal, nullptr, speed, -fpu_mul(dt2(), u2f(0x412ccccdu))));
            swc1(m + 0x58, u2f(0x3f2aaaabu));
        }
        if (0.0f < vec3_length(V(vel))) swc1(pv + 0x138, fpu_sub(lwc1(pv + 0x138), fpu_mul(dt2(), u2f(0x412ccccdu))));
        if (anim_crossed(m, 29.0f) != 0) {
            vec_clear(V(vel));
            swc1(m + 0x58, 1.0f);
        }
        break;
    }
    case 9: {  // (0x002d1254)
        const uint32_t lever = pv_moby(pv, 0xf0 + (zone(pv) != 0 ? 4u : 0u));
        sub_002604f0(m, lever, L11_ADDR(0x001613e0u), L11_ADDR(0x001613f0u), m + 0x10, m + 0x40);
        if (pok_30a480_l11(lever)) anim_to(m, 5);
        break;
    }
    case 13: {  // (0x002d12d8)
        const uint32_t scene = OV(0x0016c860);
        if (lw(scene + 0x30) != 1 || lw(scene + 0x34) != frame_scale_i(0x7d0)) break;
        pok_2cb990_l11(pv_moby(pv, 0x104));
        break;
    }
    default:  // (0x002d131c)
        break;
    }
}

// His walk: the walker moves him by his velocity (pvar +0x130); walking (4 .. 6, or 7 toward a place of kind 3) the
// zone's walls (and in zones 2 / 3 the doors' walls) push him out; his move this frame into pvar +0x140.
// orig: L11:0x002d1340
void pok_2d1340_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t start = sp, vel = sp + 0x10;
    lq_sq(start, m + 0x10);
    lq_sq(vel, pv + 0x130);
    const float r = u2f(0x3e99999au);  // (0.3)
    walker_move(m, vel, 0x10, 0.5f, r, 0.0f);
    const uint32_t st = lbu(m + 0x20);
    if (st - 4 < 3 || (st == 7 && lw(pv + 0x15c) == 3)) {
        const uint32_t pos = m + 0x10;
        path_push_out_2d(lwu(pv + 0x60 + (zone(pv) << 2)), pos, pos, r);
        if (lw(pv + 0x158) == 2) {
            path_push_out_2d(lwu(pv + 0x80), pos, pos, r);
            path_push_out_2d(lwu(pv + 0x84), pos, pos, r);
            path_push_out_2d(lwu(pv + 0x88), pos, pos, r);
        } else if (lw(pv + 0x158) == 3) {
            path_push_out_2d(lwu(pv + 0x80), pos, pos, r);
            path_push_out_2d(lwu(pv + 0x84), pos, pos, r);
        }
    }
    vec3_sub(V(pv + 0x140), V(m + 0x10), V(start));
    ee_stack_free(0x20);
}

// State `state` entered (its byte +0x20) with its animation and goal: idle (1, 3, 11, 12, 14: animation 0, standing
// still; 14 also the talk radius 10 and the talk's answer +0x56 = 2), walking (4: animation 2 at the level's walk
// speed, 5 / 6: 3 at its run speed), jumping (7: animation 4), carrying a switch to its place (8 / 10: animation 3,
// the goal the switch's place at the level's offset 0x1613e0 / 0x161400 from it), waiting (9 / 16: animation 0).
// orig: L11:0x002d2088
void pok_2d2088_l11(uint32_t m, uint32_t state) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x20, state);
    auto speed = [&](uint32_t set, uint32_t speed_at) {
        anim_to(m, set);
        swc1(pv + 0x154, fpu_mul(lwc1(speed_at), dt()));
    };
    auto carry_goal = [&](uint32_t offset) {
        anim_to(m, 3);
        const uint32_t lever = pv_moby(pv, 0xf0 + (zone(pv) != 0 ? 4u : 0u));
        Vec4 d{};
        lq_sq(pv + 0x120, lever + 0x10);
        mat3_transform(d, V(offset), M(lever + 0xc0));
        vec3_add(V(pv + 0x120), V(pv + 0x120), d);
    };
    switch (state & 0xffu) {  // (jump table L11:0x21aac0, indexed by the state less 1)
    case 1: case 3: case 11: case 12: case 14:  // (0x002d20d4)
        anim_to(m, 0);
        if (lbu(m + 0x20) == 0xe) swc1(pv + 0x2c, 10.0f);
        vec_clear(V(pv + 0x130));
        if (lbu(m + 0x20) == 0xe) sh(pv + 0x56, 2);
        break;
    case 4:  // (0x002d213c)
        speed(2, L11_ADDR(0x001613d0u));
        break;
    case 5: case 6:  // (0x002d2170)
        speed(3, L11_ADDR(0x001613d4u));
        break;
    case 7:  // (0x002d21b4)
        anim_to(m, 4);
        vec_clear(V(pv + 0x130));
        break;
    case 8:  // (0x002d21dc)
        carry_goal(L11_ADDR(0x001613e0u));
        break;
    case 9: case 16:  // (0x002d2268)
        anim_to(m, 0);
        vec_clear(V(pv + 0x130));
        break;
    case 10:  // (0x002d22a0)
        carry_goal(L11_ADDR(0x00161400u));
        break;
    default:  // 2, 13, 15 and the states out of the table (0x002d2324)
        break;
    }
}

// Whether he can walk to `to`: it lies in his zone's polygon and a way round the zone's walls (and its doors' in zones
// 2 / 3) reaches it (the way's next point into pvar +0x120).
// orig: L11:0x002d2340
bool pok_2d2340_l11(uint32_t m, uint32_t to) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t poly = zone_polygon(pv);
    if (point_in_polygon(to, poly + 0x10, lw(poly)) == 0) return false;
    const uint32_t walls = ee_stack_alloc(0x10);
    sw(walls, lwu(pv + 0x60 + (zone(pv) << 2)));
    int32_t n = 1;
    if (lw(pv + 0x158) == 2) {
        n = 4;
        sw(walls + 4, lwu(pv + 0x80));
        sw(walls + 8, lwu(pv + 0x84));
        sw(walls + 0xc, lwu(pv + 0x88));
    } else if (lw(pv + 0x158) == 3) {
        n = 3;
        sw(walls + 4, lwu(pv + 0x80));
        sw(walls + 8, lwu(pv + 0x84));
    }
    const uint32_t way = nov_276fe8_l1(walls, n, lw(pv + 0x70 + (zone(pv) << 2)), m + 0x10, to, pv + 0x120, u2f(0x3e4ccccdu));
    ee_stack_free(0x10);
    return way != 0;
}

// An enemy of class 1246 in one of its harmless states (0xb, 0xc, 0x12, 0x14).
// orig: L11:0x00316128
bool pok_316128_l11(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    return st == 0x14 || st == 0xb || st == 0x12 || st == 0xc;
}

// The enemies of his zone (the up to four moby groups of the zone's row, pvar +0x90 + zone * 16, ended by a negative
// index): 2 when a live one (not a harmless class-1246 one) is within 10 of him, 1 when there is one at all, else 0.
// orig: L11:0x002d2460
int32_t pok_2d2460_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t row = pv + 0x90 + (zone(pv) << 4);
    int32_t found = 0;
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t group = lh(row + (k << 2));
        if (group < 0 || lvd::group_count() < group) return found;
        uint32_t e = lvd::group(group);
        if (e == 0) continue;
        for (;; e += 2) {
            const uint32_t o = moby_at(lhu(e) & 0x7fffu);
            if (static_cast<int8_t>(lbu(o + 0x20)) >= 0 && !(lh(o + 0xa6) == 0x4de && pok_316128_l11(o))) {
                found = 1;
                if (vec3_distance(V(m + 0x10), V(o + 0x10)) < 10.0f) return 2;
            }
            if (lh(e) < 0) break;
        }
    }
    return found;
}

// While he stands in one of his two boxes (pvar +0xf8 / +0xfc): the spot `pos` (moved to within 4 of him) when it is in
// his zone, on a floor more than 0.5 above or below him and seen from him (a line 0.15 above both against the world):
// `pos` gets the spot on that floor, true.
// orig: L11:0x002d25e8
bool pok_2d25e8_l11(uint32_t m, uint32_t pos) {
    const uint32_t pv = lwu(m + 0x78);
    bool inside = false;
    for (uint32_t k = 0; k < 2 && !inside; ++k) inside = point_in_box(m + 0x10, lw(pv + 0xf8 + (k << 2))) != 0;
    if (!inside) return false;
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t spot = sp, from = sp + 0x10;
    vec3_sub(V(spot), V(pos), V(m + 0x10));
    if (4.0f < vec3_length(V(spot))) vec3_set_length(V(spot), V(spot), 4.0f);
    vec3_add(V(spot), V(spot), V(m + 0x10));
    bool ok = false;
    const uint32_t poly = zone_polygon(pv);
    if (point_in_polygon(spot, poly + 0x10, lw(poly)) != 0) {
        swc1(spot + 8, fpu_add(lwc1(spot + 8), 5.0f));
        const float floor = floor_probe_20(spot);
        if (0.5f < abs_f(fpu_sub(floor, lwc1(m + 0x18)))) {
            lq_sq(from, m + 0x10);
            const float up = u2f(0x3e19999au);  // (0.15)
            const float from_z = fpu_add(lwc1(from + 8), up);
            const float spot_z = fpu_add(floor, up);
            swc1(from + 8, from_z);
            swc1(spot + 8, spot_z);
            if (segment_collide_v(V(from), V(spot), 2, m, nullptr) != 0) {
                swc1(spot + 8, floor);
                lq_sq(pos, spot);
                ok = true;
            }
        }
    }
    ee_stack_free(0x20);
    return ok;
}

// His head: in his standing states (0 .. 5, 11, 12, 14, 16) it turns toward the hero (within 70 degrees, tilted
// 30 degrees down to 15 up); the springs of joints 0 / 1 (pvar +0x160 / +0x1e0) pull it either way.
// orig: L11:0x002d27b0
void pok_2d27b0_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);  // (0.02, 0.3)
    bool looking = false;
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21ab00)
    case 0: case 1: case 2: case 3: case 4: case 5: case 11: case 12: case 14: case 16:  // (0x002d2818)
        looking = true;
        break;
    default:  // (0x002d2834)
        break;
    }
    if (looking) {
        const Vec4 target = V(kHeroPos);
        Vec4 eye = V(m + 0x10), dv{};
        eye.z = fpu_add(eye.z, 1.0f);
        vec3_sub(dv, target, eye);
        float turn = angle_sub(atan2_f(dv.x, dv.y), lwc1(m + 0x48));
        float tilt = -atan2_f(vec2_length(dv), dv.z);
        const float lim = u2f(0x3f9c61aau);  // (70 degrees)
        if (lim < turn) turn = lim;
        else if (turn < -lim) turn = -lim;
        if (u2f(0x3e860a92u) < tilt) tilt = u2f(0x3e860a92u);
        else if (tilt < u2f(0xbf060a92u)) tilt = u2f(0xbf060a92u);
        const float near = fpu_mul(turn, u2f(0x3f333333u)), far = fpu_mul(turn, u2f(0x3e99999au));  // (0.7, 0.3 of it)
        swc1(pv + 0x1c4, tilt);
        swc1(pv + 0x1c8, near);
        swc1(pv + 0x248, far);
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0x1d0, u2f(0x40300000u));
    joint_node_spring(m, pv + 0x160, 0, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    joint_node_spring(m, pv + 0x1e0, 1, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
}

// His states. 3 .. 5 (following the hero) fall back to 6 when no enemy of his zone is left; 4 .. 6 jump (7) down to a
// free spot 3 in front of him when there is one (then kind 3). 0 starts: his talk, the zones' walls, then by the
// level's progress either goes (the errand done, 0x13d4ee, and his collectible taken: the switches placed, the gate
// open), or waits at the gate (14), or at his second place (6: the hero within 12 of it, a box lowered), or at the first
// (1). 1 talks (then the hero walks to 2.5 in front, 2), or once he was answered waits for the hero within 6; 2 waits
// for the scene, then 3; 3 .. 5 follow the hero (3 waits, 4 walks, 5 runs: by his distance and the way round the
// walls); 6 goes to his zone's place and, there, takes the next switch (7 then 8, a box raised), or goes on (11 / 12);
// 7 waits for the jump's end, then the kind of move after it (+0x15c); 8 / 10 carry a switch to its place (9 / 7);
// 9 waits for the switch placed; 11 waits for the door (pvar +0x100) open, then the next zone (3); 12 talks again
// (his collectible taken, 0x13d3dc, the talker's word 2, at his place facing the scene's spot 2 in front, the hero
// walking to it: 13); 13 opens the gate after the scene (14); 14 talks when his collectible is still there (then 15);
// 15 gives the O2 Mask after the scene, asks for the level change and the goal's announcement, and goes.
// (PAL: the 2.00 disc's own build differs from NTSC-U only in state 12's scene flag, a byte at +0x4a there; the rest
// of its differences are relocations.)
// orig: L11:0x002d14b0
void pok_2d14b0_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t talk = pv + 0x20;
    const int32_t near = pok_2d2460_l11(m);
    if (lbu(m + 0x20) - 3u < 3u && near == 0) {
        pok_2d2088_l11(m, 6);
        return;
    }
    if (lbu(m + 0x20) - 4u < 3u) {
        const uint32_t spot = ee_stack_alloc(0x10);
        vec3_set_length(V(spot), V(m + 0xc0), 3.0f);
        vec3_add(V(spot), V(spot), V(m + 0x10));
        if (pok_2d25e8_l11(m, spot)) {
            lq_sq(pv + 0x120, spot);
            swc1(pv + 0x128, floor_probe_20(pv + 0x120));
            sw(pv + 0x15c, 3);
            pok_2d2088_l11(m, 7);
        }
        ee_stack_free(0x10);
    }
    auto box = [&](uint32_t off) { return lvd::box(lvd::BOXES, lw(pv + off)); };
    auto place_at = [&](uint32_t b) {  // he is put at the box `b`'s place and angles
        lq_sq(m + 0x10, b + 0x30);
        lq_sq(m + 0x40, b + 0x70);
    };
    auto lift = [&](uint32_t off, float by) { swc1(box(off) + 0x38, fpu_add(lwc1(box(off) + 0x38), by)); };
    auto lower = [&](uint32_t off) { swc1(box(off) + 0x38, fpu_sub(lwc1(box(off) + 0x38), 500.0f)); };
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21aa80)
    case 0: {  // (0x002d159c)
        talk_init(m, talk);
        sb(m + 0x30, 0xff);
        path_links_build(pv + 0x60, 1, lwu(pv + 0x70));
        path_links_build(pv + 0x64, 1, lwu(pv + 0x74));
        path_links_build(pv + 0x68, 1, lwu(pv + 0x78));
        vec_clear(V(pv + 0x130));
        vec_clear(V(pv + 0x140));
        if (lbu(0x0013d4ee) != 0) {
            if (collected(lwu(pv + 0x10c))) {
                pok_30a840_l11(pv_moby(pv, 0xf0));
                pok_30a840_l11(pv_moby(pv, 0xf4));
                pok_2cb990_l11(pv_moby(pv, 0x104));
                moby_retire(m);
                return;
            }
        } else if (collected(lwu(pv + 0x10c))) {
            sw(pv + 0x158, 3);
            place_at(box(0xec));
            pok_2cb990_l11(pv_moby(pv, 0x104));
            pok_2d2088_l11(m, 0xe);
            swc1(pv + 0x2c, 10.0f);
            pok_30a840_l11(pv_moby(pv, 0xf0));
            pok_30a840_l11(pv_moby(pv, 0xf4));
            return;
        }
        if (collected(lwu(pv + 0x108)) && vec3_distance(V(kHeroPos), V(box(0xe4) + 0x30)) < 12.0f) {
            sw(pv + 0x158, 1);
            place_at(box(0xe4));
            pok_2d2088_l11(m, 6);
            lower(0x114);
            pok_30a840_l11(pv_moby(pv, 0xf0));
            return;
        }
        lower(0x110);
        lower(0x114);
        sw(pv + 0x158, 0);
        swc1(pv + 0x2c, 5.0f);
        pok_2d2088_l11(m, 1);
        break;
    }
    case 1:  // (0x002d18ac)
        if (lh(pv + 0x56) != 0) {
            if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 6.0f) pok_2d2088_l11(m, 3);
        } else if (lh(0x0013f65e) == 0 && talk_update(m, talk) != 0) {
            hero_walk_to_l1(2.5f, m);
            pok_2d2088_l11(m, 2);
        }
        break;
    case 2:  // (0x002d1930)
        if (lw(L11_ADDR(0x0015f5c4u)) != 2) pok_2d2088_l11(m, 3);
        break;
    case 3:  // (0x002d194c)
        if (6.0f < vec2_distance(V(m + 0x10), V(kHeroPos)) && pok_2d2340_l11(m, kHeroPos) && near == 1) pok_2d2088_l11(m, 4);
        break;
    case 4:  // (0x002d199c)
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 3.0f || !pok_2d2340_l11(m, kHeroPos) || near == 2) pok_2d2088_l11(m, 3);
        else if (8.0f < vec2_distance(V(m + 0x10), V(kHeroPos))) pok_2d2088_l11(m, 5);
        break;
    case 5:  // (0x002d1a20)
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 3.0f || !pok_2d2340_l11(m, kHeroPos) || near == 2) pok_2d2088_l11(m, 3);
        else if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 4.0f) pok_2d2088_l11(m, 4);
        break;
    case 6: {  // (0x002d1ab8)
        const uint32_t goal = pv + 0x120;
        pok_2d2340_l11(m, box(0xe0 + (zone(pv) << 2)) + 0x30);
        if (!(vec2_distance(V(m + 0x10), V(goal)) < 1.0f)) break;
        auto switch_place = [&](uint32_t lever_off, bool flip) {  // the goal: the switch's place, at the level's offsets along its axes
            const uint32_t lever = pv_moby(pv, lever_off);
            Vec4 d{};
            lq_sq(goal, lever + 0x10);
            vec3_set_length(d, V(lever + 0xc0), lwc1(L11_ADDR(0x00161418u)));
            vec3_add(V(goal), V(goal), d);
            const float side = lwc1(L11_ADDR(0x00161414u));
            vec3_set_length(d, V(lever + 0xd0), flip ? -side : side);
            vec3_add(V(goal), V(goal), d);
            vec3_set_length(d, V(lever + 0xe0), lwc1(L11_ADDR(0x00161410u)));
            vec3_add(V(goal), V(goal), d);
        };
        switch (lw(pv + 0x158)) {
        case 0:  // (0x002d1b4c)
            switch_place(0xf0, false);
            sw(pv + 0x15c, 8);
            pok_2d2088_l11(m, 7);
            break;
        case 1:  // (0x002d1bdc)
            switch_place(0xf4, true);
            lift(0x110, 500.0f);
            sw(pv + 0x15c, 8);
            pok_2d2088_l11(m, 7);
            break;
        case 2:  // (0x002d1ca4)
            pok_2d2088_l11(m, 0xb);
            break;
        case 3:  // (0x002d1cb8)
            lift(0x114, 500.0f);
            pok_2d2088_l11(m, 0xc);
            break;
        default:
            break;
        }
        break;
    }
    case 7:  // (0x002d1e50)
        if ((lbu(m + 0x70) & 2u) != 0) pok_2d2088_l11(m, lwu(pv + 0x15c));
        break;
    case 8:  // (0x002d1e74)
        if (vec2_distance(V(m + 0x10), V(pv + 0x120)) < 0.5f) pok_2d2088_l11(m, 9);
        break;
    case 9:  // (0x002d1eb0)
        if (moby_in_state(pv_moby(pv, 0xf0 + (zone(pv) != 0 ? 4u : 0u)), 5)) pok_2d2088_l11(m, 0xa);
        break;
    case 10:  // (0x002d1ef8)
        if (vec2_distance(V(m + 0x10), V(pv + 0x120)) < 0.5f) {
            const uint32_t next = lwu(pv + 0x158) + 1;
            sw(pv + 0x158, next);
            lq_sq(pv + 0x120, box(0xd0 + (next << 2)) + 0x30);
            sw(pv + 0x15c, 4);
            pok_2d2088_l11(m, 7);
        }
        break;
    case 11:  // (0x002d1f74)
        if (moby_in_state(pv_moby(pv, 0x100), 5)) {
            sw(pv + 0x158, lwu(pv + 0x158) + 1);
            pok_2d2088_l11(m, 3);
        }
        break;
    case 12: {  // (0x002d1cf4)
        swc1(pv + 0x2c, 255.0f);
        if (talk_update(m, talk) == 0) break;
        checkpoint_mark_collected(lwu(pv + 0x10c));
        sb(0x0013d3dc, 1);
        talker_word_set(m, 2);
        place_at(box(0xe0 + (zone(pv) << 2)));
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        Vec4 at{}, angles = V(m + 0x40);
        vec3_set_length(at, V(m + 0xc0), 2.0f);
        vec3_add(at, at, V(m + 0x10));
        angles.z = angle_add(angles.z, u2f(0x40490fd0u));
        const uint32_t scene = OV(0x0016c860);
        V(scene + 0x10) = at;
        V(scene + 0x20) = angles;
        if (region_core_moved()) sb(scene + 0x4a, 1);  // (region: the 1.02 build's walk-to flag, a byte at +0x4a)
        else sh(scene + 0x46, 1);
        hero_walk_to_l1(2.5f, m);
        pok_2d2088_l11(m, 0xd);
        break;
    }
    case 13:  // (0x002d1e18)
        if (lw(L11_ADDR(0x0015f5c4u)) == 2) break;
        pok_2cb990_l11(pv_moby(pv, 0x104));
        pok_2d2088_l11(m, 0xe);
        break;
    case 14:  // (0x002d1fb0)
        if (!collected(lwu(pv + 0x118)) || lw(0x001413d4) == 0x32) break;
        if (talk_update(m, talk) != 0) {
            hero_walk_to_l1(2.5f, m);
            pok_2d2088_l11(m, 0xf);
        }
        break;
    case 15:  // (0x002d2028)
        if (lw(L11_ADDR(0x0015f5c4u)) == 2) break;
        item_give(6, 1);
        level_change_request(0, -1);
        sub_00203908(TXT(0x2af8), 0x3a);
        moby_retire(m);
        break;
    default:
        break;
    }
}

// Class 114, the resort owner: near the camera (within 38) his floor band is kept; his moves, his walk, his states and
// his head, every frame.
// orig: L11:0x002d0fa8
void pok_class114_update_l11(uint32_t m) {
    class_mates_mark_fixed(m, 0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L11_ADDR(0x001677c0u))) < 38.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x1e);
    }
    pok_2d1030_l11(m);
    pok_2d1340_l11(m);
    pok_2d14b0_l11(m);
    pok_2d27b0_l11(m);
}

}  // namespace rac1
