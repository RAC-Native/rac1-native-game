// Novalis's own moby classes, part 4: the NPCs (the Plumber). Header novalis_mobys.h. Addresses are Novalis's (`orig:
// L1:`); Novalis-only data is L1_ADDR, the globals
// the engine shares with Veldin are the level's own (OV).
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l8.h>
#include <game/port_stubs.h>

namespace rac1 {

// A level-data change noted for the checkpoint / the saved level state (Novalis's 0x29b0a0): `size` bytes at
// `at` (relative to the moby, kind 1, or to its pvar block, else), in the list `list` (count +0, 16-byte
// entries from +4: offset, the bytes, size +8, kind +0xa, the moby's +0xb2 / class +0xc / +0xe): the entry's
// bytes replaced when the same one is there (and its kind is not 0), else a new entry.
// (Shared: the same code on Kerwan, Eudora, Rilgar and Batalia.)
// orig: L1:0x0029b0a0 L3:0x00273f50 L4:0x00278610 L5:0x002af5e8 L8:0x0028fa30
void moby_data_note_l1(uint32_t at, int32_t size, uint32_t m, int32_t kind, uint32_t list) {
    const int32_t uid = lh(m + 0xb2), cls = lh(m + 0xa6);
    int32_t s1 = kind;
    const int16_t off = static_cast<int16_t>(kind == 1 ? at - m : at - lhu(m + 0x78));
    const int32_t n = lw(list);
    uint32_t e = list + 4;
    for (int32_t i = 0; i < n; ++i, e += 0x10) {
        if (lw(e) != off || lhu(e + 8) != static_cast<uint32_t>(size) || uid != lh(e + 0xc) || cls != lh(e + 0xe)) continue;
        s1 = lh(e + 0xa);
        if (s1 != 0) {
            mem_copy(e + 4, at, size);
            return;
        }
    }
    const uint32_t s0 = list + 4 + static_cast<uint32_t>(n) * 16;
    sw(list, static_cast<uint32_t>(n + 1));
    sw(s0, static_cast<uint32_t>(static_cast<int32_t>(off)));
    mem_copy(s0 + 4, at, size);
    sh(s0 + 0xe, static_cast<uint32_t>(cls));
    sh(s0 + 8, static_cast<uint32_t>(size));
    sh(s0 + 0xa, static_cast<uint32_t>(s1));
    sh(s0 + 0xc, static_cast<uint32_t>(uid));
}

// While the scripted camera runs (0x15f5c4 == 2, 0x15edb0 set) the mobys of the hero's block list (the engine's
// 0x16c860, Novalis's 0x16cce0: count +0x44, pointers from +0x178) of the moby's own class or 0x32b are given the
// camera node (the engine's 0x17c440, Novalis's 0x17c8c0; as 0x2fb4b8) unless its byte +1 is set, and its +0x20 /
// +0x24 / +0x28 set from the level's own float (Novalis's 0x161d30, Batalia's 0x161fb0).
// (Shared: the same code on Batalia.)
// orig: L1:0x002ff028 L8:0x002f8fd8
void plumber_camera_nodes_l1(uint32_t m) {
    if (lw(OV(0x0015f5c4)) != 2) return;
    if (lbu(0x0015edb0) == 0) return;
    const uint32_t list = OV(0x0016c860);
    if (lh(list + 0x44) <= 0) return;
    const uint32_t node = OV(0x0017c440);
    for (int32_t i = 0; i < lh(list + 0x44); ++i) {
        const uint32_t mm = lwu(list + 0x178 + static_cast<uint32_t>(i) * 4);
        const int16_t cls = lh(mm + 0xa6);
        if (cls != lh(m + 0xa6) && cls != 0x32b) continue;
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(mm, 0, node);
        const float v = lwc1(on_level(BATALIA) ? L8_ADDR(0x00161fb0) : L1_ADDR(0x00161d30));
        swc1(node + 0x28, v);
        swc1(node + 0x20, v);
        swc1(node + 0x24, v);
    }
}

// The scene (the engine's 0x16c860, Novalis's 0x16cce0) told to put the hero at a spot `r` in front of moby `m` (along
// its facing, +0x48) when it ends (cutscene.cpp): the flag half +0x46 = 1, the target position (m's quad, moved by cos /
// sin * r) at +0x10, the target angles +0x20 cleared with the facing back toward the moby (+pi) at +0x28. (The PAL 2.00
// build keeps the flag as a byte at +0x4a, its scene code reading it there; the port's scene code reads +0x46.)
// (Shared: the same code on Aridia, Kerwan, Rilgar, Blarg, Batalia, Pokitaru, Hoven, Oltanis and Kalebo III.)
// orig: L1:0x002783a8 L2:0x00264d28 L3:0x00251988 L5:0x0028d960 L6:0x00272670 L8:0x0026d768 L11:0x00288e98 L12:0x0027bea8 L14:0x0026b390 L16:0x0025e948
void hero_walk_to_l1(float r, uint32_t m) {
    const uint32_t B = OV(0x0016c860);
    sh(B + 0x46, 1);
    lq_sq(B + 0x10, m + 0x10);
    swc1(B + 0x10, lwc1(B + 0x10) + cos_vu(lwc1(m + 0x48)) * r);
    swc1(B + 0x14, lwc1(B + 0x14) + sin_vu(lwc1(m + 0x48)) * r);
    vec_clear(V(B + 0x20));
    swc1(B + 0x28, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
}

// Class 774, the Plumber (an NPC of the talk system, talk_init / talk_update on its pvar block: the name string
// +0x20 -- "Water Pump Worker" on Novalis, which removes it once the flag 0x13dd42 is set --, the talk state +4 /
// +6 / +8): state 0 starts the talk, 1 talks (then the hero walks to a spot 2.2 in front, 2), 2 -- when the
// scripted camera is not running -- marks the moby's collectible (0x14c050 row), takes the checkpoint at the
// box pvar +0x4c (checkpoint_save), and at the talk's end (+4 == 4) marks level 2 visited, notes the moby's
// state change (moby_data_note_l1) and asks for the level change (level_change_request(0, -1)); 3 retires;
// 4 / 5 talk again and place the hero at (130.5, 299.5, 46) in state 6. Every state turns its head to
// the hero within 8 (a spring on the joint nodes +0x50 / +0xd0): the looking pose follows the hero's
// target (0x13f420), a wander target when he is near, else randomly 6 away every 180..300 frames.
// orig: L1:0x002ff118
void plumber_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t P = g::PLAYER, hero = 0x0013f3d0;
    plumber_camera_nodes_l1(m);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 32.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x1a);
    }
    const uint32_t st = lbu(m + 0x20);
    switch (st < 7 ? st : 6u) {
    case 0: {
        sb(m + 0x30, 0xff);
        sw(pv + 0x44, 0);
        const int32_t level = lw(0x0015ed84);
        if (level == 1) {
            if (lbu(0x0013dd42) != 0) {
                moby_retire(m);
                return;
            }
            sw(pv + 0x20, L1_ADDR(0x0020b600));
            sb(m + 0x20, 1);
        } else {
            sw(pv + 0x20, level == 8 ? L1_ADDR(0x00161d38) : L1_ADDR(0x00161d48));
            sb(m + 0x20, level == 8 ? 4u : 7u);
        }
        talk_init(m, pv);
        break;
    }
    case 1:
        if (talk_update(m, pv) != 0) {
            hero_walk_to_l1(u2f(0x400ccccdu), m);
            sb(m + 0x20, 2);
        }
        break;
    case 2: {
        if (lw(OV(0x0015f5c4)) == 2) break;
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        const uint32_t idx = lbu(m + 0xb0);
        if (lbu(0x0014c050 + idx + (level << 4)) != 0xff) {
            checkpoint_mark_collected(idx);
            if (lw(pv + 0x4c) != -1) {
                const uint32_t box = lwu(OV(0x001600ec)) + (static_cast<uint32_t>(lw(pv + 0x4c)) << 7);
                checkpoint_save(box + 0x30, box + 0x70);
            }
        }
        sb(m + 0x20, 1);
        if (lh(pv + 4) == 4) {
            level_mark_visited(2);
            level_arrival_message(2);
            sb(m + 0x20, 3);
            moby_data_note_l1(m + 0x20, 1, m, 1, OV(0x001ba620));
            level_change_request(0, -1);
        }
        break;
    }
    case 3:
        moby_retire(m);
        return;
    case 4:
        if (talk_update(m, pv) != 0) {
            sb(m + 0x20, 5);
            sb(pv + 8, 0);
        }
        break;
    case 5:
        if (lw(OV(0x0015f5c4)) == 2) break;
        if (lh(pv + 6) != 2) {
            sb(m + 0x20, 4);
        } else {
            swc1(hero, u2f(0x43028000u));
            swc1(hero + 8, u2f(0x42380000u));
            swc1(hero + 4, u2f(0x4395c000u));
            sb(m + 0x20, 6);
        }
        break;
    default:
        break;
    }
    // the head: (while its animation is the first one) turn toward the hero's target, within 8 of the moby
    float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);
    const uint32_t buf = ee_stack_alloc(0x40);
    const uint32_t tp = buf, vp = buf + 0x10, dv = buf + 0x20;
    bool looking = false;
    if (lbu(m + 0x53) == 0) {
        looking = true;
        if (vec2_distance(V(m + 0x10), V(hero)) < 8.0f) {
            const float yaw = atan2_f(lwc1(P + 0xd0) - lwc1(m + 0x10), lwc1(P + 0xd4) - lwc1(m + 0x14));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                if (u2f(0x3c23d70au) < vec3_length(V(0x0013f450))) sw(pv + 0x164, static_cast<uint32_t>(frame_scale_i(0x78)));
                else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x164)));
            } else if (lw(pv + 0x164) != 0) {
                sw(pv + 0x164, 0);
                lq_sq(pv + 0x150, 0x0013f420);
            }
        } else if (lw(pv + 0x164) != 0) {
            sw(pv + 0x164, 0);
            lq_sq(pv + 0x150, 0x0013f420);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x168))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            const uint32_t at = pv + 0x150;
            const float wait = rand_range_f(u2f(0x43340000u), u2f(0x43960000u));
            sw(pv + 0x168, static_cast<uint32_t>(float_to_int(frame_scale_f(wait))));
            const float yaw = angle_add(lwc1(m + 0x48), rand_range_f(u2f(0xc2b40000u), u2f(0x42b40000u)) * deg);
            const float tilt = rand_range_f(0.0f, u2f(0x41f00000u));
            vec_from_spherical_v(V(at), u2f(0x40c00000u), yaw, tilt * deg);
            vec3_add(V(at), V(at), V(m + 0x10));
        }
        if (lw(pv + 0x164) != 0) {
            lq_sq(tp, 0x0013f420);
            k = u2f(0x3d23d70au);
        } else {
            lq_sq(tp, pv + 0x150);
        }
    }
    if (looking) {
        lq_sq(vp, m + 0x10);
        swc1(vp + 8, lwc1(vp + 8) + 1.0f);
        vec3_sub(V(dv), V(tp), V(vp));
        float a = angle_sub(atan2_f(lwc1(dv), lwc1(dv + 4)), lwc1(m + 0x48));
        const float pitch = atan2_f(vec2_length(V(dv)), lwc1(dv + 8));
        float b = -pitch;
        if (u2f(0x3fc90fdbu) < a) a = u2f(0x3fc90fdbu);
        else if (a < u2f(0xbfc90fdbu)) a = u2f(0xbfc90fdbu);
        if (u2f(0x3f060a92u) < b) b = u2f(0x3f060a92u);
        else if (b < u2f(0xbf060a92u)) b = u2f(0xbf060a92u);
        swc1(pv + 0x138, a * 0.5f);
        swc1(pv + 0xb4, b * u2f(0x3fa00000u));
        swc1(pv + 0xb8, a * 0.5f);
    }
    float dt = lwc1(0x0015ed64);
    if (lbu(0x0015edb0) != 0) {
        swc1(pv + 0xc0, u2f(0x40300000u));
        dt = lwc1(0x0015ed64);
    }
    joint_node_spring(m, pv + 0x50, 0, k * dt, d * dt);
    joint_node_spring(m, pv + 0xd0, 1, k * lwc1(0x0015ed64), d * lwc1(0x0015ed64));
    ee_stack_free(0x40);
}

}  // namespace rac1
