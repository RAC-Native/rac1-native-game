// Novalis's own moby classes, part 1: what the player meets at the landing pad. Header
// novalis_mobys.h. Addresses are Novalis's (`orig: L1:`); the
// globals Novalis shares with Veldin are the level's own (OV), the base ELF's are bare.
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>

#include <game/math/vecmath.h>
#include <game/ovl/hydro.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// Class 641, the cave door: it turns about z by (the crank's own turning (class 0x118's pvar +0)
// times how far the crank (uid pvar +0) has turned since this door's first frame). State 0 stores the
// start angle (m+0x40) at pvar+4 and the degrees pvar+8 as radians; state 1 follows the crank.
// orig: L1:0x002f4348
void cave_door_update_l1(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 0) {
        const float r = lwc1(pv + 8) * u2f(0x3c8efa35u);
        swc1(pv + 4, lwc1(m + 0x40));
        swc1(pv + 8, r);
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    const int32_t uid = lw(pv);
    if (uid == -1) return;
    const uint32_t target = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(uid) << 8);
    if (lh(target + 0xa6) != 0x118) return;
    const uint32_t tpv = lwu(target + 0x78);
    sb(target + 0x30, 0xff);
    const float turned = angle_add(lwc1(pv + 8), -lwc1(pv + 4));
    const float scaled = angle_wrap(turned * lwc1(tpv));
    swc1(m + 0x40, angle_add(lwc1(pv + 4), scaled));
}

// Class 805, the spawn point (the checkpoint): ... see below.
// (Shared: the same code on every level but Veldin.)
// orig: L1:0x00300220 L2:0x002e28a0 L3:0x002d1140 L4:0x002d9fc0 L5:0x00309e40 L6:0x002e66e0 L7:0x003027a0 L8:0x002f99b0 L9:0x002f90e0 L10:0x002ca220 L11:0x00300578 L12:0x002f8090 L13:0x002fac70 L14:0x002f4190 L15:0x002db668 L16:0x002d69e0 L17:0x002d9b10 L18:0x002e1f40
void spawn_point_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t flags = lwu(pv + 8);
    uint32_t edge = 0;
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    if ((flags & 8) != 0) return;
    if (lbu(m + 0xbc) == 0) {
        sb(m + 0xbc, 1);
        sw(pv + 0x14, 0);
        return;
    }
    const int32_t need = lw(pv + 0x18);
    if (need != -1 && lbu(0x0014c050 + static_cast<uint32_t>(need) + (level << 4)) != 0xff) return;
    if ((flags & 2) != 0 && lbu(0x001413f4) != 1) return;
    if ((flags & 4) != 0) {
        const uint32_t k = lbu(0x001413f4);
        if (k != 3 && k != 0) return;
    }
    if ((lwu(pv + 8) & 0x10) != 0) {
        const uint32_t run = lwu(0x001413dc);
        if (!(run < 2) && run != 9) return;
    }
    if (point_in_box(0x0013f3d0, lw(pv)) != 0) {
        edge = lwu(pv + 0x14) != 0 ? 0u : 1u;
        sw(pv + 0x14, 1);
    } else {
        sw(pv + 0x14, 0);
    }
    const int32_t idx = static_cast<int16_t>(lhu(m + 0xb2));
    bool do_save = true;
    if (edge == 0) {
        if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) == 0) {
            const uint32_t bit = (lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (static_cast<uint32_t>(idx) & 0x1f)) & 1u;
            if (bit == 0) {
                sw(pv + 0xc, 0);
                do_save = false;
            }
        }
    }
    if (do_save) {
        // (s2 = 1 from here on: the rising edge, or the bit / flag said it was taken before)
        if (lwu(pv + 0xc) == 0 || lwu(pv + 0x10) != 0) {
            sw(pv + 0xc, 1);
            checkpoint_mark_collected(lbu(m + 0xb0));
            Vec4 at = V(m + 0x10);
            at.z = floor_probe_v(0.5f, at, 0);
            if (2.0f < abs_f(lwc1(m + 0x18) - at.z)) at.z = lwc1(m + 0x18);
            checkpoint_save_v(at, m + 0x40);
            const int32_t i2 = lh(m + 0xb2);
            sb(OV(0x001ba5d0) + static_cast<uint32_t>(i2) + 0x454, 0);
            sb(OV(0x001bb230) + static_cast<uint32_t>(i2) + 0x454, 0);
            const uint32_t i3 = lhu(m + 0xb2);
            const int32_t w = static_cast<int32_t>(static_cast<int16_t>(i3)) >> 5;
            const uint32_t clr = ~(1u << (i3 & 0x1f));
            const uint32_t a = 0x0014c190 + (level << 8) + (static_cast<uint32_t>(w) << 2);
            sw(a, lwu(a) & clr);
            const uint32_t b = OV(0x001ba4d0) + (static_cast<uint32_t>(w) << 2);
            sw(b, lwu(b) & clr);
        }
    }
    if (lbu(m + 0x20) == 0 && lbu(0x0014c050 + lbu(m + 0xb0) + (level << 4)) == 0xff) sb(m + 0x20, 1);
}

// A plant's bolts: 4..7 of them (bolts_drop), a pair of extra flags (2) when the hero is within 7
// units of it (xy).
// (Shared: the same code on levels 4 to 17.)
// orig: L1:0x002787a0 L4:0x00256460 L5:0x0028dd80 L6:0x00272a68 L7:0x0028c398 L8:0x0026db60 L9:0x00282b10 L10:0x00256e20 L11:0x00288fc0 L12:0x0027c2c8 L13:0x0026f828 L14:0x0026b788 L15:0x00253268 L16:0x0025eef0 L17:0x0025d9d0
void plant_bolts_drop_l1(uint32_t m) {
    const uint32_t flags = vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 7.0f ? 2u : 0u;
    bolts_drop(m, 4, 7, flags, -1);
}

// A copy of `m` of class `cls`, always drawn (+0x31 = 1, +0x32 = 0xff), scaled by the ratio of
// the two classes' sizes (the new class's +0x2c over m's own over its class's +0x24), at m's
// position and angles, its +0x38 pair, the moby's flags (+0x34) cleared.
// (Shared: the same code on Eudora, Batalia, Gaspar, Orxon, Pokitaru, Hoven and Drek's fleet.)
// orig: L1:0x00278e20 L4:0x00256ab0 L8:0x0026e1b0 L9:0x00283160 L10:0x00257470 L11:0x00289610 L12:0x0027c918 L17:0x0025e020
void moby_clone_l1(uint32_t m, uint32_t cls) {
    const uint32_t c = moby_spawn(cls);
    if (c == 0) return;
    sh(c + 0x32, 0xff);
    sb(c + 0x31, 1);
    const float k = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
    swc1(c + 0x2c, lwc1(c + 0x2c) * k);
    lq_sq(c + 0x10, m + 0x10);
    lq_sq(c + 0x40, m + 0x40);
    sb(c + 0x30, 0);
    sd(c + 0x38, ld(m + 0x38));
    sh(c + 0x34, 0);
    moby_update_transform(c);
}

// Class 1813, the tall shroom: state 0 -> 1; state 1 waits for a hit (sub_0025a420's mask
// 0x10000 with a positive damage at +0x2c), then 2; state 2 plays sound 0, drops its bolts,
// breaks into class 0x717's debris (all of the zero vector's arguments), leaves a clone of class
// 0x718 and retires.
// orig: L1:0x0030d0f0
void tall_shroom_update_l1(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st == 1) {
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    const uint32_t z = OV(0x0015f580);
    debris_spawn(m, 0x717, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    moby_clone_l1(m, 0x718);
    moby_retire(m);
}

// Class 1042, the shroom's underside: retires when hit (the mask 0x10000 with a positive +0x2c).
// orig: L1:0x00307c48
void mushroom_underside_update_l1(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) moby_retire(m);
}

// Class 754, the green mushroom: as the tall shroom up to state 2, which plays sound 0, drops its
// bolts, throws four class 0x719 pieces at 90 degrees apart round it (0.57 of its scale back,
// each turned 0.874 more than its place on the circle, 1.2 scales up), leaves a class 0x718
// clone and retires.
// orig: L1:0x002fd9a0
void green_mushroom_update_l1(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st == 1) {
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    const float s = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
    const uint32_t z = OV(0x0015f580);
    for (int32_t i = 0; i < 4; ++i) {
        const float a = static_cast<float>(i) * u2f(0x3fc90fdbu);
        Vec4 ang;
        vec_clear(ang);
        ang.z = angle_add(a, u2f(0x3f5f66f3u));
        Vec4 pos = {};
        pos.x = cos_vu(a) * u2f(0xbf11eb85u) * s;
        pos.y = sin_vu(a) * u2f(0xbf11eb85u) * s;
        pos.z = 0.0f;
        vec3_add(pos, pos, V(m + 0x10));
        pos.z = pos.z + s * u2f(0x3f99999au);
        debris_spawn_v(m, 0x719, pos, ang, 0, 0, V(z), V(z), V(z), 0.0f);
    }
    moby_clone_l1(m, 0x718);
    moby_retire(m);
}

// While the level's scripted camera plays (0x15f5c4 == 2, 0x15edb0 set), the list of the mobys the
// hero's block holds (the engine's 0x16c860, Novalis's 0x16cce0: count +0x44, pointers from +0x178): each class
// 0x398 / 0x1bf one is handed to sub_0024f590 with the fixed node (the engine's 0x17c440, Novalis's 0x17c8c0; idx 0)
// unless the node's byte +1 is set, and the node's +0x20 / +0x24 / +0x28 are set to 2.1.
// (Shared: the same code on Aridia, Kerwan, Rilgar, Umbris, Orxon and Gemlik, twice on Kerwan, Umbris and Gemlik.)
// orig: L1:0x002fb4b8 L2:0x002dfbd8 L3:0x002cf248 L3:0x002dc380 L5:0x003179a8 L7:0x002f60c8 L7:0x00300878 L10:0x002c7c00 L13:0x002edaf0 L13:0x002f8d78
void scripted_camera_nodes_l1() {
    if (lw(OV(0x0015f5c4)) != 2) return;
    if (lbu(0x0015edb0) == 0) return;
    const uint32_t list = OV(0x0016c860);
    if (lh(list + 0x44) <= 0) return;
    const uint32_t node = OV(0x0017c440);
    for (int32_t i = 0; i < lh(list + 0x44); ++i) {
        const uint32_t mm = lwu(list + 0x178 + static_cast<uint32_t>(i) * 4);
        const int16_t cls = lh(mm + 0xa6);
        if (cls != 0x398 && cls != 0x1bf) continue;
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(mm, 0, node);
        swc1(node + 0x28, u2f(0x40066666u));
        swc1(node + 0x20, u2f(0x40066666u));
        swc1(node + 0x24, u2f(0x40066666u));
    }
}

// Class 737, a scripted-camera trigger (the volume pvar +0x20 = box index): state 0 scales its
// timer (+0x2c) to frames (a missing destination box +0x24 retires it); state 1 waits for the hero in
// the box (or its own state flag +0xbc), then puts the hero at the box +0x28 (or where he is) in hero
// state 0x72 and starts the scripted camera at the destination box's camera (or from the live camera
// when +0x30 is set, with the pair +0x34), remembers the box's position / angles at pvar +0 / +0x10;
// state 2 counts the timer down and ends the scene (hero back to idle), or each frame while +0xbc
// is set keeps the scripted position (uid 5's pvars +0x80 / +0x90); state 3 waits for +0xbc.
// orig: L1:0x002fb5b0
void scripted_camera_trigger_update_l1(uint32_t m) {
    scripted_camera_nodes_l1();
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        const uint32_t idx16 = lhu(m + 0xb2);
        const int32_t idx = static_cast<int16_t>(idx16);
        if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t hero_pos = 0x0013f3d0;
        if (point_in_box(hero_pos, lw(pv + 0x20)) == 0 && lbu(m + 0xbc) != 1) return;
        const uint32_t buf = ee_stack_alloc(0x20);
        lq_sq(buf, OV(0x00166dc0));
        lq_sq(buf + 0x10, OV(0x00166dd0));
        const uint32_t boxes = lwu(OV(0x001600ec));
        const int32_t to = lw(pv + 0x28);
        if (to == -1) {
            hero_place(hero_pos, hero_pos + 0x10, 0x72, 1);
        } else {
            const uint32_t box = boxes + (static_cast<uint32_t>(to) << 7);
            hero_place(box + 0x30, box + 0x70, 0x72, 1);
        }
        const uint32_t home = boxes + (lwu(pv + 0x24) << 7);
        if (lw(pv + 0x30) != 0) {
            camera_scripted_set(buf, buf + 0x10, 2, lwu(pv + 0x34), 0);
        } else {
            camera_scripted_set(home + 0x30, home + 0x70, 0, 0, 0);
        }
        sw(OV(0x0015f404), 1);
        sb(m + 0x20, 2);
        sb(m + 0xbc, 1);
        lq_sq(pv, home + 0x30);
        lq_sq(pv + 0x10, home + 0x70);
        ee_stack_free(0x20);
        return;
    }
    if (st == 0) {
        if (lw(pv + 0x24) == -1) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        sw(pv + 0x2c, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x2c))));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        return;
    }
    if (st == 2) {
        const int32_t t = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c)));
        if (t == 0 && lbu(m + 0xbc) == 1) {
            uid5_set_pvar_80(pv);
            uid5_set_pvar_90(pv + 0x10);
            return;
        }
        hydro_2eac18(lw(pv + 0x30) != 0 ? 2u : 0u);  // jal L1:0x317070 (Veldin's 0x2eac18, the scripted camera's end)
        hero_enter_idle();
        sw(OV(0x0015f404), 0);
        sb(m + 0x20, 3);
        sb(m + 0xbc, 0);
        return;
    }
    if (st == 3) {
        if (lbu(m + 0xbc) == 1) sb(m + 0x20, 1);
    }
}

// orig: (none) -- Novalis's registrations
void novalis_moby_fns_register() {
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002ece90), empty_update_2ece90_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030d200), empty_update_30d200_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f6128), draw_hook_676_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f6180), draw_hook_678_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x00309c98), draw_hook_1225_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f2eb8), zone_marker_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f8268), water_lily_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002ff118), plumber_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030acb8), help_controller_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002efc60), peckbot_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f4710), door_half_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002fed68), door_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f95c0), piston_platform_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f9bf0), fan_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002fb8a8), bridge_half_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002fb5b0), scripted_camera_trigger_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002b9eb0), lift_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030d0f0), tall_shroom_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x00307c48), mushroom_underside_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002fd9a0), green_mushroom_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f4348), cave_door_update_l1);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x00300220), spawn_point_update_l1);
    novalis_moby_fns_register2();
}

// Whether the moby is in state 1 (the Veldin address; Novalis's 0x300518).
// orig: 0x002d8128
int32_t moby_state_is_1(uint32_t m) { return lbu(m + 0x20) == 1 ? 1 : 0; }

// Whether the moby's bounding position (its first quad, scaled by 1/1024) is on the screen within `r`.
// (Shared: the same code on Kerwan, Eudora, Rilgar, Umbris, Pokitaru, Hoven, Gemlik and Oltanis.)
// orig: L1:0x00275690 L3:0x0024eec0 L4:0x002534a8 L5:0x0028a858 L7:0x00288e20 L11:0x00285d90 L12:0x00279238 L13:0x0026c818 L14:0x00268b50
int64_t moby_on_screen_l1(uint32_t m, float r) {
    const uint32_t buf = ee_stack_alloc(0x10);
    vec4_scale(V(buf), V(m), u2f(0x3a800000u));
    const int64_t res = point_on_screen(buf, r);
    ee_stack_free(0x10);
    return res;
}

// Class 1341, the help-message controller (an invisible moby of the level): each frame it asks sub_00203908 for
// the level's hints when their conditions hold: (the scene not scripted) the first hints by the two flags
// 0x13dd42 / 0x13dd43 (1000 / 0x3e9 / 0x3ea with options 4 / 5 / 6), the box hints when the hero stands in the
// boxes pvar +0x3c / +0x38 / +0x30 (0x3ef, 0x3ec, 0x3ed), the crank hint (0x3ee) near a crank (class 0x118) not
// yet turned, the hint for staying in boxes +0x20 / +0x24 (0x3f0), the hints that fade by time (0x4e27 /
// 0x4e28), and the class-0x326 mobys near him (1 / 2). The counters of the hints shown (0x141848 +0xb0..,
// +0xb8.., +0x68..) record how often and when, and the level's bit.
// orig: L1:0x0030acb8
void help_controller_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t S = 0x00141848, H = 0x00141968, P = g::PLAYER, hero = 0x0013f3d0;
    const uint32_t G = OV(0x00179410);  // (the hud message state: +0 busy, +0x24 its message)
    auto idle = [&] { return lw(G) == 0 && lw(G + 0x24) == -1; };
    auto running = [&] {
        const int32_t run = lw(P + 0x208c);
        return static_cast<uint32_t>(run) < 2u || run == 9;
    };
    // a hint shown: how often (+o1), when (+o2, in 600-frame units) and the level's bit (+o3)
    auto bump = [&](uint32_t o1, uint32_t o2, uint32_t o3) {
        const uint32_t v1 = lhu(S + o1);
        if (!(0xfffeu < v1)) sh(S + o1, v1 + 1);
        const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 600);
        uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (static_cast<int32_t>(lhu(S + o2)) < q) {
            sh(S + o2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
            level = static_cast<uint32_t>(lw(0x0015ed84));
        }
        sw(S + o3, lwu(S + o3) | (1u << (level & 0x1f)) | 0x80000000u);
    };
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18)));
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sw(pv + 0x14, 0);
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        sw(pv + 0x28, 0);
        return;
    }
    if (st != 1) return;
    if (lw(OV(0x0015f5cc)) < frame_scale_i(400) && lhu(S + 0x60) != 0 && lhu(S + 0x58) == 0) {
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(S + 0x62)) * 600;
        if (since < frame_scale_i(0x44c) && !(lw(H + 0x28c) < 0)) sub_00203908(0x4e27, 0x51);
    }
    if (lw(OV(0x0015f5cc)) < frame_scale_i(400) && lhu(S + 0xe8) != 0) {
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(S + 0xea)) * 600;
        if (since < frame_scale_i(0x44c) && !(lw(H + 0x394) < 0)) sub_00203908(0x4e28, 0x72);
    }
    if (lw(OV(0x0015f5c4)) == 0) {
        if (lbu(0x0013dd43) != 0) {
            if (lbu(0x0013dd42) == 0) {
                if (idle() && lhu(H + 0x20) == 0) sub_00203908(1000, 4);
            } else if (idle() && lhu(H + 0x30) == 0 && lhu(H + 0x20) == 0) {
                sub_00203908(0x3ea, 6);
            }
        } else if (lbu(0x0013dd42) != 0 && idle() && lhu(H + 0x28) == 0) {
            sub_00203908(0x3e9, 5);
        }
    }
    if (point_in_box(hero, lw(pv + 0x3c)) != 0 && running() && idle()) {
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(0x00141892)) * 600;
        if (frame_scale_i(0x34bc0) < since) {
            const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
            if ((lwu(H + 0x21c) & (1u << (level & 0x1f))) == 0) sub_00203908(0x3ef, 0x43);
        }
    }
    if (point_in_box(hero, lw(pv + 0x38)) != 0 && running()) {
        if (lw(OV(0x0015f68c)) < 3 && idle() && lhu(H + 0x200) == 0) sub_00203908(0x3ec, 0x40);
    }
    if (point_in_box(hero, lw(pv + 0x38)) != 0 && running() && lw(P + 0x2084) == 1) {
        if (lw(OV(0x0015f688)) == -1 && idle() && lhu(H + 0x200) != 0 && lhu(H + 0x208) == 0) sub_00203908(0x3ed, 0x41);
    }
    if (lhu(H + 0x210) < 2u && point_in_box(hero, lw(pv + 0x30)) != 0 && lw(P + 0x2090) == 0x3b && lw(P + 0x198) == 0) {
        for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
            if (lh(o + 0xa6) != 0x118) continue;
            if (!(vec3_distance(V(o + 0x10), V(hero)) < 3.0f)) continue;
            if (lwc1(lwu(o + 0x78)) == 1.0f) continue;
            const int32_t n = lw(pv + 0x34) + 1;
            sw(pv + 0x34, static_cast<uint32_t>(n));
            if (n == 3 || n == 6) sub_00203908(0x3ee, 0x42);
        }
    }
    // staying in a box of the stay-put hints (+0x20 / +0x24 with the frame count +0x2c): shown once, logged
    if (lw(P + 0x208c) == 0x12 && lw(pv + 0x28) == 0 && idle() && (point_in_box(hero, lw(pv + 0x20)) != 0 || point_in_box(hero, lw(pv + 0x24)) != 0)) {
        sw(pv + 0x2c, lwu(pv + 0x2c) + 1);
        bool shown = false;
        if (point_in_box(hero, lw(pv + 0x20)) != 0 && frame_scale_i(0xf0) < lw(pv + 0x2c) && lhu(S + 0xb0) == 0) {
            sub_00203908(0x3f0, 0x44);
            bump(0xb0, 0xb2, 0xb4);
            shown = true;
        }
        if (!shown && point_in_box(hero, lw(pv + 0x24)) != 0 && frame_scale_i(0x168) < lw(pv + 0x2c) && lhu(S + 0xb8) == 0) {
            sub_00203908(0x3f0, 0x44);
            bump(0xb8, 0xba, 0xbc);
        }
    } else if (lw(P + 0x208c) != 0x11) {
        sw(pv + 0x2c, 0);
    } else {
        sh(H + 0x220, 0xffff);
    }
    // the class-0x326 mobys within 10 of the hero while nothing else is up
    for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        if (lh(o + 0xa6) != 0x326) continue;
        if (!(vec3_distance(V(o + 0x10), V(hero)) < 10.0f)) continue;
        if (!idle()) continue;
        if (lhu(H + 8) != 0 || lhu(H + 0x10) != 0 || lhu(S + 0x68) != 0) continue;
        if (moby_state_is_1(o) != 0) continue;
        if (lw(hero + 0x2228) < lw(0x0015eda0)) {
            bump(0x68, 0x6a, 0x6c);
            sub_00203908(1, 1);
        } else if (moby_on_screen_l1(o, u2f(0x437f0000u)) != 0) {
            sub_00203908(2, 2);
        }
    }
}

}  // namespace rac1
