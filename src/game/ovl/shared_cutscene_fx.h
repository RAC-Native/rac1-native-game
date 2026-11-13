// Oltanis's arrival scene's sparks and glow (Veldin's own copy, which cutscene.cpp runs on every level, and Oltanis's):
// one port of both copies (shared_cutscene_fx.cpp). The Veldin copies stay generated (and registered) in this phase; the
// scene code calls the shared update.
#ifndef RAC1_GAME_OVL_SHARED_CUTSCENE_FX_H
#define RAC1_GAME_OVL_SHARED_CUTSCENE_FX_H

#include <cstdint>

namespace rac1 {

// Eight sparks (particle 0x35, in pairs: a warm one and a white one spinning the other way) from the moby's joint 0.
void cutscene_l14_effect(uint32_t m);  // L0:0x0023e008 L14:0x00245e98
// Its draw callback: six quads of effect texture 14 about the joint, a glow from the joint along its turned x axis.
void cutscene_l14_effect_draw(uint32_t m);  // L0:0x0023db30 L14:0x002459c0

}  // namespace rac1

#endif
