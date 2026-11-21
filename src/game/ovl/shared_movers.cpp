// The linked movers of Umbris (level 7) and Gemlik Base (13): a platform (class 1129 on Umbris, 104 / 106 on Gemlik) and a
// door (class 1063 on Umbris, 129 / 130 / 182 / 183 / 360 on Gemlik) that slide out along an offset when another moby (the
// "link") reaches one state and back when it reaches another. The two levels' copies are the same code, relinked; they touch
// only the engine's words every overlay moves the same way (the moby list 0x15ffd8, the movers' flags 0x1844a8: OV) and the
// base ELF's (the game's level number 0x15ed84, the broken mobys 0x14c050, the frame time 0x15ed6c). Header shared_movers.h.
//
// The mover's record (the platform's at pvar +0xa0, the door's at pvar +0): its home +0, the way still to go +0xc, the link's
// moby index +0x10 (-1: none), the link's state that opens +0x14 / closes +0x18, the offset +0x1c (in the moby's frame +0xc0),
// the speeds out +0x28 / back +0x2c, its sounds +0x30 / +0x34, its flag's index +0x38 (-1: none; 1 while out).
#include <game/ovl/shared_movers.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
enum class Link { Gone, Match, Other };

// the link's state against the record's word `which` (+0x14 / +0x18): Gone when there is no link or it is dead
Link link_state(uint32_t rec, uint32_t which) {
    const int32_t index = lw(rec + 0x10);
    const uint32_t list = lwu(OV(0x0015ffd8));
    if (index == -1) return Link::Gone;
    const uint32_t link = list + (static_cast<uint32_t>(index) << 8);
    if (link == 0) return Link::Gone;
    const uint32_t state = lbu(link + 0x20);
    if (state == 0xfe || state == 0xfd) return Link::Gone;
    return state == lwu(rec + which) ? Link::Match : Link::Other;
}

void set_flag(uint32_t rec, uint32_t value) {
    const int32_t index = lw(rec + 0x38);
    if (index != -1) sw(OV(0x001844a8) + (static_cast<uint32_t>(index) << 2), value);
}

void sound(uint32_t m, int32_t idx) { moby_sound_start(idx, 0, m); }

Vec4 offset(uint32_t rec) { return Vec4{lwc1(rec + 0x1c), lwc1(rec + 0x20), lwc1(rec + 0x24), 0.0f}; }

// start going out: the way to go is the offset's length
void start_out(uint32_t m, uint32_t rec) {
    const Vec4 off = offset(rec);
    swc1(rec + 0xc, vec3_length(off));
    if (lw(rec + 0x30) != -1) sound(m, lw(rec + 0x30));
}

// One frame going out (the platform's flag is set on arrival, the door's by its state each frame).
void step_out(uint32_t m, uint32_t rec, bool flag_on_arrival) {
    Vec4 off = offset(rec);
    mat3_transform(off, off, M(m + 0xc0));
    Vec4 step{};
    vec3_set_length(step, off, fpu_mul(lwc1(rec + 0x28), lwc1(0x0015ed6c)));
    vec3_add(V(m + 0x10), V(m + 0x10), step);
    const float left = fpu_sub(lwc1(rec + 0xc), fpu_mul(lwc1(rec + 0x28), lwc1(0x0015ed6c)));
    swc1(rec + 0xc, left);
    if (!(left < 0.0f)) return;
    vec3_add(V(m + 0x10), V(rec), off);  // (arrived: home + the offset)
    if (flag_on_arrival) set_flag(rec, 1);
    swc1(rec + 0xc, 0.0f);
    sb(m + 0x20, 3);
    if (lw(rec + 0x34) != -1) sound(m, lw(rec + 0x30));  // (the second sound's word chooses, the first one plays)
}

// One frame going back home.
void step_back(uint32_t m, uint32_t rec) {
    Vec4 off = offset(rec);
    const float length = vec3_length(off);
    vec3_set_length(off, off, -fpu_mul(lwc1(rec + 0x2c), lwc1(0x0015ed6c)));
    Vec4 step{};
    mat3_transform(step, off, M(m + 0xc0));
    vec3_add(V(m + 0x10), V(m + 0x10), step);
    const float gone = fpu_add(lwc1(rec + 0xc), fpu_mul(lwc1(rec + 0x2c), lwc1(0x0015ed6c)));
    swc1(rec + 0xc, gone);
    if (!(length < gone)) return;
    V(m + 0x10) = V(rec);  // (home)
    if (lw(rec + 0x30) != -1) sound(m, lw(rec + 0x34));  // (the first sound's word chooses, the second one plays)
    set_flag(rec, 0);
    sb(m + 0x20, 1);
}

// The state machine both kinds share: 0 home, 1 waiting home, 2 going out, 3 waiting out, 4 going back.
void mover_states(uint32_t m, uint32_t rec, bool door) {
    if (lbu(m + 0xb0) != 0xff) {  // (the link broken for good: the level's broken mobys, 16 bytes a level)
        if (lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) sw(rec + 0x10, 0xffffffffu);
    }
    switch (lbu(m + 0x20)) {  // (the overlay's jump table, states 0 .. 5)
    case 0:
        V(rec) = V(m + 0x10);
        set_flag(rec, 0);
        sb(m + 0x20, 1);
        sw(rec + 0xc, 0);
        break;
    case 1:
        if (door) set_flag(rec, 0);
        if (link_state(rec, 0x14) != Link::Other) {
            start_out(m, rec);
            sb(m + 0x20, 2);
        }
        break;
    case 2:
        if (door) set_flag(rec, 1);
        if (link_state(rec, 0x18) == Link::Match) {
            if (lw(rec + 0x30) != -1) sound(m, lw(rec + 0x30));
            sb(m + 0x20, 4);
        } else {
            step_out(m, rec, !door);
        }
        break;
    case 3:
        if (door) set_flag(rec, 1);
        if (link_state(rec, 0x18) == Link::Match) {
            sw(rec + 0xc, 0);
            if (lw(rec + 0x30) != -1) sound(m, lw(rec + 0x30));
            sb(m + 0x20, 4);
        }
        break;
    case 4:
        if (door) set_flag(rec, 0);
        if (link_state(rec, 0x14) != Link::Other) {
            if (lw(rec + 0x30) != -1) sound(m, lw(rec + 0x30));
            sb(m + 0x20, 2);
        } else {
            step_back(m, rec);
        }
        break;
    default:  // (5 and past the table: nothing)
        break;
    }
}
}  // namespace

// The platform: the mover's record at pvar +0xa0; what stands on it moves with it (its attachment frame, pvar +0x60, from
// the way it moved this frame and its turn).
// orig: L7:0x0031aee0 L13:0x002c32d0
void mover_platform_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 moved{};
    vec3_scale(moved, V(m + 0x10), -1.0f);
    const Vec4 old_rot = V(m + 0x40);
    mover_states(m, pv + 0xa0, false);
    vec3_add(moved, moved, V(m + 0x10));
    attach_frame_set_v(pv + 0x60, moved, old_rot, V(m + 0x40));
}

// The door: the mover's record at pvar +0; its flag says each frame whether it is out (states 2 / 3) or home (1 / 4).
// orig: L7:0x00310df0 L13:0x002c8390
void mover_door_update(uint32_t m) {
    mover_states(m, lwu(m + 0x78), true);
}

}  // namespace rac1
