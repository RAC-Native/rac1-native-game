// Kalebo III's (level 16) class 650: a floor plate that dips while something presses it. A class 541 fighter stepping on
// it sets its timer (pvar +0x8, L16:0x2d5e80); while the timer runs the plate eases down by the level's depth (L16 0x161a84)
// from its rest height (+0x0), and the clunk (sound 1) plays when it runs out, at most once every 4 frames for all plates
// (L16 0x161a88 the frame of the last one). Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered in
// kalebo_mobys.cpp; checked by batch 250 (class650).
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

// Class 650: state 0 keeps its rest height; state 1 eases its height toward the target (speed +0x4; 10 x dt^2, at most
// 10 x dt).
// orig: L16:0x002d5cb8
void kal_class650_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        swc1(pv, lwc1(m + 0x18));
        break;
    case 1: {
        const int32_t timer = kalebo::tick32(pv + 8);
        float target = lwc1(pv);
        if (timer == 0) {
            target = fpu_add(lwc1(pv), lwc1(L16_ADDR(0x00161a84)));
        } else if (timer == 2 && !(abs_i(lw(L16_ADDR(0x00161a88)) - lw(L16_ADDR(0x0015f5cc))) < 4)) {
            sw(L16_ADDR(0x00161a88), lwu(L16_ADDR(0x0015f5cc)));
            moby_sound_start(1, 0, m);
        }
        seek_1d(m + 0x18, pv + 4, target, fpu_mul(lwc1(0x0015ed70), 10.0f), fpu_mul(lwc1(0x0015ed70), 10.0f),
                fpu_mul(lwc1(0x0015ed6c), 10.0f));
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
