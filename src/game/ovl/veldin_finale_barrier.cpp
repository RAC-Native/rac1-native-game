// Veldin's finale (level 18) class 1392: a force field held by a switch. Set up (state 0) it is at full strength (+0xc 1)
// and stands (state 1): it queues its draw (L18:0x2f1fd8, on the level's draw list) and keeps its hum playing; when its
// switch (the class 615 moby its +0 names) is in its state 3 or later, it goes down (state 2) with its hum stopped, its
// strength fading out in half a second. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data
// L18_ADDR, the base ELF's bare. Registered in veldin_finale_mobys.cpp.
//
// The pvars: +0 its switch (a moby index, -1 none), +4 its hum's sound channel (-1 none), +0xc its strength.
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>

namespace rac1 {

namespace {
constexpr int32_t SWITCH_CLASS = 0x267;  // (615)

// the hum on channel `slot`'s word stopped, when that channel still plays it on `m`; the word -1
void stop_hum(uint32_t m, uint32_t slot) {
    const int32_t ch = lw(slot);
    if (ch != -1) {
        const uint32_t c = 0x0013e550 + static_cast<uint32_t>(ch * 0x70);
        if (lwu(c + 0x88) == m && lbu(c + 0x74) != 0) sub_0028d918(ch);
    }
    sw(slot, 0xffffffffu);
}
}  // namespace

// orig: L18:0x002f1e68
void vf_class1392_update_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002f1ebc)
        sb(m + 0x20, 1);
        swc1(pv + 0xc, 1.0f);
        return;
    case 1: {  // standing (0x002f1ed0)
        sub_001f93e0(L18_ADDR(0x002f1fd8), m);
        if (sub_0028d8c0(m, lw(pv + 4)) == 0) sw(pv + 4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        const int32_t index = lw(pv + 0);
        if (index == -1) return;
        const uint32_t sw_moby = lwu(L18_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8);
        if (lh(sw_moby + 0xa6) != SWITCH_CLASS || lbu(sw_moby + 0x20) < 3) return;
        sw(m + 0x94, 0);
        sb(m + 0x20, 2);
        if (sub_0028d8c0(m, lw(pv + 4)) == 0) return;
        stop_hum(m, pv + 4);
        return;
    }
    case 2:  // down: fading (0x002f1fa8)
        approach_f(pv + 0xc, 0.0f, fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c)));
        return;
    default:
        return;
    }
}

}  // namespace rac1
