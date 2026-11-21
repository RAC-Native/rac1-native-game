// Batalia (level 8) own moby classes, hand-ported: the planet's four talking characters and their cutscene / turret
// helpers — class 774 (the Plumber), class 1130 (the Deserter who gives the Hoven Infobot), class 1144 (the Commando at
// the rail's end who gives the Gaspar Infobot, with his big-head scene helper), and class 1283 (the Commander at the
// anti-air turret who plays the Blarg Fleet movie and gives the Metal Detector, with his scene helper and the turret-mount
// helper). Registered in batalia_mobys.cpp. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR, the base ELF's
// are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/movie_mode.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_LOOK = 0x0013f420;
constexpr uint32_t HERO_VEL = 0x0013f450;

// The shared head-tracking and joint-node spring tail of Batalia's four NPCs: while animation `idle_anim` plays, tracks
// Ratchet's look point (0x13f420) within 8 horizontally and pi/2 in yaw, or wanders to a random target every 180 .. 300
// scaled frames, clamping yaw to +-pi/2 and pitch to +-pi/6; applies big-head scale 2.75 when 0x15edb0 is set, then steps
// joint nodes 0 and 1.
enum class AngleOrder { Plumber, Deserter, Commando };

void npc_head_look(uint32_t m, uint32_t pv, uint32_t idle_anim, uint32_t wander_off, uint32_t track_off, uint32_t timer_off,
                   uint32_t node0_off, uint32_t node1_off, float fast_k, float yaw0_mul, float yaw1_mul, AngleOrder order) {
    float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);  // (0.02, 0.3)
    const uint32_t sp = ee_stack_alloc(176);
    const uint32_t tp = sp, vp = sp + 0x10, dv = sp + 0x20;
    const uint32_t wander = pv + wander_off, track = pv + track_off, timer = pv + timer_off;
    bool looking = false;

    if (lbu(m + 0x53) == idle_anim) {
        looking = true;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO_LOOK), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_LOOK + 4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                if (u2f(0x3c23d70au) < vec3_length(V(HERO_VEL))) sw(track, static_cast<uint32_t>(frame_scale_i(0x78)));
                else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(track)));
            } else if (lw(track) != 0) {
                sw(track, 0);
                lq_sq(wander, HERO_LOOK);
            }
        } else if (lw(track) != 0) {
            sw(track, 0);
            lq_sq(wander, HERO_LOOK);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            const float wait = rand_range_f(180.0f, 300.0f);
            sw(timer, static_cast<uint32_t>(float_to_int(frame_scale_f(wait))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float tilt = rand_range_f(0.0f, 30.0f);
            vec_from_spherical_v(V(wander), 6.0f, yaw, fpu_mul(tilt, deg));
            vec3_add(V(wander), V(wander), V(m + 0x10));
        }
        if (lw(track) != 0) {
            lq_sq(tp, HERO_LOOK);
            k = fast_k;
        } else {
            lq_sq(tp, wander);
        }
    }

    const uint32_t node0 = pv + node0_off, node1 = pv + node1_off;
    if (looking) {
        lq_sq(vp, m + 0x10);
        swc1(vp + 8, fpu_add(lwc1(vp + 8), 1.0f));
        vec3_sub(V(dv), V(tp), V(vp));
        float yaw = angle_sub(atan2_f(lwc1(dv), lwc1(dv + 4)), lwc1(m + 0x48));
        const float pitch_raw = atan2_f(vec2_length(V(dv)), lwc1(dv + 8));
        float pitch = -pitch_raw;
        if (u2f(0x3fc90fdbu) < yaw) yaw = u2f(0x3fc90fdbu);
        else if (yaw < u2f(0xbfc90fdbu)) yaw = u2f(0xbfc90fdbu);
        if (u2f(0x3f060a92u) < pitch) pitch = u2f(0x3f060a92u);
        else if (pitch < u2f(0xbf060a92u)) pitch = u2f(0xbf060a92u);

        if (order == AngleOrder::Plumber) {
            const float y = fpu_mul(yaw, yaw0_mul);
            const float p = fpu_mul(pitch, 1.25f);
            swc1(node1 + 0x68, y);
            swc1(node0 + 0x64, p);
            swc1(node0 + 0x68, y);
        } else if (order == AngleOrder::Commando) {
            const float y = fpu_mul(yaw, yaw0_mul);
            swc1(node0 + 0x64, pitch);
            swc1(node1 + 0x68, y);
            swc1(node0 + 0x68, y);
        } else {
            const float y0 = fpu_mul(yaw, yaw0_mul);
            const float y1 = fpu_mul(yaw, yaw1_mul);
            swc1(node0 + 0x64, pitch);
            swc1(node0 + 0x68, y0);
            swc1(node1 + 0x68, y1);
        }
    }

    float dt = lwc1(0x0015ed64);
    if (lbu(0x0015edb0) != 0) {
        swc1(node0 + 0x70, 2.75f);
        dt = lwc1(0x0015ed64);
    }
    joint_node_spring(m, node0, 0, fpu_mul(k, dt), fpu_mul(d, dt));
    const float dt2 = lwc1(0x0015ed64);
    joint_node_spring(m, node1, 1, fpu_mul(k, dt2), fpu_mul(d, dt2));
    ee_stack_free(176);
}
}  // namespace

// Class 774, Batalia's copy of the Plumber (states 0 .. 6, jump table L8:0x001fc370: on level 8 state 0 sets his name
// 0x161fb8 and enters state 4; state 4 / 5 run his talk and place Ratchet at (130.5, 299.5, 46) in state 6).
// orig: L8:0x002f90c8
void bat_class774_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    plumber_camera_nodes_l1(m);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0))) < 32.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x1a);
    }

    switch (lbu(m + 0x20)) {  // (the jump table 0x1fc370: states 0 .. 6; 6 and >= 7 go to 0x2f9380)
    case 0: {  // (0x002f9178)
        sb(m + 0x30, 0xff);
        sw(pv + 0x44, 0);
        const int32_t level = lw(0x0015ed84);
        if (level == 1) {
            if (lbu(0x0013dd42) != 0) {
                moby_retire(m);
                return;
            }
            sw(pv + 0x20, L8_ADDR(0x001fc350));
            sb(m + 0x20, 1);
        } else if (level == 8) {
            sw(pv + 0x20, L8_ADDR(0x00161fb8));
            sb(m + 0x20, 4);
        } else {
            sw(pv + 0x20, L8_ADDR(0x00161fc8));
            sb(m + 0x20, 7);
        }
        talk_init(m, pv);
        break;
    }
    case 1:  // (0x002f91f4)
        if (talk_update(m, pv) != 0) {
            hero_walk_to_l1(u2f(0x400ccccdu), m);
            sb(m + 0x20, 2);
        }
        break;
    case 2: {  // (0x002f9224)
        if (lw(L8_ADDR(0x0015f5c4)) == 2) break;
        const uint32_t level = lwu(0x0015ed84);
        const uint32_t idx = lbu(m + 0xb0);
        if (lbu(0x0014c050u + idx + (level << 4)) != 0xff) {
            checkpoint_mark_collected(idx);
            if (lw(pv + 0x4c) != -1) {
                const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x4c));
                checkpoint_save(box + 0x30, box + 0x70);
            }
        }
        sb(m + 0x20, 1);
        if (lh(pv + 4) == 4) {
            level_mark_visited(2);
            level_arrival_message(2);
            sb(m + 0x20, 3);
            moby_data_note_l1(m + 0x20, 1, m, 1, L8_ADDR(0x001bae20));
            level_change_request(0, -1);
        }
        break;
    }
    case 3:  // (0x002f92ec)
        moby_retire(m);
        return;
    case 4:  // (0x002f92fc)
        if (talk_update(m, pv) != 0) {
            sb(m + 0x20, 5);
            sb(pv + 8, 0);
        }
        break;
    case 5:  // (0x002f931c)
        if (lw(L8_ADDR(0x0015f5c4)) == 2) break;
        if (lh(pv + 6) != 2) {
            sb(m + 0x20, 4);
        } else {
            swc1(HERO_POS, 130.5f);
            swc1(HERO_POS + 8, 46.0f);
            swc1(HERO_POS + 4, 299.5f);
            sb(m + 0x20, 6);
        }
        break;
    default:  // (0x002f9380)
        break;
    }

    npc_head_look(m, pv, 0, 0x150, 0x164, 0x168, 0x50, 0xd0, u2f(0x3d23d70au), 0.5f, 0.5f, AngleOrder::Plumber);
}

// Class 1130, the Deserter. States: 0 sets up talk at pv + 0x20 (name 0x1621e0, reach 3) and places him at box +0x180 in
// state 2 once 0x13d3b4 is set (retired if 0x13dd4a and his collectible byte are set); 1 talks, then moves him to box
// +0x180, sets 0x13d3b4 and state 2; 2 idles between animations 1 and 2 and talks (walks Ratchet to 2.2 in front, state 3);
// 3 after the scene either returns to state 2 (+0x28 = 1) or, at talk +0x24 == 3, unlocks Hoven (level 10), saves the
// checkpoint at Ratchet and requests the level change.
// orig: L8:0x00302ce8
void bat_class1130_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0))) < static_cast<float>(lw(L8_ADDR(0x001621d8)))) {
        moby_floor_band(m);
        sb(m + 0x7f, static_cast<uint32_t>((lw(L8_ADDR(0x001621d8)) * 0xd) >> 4));
    }

    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        if (lbu(0x0013dd4a) != 0 && lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) {
            moby_retire(m);
            return;
        }
        swc1(pv + 0x2c, 3.0f);
        sw(pv + 0x40, L8_ADDR(0x001621e0));
        if (lbu(0x0013d3b4) == 0) {
            sb(m + 0x20, 1);
        } else {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x180));
            lq_sq(m + 0x10, box + 0x30);
            lq_sq(m + 0x40, box + 0x70);
            sb(m + 0x20, 2);
        }
        talk_init(m, pv + 0x20);
        break;
    case 1:
        if (talk_update(m, pv + 0x20) != 0) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x180));
            lq_sq(m + 0x10, box + 0x30);
            lq_sq(m + 0x40, box + 0x70);
            sb(m + 0x20, 2);
            sb(0x0013d3b4, 1);
        }
        break;
    case 2:
        if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != static_cast<uint32_t>(rand_mod(2) + 1)) {
            const uint32_t next_anim = static_cast<uint32_t>(rand_mod(2) + 1);
            moby_anim_set(m, next_anim, 0, frame_scale_i(0x14));
        }
        if (talk_update(m, pv + 0x20) != 0) {
            sb(m + 0x20, 3);
            hero_walk_to_l1(u2f(0x400ccccdu), m);
        }
        break;
    case 3:
        if (lw(L8_ADDR(0x0015f5c4)) == 2) break;
        if (lh(pv + 0x24) != 3) {
            sb(pv + 0x28, 1);
            sb(m + 0x20, 2);
        } else {
            level_mark_visited(0xa);
            level_arrival_message(0xa);
            checkpoint_mark_collected(lbu(m + 0xb0));
            checkpoint_save(HERO_POS, HERO_POS + 0x10);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        break;
    default:
        break;
    }

    npc_head_look(m, pv, 1, 0x160, 0x188, 0x18c, 0x60, 0xe0, u2f(0x3d23d70au), u2f(0x3f19999au), u2f(0x3ecccccdu),
                  AngleOrder::Deserter);
}

// Big-head cutscene helper for the Commando `m`: during game mode 2 with 0x15edb0 set, nudges 0x15f59c / 0x15f5a0 on
// frames 442 .. 643 of scene 3 and scales node 0 (0x17cc40) of matching scene mobys by 2.75.
// orig: L8:0x00305138
void commando_scene_scale_l8(uint32_t m) {
    if (lw(L8_ADDR(0x0015f5c4)) != 2 || lbu(0x0015edb0) == 0) return;
    if (lw(L8_ADDR(0x0016d090)) == 3) {
        const int32_t frame = lw(L8_ADDR(0x0016d094));
        if (frame >= 442 && frame < 644) {
            swc1(L8_ADDR(0x0015f59c), 1.0f);
            swc1(L8_ADDR(0x0015f5a0), u2f(0xbe99999au));
        }
    }
    const uint32_t scene = L8_ADDR(0x0016d060);
    for (int32_t i = 0; i < lh(scene + 0x44); ++i) {
        const uint32_t sm = lwu(scene + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(sm + 0xa6) != lh(m + 0xa6)) continue;
        const uint32_t node = L8_ADDR(0x0017cc40);
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(sm, 0, node);
        swc1(node + 0x28, 2.75f);
        swc1(node + 0x20, 2.75f);
        swc1(node + 0x24, 2.75f);
    }
}

// Class 1144, the Commando. States: 0 sets up talk at pv + 0x20 (name 0x1622e8, reach 4; retired if 0x13dd49 and his
// collectible byte are set); 1 marks his collectible once Ratchet is within 8, state 2; 2 updates talk while Ratchet is
// grounded off a platform (+0x2fc == 0 and +0x300 > 30 scaled frames, or +0x56 != 0) and at +0x24 == 4 unlocks Gaspar
// (level 9), requests the level change and retires.
// orig: L8:0x00305270
void bat_class1144_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    commando_scene_scale_l8(m);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0))) < 30.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }

    switch (lbu(m + 0x20)) {
    case 0:
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        if (lbu(0x0013dd49) != 0 && lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x40, L8_ADDR(0x001622e8));
        swc1(pv + 0x2c, 4.0f);
        sb(m + 0x20, 1);
        if (lw(pv + 0x64) >= 0) talk_init(m, pv + 0x20);
        break;
    case 1:
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
            checkpoint_mark_collected(lbu(m + 0xb0));
            sb(m + 0x20, 2);
        }
        break;
    case 2:
        if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != static_cast<uint32_t>(rand_mod(2) + 1)) {
            const uint32_t next_anim = static_cast<uint32_t>(rand_mod(2) + 1);
            moby_anim_set(m, next_anim, 0, frame_scale_i(0x14));
        }
        if (lw(PLAYER + 0x2fc) == 0 && (frame_scale_i(0x1e) < lw(PLAYER + 0x300) || lh(pv + 0x56) != 0)) {
            talk_update(m, pv + 0x20);
            hero_walk_to_l1(u2f(0x400ccccdu), m);
        }
        if (lh(pv + 0x24) == 4) {
            level_mark_visited(9);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        break;
    default:
        break;
    }

    npc_head_look(m, pv, 1, 0x180, 0x194, 0x198, 0x80, 0x100, u2f(0x3d23d70au), 0.5f, 0.5f, AngleOrder::Commando);
}

// Mounts Ratchet onto the anti-air turret `turret` (+0xbc = 1, hero state 0x32).
// orig: L8:0x002e1698
int32_t turret_mount_start_l8(uint32_t turret) {
    sb(turret + 0xbc, 1);
    return hero_set_state(0x32, 1);
}

// Big-head cutscene helper for the Commander `m`: during game mode 2 with 0x15edb0 set, nudges 0x15f59c / 0x15f5a0 on
// three windows of scene 6 (620 .. 715, 1150 .. 1520, 1530 .. 1709) and scales node 0 (0x17cc40) of matching scene mobys
// (and class 0x306) by 0x162300.
// orig: L8:0x003063f0
void commander_scene_scale_l8(uint32_t m) {
    if (lw(L8_ADDR(0x0015f5c4)) != 2 || lbu(0x0015edb0) == 0) return;
    const uint32_t scene = L8_ADDR(0x0016d060);
    if (lw(L8_ADDR(0x0016d090)) == 6) {
        const int32_t f = lw(L8_ADDR(0x0016d094));
        if (f >= 620 && f < 716) {
            swc1(L8_ADDR(0x0015f59c), 1.0f);
            swc1(L8_ADDR(0x0015f5a0), u2f(0xbe4ccccdu));
        }
    }
    if (lw(scene + 0x30) == 6) {
        const int32_t f = lw(scene + 0x34);
        if (f >= 1150 && f < 1521) {
            swc1(L8_ADDR(0x0015f59c), 1.0f);
            swc1(L8_ADDR(0x0015f5a0), 0.5f);
        }
    }
    if (lw(scene + 0x30) == 6) {
        const int32_t f = lw(scene + 0x34);
        if (f >= 1530 && f < 1710) {
            swc1(L8_ADDR(0x0015f59c), 1.0f);
            swc1(L8_ADDR(0x0015f5a0), u2f(0xbdcccccdu));
        }
    }
    for (int32_t i = 0; i < lh(scene + 0x44); ++i) {
        const uint32_t sm = lwu(scene + 0x178 + static_cast<uint32_t>(i) * 4);
        const int16_t scls = lh(sm + 0xa6);
        if (scls != lh(m + 0xa6) && scls != 0x306) continue;
        const uint32_t node = L8_ADDR(0x0017cc40);
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(sm, 0, node);
        const float v = lwc1(L8_ADDR(0x00162300));
        swc1(node + 0x28, v);
        swc1(node + 0x20, v);
        swc1(node + 0x24, v);
    }
}

// Class 1283, the Commander. States: 0 sets up talk at pv (name 0x162308; retired if his collectible byte is set); 1 talks
// (walks Ratchet to 2.7 in front), at +0x36 == 2 saves the checkpoint at box +0x48 and mounts Ratchet on turret +0x50 in
// state 2, at +0x04 == 3 plays movie 1 and enters state 5, at +0x04 == 4 loses Ratchet (+0x04 = -1); 2 waits for the
// turret's outcome; 5 marks his collectible, gives item 0x1b (the Metal Detector), requests the level change and retires.
// orig: L8:0x003065d8
void bat_class1283_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    commander_scene_scale_l8(m);

    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        if (lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x20, L8_ADDR(0x00162308));
        sb(m + 0x20, 1);
        talk_init(m, pv);
        break;
    case 1:
        if (talk_update(m, pv) != 0) hero_walk_to_l1(u2f(0x402ccccdu), m);
        if (lh(pv + 0x36) == 2) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x48));
            checkpoint_save(box + 0x30, box + 0x70);
            sb(m + 0x20, 2);
            const uint32_t turret = lwu(L8_ADDR(0x0015ffd8)) + (lwu(pv + 0x50) << 8);
            turret_mount_start_l8(turret);
        }
        if (lh(pv + 4) == 3) {
            level_movie_play(1);
            sb(m + 0x20, 5);
        }
        if (lh(pv + 4) == 4) {
            hero_lost();
            sh(pv + 4, 0xffffu);
        }
        break;
    case 5:
        checkpoint_mark_collected(lbu(m + 0xb0));
        item_give(0x1b, 1);
        level_change_request(0, -1);
        moby_retire(m);
        return;
    default:
        break;
    }

    npc_head_look(m, pv, 0, 0x160, 0x174, 0x178, 0x60, 0xe0, u2f(0x3cf5c28fu), u2f(0x3f333333u), u2f(0x3e99999au),
                  AngleOrder::Deserter);
}

}  // namespace rac1
