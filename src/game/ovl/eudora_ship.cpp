// Eudora's flying ships: the shared update of the level's aircraft classes 466, 480, 485, 486, 488, 490, 493, 494, 495,
// 498 and 555, and of class 484, the Transport Ship cut from retail (retail's own update of 484 only retires it; the Cut
// Content option runs its beta update, which flies the ship through this one: cut_content.cpp, docs/advanced.md). Each
// ship flies its path on the aircraft base (jet_update_base_l1, novalis_jet_base.cpp). On its way, entering one of its
// four boxes, it leaves the path: it flies to the box's centre and turns to the box's heading, hovers there five seconds,
// flies back to where it left the path and turns back to its heading, and the boxes rest ten seconds (class 484 makes no
// such stops). Its
// rotors spin (one about x, one about z, or two about z, by class); classes 480 and 494 carry a moby. Hit, it counts the hit
// (at the tenth on the level, once, a sound and a message), explodes by class (the blasts of 485 / 486 and 493 hurt the mobys
// around), throws its pieces (the debris classes by class), and for the exploding classes hides for ten seconds, to show
// again once it is out of the camera's view. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are
// Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
//
// pvars: the aircraft base's (its speed +0xfc, its yaw rate +0x7c, its path +0x74, a long timer +0x11c), the top speed
// +0x138 (its speed at the start), the rotors' angles +0x140 / +0x144 and joints +0x180 / +0x1c0 (their rotations +0x190 /
// +0x1d0), the box it went to +0x148 (an index of the level's boxes), the stop's mode +0x149 (0 on the path, 1 to the box,
// 2 hovering, 3 back to the path), its timer +0x14a, the heading +0x14c and the place +0x160 it left the path at, the
// four boxes +0x150 .. +0x15c (-1: none), the hidden time +0x170 (hidden while +0x174 is set), the draw distance +0x178,
// the carried moby's index +0x17c (-1: none).
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float dt60() { return lwc1(0x0015ed60); }   // (the other scale)
const float kPi = u2f(0x40490fdbu);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kFourPi = u2f(0x41490fdbu);

// the level's moby `i`
uint32_t moby_at(int32_t i) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the ship's kind: its class less 466 (0 .. 89 the classes the tables cover; anything else is 90 or more)
uint32_t ship_kind(uint32_t m) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(m + 0xa6) - 0x1d2u))); }

// a blast that hurts the mobys within `radius` of the ship by `damage` (its hit record `rec` in the frame; the record's
// +0x18 word is left as the frame has it, as the original does)
void ship_blast(uint32_t m, uint32_t rec, float radius, float damage) {
    sw(rec + 0x14, 0x810000);
    swc1(rec + 0x1c, damage);
    sw(rec + 0x10, m);
    vec_clear(V(rec));
    sw(rec + 0x20, 0);
    mobys_in_sphere(m + 0x10, radius, 0x10, m, rec);
}

// a piece of the ship, of class `cls`, thrown at `vel`
void ship_piece(uint32_t m, uint32_t cls, uint32_t vel) {
    const uint32_t zero = L4_ADDR(0x0015f580);
    debris_spawn(m, cls, m + 0x10, m + 0x40, frame_scale_i(0x5a), 0, vel, zero, zero, fpu_mul(dt_sq(), 12.0f));
}
}  // namespace

// orig: L4:0x002ca420
void eud_class466_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state0 = lbu(m + 0x20);  // (the state as the frame starts: the rotors' set-up)
    if (pv == 0) {
        ee_stack_free(0x160);
        return;
    }
    const uint32_t pos = m + 0x10;
    const uint32_t step = sp + 0x30;  // (the step / the carried moby's offset / the pieces' velocity; its w as the frame has it)

    // ---- the set-up: a path whose last point is its first loses that point
    if (state0 == 0) {
        if (const int32_t id = lw(pv + 0x74); id != -1) {
            const uint32_t path = lvd::path(id);
            if (vec3_distance(V(path + 0x10), V(path + (lwu(path) << 4))) < 0.5f) lvd::set_path_count(path, lwu(path) - 1);
        } else {
            debug_printf_stub();  // (no path: the ship's index and class)
        }
        sh(m + 0x32, 0x7f);
        sb(m + 0x30, 0xff);
        swc1(pv + 0x138, lwc1(pv + 0xfc));
        sw(pv + 0x178, static_cast<uint32_t>(lh(m + 0x32)));
    }
    jet_update_base_l1(m);

    // ---- the stops at its boxes (not the Transport Ship, class 484)
    if (lh(m + 0xa6) != 0x1e4) {
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x14a)));
        auto box = [&] { return lvd::box(lvd::BOXES, static_cast<int32_t>(lbu(pv + 0x148))); };
        auto turn_to = [&](float heading) {
            yaw_turn_damped(m, pv + 0x7c, heading, fpu_mul(dt_sq(), u2f(0x3f860a92u)), fpu_mul(dt_sq(), kPi), fpu_mul(dt(), kPi));
        };
        switch (lbu(pv + 0x149)) {
        case 0: {  // (0x002ca5a4) on its path: speeding up to its top speed; in one of its boxes (rested), off to it
            const float speed = lwc1(pv + 0xfc), top = lwc1(pv + 0x138);
            if (speed < top) {
                const float faster = fpu_add(speed, fpu_mul(dt_sq(), 20.0f));
                swc1(pv + 0xfc, top < faster ? top : faster);
            }
            for (uint32_t entry = pv + 0x150; entry < pv + 0x160; entry += 4) {
                if (lh(pv + 0x14a) != 0 || lw(entry) == -1) continue;
                if (point_in_box(pos, lw(entry)) == 0) continue;
                lq_sq(pv + 0x160, pos);
                sb(pv + 0x149, 1);
                sb(pv + 0x148, lbu(entry));
                sw(pv + 0x7c, 0);
                swc1(pv + 0x14c, lwc1(m + 0x48));
                sb(m + 0x20, 3);
                sw(pv + 0x11c, static_cast<uint32_t>(frame_scale_i(0x8ca0)));
                break;
            }
            break;
        }
        case 1:  // (0x002ca63c) to the box: there and turned to its heading (within 10 degrees), hovering
            turn_to(lwc1(box() + 0x78));
            if (vec2_distance(V(pos), V(box() + 0x30)) < 1.0f && angle_dist(lwc1(m + 0x48), lwc1(box() + 0x78)) < u2f(0x3e32b8c2u)) {
                sb(pv + 0x149, 2);
                sh(pv + 0x14a, static_cast<uint32_t>(frame_scale_i(0x12c)));
            }
            vec3_sub(V(step), V(box() + 0x30), V(pos));
            vec3_set_length(V(step), V(step), fpu_mul(dt(), 3.0f));
            vec3_add(V(pos), V(pos), V(step));
            break;
        case 2:  // (0x002ca780) hovering, then back
            if (lh(pv + 0x14a) == 0) {
                swc1(pv + 0x7c, 0.0f);
                sb(pv + 0x149, 3);
            }
            break;
        case 3: {  // (0x002ca798) back to where it left its path and to its heading (within 2 degrees): the boxes rest
            turn_to(lwc1(pv + 0x14c));
            if (vec2_distance(V(pos), V(pv + 0x160)) < 1.0f && angle_dist(lwc1(m + 0x48), lwc1(pv + 0x14c)) < u2f(0x3d0efa35u)) {
                sb(pv + 0x149, 0);
                sh(pv + 0x14a, static_cast<uint32_t>(frame_scale_i(0x258)));
                swc1(pv + 0x7c, 0.0f);
                swc1(pv + 0xfc, 0.0f);
                sw(pv + 0x11c, 0);
            }
            vec3_sub(V(step), V(pv + 0x160), V(pos));
            const float left = vec3_length(V(step));
            const float most = fpu_mul(dt(), 3.0f);
            vec3_set_length(V(step), V(step), left < most ? left : most);
            vec3_add(V(pos), V(pos), V(step));
            break;
        }
        default:
            break;
        }
    }

    // ---- the rotors
    auto spin = [&](uint32_t angle, uint32_t quat, int32_t axis) {
        const float a = angle_add(lwc1(pv + angle), fpu_mul(dt(), kFourPi));
        swc1(pv + angle, a);
        quat_from_axis_angle(V(pv + quat), axis, a);
    };
    if (state0 == 0) {
        switch (ship_kind(m)) {  // (the table L4:0x1dbe40)
        case 0: case 14: case 19: case 20: case 28: case 29: case 32: case 89:  // (0x002ca940) 466 480 485 486 494 495 498 555: one
            sub_0024f590(m, 0, pv + 0x180);
            sw(pv + 0x140, 0);
            break;
        case 22: case 27:  // (0x002ca958) 488 493: two
            sub_0024f590(m, 0, pv + 0x180);
            sw(pv + 0x140, 0);
            sub_0024f590(m, 1, pv + 0x1c0);
            sw(pv + 0x144, 0);
            break;
        default:  // (0x002caa88)
            break;
        }
    } else {
        switch (ship_kind(m)) {  // (the table L4:0x1dbfb0)
        case 0: case 14:  // (0x002ca9b4) 466 480: about x
            spin(0x140, 0x190, 0);
            break;
        case 19: case 20: case 28: case 29: case 32: case 89:  // (0x002ca9f0) 485 486 494 495 498 555: about z
            spin(0x140, 0x190, 2);
            break;
        case 22: case 27:  // (0x002caa2c) 488 493: both about z
            spin(0x140, 0x190, 2);
            spin(0x144, 0x1d0, 2);
            break;
        default:  // (0x002caa88)
            break;
        }
    }

    // ---- the carried moby (480: at (0.5, 0, -5) in the ship's frame, a quarter turn more; 494: at (0, 0, -2))
    if (const int32_t index = lw(pv + 0x17c); index != -1) {
        const uint32_t carried = moby_at(index);
        sh(carried + 0x32, 0x7f);
        const int32_t cls = lh(m + 0xa6);
        if (cls == 0x1e0 || cls == 0x1ee) {
            sd(step, 0);
            sd(step + 8, 0);
            if (cls == 0x1e0) {
                swc1(step, 0.5f);
                swc1(step + 8, -5.0f);
            } else {
                swc1(step + 8, -2.0f);
            }
            mat3_transform(V(step), V(step), M(m + 0xc0));
            vec3_add(V(carried + 0x10), V(pos), V(step));
            lq_sq(carried + 0x40, m + 0x40);
            if (cls == 0x1e0) swc1(carried + 0x44, angle_add(lwc1(carried + 0x44), kHalfPi));
            moby_anim_advance(carried);
            moby_update_transform(carried);
            moby_update_transform_kept(carried);
            sh(carried + 0x34, lhu(carried + 0x34) | 6u);
        }
    }

    // ---- the hit (the weapons' mask 0x210000): downed
    if (sub_0025a420(m, NOT_ADDR(0x210000), 0) != 0) {
        const int32_t downed = lw(0x0015edf8) + 1;  // (the hits counted: at the tenth, once, a sound and a message)
        sw(0x0015edf8, static_cast<uint32_t>(downed));
        if (!(downed < 10) && lbu(0x0013d40e) == 0) {
            sb(0x0013d40e, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        auto hide = [&] {  // (hidden ten seconds)
            sw(pv + 0x170, static_cast<uint32_t>(frame_scale_i(0x258)));
            sw(pv + 0x174, 1);
        };
        switch (ship_kind(m)) {  // (the table L4:0x1dc120)
        case 19: case 20:  // (0x002cac38) 485 486: a blast first (radius 4, damage 1)
            ship_blast(m, sp + 0x40, 4.0f, 1.0f);
            [[fallthrough]];
        case 0: case 14: case 22: case 24: case 28: case 29: case 32: case 89: {  // (0x002cac7c) 466 480 488 490 494 495 498 555
            const uint32_t none = sp + 0x40;
            sd(step, 0);
            sd(step + 8, 0);
            swc1(step + 8, 0.75f);
            vec_clear(V(none));
            vec3_add(V(step), V(step), V(pos));
            explosion_effect(m, none, step, 0xa, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 100000.0f, 3.0f, 15.0f);
            const int32_t cls = lh(m + 0xa6);
            if (cls == 0x1f2 || cls == 0x22b) moby_sound_start(0, 0, m);
            else if (cls != 0x1e6) moby_sound_start(1, 0, m);
            hide();
            break;
        }
        case 27: {  // (0x002cad7c) 493: a burst at each of its joints 0 and 1, a bigger explosion, a blast (radius 6, damage 2)
            const uint32_t up = sp + 0x40, none = sp + 0x50;
            sd(up, 0);
            sd(up + 8, 0);
            swc1(up + 8, 0.75f);
            vec_clear(V(none));
            moby_chain_point(m, 0, step);
            vec3_add(V(step), V(step), V(up));
            explosion_burst(m, step, -1, 1.0f, 13.0f);
            moby_chain_point(m, 1, step);
            vec3_add(V(step), V(step), V(up));
            explosion_burst(m, step, -1, 1.0f, 13.0f);
            vec3_add(V(step), V(up), V(pos));
            explosion_effect(m, none, step, 0x14, 5, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 100000.0f, 3.0f, 15.0f);
            moby_sound_start(1, 0, m);
            ship_blast(m, sp + 0x70, 6.0f, 2.0f);
            hide();
            break;
        }
        default:  // (0x002caf04)
            break;
        }
        // the pieces: thrown along the ship's x axis and up
        vec3_scale(V(step), V(m + 0xc0), fpu_mul(dt60(), u2f(0x3d99999au)));
        swc1(step + 8, fpu_add(lwc1(step + 8), fpu_mul(dt60(), u2f(0x3da3d70au))));
        switch (lh(m + 0xa6)) {
        case 0x1d2:
            for (const uint32_t cls : {0x63fu, 0x641u, 0x642u}) ship_piece(m, cls, step);
            break;
        case 0x1e0:
            for (const uint32_t cls : {0x642u, 0x643u, 0x644u}) ship_piece(m, cls, step);
            break;
        case 0x1e6:
            ship_piece(m, 0x645, step);
            for (int i = 0; i < 6; ++i) ship_piece(m, 0x646, step);
            break;
        case 0x1ea:
            for (const uint32_t cls : {0x647u, 0x648u, 0x649u}) ship_piece(m, cls, step);
            break;
        case 0x1ee:
            for (const uint32_t cls : {0x64du, 0x64eu}) ship_piece(m, cls, step);
            break;
        case 0x1ef:
            for (const uint32_t cls : {0x64fu, 0x650u, 0x651u, 0x658u}) ship_piece(m, cls, step);
            break;
        case 0x1f2:
            for (const uint32_t cls : {0x652u, 0x653u, 0x654u}) ship_piece(m, cls, step);
            break;
        case 0x1ed:
            ship_piece(m, 0x793, step);
            ship_piece(m, 0x794, step);
            for (int i = 0; i < 3; ++i) ship_piece(m, 0x795, step);
            for (int i = 0; i < 3; ++i) ship_piece(m, 0x796, step);
            break;
        default:
            break;
        }
    }
    sb(m + 0xa4, 0xff);

    // ---- hidden: back once its time is up and it is out of the camera's view (more than a quarter turn off its heading)
    if (lw(pv + 0x174) != 0) {
        bool back = false;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x170))) != 0) {
            const float from_camera = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(L4_ADDR(0x00166f40))),
                                              fpu_sub(lwc1(m + 0x14), lwc1(L4_ADDR(0x00166f44))));
            back = kHalfPi < angle_dist(lwc1(L4_ADDR(0x00166f58)), from_camera);
        }
        if (back) {
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            sw(pv + 0x174, 0);
            if (const int32_t index = lw(pv + 0x17c); index != -1) {
                const uint32_t carried = moby_at(index);
                sh(carried + 0x34, lhu(carried + 0x34) & 0xffbeu);
            }
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        } else {
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            if (const int32_t index = lw(pv + 0x17c); index != -1) {
                const uint32_t carried = moby_at(index);
                sh(carried + 0x34, lhu(carried + 0x34) | 0x41u);
            }
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        }
    }
    ee_stack_free(0x160);
}

}  // namespace rac1
