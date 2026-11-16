// Blarg's Nebula G34 (level 6) part 2: the level's own draw callbacks (hand-ported: blarg_draws.cpp; a0 = the moby, called
// through ovl_call_moby_fn) and their registration with the shared ports' (blarg_part2.cpp).
#ifndef RAC1_GAME_OVL_BLARG_PART2_H
#define RAC1_GAME_OVL_BLARG_PART2_H

#include <cstdint>

namespace rac1 {

void blg_class1035_draw_l6(uint32_t m);      // L6:0x002f6dd0  class 1035's bolts
void blg_class1062_draw_a_l6(uint32_t m);    // L6:0x002fd1a0  class 1062's first draw
void blg_class1123_draw_l6(uint32_t m);      // L6:0x00305170  class 1123's strands
// the four strands of quads of the record `rec` through the matrix `mat` (VU1 program 7's packets)
void arc_strands_draw_l6(uint32_t rec, uint32_t mat);  // L6:0x00216b88

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by blarg_moby_fns_register
void blarg_part2_register();

}  // namespace rac1

#endif
