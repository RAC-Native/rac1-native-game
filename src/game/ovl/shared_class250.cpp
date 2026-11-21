// Class 250's update: one port of the copies Oltanis and Quartu carry (the same code relinked; each level's jump table of
// the states is its own, switched on here by the state). It reads the engine's active camera (OV 0x166e00: its type +0x86)
// and the base ELF's hero moby byte 0x1413f4. Header shared_class250.h.
//
// The pvars: +0 armed, +4 the frames it waits, +8 the timer.
#include <game/ovl/shared_class250.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr int32_t CAMERA_TYPE_20 = 0x14;

bool camera_type_20() { return lh(lwu(OV(0x00166e00)) + 0x86) == CAMERA_TYPE_20; }

// the animation that ended: the moby's +0x70 bit 1
bool anim_done(uint32_t m) { return (lbu(m + 0x70) & 2) != 0; }
}  // namespace

// Its states: 0 waits (armed) for the camera of type 20, 1 the delay, 2 the first animation (set 1) to its end, 3 the
// second (set 2) held while the camera stays, 4 rest; 5 .. 7 a second cycle (sets 2, 3, then 0 and back to state 0).
// The model is not drawn while the hero moby's byte +0x24 is 2.
// orig: L14:0x002d96e0 L15:0x002c6f08
void class250_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(0x001413f4) == 2) sw(m + 0x94, 0);
    else sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    switch (lbu(m + 0x20)) {  // (the level's jump table of eight states)
    case 0:  // (L14:0x002d9744)
        if (lw(pv) == 0 || !camera_type_20()) break;
        sb(m + 0x20, 1);
        sw(pv + 8, static_cast<uint32_t>(frame_scale_i(lw(pv + 4))));
        break;
    case 1:  // (L14:0x002d977c)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) == 0) break;
        sb(m + 0x20, 2);
        moby_set_anim(m, 1, 0);
        break;
    case 2:  // (L14:0x002d979c)
        if (!anim_done(m)) break;
        sb(m + 0x20, 3);
        moby_set_anim(m, 2, 0);
        break;
    case 3:  // (L14:0x002d97b4)
        if (camera_type_20()) break;
        moby_set_anim(m, 0, 0);
        sb(m + 0x20, 4);
        break;
    case 5:  // (L14:0x002d97e4)
        if (!anim_done(m)) break;
        sb(m + 0x20, 6);
        moby_set_anim(m, 2, 0);
        break;
    case 6:  // (L14:0x002d9810)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) == 0) break;
        moby_set_anim(m, 3, 0);
        sb(m + 0x20, 7);
        break;
    case 7:  // (L14:0x002d9838)
        if (!anim_done(m)) break;
        sb(m + 0x20, 0);
        moby_set_anim(m, 0, 0);
        break;
    default:  // (4 and past the table: nothing)
        break;
    }
}

}  // namespace rac1
