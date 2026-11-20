// Gemlik Base's (level 13) class 6: a trigger box. When the hero walks into its box (the level's box +0 of its pvars)
// it sets the states of up to four mobys and four moby groups, its own state and its +0xbc; when he walks out, the
// second set of states. Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the
// base ELF's bare. Registered in gemlik_mobys.cpp.
//
// The pvars: +0 the box (-1: none, the moby goes), +4 .. +0x10 four mobys (their index in the level's moby table, -1
// none), +0x14 .. +0x20 four groups (-1 none); the states to set (signed bytes, -1 leaves it): entering +0x24 .. +0x27
// the mobys', +0x2c .. +0x2f the groups', +0x34 its own state, +0x36 its +0xbc; leaving +0x28 .. +0x2b, +0x30 .. +0x33,
// +0x35, +0x37; +0x38 whether the hero is inside (1) or not (0).
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

namespace {
// one of the box's two sets of states: `mobys` / `groups` the pvar offsets of the four mobys' and groups' states,
// `own` / `own_bc` those of the moby's own state and +0xbc; the four mobys and groups in turn
void trigger_set_states(uint32_t m, uint32_t pv, uint32_t mobys, uint32_t groups, uint32_t own, uint32_t own_bc) {
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t idx = lw(pv + 4 + k * 4);
        if (idx != -1) {
            const uint32_t t = lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
            if (t != 0) {
                const uint32_t st = lbu(t + 0x20);
                if (st != 0xfe && st != 0xfd && static_cast<int8_t>(lbu(pv + mobys + k)) != -1) sb(t + 0x20, lbu(pv + mobys + k));
            }
        }
        const int32_t group = lw(pv + 0x14 + k * 4);
        const int32_t gst = static_cast<int8_t>(lbu(pv + groups + k));
        if (group != -1 && gst != -1) moby_group_set_state(group, static_cast<uint32_t>(gst));
    }
    if (static_cast<int8_t>(lbu(pv + own)) != -1) sb(m + 0x20, lbu(pv + own));
    if (static_cast<int8_t>(lbu(pv + own_bc)) != -1) sb(m + 0xbc, lbu(pv + own_bc));
}
}  // namespace

// orig: L13:0x002b0e80
void gml_class6_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t box = lw(pv + 0);
    if (box == -1) {
        debug_printf_stub();  // (the original prints the moby's uid +0xb2 with the format L13:0x1f41f8)
        moby_retire(m);
        return;
    }
    const int8_t inside = static_cast<int8_t>(lbu(pv + 0x38));
    if (point_in_box(0x0013f3d0, box) != 0) {
        if (inside != 0) return;
        trigger_set_states(m, pv, 0x24, 0x2c, 0x34, 0x36);
        sb(pv + 0x38, 1);
    } else {
        if (inside != 1) return;
        trigger_set_states(m, pv, 0x28, 0x30, 0x35, 0x37);
        sb(pv + 0x38, 0);
    }
}

}  // namespace rac1
