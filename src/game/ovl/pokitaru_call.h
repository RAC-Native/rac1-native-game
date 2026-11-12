// Pokitaru (level 11): the call helpers of the dispatch lines of the Veldin hero ports. Level 11's own hero controller
// functions (pokitaru_hero.h, uniform ABI) are called with the Veldin port's own arguments in a0..a3 (and f12).
#ifndef RAC1_GAME_OVL_POKITARU_CALL_H
#define RAC1_GAME_OVL_POKITARU_CALL_H

#include <cstdint>

#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/variants_l11.h>
#include <game/math/vecmath.h>

namespace rac1 {

// orig: (host) a uniform-ABI variant of level 11 called with its first four integer arguments
template <class F>
inline TrRet pok_call(F f, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    return f(a0, a1, a2, a3, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// orig: (host) a uniform-ABI variant of level 11 called with one float argument ($f12)
template <class F>
inline TrRet pok_callf(F f, float f12) {
    return f(0, 0, 0, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

}  // namespace rac1

#endif
