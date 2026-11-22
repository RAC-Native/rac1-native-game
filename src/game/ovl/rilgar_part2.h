// Rilgar's (level 5) part 2: its draw callbacks (hand-written, the files named below) and their registration (rilgar_part2.cpp).
#ifndef RAC1_GAME_OVL_RILGAR_PART2_H
#define RAC1_GAME_OVL_RILGAR_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- the draw callbacks the classes queue (draw_callback_add), called through ovl_call_moby_fn with a0 = the moby
void ril_class79_draw_a_l5(uint32_t m);   // L5:0x002d6f48  queued by 0x2d7020 (ported: rilgar_air_traffic.cpp)
void ril_class79_draw_b_l5(uint32_t m);   // L5:0x002d7920  queued by class 79's update 0x2d7140 (ported: rilgar_draws.cpp)
void ril_class838_draw_l5(uint32_t m);    // L5:0x0030de90  queued by class 838's update 0x30dc68 (ported: rilgar_draws.cpp)
void ril_class879_draw_l5(uint32_t m);    // L5:0x00316070  queued by class 879's update 0x316110 (ported: rilgar_ocean.cpp)

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by rilgar_moby_fns_register
void rilgar_part2_register();

}  // namespace rac1

#endif
