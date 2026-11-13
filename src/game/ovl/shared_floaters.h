// The floating rafts that ride paths (class 998 on Rilgar, 424 on Batalia, 712 on Oltanis, 1410 on Kalebo III): their
// helpers, shared (shared_floaters.cpp). The copies read the rafts' pvars at offsets that differ between the levels'
// classes: the parameters (each level's values in the comments).
#ifndef RAC1_GAME_OVL_SHARED_FLOATERS_H
#define RAC1_GAME_OVL_SHARED_FLOATERS_H

#include <cstdint>

namespace rac1 {

// The raft at its ride's point (pvars +0x60, its turn +0x70), bobbing and rocking: three phases at pvars `phases`
// (Rilgar, Oltanis 0xc8; Batalia 0xec; Kalebo III 0xb8).
void floater_bob(uint32_t m, uint32_t phases);  // L5:0x00319740 L8:0x002dc4a0 L14:0x002f1490 L16:0x002e4d88
// Along its path (pvars `path`): its place (pvars `along`, 0..1) seeks 1 (its speed +4, its rate +0x18 and most +0x14 from
// there) and the ride's point and turn are the path's there. 1 at the path's end. (Rilgar, Oltanis: 0xc0 / 0xac; Kalebo
// III: 0xb0 / 0xa4.)
int32_t floater_ride(uint32_t m, uint32_t along, uint32_t path);  // L5:0x003195f8 L14:0x002f1360 L16:0x002e4c40
// The path (pvars `path`: an index of the level's paths) of 7 points laid out about the raft (the level's 7 offsets
// turned by it), and the hero's +0x22da the path. (Rilgar, Oltanis 0xdc; Kalebo III 0xa0.)
void floater_lay_path(uint32_t m, uint32_t path);  // L5:0x003198e8 L14:0x002f1638 L16:0x002e4f30
// The HUD message (as who 8) of the raft's stage (pvars `stage`, a half): `first` / `second` for the stages 0 / 1, then
// the level's next ones. (Rilgar: 0xb6, 0x13a5, 0x1398; Kalebo III: 0xae, 0x3e89, 0x3e8a.)
void floater_message(uint32_t m, uint32_t stage, int32_t first, int32_t second);  // L5:0x00319690 L16:0x002e4cd8
// The moby of the raft's group (+0x21) whose pvars `key` (a half) is `value`, or 0. (Rilgar 0xb4; Kalebo III 0xac.)
uint32_t floater_find(uint32_t m, int32_t value, uint32_t key);  // L5:0x00319598 L16:0x002e4be0

}  // namespace rac1

#endif
