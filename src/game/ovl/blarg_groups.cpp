// Blarg's Nebula G34 (level 6) own code, hand-ported: the small helpers the level's classes share, on a moby group (the
// level's group lists, lvd::group) or on one of the level's door sets (a class-1105 moby with its seven parts, pvars
// +0x24 .. +0x3c). Header blarg_own.h. Class 1105's own update (part 3) still calls the door sets' and the groups' helpers
// from its generated code (glued to these). Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// the moby of the level's moby table at the index a group list holds (bit 15: the list's last)
uint32_t listed_moby(uint32_t entry) { return lwu(L6_ADDR(0x0015ffd8)) + ((lhu(entry) & 0x7fffu) << 8); }

// each moby of group `group`'s list (none when the group has no list)
template <class F>
void each_in_group(int32_t group, F f) {
    const uint32_t list = lvd::group(group);
    if (list == 0) return;
    for (uint32_t e = list;; e += 2) {
        f(listed_moby(e));
        if (lh(e) < 0) break;
    }
}

constexpr uint32_t SET_PARTS = 0x24;  // a door set's seven parts (pvars)
constexpr int kSetParts = 7;
}  // namespace

// The sliding panels (class 0x16f, 367) of group `group` sent out (goal 1) or back (goal 0): their state 2, their goal
// pvar +0x28.
// orig: L6:0x002d9e08
void panels_slide_out_l6(int32_t group) {
    each_in_group(group, [](uint32_t m) {
        if (lh(m + 0xa6) != 0x16f) return;
        const uint32_t pv = lwu(m + 0x78);
        sb(m + 0x20, 2);
        swc1(pv + 0x28, 1.0f);
    });
}

// orig: L6:0x002d9e78
void panels_slide_back_l6(int32_t group) {
    each_in_group(group, [](uint32_t m) {
        if (lh(m + 0xa6) != 0x16f) return;
        const uint32_t pv = lwu(m + 0x78);
        sb(m + 0x20, 2);
        sw(pv + 0x28, 0);
    });
}

// Whether group `group` has no class-0x33b moby (827) still standing: each one gone (state 0xfe / 0xfd) or down (0x10);
// 1 for a group without a list.
// orig: L6:0x002e9ea0
uint32_t group_walkers_down_l6(int32_t group) {
    const uint32_t list = lvd::group(group);
    if (list == 0) return 1;
    const uint32_t first = lwu(L6_ADDR(0x0015ffd8));
    for (uint32_t e = list;; e += 2) {
        const uint32_t m = first + ((lhu(e) & 0x7fffu) << 8);
        if (m != 0 && lh(m + 0xa6) == 0x33b) {
            const uint32_t st = lbu(m + 0x20);
            if (st != 0xfe && st != 0xfd && st != 0x10) return 0;
        }
        if (lh(e) < 0) break;
    }
    return 1;
}

// A door set `m` that is shut (state 2) opens: its sound, state 4, its timers +0x14 / +0x18 cleared, its parts to state 5.
// Whatever its state, +0xbc becomes 1.
// orig: L6:0x002f5360
void door_set_open_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0xbc, 1);
    if (lbu(m + 0x20) != 2) return;
    moby_sound_start(0, 0, m);
    sb(m + 0x20, 4);
    sw(pv + 0x18, 0);
    sw(pv + 0x14, 0);
    for (int k = 0; k < kSetParts; ++k) sb(lwu(pv + SET_PARTS + static_cast<uint32_t>(k) * 4) + 0x20, 5);
}

// A door set `m` that is open (state 6) shuts: its sound, it and its parts shown (+0x31 1, +0x34 bit 0 cleared), state 8,
// its parts 9, its timer +0x18 cleared. Whatever its state, +0xbc becomes 1.
// orig: L6:0x002f53e8
void door_set_shut_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0xbc, 1);
    if (lbu(m + 0x20) != 6) return;
    moby_sound_start(0, 0, m);
    const uint32_t flags = lhu(m + 0x34);
    sb(m + 0x31, 1);
    sb(m + 0x20, 8);
    sh(m + 0x34, flags & 0xfffeu);
    sw(pv + 0x18, 0);
    for (int k = 0; k < kSetParts; ++k) {
        const uint32_t part = pv + SET_PARTS + static_cast<uint32_t>(k) * 4;
        sb(lwu(part) + 0x20, 9);
        sh(lwu(part) + 0x34, lhu(lwu(part) + 0x34) & 0xfffeu);
        sb(lwu(part) + 0x31, 1);
    }
}

// A door set `m` that is open (state 6) is set back shut at once: it and its parts shown, state 2 (parts 3), the timers
// +0x14 cleared and each turned back to its angle +0x10 (about x). Whatever its state, +0xbc becomes 1.
// orig: L6:0x002f54a0
void door_set_reset_l6(uint32_t m) {
    sb(m + 0xbc, 1);
    if (lbu(m + 0x20) != 6) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t flags = lhu(m + 0x34);
    sb(m + 0x31, 1);
    sb(m + 0x20, 2);
    sh(m + 0x34, flags & 0xfffeu);
    sw(pv + 0x14, 0);
    swc1(m + 0x40, angle_sub(lwc1(pv + 0x10), lwc1(pv + 0x14)));
    moby_update_transform(m);
    for (int k = 0; k < kSetParts; ++k) {
        const uint32_t part = pv + SET_PARTS + static_cast<uint32_t>(k) * 4;
        const uint32_t p = lwu(part);
        sb(p + 0x20, 3);
        const uint32_t ppv = lwu(p + 0x78);
        sh(lwu(part) + 0x34, lhu(lwu(part) + 0x34) & 0xfffeu);
        sb(lwu(part) + 0x31, 1);
        sw(ppv + 0x14, 0);
        swc1(lwu(part) + 0x40, angle_sub(lwc1(ppv + 0x10), lwc1(ppv + 0x14)));
        moby_update_transform(lwu(part));
    }
}

// Group `group`'s mobys put away: state 0x12, hidden (+0x34 bit 0 set, bit 12 cleared), no model (+0x94 0).
// orig: L6:0x002f9948
void group_put_away_l6(int32_t group) {
    each_in_group(group, [](uint32_t m) {
        const uint32_t flags = lhu(m + 0x34);
        sb(m + 0x20, 0x12);
        sw(m + 0x94, 0);
        sh(m + 0x34, (flags | 1u) & 0xefffu);
    });
}

// Group `group`'s mobys put away (state 0x12) brought back: state 0, shown (bit 0 cleared, bit 12 set), their class's
// model (+0x94).
// orig: L6:0x002f99b0
void group_bring_back_l6(int32_t group) {
    each_in_group(group, [](uint32_t m) {
        if (lbu(m + 0x20) != 0x12) return;
        const uint32_t flags = lhu(m + 0x34);
        sb(m + 0x20, 0);
        sh(m + 0x34, (flags & 0xfffeu) | 0x1000u);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    });
}

// A creature `m` waiting to be let out (state 0xf) thrown out from `from`: shown, state 6, a random heading, its move
// record (+0x120) thrown up and out (move_state_start, the set 2), its turn +0x128 reversed; 1 when it was, else 0.
// orig: L6:0x002e9d10
uint32_t creature_let_out_l6(uint32_t m, uint32_t from) {
    if (lbu(m + 0x20) != 0xf) return 0;
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(m + 0x10, from);
    const uint32_t model = lwu(lwu(m + 0x24) + 0x10);
    const uint32_t flags = lhu(m + 0x34);
    sb(m + 0x20, 6);
    sw(m + 0x94, model);
    sh(m + 0x34, (flags & 0xffbeu) | 0x1000u);
    vec_clear(V(m + 0x40));
    swc1(m + 0x48, rand_angle());
    euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
    const float dt = lwc1(0x0015ed6c), dt_sq = lwc1(0x0015ed70);
    const float up = fpu_add(dt, dt);
    sw(pv + 0x134, 0);
    sw(pv + 0x144, 5);
    sb(pv + 0x15d, 3);
    swc1(pv + 0x13c, up);
    swc1(pv + 0x130, fpu_mul(dt_sq, 20.0f));
    swc1(pv + 0x138, up);
    move_state_start(m, pv + 0x120, 2, 1, 0, rand_angle());
    swc1(pv + 0x128, -lwc1(pv + 0x128));
    return 1;
}

}  // namespace rac1
