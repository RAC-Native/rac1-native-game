// Kalebo III's (level 16) lock-in fights: class 654, a fight's controller, and class 647, the gates it works. The gates of
// its gate group (+0x4) stand open (lifted by the level's offset L16 0x161a80 along their direction); when Ratchet enters the
// controller's box (pvar +0x0) they slide shut behind him, a scripted camera looks on for a second (the controller's camera
// path +0xc) and the fighters of its fighter group (+0x8, class 541) wake; when none is left the gates open again, Ratchet is
// put at a level point (+0x10) with the scripted camera let go (hydro_2eac18(3)), and the controller's byte in the level's
// table at 0x13d388 (+0x18) is set. A controller with no camera path (+0xc = -1) does the same without the camera and the
// placing. One
// with no box (or no pvars) is retired. Class 647 slides along its direction +0xc0 from its rest place (pvar +0x0) toward the
// offset +0x14 the controller gives it. Level 16 only: `orig: L16:`, level data L16_ADDR / lvd::*, base ELF bare.
// Registered in kalebo_mobys.cpp; checked by batch 250 (class647, class654, arena_*).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hydro.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
using kalebo::Frame;
using kalebo::group_entry_of_class;
using kalebo::tick32;

constexpr int16_t kGateClass = 0x287;     // class 647
constexpr int16_t kFighterClass = 0x21d;  // class 541

// The fighters of `group` woken: each one's state 2, its collision on (+0x31 = 1), its +0x34 bit 1 cleared.
// orig: L16:0x002ceca8
void fighters_wake(int32_t group) {
    uint32_t e = lvd::group(group);
    if (e == 0) return;
    for (;; e += 2) {
        const uint32_t m = group_entry_of_class(e, kFighterClass);
        sb(m + 0x31, 1);
        sb(m + 0x20, 2);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        if (lh(e) < 0) break;
    }
}

// The gates of `group` set moving (state 1) toward `offset` (their target +0x14; with `now` also their offset +0x18, so they
// stand there at once); returns the gate nearest the camera (within 1000), or 0. (`as_word`: the offset stored as the word 0.)
uint32_t gates_move(int32_t group, bool now, float offset, bool as_word) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 0;
    uint32_t nearest = 0;
    float best = 1000.0f;
    for (;; e += 2) {
        const uint32_t m = group_entry_of_class(e, kGateClass);
        const float d = vec3_distance(V(L16_ADDR(0x001671c0)), V(m + 0x10));
        if (d < best) { best = d; nearest = m; }
        const uint32_t pv = lwu(m + 0x78);
        sb(m + 0x20, 1);
        if (as_word) {
            sw(pv + 0x14, 0u);
            if (now) sw(pv + 0x18, 0);
        } else {
            swc1(pv + 0x14, offset);
            if (now) swc1(pv + 0x18, offset);
        }
        if (lh(e) < 0) break;
    }
    return nearest;
}

// The gates of `group` opened: toward the level's gate offset (L16 0x161a80).
// orig: L16:0x002d5ad0
uint32_t gates_open(int32_t group, bool now) { return gates_move(group, now, lwc1(L16_ADDR(0x00161a80)), false); }

// The gates of `group` shut: back to their rest place (offset 0).
// orig: L16:0x002d5bc8
uint32_t gates_shut(int32_t group, bool now) { return gates_move(group, now, 0.0f, true); }

// The fight's camera (state 3): the point along the controller's camera path (+0xc) eased toward where Ratchet stands across
// its box (the box's unit y, from -1 .. 1 to 0 .. 1), into `pos`; its angles (0, the level's pitch L16 0x161a8c, the
// controller's yaw) into `rot`.
// orig: L16:0x002d6260
void arena_camera_place(uint32_t m, uint32_t pos, uint32_t rot) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv));
    const uint32_t path = lvd::path(lw(pv + 0xc));
    Vec4 local;
    vec3_sub(local, V(0x0013f3d0), V(box + 0x30));
    local.w = 0.0f;
    mat3_transform(local, local, M(box + 0x40));
    const float across = fpu_mul(fpu_add(local.y, 1.0f), 0.5f);
    seek_1d(pv + 0x1c, pv + 0x20, across, lwc1(0x0015ed70), lwc1(0x0015ed70), fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c)));
    vec3_lerp(V(pos), V(path + 0x10), V(path + 0x20), lwc1(pv + 0x1c));
    vec_clear(V(rot));
    swc1(rot + 4, lwc1(L16_ADDR(0x00161a8c)));
    swc1(rot + 8, lwc1(m + 0x48));
}

// The level-wide player halfword the controller sets from its pvar +0x14 (when not negative).
void arena_set_hero_word(uint32_t pv) {
    if (lw(pv + 0x14) >= (0)) sh(0x0014162au, lhu(pv + 0x14));
}

}  // namespace

// Class 647, the gate: state 0 keeps its rest place; state 1 eases its offset +0x18 toward +0x14 (speed +0x10, 4 x dt^2 a
// frame, at most 8 x dt) and stands there along its direction, back to state 2 once there; state 2 rests.
// orig: L16:0x002d59e0
void kal_class647_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        V(pv) = V(m + 0x10);
        sb(m + 0x20, 2);
        break;
    case 1: {
        V(m + 0x10) = V(pv);
        const float accel = fpu_mul(lwc1(0x0015ed70), 4.0f);
        seek_1d(pv + 0x18, pv + 0x10, lwc1(pv + 0x14), accel, accel, fpu_mul(lwc1(0x0015ed6c), 8.0f));
        Vec4 along;
        vec3_set_length(along, V(m + 0xc0), lwc1(pv + 0x18));
        vec3_add(V(m + 0x10), V(m + 0x10), along);
        if (lwc1(pv + 0x18) == lwc1(pv + 0x14)) sb(m + 0x20, 2);
        break;
    }
    default:
        break;
    }
}

// Class 654, the fight's controller. States (the table L16 0x1e8330, by index): 0 sets up (retired without pvars or a box;
// without a camera path to 5, else the gates opened at once and to 1); 1 waits for Ratchet in the box (to 2: the gates shut,
// the camera on); 2 the camera's second (to 3: the fighters woken); 3 the fight (the camera follows; when no fighter is left
// and a second has passed: to 4, the gates open, Ratchet placed, the byte set); 4 done; 5 (no camera) waits for Ratchet (to
// 6: the gates shut, the fighters woken); 6 the fight (to 4 when none is left: the gates open, the byte set).
// orig: L16:0x002d5ef8
void kal_class654_update_l16(uint32_t m) {
    const Frame f(96);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state >= 7) return;
    auto gate_sound = [](uint32_t gate) { if (gate != 0) moby_sound_start(1, 0, gate); };
    switch (state) {  // (the jump table L16 0x1e8330)
    case 0: {  // (0x002d5f3c)
        if (pv == 0 || lw(pv) == -1) { moby_retire(m); break; }
        if (lw(pv + 0xc) == -1) {
            sb(m + 0x20, 5);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sb(m + 0x31, 0);
            break;
        }
        sb(m + 0x20, 1);
        gates_open(lw(pv + 4), true);
        break;
    }
    case 1:  // (0x002d5fa0)
        if (point_in_box(0x0013f3d0, lw(pv)) == 0) break;
        sw(pv + 0x24, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sb(m + 0x20, 2);
        arena_set_hero_word(pv);
        camera_scripted_set(L16_ADDR(0x001671c0), L16_ADDR(0x001671c0) + 0x10, 1, 0, 0);
        gate_sound(gates_shut(lw(pv + 4), false));
        break;
    case 2:  // (0x002d6010)
        arena_set_hero_word(pv);
        if (tick32(pv + 0x24) == 0) break;
        sb(m + 0x20, 3);
        sw(pv + 0x24, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        fighters_wake(lw(pv + 8));
        break;
    case 3: {  // (0x002d6064)
        arena_camera_place(m, f.sp, f.sp + 0x10);
        uid5_set_pvar_80(f.sp);
        uid5_set_pvar_90(f.sp + 0x10);
        arena_set_hero_word(pv);
        if (moby_group_count_l1(lw(pv + 8), -1) != 0) break;
        if (tick32(pv + 0x24) == 0) break;
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        sb(m + 0x20, 4);
        hydro_2eac18(3);
        const uint32_t spot = lvd::box(lvd::BOXES, lw(pv + 0x10));
        hero_place(spot + 0x30, spot + 0x70, 0, 1);
        moby_sphere_contact(m, lvd::box(lvd::BOXES, lw(pv + 0x10)) + 0x30, 0x10000, 0, 1, 0, 5.0f, 50.0f, 1.0f);
        sb(0x0013d388u + lwu(pv + 0x18) + 0x72, 1);
        gate_sound(gates_open(lw(pv + 4), false));
        break;
    }
    case 5:  // (0x002d618c)
        if (point_in_box(0x0013f3d0, lw(pv)) == 0) break;
        gates_shut(lw(pv + 4), false);
        fighters_wake(lw(pv + 8));
        sb(m + 0x20, 6);
        break;
    case 6:  // (0x002d61c4)
        if (point_in_box(0x0013f3d0, lw(pv)) != 0) arena_set_hero_word(pv);
        if (moby_group_count_l1(lw(pv + 8), -1) != 0) break;
        sb(m + 0x20, 4);
        gate_sound(gates_open(lw(pv + 4), false));
        sb(0x0013d388u + lwu(pv + 0x18) + 0x72, 1);
        break;
    default:  // 4 (0x002d6248)
        break;
    }
}

}  // namespace rac1
