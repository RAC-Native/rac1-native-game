// Class 615, the Trespasser's pad, and its puzzle (three rings of twelve slots turned by the player until a path of links
// joins the twelve lights): one port of the copies of Aridia, Eudora, Blarg, Batalia, Pokitaru, Gemlik and the Veldin
// finale (shared_class615.cpp). Each declaration lists the levels' addresses of the copies it replaces.
#ifndef RAC1_GAME_OVL_SHARED_CLASS615_H
#define RAC1_GAME_OVL_SHARED_CLASS615_H

#include <cstdint>

namespace rac1 {

// The pad's update: waits for the hero with the Trespasser in hand, runs the puzzle (the camera on the pad, the rings turned
// by the stick, the draw callback queued), and records the pad solved.
void class615_update(uint32_t m);  // L2:0x002d8ad0 L4:0x002d4ae0 L6:0x002e1940 L8:0x002f2030 L11:0x002fcda8 L13:0x002f5618 L18:0x002d8fd0
// The camera's goal while the puzzle runs into `pos` / `rot` (EE vectors): the pad's camera record (pvars +0xe8), or half way
// between the hero and the pad, two units to the pad's side nearer the camera, looking back.
void class615_camera_goal(uint32_t pos, uint32_t rot, uint32_t m);  // L2:0x002d8910 L4:0x002d4920 L6:0x002e1780 L8:0x002f1e70 L11:0x002fcbe8 L13:0x002f5458 L18:0x002d8e10
// The hero pulled onto the pad's point 0 and turned to face the pad (each at most a frame's worth).
void class615_hero_to_pad(uint32_t m);  // L2:0x002d92f0 L4:0x002d5300 L6:0x002e2160 L8:0x002f2850 L11:0x002fd5c8 L13:0x002f5e38 L18:0x002d97f0

// The puzzle's draw callback (the pad's draw-list entry): the frame, the links between the rings, the rings and their
// markers, the twelve lights.
void class615_puzzle_draw(uint32_t m);  // L2:0x002d93e8 L4:0x002d53f8 L6:0x002e2258 L8:0x002f2948 L11:0x002fd6c0 L13:0x002f5f30 L18:0x002d98e8
// A link of the puzzle: two quads (a scrolling one and a wider one) along the ray `angle` from radius `r0` to `r1`.
void class615_link_draw(uint32_t colour, float angle, float r0, float r1);  // L2:0x002da938 L4:0x002d6948 L6:0x002e37a8 L8:0x002f3e98 L11:0x002fec10 L13:0x002f7480 L18:0x002dae38
// A ring's segment from `a0` to `a1` (five quads) between the radii `w0` and `w1`, texture register `tex0`.
void class615_ring_draw(uint64_t tex0, uint32_t colour, float a0, float a1, float w0, float w1);  // L2:0x002da3d8 L4:0x002d63e8 L6:0x002e3248 L8:0x002f3938 L11:0x002fe6b0 L13:0x002f6f20 L18:0x002da8d8
// A slot's marker (`kind` 0 a new link, 1 a link, 2 a gap) at `angle`, `radius` out, turned by the frame's matrix (the
// colour argument the callers pass is not read).
void class615_marker_draw(uint32_t kind, float angle, float radius);  // L2:0x002da6c0 L4:0x002d66d0 L6:0x002e3530 L8:0x002f3c20 L11:0x002fe998 L13:0x002f7208 L18:0x002dabc0
// The puzzle's frame: four quads of the level's block.
void class615_frame_draw();  // L2:0x002dae88 L4:0x002d6e98 L6:0x002e3cf8 L8:0x002f43e8 L11:0x002ff160 L13:0x002f79d0 L18:0x002db388

}  // namespace rac1

#endif
