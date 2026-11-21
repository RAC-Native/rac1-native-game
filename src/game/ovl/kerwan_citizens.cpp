// Kerwan's own moby classes: the two citizens of the talk system who give the hero an item and send him on, class 890
// (Helga: the Swingshot, item 0xc) and class 909 (the Heli-Pack, item 2), and the scene helper class 890 runs. Header
// kerwan_mobys.h, registered in kerwan_mobys.cpp. Addresses are Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR,
// the engine's words shared with Veldin are the level's own (OV), the base ELF's (the hero 0x13f350, the frame times, the
// level rows of 0x14c050) are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l3.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (0x13f350 + 0x80)
constexpr uint16_t FERRY = 0x3f4;          // class 1012, the ferry the citizens call

// the moby of index `i` (the moby list in 0x100-byte steps)
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the level row of the collectibles' table (0x14c050 + level * 16) at `slot`
uint32_t level_row(uint32_t slot) { return 0x0014c050 + (static_cast<uint32_t>(lw(0x0015ed84)) << 4) + slot; }

// the ferry the citizen names in its pvar word `word` (an index into the moby list, -1 for none), when it is one: else 0
uint32_t ferry_of(uint32_t word) {
    if (lw(word) == -1) return 0;
    const uint32_t o = moby_at(lw(word));
    return lh(o + 0xa6) == FERRY ? o : 0u;
}

// the ferry called (+0x8c); with `note`, the change noted for the level's moby data (0x273f50, list L3:0x1ba720)
void ferry_call(uint32_t citizen, uint32_t ferry, bool note) {
    const uint32_t fpv = lwu(ferry + 0x78);
    sw(fpv + 0x8c, 1);
    if (note) moby_data_note_l1(fpv + 0x8c, 4, citizen, 2, L3_ADDR(0x001ba720));
}

// the ferry back to its own box (+0xa0 from the citizen's word `box`), no longer called
void ferry_release(uint32_t ferry, uint32_t box) {
    const uint32_t fpv = lwu(ferry + 0x78);
    sw(fpv + 0x7c, 0);
    sw(fpv + 0x8c, 0);
    sw(fpv + 0xa0, lwu(box));
}

// a given prize's moby `o` marked collected: its byte in the level's table 0x1ba5d0 (+0x454 by its index +0xb2), and in the
// table 0x1bb230 when its collectible slot (+0xb0) is none or that slot is collected on this level but not this visit
void prize_marks(uint32_t o) {
    sb(OV(0x001ba5d0) + static_cast<uint32_t>(static_cast<int32_t>(lh(o + 0xb2))) + 0x454, lbu(o + 0xb0) + 2u);
    const uint32_t slot = lbu(o + 0xb0);
    if (slot == 0xff || (lbu(OV(0x0015fc88) + slot) != 0xff && lbu(level_row(slot)) == 0xff))
        sb(OV(0x001bb230) + static_cast<uint32_t>(static_cast<int32_t>(lh(o + 0xb2))) + 0x454, slot + 2u);
}

// the checkpoint at the box of the citizen's word `word` (none for -1)
void checkpoint_at(uint32_t word) {
    if (lw(word) == -1) return;
    const uint32_t box = lvd::box(lvd::BOXES, lw(word));
    checkpoint_save(box + 0x30, box + 0x70);
}

// The citizens' look (Novalis's Plumber has the same code inline, with its own pvars and the pitch x1.25): with the hero
// within 8 and in front (within 90 degrees) it follows the hero's target (0x13f420) while he moves, for 120 frames after;
// else every 180 .. 300 frames a point 6 away, within 90 degrees of its facing and 30 up. Out: the turn toward the point
// (within 90 degrees each way) and the tilt (within 30), and the spring's stiffness (0.04 on the hero's target, else
// 0.02). False (nothing done) while the moby's +0x53 is set.
struct Look {
    uint32_t near_timer, wander_timer, point;
};
bool citizen_look(uint32_t m, uint32_t pv, const Look& w, uint32_t sp, float& turn, float& tilt, float& k) {
    if (lbu(m + 0x53) != 0) return false;
    const uint32_t near = pv + w.near_timer, wander = pv + w.wander_timer, point = pv + w.point;
    bool lost = true;
    if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
        const float to = atan2_f(fpu_sub(lwc1(0x0013f420), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f424), lwc1(m + 0x14)));
        if (angle_dist(lwc1(m + 0x48), to) < u2f(0x3fc90fdbu)) {
            lost = false;
            if (u2f(0x3c23d70au) < vec3_length(V(0x0013f450))) sw(near, static_cast<uint32_t>(frame_scale_i(0x78)));
            else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(near)));
        }
    }
    if (lost && lw(near) != 0) {
        sw(near, 0);
        lq_sq(point, 0x0013f420);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(wander))) != 0) {
        const float deg = u2f(0x3c8efa35u);
        sw(wander, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
        const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
        const float up = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
        vec_from_spherical_v(V(point), 6.0f, yaw, up);
        vec3_add(V(point), V(point), V(m + 0x10));
    }
    k = u2f(0x3ca3d70au);
    if (lw(near) != 0) {
        lq_sq(sp, 0x0013f420);
        k = u2f(0x3d23d70au);
    } else {
        lq_sq(sp, point);
    }
    const uint32_t from = sp + 0x10, d = sp + 0x20;
    lq_sq(from, m + 0x10);
    swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
    vec3_sub(V(d), V(sp), V(from));
    turn = angle_sub(atan2_f(lwc1(d), lwc1(d + 4)), lwc1(m + 0x48));
    tilt = -atan2_f(vec2_length(V(d)), lwc1(d + 8));
    const float right = u2f(0x3fc90fdbu), lim = u2f(0x3f060a92u);
    if (right < turn) turn = right;
    else if (turn < -right) turn = -right;
    if (lim < tilt) tilt = lim;
    else if (tilt < -lim) tilt = -lim;
    return true;
}
}  // namespace

// Class 890's scene work (game mode 2, with the frame's flag 0x15edb0): on scene 2 the camera's tilt word 0x15f5a0 to -0.2
// from frame 490 to 613 and to 0.2 from 750 to 800; every scene moby of its class gets its light node 0 (0x24f590 into
// 0x17c440, once) at the level's size L3:0x161bb4.
// orig: L3:0x002da710
void helga_scene_l3(uint32_t m) {
    if (lw(OV(0x0015f5c4)) != 2) return;
    if (lbu(0x0015edb0) == 0) return;
    constexpr OvConst<0x0016c860u> SC{};
    if (lw(SC + 0x30) == 2) {
        const int32_t f = lw(SC + 0x34);
        if (!(f < 490) && f < 614) swc1(OV(0x0015f5a0), u2f(0xbe4ccccdu));
        if (lw(SC + 0x30) == 2) {
            const int32_t g = lw(SC + 0x34);
            if (!(g < 750) && g < 801) swc1(OV(0x0015f5a0), u2f(0x3e4ccccdu));
        }
    }
    for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
        const uint32_t o = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != lh(m + 0xa6)) continue;
        const uint32_t rec = OV(0x0017c440);
        if (lbu(rec + 1) != 0) continue;
        sub_0024f590(o, 0, rec);
        const float size = lwc1(L3_ADDR(0x00161bb4));
        swc1(rec + 0x28, size);
        swc1(rec + 0x20, size);
        swc1(rec + 0x24, size);
    }
}

// Class 890, Helga (pvars: +0x80 the talk block (+4 / +0x36 its state, +0x20 its name L3:0x161bb8, +0x48 its kind 5),
// +0xcc a checkpoint box, +0xd0 the prize moby, +0xd4 the ferry it calls, +0xd8 the ferry's box after; the look's +0x1e0
// .. +0x1f4, the head's springs +0xe0 / +0x160). State 0: with the flag 0x13d4cc set (her course done) it calls the ferry
// and is gone; else, its collectible slot (+0xb0) collected on this level, it calls the ferry (noted); it starts the talk
// (in the hero's colour, 1); 1 talks (then the hero walks to a spot 2.3 in front, it calls the ferry, 2); 2: in a scene
// (game mode 2) at the talk's state 2 it releases the ferry; out of it: talk again (1) and, at the talk's state other than
// 2, the checkpoint and the ferry called; at the talk's state 2 the Swingshot (item 0xc) with its message 0xbc8, the prize
// moby marked and gone, the ferry released, a level change asked (0, -1) and the moby gone. Every frame its scene work
// (0x2da710) and its look.
// orig: L3:0x002da870
void kerwan_class890_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t pv = lwu(m + 0x78), talk = pv + 0x80;
    helga_scene_l3(m);
    const uint32_t st = lbu(m + 0x20);
    auto gone = [&] {
        moby_retire(m);
        ee_stack_free(0xb0);
    };
    if (st == 0) {  // (0x002da8e4)
        if (lbu(0x0013d4cc) != 0) {
            if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_call(m, f, true);
            gone();
            return;
        }
        if (lbu(level_row(lbu(m + 0xb0))) == 0xff)
            if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_call(m, f, true);
        sb(m + 0x20, 1);
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));  // the hero's colour
        sw(talk + 0x20, L3_ADDR(0x00161bb8));
        sw(pv + 0xc8, 5);
        talk_init(m, talk);
        band_set(m);
    } else if (st == 1) {  // (0x002daa00)
        if (talk_update(m, talk) != 0) {
            hero_walk_to_l1(u2f(0x40133333u), m);
            sb(m + 0x20, 2);
            if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_call(m, f, true);
        }
    } else if (st == 2) {  // (0x002daa84)
        if (lw(OV(0x0015f5c4)) == 2) {
            if (lh(pv + 0xb6) == 2)
                if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_release(f, pv + 0xd8);
        } else {
            if (lh(pv + 0x84) != 2) {
                checkpoint_mark_collected(lbu(m + 0xb0));
                if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_call(m, f, true);
                checkpoint_at(pv + 0xcc);
            }
            sb(m + 0x20, 1);
            if (lh(pv + 0x84) == 2) {
                item_give(0xc, 1);
                sub_00263d40(TXT(0xbc8), -1);
                if (lw(pv + 0xd0) != -1) {
                    prize_marks(moby_at(lw(pv + 0xd0)));
                    moby_retire(moby_at(lw(pv + 0xd0)));
                }
                if (const uint32_t f = ferry_of(pv + 0xd4)) ferry_release(f, pv + 0xd8);
                level_change_request(0, -1);
                gone();
                return;
            }
        }
    }
    float turn = 0.0f, tilt = 0.0f, k = u2f(0x3ca3d70au);
    if (citizen_look(m, pv, Look{0x1f0, 0x1f4, 0x1e0}, sp, turn, tilt, k)) {
        swc1(pv + 0x144, tilt);
        swc1(pv + 0x148, fpu_mul(turn, 0.5f));
    }
    const float d = u2f(0x3e99999au);
    if (lbu(0x0015edb0) != 0) swc1(pv + 0x150, 2.75f);
    joint_node_spring(m, pv + 0xe0, 0, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    joint_node_spring(m, pv + 0x160, 1, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    ee_stack_free(0xb0);
}

// Class 909, the Heli-Pack's giver (pvars: the talk block at +0 (+4 / +0x36 its state), +0x140 turned round, +0x144 the
// prize moby (class 0x3e5), +0x148 the ferry it calls, +0x14c a checkpoint box; the look's +0x150 .. +0x168, the head's
// springs +0x40 / +0xc0). The class's mates mark the level (0x2db480) every frame. State 0: turned round unless the flag
// 0x13d6fc is set; its collectible slot collected on this level, the ferry called; the talk started (in the hero's colour,
// with the flag 0x13d4c2 its state -1), 1; 1: with the flag 0x13d4c2 the prize moby (still in state 0) marked given, and
// with the slot collected nothing more; else it talks (then turned round once, the hero walks to a spot 2.7 in front, its
// +0x58 cleared, 2); 2: out of a scene its +0x58 = 1, talk again (1); at the talk's state other than 2 the slot collected,
// the ferry called, the checkpoint; at the talk's state 2 the Heli-Pack (item 2) with its message 0xbc7, the prize moby
// marked, the checkpoint, a level change asked (0, -1) and the hero placed at the box. Every frame its look.
// orig: L3:0x002db558
void kerwan_class909_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_level(m, 1);
    auto give_prize = [pv] {
        if (lw(pv + 0x144) == -1) return;
        const uint32_t o = moby_at(lw(pv + 0x144));
        if (lh(o + 0xa6) != 0x3e5 || lbu(o + 0x20) != 0) return;
        sb(o + 0x20, 1);
        prize_marks(o);
    };
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {  // (0x002db5c8)
        if (lw(0x0013d6fc) != 0) {
            sw(pv + 0x140, 1);
        } else {
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
            sw(pv + 0x140, 0);
        }
        if (lbu(level_row(lbu(m + 0xb0))) == 0xff)
            if (const uint32_t f = ferry_of(pv + 0x148)) ferry_call(m, f, false);
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));  // the hero's colour
        sb(m + 0x20, 1);
        talk_init(m, pv);
        band_set(m);
        if (lbu(0x0013d4c2) != 0) sh(pv + 0x36, 0xffffu);
    } else if (st == 1) {  // (0x002db69c)
        bool talks = true;
        if (lbu(0x0013d4c2) != 0) {
            give_prize();
            if (lbu(0x0013d4c2) != 0 && lbu(level_row(lbu(m + 0xb0))) == 0xff) talks = false;
        }
        if (talks && talk_update(m, pv) != 0) {
            if (lw(pv + 0x140) == 0) {
                swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
                sw(pv + 0x140, 1);
            }
            hero_walk_to_l1(u2f(0x402ccccdu), m);
            sw(m + 0x58, 0);
            sb(m + 0x20, 2);
        }
    } else if (st == 2 && lw(OV(0x0015f5c4)) != 2) {  // (0x002db820)
        swc1(m + 0x58, 1.0f);
        if (lh(pv + 4) != 2) {
            checkpoint_mark_collected(lbu(m + 0xb0));
            if (const uint32_t f = ferry_of(pv + 0x148)) ferry_call(m, f, false);
            checkpoint_at(pv + 0x14c);
        }
        sb(m + 0x20, 1);
        if (lh(pv + 4) == 2) {
            item_give(2, 1);
            sub_00263d40(TXT(0xbc7), -1);
            give_prize();
            checkpoint_at(pv + 0x14c);
            level_change_request(0, -1);
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x14c));
            hero_place(box + 0x30, box + 0x70, 0, 1);
        }
    }
    float turn = 0.0f, tilt = 0.0f, k = u2f(0x3ca3d70au);
    if (citizen_look(m, pv, Look{0x164, 0x168, 0x150}, sp, turn, tilt, k)) {
        swc1(pv + 0xa4, tilt);
        swc1(pv + 0x128, fpu_mul(turn, 0.5f));
        swc1(pv + 0xa8, fpu_mul(turn, 0.5f));
    }
    const float d = u2f(0x3e99999au);
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xb0, 2.75f);
    joint_node_spring(m, pv + 0x40, 1, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    joint_node_spring(m, pv + 0xc0, 0, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    ee_stack_free(0xb0);
}

}  // namespace rac1
