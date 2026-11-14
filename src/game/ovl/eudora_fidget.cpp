// Eudora's class 161: a prop that idles (animation 0) and now and then plays its animation 1 once (none is placed on the
// level, nothing spawns one). Header eudora_mobys.h; registered in eudora_mobys.cpp.
#include <game/lib/libc.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// pvars +0: the frames to its next fidget.
// orig: L4:0x002a5a08
void eud_class161_update_l4(uint32_t m) {
    if (m == 0) return;
    sw(m + 0x94, 0);
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv) == 0) {
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(3));
        sw(pv, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>((static_cast<uint32_t>(libc_rand()) + 0x1e) & 0xffu))));
    }
    if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(3));
    sw(pv, lwu(pv) - 1);
    moby_update_transform_kept(m);
}

}  // namespace rac1
