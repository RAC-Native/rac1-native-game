// Hero movement-mode per-frame update (PLAYER+0x20a4 != 0):
// - Mode 1: Clank / Gadgebot control (glow 0x4b4, antenna 0x47a, command colors/sounds)
// - Mode 2: Giant Clank / rider attach
// - Mode 3: Hologuise (pulsing RGBA tint, chain points 0..2, alert/water flicker sound)
//
// All four disc builds share the same logic parameterized by (has_mode_1, has_mode_2):
// - (true,  true):  Veldin, Eudora, Umbris, Gaspar, Orxon, Gemlik (0x958 bytes, 0x002062b0)
// - (true,  false): Blarg, Veldin Orbit (0x8d0 bytes, L6:0x0021e1a0, L17:0x00203290)
// - (false, true):  Quartu, Veldin Finale (0x428 bytes, L15:0x001ff938, L18:0x0020efa8)
// - (false, false): Novalis, Aridia, Kerwan, Rilgar, Batalia, Pokitaru, Hoven, Oltanis, Kalebo (0x3a8 bytes)
#ifndef RAC1_GAME_OVL_HERO_MOVE_MODE_H
#define RAC1_GAME_OVL_HERO_MOVE_MODE_H

namespace rac1 {

void hero_move_mode_ex(bool has_mode_1, bool has_mode_2);
void hero_move_mode();     // orig: 0x002062b0
void hero_move_mode_l1();  // orig: L1:0x00228000 L2:0x00217158 L3:0x001ffa88 L5:0x00234dd8

}  // namespace rac1

#endif
