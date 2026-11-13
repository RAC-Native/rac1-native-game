// Class 604: one port of the copies ten levels carry (Novalis, Aridia, Kerwan, Eudora, Rilgar, Blarg, Batalia, Pokitaru,
// Hoven, Oltanis), the same code relinked (the normalised texts compared, the state table's entries the same offsets in
// every copy). Its addresses are the base ELF's (bare: the frame time's square 0x15ed70, the flags 0x13d4c5 and 0x1404a8);
// its only level-own word is the state table, replaced by a switch on the index. Header shared_class604.h.
//
// The pvars: the yaw it turns to +0, the turn rate +4.
#include <game/ovl/shared_class604.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// While the flag 0x13d4c5 is set and 0x1404a8 is 5 it turns to its yaw (from state 1 on). The states (the jump table
// L2:0x1fa990, 9 entries, by its index; the animation set n, from frame 0, unless it plays already, then the next state):
// 0 its yaw kept, to 1; 1 asked 3..5: set 1 (10 frames), 2; 2 at the animation's end: set 2, 3; 3 asked 4 / 5: set 3, 4,
// asked 7 / 1: set 6, 7; 4 at the end: set 4, 5; 5 asked 3, 6, 7 or 1: set 5, 6; 6 at the end: asked 7 / 1 set 6, 7, else
// set 2, 3; 7 at the end: set 0, 1. (Each in 3 frames but where said.)
// orig: L1:0x002f2b68 L2:0x002d7190 L3:0x002cb8e8 L4:0x002d4368 L5:0x00301988 L6:0x002e11c8 L8:0x002f18b8 L11:0x002fc630 L12:0x002f51f0 L14:0x002ea9d0
void class604_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv != 0 && lbu(m + 0x20) != 0 && lbu(0x0013d4c5) != 0 && lw(0x001404a8) == 5) {
        const float k = lwc1(0x0015ed70);
        swc1(m + 0x48, angle_seek_rate(pv + 4, lwc1(m + 0x48), lwc1(pv), fpu_mul(k, u2f(0x40c90fdbu)), fpu_mul(k, u2f(0x40b84e88u)), 0.0f));
    }
    // the animation `set` (over `frames` frames) unless it plays, then the state `next`
    auto go = [m](uint32_t set, int32_t frames, uint32_t next) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
        sb(m + 0x20, next);
    };
    const bool ended = (lbu(m + 0x70) & 2u) != 0;
    const uint32_t asked = lbu(m + 0xbc);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002d7244)
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0));
        sb(m + 0x20, 1);
        swc1(pv, lwc1(m + 0x48));
        break;
    case 1:  // (0x002d7280)
        if (asked - 3u < 3u) go(1, 10, 2);
        break;
    case 2:  // (0x002d72c4)
        if (ended) go(2, 3, 3);
        break;
    case 3:  // (0x002d7304)
        if (asked - 4u < 2u) go(3, 3, 4);
        else if (asked == 7 || asked == 1) go(6, 3, 7);
        break;
    case 4:  // (0x002d7368)
        if (ended) go(4, 3, 5);
        break;
    case 5:  // (0x002d73a8)
        if (asked == 3 || asked == 6 || asked == 7 || asked == 1) go(5, 3, 6);
        break;
    case 6:  // (0x002d7404)
        if (!ended) break;
        if (asked == 7 || asked == 1) go(6, 3, 7);
        else go(2, 3, 3);
        break;
    case 7:  // (0x002d748c)
        if (ended) go(0, 3, 1);
        break;
    default:  // 8 (0x002d74cc) and beyond the table
        break;
    }
}

}  // namespace rac1
