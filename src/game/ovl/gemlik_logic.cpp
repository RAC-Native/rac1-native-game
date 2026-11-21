// Gemlik Base's (level 13) classes 533, 558, 667, 674, 1271, 1353, 1558 and 1577: mobys of the level's logic. Level 13 only: the
// addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Class 533: a ship model left over; hidden without collision unless it is the ship the arrival scene uses (0x13e030).
// Class 558: the space fight's area. pvars +0 a box where Ratchet hears the help message 0x32c8 (while the hero's
// +0x2994 has no bit 31), +4 a class 0xaa moby (an index, -1 none), +8 the box whose outside sets the hero's byte
// +0x22cb (1 outside, 0 inside). While no scene runs (L13:0x179710 0, L13:0x179734 -1), the hero's +0x208c is below 2
// or 9 and L13:0x15f5c4 is 0: with that moby there, or neither flag 0x13d4cb / 0x13d4cd set, the help; else it counts
// in the hero's +0x2990 (once a frame, up to 0xffff), +0x2992 (the level's time over 600 frames, the highest) and
// +0x2994 (the level's bit and bit 31).
// Class 667: a relay. pvars +0 the moby it watches (an index), +4 .. +9 and +0xa .. +0xf two lists of six states, +0x10
// the moby it sets (an index, -1 none), +0x14 / +0x15 the state and +0x16 / +0x17 the step it gives that moby, +0x18 /
// +0x19 and +0x1a / +0x1b the ones it takes itself (-1: none), for each list; +0x1c the list it waits on (0 set up, 1
// the second list, 2 the first), +0x1d the one it starts on (0: the first). When the watched moby's state is in the
// list waited on, the states are given and it waits on the other list.
#include <game/level/level_data.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

using namespace gml;

namespace {
// the level's moby of index `idx` (not -1)
uint32_t moby_at(int32_t idx) { return lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
}  // namespace

// orig: L13:0x002f3750
void gml_class533_update_l13(uint32_t m) {
    if (lwu(0x0013e030) == m) return;
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) | 3u);
}

// orig: L13:0x002f3778
void gml_class558_update_l13(uint32_t m) {
    sb(m + 0x30, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hero_pos = 0x0013f3d0;
    if (lwu(L13_ADDR(0x00179710)) == 0 && lw(L13_ADDR(0x00179734)) == -1) {
        const uint32_t walk = lwu(0x001413dc);
        if ((walk < 2 || walk == 9) && lwu(L13_ADDR(0x0015f5c4)) == 0) {
            const int32_t idx = lw(pv + 4);
            bool present = false;  // (the class 0xaa moby +4 names, there)
            if (idx != -1) {
                const uint32_t o = moby_at(idx);
                present = o != 0 && lh(o + 0xa6) == 0xaa && alive(o);
            }
            if (present || (lbu(0x0013d4cb) == 0 && lbu(0x0013d4cd) == 0)) {
                if (point_in_box(hero_pos, lw(pv)) != 0 && lw(0x00141ce4) >= 0) sub_00203908(TXT(0x32c8), 0x6f);
            } else {
                const uint32_t stats = 0x00141968;
                if (!(0xfffeu < lhu(stats + 0x378))) sh(stats + 0x378, lhu(stats + 0x378) + 1);
                if (static_cast<int32_t>(lhu(stats + 0x37a)) < ee_div(frame_scale_i(lw(0x0015eea4)), 600))
                    sh(stats + 0x37a, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
                sw(stats + 0x37c, lwu(stats + 0x37c) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
            }
        }
    }
    sb(0x0014161b, point_in_box(hero_pos, lw(pv + 8)) == 0 ? 1u : 0u);
}

// orig: L13:0x002f8880
void gml_class667_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t idx = lw(pv);
    const uint32_t watched = idx == -1 ? 0u : moby_at(idx);
    // the watched moby's state in the list at `list`: the states of +`give` / +`give` + 2 given to the moby +0x10 names,
    // those of +`take` / +`take` + 2 taken, then it waits on `next`
    auto relay = [m, pv, watched](uint32_t list, uint32_t give, uint32_t take, uint32_t next) {
        const int32_t st = lbu(watched + 0x20);
        bool found = false;
        for (uint32_t k = 0; k < 6 && !found; ++k) found = st == static_cast<int8_t>(lbu(pv + list + k));
        if (!found) return;
        if (const int32_t target = lw(pv + 0x10); target != -1) {
            const uint32_t t = moby_at(target);
            if (static_cast<int8_t>(lbu(pv + give)) != -1) sb(t + 0x20, lbu(pv + give));
            if (static_cast<int8_t>(lbu(pv + give + 2)) != -1) sb(t + 0xbc, lbu(pv + give + 2));
        }
        if (static_cast<int8_t>(lbu(pv + take)) != -1) sb(m + 0x20, lbu(pv + take));
        if (static_cast<int8_t>(lbu(pv + take + 2)) != -1) sb(m + 0xbc, lbu(pv + take + 2));
        sb(pv + 0x1c, next);
    };
    switch (static_cast<int8_t>(lbu(pv + 0x1c))) {
    case 0:  // set up: retired without a moby to watch
        if (watched == 0) {
            debug_printf_stub();  // (the format L13:0x1f51b0 with its uid and class: the release build prints nothing)
            moby_retire(m);
            return;
        }
        sb(pv + 0x1c, static_cast<int8_t>(lbu(pv + 0x1d)) == 0 ? 2u : 1u);
        break;
    case 1:
        relay(0xa, 0x15, 0x19, 2);
        break;
    case 2:
        relay(4, 0x14, 0x18, 1);
        break;
    default:
        break;
    }
}

// orig: L13:0x002f8a78
void gml_class674_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t idx = lw(pv);
    const uint32_t watched = idx == -1 ? 0u : moby_at(idx);
    const uint32_t state = lbu(m + 0x20);
    bool turn_on = false, turn_off = false;  // (the watched moby's state in the first / the second list)
    if (state != 0) {
        const int32_t st = lbu(watched + 0x20);
        for (uint32_t k = 0; k < 6 && !turn_on; ++k) turn_on = st == static_cast<int8_t>(lbu(pv + 4 + k));
        for (uint32_t k = 0; k < 6 && !turn_off; ++k) turn_off = st == static_cast<int8_t>(lbu(pv + 0xa + k));
    }
    auto collision_on = [m] { sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10)); };
    switch (state) {  // (the table L13:0x1f5210)
    case 0:  // set up
        if (watched == 0) {
            debug_printf_stub();  // (the format L13:0x1f51e0 with its uid and class: the release build prints nothing)
            moby_retire(m);
            return;
        }
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        sh(m + 0x34, lhu(m + 0x34) | 0xa08u);
        if (lwu(pv + 0x18) != 0) {
            swc1(pv + 0x10, 0.0f);
            sb(m + 0x20, 3);
            sw(m + 0x94, 0);
            sb(m + 0x23, 0);
        } else {
            swc1(pv + 0x10, 1.0f);
            sb(m + 0x20, 1);
            sb(m + 0x23, 0x14);
            collision_on();
        }
        break;
    case 1:  // on
        if (turn_off) {
            sw(m + 0x94, 0);
            sb(m + 0x20, 2);
        }
        break;
    case 2: {  // fading out
        if (turn_on) {
            sb(m + 0x20, 4);
            collision_on();
        }
        const float k = fpu_sub(lwc1(pv + 0x10), fpu_mul(step60(), 0.05f));
        swc1(pv + 0x10, k);
        if (k < 0.0f) {
            swc1(pv + 0x10, 0.0f);
            sb(m + 0x20, 3);
            sw(m + 0x94, 0);
            sb(m + 0x31, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
        }
        break;
    }
    case 3:  // off
        if (turn_on) {
            sb(m + 0x20, 4);
            sb(m + 0x31, 1);
            collision_on();
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        }
        break;
    case 4: {  // fading in
        if (turn_off) {
            sw(m + 0x94, 0);
            sb(m + 0x20, 2);
        }
        const float k = fpu_add(lwc1(pv + 0x10), fpu_mul(step60(), 0.05f));
        swc1(pv + 0x10, k);
        if (1.0f < k) {
            swc1(pv + 0x10, 1.0f);
            sb(m + 0x20, 1);
            collision_on();
        }
        break;
    }
    default:
        break;
    }
    sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x10), 20.0f))));
    // the texture's scroll, 0xc0 a frame, kept within 0 .. 0x1000
    const int32_t scroll = lw(pv + 0x14);
    int32_t step = 0xc0;
    if (scroll + 0xc0 >= 4097) step = -0xf40;
    else if (scroll + 0xc0 < 0) step = 0x10c0;
    sw(pv + 0x14, static_cast<uint32_t>(scroll + step));
    class_uv_scroll(lwu(m + 0x24), step, 0);
}

// The mobys of the level's group `group` still standing: alive (a state byte below 0x80), with a hit record (class hook
// 10) of fewer than 4 at its +0x68 or none, and not in the state `state` (-1: any).
// orig: L13:0x0030ae68
int32_t gml_30ae68_l13(int32_t group, int32_t state) {
    const uint32_t list = lvd::group(group);
    if (list == 0) return 0;
    int32_t n = 0;
    for (uint32_t e = list;; e += 2) {
        const uint32_t h = lhu(e);
        const uint32_t o = moby_at(static_cast<int32_t>(h & 0x7fffu));
        if (static_cast<int8_t>(lbu(o + 0x20)) >= 0) {
            const uint32_t rec = class_hook_10(o);
            if ((rec == 0 || lh(rec + 0x68) < 4) && (state == -1 || static_cast<int32_t>(lbu(o + 0x20)) != state)) ++n;
        }
        if ((h & 0x8000u) != 0) break;
    }
    return n;
}

// orig: L13:0x0030af50
void gml_class1271_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t e = pv;
    for (int k = 0; k < 8; ++k) {  // (a -1 ends the list: the original does not step past it)
        const int32_t g = lw(e);
        if (g == -1) continue;
        if (gml_30ae68_l13(g, -1) != 0) return;
        e += 4;
    }
    if (lwu(HERO + 0x15f0) != 0 && lw(0x001413d4) == 0x32) return;
    if (const int32_t box = lw(pv + 0x28); box != -1 && point_in_box(HERO + 0x80, box) == 0) return;
    if (lw(pv + 0x20) != -1) sb(m + 0x20, lbu(pv + 0x20));
    if (lw(pv + 0x24) != -1) sb(m + 0xbc, lbu(pv + 0x24));
}

// orig: L13:0x0030b628
void gml_class1353_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const bool scene = lw(L13_ADDR(0x0015f5c4)) == 2;  // (a scene runs)
    auto hide = [pv](uint32_t at, uint32_t bits) {
        if (const int32_t i = lw(pv + at); i != -1) sh(moby_at(i) + 0x34, lhu(moby_at(i) + 0x34) | bits);
    };
    auto show = [pv](uint32_t at, uint32_t keep) {
        if (const int32_t i = lw(pv + at); i != -1) sh(moby_at(i) + 0x34, lhu(moby_at(i) + 0x34) & keep);
    };
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f5750: state n at case n)
    case 0:
        if (lw(L13_ADDR(0x0015f5c4)) == 0) sb(m + 0x20, 1);
        break;
    case 1:  // the first scene, once
        if (lbu(0x0013d3ec) != 0) {
            sb(m + 0x20, 3);
            break;
        }
        sb(0x0013d3ec, 1);
        sb(m + 0x20, 2);
        cutscene_start(0);
        break;
    case 2:
        if (!scene) sb(m + 0x20, 3);
        break;
    case 3:
        sb(m + 0x20, 4);
        break;
    case 4: {  // the second scene, once, when the hero is in the box
        if (lbu(0x0013d3ed) != 0) {
            sb(m + 0x20, 6);
        } else if (lw(pv) == -1) {
            sb(m + 0x20, 7);
        } else if (point_in_box(0x0013f3d0, lw(pv)) != 0) {
            sb(0x0013d3ed, 1);
            hide(0x14, 0x41);
            sb(m + 0x20, 5);
            cutscene_start(1);
        }
        if (lbu(m + 0x20) == 4) break;
        if (const int32_t i = lw(pv + 0xc); i != -1) moby_retire(moby_at(i));
        break;
    }
    case 5: {  // its end: the ship at the box
        if (scene) break;
        show(0x14, 0xffbe);
        const int32_t b = lw(pv + 8);
        if (b != -1 && lw(pv + 0x10) != -1) {
            const uint32_t ship = moby_at(lw(pv + 0x10));
            const uint32_t box = lvd::box(lvd::BOXES, b);
            lq_sq(ship + 0x10, box + 0x30);
            hero_place(box + 0x30, box + 0x70, 0, 1);
            sb(0x001413f5, 1);
            swc1(lwu(ship + 0x78) + 0x7c, u2f(0x3f7d70a4u));
            swc1(L13_ADDR(0x0015f3fc), u2f(0x3f7d70a4u));
            sb(ship + 0x20, 2);
        }
        sb(m + 0x20, 6);
        break;
    }
    case 6:
        sb(m + 0x20, 7);
        break;
    case 7: {  // the third scene, once, when the class 0x184 moby is gone
        if (lbu(0x0013d3ee) != 0) {
            sb(m + 0x20, 0xc);
            break;
        }
        const int32_t i = lw(pv + 0x14);
        if (i == -1) {
            sb(m + 0x20, 0xd);
            break;
        }
        if (const uint32_t o = moby_at(i); o != 0 && lh(o + 0xa6) == 0x184 && alive(o)) break;
        hide(0x10, 2);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 8));
        hero_place(box + 0x30, box + 0x70, 0x72, 1);
        sb(0x0013d3ee, 1);
        hide(0x20, 0x41);
        music_track_request(0, 5);
        sb(m + 0x20, 8);
        cutscene_start(2);
        break;
    }
    case 8:
        if (scene) break;
        sb(m + 0x20, 9);
        movie_play(0xe);
        break;
    case 9:
        if (scene) break;
        cutscene_start(3);
        sb(m + 0x20, 0xa);
        break;
    case 10: {  // the ship model set up and placed, the level marked visited
        if (scene) break;
        show(0x10, 0xfffd);
        level_mark_visited(0xe);
        if (const int32_t i = lw(pv + 0x20); i != -1) {
            const uint32_t o = moby_at(i);
            sh(o + 0x34, lhu(o + 0x34) & 0xffbeu);
            ship_init(o);
            swc1(0x0013e090, u2f(0x43e8547bu));
            swc1(0x0013e0a8, u2f(0x403147aeu));
            swc1(0x0013e094, u2f(0x44112b85u));
            swc1(0x0013e098, u2f(0x439e5c29u));
        }
        sw(L13_ADDR(0x0015f404), 0);
        sb(m + 0x20, 0xb);
        break;
    }
    case 11: {  // the level change once the ship is gone
        const int32_t i = lw(pv + 0x10);
        if (i == -1) {
            sb(m + 0x20, 0xc);
            break;
        }
        if (const uint32_t o = moby_at(i); o != 0 && lh(o + 0xa6) == 0x45 && alive(o)) break;
        level_change_request(0, -1);
        sb(m + 0x20, 0xc);
        break;
    }
    case 12:
        sb(m + 0x20, 0xd);
        break;
    case 13: {  // the end, once the ship's slot is free
        const int32_t i = lw(pv + 0x10);
        if (i == -1) {
            moby_retire(m);
            break;
        }
        if (lbu(L13_ADDR(0x0015fc88) + lbu(moby_at(i) + 0xb0)) != 0xff) break;
        if (const int32_t j = lw(pv + 0x20); j != -1) moby_retire(moby_at(j));
        sb(m + 0x20, 0xe);
        break;
    }
    case 14:
        sh(m + 0x34, lhu(m + 0x34) | 2u);
        break;
    default:
        break;
    }
}

// orig: L13:0x0030bdf0
void gml_class1558_update_l13(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    case 1:
        if (lw(L13_ADDR(0x0015f5c4)) == 2 && lw(L13_ADDR(0x0016cb90)) == 2) scene_moby_smoke(lwu(L13_ADDR(0x0016cb60) + 0x180));
        break;
    default:
        break;
    }
}

// orig: L13:0x0030be60
void gml_class1577_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t base = lwu(L13_ADDR(0x0015ffd8));
    switch (lbu(m + 0x20)) {
    case 0:  // set up: the eight mobys' classes
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 0x20; k += 4) {
            const uint32_t o = base + (lwu(pv + 0x10 + k) << 8);
            sw(pv + 0x30 + k, o == 0 || !alive(o) ? 0xffffffffu : static_cast<uint32_t>(static_cast<int32_t>(lh(o + 0xa6))));
        }
        if (lwu(pv) == 0) sb(m + 0x20, 2);
        break;
    case 1: {  // the living counted
        int32_t n = 0;
        for (uint32_t k = 0; k < 0x20; k += 4) {
            const int32_t idx = lw(pv + 0x10 + k), cls = lw(pv + 0x30 + k);
            if (idx == -1 || cls == -1) {
                sw(pv + 0x30 + k, 0xffffffffu);
                continue;
            }
            const uint32_t o = base + (static_cast<uint32_t>(idx) << 8);
            if (o == 0 || lh(o + 0xa6) != cls || !alive(o)) sw(pv + 0x30 + k, 0xffffffffu);
            else ++n;
        }
        if (n < lw(pv)) moby_retire(m);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
