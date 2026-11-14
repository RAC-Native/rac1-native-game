// Orxon's own moby classes (level 10): the doors and their floor switches. Class 353, a gate that sinks into the floor
// once its one-shot flag is set; class 1015, a door that closes (sinks) when told and opens (rises) again while anything
// stands under it, or when told; class 1031, the floor switch of Clank's part that opens its door (pvar +0) while a moby
// or the hero presses it. Header orxon_mobys.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// Class 353: state 0 notes its position (pvar +0) and goes to 1 when its flag is set; 1 sinks by the frame time down to
// 3.75 below it, then notes the sinking in the level's tables (the byte of its index: its +0xb0 plus 2) and holds (2):
// on level 10 also the game's byte 0x13d3d8. State 2 keeps it 3.75 below its place.
// orig: L10:0x002befe8
void orx_class353_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        lq_sq(pv, m + 0x10);
        if (flag_set(m)) sb(m + 0x20, 1);
        break;
    case 1: {
        const float y = fpu_add(lwc1(m + 0x14), -dt());
        swc1(m + 0x14, y);
        if (!(y <= fpu_add(lwc1(pv + 4), -3.75f))) break;
        const int32_t idx = lh(m + 0xb2);
        sb(OV(0x001ba5d0) + static_cast<uint32_t>(idx) + 0x454, lbu(m + 0xb0) + 2u);
        const uint32_t uid = lbu(m + 0xb0);
        // (the second byte too unless the moby's byte of the level's row is set and not gone for good)
        if (uid == 0xff || (lbu(OV(0x0015fc88) + uid) != 0xff && gone_byte(m) == 0xff))
            sb(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454, uid + 2u);
        sb(m + 0x20, 2);
        if (level_number() == 10) sb(0x0013d3d8, 1);
        break;
    }
    case 2:
        swc1(m + 0x14, fpu_add(lwc1(pv + 4), -3.75f));
        break;
    default:
        break;
    }
}

// Whether `p` lies inside the door's footprint: in the moby's own frame within the level's half-width (x) and
// half-depth (y).
// orig: L10:0x002d9000
bool orx_2d9000_l10(uint32_t m, const Vec4& p) {
    Mat4 to_local;
    mat3_transpose(to_local, M(m + 0xc0));
    Vec4 local;
    vec3_sub(local, p, V(m + 0x10));
    mat4_transform(local, local, to_local);
    if (!(abs_f(local.x) < lwc1(L10_ADDR(0x00161c10)))) return false;
    return abs_f(local.y) < lwc1(L10_ADDR(0x00161c0c));
}

// Class 1015: state 0 notes its place (pvar +0) and holds open (2); 1 closes: it sinks by 4 over 20 frames to its place
// (then 2), and while a moby of its group (pvar +0x10), the camera or the hero is in its footprint it opens again (3);
// 3 rises 4 above its place, then holds (4) and sets the game's byte 0x13d3d4 + its pvar +0x14 (-1: none).
// orig: L10:0x002d90a8
void orx_class1015_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {  // (the original prints the class and index of a moby without pvars)
        debug_printf_stub();
        moby_retire(m);
        return;
    }
    auto step = [] { return fpu_div(4.0f, frame_scale_f(20.0f)); };
    switch (lbu(m + 0x20)) {  // (the table at L10:0x001dd1b0)
    case 0:  // (0x002d9114)
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 2);
        break;
    case 1: {  // closing (0x002d9130)
        const float z = fpu_sub(lwc1(m + 0x18), step());
        swc1(m + 0x18, z);
        if (z <= lwc1(pv + 8)) {
            swc1(m + 0x18, lwc1(pv + 8));
            sb(m + 0x20, 2);
        }
        bool blocked = false;
        const int32_t group = lw(pv + 0x10);
        if (group != -1) {
            const uint32_t cur = ee_stack_alloc(0x10);  // (the group's walk: its current moby)
            for (int32_t more = group_first(cur, group, 0, 0); more != 0; more = group_next(cur, lwu(cur), 0, 0)) {
                if (orx_2d9000_l10(m, V(lwu(cur) + 0x10))) {
                    blocked = true;
                    break;
                }
            }
            ee_stack_free(0x10);
        }
        if (!blocked) blocked = orx_2d9000_l10(m, V(OV(0x00166dc0))) || orx_2d9000_l10(m, V(HERO_POS));  // (the camera, the hero)
        if (blocked) sb(m + 0x20, 3);
        break;
    }
    case 3: {  // opening (0x002d9238)
        const float z = fpu_add(lwc1(m + 0x18), step());
        swc1(m + 0x18, z);
        const float top = fpu_add(lwc1(pv + 8), 4.0f);
        if (top <= z) {
            swc1(m + 0x18, top);
            sb(m + 0x20, 4);
            const int32_t note = lw(pv + 0x14);
            if (note != -1) sb(0x0013d388 + static_cast<uint32_t>(note) + 0x4c, 1);
        }
        break;
    }
    default:  // 2, 4: held (0x002d92a0)
        break;
    }
}

// Class 1031: a floor switch for its door (pvar +0: the door's index; -1 none: dark). Pressed while a moby other than
// class 1007 stands on it (its floor 0.1 or less away from the moby's feet), while it is gone for good, or while another
// switch holds it (pvar +0xc, set by a switch whose pvar +4 names this one); a pressed switch holds the switch of its
// pvar +4. Pressed: a closed or closing door (state 1 / 2) opens (3), lit; released: an open door (4) closes (1), dark.
// The two sounds start at most every 30 frames (pvar +0x10).
// orig: L10:0x002d92b8
void orx_class1031_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv) == -1) {
        moby_set_color_hi(m, 8, 8, 8);
        return;
    }
    const uint32_t door = moby_at(lw(pv));
    bool pressed = false;
    const int32_t n = mobys_in_sphere(m + 0x10, 1.0f, 0, m, 0);
    for (int32_t k = 0; k < n; ++k) {
        const uint32_t other = coll_hit_list_at(k);
        const float floor = floor_probe_v(0.5f, V(other + 0x10), 0);
        if (coll_hit_moby() != m) continue;
        if (!(abs_f(fpu_sub(floor, lwc1(coll_hit_list_at(k) + 0x18))) < u2f(0x3dcccccdu))) continue;
        if (lh(coll_hit_list_at(k) + 0xa6) != 0x3ef) pressed = true;
    }
    tick32(pv + 0x10);
    if (gone_byte(m) == 0xff) pressed = true;
    bool open = pressed;
    if (pressed) {
        const int32_t other = lw(pv + 4);
        if (other != -1) sw(lwu(moby_at(other) + 0x78) + 0xc, 1);
        else sw(pv + 0xc, 0);
    } else if (lw(pv + 0xc) != 0) {
        sw(pv + 0xc, 0);
        open = true;
    }
    if (open) {
        if (lbu(door + 0x20) - 1u < 2u) {
            sb(door + 0x20, 3);
            if (lw(pv + 0x10) == 0) {
                moby_sound_start(0, 0, door);
                moby_sound_start(0, 0, m);
                sw(pv + 0x10, static_cast<uint32_t>(frame_scale_i(0x1e)));
            }
        }
        moby_set_color_hi(m, 0x7f, 0x7f, 0x7f);
        return;
    }
    if (lbu(door + 0x20) == 4) {
        sb(door + 0x20, 1);
        if (lw(pv + 0x10) == 0) {
            moby_sound_start(1, 0, door);
            moby_sound_start(0, 0, m);
            sw(pv + 0x10, static_cast<uint32_t>(frame_scale_i(0x1e)));
        }
    }
    moby_set_color_hi(m, 8, 8, 8);
}

}  // namespace rac1
