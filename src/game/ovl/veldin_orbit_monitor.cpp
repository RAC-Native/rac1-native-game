// Drek's fleet's (level 17) class 1470, ported by hand from the level's own code: the level's monitor (one on the level).
// Every frame it watches the level's two skill points (the bytes 0x13d408 + 0x1c and + 0x1b): the first when Ratchet,
// after a pose in the box pvars +0x60 (hero state 0x11), reaches the box +0x64 or +0x68 without flying the ship or falling
// out (states 0x32 / 0x76); the second when a scan of the moby table (30 mobys a frame, from pvars +0x70) finds no live
// class-44 / class-1382 enemy left outside the box +0x78. Its state machine gives the help system's voice hints (by the
// boxes +0 and +0x20 .. +0x28, after their waits) and once (pvars +0x4c, or the level's record +0x58 done) turns the camera
// to the box +0x44 and then +0x40 when Ratchet enters the box +0x3c. The original's order of calls kept. Level 17 only:
// `orig: L17:`, level data through lvd::*, the level's own data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
//
// The pvars: +0 the box of the first hint, +0x10 its mate's moby (a class-78 moby; its hint is not given while that
// moby is gone (0xfd / 0xfe) or in its state 3), +0xc / +0x38 the frames Ratchet has stayed in it and the wait (600 frames,
// 1800 after the hint), +0x20 .. +0x28 three boxes of the hint 0x4e2e (+0x2c the frames in them, 3600 at least, unless
// Ratchet is in state 0x35), +0x30 the box last hinted, +0x3c .. +0x48 the camera's boxes (any -1: the look is off),
// +0x4c the look done, +0x50 / +0x5c the look's timer, +0x58 the level record's index (0xff none), +0x60 .. +0x6c the first
// skill point's boxes and its stage, +0x70 / +0x74 / +0x78 the scan's moby, its count of enemies left, its box.
//
// States: 0 set up (-> 1), 1 watching, 2 the camera turns to the box +0x44 (-> 3), 3 after the look's wait, to the box
// +0x40 (-> 1, the look done); the others do nothing.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;       // the hero (+0x80 his place, +0x2084 his state, +0x208c his pose)
constexpr uint32_t SKILLS = 0x0013d408;  // the skill points' bytes

bool hero_in(int32_t box) { return point_in_box(P + 0x80, box) != 0; }

// the dialogue record (L17:0x179d10) idle: none playing (+0) and none asked for (+0x24)
bool dialogue_idle() { return lw(L17_ADDR(0x00179d10)) == 0 && lw(L17_ADDR(0x00179d10) + 0x24) == -1; }

// a skill point earned: its jingle and its message
void skill_point_earned() {
    sound_play_indexed(1, 0, 0);
    sub_00263d40(TXT(0x53d6), -1);
}

// the camera turned toward the centre of box `box`; true when it looks there (within 2 degrees)
bool camera_turned_to(int32_t box) {
    cam0_turn_point(lvd::box(lvd::BOXES, box) + 0x30, u2f(0x3cd67750u), 0.0f);
    const uint32_t b = lvd::box(lvd::BOXES, box);
    const float yaw = atan2_f(fpu_sub(lwc1(b + 0x30), lwc1(L17_ADDR(0x001676c0))), fpu_sub(lwc1(b + 0x34), lwc1(L17_ADDR(0x001676c4))));
    return angle_dist(lwc1(L17_ADDR(0x001676d8)), yaw) < u2f(0x3d0efa35u);
}
}  // namespace

// orig: L17:0x002f26d0
void vo_class1470_update_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    // the first skill point: the pose in box +0x60 (stage 1), then box +0x64 or +0x68 reached without the ship or a fall
    const int32_t stage = lw(pv + 0x6c);
    if (stage == 0) {
        if (hero_in(lw(pv + 0x60)) && lw(P + 0x208c) == 0x11) sw(pv + 0x6c, 1);
    } else if (stage == 1) {
        const int32_t st = lw(P + 0x2084);
        if (st == 0x76 || st == 0x32) {
            sw(pv + 0x6c, 0);
        } else if ((hero_in(lw(pv + 0x64)) || hero_in(lw(pv + 0x68))) && lbu(SKILLS + 0x1c) == 0) {
            sb(SKILLS + 0x1c, 1);
            skill_point_earned();
        }
    }

    // the second: no live enemy (class 44 / 1382) left outside the box +0x78, 30 mobys of the table a frame
    if (lbu(SKILLS + 0x1b) == 0) {
        for (int32_t k = 0; k < 30; ++k) {
            const uint32_t q = lwu(L17_ADDR(0x0015ffd8)) + (lwu(pv + 0x70) << 8);
            const int16_t cls = static_cast<int16_t>(lhu(q + 0xa6));
            if ((cls == 0x2c || cls == 0x566) && static_cast<int8_t>(lbu(q + 0x20)) >= 0 && point_in_box(q + 0x10, lw(pv + 0x78)) == 0)
                sw(pv + 0x74, lwu(pv + 0x74) + 1);
            const uint32_t next = lwu(pv + 0x70) + 1;
            sw(pv + 0x70, next);
            if (lwu(L17_ADDR(0x0015ffd8)) + (next << 8) < lwu(L17_ADDR(0x0015ffdc))) continue;
            // the table's end: none counted this round, the point earned
            if (lw(pv + 0x74) == 0 && lbu(SKILLS + 0x1b) == 0) {
                sb(SKILLS + 0x1b, 1);
                skill_point_earned();
            }
            sw(pv + 0x70, 0);
            sw(pv + 0x74, 0);
        }
    }

    switch (lbu(m + 0x20)) {
    case 0: {
        sb(m + 0x30, 0xff);
        sw(pv + 0x38, static_cast<uint32_t>(frame_scale_i(600)));
        sb(pv + 0x4c, 0);
        if (lw(pv + 0x44) == -1 || lw(pv + 0x40) == -1 || lw(pv + 0x3c) == -1 || lw(pv + 0x48) == -1) {
            debug_printf_stub();  // (the original prints the four boxes, L17:0x1e6f60)
            sb(pv + 0x4c, 1);
        }
        sb(m + 0x20, 1);
        break;
    }
    case 1: {
        // the first hint: Ratchet in box +0 (a loop of one box in the original) and a box not hinted last
        int32_t hint_box = -1, mate = -1;
        bool inside = false;
        if (hero_in(lw(pv + 0))) {
            inside = true;
            if (lw(pv + 0x30) != lw(pv + 0)) {
                hint_box = lw(pv + 0);
                mate = lw(pv + 0x10);
            }
        }
        if (inside) {
            sw(pv + 0xc, lwu(pv + 0xc) + 1);
        } else {
            sw(pv + 0xc, 0);
            sw(pv + 0x30, 0xffffffffu);
        }
        bool mate_ok = true;
        if (mate != -1) {
            const uint32_t q = lwu(L17_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate) << 8);
            if (q == 0 || static_cast<int16_t>(lhu(q + 0xa6)) != 0x4e) {
                mate_ok = false;
            } else {
                const uint8_t st = lbu(q + 0x20);
                mate_ok = st != 0xfe && st != 0xfd && st != 3;
            }
        }
        if (mate_ok && lw(pv + 0x38) < lw(pv + 0xc) && hint_box != -1) {
            const uint32_t pose = lwu(P + 0x208c);
            if ((pose < 2 || pose == 9) && dialogue_idle() && lbu(0x0013d4df) != 0) {
                sw(pv + 0x38, static_cast<uint32_t>(frame_scale_i(0x708)));
                if (lw(0x00141d54) >= 0) sub_00203908(0x3a9a, 0x7d);  // (a dialogue id)
                sw(pv + 0x30, static_cast<uint32_t>(hint_box));
                sw(pv + 0xc, 0);
            }
        }

        // the hint 0x3a9d in box +0x20 (once a level: the level's bit of 0x141d7c)
        if (hero_in(lw(pv + 0x20)) && dialogue_idle() && lbu(0x0013d4c4) == 0 &&
            (lwu(0x00141d7c) & (1u << (lwu(0x0015ed84) & 31))) == 0)
            sub_00203908(0x3a9d, 0x82);

        // the hint 0x4e2e after a long stay in one of the boxes +0x20 .. +0x28
        int32_t stay_box = -1;
        bool in_stay = false;
        for (uint32_t k = 0; k < 3 * 4; k += 4) {
            if (!hero_in(lw(pv + 0x20 + k))) continue;
            const int32_t b = lw(pv + 0x20 + k);
            in_stay = true;
            if (lw(pv + 0x30) != b) stay_box = b;
            break;
        }
        bool reset = !in_stay;
        if (in_stay) {
            sw(pv + 0x2c, lwu(pv + 0x2c) + 1);
            reset = lw(P + 0x2084) == 0x35;
        }
        if (reset) {
            sw(pv + 0x2c, 0);
            sw(pv + 0x30, 0xffffffffu);
        }
        if (frame_scale_i(0xe10) < lw(pv + 0x2c) && stay_box != -1 && sub_0020d568() != 0 && dialogue_idle() &&
            lbu(0x0013d4c4) != 0) {
            sub_00203908(0x4e2e, 0x78);
            sw(pv + 0x30, static_cast<uint32_t>(stay_box));
            sw(pv + 0x2c, 0);
        }

        // the camera's look, once: Ratchet in box +0x3c
        const uint32_t record = lwu(pv + 0x58);
        if (record != 0xff && lbu(0x0014c050u + record + (lwu(0x0015ed84) << 4)) == 0xff) sb(pv + 0x4c, 1);
        if (lbu(pv + 0x4c) != 0 || !hero_in(lw(pv + 0x3c))) break;
        sub_002039a0();
        if (lw(0x00141dc4) >= 0) sub_00203908(0x4268, 0x8b);
        if (dialogue_idle()) break;  // (no dialogue started: the look waits)
        sw(pv + 0x50, 0);
        sb(m + 0x20, 2);
        sw(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0xb4)));
        hero_set_state(0x72, 0);
        break;
    }
    case 2:
        cam0_set_flags3();
        if (camera_turned_to(lw(pv + 0x44))) {
            sb(m + 0x20, 3);
            sw(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
        break;
    case 3:
        cam0_set_flags3();
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x5c))) == 0) break;
        if (camera_turned_to(lw(pv + 0x40))) {
            hero_enter_idle();
            sb(pv + 0x4c, 1);
            sb(m + 0x20, 1);
        }
        break;
    default:
        break;
    }
}

}  // namespace rac1
