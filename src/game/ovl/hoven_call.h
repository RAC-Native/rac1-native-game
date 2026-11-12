// Hoven (level 12): the call helpers of the dispatch lines of the Veldin hero ports. Level 12's own hero controller
// functions (hoven_hero.h, uniform ABI) are called with the Veldin port's own arguments in a0..a3 (and f12).
#ifndef RAC1_GAME_OVL_HOVEN_CALL_H
#define RAC1_GAME_OVL_HOVEN_CALL_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/variants_l12.h>
#include <game/math/vecmath.h>

namespace rac1 {

// orig: (host) a uniform-ABI variant of level 12 called with its first four integer arguments
template <class F>
inline TrRet hov_call(F f, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    return f(a0, a1, a2, a3, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// orig: (host) a uniform-ABI variant of level 12 called with one float argument ($f12)
template <class F>
inline TrRet hov_callf(F f, float f12) {
    return f(0, 0, 0, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

}  // namespace rac1

#endif
