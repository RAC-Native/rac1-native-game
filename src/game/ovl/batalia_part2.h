// Batalia (level 8) part 2: the registration of its draw-list callbacks (batalia_part2.cpp; the callbacks are hand-written: the
// classes' own files and the shared ports, among them the sea's sea_draw and the scrolling layers' class600_draw / class1649_draw).
#ifndef RAC1_GAME_OVL_BATALIA_PART2_H
#define RAC1_GAME_OVL_BATALIA_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by batalia_moby_fns_register
void batalia_part2_register();

}  // namespace rac1

#endif
