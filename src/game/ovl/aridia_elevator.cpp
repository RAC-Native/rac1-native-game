// Aridia's own moby classes: the elevator, hand-ported from Aridia's overlay (`orig: L2:`). Aridia-only data is L2_ADDR;
// the level's cylinders go through the level-data facade; the base ELF's (the hero 0x13f350, the frame time 0x15ed6c) are
// bare. Declared in aridia_enemies.h, registered in aridia_mobys.cpp (with its draw callback 0x2dc2c8).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>
#include <game/port_stubs.h>

namespace rac1 {

// The elevator: a platform that moves between the two ends of its level cylinder (pvar +0xa0; the end +0xbc: 0 the
// top, 1 the bottom, each the cylinder's centre height +0x38 plus or less its axis, and the level's 0x161bd0 above)
// and carries what stands on it (the attachment record +0x60, moved by the frame's change of position and turn).
// States: 0 set up (at the top; +0xa4 set: state 2, waiting for the hero, else 1, called), 1 idle until the hero
// stands on it (0x13f64c with no 0x13f65e) or the call flag 0x13d4da is set, 2 waits for the hero to get on (within
// 20 and level with an end: it goes there; on it: it goes on), 3 moves to its goal (+0xa8, the speed +0xb0; its sound
// +0xb4 while it moves; its draw callback queued while it moves; there, without the hero on it, back to 2).
// orig: L2:0x002dbd38
void elevator_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t moved = sp, turn0 = sp + 0x10, end = sp + 0x20;
    constexpr uint32_t HERO = 0x0013f350;
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(turn0, m + 0x40);
    if (pv == 0) {
        ee_stack_free(0xc0);
        return;
    }
    // the cylinder's end `sign` (+1 the top, -1 the bottom) into `end`, its height with the level's lift above
    auto cylinder = [pv] { return lvd::box(lvd::CYLINDERS, lw(pv + 0xa0)); };
    auto end_height = [&] { return fpu_add(fpu_add(lwc1(end + 8), lwc1(cylinder() + 0x38)), lwc1(L2_ADDR(0x00161bd0))); };
    // to the other end: the goal there, the speed 0, state 3
    auto go_other_end = [&] {
        vec_clear(V(end));
        swc1(end + 8, lbu(m + 0xbc) != 0 ? -1.0f : 1.0f);
        sb(m + 0xbc, (lbu(m + 0xbc) + 1) & 1);
        mat4_transform(V(end), V(end), M(cylinder()));
        sw(pv + 0xb0, 0);
        swc1(pv + 0xa8, end_height());
        sb(m + 0x20, 3);
    };
    auto hero_on = [m] { return lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0; };
    // the sound +0xb4 stopped (-1)
    auto sound_off = [m, pv] {
        if (sub_0028d8c0(m, lw(pv + 0xb4)) == 0) return;
        sub_0028d918(lw(pv + 0xb4));
        sw(pv + 0xb4, 0xffffffffu);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // set up: at the top
        V(end) = Vec4{};
        swc1(end + 8, 1.0f);
        sw(pv + 0xb4, 0xffffffffu);
        mat3_transform(V(end), V(end), M(cylinder()));
        swc1(pv + 0xa8, fpu_add(lwc1(end + 8), lwc1(cylinder() + 0x38)));
        sb(m + 0xbc, 1);
        swc1(m + 0x18, fpu_add(fpu_add(lwc1(end + 8), lwc1(cylinder() + 0x38)), lwc1(L2_ADDR(0x00161bd0))));
        sb(m + 0x20, lw(pv + 0xa4) != 0 ? 2u : 1u);
        break;
    }
    case 1:  // idle: the hero on it, or called
        if (hero_on() || lbu(0x0013d4da) != 0) go_other_end();
        break;
    case 2: {  // waiting for the hero
        sound_off();
        if (vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 20.0f) {
            vec_clear(V(end));
            swc1(end + 8, lbu(m + 0xbc) != 0 ? -1.0f : 1.0f);
            mat4_transform(V(end), V(end), M(cylinder()));
            const float h = end_height();
            swc1(end + 8, h);
            if (abs_f(fpu_sub(lwc1(HERO + 0x88), h)) < 1.0f) {  // the hero level with that end: it goes there
                sb(m + 0xbc, (lbu(m + 0xbc) + 1) & 1);
                swc1(pv + 0xa8, h);
                sw(pv + 0xb0, 0);
                sb(m + 0x20, 3);
                break;
            }
        }
        if (lwu(HERO + 0x2fc) != m) break;
        if (lh(HERO + 0x30e) != 0) break;
        go_other_end();
        break;
    }
    case 3: {  // moving to its goal; with the hero in its cylinder about 2 below its top (within 1) and itself not below
               // its goal, it seeks its own height at four times the rate
        float goal = lwc1(pv + 0xa8);
        float accel = fpu_mul(lwc1(L2_ADDR(0x00161be0)), lwc1(0x0015ed70));
        if (point_in_cylinder(HERO + 0xd0, lw(pv + 0xa0)) != 0 &&
            !(1.0f < abs_f(fpu_sub(fpu_sub(lwc1(m + 0x18), lwc1(HERO + 0x88)), 2.0f))) && !(lwc1(m + 0x18) < lwc1(pv + 0xa8))) {
            goal = lwc1(m + 0x18);  // (the original's register still holds its height: it seeks where it is)
            accel = fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161be0)), 4.0f), lwc1(0x0015ed70));
        }
        const float speed = seek_1d(m + 0x18, pv + 0xb0, goal, accel, accel, fpu_mul(lwc1(L2_ADDR(0x00161bdc)), lwc1(0x0015ed6c)));
        vec_clear(V(end));
        swc1(end + 8, speed);
        attach_frame_set(pv + 0x60, end, m + 0x40, m + 0x40);
        if (lwc1(end + 8) == 0.0f) {
            sound_off();
        } else if (sub_0028d8c0(m, lw(pv + 0xb4)) == 0) {
            sw(pv + 0xb4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
        if (lwc1(m + 0x18) == lwc1(pv + 0xa8) && !hero_on()) {  // there, the hero off it: waiting again
            vec_clear(V(pv + 0x70));
            sb(m + 0x20, 2);
            break;
        }
        if (lwc1(pv + 0xb0) != 0.0f) sub_001f93e0(L2_ADDR(0x002dc2c8), m);  // (its draw callback for this frame)
        break;
    }
    default:
        break;
    }
    // what stands on it moved with it: the frame's change of position, the turn from the last frame's to this one's
    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x60, moved, turn0, m + 0x40);
    ee_stack_free(0xc0);
}

}  // namespace rac1
