// Hoven's (level 12) class 293: the owner of the level's sky-reflecting objects (their draw is the shared env_objects_callback,
// shared_env_objects.cpp). Level 12 only: the addresses are Hoven's (`orig: L12:`). Registered in hoven_mobys.cpp.
//
// The moby: +0x20 the state (0 set up, 1 running); the pvars: +0xc the box the camera must be in (-1: anywhere).
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l12.h>

namespace rac1 {

// Class 293, the owner of the level's sky-reflecting objects: its first frame set up, then their draw (the shared
// env_objects_callback, L12:0x2e71b0) queued every frame the camera is inside its box (+0xc of the pvars; -1: always).
// orig: L12:0x002e7208
void hov_class293_update_l12(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        break;
    case 1: {
        const int32_t box = lw(lwu(m + 0x78) + 0xc);
        if (box == -1 || point_in_box(L12_ADDR(0x00167240), box) != 0) draw_hook_add_dc80_l2(L12_ADDR(0x002e71b0), m);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
