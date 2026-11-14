// Aridia's own moby classes: the liftcar's platform, riding the liftcar (a moby that runs along its animation) between
// two stations (hand-ported from Aridia's overlay, `orig: L2:`). The base ELF's (the hero 0x13f350, the sound slots
// 0x13e550, the switches 0x14c050) are bare. Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)

// the sound in slot `*slot_at` stopped when the slot still plays it for `owner`; the slot -1
void sound_stop_slot(uint32_t slot_at, uint32_t owner) {
    const int32_t slot = lw(slot_at);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == owner && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(slot_at, 0xffffffffu);
}
}  // namespace

// The platform on the liftcar (pvar +0xa0: the car's moby index): it sits at the car's joint 0 every frame and carries
// what stands on it (the attachment record +0x60). States: 0 set up (facing the car's joint, the car's animation 3), 1
// waits for its switch (0x14c050, the row of the frame's mode 0x15ed84, column +0xb0: 0xff on) and then is at the low
// station (5), 3 / 5 at the high / low station: the hero gets on (and has not ridden yet, +0xbc), or within 16 and more
// than 5 below (at the low one) / above (at the high one): the ride down (4: the car's animation 2, sound 0) or up (2:
// animation 0, sound 2), the sound in +0xa4; 2 / 4 riding: the car stopped (+0x58 0, the sound off) while the hero is
// below it within 1.5, going again when he is off it; at the end of the animation, at the other station (3 / 5).
// orig: L2:0x002e2228
void liftcar_platform_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t car = lwu(OV(0x0015ffd8)) + (lwu(pv + 0xa0) << 8);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t moved = sp, turn0 = sp + 0x10, joint = sp + 0x20, at = sp + 0x60;
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(turn0, m + 0x40);
    moby_chain_point_v(car, 0, V(at));
    lq_sq(m + 0x10, at);
    auto face_joint = [&] {
        moby_chain_world_one(car, 0, joint);
        swc1(m + 0x48, atan2_f(lwc1(joint), lwc1(joint + 4)));
    };
    switch (lbu(m + 0x20)) {  // (the original's jump table at L2:0x1fab20)
    case 0:  // (0x002e22d8)
        face_joint();
        sb(m + 0x20, 1);
        moby_anim_set(car, 3, 0, 0);
        break;
    case 1:  // (0x002e231c)
        if (lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) sb(m + 0x20, 5);
        break;
    case 3:
    case 5: {  // (0x002e2358)
        const uint32_t state = lbu(m + 0x20);
        bool go = false;
        if (lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0 && lbu(m + 0xbc) == 0) {
            go = true;
        } else if (vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 16.0f) {
            if (state == 5 && lwc1(HERO + 0x88) < fpu_sub(lwc1(m + 0x18), 5.0f)) go = true;
            else if (state == 3 && fpu_add(lwc1(m + 0x18), 5.0f) < lwc1(HERO + 0x88)) go = true;
        }
        if (!go) {
            if (lwu(HERO + 0x2fc) != m) sb(m + 0xbc, 0);
            break;
        }
        if (state == 5) {  // up
            sb(m + 0x20, 2);
            moby_set_anim(car, 0, 0);
            sw(pv + 0xa4, static_cast<uint32_t>(moby_sound_start(2, 4, car)));
        } else {  // down
            sb(m + 0x20, 4);
            moby_set_anim(car, 2, 0);
            sw(pv + 0xa4, static_cast<uint32_t>(moby_sound_start(0, 4, car)));
        }
        break;
    }
    case 2:
    case 4: {  // (0x002e249c)
        face_joint();
        if (vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 1.5f && lwc1(HERO + 0x88) < fpu_sub(lwc1(m + 0x18), 1.5f)) {
            sw(car + 0x58, 0);  // (the hero under it: the car stopped)
            sound_stop_slot(pv + 0xa4, car);
        }
        if (lwc1(car + 0x58) == 0.0f &&
            (2.0f < vec2_distance(V(m + 0x10), V(HERO + 0x80)) || fpu_sub(lwc1(m + 0x18), 1.25f) < lwc1(HERO + 0x88))) {
            swc1(car + 0x58, 1.0f);  // (going again)
            sw(pv + 0xa4, static_cast<uint32_t>(moby_sound_start(lbu(m + 0x20) == 2 ? 2 : 0, 4, car)));
        }
        if ((lbu(car + 0x70) & 2) == 0) break;
        sound_stop_slot(pv + 0xa4, car);
        if (lbu(m + 0x20) == 2) {
            sb(m + 0x20, 3);
            moby_anim_set(car, 1, 0, 0);
            moby_sound_start(3, 0, car);
        } else {
            sb(m + 0x20, 5);
            moby_anim_set(car, 3, 0, 0);
            moby_sound_start(1, 0, car);
        }
        sb(m + 0xbc, 1);
        break;
    }
    default:  // 6 and past the table: nothing
        break;
    }
    // what stands on it moved with it: the frame's change of position, the turn from the last frame's to this one's
    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x60, moved, turn0, m + 0x40);
    ee_stack_free(0xf0);
}

}  // namespace rac1
