// Gemlik Base's (level 13) classes 533, 558 and 667: three invisible mobys of the level's logic. Level 13 only: the
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
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>

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

}  // namespace rac1
