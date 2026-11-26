// Novalis's own moby classes: the Infobot that comes down to the hero (classes 730 / 731; hand-ported from Novalis's own
// overlay). Header novalis_mobys2.h, registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`); Novalis-only
// data is L1_ADDR, the base ELF's are bare (the hero 0x13f350 / its moby 0x1413d0, the frame time 0x15ed6c, the level
// number 0x15ed84, the per-level flag bytes 0x14c050, the story bytes 0x13d397, the scene flag 0x15edb0).
//
// The pvars: its rise above its place +0 (where it starts), the camera moby's index +4 (-1 none), its timer +8, its resting
// height +0xc, the hero's box +0x10, three mobys that must go before it plays +0x14 .. +0x1c, its checkpoint's box +0x30,
// two mobys noted when it is done +0x34 / +0x38 and a third one woken +0x3c (-1 none).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_level_helpers.h>
#include <game/ovl/veldin_mobys.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
int32_t scene_mode() { return lw(L1_ADDR(0x0015f5c4)); }  // (2: a scene playing)
uint32_t moby_at(int32_t idx) { return lwu(L1_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }  // the moby table's entry

// the camera moby's angles (its box's centre +0x30) toward the Infobot: the pitch from the distance and the height above
// the centre, the heading
float pitch_to(uint32_t m, uint32_t box) {
    const float d = vec2_distance(V(box + 0x30), V(m + 0x10));
    return -atan2_f(d, fpu_sub(lwc1(m + 0x18), lwc1(box + 0x38)));
}
float heading_to(uint32_t m, uint32_t box) {
    return atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(box + 0x30)), fpu_sub(lwc1(m + 0x14), lwc1(box + 0x34)));
}
}  // namespace

// During a scene (mode 2, the flag 0x15edb0 set): each moby of class 811 in the scene's list (0x16cce0: +0x178, the count
// +0x44) puts its joint 0's point into the record 0x17c8c0 when that is free (+1 0), sized 2.75. (class_mates_mark's build
// for one fixed class.)
// orig: L1:0x002fac80
void nov_2fac80_l1() {
    if (scene_mode() != 2 || lbu(0x0015edb0) == 0) return;
    const uint32_t list = L1_ADDR(0x0016cce0);
    const uint32_t rec = L1_ADDR(0x0017c8c0);
    for (int32_t i = 0; i < lh(list + 0x44); ++i) {
        const uint32_t o = lwu(list + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != 0x32b || lbu(rec + 1) != 0) continue;
        sub_0024f590(o, 0, rec);
        swc1(rec + 0x28, 2.75f);
        swc1(rec + 0x20, 2.75f);
        swc1(rec + 0x24, 2.75f);
    }
}

// The Infobot. 0 sets it up above its place; 1 waits (hidden, its first scene played once) for the hero in its box, then
// points the camera moby at it and comes down (2) to its resting height, the landing animation (3); class 730 stays there
// (10), class 731 waits (4) until its three mobys are gone and the hero is free, then (5 .. 8) its scenes and movie, Kerwan
// opened, its flags and checkpoint, the level change. Already done (its flag byte 0xff), it rests at once (10).
// orig: L1:0x002fad68
void nov_class730_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    nov_2fac80_l1();
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 8)));
    auto open_anim = [m] {
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(20));
    };
    switch (lbu(m + 0x20)) {  // (the table 0x20b520)
        case 0: {  // (0x2fadbc)
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 1);
            sh(pv + 8, 0);
            swc1(pv + 0xc, fpu_sub(lwc1(m + 0x18), 0.75f));
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv)));
            break;
        }
        case 1: {  // (0x2fae04)
            const uint32_t done = lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4));
            if (done == 0xff) {
                open_anim();
                sb(m + 0x20, 10);
                sh(m + 0x32, done);
                sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
                sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
                break;
            }
            if (scene_mode() == 0 && lbu(0x0013d397) == 0) {
                cutscene_start(5);
                sb(0x0013d397, 1);
            }
            ship_hide();
            if (point_in_box(0x0013f3d0, lw(pv + 0x10)) == 0) break;
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            if (lw(pv + 4) != -1) {
                const uint32_t cam = moby_at(lw(pv + 4));
                const uint32_t cpv = lwu(cam + 0x78);
                sw(cpv + 0x2c, static_cast<uint32_t>(frame_scale_i(420)));
                sb(cam + 0xbc, 1);
                lvd::set_shape_row(lvd::box(lvd::BOXES, lw(cpv + 0x24)) + 0x70, Vec4{0.0f, 0.0f, 0.0f, 0.0f});
                vec_clear(V(cpv + 0x10));
                const float pitch = pitch_to(m, lvd::box(lvd::BOXES, lw(cpv + 0x24)));
                lvd::set_shape_f32(lvd::box(lvd::BOXES, lw(cpv + 0x24)) + 0x74, pitch);
                const float heading = heading_to(m, lvd::box(lvd::BOXES, lw(cpv + 0x24)));
                lvd::set_shape_f32(lvd::box(lvd::BOXES, lw(cpv + 0x24)) + 0x78, heading);
            }
            sb(m + 0x20, 2);
            break;
        }
        case 2: {  // (0x2faffc) coming down, the camera moby following it
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(dt(), 5.0f)));
            if (lw(pv + 4) != -1) {
                const uint32_t cpv = lwu(moby_at(lw(pv + 4)) + 0x78);
                swc1(cpv + 0x14, pitch_to(m, lvd::box(lvd::BOXES, lw(cpv + 0x24))));
                swc1(cpv + 0x18, heading_to(m, lvd::box(lvd::BOXES, lw(cpv + 0x24))));
            }
            const float rest = lwc1(pv + 0xc);
            if (lbu(m + 0x53) == 0 && fpu_sub(lwc1(m + 0x18), rest) <= 0.0f) moby_set_anim(m, 1, 0);
            if (lwc1(m + 0x18) <= lwc1(pv + 0xc)) {
                swc1(m + 0x18, lwc1(pv + 0xc));
                sb(m + 0x20, 3);
            }
            break;
        }
        case 3:  // (0x2fb114) landed: when the landing animation ends
            if (lbu(m + 0x52) != 1 || (lbu(m + 0x70) & 2) == 0) break;
            open_anim();
            sb(m + 0x20, lh(m + 0xa6) == 0x2da ? 10 : 4);
            break;
        case 4: {  // (0x2fb174) its three mobys gone (none, or dying / dead), the hero not busy (states 7, 20)
            bool gone = true;
            for (uint32_t k = 0; k < 3; ++k) {
                const int32_t idx = lw(pv + 0x14 + k * 4);
                if (idx == -1) continue;
                const uint32_t o = moby_at(idx);
                if (o == 0) continue;
                const uint32_t st = lbu(o + 0x20);
                if (st != 0xfe && st != 0xfd) {
                    gone = false;
                    break;
                }
            }
            if (!gone) break;
            const uint32_t hs = lwu(0x001413dc);
            if (hs == 7 || hs == 0x14) break;
            sh(pv + 8, static_cast<uint32_t>(frame_scale_i(45)));
            sb(m + 0x20, 5);
            break;
        }
        case 5:  // (0x2fb20c)
            if (lh(pv + 8) != 0) break;
            sh(pv + 0xa, lhu(m + 0x32));
            sh(m + 0x34, lhu(m + 0x34) | 0x41);
            cutscene_start(3);
            sb(m + 0x20, 6);
            break;
        case 6:  // (0x2fb240)
            if (scene_mode() == 2) break;
            movie_play(3);
            sb(m + 0x20, 7);
            break;
        case 7:  // (0x2fb268)
            if (scene_mode() == 2) break;
            cutscene_start(4);
            sb(m + 0x20, 8);
            break;
        case 8: {  // (0x2fb290) the message given: Kerwan opened, the flags, the checkpoint, the level change
            if (scene_mode() == 2) break;
            level_mark_visited(3);
            level_arrival_message(3);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            if (lw(pv + 4) != -1) {
                const uint32_t cam = moby_at(lw(pv + 4));
                sb(L1_ADDR(0x001baa50) + static_cast<uint32_t>(lh(cam + 0xb2)) + 0x454, lbu(cam + 0xb0) + 2);
                const uint32_t idx = lbu(cam + 0xb0);
                const bool mark = idx == 0xff ||
                                  (lbu(L1_ADDR(0x0015fc88) + idx) != 0xff && lbu(0x0014c050 + idx + (lwu(0x0015ed84) << 4)) == 0xff);
                if (mark) sb(L1_ADDR(0x001bb6b0) + static_cast<uint32_t>(lh(cam + 0xb2)) + 0x454, idx + 2);
            }
            if (lw(pv + 0x3c) != -1) {
                sw(lwu(moby_at(lw(pv + 0x3c)) + 0x78) + 0x2c, static_cast<uint32_t>(frame_scale_i(180)));
                sb(moby_at(lw(pv + 0x3c)) + 0xbc, 1);
                if (lw(pv + 0x34) != -1 && lw(pv + 0x38) != -1) {
                    sb(moby_at(lw(pv + 0x34)) + 0xbc, 2);
                    sb(moby_at(lw(pv + 0x38)) + 0xbc, 2);
                    const uint32_t a = moby_at(lw(pv + 0x34));
                    moby_data_note_l1(a + 0xbc, 1, a, 1, L1_ADDR(0x001baaa0));
                    const uint32_t b = moby_at(lw(pv + 0x38));
                    moby_data_note_l1(b + 0xbc, 1, b, 1, L1_ADDR(0x001baaa0));
                }
            }
            checkpoint_mark_collected(lbu(m + 0xb0));
            if (lw(pv + 0x30) != -1) {
                const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x30));
                checkpoint_save(box + 0x30, box + 0x70);
            }
            level_change_request(0, -1);
            held_moby_show();
            sb(m + 0x20, 10);
            break;
        }
        case 10:  // (0x2fb490) resting
            swc1(m + 0x18, lwc1(pv + 0xc));
            break;
        default:  // (9: 0x2fb498, and past the table)
            break;
    }
}

}  // namespace rac1
