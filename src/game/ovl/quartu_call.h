// Quartu (level 15): the call helpers of the dispatch lines of the Veldin hero ports. Level 15's own hero controller
// functions (quartu_hero.h, uniform ABI) are called with the Veldin port's own arguments in a0..a3 (and f12).
#ifndef RAC1_GAME_OVL_QUARTU_CALL_H
#define RAC1_GAME_OVL_QUARTU_CALL_H

#include <cstdint>

#include <game/ovl/quartu_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/variants_l15.h>
#include <game/math/vecmath.h>

namespace rac1 {

// orig: (host) a uniform-ABI variant of level 15 called with its first four integer arguments
template <class F>
inline TrRet qua_call(F f, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    return f(a0, a1, a2, a3, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// orig: (host) a uniform-ABI variant of level 15 called with one float argument ($f12)
template <class F>
inline TrRet qua_callf(F f, float f12) {
    return f(0, 0, 0, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

}  // namespace rac1

#endif
