// Class 1023 (Batalia, Gaspar), a walker that patrols its path, chases its target, leaps between two points, fires shots
// (class 1292) and strikes up close, shared by the two levels: one port of their copies (shared_class1023.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1023_H
#define RAC1_GAME_OVL_SHARED_CLASS1023_H

#include <cstdint>

namespace rac1 {

// The update: states 0 (set up) .. 11 (dying).
void class1023_update(uint32_t m);  // L8:0x00301158 L9:0x003009d0
// Its every-frame part: the size, the glow moby it carries (class 1025), the hits and their reactions, the colour pulse,
// the target, the floor.
void class1023_frame(uint32_t m);  // L8:0x003024d8 L9:0x00301d50
// A shot (class 1292) at `pos` (an EE vector) moving by `vel` (an EE vector), fired by `owner` at `target`, living `life`
// frames, the shot's index `index`; returns the shot (0: none).
uint32_t class1023_shot_spawn(uint32_t vel, uint32_t pos, uint32_t owner, uint32_t target, int32_t life, uint32_t index);  // L8:0x00307190 L9:0x00308e00

}  // namespace rac1

#endif
